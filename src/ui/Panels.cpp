// Riffhouse - free, open-source guitar rig
// Copyright (C) 2026 Riffhouse contributors. Licensed under AGPL-3.0-or-later.
#include "AudioPicker.h"
#include "Panels.h"
#include "../plugin/DeviceLog.h"
#include "../plugin/StandaloneHost.h"
#include "../plugin/Updater.h"
#include "../plugin/Version.h"

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
    mute.setBounds (juce::Rectangle<int> (290, 28).withCentre ({ getWidth() / 2, 600 })); // with the rest of the tuner, not at the far edge
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
        if (major)
        {
            g.setFont (font (14.0f, Weight::medium));
            g.setColour (cents == 0 ? colours::good : colours::textDim);
            g.drawText ((cents > 0 ? "+" : "") + juce::String (cents), juce::Rectangle<float> (50.0f, 18.0f).withCentre ({ x, y + 40.0f }), juce::Justification::centred);
        }
    }
    g.setFont (font (13.0f, Weight::semibold));
    g.setColour (colours::textFaint);
    g.drawText ("FLAT", juce::Rectangle<float> (x0 - 90.0f, y - 10.0f, 70.0f, 20.0f), juce::Justification::centredRight);
    g.drawText ("SHARP", juce::Rectangle<float> (x0 + w + 20.0f, y - 10.0f, 70.0f, 20.0f), juce::Justification::centredLeft);

    if (live)
    {
        const float x = x0 + (juce::jlimit (-50.0f, 50.0f, shownCents) + 50.0f) / 100.0f * w;
        // a needle: one thin line through the scale, with a small head above it to find it by
        g.setColour (c);
        g.fillRect (x - 1.25f, y - 34.0f, 2.5f, 68.0f);
        juce::Path head;
        head.addTriangle (x - 7.0f, y - 46.0f, x + 7.0f, y - 46.0f, x, y - 34.0f);
        g.fillPath (head);
    }

    g.setFont (font (22.0f, Weight::semibold));
    g.setColour (c);
    const juce::String status = ! live ? "Play one string" : (inTune ? "In tune" : (reading.cents < 0 ? "A little flat, tune up" : "A little sharp, tune down"));
    g.drawText (status, juce::Rectangle<float> (600.0f, 30.0f).withCentre ({ cx, y + 86.0f }), juce::Justification::centred);
    g.setFont (font (14.5f));
    g.setColour (colours::textDim);
    if (live)
        g.drawText (juce::String (reading.hz, 1) + juce::String (" Hz, ") + (reading.cents > 0 ? "+" : "") + juce::String (juce::roundToInt (reading.cents)) + " cents",
                    juce::Rectangle<float> (500.0f, 24.0f).withCentre ({ cx, y + 116.0f }), juce::Justification::centred);
    g.setColour (colours::textFaint);
    g.drawText ("Any tuning works: standard, drop, open or down-tuned. It always shows the nearest note.",
                juce::Rectangle<float> (760.0f, 24.0f).withCentre ({ cx, y + 146.0f }), juce::Justification::centred);
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
    openFolder.onClick = [this] { if (onShowTakes) onShowTakes(); };
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
    songSpeed.setSliderStyle (juce::Slider::LinearHorizontal);
    songSpeed.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    songSpeed.setRange (SongStretch::minSpeed * 100.0, SongStretch::maxSpeed * 100.0, 1.0);
    songSpeed.setValue (proc.jamSpeed() * 100.0, juce::dontSendNotification);
    songSpeed.setDoubleClickReturnValue (true, 100.0);
    songSpeed.onValueChange = [this]
    {
        // normal speed is where the slider wants to rest
        const double v = std::abs (songSpeed.getValue() - 100.0) < 2.5 ? 100.0 : songSpeed.getValue();
        proc.setJamSpeed ((float) (v / 100.0));
        repaint();
    };
    songSpeed.onDragEnd = [this] { songSpeed.setValue (proc.jamSpeed() * 100.0, juce::dontSendNotification); };
    songSpeed.setTooltip ("Slower or faster, at the same pitch. Double-click for normal speed.");
    keyDown.onClick = [this] { proc.setJamKey (proc.jamKey() - 1); repaint(); };
    keyUp.onClick = [this] { proc.setJamKey (proc.jamKey() + 1); repaint(); };
    keyDown.setTooltip ("The song one semitone lower, at the same speed");
    keyUp.setTooltip ("The song one semitone higher, at the same speed");
    partFrom.onClick = [this]
    {
        const double at = proc.jamPosition();
        partNote = {};
        if (proc.jamHasSection() && proc.setJamSection (at, proc.jamSectionEnd())) {} // the part now starts here
        else { proc.clearJamSection(); partStart = at; }
        repaint();
    };
    partTo.onClick = [this]
    {
        const double at = proc.jamPosition();
        const double from = proc.jamHasSection() ? proc.jamSectionStart() : std::max (0.0, partStart);
        if (proc.setJamSection (from, at)) { partStart = -1.0; partNote = {}; }
        else partNote = at - from < SongSection::shortest ? "That part is too short to go round." : "That part could not be read.";
        repaint();
    };
    partClear.onClick = [this] { proc.clearJamSection(); partStart = -1.0; partNote = {}; repaint(); };
    partFrom.setTooltip ("The part to go round starts where the song is now");
    partTo.setTooltip ("The part ends where the song is now, and starts going round");
    partClear.setTooltip ("Stop going round and play the whole song");
    for (juce::Component* c : { (juce::Component*) &countIn, (juce::Component*) &saveDi, (juce::Component*) &openFolder, (juce::Component*) &changeFolder,
                                (juce::Component*) &loadSong, (juce::Component*) &playSong, (juce::Component*) &songLevel, (juce::Component*) &songPos,
                                (juce::Component*) &songSpeed, (juce::Component*) &keyDown, (juce::Component*) &keyUp,
                                (juce::Component*) &partFrom, (juce::Component*) &partTo, (juce::Component*) &partClear })
        addAndMakeVisible (c);
}

void RecordPanel::visibilityChanged()
{
    if (isVisible()) startTimerHz (10); else stopTimer();
}

void RecordPanel::timerCallback()
{
    const bool has = proc.jamTrackName().isNotEmpty();
    if (has && loadSong.getButtonText() != proc.jamTrackName()) loadSong.setButtonText (proc.jamTrackName()); // a song dropped on the window
    for (juce::Component* c : { (juce::Component*) &playSong, (juce::Component*) &songPos, (juce::Component*) &songSpeed,
                                (juce::Component*) &partFrom, (juce::Component*) &partTo })
        c->setEnabled (has);
    keyDown.setEnabled (has && proc.jamKey() > -SongStretch::maxKeyShift);
    keyUp.setEnabled (has && proc.jamKey() < SongStretch::maxKeyShift);
    partClear.setEnabled (has && (proc.jamHasSection() || partStart >= 0.0));
    if (! songSpeed.isMouseButtonDown() && std::abs (songSpeed.getValue() - proc.jamSpeed() * 100.0) > 0.5)
    {
        // a new song is back at normal speed, and its part is gone
        songSpeed.setValue (proc.jamSpeed() * 100.0, juce::dontSendNotification);
        partStart = -1.0;
        partNote = {};
        repaint();
    }
    if (proc.jamHasSection() || partStart >= 0.0) repaint (songPos.getBounds().expanded (0, 6));
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
                                  partStart = -1.0;
                                  partNote = {};
                                  songSpeed.setValue (100.0, juce::dontSendNotification);
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
    r.removeFromTop (4);
    row = r.removeFromTop (26);
    row.removeFromLeft (80);
    row.removeFromRight (48);
    songSpeed.setBounds (row);
    r.removeFromTop (6);
    row = r.removeFromTop (30);
    row.removeFromLeft (80 + 6);
    keyDown.setBounds (row.removeFromLeft (34));
    keyUp.setBounds (row.removeFromRight (34));
    r.removeFromTop (12 + 24);
    row = r.removeFromTop (32);
    const int third = (row.getWidth() - 16) / 3;
    partFrom.setBounds (row.removeFromLeft (third));
    row.removeFromLeft (8);
    partTo.setBounds (row.removeFromLeft (third));
    row.removeFromLeft (8);
    partClear.setBounds (row);
}

namespace
{
juce::String minSec (double seconds)
{
    const int s = (int) std::max (0.0, seconds);
    return juce::String (s / 60) + ":" + juce::String (s % 60).paddedLeft ('0', 2);
}
}

void RecordPanel::paintOverChildren (juce::Graphics& g)
{
    // the part that goes round, drawn on the line that shows where the song is
    const double length = proc.jamLength();
    if (length <= 0.0) return;
    const auto track = songPos.getBounds().toFloat();
    const auto xAt = [&] (double seconds) { return track.getX() + songPos.getPositionOfValue (juce::jlimit (0.0, 1.0, seconds / length)); };
    const float y = track.getCentreY() + 9.0f;
    g.setColour (colours::lamp);
    if (proc.jamHasSection())
    {
        const float a = xAt (proc.jamSectionStart()), b = xAt (proc.jamSectionEnd());
        g.fillRect (a, y, std::max (2.0f, b - a), 2.0f);
        g.fillRect (a, y - 4.0f, 2.0f, 6.0f);
        g.fillRect (std::max (a, b - 2.0f), y - 4.0f, 2.0f, 6.0f);
    }
    else if (partStart >= 0.0)
        g.fillRect (xAt (partStart), y - 4.0f, 2.0f, 6.0f);
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
    r.removeFromTop (4);
    auto row = r.removeFromTop (26);
    g.drawText ("Speed", row, juce::Justification::centredLeft);
    const bool has = proc.jamTrackName().isNotEmpty();
    const int percent = juce::roundToInt (proc.jamSpeed() * 100.0f);
    g.setColour (! has ? colours::textFaint : percent == 100 ? colours::textDim : colours::text);
    g.drawText (juce::String (percent) + "%", row, juce::Justification::centredRight);
    r.removeFromTop (6);
    row = r.removeFromTop (30);
    g.setColour (colours::textDim);
    g.drawText ("Key", row, juce::Justification::centredLeft);
    const int key = proc.jamKey();
    g.setColour (! has ? colours::textFaint : key == 0 ? colours::textDim : colours::text);
    g.drawText (key == 0 ? juce::String ("As recorded")
                         : juce::String (std::abs (key)) + (std::abs (key) == 1 ? " semitone " : " semitones ") + (key > 0 ? "higher" : "lower"),
                row.withTrimmedLeft (80 + 6 + 34).withTrimmedRight (34), juce::Justification::centred);
    r.removeFromTop (12);
    row = r.removeFromTop (24);
    g.setColour (colours::textDim);
    g.drawText ("Go round one part", row, juce::Justification::centredLeft);
    juce::String part = partNote;
    if (part.isEmpty() && proc.jamHasSection()) part = minSec (proc.jamSectionStart()) + " to " + minSec (proc.jamSectionEnd());
    if (part.isEmpty() && partStart >= 0.0) part = "From " + minSec (partStart) + ". Now mark the end.";
    g.setColour (partNote.isNotEmpty() ? colours::hot : colours::text);
    g.setFont (font (12.5f));
    g.drawText (part, row, juce::Justification::centredRight);
    r.removeFromTop (32 + 8);
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

//==============================================================================
LooperPanel::LooperPanel (RiffhouseProcessor& p) : proc (p)
{
    title = "Looper";
    main.onClick = [this] { proc.loopPress(); timerCallback(); };
    stopButton.onClick = [this] { proc.looper().stop(); };
    clearButton.onClick = [this] { proc.looper().clear(); };
    level.setSliderStyle (juce::Slider::LinearHorizontal);
    level.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    level.setRange (0.0, 1.0);
    level.setValue (proc.looper().level.load(), juce::dontSendNotification);
    level.onValueChange = [this] { proc.looper().level.store ((float) level.getValue()); };
    for (juce::Component* c : { (juce::Component*) &main, (juce::Component*) &stopButton, (juce::Component*) &clearButton, (juce::Component*) &level })
        addAndMakeVisible (c);
    timerCallback();
}

juce::String LooperPanel::stateLine (RiffhouseProcessor& proc)
{
    auto& lp = proc.looper();
    const auto secs = [] (double s) { return juce::String (s, 1) + " s"; };
    switch (lp.state())
    {
        case Looper::State::empty:       return "Nothing recorded yet";
        case Looper::State::recording:   return "Recording " + secs (lp.positionSeconds());
        case Looper::State::playing:     return "Playing a loop of " + secs (lp.lengthSeconds());
        case Looper::State::overdubbing: return "Adding a layer to a loop of " + secs (lp.lengthSeconds());
        case Looper::State::stopped:     return "Stopped, with a loop of " + secs (lp.lengthSeconds());
    }
    return {};
}

void LooperPanel::visibilityChanged()
{
    if (isVisible()) { timerCallback(); startTimerHz (20); } else stopTimer();
}

void LooperPanel::timerCallback()
{
    const auto st = proc.looper().state();
    // the button always says what the next press will do
    const char* next = st == Looper::State::empty ? "Record"
                     : st == Looper::State::recording ? "Close the loop and play it"
                     : st == Looper::State::playing ? "Add a layer"
                     : st == Looper::State::overdubbing ? "Stop adding"
                     : "Play";
    if (main.getButtonText() != next) main.setButtonText (next);
    stopButton.setEnabled (st != Looper::State::empty && st != Looper::State::stopped);
    clearButton.setEnabled (st != Looper::State::empty);
    repaint();
}

void LooperPanel::resized()
{
    auto r = getLocalBounds().reduced (20, 16);
    r.removeFromTop (34);
    r.removeFromTop (26 + 8 + 10 + 16); // the state line and the bar that shows where the loop is
    main.setBounds (r.removeFromTop (38));
    r.removeFromTop (10);
    auto row = r.removeFromTop (32);
    stopButton.setBounds (row.removeFromLeft ((row.getWidth() - 10) / 2));
    row.removeFromLeft (10);
    clearButton.setBounds (row);
    r.removeFromTop (14);
    row = r.removeFromTop (26);
    row.removeFromLeft (60);
    level.setBounds (row);
}

void LooperPanel::paint (juce::Graphics& g)
{
    Popover::paint (g);
    auto r = getLocalBounds().reduced (20, 16);
    r.removeFromTop (34);
    auto& lp = proc.looper();
    const auto st = lp.state();
    g.setFont (font (14.5f, Weight::medium));
    g.setColour (st == Looper::State::recording ? colours::record : st == Looper::State::empty ? colours::textDim : colours::text);
    g.drawText (stateLine (proc), r.removeFromTop (26), juce::Justification::centredLeft);
    r.removeFromTop (8);
    // where the loop is: the bar fills once per time round; while recording it fills toward the longest a loop can be
    const auto bar = r.removeFromTop (10).toFloat();
    g.setColour (juce::Colours::white.withAlpha (0.08f));
    g.fillRoundedRectangle (bar, 2.0f);
    const double whole = st == Looper::State::recording ? lp.maxSeconds() : lp.lengthSeconds();
    if (whole > 0.0 && st != Looper::State::empty)
    {
        g.setColour (st == Looper::State::recording ? colours::record : st == Looper::State::overdubbing ? colours::hot : colours::accent);
        g.fillRoundedRectangle (bar.withWidth (bar.getWidth() * (float) juce::jlimit (0.0, 1.0, lp.positionSeconds() / whole)), 2.0f);
    }
    r.removeFromTop (16 + 38 + 10 + 32 + 14);
    g.setFont (font (14.0f));
    g.setColour (colours::textDim);
    g.drawText ("Volume", r.removeFromTop (26), juce::Justification::centredLeft);
    r.removeFromTop (8);
    g.setFont (font (12.5f));
    g.drawText ("The O key is the big button.", r.removeFromTop (18), juce::Justification::centredLeft);
    g.drawText ("The loop is for your ears. Takes you record leave it out.", r.removeFromTop (18), juce::Justification::centredLeft);
}

void MetronomePanel::tap()
{
    proc.tapTempo(); // the same tap a footswitch gives
    bpm.setValue (proc.metronomeBpm.load());
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
// What each controller does, one line each, with the two things you can do to a line.
class SettingsView::FootList : public juce::Component
{
public:
    explicit FootList (RiffhouseProcessor& p) : proc (p) {}
    static constexpr int rowH = 28;

    void refresh()
    {
        setSize (getWidth(), std::max (rowH, (int) proc.footControl().bindings().size() * rowH));
        repaint();
    }

    void paint (juce::Graphics& g) override
    {
        const auto& list = proc.footControl().bindings();
        g.setFont (font (13.5f));
        if (list.empty())
        {
            g.setColour (colours::textFaint);
            g.drawText ("Nothing is taught yet.", getLocalBounds().removeFromTop (rowH), juce::Justification::centredLeft);
            return;
        }
        for (size_t i = 0; i < list.size(); ++i)
        {
            const auto a = areas ((int) i, list[i].target.isSwitch());
            g.setColour (colours::textDim);
            g.drawText ("Controller " + juce::String (list[i].controller), a.number, juce::Justification::centredLeft);
            g.setColour (colours::text);
            g.drawText (juce::String (footTargetName (list[i].target)), a.name, juce::Justification::centredLeft, true);
            if (list[i].target.isSwitch()) link (g, list[i].press ? "Momentary" : "Latching", a.kind);
            link (g, "Remove", a.remove);
        }
    }

    void mouseUp (const juce::MouseEvent& e) override
    {
        const auto& list = proc.footControl().bindings();
        const int i = e.y / rowH;
        if (i < 0 || i >= (int) list.size()) return;
        const auto a = areas (i, list[(size_t) i].target.isSwitch());
        if (a.remove.contains (e.getPosition())) proc.footRemove ((size_t) i);
        else if (a.kind.contains (e.getPosition())) proc.footSetPress ((size_t) i, ! list[(size_t) i].press);
        else return;
        refresh();
    }

    void mouseMove (const juce::MouseEvent& e) override
    {
        const auto& list = proc.footControl().bindings();
        const int i = e.y / rowH;
        bool onLink = false;
        if (i >= 0 && i < (int) list.size())
        {
            const auto a = areas (i, list[(size_t) i].target.isSwitch());
            onLink = a.remove.contains (e.getPosition()) || a.kind.contains (e.getPosition());
        }
        setMouseCursor (onLink ? juce::MouseCursor::PointingHandCursor : juce::MouseCursor::NormalCursor);
    }

private:
    struct Areas { juce::Rectangle<int> number, name, kind, remove; };
    Areas areas (int row, bool isSwitch) const
    {
        auto r = juce::Rectangle<int> (0, row * rowH, getWidth(), rowH);
        Areas a;
        a.number = r.removeFromLeft (112);
        a.remove = r.removeFromRight (62);
        r.removeFromRight (14);
        if (isSwitch) { a.kind = r.removeFromRight (84); r.removeFromRight (10); }
        a.name = r;
        return a;
    }
    static void link (juce::Graphics& g, const juce::String& text, juce::Rectangle<int> area)
    {
        const auto f = font (13.5f);
        g.setFont (f);
        g.setColour (colours::text);
        g.drawText (text, area, juce::Justification::centredRight);
        const int w = juce::roundToInt (juce::GlyphArrangement::getStringWidth (f, text));
        g.setColour (colours::textDim);
        g.fillRect (area.getRight() - w, area.getCentreY() + 9, w, 1);
    }
    RiffhouseProcessor& proc;
};

SettingsView::SettingsView (RiffhouseProcessor& p)
    : proc (p), inGain (p, P::inGain, colours::accent, "Input level"), gate (p, P::gateThresh, juce::Colour (0xff94a3b8), "Gate threshold")
{
    close.onClick = [this] { if (onClose) onClose(); };
    addAndMakeVisible (close);
    scroller.setViewedComponent (&page, false);
    scroller.setScrollBarsShown (true, false);
    scroller.setScrollBarThickness (8);
    addAndMakeVisible (scroller);
    const bool standalone = StandaloneHost::get() != nullptr;
    runSetup.onClick = [this] { if (onRunSetup) onRunSetup(); };
    advanced.onClick = [this] { showAdvancedAudio(); };
    runSetup.setVisible (standalone);
    advanced.setVisible (standalone);
    page.addChildComponent (runSetup);
    page.addChildComponent (advanced);
    runSetup.setColour (juce::TextButton::buttonColourId, colours::accent);
    runSetup.setColour (juce::TextButton::textColourOffId, colours::bg);
    website.setTooltip ("Opens the page of common problems and how to fix them.");
    website.onClick = [] { juce::URL ("https://github.com/NatikChhabra/riffhouse/blob/main/docs/TROUBLESHOOTING.md").launchInDefaultBrowser(); };
    page.addAndMakeVisible (website);
    problem.setTooltip ("Copies the report to your clipboard and opens the page where problems are reported. "
                        "Paste it there and say what went wrong. Nothing is sent by itself.");
    problem.onClick = [this]
    {
        juce::SystemClipboard::copyTextToClipboard (reportText());
        juce::URL ("https://github.com/NatikChhabra/riffhouse/issues/new")
            .withParameter ("title", "Problem: ")
            .withParameter ("body", "What happened:\n\n\nWhat I expected:\n\n\nReport (press Ctrl+V here, Riffhouse put it on your clipboard):\n\n\n\nRiffhouse "
                                        + versionLabel() + " on " + juce::SystemStats::getOperatingSystemName())
            .launchInDefaultBrowser();
        if (onMessage) onMessage ("The report is on your clipboard. Paste it into the page that just opened.");
    };
    page.addAndMakeVisible (problem);
    report.setTooltip ("Writes your settings and what setup measured into a text file in your recordings folder. "
                       "Nothing is sent anywhere. Attach it when you ask for help.");
    report.onClick = [this] { saveReport(); };
    page.addAndMakeVisible (report);

    // starts from what Windows is set to ("Animation effects"); the choice made here is kept
    lessMotion.setToggleState (motion::reduced(), juce::dontSendNotification);
    lessMotion.onClick = [this]
    {
        motion::setReduced (lessMotion.getToggleState());
        proc.uiState.setProperty ("lessMotion", lessMotion.getToggleState() ? 1 : 0, nullptr);
    };
    page.addAndMakeVisible (lessMotion);

    if (auto* host = StandaloneHost::get())
        if (auto* up = host->updater())
        {
            hasUpdater = true;
            page.addAndMakeVisible (updateButton);
            page.addAndMakeVisible (autoUpdate);
            page.addAndMakeVisible (earlyVersions);
            autoUpdate.setToggleState (up->automatic(), juce::dontSendNotification);
            earlyVersions.setToggleState (up->early(), juce::dontSendNotification);
            earlyVersions.onClick = [this]
            {
                if (auto* h = StandaloneHost::get())
                    if (auto* u = h->updater()) u->setEarly (earlyVersions.getToggleState());
            };
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
    {
        k->setGear (KnobStyle::modern, colours::text, colours::textDim, Face::instrumentSemi, true);
        k->capsLabel = true;
    }
    page.addAndMakeVisible (inGain);
    page.addAndMakeVisible (gate);
    page.addAndMakeVisible (inputFix);
    page.addAndMakeVisible (gateOn);
    inputFixAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (proc.state(), spec (P::inputFix).key, inputFix);
    gateOnAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (proc.state(), spec (P::gateOn).key, gateOn);
    hum = std::make_unique<ChipRow> (proc, P::humFilter, colours::accent);
    page.addAndMakeVisible (*hum);
    looks = std::make_unique<ChipRow> (juce::StringArray { lookName (Look::studio), lookName (Look::signal), lookName (Look::pedalboard) },
                                       colours::accent, [this] (int i) { if (onLookChange) onLookChange ((Look) i); });
    looks->setSelected ((int) look());
    page.addChildComponent (*looks); // one look now: the gear brings the colour
    page.addAndMakeVisible (inMeter);

    footList = std::make_unique<FootList> (proc);
    footView.setViewedComponent (footList.get(), false);
    footView.setScrollBarsShown (true, false);
    footView.setScrollBarThickness (8);
    page.addAndMakeVisible (footView);
    // Teaching happens on the rig itself, so this screen steps aside for it.
    teachControl.onClick = [this] { proc.footTeachTouch(); if (onClose) onClose(); };
    teachJob.onClick = [this]
    {
        juce::PopupMenu m;
        int id = 1;
        for (auto job : { FootJob::nextTone, FootJob::previousTone, FootJob::looper, FootJob::looperStop, FootJob::tapTempo, FootJob::tuner, FootJob::record, FootJob::song })
            m.addItem (id++, juce::String (footTargetName ({ job, 0 })));
        m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (teachJob).withMinimumWidth (teachJob.getWidth()),
                         [safe = juce::Component::SafePointer<SettingsView> (this)] (int picked)
                         {
                             if (safe == nullptr || picked <= 0) return;
                             static constexpr FootJob jobs[] = { FootJob::nextTone, FootJob::previousTone, FootJob::looper, FootJob::looperStop,
                                                                 FootJob::tapTempo, FootJob::tuner, FootJob::record, FootJob::song };
                             safe->proc.footTeachJob (jobs[picked - 1]);
                             if (safe->onClose) safe->onClose();
                         });
    };
    page.addAndMakeVisible (teachControl);
    page.addAndMakeVisible (teachJob);
    if (! standalone) current = Section::guitar;
    show (current);
}

SettingsView::~SettingsView() = default;

void SettingsView::visibilityChanged()
{
    if (isVisible())
    {
        summary = deviceSummary();
        lessMotion.setToggleState (motion::reduced(), juce::dontSendNotification);
        show (current);
        startTimerHz (30);
    }
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
    if (text != lastUpdateText) { lastUpdateText = text; page.repaint (updateStatus); }
}

void SettingsView::timerCallback()
{
    followScroll();
    refreshUpdate();
    inMeter.setLevel (proc.meters().gateInPeak.load());
    const auto s = deviceSummary();
    if (s != summary) { summary = s; layoutPage(); page.repaint(); }
    static int tick = 0;
    if (++tick % 10 == 0) page.repaint(); // the hum and Input Fix readings move slowly
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

juce::String SettingsView::reportText() const
{
    // Everything someone helping would ask for first, in one text the player can read and
    // hand over. Device names are in it; nothing about the person is.
    juce::String t;
    auto line = [&t] (const juce::String& k, const juce::String& v) { t << k << ": " << v << "\n"; };
    auto db = [] (float v) { return juce::String (v, 1) + " dB"; };
    line ("Riffhouse", versionLabel());
    line ("System", juce::SystemStats::getOperatingSystemName() + ", " + juce::String (juce::SystemStats::getNumCpus()) + " cores");
    line ("Written", juce::Time::getCurrentTime().toString (true, true, false, true));
    t << "\nAudio\n";
    if (auto* host = StandaloneHost::get())
    {
        auto& dm = host->deviceManager();
        juce::AudioDeviceManager::AudioDeviceSetup s;
        dm.getAudioDeviceSetup (s);
        line ("Driver", dm.getCurrentAudioDeviceType());
        line ("Input", s.inputDeviceName);
        line ("Output", s.outputDeviceName);
        if (auto* dev = dm.getCurrentAudioDevice())
        {
            line ("Buffer", juce::String (dev->getCurrentBufferSizeSamples()) + " samples at " + juce::String (dev->getCurrentSampleRate(), 0) + " Hz");
            line ("Reported delay", juce::String (dev->getInputLatencyInSamples()) + " in, " + juce::String (dev->getOutputLatencyInSamples()) + " out (samples)");
            line ("Dropouts", juce::String (dev->getXRunCount()));
        }
        else line ("Device", "none open");
        juce::StringArray drivers;
        for (auto* type : dm.getAvailableDeviceTypes())
            drivers.add (type->getTypeName() + " (" + juce::String (type->getDeviceNames (true).size()) + " inputs)");
        line ("Drivers on this computer", drivers.joinIntoString (", "));
        for (auto* type : dm.getAvailableDeviceTypes())
        {
            // which interface, and what Windows calls its inputs and outputs: the first thing a helper needs
            if (type->getTypeName() != "Windows Audio" && type->getTypeName() != "ASIO" && type->getTypeName() != dm.getCurrentAudioDeviceType()) continue;
            line ("  " + type->getTypeName() + " inputs", type->getDeviceNames (true).joinIntoString (" | "));
            line ("  " + type->getTypeName() + " outputs", type->getDeviceNames (false).joinIntoString (" | "));
        }
        t << "\nLast setup\n";
        auto& st = host->settings();
        for (auto* k : { "setupVersion", "setupInput", "setupPeakDb", "setupNoiseDb", "setupTiltDb", "setupToneSeconds", "setupPlayedSeconds", "setupFixDb", "setupGainDb", "setupGateDb", "setupFixHeldBack" })
            if (st.containsKey (k)) line (juce::String (k).substring (5), st.getValue (k));
    }
    else line ("Host", "running as a plugin");
    t << "\nInput\n";
    line ("Input level", db (proc.value (P::inGain)));
    line ("Input Fix", proc.value (P::inputFix) > 0.5f ? db (proc.value (P::inputFixAmt)) : juce::String ("off"));
    line ("Hum filter", juce::String (juce::roundToInt (proc.value (P::humFilter))) + " (0 off, 1 auto, 2 50 Hz, 3 60 Hz), mains found: " + juce::String (proc.meters().mains.load()));
    line ("Gate", juce::String (proc.value (P::gateOn) > 0.5f ? "on" : "off") + ", chosen " + db (proc.value (P::gateThresh)) + ", sitting at " + db (proc.meters().gateThreshNow.load()));
    line ("Treble now against a full-range guitar", db (proc.tiltProbe().tiltDb() - TiltProbe::referenceTiltDb) + " over " + juce::String (proc.tiltProbe().secondsMeasured(), 1) + " s of playing");
    t << "\nTone\n";
    line ("Tone", proc.currentPresetName());
    line ("Auto level", proc.value (P::autoLevel) > 0.5f ? db (proc.levels().trimDb()) + (proc.levels().hasPlayersPhrase() ? " (matched to your playing)" : " (house phrase)") : juce::String ("off"));
    for (const auto& s : paramSpecs())
        t << "  " << s.key << " = " << juce::String (proc.value (s.id), 2) << "\n";
    // what the app did with the sound card lately, and how long each thing took
    t << "\nWhat happened\n" << devlog::tail (120) << "\n";
    return t;
}

void SettingsView::saveReport()
{
    const auto t = reportText();
    auto folder = proc.recordingFolder == juce::File() ? Recorder::defaultFolder() : proc.recordingFolder;
    folder.createDirectory();
    const auto f = folder.getChildFile ("Riffhouse report " + juce::Time::getCurrentTime().formatted ("%Y-%m-%d %H%M") + ".txt");
    const bool ok = f.replaceWithText (t);
    if (onMessage) onMessage (ok ? "Report saved in your recordings folder" : "Couldn't write the report into " + folder.getFullPathName());
    if (ok) f.revealToUser();
}

void SettingsView::showAdvancedAudio()
{
    showAudioPicker (proc, this);
}

juce::Array<SettingsView::Section> SettingsView::sections() const
{
    juce::Array<Section> s;
    if (StandaloneHost::get() != nullptr) s.add (Section::sound);
    s.add (Section::guitar);
    s.add (Section::foot);
    s.add (Section::app);
    s.add (Section::help);
    return s;
}

juce::String SettingsView::sectionName (Section s)
{
    switch (s)
    {
        case Section::sound:  return "Sound card";
        case Section::guitar: return "Guitar input";
        case Section::foot:   return "Foot control";
        case Section::app:    return "Updates and motion";
        case Section::help:   return "Help and about";
    }
    return {};
}

juce::String SettingsView::sectionLead (Section s) const
{
    const bool standalone = StandaloneHost::get() != nullptr;
    switch (s)
    {
        case Section::sound:  return "Where the guitar comes in, where the sound goes, and how long it takes.";
        case Section::guitar: return "What happens to the guitar before it reaches the first pedal.";
        case Section::foot:   return standalone ? "Play hands free: a MIDI foot controller, expression pedal or keyboard works the moment you plug it in."
                                                : "Send MIDI from a foot controller, expression pedal or keyboard to this plugin.";
        case Section::app:    return "New versions, and how much the screen moves.";
        case Section::help:   return "When something sounds wrong, or does not work at all.";
    }
    return {};
}

void SettingsView::show (Section s)
{
    // Everything is on one page; the side list only jumps to a part of it.
    if (! sections().contains (s)) s = sections().getFirst();
    current = s;
    const bool standalone = StandaloneHost::get() != nullptr;
    runSetup.setVisible (standalone);
    advanced.setVisible (standalone);
    updateButton.setVisible (hasUpdater);
    autoUpdate.setVisible (hasUpdater);
    earlyVersions.setVisible (hasUpdater);
    for (auto* c : std::initializer_list<juce::Component*> { &inGain, &gate, &inMeter, &gateOn, &inputFix, hum.get(), &teachControl, &teachJob,
                                                             &footView, &lessMotion, &website, &problem, &report })
        c->setVisible (true);
    layoutPage();
    scroller.setViewPosition (0, s == sections().getFirst() ? 0 : sectionTop[(size_t) s] - 8);
    repaint();
    page.repaint();
}

void SettingsView::followScroll()
{
    // the side list lights the part of the page being read
    const int y = scroller.getViewPositionY() + 60;
    auto now = sections().getFirst();
    for (auto sec : sections())
        if (sectionTop[(size_t) sec] <= y) now = sec;
    // at the very bottom the last part counts, even when it is too short to reach the top
    if (scroller.getViewPositionY() + scroller.getViewHeight() >= page.getHeight() - 4) now = sections().getLast();
    if (now != current) { current = now; repaint (rail); }
}

int SettingsView::railItemAt (juce::Point<int> p) const
{
    for (size_t i = 0; i < railItems.size(); ++i)
        if (railItems[i].second.contains (p)) return (int) i;
    return -1;
}

void SettingsView::mouseMove (const juce::MouseEvent& e)
{
    const int h = railItemAt (e.getPosition());
    if (h != hover) { hover = h; repaint (rail); }
    setMouseCursor (h >= 0 ? juce::MouseCursor::PointingHandCursor : juce::MouseCursor::NormalCursor);
}

void SettingsView::mouseExit (const juce::MouseEvent&)
{
    if (hover >= 0) { hover = -1; repaint (rail); }
}

void SettingsView::mouseUp (const juce::MouseEvent& e)
{
    const int i = railItemAt (e.getPosition());
    if (i >= 0 && e.mouseWasClicked())
    {
        show (railItems[(size_t) i].first);
        current = railItems[(size_t) i].first; // lit even when the page cannot scroll that far
        repaint (rail);
    }
}

void SettingsView::resized()
{
    // One page, no side list: it scrolled anyway (Natik, 12 Nov). The column sits in the middle,
    // under the title.
    auto r = getLocalBounds();
    rail = {};
    railItems.clear();
    close.setBounds (getWidth() - 64, 18, 44, 44);
    const int colW = std::min (r.getWidth() - 2 * 72, 720 + 14);
    titleArea = juce::Rectangle<int> ((getWidth() - colW) / 2, 36, colW, 64);
    scroller.setBounds (titleArea.getX(), titleArea.getBottom() + 24, colW + 14, getHeight() - titleArea.getBottom() - 24);
    layoutPage();
}

void SettingsView::layoutPage()
{
    const int w = std::max (320, scroller.getWidth() - 14);
    const int col = std::min (w, 720);
    int y = 0;
    auto at = [&] (int height) { auto a = juce::Rectangle<int> (0, y, col, height); y += height; return a; };
    auto gap = [&] (int g) { y += g; };
    plateArea = soundText = meterCaption = fixText = humRow = humText = footText = updateStatus = earlyText = motionHead = helpText = aboutHead = aboutArea = {};

    // one page: every part under its own heading, one after another
    bool firstPart = true;
    for (auto part : sections())
    {
    if (! firstPart) gap (56); // room between parts, and a hairline in it (paintPage)
    firstPart = false;
    sectionTop[(size_t) part] = y;
    gap (76); // the part's name and its one line
    switch (part)
    {
        case Section::sound:
        {
            plateArea = at (164).withWidth (std::min (col, 660));
            gap (24);
            auto row = at (42);
            runSetup.setBounds (row.removeFromLeft (250));
            row.removeFromLeft (12);
            advanced.setBounds (row.removeFromLeft (250));
            gap (16);
            soundText = at (44);
            break;
        }
        case Section::guitar:
        {
            auto knobs = at (130);
            inGain.setBounds (knobs.removeFromLeft (120));
            knobs.removeFromLeft (10);
            gate.setBounds (knobs.removeFromLeft (120));
            knobs.removeFromLeft (32);
            knobs.removeFromTop (16);
            meterCaption = knobs.removeFromTop (22);
            inMeter.setBounds (knobs.removeFromTop (8).withTrimmedRight (10));
            knobs.removeFromTop (18);
            gateOn.setBounds (knobs.removeFromTop (30).withWidth (220));
            gap (30);
            inputFix.setBounds (at (30));
            gap (4);
            fixText = at (40).withTrimmedLeft (52);
            gap (28);
            humRow = at (26);
            gap (8);
            hum->setBounds (at (36).withWidth (std::min (col, 430)));
            gap (12);
            humText = at (40);
            break;
        }
        case Section::foot:
        {
            auto row = at (40);
            teachControl.setBounds (row.removeFromLeft (240));
            row.removeFromLeft (12);
            teachJob.setBounds (row.removeFromLeft (240));
            gap (12);
            footText = at (22);
            gap (10);
            footView.setBounds (at (220));
            footList->setSize (footView.getWidth() - 12, FootList::rowH);
            footList->refresh();
            break;
        }
        case Section::app:
        {
            if (hasUpdater)
            {
                updateStatus = at (26);
                gap (14);
                autoUpdate.setBounds (at (30));
                gap (8);
                earlyVersions.setBounds (at (30));
                earlyText = at (22).withTrimmedLeft (44);
                gap (16);
                updateButton.setBounds (at (40).withWidth (250));
                gap (40);
            }
            motionHead = at (26);
            gap (10);
            lessMotion.setBounds (at (30));
            break;
        }
        case Section::help:
        {
            auto row = at (42);
            for (auto* b : { &website, &problem, &report })
            {
                b->setBounds (row.removeFromLeft (180));
                row.removeFromLeft (12);
            }
            gap (16);
            helpText = at (66);
            gap (36);
            aboutHead = at (26);
            gap (10);
            aboutArea = at (150);
            break;
        }
    }
    }
    page.setSize (w, y + 40);
}

void SettingsView::paint (juce::Graphics& g)
{
    g.fillAll (colours::bg);
    auto head = titleArea;
    g.setColour (colours::text);
    g.setFont (font (28.0f, Weight::display));
    g.drawText ("Settings", head.removeFromTop (36), juce::Justification::centredLeft);
    g.setColour (colours::textFaint);
    g.setFont (font (12.5f));
    g.drawText ("Riffhouse " + versionForPeople(), head.removeFromTop (22), juce::Justification::centredLeft);

    for (size_t i = 0; i < railItems.size(); ++i)
    {
        const auto [s, area] = railItems[i];
        const bool on = s == current, over = (int) i == hover;
        auto a = area.toFloat();
        if (on || over)
        {
            g.setColour (juce::Colours::white.withAlpha (on ? 0.06f : 0.03f));
            g.fillRoundedRectangle (a, 6.0f);
        }
        // the open section has its pilot light lit, like the AMP ON lamp on the rig
        const auto lamp = juce::Rectangle<float> (7.0f, 7.0f).withCentre ({ a.getX() + 16.0f, a.getCentreY() });
        if (on)
        {
            g.setColour (colours::lamp.withAlpha (0.25f));
            g.fillEllipse (lamp.expanded (3.0f));
            g.setColour (colours::lamp);
            g.fillEllipse (lamp);
        }
        else
        {
            g.setColour (juce::Colour (0xff2a2826));
            g.fillEllipse (lamp);
        }
        g.setColour (on ? colours::text : over ? colours::text.withAlpha (0.85f) : colours::textDim);
        g.setFont (font (15.0f, on ? Weight::semibold : Weight::medium));
        g.drawText (sectionName (s), area.withTrimmedLeft (34), juce::Justification::centredLeft);
    }
}

void SettingsView::paintPlate (juce::Graphics& g, juce::Rectangle<float> a)
{
    // The sound card's state on a display window, like the tone's name in the top bar: the delay
    // in lit figures on the left, what is plugged in where on the right.
    g.setColour (juce::Colour (0xff151413));
    g.fillRoundedRectangle (a, 6.0f);
    g.setGradientFill (juce::ColourGradient (juce::Colours::black.withAlpha (0.55f), 0.0f, a.getY(),
                                             juce::Colours::transparentBlack, 0.0f, a.getY() + 10.0f, false));
    g.fillRoundedRectangle (a, 6.0f);
    g.setColour (juce::Colours::white.withAlpha (0.07f));
    g.fillRect (a.getX() + 4.0f, a.getBottom() - 1.0f, a.getWidth() - 8.0f, 1.0f);

    // what is running, or the example used for pictures of the app
    struct Card { bool open = false; juce::String driver, input, output; int buffer = 0, dropouts = 0; double rate = 48000.0, ms = 0.0; } c;
    auto* host = StandaloneHost::get();
    if (exampleCard)
        c = { true, "Windows Audio (Exclusive Mode)", "Guitar (Realtek USB Audio)", "Headphones (Realtek USB Audio)", 144, 0, 48000.0, 6.3 };
    else if (auto* dev = host != nullptr ? host->deviceManager().getCurrentAudioDevice() : nullptr)
    {
        juce::AudioDeviceManager::AudioDeviceSetup setup;
        host->deviceManager().getAudioDeviceSetup (setup);
        c.open = true;
        c.driver = dev->getTypeName();
        c.input = setup.inputDeviceName.isNotEmpty() ? setup.inputDeviceName : juce::String ("none");
        c.output = setup.outputDeviceName.isNotEmpty() ? setup.outputDeviceName : juce::String ("none");
        c.rate = std::max (1.0, dev->getCurrentSampleRate());
        c.buffer = dev->getCurrentBufferSizeSamples();
        int lat = dev->getInputLatencyInSamples() + dev->getOutputLatencyInSamples();
        if (lat <= 0) lat = 2 * c.buffer;
        c.ms = 1000.0 * (lat + proc.getLatencySamples()) / c.rate;
        c.dropouts = dev->getXRunCount();
    }
    auto in = a.reduced (26.0f, 20.0f);
    auto figure = in.removeFromLeft (190.0f);
    g.setColour (juce::Colours::black.withAlpha (0.6f));
    g.fillRect (figure.getRight() + 4.0f, in.getY(), 1.0f, in.getHeight());
    in.removeFromLeft (30.0f);

    if (! c.open)
    {
        g.setColour (colours::lamp.withAlpha (0.35f));
        g.setFont (faceFont (Face::doto, 52.0f));
        g.drawText ("0.0", figure.withTrimmedBottom (26.0f), juce::Justification::centredLeft);
        drawCaps (g, "No sound card", figure.removeFromBottom (20.0f), 11.0f, colours::textDim, juce::Justification::centredLeft, 0.14f);
        g.setColour (colours::text);
        g.setFont (font (16.0f, Weight::medium));
        g.drawFittedText ("Nothing is open yet. Run the automatic setup and Riffhouse finds your guitar for you.",
                          in.toNearestInt(), juce::Justification::centredLeft, 3, 1.0f);
        return;
    }
    const double ms = c.ms, sr = c.rate;
    const int buf = c.buffer, dropouts = c.dropouts;

    // fast enough not to notice is amber; slow enough to feel is a softer amber, never red
    g.setColour (ms <= 12.0 ? colours::lamp : colours::lamp.withAlpha (0.7f));
    g.setFont (faceFont (Face::doto, 52.0f));
    auto num = figure.withTrimmedBottom (26.0f);
    const auto msText = delayFigure (ms);
    g.drawText (msText, num, juce::Justification::centredLeft);
    const float numW = juce::GlyphArrangement::getStringWidth (faceFont (Face::doto, 52.0f), msText);
    g.setFont (font (16.0f, Weight::medium));
    g.setColour (colours::textDim);
    g.drawText ("ms", num.withTrimmedLeft (numW + 8.0f).withTrimmedTop (18.0f), juce::Justification::centredLeft);
    drawCaps (g, "From string to ear", figure.removeFromBottom (20.0f), 11.0f, colours::textDim, juce::Justification::centredLeft, 0.14f);

    const float rowH = in.getHeight() / 4.0f;
    auto line = [&] (const juce::String& k, const juce::String& v, juce::Colour ink)
    {
        auto row = in.removeFromTop (rowH);
        drawCaps (g, k, row.removeFromLeft (52.0f), 10.5f, colours::textFaint, juce::Justification::centredLeft, 0.14f);
        g.setColour (ink);
        g.setFont (font (15.0f));
        g.drawText (v, row, juce::Justification::centredLeft, true);
    };
    line ("Driver", c.driver, colours::text);
    line ("In", c.input, colours::text);
    line ("Out", c.output, colours::text);
    line ("Runs", juce::String (buf) + " samples at " + juce::String (sr / 1000.0, 1) + " kHz, a "
                      + juce::String (1000.0 * buf / sr, 1) + " ms buffer"
                      + (dropouts > 0 ? ", " + juce::String (dropouts) + (dropouts == 1 ? " dropout" : " dropouts") : juce::String()),
          dropouts > 0 ? colours::hot : colours::textDim);
}

void SettingsView::paintPage (juce::Graphics& g)
{
    const int col = std::min (page.getWidth(), 720);
    auto heading = [&] (Section part)
    {
        const int top = sectionTop[(size_t) part];
        if (part != sections().getFirst())
        {
            g.setColour (juce::Colours::white.withAlpha (0.07f));
            g.fillRect (0, top - 30, col, 1);
        }
        g.setColour (colours::text);
        g.setFont (font (22.0f, Weight::display));
        g.drawText (sectionName (part), 0, top, col, 32, juce::Justification::centredLeft);
        g.setColour (colours::textDim);
        g.setFont (font (14.5f));
        g.drawFittedText (sectionLead (part), juce::Rectangle<int> (0, top + 34, col, 36), juce::Justification::topLeft, 2, 1.0f);
    };

    auto small = [&g] (const juce::String& t, juce::Rectangle<int> a, int lines = 3)
    {
        g.setColour (colours::textDim);
        g.setFont (font (13.5f));
        g.drawFittedText (t, a, juce::Justification::topLeft, lines, 1.0f);
    };
    auto subhead = [&g] (const juce::String& t, juce::Rectangle<int> a)
    {
        g.setColour (colours::text);
        g.setFont (font (16.0f, Weight::semibold));
        g.drawText (t, a, juce::Justification::centredLeft);
    };

    for (auto part : sections())
    {
    heading (part);
    switch (part)
    {
        case Section::sound:
            paintPlate (g, plateArea.toFloat());
            small ("Setup listens to your guitar and picks the fastest way that plays clean. To choose the input, the "
                   "headphones or the delay yourself, use Driver, input and output.", soundText, 2);
            break;
        case Section::guitar:
        {
            g.setColour (colours::textDim);
            g.setFont (font (13.0f));
            g.drawText ("Level going into the rig", meterCaption, juce::Justification::centredLeft);
            small ("Setup measures how much treble your input loses and sets the strength (now "
                       + juce::String (juce::roundToInt (proc.value (P::inputFixAmt))) + " dB). Interfaces with a guitar (Hi-Z) input don't need it.",
                   fixText, 2);
            subhead ("Mains hum filter", humRow);
            const int mains = proc.meters().mains.load();
            g.setColour (mains > 0 ? colours::good : colours::textFaint);
            g.setFont (font (13.0f));
            g.drawText (mains > 0 ? "Found " + juce::String (mains) + " Hz hum, removing it" : "No hum heard", humRow, juce::Justification::centredRight);
            small ("Auto listens for the 50 Hz (India, Europe, most of Asia and Africa) or 60 Hz (Americas) buzz from "
                   "power lines and removes just that, leaving your low notes alone.", humText, 2);
            break;
        }
        case Section::foot:
            small ("Program changes pick your starred tones in order. What you have taught is listed here.", footText, 1);
            break;
        case Section::app:
            if (hasUpdater)
            {
                g.setColour (colours::text);
                g.setFont (font (16.0f, Weight::medium));
                g.drawText (lastUpdateText, updateStatus, juce::Justification::centredLeft);
                small ("Test versions come out a few days before everyone gets them. They can have rough edges.", earlyText, 1);
            }
            subhead ("Motion", motionHead);
            break;
        case Section::help:
        {
            small ("Help and fixes opens the page of common problems. Report a problem puts a report on your clipboard and opens "
                   "the page to paste it into. Save a report writes it to a text file in your recordings folder, to send to "
                   "whoever is helping you. Nothing is sent by itself.", helpText, 3);
            subhead ("About", aboutHead);
            juce::AttributedString about;
            about.setLineSpacing (4.0f);
            about.setWordWrap (juce::AttributedString::byWord);
            about.append ("Riffhouse " + versionLabel() + ", made by Natik Chhabra. Free and open source under the AGPL-3.0 license. "
                          "Tone names saying \"Inspired by\" describe a style: Riffhouse has no affiliation with, and is not endorsed by, "
                          "any artist, band or equipment maker, and all trademarks belong to their owners. If you are an artist named in a "
                          "tone and want it changed or removed, open an issue and it will be done. ASIO and VST are trademarks of "
                          "Steinberg Media Technologies GmbH. Riffhouse comes with absolutely no warranty. "
                          "Built with JUCE, Neural Amp Modeler Core, Signalsmith Stretch and LAME (MP3 copies, LGPL). Fonts under the "
                          "SIL Open Font License and the Apache License 2.0. Every licence is in Licences.txt beside the app.",
                          font (13.5f), colours::textDim);
            about.draw (g, aboutArea.toFloat());
            break;
        }
    }
    }
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
