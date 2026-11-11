// Riffhouse - free, open-source guitar rig
// Copyright (C) 2026 Riffhouse contributors. Licensed under AGPL-3.0-or-later.
#include "AudioPicker.h"

#include "../plugin/AutoSetup.h"
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
    for (auto* b : { &type, &input, &output, &buffer })
    {
        addAndMakeVisible (*b);
        b->setTextWhenNothingSelected ("Nothing found");
        b->setTextWhenNoChoicesAvailable ("Nothing found");
    }
    buffer.setTextWhenNothingSelected ("Set by the driver");
    type.onChange = [this] { if (! filling) typePicked(); };
    input.onChange = [this] { if (! filling) apply(); };
    output.onChange = [this] { if (! filling) apply(); };
    buffer.onChange = [this] { if (! filling) apply(); };
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

juce::String AudioPicker::pickedType() const
{
    return typeNames[type.getSelectedItemIndex()];
}

void AudioPicker::refresh()
{
    auto* host = StandaloneHost::get();
    if (host == nullptr || example) return;
    auto& dm = host->deviceManager();
    filling = true;
    type.clear (juce::dontSendNotification);
    typeNames.clear();
    for (auto* t : dm.getAvailableDeviceTypes())
    {
        typeNames.add (t->getTypeName());
        type.addItem (AudioSwitch::friendlyType (t->getTypeName()), typeNames.size());
    }
    type.setSelectedItemIndex (typeNames.indexOf (dm.getCurrentAudioDeviceType()), juce::dontSendNotification);
    filling = false;
    fillDevices();
    if (status.isEmpty()) status = AudioSwitch::running (dm, proc);
    repaint();
}

void AudioPicker::fillDevices()
{
    auto* host = StandaloneHost::get();
    if (host == nullptr) return;
    auto& dm = host->deviceManager();
    const auto picked = pickedType();
    isAsio = picked == "ASIO";
    auto* t = findType (dm, picked);
    const bool current = picked == dm.getCurrentAudioDeviceType();
    juce::AudioDeviceManager::AudioDeviceSetup s;
    dm.getAudioDeviceSetup (s);
    auto* dev = dm.getCurrentAudioDevice();

    filling = true;
    input.clear (juce::dontSendNotification);
    output.clear (juce::dontSendNotification);
    buffer.clear (juce::dontSendNotification);
    bufferSizes.clear();
    if (t != nullptr)
    {
        if (! current) t->scanForDevices();
        const auto ins = t->getDeviceNames (true), outs = t->getDeviceNames (false);
        input.addItemList (ins, 1);
        output.addItemList (outs, 1);
        int in = current ? ins.indexOf (s.inputDeviceName) : ins.indexOf (s.inputDeviceName);
        if (in < 0) in = t->getDefaultDeviceIndex (true);
        int out = outs.indexOf (s.outputDeviceName);
        if (out < 0) out = t->getDefaultDeviceIndex (false);
        if (! ins.isEmpty()) input.setSelectedItemIndex (juce::jlimit (0, ins.size() - 1, in), juce::dontSendNotification);
        if (! outs.isEmpty()) output.setSelectedItemIndex (juce::jlimit (0, outs.size() - 1, out), juce::dontSendNotification);
    }
    if (current && dev != nullptr)
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
    filling = false;

    note = {};
    if (isAsio && input.getNumItems() == 0)
        note = "No ASIO driver is installed on this computer. ASIO drivers come with audio interfaces. "
               "Without an interface, Windows Audio in exclusive mode is the fastest there is, and setup already picks it.";
    else if (isAsio && AutoSetup::isAsioWrapper (input.getText()))
        note = input.getText() + " is a stand-in driver, not an audio interface's own. It rarely beats exclusive mode.";
    else if (picked == "Windows Audio (Exclusive Mode)")
        note = "Exclusive mode: the lowest delay without an interface. Other apps' sound pauses while Riffhouse is open.";
    else if (picked == "DirectSound")
        note = "DirectSound adds a delay you can feel. Use it only if nothing else starts.";

    output.setVisible (! isAsio);
    panel.setVisible (current && dev != nullptr && dev->hasControlPanel());
    resized();
    repaint();
}

void AudioPicker::typePicked()
{
    fillDevices();
    if (input.getNumItems() == 0)
    {
        // nothing to open under this driver type: say so (the note does) and leave what is running alone
        status = {};
        statusIsProblem = false;
        return;
    }
    apply();
}

void AudioPicker::apply()
{
    auto* host = StandaloneHost::get();
    if (host == nullptr || changer == nullptr || changer->busy()) return;
    AudioChoice c;
    c.type = pickedType();
    c.input = input.getText();
    c.output = output.getText();
    // a buffer size belongs to the device it was listed for
    const bool sameDevice = c.type == host->deviceManager().getCurrentAudioDeviceType();
    c.bufferSize = sameDevice && buffer.getSelectedItemIndex() >= 0 ? bufferSizes[buffer.getSelectedItemIndex()] : 0;
    if (c.type.isEmpty() || c.input.isEmpty()) return;

    status = "Trying " + c.input + "...";
    statusIsProblem = false;
    for (auto* b : { &type, &input, &output, &buffer }) b->setEnabled (false);
    repaint();
    host->setupRunning (true); // whoever guards the connection stands aside while it is changed on purpose
    changer->apply (c, [safe = juce::Component::SafePointer<AudioPicker> (this)] (AudioSwitch::Result r)
    {
        if (auto* h = StandaloneHost::get())
        {
            if (r.ok) h->saveAudioState(); // this is now the connection to keep alive
            h->setupRunning (false);
        }
        if (safe == nullptr) return;
        for (auto* b : { &safe->type, &safe->input, &safe->output, &safe->buffer }) b->setEnabled (true);
        safe->status = r.message;
        safe->statusIsProblem = ! r.ok;
        safe->refresh();
    });
}

void AudioPicker::showExample()
{
    example = true;
    filling = true;
    for (auto* b : { &type, &input, &output, &buffer }) b->clear (juce::dontSendNotification);
    type.addItem (AudioSwitch::friendlyType ("ASIO"), 1);
    type.setSelectedItemIndex (0, juce::dontSendNotification);
    isAsio = true;
    output.setVisible (false);
    filling = false;
    note = "No ASIO driver is installed on this computer. ASIO drivers come with audio interfaces. "
           "Without an interface, Windows Audio in exclusive mode is the fastest there is, and setup already picks it.";
    status = "Running: 144 samples at 48.0 kHz, about 9.3 ms from string to ear.";
    resized();
    repaint();
}

void AudioPicker::resized()
{
    auto r = getLocalBounds().reduced (28, 24);
    r.removeFromTop (58); // heading and a line under it
    for (auto* b : { &type, &input, &output, &buffer })
    {
        if (! b->isVisible()) continue;
        b->setBounds (r.removeFromTop (kRow).withTrimmedLeft (kLabel));
        r.removeFromTop (kGap);
    }
    if (panel.isVisible()) panel.setBounds (r.removeFromTop (kRow).withTrimmedLeft (kLabel).removeFromLeft (260));
}

void AudioPicker::paint (juce::Graphics& g)
{
    g.fillAll (colours::panel);
    auto r = getLocalBounds().reduced (28, 24);
    g.setColour (colours::text);
    g.setFont (font (20.0f, Weight::display));
    g.drawText ("Audio by hand", r.removeFromTop (28), juce::Justification::centredLeft);
    g.setColour (colours::textDim);
    g.setFont (font (13.5f));
    g.drawText ("Every change is tried first. If no sound comes through, the last working choice comes back.", r.removeFromTop (22), juce::Justification::centredLeft);
    r.removeFromTop (8);

    const char* labels[] = { "Driver", isAsio ? "ASIO driver" : "Input", "Output", "Buffer" };
    juce::ComboBox* boxes[] = { &type, &input, &output, &buffer };
    g.setFont (font (14.0f));
    for (int i = 0; i < 4; ++i)
    {
        if (! boxes[i]->isVisible()) continue;
        g.setColour (colours::textDim);
        g.drawText (labels[i], r.removeFromTop (kRow).removeFromLeft (kLabel), juce::Justification::centredLeft);
        r.removeFromTop (kGap);
    }
    if (panel.isVisible()) { r.removeFromTop (kRow); r.removeFromTop (kGap); }

    // what happened, then the standing remark about this driver type
    g.setColour (juce::Colours::white.withAlpha (0.10f));
    g.fillRect (r.removeFromTop (1));
    r.removeFromTop (10);
    if (status.isNotEmpty())
    {
        g.setColour (statusIsProblem ? colours::warn : colours::text);
        g.setFont (font (14.0f, statusIsProblem ? Weight::semibold : Weight::regular));
        g.drawFittedText (status, r.removeFromTop (statusIsProblem ? 56 : 22), juce::Justification::topLeft, 3, 1.0f);
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
