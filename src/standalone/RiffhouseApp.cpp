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
#include <iostream>
#include <juce_audio_plugin_client/Standalone/juce_StandaloneFilterWindow.h>

#include "../plugin/AudioProbe.h"
#include "../plugin/AudioSwitch.h"
#include "../plugin/AutoSetup.h"
#include "../engine/DeviceNames.h"
#include "../plugin/DeviceGuard.h"
#include "../plugin/DeviceLog.h"
#include "../plugin/Reports.h"
#include "../plugin/StandaloneHost.h"
#include "../plugin/Updater.h"
#include "../plugin/Version.h"
#include "../engine/UpdateInfo.h"
#include "../ui/Editor.h"
#include "../ui/Theme.h"

namespace rh
{
// Holds the editor so it always fills the window: the editor lays itself out for the window's shape.
class Letterbox : public juce::Component
{
public:
    explicit Letterbox (juce::AudioProcessorEditor& e) : editor (e)
    {
        addAndMakeVisible (editor);
    }
    void paint (juce::Graphics& g) override { g.fillAll (ui::colours::bg); }
    void resized() override { editor.setBounds (getLocalBounds()); }

private:
    juce::AudioProcessorEditor& editor;
};

class MainWindow : public juce::DocumentWindow
{
public:
    MainWindow (juce::StandalonePluginHolder& h, juce::PropertiesFile& s)
        : DocumentWindow (isPreRelease (versionLabel()) ? "Riffhouse " + versionForPeople() : juce::String ("Riffhouse"),
                          ui::colours::bg, DocumentWindow::allButtons), holder (h), settings (s)
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
            const float fit = std::min ({ 1.0f, 0.95f * (float) area.getWidth() / RiffhouseEditor::baseWidth,
                                        0.88f * (float) area.getHeight() / RiffhouseEditor::baseHeight });
            centreWithSize (juce::roundToInt (RiffhouseEditor::baseWidth * fit), juce::roundToInt (RiffhouseEditor::baseHeight * fit));
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
    const juce::String getApplicationVersion() override { return versionLabel(); }
    // a test run of one sound card driver (AudioProbe) runs beside the open app
    bool moreThanOneInstanceAllowed() override
    {
        const auto cl = getCommandLineParameters();
        return AudioProbe::isProbeCommandLine (cl) || cl.contains ("--report-check");
    }
    void anotherInstanceStarted (const juce::String&) override
    {
        if (window != nullptr) window->toFront (true);
    }

    void initialise (const juce::String& commandLine) override
    {
        if (AudioProbe::isProbeCommandLine (commandLine))
        {
            // riffhouse --probe-audio <type> <driver>: start that driver, say whether it played, quit
            probeOnly = true;
            setApplicationReturnValue (AudioProbe::run (juce::StringArray::fromTokens (commandLine, true)));
            quit();
            return;
        }
        if (commandLine.contains ("--report-check"))
        {
            // riffhouse --report-check: send one test report the way the app does, say how it went, quit
            probeOnly = true;
            const auto err = reports::sendNow ("Report check from " + versionLabel(),
                                               "Riffhouse: " + versionLabel() + "\nA check that this build reaches the report relay. "
                                               "Sent from C:\\Users\\" + juce::SystemStats::getLogonName() + "\\ on " + juce::SystemStats::getComputerName()
                                               + ": both names must arrive replaced.");
            std::cout << (err.isEmpty() ? juce::String ("RH-REPORT ok") : "RH-REPORT failed " + err) << std::endl;
            setApplicationReturnValue (err.isEmpty() ? 0 : 1);
            quit();
            return;
        }
        auto* s = properties.getUserSettings();
        AudioProbe::useSettings (s);
        // A diary of what happens with the sound card, kept beside the settings. Pictures of the
        // app (--snapshots, --motion) leave no trace.
        if (! commandLine.contains ("--snapshots") && ! commandLine.contains ("--motion"))
            devlog::start (s->getFile().getParentDirectory(), versionLabel());
        // The guitar must be heard. JUCE's default mutes the input to avoid
        // feedback; Riffhouse instead asks for headphones in the setup.
        s->setValue ("shouldMuteInput", false);

        juce::Array<juce::StandalonePluginHolder::PluginInOuts> channels;
        channels.add ({ 2, 2 });
        // the last argument: every MIDI input is opened as it appears, so a foot controller works the moment it is plugged in
        {
            // the sound card that was saved is opened in here; if the start hangs, the diary says where
            devlog::Timed opening ("Start: opening the sound card that was in use last time");
            holder = std::make_unique<juce::StandalonePluginHolder> (s, false, juce::String(), nullptr, channels, true);
            if (auto* dev = holder->deviceManager.getCurrentAudioDevice())
                opening.result (holder->deviceManager.getCurrentAudioDeviceType() + ", " + dev->getName() + ", "
                                + juce::String (dev->getCurrentBufferSizeSamples()) + " samples");
            else
                opening.result ("nothing opened");
        }
        holder->getMuteInputValue().setValue (false);
        instance = this;

        // Keep the guitar connected through unplugging, sleep and device swaps.
        auto& rig = *dynamic_cast<RiffhouseProcessor*> (holder->processor.get());
        guard = std::make_unique<DeviceGuard> (holder->deviceManager, rig.blockCounter, rig.muteOutput);
        guard->onMessage = [this] (const juce::String& m) { if (auto* ed = editor()) ed->showToast (m); };
        guard->onNeedSetup = [this] { if (auto* ed = editor()) ed->showSetupWizard(); };
        startTimer (30000); // autosave: a crash or a power cut never costs more than half a minute of tweaking

        // Updates arrive by themselves: fetched in the background, installed when the app closes.
        runLock.enter (0); // lets the installer see that the app is still open
        updates = std::make_unique<Updater> (*s, versionLabel());
        updates->onChange = [this] { if (auto* ed = editor()) ed->repaint(); };
        if (! commandLine.contains ("--snapshots") && ! commandLine.contains ("--motion"))
        {
            updates->start();
            const auto fresh = updates->justUpdatedTo();
            if (fresh.isNotEmpty())
                juce::Timer::callAfterDelay (2500, [this, fresh] { if (auto* ed = editor()) ed->showToast ("Updated to version " + fresh); });
        }

        // the window comes last: its settings screen asks for the guard and the updater
        window = std::make_unique<MainWindow> (*holder, *s);

        // A start on a slow way in (DirectSound, or a huge buffer) tries the fastest way the
        // same input has, once the window is up. It only stays if sound proves to come through.
        if (! commandLine.contains ("--snapshots") && ! commandLine.contains ("--motion"))
            juce::Timer::callAfterDelay (3000, [this] { healSlowStart(); });

        // A run that crashed or froze says so once, and offers to send what it wrote down.
        if (const auto last = devlog::lastRun(); last == devlog::LastRun::crashed || last == devlog::LastRun::froze)
            juce::Timer::callAfterDelay (5500, [this, last]
            {
                const juce::String what = last == devlog::LastRun::crashed ? "Riffhouse closed by itself last time." : "Riffhouse froze last time.";
                if (reports::canSend()) offerReport (what);
                else if (auto* ed = editor()) ed->showToast (what + " Settings, Save a report tells us why.");
            });
        // the weekly count, only for players who switched it on
        if (! commandLine.contains ("--snapshots") && ! commandLine.contains ("--motion"))
            juce::Timer::callAfterDelay (20000, [this] { reports::sayHelloIfDue (*properties.getUserSettings()); });

        // riffhouse --snapshots <folder> [scale]: render every screen to PNG and quit
        auto args = juce::StringArray::fromTokens (commandLine, true);
        if (const int m = args.indexOf ("--motion"); m >= 0 && m + 1 < args.size())
        {
            // riffhouse --motion <folder>: every move in the app as a strip of frames, then quit
            const juce::File folder (juce::File::getCurrentWorkingDirectory().getChildFile (args[m + 1].unquoted()));
            juce::Timer::callAfterDelay (1500, [this, folder]
            {
                if (auto* ed = dynamic_cast<RiffhouseEditor*> (holder->processor->getActiveEditor()))
                    ed->saveFilmstrips (folder);
                systemRequestedQuit();
            });
        }
        const int at = args.indexOf ("--snapshots");
        if (at >= 0 && at + 1 < args.size())
        {
            const juce::File folder (juce::File::getCurrentWorkingDirectory().getChildFile (args[at + 1].unquoted()));
            const float scale = at + 2 < args.size() ? (float) args[at + 2].getDoubleValue() : 1.0f;
            juce::Timer::callAfterDelay (1500, [this, folder, scale]
            {
                if (auto* ed = dynamic_cast<RiffhouseEditor*> (holder->processor->getActiveEditor()))
                {
                    ed->saveSnapshots (folder, scale > 0.0f ? scale : 1.0f);
                    ed->rehearseSetup();
                    folder.getChildFile ("rehearsed.txt").replaceWithText ("The end of a real setup ran through.\n");
                }
                systemRequestedQuit();
            });
        }
    }

    void shutdown() override
    {
        if (probeOnly) return; // nothing else was started
        stopTimer();
        healer.reset();
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
        devlog::stop();
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
        if (guard != nullptr)
            if (const auto n = guard->notice(); n.text.isNotEmpty()) return { n.text, n.action, n.warning };
        if (reportOffer.isNotEmpty())
            return { reportOffer + " Send a report so it gets fixed? Your name and PC name are taken out.", "Send report", false, "Not now" };
        return {};
    }
    bool offerShowing() const { return reportOffer.isNotEmpty() && (guard == nullptr || guard->notice().text.isEmpty()); }
    void offerReport (const juce::String& what) override
    {
        if (! reports::canSend() || what.isEmpty()) return;
        // the same trouble is offered once a day, however often it happens
        auto& s = *properties.getUserSettings();
        const auto stamp = juce::Time::getCurrentTime().formatted ("%Y-%m-%d ") + what;
        if (s.getValue ("lastReportOffer") == stamp) return;
        s.setValue ("lastReportOffer", stamp);
        reportOffer = what;
        devlog::note ("Offered to send a report: " + what);
    }
    void audioNoticeSecond() override
    {
        if (offerShowing()) { devlog::note ("Report not sent: not now"); reportOffer = {}; }
    }
    void audioNoticeAction() override
    {
        if (offerShowing())
        {
            const auto what = reportOffer;
            reportOffer = {};
            const auto text = makeReport ? makeReport() : devlog::tail (120);
            reports::send (*properties.getUserSettings(), what, text, [this] (bool ok, juce::String why)
            {
                devlog::note (ok ? "Report sent" : "Report did not go: " + why);
                if (auto* ed = editor()) ed->showToast (ok ? "Report sent. Thank you, it gets read." : "The report did not go: " + why);
            });
            return;
        }
        if (guard == nullptr) return;
        if (guard->state() == DeviceGuard::State::healthy) guard->dismissNotice();
        else if (guard->state() == DeviceGuard::State::stalled || guard->gaveUp()) guard->retry();
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
            systemRequestedQuit(); // closes any open file window or menu first, then quits
        }
    }
    juce::PropertiesFile& settings() override { return *properties.getUserSettings(); }

private:
    // Riffhouse can come back on DirectSound with thousands of samples of delay (a fallback that
    // was saved, a crash in the middle of trying another driver). The same input is offered the
    // faster ways first; whatever proves to work is kept and named, and nothing changes if none do.
    void healSlowStart()
    {
        if (holder == nullptr || (healer != nullptr && healer->busy())) return;
        auto& dm = holder->deviceManager;
        auto* dev = dm.getCurrentAudioDevice();
        if (dev == nullptr) return; // nothing open: the guard and setup look after that
        const auto now = AudioOpen::running (dm);
        const int buf = dev->getCurrentBufferSizeSamples();
        const double rate = std::max (1.0, dev->getCurrentSampleRate());
        const bool slow = now.type == "DirectSound" || 1000.0 * buf / rate > 20.0;
        if (! slow || now.input.isEmpty() || now.type == "ASIO") return;

        // An audio interface plays best through its own ASIO driver, and some only work that
        // way: that is tried first when the input belongs to one. Then the fastest Windows ways.
        std::vector<AudioChoice> tries;
        if (auto* asio = [&]() -> juce::AudioIODeviceType* { for (auto* t : dm.getAvailableDeviceTypes()) if (t->getTypeName() == "ASIO") return t; return nullptr; }())
            for (const auto& d : asio->getDeviceNames (true))
                if (! AutoSetup::isAsioWrapper (d) && ! AudioProbe::knownBad (d) && sameHardware (d, now.input))
                {
                    AudioChoice a;
                    a.types = { "ASIO" };
                    a.input = a.output = d;
                    tries.push_back (a);
                    break;
                }
        AudioChoice c;
        c.input = now.input;
        c.output = now.output;
        c.types = AudioSwitch::modesFor (dm, "Windows Audio", now.input, AudioSwitch::isShared (now.type) && now.type != "DirectSound");
        c.types.removeString ("DirectSound");
        if (! c.types.isEmpty()) tries.push_back (c);
        if (tries.empty()) return;

        devlog::note ("Start was slow (" + now.type + ", " + juce::String (buf) + " samples): trying a faster way");
        auto& rig = *dynamic_cast<RiffhouseProcessor*> (holder->processor.get());
        healer = std::make_unique<AudioSwitch> (dm, rig);
        setupRunning (true);
        healNext (std::move (tries), 0, now.type, 1000.0 * buf / rate);
    }

    void healNext (std::vector<AudioChoice> tries, size_t i, juce::String typeBefore, double msBefore)
    {
        if (i >= tries.size() || healer == nullptr) { setupRunning (false); return; }
        const auto choice = tries[i];
        healer->apply (choice, [this, tries, i, typeBefore, msBefore] (AudioSwitch::Result r)
        {
            if (! r.ok)
            {
                devlog::note ("Not faster: " + r.message);
                juce::MessageManager::callAsync ([this, tries, i, typeBefore, msBefore] { healNext (tries, i + 1, typeBefore, msBefore); });
                return;
            }
            setupRunning (false);
            // the faster way works: keep it, and leave the driver box on Automatic
            properties.getUserSettings()->setValue ("audioDriver", juce::String());
            saveAudioState();
            juce::String msNow;
            if (auto* d = holder->deviceManager.getCurrentAudioDevice())
                msNow = juce::String (1000.0 * d->getCurrentBufferSizeSamples() / std::max (1.0, d->getCurrentSampleRate()), 1);
            const auto text = "Riffhouse had started on " + AudioSwitch::friendlyType (typeBefore) + " with a "
                            + juce::String (juce::roundToInt (msBefore)) + " ms buffer. It moved to "
                            + AudioSwitch::friendlyType (holder->deviceManager.getCurrentAudioDeviceType())
                            + (msNow.isNotEmpty() ? ", " + msNow + " ms" : juce::String()) + ", which plays clean.";
            devlog::note (text);
            if (auto* ed = editor()) ed->showToast (text);
        });
    }

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
    std::unique_ptr<AudioSwitch> healer;
    bool probeOnly = false;
    juce::String reportOffer; // what went wrong, while the strip asks to send a report
    std::unique_ptr<Updater> updates;
    juce::InterProcessLock runLock { "RiffhouseRunning" };
    bool installerStarted = false;
};
} // namespace rh

juce::JUCEApplicationBase* juce_CreateApplication();
juce::JUCEApplicationBase* juce_CreateApplication() { return new rh::RiffhouseApp(); }

#endif
