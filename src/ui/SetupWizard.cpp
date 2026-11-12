// Riffhouse - free, open-source guitar rig
// Copyright (C) 2026 Riffhouse contributors. Licensed under AGPL-3.0-or-later.
#include "SetupWizard.h"

#include "AudioPicker.h"
#include "../plugin/StandaloneHost.h"
#include "../engine/DeviceNames.h"

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
            // Windows Audio hears about a new input by itself; asking it again and again only
            // keeps the window waiting. The others are quick to ask.
            if (! t->getTypeName().startsWith ("Windows Audio")) t->scanForDevices();
            for (auto& n : t->getDeviceNames (true)) names.addIfNotAlreadyThere (n);
        }
    return names;
}

void SetupWizard::begin (bool again, const std::vector<AutoSetup::Input>* pretend)
{
    auto* host = StandaloneHost::get();
    if (host != nullptr) host->setupRunning (true);
    if (pretend != nullptr) inputs = *pretend;
    else if (host != nullptr) inputs = AutoSetup::inputs (host->deviceManager());
    picked = hoverRow = -1;
    firstRow = 0;
    offeredAgain = again;
    step = Step::welcome;
    cardH.snap ((float) heightFor (step));
    layoutButtons();
    if (pretend != nullptr) return;
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
            shownSummary = setup->summary();
            go (Step::done);
            if (auto* h = StandaloneHost::get())
            {
                h->saveAudioState();
                h->settings().removeValue ("audioDriver"); // setup chose: the driver is Automatic again
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
            go (Step::failed);
            inputsAtFailure = scanInputs();
            ticksSinceScan = 0;
        }
        layoutButtons();
        repaint();
    };
    for (auto& r : rowDone) r.snap (0.0f);
    go (Step::running);
    layoutButtons();
    setup->start (juce::isPositiveAndBelow (picked, (int) inputs.size()) ? &inputs[(size_t) picked] : nullptr);
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

void SetupWizard::showExample (int which, bool moving)
{
    using Phase = AutoSetup::Phase;
    example = true;
    stopTimer();
    exampleView = {};
    for (auto& f : found) f = {};
    const juce::String input = "Microphone (High Definition Audio)";
    if (which >= 5)
    {
        shownSummary.inputName = input;
        shownSummary.outputName = "Headphones (High Definition Audio)";
        shownSummary.deviceType = "Windows Audio (Exclusive Mode)";
        shownSummary.latencyMs = 9.3; shownSummary.bufferSize = 144; shownSummary.exclusive = true;
        if (which >= 6)
        {
            // a guitar on one USB device and headphones on another: shared mode, and why
            shownSummary.inputName = "Guitar (Realtek USB Audio)";
            shownSummary.outputName = "Headphones (CX31993 HiFi)";
            shownSummary.deviceType = "Windows Audio";
            shownSummary.latencyMs = 24.1; shownSummary.bufferSize = 576; shownSummary.exclusive = false;
        }
        shownSummary.inputGainDb = 1.5f; shownSummary.inputFix = true; shownSummary.inputFixDb = 21.0f; shownSummary.gateDb = -52.0f;
        if (moving) go (Step::done); else step = Step::done;
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
        if (moving) go (Step::running); else step = Step::running;
        meter.setLevel (which == 3 ? 0.004f : 0.3f);
    }
    shownProgress = exampleView.progress;
    if (! moving)
    {
        cardH.snap ((float) heightFor (step));
        contentIn.snap (1.0f);
        titleIn.snap (1.0f);
        lastTitle = {};
        doneAt = -1.0e12;
    }
    layoutButtons();
    repaint();
}

void SetupWizard::finishedWith (const AutoSetup::Summary& s)
{
    shownSummary = s;
    go (Step::done);
    layoutButtons();
    repaint();
}

void SetupWizard::complete (bool keep)
{
    ranToTheEnd = keep && step == Step::done;
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
            // the headphones are somewhere else: one window with the input and the output, and setup is kept
            tertiary.setButtonText ("Change where the sound goes");
            tertiary.setVisible (StandaloneHost::get() != nullptr);
            tertiary.onClick = [this]
            {
                showAudioPicker (proc, getParentComponent() != nullptr ? getParentComponent() : this);
                complete (true);
            };
            break;
        case Step::failed:
            primary.setColour (juce::TextButton::buttonColourId, colours::accent);
            primary.setColour (juce::TextButton::textColourOffId, colours::bg);
            primary.setButtonText ("Try again");
            primary.onClick = [this] { startSetup(); };
            // back to the list of inputs, where another one can be picked or setup skipped
            secondary.setButtonText ("Back");
            secondary.setVisible (true);
            secondary.onClick = [this]
            {
                if (setup != nullptr) setup->cancel();
                setup.reset();
                if (auto* h = StandaloneHost::get()) inputs = AutoSetup::inputs (h->deviceManager());
                picked = std::min (picked, (int) inputs.size() - 1);
                go (Step::welcome);
                layoutButtons();
            };
           #if JUCE_WINDOWS
            tertiary.setButtonText ("Open Windows microphone privacy");
            tertiary.setVisible (true);
            tertiary.onClick = [] { juce::URL ("ms-settings:privacy-microphone").launchInDefaultBrowser(); };
           #endif
            break;
    }
    resized();
}

int SetupWizard::heightFor (Step s) const
{
    const int list = listRows() > 0 ? 44 + listRows() * kRowH : 0;
    return s == Step::welcome ? 372 + list : s == Step::running ? 504 : s == Step::failed ? 470 : 560;
}

juce::Rectangle<int> SetupWizard::listArea() const
{
    if (step != Step::welcome || listRows() == 0) return {};
    // under the headphones note: title 52, text 48, gap 16, note 104, gap 14, heading 26
    return cardBounds().reduced (48, 40).withTrimmedTop (52 + 48 + 16 + 104 + 14 + 26).withHeight (listRows() * kRowH);
}

int SetupWizard::rowAt (juce::Point<int> p) const
{
    const auto area = listArea();
    if (! area.contains (p)) return -1;
    return firstRow + (p.y - area.getY()) / kRowH;
}

void SetupWizard::mouseDown (const juce::MouseEvent& e)
{
    const int row = rowAt (e.getPosition());
    if (row < 0 || row > (int) inputs.size()) return;
    picked = row - 1;
    repaint();
}

void SetupWizard::mouseMove (const juce::MouseEvent& e)
{
    const int row = rowAt (e.getPosition());
    if (row == hoverRow) return;
    hoverRow = row;
    setMouseCursor (row >= 0 ? juce::MouseCursor::PointingHandCursor : juce::MouseCursor::NormalCursor);
    repaint();
}

void SetupWizard::mouseExit (const juce::MouseEvent&)
{
    hoverRow = -1;
    repaint();
}

void SetupWizard::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w)
{
    const int hidden = (int) inputs.size() + 1 - listRows();
    if (hidden <= 0 || ! listArea().contains (e.getPosition())) return;
    firstRow = juce::jlimit (0, hidden, firstRow + (w.deltaY < 0.0f ? 1 : -1));
    repaint();
}

juce::Rectangle<int> SetupWizard::cardBounds() const
{
    // an even height, so the centred card never sits on a half pixel while it grows
    return getLocalBounds().withSizeKeepingCentre (640, 2 * juce::roundToInt (cardH.get() * 0.5f));
}

void SetupWizard::go (Step s)
{
    if (s == step) return;
    step = s;
    lastTitle = {};
    titleIn.snap (1.0f);
    contentIn.snap (0.0f);
    contentIn.to (1.0f, motion::ui, motion::Curve::out, true);
    cardH.to ((float) heightFor (s), motion::overlay, motion::Curve::inOut);
    if (s == Step::done) doneAt = motion::now();
    animate();
}

void SetupWizard::animate()
{
    // the buttons and the meter ride on the card while it changes size
    motion::run (*this,
                 [this]
                 {
                     bool moving = cardH.moving() || contentIn.moving() || titleIn.moving() || motion::now() < doneAt + 800.0;
                     for (auto& r : rowDone) moving = moving || r.moving();
                     return moving;
                 },
                 [this] { resized(); });
}

void SetupWizard::resized()
{
    auto card = cardBounds();
    auto buttons = card.reduced (48, 36).removeFromBottom (44);
    primary.setBounds (buttons.removeFromRight (180));
    buttons.removeFromRight (12);
    if (secondary.isVisible()) secondary.setBounds (buttons.removeFromRight (150));
    buttons.removeFromRight (12);
    if (tertiary.isVisible()) tertiary.setBounds (buttons.removeFromLeft (juce::jmin (270, buttons.getWidth())));
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
    // Only what moves by itself is drawn again: the bar, the line under it and the four steps.
    // Drawing the whole window thirty times a second made setup stutter on big screens.
    if (step == Step::running)
        repaint (cardBounds().reduced (48, 40).withTrimmedTop (146).withHeight (204));
}

void SetupWizard::paint (juce::Graphics& g)
{
    g.fillAll (colours::bg.withAlpha (0.96f));
    const auto card = cardBounds();
    drawCard (g, card.toFloat(), colours::panel, 18.0f);
    auto r = card.reduced (48, 40);

    // what is on the card fades in after a change of step, and never spills over a card still growing
    juce::Graphics::ScopedSaveState whole (g);
    g.reduceClipRegion (card.reduced (2));
    const float in = contentIn.get();
    if (in < 1.0f) g.beginTransparencyLayer (in);

    float headIn = 1.0f; // the heading and the line under it: they change within a step too
    auto title = [&] (const juce::String& t)
    {
        if (t != lastTitle)
        {
            if (lastTitle.isNotEmpty())
            {
                titleIn.snap (0.0f);
                titleIn.to (1.0f, motion::ui, motion::Curve::out, true);
                animate();
            }
            lastTitle = t;
        }
        headIn = titleIn.get();
        g.setColour (colours::text.withMultipliedAlpha (headIn));
        g.setFont (font (30.0f, Weight::display));
        g.drawText (t, r.removeFromTop (44), juce::Justification::centredLeft);
        r.removeFromTop (8);
    };
    auto body = [&] (const juce::String& t, int h, juce::Colour c = colours::textDim)
    {
        g.setColour (c.withMultipliedAlpha (headIn));
        g.setFont (font (16.0f));
        g.drawFittedText (t, r.removeFromTop (h), juce::Justification::topLeft, 6, 1.0f);
    };
    // a check mark is drawn the way a pen would: from its short leg to the end of its long one
    auto check = [&] (juce::Rectangle<float> box, float drawn, float thickness)
    {
        if (drawn <= 0.0f) return;
        juce::Graphics::ScopedSaveState s (g);
        const auto room = box.expanded (4.0f);
        if (drawn < 1.0f) g.reduceClipRegion (room.withWidth (room.getWidth() * drawn).toNearestInt());
        strokeIcon (g, icons::check(), box, colours::good, thickness);
    };
    int line = 0;
    bool oneByOne = false; // the summary's lines arrive one after another; a list of things to check is simply there
    auto bullet = [&] (bool ok, const juce::String& t)
    {
        // a long explanation wraps onto more lines instead of shrinking
        const int textW = std::max (1, r.getWidth() - 30);
        const int lines = juce::jlimit (1, 4, (int) std::ceil (juce::GlyphArrangement::getStringWidth (font (15.0f), t) / (float) textW * 1.08f));
        auto row = r.removeFromTop (32 + 21 * (lines - 1));
        const int wait = 70 + 45 * line++;
        const float shown = oneByOne ? motion::after (doneAt, wait, motion::ui) : 1.0f;
        const auto icon = row.removeFromLeft (30).removeFromTop (32).toFloat().withSizeKeepingCentre (18, 18);
        if (ok) check (icon, oneByOne ? motion::after (doneAt, wait + 60, motion::overlay) : 1.0f, 2.2f);
        else { g.setColour (colours::warn.withMultipliedAlpha (shown)); g.fillEllipse (icon.withSizeKeepingCentre (8, 8)); }
        g.setColour (colours::text.withMultipliedAlpha (shown));
        g.setFont (font (15.0f));
        g.drawFittedText (t, row.reduced (0, lines > 1 ? 5 : 0), lines > 1 ? juce::Justification::topLeft : juce::Justification::centredLeft, lines, 1.0f);
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
                body ("Riffhouse picks the fastest audio settings this computer can run without crackles, "
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
            if (listRows() == 0) break;

            // where the guitar is: say so, or let Riffhouse listen for it
            r.removeFromTop (14);
            drawCaps (g, "Where is your guitar plugged in?", r.removeFromTop (26).toFloat(), 11.5f, colours::textFaint);
            for (int line = 0; line < listRows(); ++line)
            {
                const int row = firstRow + line;
                auto area = r.removeFromTop (kRowH);
                const bool on = row - 1 == picked, over = row == hoverRow;
                const auto ring = area.removeFromLeft (30).toFloat().withSizeKeepingCentre (14.0f, 14.0f);
                g.setColour (on ? colours::accent : (over ? colours::textDim : colours::textFaint));
                g.drawEllipse (ring, 1.5f);
                if (on) g.fillEllipse (ring.reduced (4.0f));
                const bool isAuto = row == 0;
                const auto& in = inputs[(size_t) std::max (0, row - 1)];
                const juce::String side = isAuto ? "Strum, and Riffhouse listens on each input" : (in.type == "ASIO" ? "ASIO driver" : "");
                g.setColour (colours::textFaint);
                g.setFont (font (13.0f));
                const int sideW = side.isEmpty() ? 0 : juce::roundToInt (juce::GlyphArrangement::getStringWidth (g.getCurrentFont(), side)) + 8;
                g.drawText (side, area.removeFromRight (sideW), juce::Justification::centredRight);
                g.setColour (on || over ? colours::text : colours::textDim);
                g.setFont (font (15.0f, on ? Weight::semibold : Weight::regular));
                g.drawFittedText (isAuto ? juce::String ("Find it for me") : in.name, area, juce::Justification::centredLeft, 1, 0.85f);
            }
            if ((int) inputs.size() + 1 > listRows())
            {
                g.setColour (colours::textFaint);
                g.setFont (font (12.5f));
                g.drawText ("Scroll for more inputs", r.removeFromTop (18), juce::Justification::centredLeft);
            }
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
                body (juce::isPositiveAndBelow (picked, (int) inputs.size())
                          ? "Play anything, in any tuning, and keep going. Riffhouse is listening on " + inputs[(size_t) picked].name + "."
                          : juce::String ("Play anything, in any tuning, and keep going. Riffhouse is listening on each input to find the one your guitar is on."), 66);
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
                if (isDone)
                {
                    if (rowDone[i].target() < 1.0f)
                    {
                        rowDone[i].to (1.0f, motion::overlay, motion::Curve::out);
                        animate();
                    }
                }
                else rowDone[i].snap (0.0f);
                const float drawn = rowDone[i].get();
                if (isDone) check (mark, drawn, 2.2f);
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
                    g.setColour (colours::textDim.withMultipliedAlpha (drawn));
                    g.setFont (font (13.5f));
                    g.drawFittedText (found[i], row, juce::Justification::centredRight, 1, 0.9f);
                }
            }
            break;
        }
        case Step::done:
        {
            const auto& s = shownSummary;
            title ("You're all set");
            body ("Here's what Riffhouse set up. You can change any of it later in Settings.", 30);
            r.removeFromTop (10);
            oneByOne = true;
            // an interface carries both ways: the name Windows gives the hardware stands in brackets at the end
            const auto hardware = hardwareOf (s.inputName);
            if (s.outputFollows)
                bullet (true, "Guitar in and sound out on " + (hardware.isNotEmpty() ? hardware : s.inputName));
            else
            {
                bullet (true, "Listening on " + s.inputName);
                if (s.outputName.isNotEmpty() && s.outputName != s.inputName) bullet (true, "Sound out on " + s.outputName);
            }
            bullet (s.latencyMs < 15.0, "About " + juce::String (s.latencyMs, 1) + " ms from string to ear ("
                                          + juce::String (s.bufferSize) + " samples" + (s.exclusive ? ", exclusive mode)" : ")"));
            bullet (true, "Input level " + juce::String (s.inputGainDb >= 0 ? "+" : "") + juce::String (s.inputGainDb, 1)
                              + " dB, set from your hardest playing");
            bullet (true, s.inputFix ? "Input Fix on: brings back " + juce::String (juce::roundToInt (s.inputFixDb)) + " dB of treble your input was losing"
                                     : "Input Fix off: your input already has its full treble");
            bullet (true, "Gate at " + juce::String (juce::roundToInt (s.gateDb)) + " dB, and it follows the room if the noise changes");
            bullet (true, "Auto level on: every tone comes out as loud as the last, matched to your playing");
            bullet (true, "Hum filter on automatic (removes 50 or 60 Hz buzz)");
            juce::StringArray outs;
            if (auto* host = StandaloneHost::get())
                for (auto* t : host->deviceManager().getAvailableDeviceTypes())
                    if (t->getTypeName() == "Windows Audio") outs = t->getDeviceNames (false);
            const auto shared = whyShared (s.deviceType, s.inputName, s.outputName, outs);
            if (s.clipping)
                bullet (false, "Your input is clipping. Turn the guitar volume down a little.");
            else if (shared.isNotEmpty())
                bullet (false, shared);
            else if (s.fixHeldBack)
                bullet (false, "This input hisses, so a little of the Input Fix was held back.");
            else if (s.latencyMs >= 15.0)
                bullet (false, "Delay is a bit high. Closing other audio apps can help.");
            else if (! AutoSetup::looksLikeOnboardInput (s.inputName, s.deviceType))
                bullet (false, "Hearing the guitar twice? Switch off Direct Monitor on your interface.");
            else if (s.exclusive)
                bullet (false, "Other apps' sound pauses while Riffhouse is open. Use Play along for songs.");
            break;
        }
        case Step::failed:
        {
            title ("We can't hear your guitar");
            body (juce::isPositiveAndBelow (picked, (int) inputs.size())
                      ? "Riffhouse listened on " + inputs[(size_t) picked].name + " but didn't hear anything. Go back to pick another input, or check these:"
                      : juce::String ("Riffhouse listened on every input but didn't hear anything. Plug in now and it starts again by itself, or check these:"), 50);
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
    if (in < 1.0f) g.endTransparencyLayer();
}
} // namespace rh::ui
