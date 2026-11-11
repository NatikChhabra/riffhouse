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

void SetupWizard::begin()
{
    if (auto* h = StandaloneHost::get()) h->setupRunning (true);
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
    setup->onPhaseChanged = [this]
    {
        if (setup == nullptr) return;
        if (setup->phase() == AutoSetup::Phase::finished)
        {
            step = Step::done;
            if (auto* h = StandaloneHost::get())
            {
                h->saveAudioState();
                h->settings().setValue ("setupDone", true);
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

void SetupWizard::complete (bool keep)
{
    if (! keep && setup != nullptr) setup->cancel();
    setup.reset();
    stopTimer();
    if (auto* h = StandaloneHost::get())
    {
        h->settings().setValue ("setupDone", true);
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
            primary.setButtonText ("Start setup");
            primary.onClick = [this] { startSetup(); };
            secondary.setButtonText ("Skip for now");
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
    const int h = step == Step::welcome ? 372 : step == Step::running ? 470 : step == Step::failed ? 470 : 520;
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
    meter.setBounds (card.reduced (48, 0).withY (card.getY() + 222).withHeight (12));
}

void SetupWizard::timerCallback()
{
    pulse += 0.06f;
    if (setup != nullptr) meter.setLevel (setup->liveLevel());
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
            title ("Let's get your guitar sounding right");
            body ("This takes about 20 seconds. Riffhouse finds your guitar, picks the fastest audio settings your "
                  "computer can handle without crackles, and sets the input level and noise gate for you.", 48);
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
            const auto ph = setup != nullptr ? setup->phase() : AutoSetup::Phase::idle;
            if (ph == AutoSetup::Phase::quiet)
            {
                title ("Now mute the strings");
                body ("Rest your hand on the strings so nothing rings, and keep still for a moment. "
                      "Riffhouse is measuring the background noise to set the gate.", 70);
            }
            else if (ph == AutoSetup::Phase::tuning)
            {
                title ("Keep strumming");
                body ("Found your guitar. Now finding the lowest delay your computer can run without crackles.", 70);
            }
            else
            {
                title ("Strum your guitar");
                body ("Keep playing anything, any tuning. Riffhouse is checking each input to find the one your guitar is on.", 70);
            }
            // steps
            r.removeFromTop (6);
            const char* names[] = { "Find the guitar", "Lowest safe delay", "Level and gate" };
            const int active = ph == AutoSetup::Phase::listening ? 0 : ph == AutoSetup::Phase::tuning ? 1 : 2;
            auto row = r.removeFromTop (40);
            for (int i = 0; i < 3; ++i)
            {
                auto s = row.removeFromLeft (row.getWidth() / (3 - i)).reduced (0, 4).toFloat();
                g.setColour (i < active ? colours::good.withAlpha (0.15f) : (i == active ? colours::accent.withAlpha (0.15f) : colours::raised));
                g.fillRoundedRectangle (s.reduced (4, 0), 8.0f);
                g.setColour (i < active ? colours::good : (i == active ? colours::accentHi : colours::textFaint));
                g.setFont (font (13.5f, Weight::semibold));
                g.drawText (names[i], s, juce::Justification::centred);
            }
            r.removeFromTop (40);
            // live level meter sits here (child component)
            r.removeFromTop (16);
            g.setColour (colours::textDim);
            g.setFont (font (13.5f));
            g.drawText (setup != nullptr ? setup->detail() : juce::String(), r.removeFromTop (24), juce::Justification::centredLeft);
            r.removeFromTop (8);
            auto bar = r.removeFromTop (6).toFloat();
            g.setColour (colours::raisedHi);
            g.fillRoundedRectangle (bar, 3.0f);
            g.setColour (colours::accent);
            g.fillRoundedRectangle (bar.withWidth (bar.getWidth() * (setup != nullptr ? setup->progress() : 0.0f)), 3.0f);
            if (ph == AutoSetup::Phase::quiet && setup->progress() < 0.41f)
            {
                g.setColour (colours::accentHi);
                g.setFont (font (15.0f, Weight::semibold));
                g.drawText (utf8 ("Measuring in a moment\xe2\x80\xa6"), r.removeFromTop (34), juce::Justification::centredLeft);
            }
            break;
        }
        case Step::done:
        {
            const auto& s = setup->summary();
            title ("You're all set");
            body ("Here's what Riffhouse set up. You can change any of it later in Settings.", 30);
            r.removeFromTop (10);
            bullet (true, "Guitar on " + s.inputName);
            bullet (s.latencyMs < 15.0, "About " + juce::String (s.latencyMs, 1) + " ms from string to ear ("
                                          + juce::String (s.bufferSize) + " samples" + (s.exclusive ? ", exclusive mode)" : ")"));
            bullet (true, "Input level " + juce::String (s.inputGainDb >= 0 ? "+" : "") + juce::String (s.inputGainDb, 1) + " dB, gate at "
                              + juce::String (juce::roundToInt (s.gateDb)) + " dB");
            bullet (true, s.inputFix ? "Input Fix on: brings back " + juce::String (juce::roundToInt (s.inputFixDb)) + " dB of treble your input was losing"
                                     : "Input Fix off: your input already has its full treble");
            bullet (true, "Hum filter on automatic (removes 50 or 60 Hz buzz)");
            if (s.clipping)
                bullet (false, "Your input is clipping. Turn the guitar volume down a little.");
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
