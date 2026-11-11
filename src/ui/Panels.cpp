// Riffhouse - free, open-source guitar rig
// Copyright (C) 2026 Riffhouse contributors. Licensed under AGPL-3.0-or-later.
#include "Panels.h"
#include "../plugin/StandaloneHost.h"
#include "../plugin/Updater.h"

namespace rh::ui
{
//==============================================================================
TunerView::TunerView (RiffhouseProcessor& p) : proc (p)
{
    close.onClick = [this] { if (onClose) onClose(); };
    close.setTooltip ("Close the tuner");
    addAndMakeVisible (close);
    mute.setToggleState (proc.tunerMute.load(), juce::dontSendNotification);
    mute.onClick = [this] { proc.tunerMute.store (mute.getToggleState()); };
    addAndMakeVisible (mute);
}

void TunerView::resized()
{
    close.setBounds (getWidth() - 70, 20, 46, 46);
    mute.setBounds (getLocalBounds().removeFromBottom (70).withSizeKeepingCentre (290, 28));
}

void TunerView::refresh()
{
    const auto r = proc.tuner().analyse();
    if (r.valid)
    {
        reading = r;
        lostFrames = 0;
    }
    else
        ++lostFrames;
    const float target = lostFrames < 20 ? reading.cents : 0.0f;
    shownCents += 0.35f * (target - shownCents);
    repaint();
}

void TunerView::paint (juce::Graphics& g)
{
    g.fillAll (colours::bg);
    const bool live = lostFrames < 20 && reading.valid;
    const bool inTune = live && std::abs (reading.cents) < 3.0f;
    const auto c = ! live ? colours::textFaint : (inTune ? colours::good : (std::abs (reading.cents) < 12.0f ? colours::warn : colours::accent));
    const float cx = (float) getWidth() * 0.5f;

    // note name, big
    const auto noteArea = juce::Rectangle<float> (420.0f, 190.0f).withCentre ({ cx, 210.0f });
    if (inTune)
    {
        g.setGradientFill (juce::ColourGradient (colours::good.withAlpha (0.16f), cx, 210.0f, colours::good.withAlpha (0.0f), cx + 220.0f, 210.0f, true));
        g.fillEllipse (noteArea.withSizeKeepingCentre (440.0f, 300.0f));
    }
    const bool display = look() != Look::studio;
    if (display)
        drawWell (g, noteArea.withSizeKeepingCentre (340.0f, 210.0f), 16.0f);
    g.setColour (live ? (display && inTune ? colours::good : colours::text) : colours::textFaint.withAlpha (0.6f));
    g.setFont (display ? font (165.0f, Weight::numeric) : font (168.0f, Weight::display));
    const juce::String note = reading.valid ? reading.noteName : "--";
    g.drawText (note, noteArea, juce::Justification::centred);
    if (reading.valid)
    {
        const float nw = juce::GlyphArrangement::getStringWidth (g.getCurrentFont(), note);
        g.setFont (font (36.0f, Weight::semibold));
        g.setColour (colours::textDim);
        g.drawText (juce::String (reading.octave), juce::Rectangle<float> (cx + std::min (nw * 0.5f, 150.0f) + 6.0f, noteArea.getCentreY() + 18.0f, 60.0f, 50.0f),
                    juce::Justification::centredLeft);
    }

    // horizontal cents scale
    const float w = 680.0f, y = 400.0f;
    const float x0 = cx - w * 0.5f;
    for (int cents = -50; cents <= 50; cents += 5)
    {
        const float x = x0 + (float) (cents + 50) / 100.0f * w;
        const bool major = cents % 25 == 0;
        const float h = cents == 0 ? 44.0f : (major ? 26.0f : 14.0f);
        g.setColour (cents == 0 ? colours::good.withAlpha (0.9f) : colours::line.brighter (major ? 0.35f : 0.1f));
        g.fillRoundedRectangle (x - (cents == 0 ? 1.5f : 1.0f), y - h * 0.5f, cents == 0 ? 3.0f : 2.0f, h, 1.0f);
        if (major && cents != 0)
        {
            g.setFont (font (12.5f, Weight::medium));
            g.setColour (colours::textFaint);
            g.drawText ((cents > 0 ? "+" : "") + juce::String (cents), juce::Rectangle<float> (50.0f, 18.0f).withCentre ({ x, y + 34.0f }), juce::Justification::centred);
        }
    }
    g.setFont (font (13.0f, Weight::semibold));
    g.setColour (colours::textFaint);
    g.drawText ("FLAT", juce::Rectangle<float> (x0 - 90.0f, y - 10.0f, 70.0f, 20.0f), juce::Justification::centredRight);
    g.drawText ("SHARP", juce::Rectangle<float> (x0 + w + 20.0f, y - 10.0f, 70.0f, 20.0f), juce::Justification::centredLeft);

    if (live)
    {
        const float x = x0 + (juce::jlimit (-50.0f, 50.0f, shownCents) + 50.0f) / 100.0f * w;
        const auto pill = juce::Rectangle<float> (14.0f, 56.0f).withCentre ({ x, y });
        g.setColour (c.withAlpha (0.25f));
        g.fillRoundedRectangle (pill.expanded (6.0f, 4.0f), 10.0f);
        g.setColour (c);
        g.fillRoundedRectangle (pill, 7.0f);
    }

    g.setFont (font (22.0f, Weight::semibold));
    g.setColour (c);
    const juce::String status = ! live ? "Play one string" : (inTune ? "In tune" : (reading.cents < 0 ? "A little flat, tune up" : "A little sharp, tune down"));
    g.drawText (status, juce::Rectangle<float> (600.0f, 30.0f).withCentre ({ cx, y + 78.0f }), juce::Justification::centred);
    g.setFont (font (14.5f));
    g.setColour (colours::textDim);
    if (live)
        g.drawText (juce::String (reading.hz, 1) + juce::String (" Hz, ") + (reading.cents > 0 ? "+" : "") + juce::String (juce::roundToInt (reading.cents)) + " cents",
                    juce::Rectangle<float> (500.0f, 24.0f).withCentre ({ cx, y + 108.0f }), juce::Justification::centred);
    g.setColour (colours::textFaint);
    g.drawText ("Any tuning works: standard, drop, open or down-tuned. It always shows the nearest note.",
                juce::Rectangle<float> (760.0f, 24.0f).withCentre ({ cx, y + 140.0f }), juce::Justification::centred);
}

//==============================================================================
void Popover::paint (juce::Graphics& g)
{
    drawCard (g, getLocalBounds().toFloat().reduced (1.0f), colours::raised, 14.0f);
    if (title.isNotEmpty())
    {
        g.setColour (colours::text);
        g.setFont (font (17.0f, Weight::semibold));
        g.drawText (title, getLocalBounds().reduced (20, 16).removeFromTop (24), juce::Justification::centredLeft);
    }
}

//==============================================================================
RecordPanel::RecordPanel (RiffhouseProcessor& p) : proc (p)
{
    title = "Recording";
    auto& ui = proc.uiState;
    countIn.setToggleState ((bool) ui.getProperty ("countIn", true), juce::dontSendNotification);
    countIn.onClick = [this] { proc.uiState.setProperty ("countIn", countIn.getToggleState(), nullptr); };
    saveDi.setToggleState (proc.saveDi.load(), juce::dontSendNotification);
    saveDi.onClick = [this] { proc.saveDi.store (saveDi.getToggleState()); };
    openFolder.onClick = [this]
    {
        proc.recordingFolder.createDirectory();
        if (proc.recorder().lastTake().existsAsFile()) proc.recorder().lastTake().revealToUser();
        else proc.recordingFolder.startAsProcess();
    };
    changeFolder.onClick = [this]
    {
        chooser = std::make_unique<juce::FileChooser> ("Where should recordings go?", proc.recordingFolder);
        chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectDirectories,
                              [this] (const juce::FileChooser& fc)
                              {
                                  if (fc.getResult().isDirectory()) proc.recordingFolder = fc.getResult();
                                  repaint();
                              });
    };
    loadSong.onClick = [this] { chooseSong(); };
    playSong.onClick = [this] { proc.setJamPlaying (! proc.isJamPlaying()); timerCallback(); };
    playSong.setTooltip ("Play or pause the song");
    songLevel.setSliderStyle (juce::Slider::LinearHorizontal);
    songLevel.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    songLevel.setRange (0.0, 1.0);
    songLevel.setValue (proc.jamLevel.load(), juce::dontSendNotification);
    songLevel.onValueChange = [this] { proc.jamLevel.store ((float) songLevel.getValue()); };
    songLevel.setTooltip ("Song volume (the song is never recorded)");
    songPos.setSliderStyle (juce::Slider::LinearHorizontal);
    songPos.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    songPos.setRange (0.0, 1.0);
    songPos.onDragEnd = [this] { proc.setJamPosition (songPos.getValue() * proc.jamLength()); };
    for (juce::Component* c : { (juce::Component*) &countIn, (juce::Component*) &saveDi, (juce::Component*) &openFolder, (juce::Component*) &changeFolder,
                                (juce::Component*) &loadSong, (juce::Component*) &playSong, (juce::Component*) &songLevel, (juce::Component*) &songPos })
        addAndMakeVisible (c);
}

void RecordPanel::visibilityChanged()
{
    if (isVisible()) startTimerHz (10); else stopTimer();
}

void RecordPanel::timerCallback()
{
    const bool has = proc.jamTrackName().isNotEmpty();
    playSong.setEnabled (has);
    songPos.setEnabled (has);
    playSong.setIcon (proc.isJamPlaying() ? icons::pause() : icons::play(), true);
    if (has && ! songPos.isMouseButtonDown() && proc.jamLength() > 0)
        songPos.setValue (proc.jamPosition() / proc.jamLength(), juce::dontSendNotification);
}

void RecordPanel::chooseSong()
{
    chooser = std::make_unique<juce::FileChooser> ("Choose a song or backing track", juce::File::getSpecialLocation (juce::File::userMusicDirectory),
                                                   "*.wav;*.mp3;*.flac;*.ogg;*.aif;*.aiff;*.m4a");
    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                          [this] (const juce::FileChooser& fc)
                          {
                              const auto f = fc.getResult();
                              if (f.existsAsFile() && proc.loadJamTrack (f))
                              {
                                  loadSong.setButtonText (f.getFileNameWithoutExtension());
                                  proc.setJamPlaying (true);
                              }
                              timerCallback();
                              repaint();
                          });
}

void RecordPanel::resized()
{
    auto r = getLocalBounds().reduced (20, 16);
    r.removeFromTop (34);
    countIn.setBounds (r.removeFromTop (30));
    r.removeFromTop (4);
    saveDi.setBounds (r.removeFromTop (30));
    r.removeFromTop (34);
    auto row = r.removeFromTop (34);
    openFolder.setBounds (row.removeFromLeft (row.getWidth() / 2 - 5));
    row.removeFromLeft (10);
    changeFolder.setBounds (row);
    r.removeFromTop (44);
    loadSong.setBounds (r.removeFromTop (34));
    r.removeFromTop (10);
    row = r.removeFromTop (34);
    playSong.setBounds (row.removeFromLeft (34));
    row.removeFromLeft (10);
    songPos.setBounds (row);
    r.removeFromTop (6);
    row = r.removeFromTop (26);
    row.removeFromLeft (80);
    songLevel.setBounds (row);
}

void RecordPanel::paint (juce::Graphics& g)
{
    Popover::paint (g);
    auto r = getLocalBounds().reduced (20, 16);
    r.removeFromTop (34 + 30 + 4 + 30);
    g.setFont (font (12.5f));
    g.setColour (colours::textFaint);
    // inside the home folder the long machine path says nothing: show the part the player chose
    const auto home = juce::File::getSpecialLocation (juce::File::userHomeDirectory);
    const juce::String where = proc.recordingFolder.isAChildOf (home)
                                   ? proc.recordingFolder.getRelativePathFrom (home).replaceCharacter ('\\', '/').replace ("/", " / ")
                                   : proc.recordingFolder.getFullPathName();
    g.drawFittedText ("Saves to " + where, r.removeFromTop (34), juce::Justification::centredLeft, 2);
    r.removeFromTop (34);
    g.setColour (colours::line);
    g.fillRect (r.getX(), r.getY() + 6, r.getWidth(), 1);
    g.setColour (colours::text);
    g.setFont (font (15.0f, Weight::semibold));
    g.drawText ("Play along", r.removeFromTop (44).withTrimmedTop (8), juce::Justification::centredLeft);
    r.removeFromTop (34 + 10 + 34 + 6);
    g.setFont (font (13.0f));
    g.setColour (colours::textDim);
    g.drawText ("Song level", r.removeFromTop (26), juce::Justification::centredLeft);
    g.setColour (colours::textFaint);
    g.setFont (font (12.0f));
    g.drawFittedText ("The song and the click go to your headphones only. Recordings contain just your guitar.",
                      r.removeFromTop (36), juce::Justification::topLeft, 2, 1.0f);
}

//==============================================================================
MetronomePanel::MetronomePanel (RiffhouseProcessor& p) : proc (p)
{
    title = "Metronome";
    enabled.setToggleState (proc.metronomeOn.load(), juce::dontSendNotification);
    enabled.onClick = [this] { proc.metronomeOn.store (enabled.getToggleState()); if (onChange) onChange(); };
    bpm.setSliderStyle (juce::Slider::LinearHorizontal);
    bpm.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    bpm.setRange (30.0, 300.0, 1.0);
    bpm.setValue (proc.metronomeBpm.load(), juce::dontSendNotification);
    bpm.onValueChange = [this] { proc.metronomeBpm.store ((float) bpm.getValue()); repaint(); if (onChange) onChange(); };
    level.setSliderStyle (juce::Slider::LinearHorizontal);
    level.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    level.setRange (0.0, 1.0);
    level.setValue (proc.metronomeLevel.load(), juce::dontSendNotification);
    level.onValueChange = [this] { proc.metronomeLevel.store ((float) level.getValue()); };
    tapButton.onClick = [this] { tap(); };
    minus.onClick = [this] { bpm.setValue (bpm.getValue() - 1.0); };
    plus.onClick = [this] { bpm.setValue (bpm.getValue() + 1.0); };
    for (juce::Component* c : { (juce::Component*) &enabled, (juce::Component*) &bpm, (juce::Component*) &level,
                                (juce::Component*) &tapButton, (juce::Component*) &minus, (juce::Component*) &plus })
        addAndMakeVisible (c);
}

void MetronomePanel::tap()
{
    const double now = juce::Time::getMillisecondCounterHiRes();
    if (! taps.empty() && now - taps.back() > 2000.0) taps.clear();
    taps.push_back (now);
    if (taps.size() > 6) taps.erase (taps.begin());
    if (taps.size() >= 2)
    {
        const double avg = (taps.back() - taps.front()) / (double) (taps.size() - 1);
        bpm.setValue (juce::jlimit (30.0, 300.0, std::round (60000.0 / avg)));
    }
}

void MetronomePanel::resized()
{
    auto r = getLocalBounds().reduced (20, 16);
    r.removeFromTop (34);
    enabled.setBounds (r.removeFromTop (30));
    r.removeFromTop (64);
    auto row = r.removeFromTop (30);
    minus.setBounds (row.removeFromLeft (30));
    plus.setBounds (row.removeFromRight (30));
    row.reduce (10, 0);
    bpm.setBounds (row);
    r.removeFromTop (12);
    tapButton.setBounds (r.removeFromTop (34));
    r.removeFromTop (10);
    row = r.removeFromTop (26);
    row.removeFromLeft (60);
    level.setBounds (row);
}

void MetronomePanel::paint (juce::Graphics& g)
{
    Popover::paint (g);
    auto r = getLocalBounds().reduced (20, 16);
    r.removeFromTop (34 + 30);
    auto big = r.removeFromTop (64);
    const bool display = look() != Look::studio;
    if (display) drawWell (g, big.toFloat().withSizeKeepingCentre (200.0f, 60.0f), 10.0f);
    g.setColour (display ? colours::accent : colours::text);
    g.setFont (display ? font (40.0f, Weight::numeric) : font (40.0f, Weight::display));
    g.drawText (juce::String (juce::roundToInt (bpm.getValue())), big, juce::Justification::centred);
    g.setFont (font (14.0f));
    g.setColour (colours::textDim);
    g.setColour (colours::textDim);
    g.drawText ("BPM", big.withTrimmedLeft (130), juce::Justification::centred);
    r.removeFromTop (30 + 12 + 34 + 10);
    g.drawText ("Volume", r.removeFromTop (26), juce::Justification::centredLeft);
}

//==============================================================================
SettingsView::SettingsView (RiffhouseProcessor& p)
    : proc (p), inGain (p, P::inGain, colours::accent, "Input level"), gate (p, P::gateThresh, juce::Colour (0xff94a3b8), "Gate threshold")
{
    close.onClick = [this] { if (onClose) onClose(); };
    addAndMakeVisible (close);
    const bool standalone = StandaloneHost::get() != nullptr;
    runSetup.onClick = [this] { if (onRunSetup) onRunSetup(); };
    advanced.onClick = [this] { showAdvancedAudio(); };
    runSetup.setVisible (standalone);
    advanced.setVisible (standalone);
    addChildComponent (runSetup);
    addChildComponent (advanced);
    runSetup.setColour (juce::TextButton::buttonColourId, colours::accent);
    runSetup.setColour (juce::TextButton::textColourOffId, colours::bg);
    website.onClick = [] { juce::URL ("https://github.com/NatikChhabra/riffhouse").launchInDefaultBrowser(); };
    addAndMakeVisible (website);

    if (auto* host = StandaloneHost::get())
        if (auto* up = host->updater())
        {
            addAndMakeVisible (updateButton);
            addAndMakeVisible (autoUpdate);
            autoUpdate.setToggleState (up->automatic(), juce::dontSendNotification);
            autoUpdate.onClick = [this]
            {
                if (auto* h = StandaloneHost::get())
                    if (auto* u = h->updater()) u->setAutomatic (autoUpdate.getToggleState());
            };
            updateButton.onClick = [this]
            {
                auto* h = StandaloneHost::get();
                auto* u = h != nullptr ? h->updater() : nullptr;
                if (u == nullptr) return;
                if (u->canInstall()) h->restartToUpdate();
                else if (u->state() == Updater::State::available && u->release().pageUrl.startsWith ("https://github.com/"))
                    juce::URL (u->release().pageUrl).launchInDefaultBrowser();
                else u->checkNow();
            };
            refreshUpdate();
        }

    for (auto* k : { &inGain, &gate }) // the same knob as the bottom bar, not a second kind
        k->setGear (KnobStyle::modern, colours::text, colours::textDim, Face::instrumentSemi, true);
    addAndMakeVisible (inGain);
    addAndMakeVisible (gate);
    addAndMakeVisible (inputFix);
    addAndMakeVisible (gateOn);
    inputFixAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (proc.state(), spec (P::inputFix).key, inputFix);
    gateOnAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (proc.state(), spec (P::gateOn).key, gateOn);
    hum = std::make_unique<ChipRow> (proc, P::humFilter, colours::accent);
    addAndMakeVisible (*hum);
    looks = std::make_unique<ChipRow> (juce::StringArray { lookName (Look::studio), lookName (Look::signal), lookName (Look::pedalboard) },
                                       colours::accent, [this] (int i) { if (onLookChange) onLookChange ((Look) i); });
    looks->setSelected ((int) look());
    addChildComponent (*looks); // one look now: the gear brings the colour
    addAndMakeVisible (inMeter);
}

SettingsView::~SettingsView() = default;

void SettingsView::visibilityChanged()
{
    if (isVisible()) { summary = deviceSummary(); startTimerHz (30); }
    else stopTimer();
}

void SettingsView::refreshUpdate()
{
    auto* host = StandaloneHost::get();
    auto* up = host != nullptr ? host->updater() : nullptr;
    if (up == nullptr) return;
    const auto st = up->state();
    const bool busy = st == Updater::State::checking || st == Updater::State::downloading;
    // an update never interrupts a take: the button waits until recording has stopped
    const bool recording = proc.recorder().isRecording();
    const auto label = up->canInstall() ? juce::String (recording ? "Finish recording to update" : "Restart and update")
                     : st == Updater::State::available ? juce::String ("Get version " + up->release().version)
                     : juce::String ("Check for updates");
    if (updateButton.getButtonText() != label) updateButton.setButtonText (label);
    updateButton.setEnabled (! busy && ! (up->canInstall() && recording));
    const auto text = up->statusText();
    if (text != lastUpdateText) { lastUpdateText = text; repaint (updateStatus); }
}

void SettingsView::timerCallback()
{
    refreshUpdate();
    inMeter.setLevel (proc.meters().gateInPeak.load());
    const auto s = deviceSummary();
    if (s != summary) { summary = s; repaint(); }
    static int tick = 0;
    if (++tick % 10 == 0) repaint (getLocalBounds().withTrimmedTop (getHeight() / 2));
}

juce::String SettingsView::deviceSummary() const
{
    auto* host = StandaloneHost::get();
    if (host == nullptr)
        return "Riffhouse is running inside another app, which handles audio devices. Latency added by Riffhouse: "
               + juce::String (1000.0 * proc.getLatencySamples() / std::max (1.0, proc.currentSampleRate()), 2) + " ms.";
    auto* dev = host->deviceManager().getCurrentAudioDevice();
    if (dev == nullptr) return "No audio device is open. Run the automatic setup.";
    juce::AudioDeviceManager::AudioDeviceSetup setup;
    host->deviceManager().getAudioDeviceSetup (setup);
    const double sr = dev->getCurrentSampleRate();
    const int buf = dev->getCurrentBufferSizeSamples();
    int lat = dev->getInputLatencyInSamples() + dev->getOutputLatencyInSamples();
    if (lat <= 0) lat = 2 * buf;
    lat += proc.getLatencySamples();
    juce::String s;
    s << dev->getTypeName() << "\n"
      << "In: " << (setup.inputDeviceName.isNotEmpty() ? setup.inputDeviceName : juce::String ("none")) << "\n"
      << "Out: " << (setup.outputDeviceName.isNotEmpty() ? setup.outputDeviceName : juce::String ("none")) << "\n"
      << buf << " samples at " << juce::String (sr / 1000.0, 1) << " kHz, about "
      << juce::String (1000.0 * lat / std::max (1.0, sr), 1) << " ms from string to ear";
    const int xr = dev->getXRunCount();
    if (xr > 0) s << ", " << xr << " dropouts";
    return s;
}

void SettingsView::showAdvancedAudio()
{
    auto* host = StandaloneHost::get();
    if (host == nullptr) return;
    auto* sel = new juce::AudioDeviceSelectorComponent (host->deviceManager(), 1, 2, 2, 2, false, false, true, false);
    sel->setSize (520, 420);
    juce::DialogWindow::LaunchOptions o;
    o.content.setOwned (sel);
    o.dialogTitle = "Audio settings";
    o.dialogBackgroundColour = colours::panel;
    o.escapeKeyTriggersCloseButton = true;
    o.useNativeTitleBar = true;
    o.resizable = false;
    o.componentToCentreAround = this;
    o.launchAsync();
}

void SettingsView::resized()
{
    close.setBounds (getWidth() - 70, 20, 46, 46);
    auto r = getLocalBounds().reduced (60, 40);
    r.removeFromTop (50);
    auto cols = r;
    auto left = cols.removeFromLeft (cols.getWidth() / 2 - 20);
    cols.removeFromLeft (40);
    auto right = cols;

    // left: audio
    left.removeFromTop (36);
    left.removeFromTop (96); // summary text
    auto row = left.removeFromTop (40);
    runSetup.setBounds (row.removeFromLeft (250));
    row.removeFromLeft (10);
    advanced.setBounds (row.removeFromLeft (220));
    left.removeFromTop (40);
    left.removeFromTop (36);
    left.removeFromTop (110); // about text
    website.setBounds (left.removeFromTop (36).removeFromLeft (220));
    left.removeFromTop (28);
    updateHeading = left.removeFromTop (36);
    updateStatus = left.removeFromTop (24);
    left.removeFromTop (8);
    autoUpdate.setBounds (left.removeFromTop (30));
    left.removeFromTop (12);
    updateButton.setBounds (left.removeFromTop (36).removeFromLeft (250));

    // right: input
    right.removeFromTop (36);
    auto knobs = right.removeFromTop (130);
    inGain.setBounds (knobs.removeFromLeft (120));
    knobs.removeFromLeft (10);
    gate.setBounds (knobs.removeFromLeft (120));
    knobs.removeFromLeft (24);
    knobs.removeFromTop (18);
    meterCaption = knobs.removeFromTop (22);
    inMeter.setBounds (knobs.removeFromTop (8).withTrimmedRight (10));
    knobs.removeFromTop (18);
    gateOn.setBounds (knobs.removeFromTop (30));
    right.removeFromTop (14);
    inputFix.setBounds (right.removeFromTop (30));
    right.removeFromTop (40);
    right.removeFromTop (26);
    hum->setBounds (right.removeFromTop (34));
}

void SettingsView::paint (juce::Graphics& g)
{
    g.fillAll (colours::bg);
    auto r = getLocalBounds().reduced (60, 40);
    g.setColour (colours::text);
    g.setFont (font (28.0f, Weight::display));
    g.drawText ("Settings", r.removeFromTop (50), juce::Justification::centredLeft);
    auto cols = r;
    auto left = cols.removeFromLeft (cols.getWidth() / 2 - 20);
    cols.removeFromLeft (40);
    auto right = cols;

    auto heading = [&g] (juce::Rectangle<int> a, const juce::String& t)
    {
        g.setColour (colours::textFaint);
        g.setFont (font (12.5f, Weight::semibold));
        g.drawText (t.toUpperCase(), a, juce::Justification::centredLeft);
    };
    heading (left.removeFromTop (36), "Audio");
    g.setColour (colours::text);
    g.setFont (font (15.0f));
    g.drawFittedText (summary, left.removeFromTop (96), juce::Justification::topLeft, 5, 1.0f);
    left.removeFromTop (40 + 40);
    heading (left.removeFromTop (36), "About");
    g.setColour (colours::textDim);
    g.setFont (font (13.5f));
    g.drawFittedText ("Riffhouse " JucePlugin_VersionString ". Free and open source under the AGPL-3.0 license.\n"
                      "Tone names saying \"Inspired by\" describe a style. Riffhouse has no affiliation with, and is not endorsed by, "
                      "any artist, band or equipment maker. All trademarks belong to their owners.\n"
                      "Built with JUCE and Neural Amp Modeler Core. Fonts: Inter, Instrument Sans, Doto and Big Shoulders (SIL Open Font License).",
                      left.removeFromTop (110), juce::Justification::topLeft, 7, 1.0f);
    if (updateButton.isVisible())
    {
        heading (updateHeading, "Updates");
        g.setColour (colours::text);
        g.setFont (font (15.0f));
        g.drawText (lastUpdateText, updateStatus, juce::Justification::centredLeft);
    }

    heading (right.removeFromTop (36), "Your guitar input");
    g.setColour (colours::textDim);
    g.setFont (font (13.0f));
    g.drawText ("Level going into the rig", meterCaption, juce::Justification::centredLeft);
    right.removeFromTop (130 + 14 + 30);
    g.setColour (colours::textDim);
    g.setFont (font (13.0f));
    g.drawFittedText ("Setup measures how much treble your input loses and sets the strength (now "
                          + juce::String (juce::roundToInt (proc.value (P::inputFixAmt))) + " dB). Interfaces with a guitar (Hi-Z) input don't need it.",
                      right.removeFromTop (40).withTrimmedLeft (44), juce::Justification::topLeft, 2, 1.0f);
    g.setColour (colours::text);
    g.setFont (font (14.5f, Weight::medium));
    auto humRow = right.removeFromTop (26);
    g.drawText ("Mains hum filter", humRow, juce::Justification::centredLeft);
    const int mains = proc.meters().mains.load();
    g.setColour (mains > 0 ? colours::good : colours::textFaint);
    g.setFont (font (13.0f));
    g.drawText (mains > 0 ? "Found " + juce::String (mains) + " Hz hum, removing it" : "No hum detected", humRow, juce::Justification::centredRight);
    right.removeFromTop (34 + 10);
    g.setColour (colours::textDim);
    g.setFont (font (13.0f));
    g.drawFittedText ("Auto listens for the 50 Hz (India, Europe, most of Asia and Africa) or 60 Hz (Americas) buzz from "
                      "power lines and removes just that, leaving your low notes alone.",
                      right.removeFromTop (40), juce::Justification::topLeft, 3, 1.0f);
}

//==============================================================================
SaveDialog::SaveDialog (RiffhouseProcessor& p) : proc (p)
{
    title = "Save tone";
    name.setFont (font (16.0f));
    name.setIndents (12, 8);
    name.setTextToShowWhenEmpty ("Name your tone", colours::textFaint);
    name.onReturnKey = [this] { save(); };
    name.onEscapeKey = [this] { setVisible (false); };
    addAndMakeVisible (name);
    category.addItemList ({ "My Tones", "Clean", "Crunch", "Rock", "Blues", "Metal", "Lead", "Funk", "Ambient", "Lo-Fi", "Bass" }, 1);
    category.setSelectedId (1, juce::dontSendNotification);
    addAndMakeVisible (category);
    saveButton.setColour (juce::TextButton::buttonColourId, colours::accent);
    saveButton.setColour (juce::TextButton::textColourOffId, colours::bg);
    saveButton.onClick = [this] { save(); };
    cancel.onClick = [this] { setVisible (false); };
    addAndMakeVisible (saveButton);
    addAndMakeVisible (cancel);
}

void SaveDialog::visibilityChanged()
{
    if (isVisible())
    {
        auto n = proc.currentPresetName();
        const auto idx = proc.currentPresetIndex();
        if (idx >= 0 && proc.presets().all()[(size_t) idx].factory) n << " (mine)";
        name.setText (n, false);
        name.selectAll();
        name.grabKeyboardFocus();
    }
}

void SaveDialog::save()
{
    const auto n = name.getText().trim();
    if (n.isEmpty()) return;
    const auto f = proc.savePreset (n, category.getText());
    setVisible (false);
    if (onSaved) onSaved ("Saved \"" + n + "\"");
    juce::ignoreUnused (f);
}

void SaveDialog::resized()
{
    auto r = getLocalBounds().reduced (20, 16);
    r.removeFromTop (34);
    name.setBounds (r.removeFromTop (40));
    r.removeFromTop (10);
    category.setBounds (r.removeFromTop (36));
    r.removeFromTop (14);
    auto row = r.removeFromTop (36);
    saveButton.setBounds (row.removeFromRight (120));
    row.removeFromRight (10);
    cancel.setBounds (row.removeFromRight (100));
}

void SaveDialog::paint (juce::Graphics& g) { Popover::paint (g); }
} // namespace rh::ui
