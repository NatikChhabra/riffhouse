// Riffhouse - free, open-source guitar rig
// Copyright (C) 2026 Riffhouse contributors. Licensed under AGPL-3.0-or-later.
#include "AudioPicker.h"

#include "../engine/DeviceNames.h"
#include "../plugin/StandaloneHost.h"

namespace rh::ui
{
namespace
{
    constexpr int kRow = 36, kGap = 12, kLabel = 96;

    // The whole text, wrapped to the width, never cut short: a sentence about what went wrong
    // is the one thing in this window that has to be read to the end.
    juce::TextLayout wrapped (const juce::String& text, const juce::Font& f, juce::Colour c, float width)
    {
        juce::AttributedString a;
        a.append (text, f, c);
        a.setJustification (juce::Justification::topLeft);
        a.setLineSpacing (2.0f);
        juce::TextLayout t;
        t.createLayout (a, width);
        return t;
    }
    juce::Font statusFont (bool problem) { return font (14.0f, problem ? Weight::semibold : Weight::regular); }
    juce::Font noteFont() { return font (13.5f); }

    juce::AudioIODeviceType* findType (juce::AudioDeviceManager& dm, const juce::String& name)
    {
        for (auto* t : dm.getAvailableDeviceTypes())
            if (t->getTypeName() == name) return t;
        return nullptr;
    }
}

juce::String AudioPicker::driverLabel (const juce::String& t)
{
    if (t.isEmpty()) return "Automatic (recommended)";
    if (t == "ASIO") return "ASIO: audio interface drivers";
    if (t == "Windows Audio (Exclusive Mode)") return "WASAPI exclusive: fastest Windows mode";
    if (t == "Windows Audio (Low Latency Mode)") return "WASAPI low latency";
    if (t == "Windows Audio") return "WASAPI shared: other apps play too";
    if (t == "DirectSound") return "DirectSound: oldest and slowest";
    return t;
}

AudioPicker::AudioPicker (RiffhouseProcessor& p) : proc (p)
{
    if (auto* host = StandaloneHost::get()) forced = host->settings().getValue ("audioDriver");
    addAndMakeVisible (driver);
    addChildComponent (socket);
    // the names a screen reader says, the same as the labels drawn beside them
    driver.setTitle ("Driver");
    input.setTitle ("Guitar in");
    socket.setTitle ("Socket");
    output.setTitle ("Sound out");
    buffer.setTitle ("Buffer");
    socket.setTooltip ("The pair of sockets on your interface the guitar is plugged into");
    driver.onChange = [this]
    {
        if (filling) return;
        const int i = driver.getSelectedItemIndex();
        if (! juce::isPositiveAndBelow (i, driverTypes.size())) return;
        forced = driverTypes[i];
        if (auto* host = StandaloneHost::get()) host->settings().setValue ("audioDriver", forced);
        refresh(); // the inputs and outputs that driver offers
        apply (Change::driver);
    };
    for (auto* b : { &input, &output, &buffer })
    {
        addAndMakeVisible (*b);
        b->setTextWhenNothingSelected ("Nothing found");
        b->setTextWhenNoChoicesAvailable ("Nothing found");
    }
    buffer.setTextWhenNothingSelected ("Set by the sound card");
    input.onChange = [this]
    {
        if (filling) return;
        // an audio interface is listened through too: its output comes with it, and can be changed back below
        if (const auto* in = picked())
        {
            juce::StringArray outputs;
            for (int i = 0; i < output.getNumItems(); ++i) outputs.add (output.getItemText (i));
            const auto follow = AudioSwitch::outputToFollow (in->type, in->name, outputs);
            followed = follow.isNotEmpty() && follow != output.getText();
            if (followed) output.setSelectedItemIndex (outputs.indexOf (follow), juce::dontSendNotification);
        }
        apply (Change::input);
    };
    output.onChange = [this]
    {
        if (filling) return;
        if (isAsio && output.getSelectedId() >= kOtherOutputs) pickOtherOutputForAsio();
        else apply (Change::output);
    };
    buffer.onChange = [this] { if (! filling) apply (Change::buffer); };
    socket.onChange = [this] { if (! filling) apply (Change::socket); };
    sharing.onClick = [this] { if (! filling) apply (Change::sharing); };
    addChildComponent (sharing);
    addChildComponent (panel);
    panel.onClick = [this]
    {
        if (auto* host = StandaloneHost::get())
            if (auto* dev = host->deviceManager().getCurrentAudioDevice())
                if (dev->hasControlPanel() && dev->showControlPanel())
                {
                    // the driver's own window may have changed the buffer: open the device afresh with it
                    host->deviceManager().restartLastAudioDevice();
                    host->saveAudioState();
                    refresh();
                }
    };
    if (auto* host = StandaloneHost::get())
        changer = std::make_unique<AudioSwitch> (host->deviceManager(), proc);
    setSize (580, 452);
    refresh();
}

const AutoSetup::Input* AudioPicker::picked() const
{
    const int i = input.getSelectedItemIndex();
    return juce::isPositiveAndBelow (i, (int) inputs.size()) ? &inputs[(size_t) i] : nullptr;
}

void AudioPicker::refresh()
{
    auto* host = StandaloneHost::get();
    if (host == nullptr || example) return;
    auto& dm = host->deviceManager();
    const auto now = AudioOpen::running (dm);
    auto* dev = dm.getCurrentAudioDevice();
    const bool asioNow = now.type == "ASIO";

    filling = true;
    // the driver row: Automatic, then every driver this computer has
    driver.clear (juce::dontSendNotification);
    driverTypes.clear();
    driverTypes.add ({});
    for (auto* t : dm.getAvailableDeviceTypes())
        driverTypes.add (t->getTypeName());
    if (! driverTypes.contains (forced)) forced = {};
    for (int i = 0; i < driverTypes.size(); ++i)
        driver.addItem (driverLabel (driverTypes[i]), i + 1);
    driver.setSelectedItemIndex (driverTypes.indexOf (forced), juce::dontSendNotification);

    if (forced.isEmpty())
    {
        // every input a guitar could be on: an interface's own ASIO driver first, then what Windows lists.
        // Drivers that only wrap Windows audio are left to the player who asks for ASIO by name.
        inputs = AutoSetup::inputs (dm);
        for (size_t i = inputs.size(); i-- > 0;)
            if (inputs[i].type == "ASIO" && AutoSetup::isAsioWrapper (inputs[i].name)) inputs.erase (inputs.begin() + (long) i);
    }
    else
    {
        // one driver, every input it has: ASIO4ALL and FlexASIO included
        inputs.clear();
        if (auto* t = findType (dm, forced))
        {
            t->scanForDevices();
            for (const auto& n : t->getDeviceNames (true))
                inputs.push_back ({ forced, n });
        }
    }
    input.clear (juce::dontSendNotification);
    int sel = -1;
    for (size_t i = 0; i < inputs.size(); ++i)
    {
        const bool asio = inputs[i].type == "ASIO";
        input.addItem (inputs[i].name + (asio ? "  (ASIO driver)" : ""), (int) i + 1);
        if (now.valid() && inputs[i].name == now.input && asio == asioNow) sel = (int) i;
    }
    input.setSelectedItemIndex (sel, juce::dontSendNotification);
    isAsio = sel >= 0 ? inputs[(size_t) sel].type == "ASIO" : asioNow;

    // outputs go by the same names in every Windows mode; DirectSound has its own
    output.clear (juce::dontSendNotification);
    pairs = 0;
    auto* outType = findType (dm, forced.isNotEmpty() && forced != "ASIO" ? forced
                                  : asioNow || ! now.valid() ? juce::String ("Windows Audio") : now.type);
    if (outType == nullptr && ! asioNow) outType = dm.getCurrentDeviceTypeObject();
    if (asioNow && dev != nullptr)
    {
        // the interface's own outputs, two at a time: headphones and speakers each hang on a pair
        const auto names = dev->getOutputChannelNames();
        pairs = names.size() / 2;
        for (int i = 0; i < pairs; ++i) output.addItem (outputPairName (names[2 * i], names[2 * i + 1]), i + 1);
        output.setSelectedId (juce::jlimit (0, std::max (0, pairs - 1), std::max (0, now.outputPair)) + 1, juce::dontSendNotification);
        if (outType != nullptr && outType->getDeviceNames (false).size() > 0)
        {
            output.addSectionHeading ("Somewhere else (through Windows, a little slower)");
            const auto outs = outType->getDeviceNames (false);
            for (int i = 0; i < outs.size(); ++i) output.addItem (outs[i], kOtherOutputs + i);
        }
    }
    else if (outType != nullptr)
    {
        const auto outs = outType->getDeviceNames (false);
        output.addItemList (outs, 1);
        int out = outs.indexOf (now.output);
        if (out < 0) out = outType->getDefaultDeviceIndex (false);
        if (! outs.isEmpty()) output.setSelectedItemIndex (juce::jlimit (0, outs.size() - 1, out), juce::dontSendNotification);
    }

    // an interface with more than two sockets: which pair the guitar is plugged into
    socket.clear (juce::dontSendNotification);
    if (dev != nullptr && sel >= 0)
    {
        const auto ins = dev->getInputChannelNames();
        for (int i = 0; 2 * i < ins.size(); ++i)
            socket.addItem (2 * i + 1 < ins.size() ? outputPairName (ins[2 * i], ins[2 * i + 1]) : ins[2 * i], i + 1);
        socket.setSelectedId (std::max (0, now.inputPair) + 1, juce::dontSendNotification);
    }
    socket.setVisible (socket.getNumItems() > 1);

    buffer.clear (juce::dontSendNotification);
    bufferSizes.clear();
    if (dev != nullptr && sel >= 0)
    {
        const double sr = std::max (1.0, dev->getCurrentSampleRate());
        for (int b : dev->getAvailableBufferSizes())
        {
            if (b > 2048) continue;
            bufferSizes.add (b);
            buffer.addItem (juce::String (b) + " samples  (" + juce::String (1000.0 * b / sr, 1) + " ms)", bufferSizes.size());
        }
        buffer.setSelectedItemIndex (bufferSizes.indexOf (dev->getCurrentBufferSizeSamples()), juce::dontSendNotification);
    }
    const bool canShare = findType (dm, "Windows Audio (Exclusive Mode)") != nullptr;
    sharing.setToggleState (now.valid() ? AudioSwitch::isShared (now.type) : false, juce::dontSendNotification);
    filling = false;

    note = {};
    if (inputs.empty())
        note = "No input was found on this computer. Plug the guitar in, or connect your audio interface, and open this window again.";
    else if (asioNow && dev != nullptr
             && asioBufferAdvice (dev->getCurrentBufferSizeSamples(), dev->getCurrentSampleRate(), bufferSizes.size(), panel.getButtonText()).isNotEmpty())
        note = asioBufferAdvice (dev->getCurrentBufferSizeSamples(), dev->getCurrentSampleRate(), bufferSizes.size(), panel.getButtonText());
    else if (asioNow)
        note = "This is your interface's own driver, the fastest way in. Under Sound out, pick the outputs your headphones or speakers are on.";
    else if (now.type == "Windows Audio (Exclusive Mode)")
        note = "Riffhouse has the sound card to itself, which gives the shortest delay. Other apps are silent while it is open.";
    else if (now.type == "DirectSound")
        note = "Only the slowest way of driving this input started. Close other audio apps and pick the input again.";
    else if (now.valid() && forced.isEmpty() && ! sharing.getToggleState() && whyShared (now.type, now.input, now.output).isNotEmpty())
        note = whyShared (now.type, now.input, now.output, findType (dm, "Windows Audio") != nullptr ? findType (dm, "Windows Audio")->getDeviceNames (false) : juce::StringArray());
    else if (now.valid() && canShare)
        note = "Other apps can play too. The delay is a little longer than with the sound card to itself.";

    output.setVisible (true);
    sharing.setVisible (forced.isEmpty() && ! isAsio && canShare);
    // The driver's own window is full of settings nobody needs here. It is offered only when the
    // driver keeps the delay to itself and that window is the one place to change it.
    panel.setVisible (dev != nullptr && dev->hasControlPanel() && bufferSizes.size() <= 1);
    if (status.isEmpty()) status = AudioSwitch::running (dm, proc);
    const int extra = (socket.isVisible() ? 1 : 0) + (sharing.isVisible() ? 1 : 0) + (panel.isVisible() ? 1 : 0);
    fitToText (extra);
    resized();
    repaint();
}

AudioPicker::~AudioPicker()
{
    // A change still running when the window closes: it is ended here, before the boxes go, and
    // the connection guard is told the change is over (the switch no longer calls back).
    const bool wasBusy = changer != nullptr && changer->busy();
    changer.reset();
    if (wasBusy)
        if (auto* h = StandaloneHost::get()) h->setupRunning (false);
}

void AudioPicker::apply (Change what)
{
    // a new driver lists its own inputs: when the one in use is not among them, the first is tried
    if (what == Change::driver && input.getSelectedItemIndex() < 0 && input.getNumItems() > 0)
        input.setSelectedItemIndex (0, juce::dontSendNotification);
    auto* host = StandaloneHost::get();
    const auto* in = picked();
    if (host == nullptr || changer == nullptr || changer->busy() || in == nullptr) return;
    auto& dm = host->deviceManager();
    const auto now = AudioOpen::running (dm);
    const bool sameInput = now.valid() && now.input == in->name && (now.type == "ASIO") == (in->type == "ASIO");

    AudioChoice c;
    c.input = in->name;
    c.output = output.getText();
    if (in->type == "ASIO" && now.type == "ASIO" && sameInput && output.getSelectedId() > 0 && output.getSelectedId() <= pairs)
        c.outputPair = output.getSelectedId() - 1;
    c.types = forced.isNotEmpty() ? juce::StringArray { forced } // the driver the player chose, and only that
                                  : AudioSwitch::modesFor (dm, in->type, in->name, sharing.getToggleState());
    if (sameInput) c.inputPair = what == Change::socket ? socket.getSelectedId() - 1 : now.inputPair;
    if (sameInput && (what == Change::output || what == Change::buffer || what == Change::socket) && c.types.contains (now.type))
    {
        // only the output or the buffer changes: stay with the mode that runs
        c.types.removeString (now.type);
        c.types.insert (0, now.type);
        // a buffer size belongs to the device it was listed for
        c.bufferSize = what == Change::buffer && buffer.getSelectedItemIndex() >= 0 ? bufferSizes[buffer.getSelectedItemIndex()] : now.bufferSize;
    }
    if (c.types.isEmpty() || c.input.isEmpty()) return;

    status = "Trying " + c.input + "...";
    statusIsProblem = false;
    for (juce::Component* b : { (juce::Component*) &driver, (juce::Component*) &input, (juce::Component*) &output, (juce::Component*) &buffer, (juce::Component*) &socket, (juce::Component*) &sharing })
        b->setEnabled (false);
    repaint();
    host->setupRunning (true); // whoever guards the connection stands aside while it is changed on purpose
    // The sound card is opened a moment later, so "Trying..." is on screen before the window
    // waits for it.
    juce::Timer::callAfterDelay (60, [safe = juce::Component::SafePointer<AudioPicker> (this), c]
    {
        auto finished = [safe] (AudioSwitch::Result r)
        {
            auto* h = StandaloneHost::get();
            if (h != nullptr)
            {
                if (r.ok) h->saveAudioState(); // this is now the connection to keep alive
                h->setupRunning (false);
                // a driver that would not play is worth a report, if the player wants to send one
                if (! r.ok) h->offerReport ("The sound card change did not work.");
            }
            if (safe == nullptr) return;
            for (juce::Component* b : { (juce::Component*) &safe->driver, (juce::Component*) &safe->input, (juce::Component*) &safe->output, (juce::Component*) &safe->buffer, (juce::Component*) &safe->socket, (juce::Component*) &safe->sharing })
                b->setEnabled (true);
            safe->status = r.message;
            if (! r.ok && safe->forced.isNotEmpty())
            {
                // the boxes show what runs now, not the driver that failed: the driver box follows
                // the one Riffhouse went back to (Automatic when nothing is open)
                const auto back = h != nullptr ? AudioOpen::running (h->deviceManager()) : AudioOpen {};
                if (h != nullptr && back.type != safe->forced)
                {
                    safe->forced = back.valid() ? back.type : juce::String();
                    h->settings().setValue ("audioDriver", safe->forced);
                }
                safe->status << " Pick another input, another driver, or Automatic above: nothing is locked.";
            }
            if (r.ok && ! r.fastest) safe->status << " The fastest way would not start on this input, so the next one is in use.";
            if (r.ok && safe->followed) safe->status << " Sound now comes out of " << safe->output.getText() << ".";
            if (r.ok && safe->afterNote.isNotEmpty()) safe->status << " " << safe->afterNote;
            safe->afterNote = {};
            safe->followed = false;
            safe->statusIsProblem = ! r.ok;
            safe->refresh();
        };
        if (safe == nullptr || safe->changer == nullptr)
        {
            if (auto* h = StandaloneHost::get()) h->setupRunning (false);
            return;
        }
        safe->changer->apply (c, finished);
    });
}

void AudioPicker::pickOtherOutputForAsio()
{
    const auto* in = picked();
    const auto wanted = output.getText();
    if (in == nullptr || in->type != "ASIO") return;
    // the same interface as Windows lists it, to play through any output
    for (size_t i = 0; i < inputs.size(); ++i)
        if (inputs[i].type != "ASIO" && sameHardware (in->name, inputs[i].name))
        {
            filling = true;
            input.setSelectedItemIndex ((int) i, juce::dontSendNotification);
            isAsio = false;
            filling = false;
            afterNote = "ASIO plays only through the interface itself, so its Windows driver sends the sound to " + wanted + ".";
            apply (Change::input);
            return;
        }
    status = "ASIO plays only through " + in->name + " itself. To hear the guitar on " + wanted
           + ", pick the interface's Windows entry under Guitar in (the one without \"ASIO driver\").";
    statusIsProblem = true;
    refresh();
}

void AudioPicker::showExample (bool asio)
{
    example = true;
    filling = true;
    for (auto* b : { &driver, &input, &socket, &output, &buffer }) b->clear (juce::dontSendNotification);
    socket.setVisible (asio);
    driver.addItem (driverLabel (asio ? juce::String ("ASIO") : juce::String()), 1);
    driver.setSelectedItemIndex (0, juce::dontSendNotification);
    isAsio = asio;
    if (asio)
    {
        input.addItem ("Focusrite USB ASIO  (ASIO driver)", 1);
        socket.addItem (outputPairName ("Input 1", "Input 2"), 1);
        socket.addItem (outputPairName ("Input 3", "Input 4"), 2);
        socket.setSelectedId (1, juce::dontSendNotification);
        // the channel names an interface's driver gives, paired the way the list pairs them
        pairs = 2;
        output.addItem (outputPairName ("Output 1", "Output 2"), 1);
        output.addItem (outputPairName ("Headphones L", "Headphones R"), 2);
        output.addSectionHeading ("Somewhere else (through Windows, a little slower)");
        output.addItem ("Speakers (Realtek(R) Audio)", kOtherOutputs);
        output.setSelectedId (2, juce::dontSendNotification);
        buffer.addItem ("64 samples  (1.3 ms)", 1);
        buffer.addItem ("128 samples  (2.7 ms)", 2);
        buffer.setSelectedId (1, juce::dontSendNotification);
        note = "This is your interface's own driver, the fastest way in. Under Sound out, pick the outputs your headphones or speakers are on.";
        status = "Running: 64 samples at 48.0 kHz, about 4.9 ms from string to ear.";
    }
    else
    {
        input.addItem ("Microphone (High Definition Audio)", 1);
        output.addItem ("Headphones (High Definition Audio)", 1);
        output.setSelectedItemIndex (0, juce::dontSendNotification);
        buffer.addItem ("144 samples  (3.0 ms)", 1);
        buffer.setSelectedItemIndex (0, juce::dontSendNotification);
        note = "Riffhouse has the sound card to itself, which gives the shortest delay. Other apps are silent while it is open.";
        status = "Running: 144 samples at 48.0 kHz, about 9.3 ms from string to ear.";
    }
    input.setSelectedItemIndex (0, juce::dontSendNotification);
    output.setVisible (true);
    sharing.setVisible (! asio);
    sharing.setToggleState (false, juce::dontSendNotification);
    panel.setVisible (false);
    filling = false;
    fitToText ((sharing.isVisible() ? 1 : 0) + (socket.isVisible() ? 1 : 0));
    resized();
    repaint();
}

void AudioPicker::fitToText (int extra)
{
    // The window has room for one row beyond the four everyone has; each further one makes it
    // taller, and it grows further when what it has to say needs the room
    const float textW = 580.0f - 56.0f;
    const int rows = 4 + extra;
    const int words = (int) std::ceil ((status.isNotEmpty() ? wrapped (status, statusFont (statusIsProblem), colours::text, textW).getHeight() + 6.0f : 0.0f)
                                       + (note.isNotEmpty() ? wrapped (note, noteFont(), colours::text, textW).getHeight() : 0.0f));
    setSize (580, std::max (452 + std::max (0, extra - 1) * (kRow + kGap), 24 + 58 + rows * (kRow + kGap) + 11 + words + 24));
}

void AudioPicker::showProblemExample()
{
    showExample();
    status = "Windows runs Guitar (Realtek USB Audio) and your headphones at different rates, and Windows Audio, low latency mode needs one rate for both. "
             "In Windows sound settings, open each one's Properties, then Advanced, and set both to 48000 Hz. Riffhouse went back to Guitar (Realtek USB Audio) on "
             "Windows Audio, exclusive mode, which works. Pick another input, another driver, or Automatic above: nothing is locked.";
    statusIsProblem = true;
    fitToText (sharing.isVisible() ? 1 : 0);
    resized();
    repaint();
}

void AudioPicker::resized()
{
    auto r = getLocalBounds().reduced (28, 24);
    r.removeFromTop (58); // heading and a line under it
    for (auto* b : { &driver, &input, &socket, &output, &buffer })
    {
        if (! b->isVisible()) continue;
        b->setBounds (r.removeFromTop (kRow).withTrimmedLeft (kLabel));
        r.removeFromTop (kGap);
    }
    if (sharing.isVisible()) { sharing.setBounds (r.removeFromTop (kRow).withTrimmedLeft (kLabel)); r.removeFromTop (kGap); }
    if (panel.isVisible()) panel.setBounds (r.removeFromTop (kRow).withTrimmedLeft (kLabel).removeFromLeft (260));
}

void AudioPicker::paint (juce::Graphics& g)
{
    g.fillAll (colours::panel);
    auto r = getLocalBounds().reduced (28, 24);
    g.setColour (colours::text);
    g.setFont (font (20.0f, Weight::display));
    g.drawText ("Driver, input and output", r.removeFromTop (28), juce::Justification::centredLeft);
    g.setColour (colours::textDim);
    g.setFont (font (13.5f));
    g.drawText ("Every change is tried first. If no sound comes through, the last working choice comes back.", r.removeFromTop (22), juce::Justification::centredLeft, true);
    r.removeFromTop (8);

    const char* labels[] = { "Driver", "Guitar in", "Socket", "Sound out", "Buffer" };
    juce::ComboBox* boxes[] = { &driver, &input, &socket, &output, &buffer };
    g.setFont (font (14.0f));
    for (int i = 0; i < 5; ++i)
    {
        if (! boxes[i]->isVisible()) continue;
        g.setColour (colours::textDim);
        g.drawText (labels[i], r.removeFromTop (kRow).removeFromLeft (kLabel), juce::Justification::centredLeft);
        r.removeFromTop (kGap);
    }
    if (sharing.isVisible()) { r.removeFromTop (kRow); r.removeFromTop (kGap); }
    if (panel.isVisible()) { r.removeFromTop (kRow); r.removeFromTop (kGap); }

    // what happened, then the standing remark about how it runs now
    g.setColour (juce::Colours::white.withAlpha (0.10f));
    g.fillRect (r.removeFromTop (1));
    r.removeFromTop (10);
    auto area = r.toFloat();
    if (status.isNotEmpty())
    {
        const auto t = wrapped (status, statusFont (statusIsProblem), statusIsProblem ? colours::warn : colours::text, area.getWidth());
        t.draw (g, area.removeFromTop (t.getHeight()));
        area.removeFromTop (6.0f);
    }
    if (note.isNotEmpty())
    {
        const auto t = wrapped (note, noteFont(), colours::textDim, area.getWidth());
        t.draw (g, area.removeFromTop (t.getHeight()));
    }
}

void showAudioPicker (RiffhouseProcessor& proc, juce::Component* near)
{
    if (StandaloneHost::get() == nullptr) return;
    juce::DialogWindow::LaunchOptions o;
    o.content.setOwned (new AudioPicker (proc));
    o.dialogTitle = "Audio settings";
    o.dialogBackgroundColour = colours::panel;
    o.escapeKeyTriggersCloseButton = true;
    o.useNativeTitleBar = true;
    o.resizable = false;
    o.componentToCentreAround = near;
    o.launchAsync();
}
} // namespace rh::ui
