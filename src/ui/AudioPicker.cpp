// Riffhouse - free, open-source guitar rig
// Copyright (C) 2026 Riffhouse contributors. Licensed under AGPL-3.0-or-later.
#include "AudioPicker.h"

#include "../plugin/StandaloneHost.h"

namespace rh::ui
{
namespace
{
    constexpr int kRow = 36, kGap = 12, kLabel = 96;

    juce::AudioIODeviceType* findType (juce::AudioDeviceManager& dm, const juce::String& name)
    {
        for (auto* t : dm.getAvailableDeviceTypes())
            if (t->getTypeName() == name) return t;
        return nullptr;
    }
}

AudioPicker::AudioPicker (RiffhouseProcessor& p) : proc (p)
{
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
    output.onChange = [this] { if (! filling) apply (Change::output); };
    buffer.onChange = [this] { if (! filling) apply (Change::buffer); };
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
    setSize (560, 392);
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
    // every input a guitar could be on: an interface's own ASIO driver first, then what Windows lists
    inputs = AutoSetup::inputs (dm);
    for (size_t i = inputs.size(); i-- > 0;)
        if (inputs[i].type == "ASIO" && AutoSetup::isAsioWrapper (inputs[i].name)) inputs.erase (inputs.begin() + (long) i);
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
    auto* outType = findType (dm, asioNow || ! now.valid() ? juce::String ("Windows Audio") : now.type);
    if (outType == nullptr) outType = dm.getCurrentDeviceTypeObject();
    if (outType != nullptr)
    {
        const auto outs = outType->getDeviceNames (false);
        output.addItemList (outs, 1);
        int out = outs.indexOf (now.output);
        if (out < 0) out = outType->getDefaultDeviceIndex (false);
        if (! outs.isEmpty()) output.setSelectedItemIndex (juce::jlimit (0, outs.size() - 1, out), juce::dontSendNotification);
    }

    buffer.clear (juce::dontSendNotification);
    bufferSizes.clear();
    if (dev != nullptr && sel >= 0)
    {
        const double sr = std::max (1.0, dev->getCurrentSampleRate());
        for (int b : dev->getAvailableBufferSizes())
        {
            if (b > 2048) continue;
            bufferSizes.add (b);
            buffer.addItem (juce::String (1000.0 * b / sr, 1) + " ms  (" + juce::String (b) + " samples)", bufferSizes.size());
        }
        buffer.setSelectedItemIndex (bufferSizes.indexOf (dev->getCurrentBufferSizeSamples()), juce::dontSendNotification);
    }
    const bool canShare = findType (dm, "Windows Audio (Exclusive Mode)") != nullptr;
    sharing.setToggleState (now.valid() ? AudioSwitch::isShared (now.type) : false, juce::dontSendNotification);
    filling = false;

    note = {};
    if (inputs.empty())
        note = "No input was found on this computer. Plug the guitar in, or connect your audio interface, and open this window again.";
    else if (asioNow)
        note = "This is your interface's own driver, the fastest way in. The sound comes out of the interface too.";
    else if (now.type == "Windows Audio (Exclusive Mode)")
        note = "Riffhouse has the sound card to itself, which gives the shortest delay. Other apps are silent while it is open.";
    else if (now.type == "DirectSound")
        note = "Only the slowest way of driving this input started. Close other audio apps and pick the input again.";
    else if (now.valid() && canShare)
        note = "Other apps can play too. The delay is a little longer than with the sound card to itself.";

    output.setVisible (! isAsio);
    sharing.setVisible (! isAsio && canShare);
    panel.setVisible (dev != nullptr && dev->hasControlPanel());
    if (status.isEmpty()) status = AudioSwitch::running (dm, proc);
    resized();
    repaint();
}

void AudioPicker::apply (Change what)
{
    auto* host = StandaloneHost::get();
    const auto* in = picked();
    if (host == nullptr || changer == nullptr || changer->busy() || in == nullptr) return;
    auto& dm = host->deviceManager();
    const auto now = AudioOpen::running (dm);
    const bool sameInput = now.valid() && now.input == in->name && (now.type == "ASIO") == (in->type == "ASIO");

    AudioChoice c;
    c.input = in->name;
    c.output = output.getText();
    c.types = AudioSwitch::modesFor (dm, in->type, in->name, sharing.getToggleState());
    if (sameInput && (what == Change::output || what == Change::buffer) && c.types.contains (now.type))
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
    for (juce::Component* b : { (juce::Component*) &input, (juce::Component*) &output, (juce::Component*) &buffer, (juce::Component*) &sharing })
        b->setEnabled (false);
    repaint();
    host->setupRunning (true); // whoever guards the connection stands aside while it is changed on purpose
    // The sound card is opened a moment later, so "Trying..." is on screen before the window
    // waits for it.
    juce::Timer::callAfterDelay (60, [safe = juce::Component::SafePointer<AudioPicker> (this), c]
    {
        auto finished = [safe] (AudioSwitch::Result r)
        {
            if (auto* h = StandaloneHost::get())
            {
                if (r.ok) h->saveAudioState(); // this is now the connection to keep alive
                h->setupRunning (false);
            }
            if (safe == nullptr) return;
            for (juce::Component* b : { (juce::Component*) &safe->input, (juce::Component*) &safe->output, (juce::Component*) &safe->buffer, (juce::Component*) &safe->sharing })
                b->setEnabled (true);
            safe->status = r.message;
            if (r.ok && ! r.fastest) safe->status << " The fastest way would not start on this input, so the next one is in use.";
            if (r.ok && safe->followed) safe->status << " Sound now comes out of " << safe->output.getText() << ".";
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

void AudioPicker::showExample()
{
    example = true;
    filling = true;
    for (auto* b : { &input, &output, &buffer }) b->clear (juce::dontSendNotification);
    input.addItem ("Microphone (High Definition Audio)", 1);
    input.setSelectedItemIndex (0, juce::dontSendNotification);
    output.addItem ("Headphones (High Definition Audio)", 1);
    output.setSelectedItemIndex (0, juce::dontSendNotification);
    buffer.addItem ("3.0 ms  (144 samples)", 1);
    buffer.setSelectedItemIndex (0, juce::dontSendNotification);
    isAsio = false;
    output.setVisible (true);
    sharing.setVisible (true);
    sharing.setToggleState (false, juce::dontSendNotification);
    filling = false;
    note = "Riffhouse has the sound card to itself, which gives the shortest delay. Other apps are silent while it is open.";
    status = "Running: 144 samples at 48.0 kHz, about 9.3 ms from string to ear.";
    resized();
    repaint();
}

void AudioPicker::resized()
{
    auto r = getLocalBounds().reduced (28, 24);
    r.removeFromTop (58); // heading and a line under it
    for (auto* b : { &input, &output, &buffer })
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
    g.drawText ("Input and output", r.removeFromTop (28), juce::Justification::centredLeft);
    g.setColour (colours::textDim);
    g.setFont (font (13.5f));
    g.drawText ("Every change is tried first. If no sound comes through, the last working choice comes back.", r.removeFromTop (22), juce::Justification::centredLeft);
    r.removeFromTop (8);

    const char* labels[] = { "Guitar in", "Sound out", "Delay" };
    juce::ComboBox* boxes[] = { &input, &output, &buffer };
    g.setFont (font (14.0f));
    for (int i = 0; i < 3; ++i)
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
    if (status.isNotEmpty())
    {
        g.setColour (statusIsProblem ? colours::warn : colours::text);
        g.setFont (font (14.0f, statusIsProblem ? Weight::semibold : Weight::regular));
        g.drawFittedText (status, r.removeFromTop (statusIsProblem || status.length() > 150 ? 56 : (status.length() > 76 ? 40 : 22)), juce::Justification::topLeft, 3, 1.0f);
        r.removeFromTop (6);
    }
    if (note.isNotEmpty())
    {
        g.setColour (colours::textDim);
        g.setFont (font (13.5f));
        g.drawFittedText (note, r, juce::Justification::topLeft, 4, 1.0f);
    }
}
} // namespace rh::ui
