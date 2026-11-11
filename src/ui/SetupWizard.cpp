// Riffhouse - free, open-source guitar rig
// Copyright (C) 2026 Riffhouse contributors. Licensed under AGPL-3.0-or-later.
#include "SetupWizard.h"
#include "../plugin/StandaloneHost.h"

namespace rh::ui
{
SetupWizard::SetupWizard (RiffhouseProcessor& p) : proc (p)
{
    for (auto* b : { &primary, &secondary, &tertiary })
        addChildComponent (*b);
    primary.setColour (juce::TextButton::buttonColourId, colours::accent);
    primary.setColour (juce::TextButton::textColourOffId, colours::bg);
    addChildComponent (meter);
    setWantsKeyboardFocus (true);
}

SetupWizard::~SetupWizard()
{
    if (setup != nullptr) setup->cancel();
    if (auto* h = StandaloneHost::get()) h->setupRunning (false);
}

juce::StringArray SetupWizard::scanInputs() const
{
    juce::StringArray names;
    if (auto* h = StandaloneHost::get())
        for (auto* t : h->deviceManager().getAvailableDeviceTypes())
        {
            t->scanForDevices();
            for (auto& n : t->getDeviceNames (true)) names.addIfNotAlreadyThere (n);
        }
    return names;
}

void SetupWizard::begin (bool again)
{
    if (auto* h = StandaloneHost::get()) h->setupRunning (true);
    offeredAgain = again;
    step = Step::welcome;
    layoutButtons();
    startTimerHz (30);
    repaint();
}

void SetupWizard::startSetup()
{
    auto* host = StandaloneHost::get();
    if (host == nullptr) return;
    setup = std::make_unique<AutoSetup> (host->deviceManager(), proc);
    for (auto& f : found) f = {};
    shownProgress = 0.0f;
    setup->onPhaseChanged = [this]
    {
        if (setup == nullptr) return;
        noteFound();
        shownProgress = setup->pausing() ? 1.0f : 0.0f;
        if (setup->phase() == AutoSetup::Phase::finished)
        {
            step = Step::done;
            if (auto* h = StandaloneHost::get())
            {
                h->saveAudioState();
                h->settings().setValue ("setupDone", true);
                h->settings().setValue ("setupVersion", AutoSetup::kSetupVersion);
                // what was measured, kept for the report a player can save when asking for help
                const auto& s = setup->summary();
                auto& st = h->settings();
                st.setValue ("setupInput", s.deviceType + " / " + s.inputName);
                st.setValue ("setupPeakDb", (double) s.guitarPeakDb);
                st.setValue ("setupNoiseDb", (double) s.noiseDb);
                st.setValue ("setupTiltDb", (double) s.tiltDb);
                st.setValue ("setupToneSeconds", s.toneSeconds);
                st.setValue ("setupPlayedSeconds", s.playedSeconds);
                st.setValue ("setupFixDb", s.inputFix ? (double) s.inputFixDb : 0.0);
                st.setValue ("setupGainDb", (double) s.inputGainDb);
                st.setValue ("setupGateDb", (double) s.gateDb);
                st.setValue ("setupFixHeldBack", s.fixHeldBack);
                h->settings().saveIfNeeded();
            }
        }
        else if (setup->phase() == AutoSetup::Phase::noSignal)
        {
            step = Step::failed;
            inputsAtFailure = scanInputs();
            ticksSinceScan = 0;
        }
        layoutButtons();
        repaint();
    };
    step = Step::running;
    layoutButtons();
    setup->start();
}

SetupWizard::View SetupWizard::view() const
{
    if (example || setup == nullptr) return exampleView;
    View v;
    v.phase = setup->phase();
    v.pausing = setup->pausing();
    v.measuringNoise = setup->measuringNoise();
    v.progress = setup->progress();
    v.detail = setup->detail();
    return v;
}

void SetupWizard::noteFound()
{
    using Phase = AutoSetup::Phase;
    if (setup == nullptr || ! setup->pausing()) return;
    const auto& s = setup->summary();
    switch (setup->phase())
    {
        case Phase::listening: found[0] = setup->found().fromFirstOccurrenceOf (" on ", false, false); break;
        case Phase::tuning:    found[1] = juce::String (s.latencyMs, 1) + " ms"; break;
        case Phase::playing:   found[2] = juce::String (s.inputGainDb >= 0 ? "+" : "") + juce::String (s.inputGainDb, 1) + " dB input level"
                                          + (s.inputFix ? ", Input Fix " + juce::String (juce::roundToInt (s.inputFixDb)) + " dB" : juce::String()); break;
        case Phase::idle: case Phase::quiet: case Phase::finished: case Phase::noSignal: break;
    }
}

void SetupWizard::showExample (int which)
{
    using Phase = AutoSetup::Phase;
    example = true;
    stopTimer();
    exampleView = {};
    for (auto& f : found) f = {};
    const juce::String input = "Microphone (High Definition Audio)";
    if (which >= 5)
    {
        exampleSummary.inputName = input;
        exampleSummary.latencyMs = 9.3; exampleSummary.bufferSize = 144; exampleSummary.exclusive = true;
        exampleSummary.inputGainDb = 1.5f; exampleSummary.inputFix = true; exampleSummary.inputFixDb = 21.0f; exampleSummary.gateDb = -52.0f;
        step = Step::done;
    }
    else
    {
        const Phase phases[] = { Phase::listening, Phase::tuning, Phase::playing, Phase::quiet, Phase::listening };
        exampleView.phase = phases[which];
        exampleView.pausing = which == 4;
        exampleView.progress = which == 4 ? 1.0f : 0.55f;
        exampleView.detail = which == 0 ? "Listening on " + input : which == 1 ? juce::String ("Trying 144 samples in exclusive mode") : juce::String();
        const int doneSteps = which == 4 ? 1 : which;
        if (doneSteps > 0) found[0] = input;
        if (doneSteps > 1) found[1] = "9.3 ms";
        if (doneSteps > 2) found[2] = "+1.5 dB input level, Input Fix 21 dB";
        step = Step::running;
        meter.setLevel (which == 3 ? 0.004f : 0.3f);
    }
    shownProgress = exampleView.progress;
    layoutButtons();
    repaint();
}

void SetupWizard::complete (bool keep)
{
    if (! keep && setup != nullptr) setup->cancel();
    setup.reset();
    stopTimer();
    if (auto* h = StandaloneHost::get())
    {
        h->settings().setValue ("setupDone", true);
        h->settings().setValue ("setupVersion", AutoSetup::kSetupVersion); // asked once, whatever the answer
        h->settings().saveIfNeeded();
        h->setupRunning (false);
    }
    if (onFinished) onFinished();
}

void SetupWizard::layoutButtons()
{
    primary.onClick = nullptr;
    secondary.onClick = nullptr;
    tertiary.onClick = nullptr;
    primary.setVisible (true);
    secondary.setVisible (false);
    tertiary.setVisible (false);
    meter.setVisible (step == Step::running);
    switch (step)
    {
        case Step::welcome:
            primary.setButtonText (offeredAgain ? "Run setup again" : "Start setup");
            primary.onClick = [this] { startSetup(); };
            secondary.setButtonText (offeredAgain ? "Not now" : "Skip for now");
            secondary.setVisible (true);
            secondary.onClick = [this] { complete (true); };
            break;
        case Step::running:
            primary.setButtonText ("Cancel");
            primary.setColour (juce::TextButton::buttonColourId, colours::raised);
            primary.setColour (juce::TextButton::textColourOffId, colours::text);
            primary.onClick = [this] { complete (false); };
            break;
        case Step::done:
            primary.setColour (juce::TextButton::buttonColourId, colours::accent);
            primary.setColour (juce::TextButton::textColourOffId, colours::bg);
            primary.setButtonText ("Start playing");
            primary.onClick = [this] { complete (true); };
            secondary.setButtonText ("Run it again");
            secondary.setVisible (true);
            secondary.onClick = [this] { startSetup(); };
            break;
        case Step::failed:
            primary.setColour (juce::TextButton::buttonColourId, colours::accent);
            primary.setColour (juce::TextButton::textColourOffId, colours::bg);
            primary.setButtonText ("Try again");
            primary.onClick = [this] { startSetup(); };
            secondary.setButtonText ("Skip");
            secondary.setVisible (true);
            secondary.onClick = [this] { complete (true); };
           #if JUCE_WINDOWS
            tertiary.setButtonText ("Open Windows microphone privacy");
            tertiary.setVisible (true);
            tertiary.onClick = [] { juce::URL ("ms-settings:privacy-microphone").launchInDefaultBrowser(); };
           #endif
            break;
    }
    resized();
}

juce::Rectangle<int> SetupWizard::cardBounds() const
{
    const int h = step == Step::welcome ? 372 : step == Step::running ? 504 : step == Step::failed ? 470 : 560;
    return getLocalBounds().withSizeKeepingCentre (640, h);
}

void SetupWizard::resized()
{
    auto card = cardBounds();
    auto buttons = card.reduced (48, 36).removeFromBottom (44);
    primary.setBounds (buttons.removeFromRight (180));
    buttons.removeFromRight (12);
    if (secondary.isVisible()) secondary.setBounds (buttons.removeFromRight (150));
    if (tertiary.isVisible()) tertiary.setBounds (buttons.removeFromLeft (270));
    meter.setBounds (card.reduced (48, 0).withY (card.getY() + 168).withHeight (12));
}

void SetupWizard::timerCallback()
{
    pulse += 0.06f;
    if (setup != nullptr) meter.setLevel (setup->liveLevel());
    // the bar glides to where the step has got to; it never jumps
    const float target = view().progress;
    shownProgress += (target - shownProgress) * (target > shownProgress ? 0.22f : 1.0f);
    // Nothing was heard, and then something new is plugged in: start over without being asked.
    if (step == Step::failed && ++ticksSinceScan >= 60)
    {
        ticksSinceScan = 0;
        const auto now = scanInputs();
        bool fresh = false;
        for (auto& n : now)
            if (! inputsAtFailure.contains (n)) fresh = true;
        inputsAtFailure = now;
        if (fresh) { startSetup(); return; }
    }
    repaint();
}

void SetupWizard::paint (juce::Graphics& g)
{
    g.fillAll (colours::bg.withAlpha (0.96f));
    const auto card = cardBounds();
    drawCard (g, card.toFloat(), colours::panel, 18.0f);
    auto r = card.reduced (48, 40);

    auto title = [&] (const juce::String& t)
    {
        g.setColour (colours::text);
        g.setFont (font (30.0f, Weight::display));
        g.drawText (t, r.removeFromTop (44), juce::Justification::centredLeft);
        r.removeFromTop (8);
    };
    auto body = [&] (const juce::String& t, int h, juce::Colour c = colours::textDim)
    {
        g.setColour (c);
        g.setFont (font (16.0f));
        g.drawFittedText (t, r.removeFromTop (h), juce::Justification::topLeft, 6, 1.0f);
    };
    auto bullet = [&] (bool ok, const juce::String& t)
    {
        auto row = r.removeFromTop (32);
        const auto icon = row.removeFromLeft (30).toFloat().withSizeKeepingCentre (18, 18);
        if (ok) strokeIcon (g, icons::check(), icon, colours::good, 2.2f);
        else { g.setColour (colours::warn); g.fillEllipse (icon.withSizeKeepingCentre (8, 8)); }
        g.setColour (colours::text);
        g.setFont (font (15.0f));
        g.drawFittedText (t, row, juce::Justification::centredLeft, 1);
    };

    switch (step)
    {
        case Step::welcome:
        {
            if (offeredAgain)
            {
                title ("Setup got better. Run it again?");
                body ("It now sets your input level from your hardest playing and wins back the treble a computer's mic jack "
                      "swallows. That is what made heavy tones loud and harsh. It takes under a minute.", 48);
            }
            else
            {
                title ("Let's get your guitar sounding right");
                body ("Riffhouse finds your guitar, picks the fastest audio settings this computer can run without crackles, "
                      "and sets your input level and noise gate from how you really play. It takes under a minute.", 48);
            }
            r.removeFromTop (16);
            auto box = r.removeFromTop (104).toFloat();
            // one thing to do before starting, set off by a rule above and below instead of a box in a box
            g.setColour (juce::Colours::white.withAlpha (0.10f));
            g.fillRect (box.getX(), box.getY(), box.getWidth(), 1.0f);
            g.fillRect (box.getX(), box.getBottom() - 1.0f, box.getWidth(), 1.0f);
            strokeIcon (g, icons::headphones(), box.removeFromLeft (58).withTrimmedRight (20.0f).withSizeKeepingCentre (34, 34), colours::accent, 2.0f);
            g.setColour (colours::text);
            g.setFont (font (16.0f, Weight::semibold));
            g.drawText ("Put on headphones first", box.removeFromTop (44).withTrimmedTop (14), juce::Justification::centredLeft);
            g.setColour (colours::textDim);
            g.setFont (font (14.5f));
            g.drawFittedText ("If your guitar is plugged into the computer's mic jack, speakers can howl. "
                              "Plug the guitar in, turn its volume up, and press Start.",
                              box.reduced (0, 4).toNearestInt().withTrimmedRight (16), juce::Justification::topLeft, 3, 1.0f);
            break;
        }
        case Step::running:
        {
            using Phase = AutoSetup::Phase;
            const auto v = view();
            const int active = v.phase == Phase::listening ? 0 : v.phase == Phase::tuning ? 1 : v.phase == Phase::playing ? 2 : 3;
            if (v.pausing)
            {
                // a step is done: say so, and say what comes next before it starts
                const char* done[] = { "Found your guitar", "Delay sorted", "Level set", "" };
                const char* next[] = { "Next, keep strumming while Riffhouse finds the lowest delay this computer can run.",
                                       "Next, play as hard as you ever will. The input level is set from that.",
                                       "Next, mute the strings so the background noise can be measured.", "" };
                title (done[active]);
                body (next[active], 66);
            }
            else if (v.phase == Phase::quiet && v.measuringNoise)
            {
                title ("Hold still");
                body ("Measuring the background noise now, with your level and Input Fix on. The gate is set just above it.", 66);
            }
            else if (v.phase == Phase::quiet)
            {
                title ("Now mute the strings");
                body ("Rest your hand across the strings so nothing rings. Measuring starts by itself as soon as it is quiet.", 66);
            }
            else if (v.phase == Phase::playing)
            {
                title ("Now play as hard as you ever will");
                body ("Big chords, hit like the loudest part of a song, for about four seconds. "
                      "Your input level is set from this, so nothing gets harsh or jumps out when you dig in later.", 66);
            }
            else if (v.phase == Phase::tuning)
            {
                title ("Keep strumming");
                body ("Trying the fastest audio settings one by one and keeping the lowest delay that runs without crackles. "
                      "This takes a few seconds.", 66);
            }
            else
            {
                title ("Strum your guitar");
                body ("Play anything, in any tuning, and keep going. Riffhouse is listening on each input to find the one your guitar is on.", 66);
            }

            r.removeFromTop (28); // the live level meter sits here (child component)
            r.removeFromTop (10);
            auto bar = r.removeFromTop (3).toFloat();
            g.setColour (colours::raisedHi);
            g.fillRect (bar);
            g.setColour (v.pausing ? colours::good : colours::accent);
            g.fillRect (bar.withWidth (bar.getWidth() * juce::jlimit (0.0f, 1.0f, shownProgress)));
            r.removeFromTop (8);
            g.setColour (colours::textDim);
            g.setFont (font (13.5f));
            g.drawText (v.detail, r.removeFromTop (22), juce::Justification::centredLeft);

            // the four steps, each with what it found once it is done
            r.removeFromTop (12);
            g.setColour (juce::Colours::white.withAlpha (0.10f));
            g.fillRect (r.removeFromTop (1));
            r.removeFromTop (8);
            const char* names[] = { "Find your guitar", "Lowest safe delay", "Your hardest playing", "Background noise" };
            for (int i = 0; i < 4; ++i)
            {
                auto row = r.removeFromTop (32);
                const bool isDone = i < active || (i == active && v.pausing), isNow = i == active && ! v.pausing;
                const auto mark = row.removeFromLeft (30).toFloat().withSizeKeepingCentre (16, 16);
                if (isDone) strokeIcon (g, icons::check(), mark, colours::good, 2.2f);
                else if (isNow)
                {
                    g.setColour (colours::accent.withAlpha (0.55f + 0.45f * std::sin (pulse * 2.2f)));
                    g.fillEllipse (mark.withSizeKeepingCentre (8, 8));
                }
                else
                {
                    g.setColour (colours::textFaint);
                    g.setFont (font (13.0f));
                    g.drawText (juce::String (i + 1), mark.expanded (4.0f, 2.0f), juce::Justification::centred);
                }
                g.setColour (isNow ? colours::text : (isDone ? colours::textDim : colours::textFaint));
                g.setFont (font (15.0f, isNow ? Weight::semibold : Weight::regular));
                g.drawText (names[i], row.removeFromLeft (190), juce::Justification::centredLeft);
                if (isDone)
                {
                    g.setColour (colours::textDim);
                    g.setFont (font (13.5f));
                    g.drawFittedText (found[i], row, juce::Justification::centredRight, 1, 0.9f);
                }
            }
            break;
        }
        case Step::done:
        {
            const auto& s = example ? exampleSummary : setup->summary();
            title ("You're all set");
            body ("Here's what Riffhouse set up. You can change any of it later in Settings.", 30);
            r.removeFromTop (10);
            bullet (true, "Guitar on " + s.inputName);
            bullet (s.latencyMs < 15.0, "About " + juce::String (s.latencyMs, 1) + " ms from string to ear ("
                                          + juce::String (s.bufferSize) + " samples" + (s.exclusive ? ", exclusive mode)" : ")"));
            bullet (true, "Input level " + juce::String (s.inputGainDb >= 0 ? "+" : "") + juce::String (s.inputGainDb, 1)
                              + " dB, set from your hardest playing");
            bullet (true, s.inputFix ? "Input Fix on: brings back " + juce::String (juce::roundToInt (s.inputFixDb)) + " dB of treble your input was losing"
                                     : "Input Fix off: your input already has its full treble");
            bullet (true, "Gate at " + juce::String (juce::roundToInt (s.gateDb)) + " dB, and it follows the room if the noise changes");
            bullet (true, "Auto level on: every tone comes out as loud as the last, matched to your playing");
            bullet (true, "Hum filter on automatic (removes 50 or 60 Hz buzz)");
            if (s.clipping)
                bullet (false, "Your input is clipping. Turn the guitar volume down a little.");
            else if (s.fixHeldBack)
                bullet (false, "This input hisses, so a little of the Input Fix was held back.");
            else if (s.latencyMs >= 15.0)
                bullet (false, "Delay is a bit high. Closing other audio apps can help.");
            else if (s.exclusive)
                bullet (false, "Other apps' sound pauses while Riffhouse is open. Use Play along for songs.");
            break;
        }
        case Step::failed:
        {
            title ("We can't hear your guitar");
            body ("Riffhouse listened on every input but didn't hear anything. Plug in now and it starts again by itself, or check these:", 50);
            r.removeFromTop (6);
            bullet (false, "The cable is pushed all the way into the mic (pink) or line-in jack");
            bullet (false, "The guitar's volume knob is turned up");
           #if JUCE_WINDOWS
            bullet (false, "Windows lets desktop apps use the microphone (privacy settings)");
            bullet (false, "Microphone isn't muted in Windows sound settings");
           #else
            bullet (false, "This app is allowed to use the microphone");
           #endif
            break;
        }
    }
}
} // namespace rh::ui
