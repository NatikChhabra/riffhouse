// Riffhouse - free, open-source guitar rig
// Copyright (C) 2026 Riffhouse contributors. Licensed under AGPL-3.0-or-later.
//
// The standalone app. It replaces JUCE's generic standalone window with one
// that has no "Options" button and no "input muted" banner: audio setup is
// handled by Riffhouse's own automatic setup instead.

#include <juce_core/system/juce_TargetPlatform.h>
#include <juce_audio_plugin_client/detail/juce_CheckSettingMacros.h>

#if JucePlugin_Build_Standalone

#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_audio_plugin_client/Standalone/juce_StandaloneFilterWindow.h>

#include "../plugin/DeviceGuard.h"
#include "../plugin/StandaloneHost.h"
#include "../plugin/Updater.h"
#include "../ui/Editor.h"
#include "../ui/Theme.h"

namespace rh
{
// Keeps the editor at its designed aspect ratio inside a freely resizable window.
class Letterbox : public juce::Component
{
public:
    explicit Letterbox (juce::AudioProcessorEditor& e) : editor (e)
    {
        addAndMakeVisible (editor);
    }
    void paint (juce::Graphics& g) override { g.fillAll (ui::colours::bg); }
    void resized() override
    {
        const float s = std::min ((float) getWidth() / RiffhouseEditor::baseWidth, (float) getHeight() / RiffhouseEditor::baseHeight);
        const int w = juce::roundToInt (RiffhouseEditor::baseWidth * s), h = juce::roundToInt (RiffhouseEditor::baseHeight * s);
        editor.setBounds (getLocalBounds().withSizeKeepingCentre (w, h));
    }

private:
    juce::AudioProcessorEditor& editor;
};

class MainWindow : public juce::DocumentWindow
{
public:
    MainWindow (juce::StandalonePluginHolder& h, juce::PropertiesFile& s)
        : DocumentWindow ("Riffhouse", ui::colours::bg, DocumentWindow::allButtons), holder (h), settings (s)
    {
        setUsingNativeTitleBar (true);
        editor.reset (holder.processor->createEditorIfNeeded());
        if (auto* ed = editor.get())
        {
            ed->setResizable (false, false);
            letterbox = std::make_unique<Letterbox> (*ed);
        }
        setContentNonOwned (letterbox.get(), false);
        setResizable (true, false);
        setResizeLimits (RiffhouseEditor::baseWidth / 2, RiffhouseEditor::baseHeight / 2, 10000, 10000);

        const auto saved = juce::Rectangle<int>::fromString (settings.getValue ("windowBounds"));
        const auto& displays = juce::Desktop::getInstance().getDisplays();
        auto* display = displays.getPrimaryDisplay();
        const auto area = display != nullptr ? display->userArea : juce::Rectangle<int> (0, 0, 1280, 800);
        if (! saved.isEmpty() && displays.getDisplayForRect (saved) != nullptr && area.intersects (saved))
            setBounds (saved);
        else
        {
            // fit comfortably on small laptop screens too
            const float s = std::min ({ 1.0f, 0.95f * (float) area.getWidth() / RiffhouseEditor::baseWidth,
                                        0.88f * (float) area.getHeight() / RiffhouseEditor::baseHeight });
            centreWithSize (juce::roundToInt (RiffhouseEditor::baseWidth * s), juce::roundToInt (RiffhouseEditor::baseHeight * s));
        }
        setVisible (true);
        if (editor != nullptr) editor->grabKeyboardFocus();
    }

    ~MainWindow() override
    {
        settings.setValue ("windowBounds", getBounds().toString());
        clearContentComponent();
        letterbox.reset();
        if (editor != nullptr) holder.processor->editorBeingDeleted (editor.get());
        editor.reset();
    }

    void closeButtonPressed() override
    {
        juce::JUCEApplicationBase::quit();
    }

private:
    juce::StandalonePluginHolder& holder;
    juce::PropertiesFile& settings;
    std::unique_ptr<juce::AudioProcessorEditor> editor;
    std::unique_ptr<Letterbox> letterbox;
};

class RiffhouseApp : public juce::JUCEApplication, public StandaloneHost, private juce::Timer
{
public:
    RiffhouseApp()
    {
        juce::PropertiesFile::Options o;
        o.applicationName = "Riffhouse";
        o.filenameSuffix = ".settings";
        o.osxLibrarySubFolder = "Application Support";
       #if JUCE_LINUX || JUCE_BSD
        o.folderName = "~/.config/Riffhouse";
       #else
        o.folderName = "Riffhouse";
       #endif
        properties.setStorageParameters (o);
    }

    const juce::String getApplicationName() override { return "Riffhouse"; }
    const juce::String getApplicationVersion() override { return JucePlugin_VersionString; }
    bool moreThanOneInstanceAllowed() override { return false; }
    void anotherInstanceStarted (const juce::String&) override
    {
        if (window != nullptr) window->toFront (true);
    }

    void initialise (const juce::String& commandLine) override
    {
        auto* s = properties.getUserSettings();
        // The guitar must be heard. JUCE's default mutes the input to avoid
        // feedback; Riffhouse instead asks for headphones in the setup.
        s->setValue ("shouldMuteInput", false);

        juce::Array<juce::StandalonePluginHolder::PluginInOuts> channels;
        channels.add ({ 2, 2 });
        holder = std::make_unique<juce::StandalonePluginHolder> (s, false, juce::String(), nullptr, channels, false);
        holder->getMuteInputValue().setValue (false);
        instance = this;

        // Keep the guitar connected through unplugging, sleep and device swaps.
        guard = std::make_unique<DeviceGuard> (holder->deviceManager, *dynamic_cast<RiffhouseProcessor*> (holder->processor.get()));
        guard->onMessage = [this] (const juce::String& m) { if (auto* ed = editor()) ed->showToast (m); };
        guard->onNeedSetup = [this] { if (auto* ed = editor()) ed->showSetupWizard(); };
        startTimer (30000); // autosave: a crash or a power cut never costs more than half a minute of tweaking

        // Updates arrive by themselves: fetched in the background, installed when the app closes.
        runLock.enter (0); // lets the installer see that the app is still open
        updates = std::make_unique<Updater> (*s, JucePlugin_VersionString);
        updates->onChange = [this] { if (auto* ed = editor()) ed->repaint(); };
        if (! commandLine.contains ("--snapshots"))
        {
            updates->start();
            const auto fresh = updates->justUpdatedTo();
            if (fresh.isNotEmpty())
                juce::Timer::callAfterDelay (2500, [this, fresh] { if (auto* ed = editor()) ed->showToast ("Updated to version " + fresh); });
        }

        // the window comes last: its settings screen asks for the guard and the updater
        window = std::make_unique<MainWindow> (*holder, *s);

        // riffhouse --snapshots <folder> [scale]: render every screen to PNG and quit
        auto args = juce::StringArray::fromTokens (commandLine, true);
        const int at = args.indexOf ("--snapshots");
        if (at >= 0 && at + 1 < args.size())
        {
            const juce::File folder (juce::File::getCurrentWorkingDirectory().getChildFile (args[at + 1].unquoted()));
            const float scale = at + 2 < args.size() ? (float) args[at + 2].getDoubleValue() : 1.0f;
            juce::Timer::callAfterDelay (1500, [this, folder, scale]
            {
                if (auto* ed = dynamic_cast<RiffhouseEditor*> (holder->processor->getActiveEditor()))
                    ed->saveSnapshots (folder, scale > 0.0f ? scale : 1.0f);
                quit();
            });
        }
    }

    void shutdown() override
    {
        stopTimer();
        guard.reset();
        if (holder != nullptr) holder->savePluginState();
        window.reset();
        if (holder != nullptr) holder->saveAudioDeviceState();
        instance = nullptr;
        holder.reset();
        properties.saveIfNeeded();
        // Everything is saved and the audio device is closed: now is the moment an update can go in.
        if (updates != nullptr && ! installerStarted && updates->automatic() && updates->canInstall())
            updates->launchInstaller (false);
        updates.reset();
        runLock.exit();
    }

    void systemRequestedQuit() override
    {
        if (holder != nullptr) holder->savePluginState();
        if (juce::ModalComponentManager::getInstance()->cancelAllModalComponents())
            juce::Timer::callAfterDelay (100, [] { if (auto* app = juce::JUCEApplicationBase::getInstance()) app->systemRequestedQuit(); });
        else
            quit();
    }

    // StandaloneHost
    juce::AudioDeviceManager& deviceManager() override { return holder->deviceManager; }
    void saveAudioState() override
    {
        holder->saveAudioDeviceState();
        properties.saveIfNeeded();
        if (guard != nullptr) guard->adopt();
    }
    Notice audioNotice() const override
    {
        if (guard == nullptr) return {};
        const auto n = guard->notice();
        return { n.text, n.action, n.warning };
    }
    void audioNoticeAction() override
    {
        if (guard == nullptr) return;
        if (guard->state() == DeviceGuard::State::healthy) guard->dismissNotice();
        else if (auto* ed = editor()) ed->showSetupWizard();
    }
    void setupRunning (bool running) override
    {
        if (guard != nullptr) guard->pause (running);
    }
    Updater* updater() override { return updates.get(); }
    void restartToUpdate() override
    {
        if (updates == nullptr || ! updates->canInstall()) return;
        if (holder != nullptr) holder->savePluginState();
        properties.saveIfNeeded();
        if (updates->launchInstaller (true))
        {
            installerStarted = true;
            quit();
        }
    }
    juce::PropertiesFile& settings() override { return *properties.getUserSettings(); }

private:
    RiffhouseEditor* editor() const
    {
        return holder != nullptr ? dynamic_cast<RiffhouseEditor*> (holder->processor->getActiveEditor()) : nullptr;
    }
    void timerCallback() override
    {
        if (holder == nullptr) return;
        holder->savePluginState();
        properties.saveIfNeeded();
    }

    juce::ApplicationProperties properties;
    std::unique_ptr<juce::StandalonePluginHolder> holder;
    std::unique_ptr<MainWindow> window;
    std::unique_ptr<DeviceGuard> guard;
    std::unique_ptr<Updater> updates;
    juce::InterProcessLock runLock { "RiffhouseRunning" };
    bool installerStarted = false;
};
} // namespace rh

juce::JUCEApplicationBase* juce_CreateApplication();
juce::JUCEApplicationBase* juce_CreateApplication() { return new rh::RiffhouseApp(); }

#endif
