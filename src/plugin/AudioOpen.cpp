// Riffhouse - free, open-source guitar rig
// Copyright (C) 2026 Riffhouse contributors. Licensed under AGPL-3.0-or-later.
#include "AudioOpen.h"

#include "DeviceLog.h"

namespace rh
{
AudioOpen AudioOpen::running (juce::AudioDeviceManager& dm)
{
    auto* dev = dm.getCurrentAudioDevice();
    if (dev == nullptr) return {};
    juce::AudioDeviceManager::AudioDeviceSetup s;
    dm.getAudioDeviceSetup (s);
    AudioOpen o { dm.getCurrentAudioDeviceType(), s.inputDeviceName, s.outputDeviceName, dev->getCurrentBufferSizeSamples(), dev->getCurrentSampleRate() };
    if (o.type == "ASIO" && ! s.useDefaultOutputChannels && s.outputChannels.getHighestBit() >= 0)
        o.outputPair = s.outputChannels.findNextSetBit (0) / 2;
    return o;
}

juce::String openExactly (juce::AudioDeviceManager& dm, const AudioOpen& o)
{
    devlog::Timed timed ("Open " + o.type + ", in: " + o.input + ", out: " + o.output
                         + (o.bufferSize > 0 ? ", " + juce::String (o.bufferSize) + " samples" : juce::String()));
    if (! o.valid())
    {
        timed.result ("nothing to open");
        return "Nothing to open";
    }
    // foot controllers stay connected through the change
    juce::StringArray midiOn;
    for (const auto& d : juce::MidiInput::getAvailableDevices())
        if (dm.isMidiInputDeviceEnabled (d.identifier)) midiOn.add (d.identifier);

    juce::String err;
    if (dm.getCurrentAudioDeviceType() == o.type)
    {
        juce::AudioDeviceManager::AudioDeviceSetup s;
        dm.getAudioDeviceSetup (s);
        s.inputDeviceName = o.input;
        s.outputDeviceName = o.output;
        s.useDefaultInputChannels = false;
        s.inputChannels.clear();
        s.inputChannels.setRange (0, 2, true);
        s.useDefaultOutputChannels = o.outputPair < 0;
        s.outputChannels.clear();
        if (o.outputPair >= 0) s.outputChannels.setRange (2 * o.outputPair, 2, true);
        s.bufferSize = o.bufferSize;
        s.sampleRate = o.sampleRate;
        err = dm.setAudioDeviceSetup (s, true);
    }
    else
    {
        // another driver type: close what runs, then open the new choice straight away
        dm.closeAudioDevice();
        juce::XmlElement xml ("DEVICESETUP");
        xml.setAttribute ("deviceType", o.type);
        xml.setAttribute ("audioOutputDeviceName", o.output);
        xml.setAttribute ("audioInputDeviceName", o.input);
        if (o.sampleRate > 0) xml.setAttribute ("audioDeviceRate", o.sampleRate);
        if (o.bufferSize > 0) xml.setAttribute ("audioDeviceBufferSize", o.bufferSize);
        xml.setAttribute ("audioDeviceInChans", "11");
        if (o.outputPair >= 0)
        {
            juce::BigInteger outs;
            outs.setRange (2 * o.outputPair, 2, true);
            xml.setAttribute ("audioDeviceOutChans", outs.toString (2));
        }
        err = dm.initialise (2, 2, &xml, false);
    }
    auto* dev = dm.getCurrentAudioDevice();
    if (err.isEmpty() && dev == nullptr) err = "The device did not open";
    for (const auto& id : midiOn)
        if (! dm.isMidiInputDeviceEnabled (id)) dm.setMidiInputDeviceEnabled (id, true);
    timed.result (err.isEmpty() ? "ok, " + juce::String (dev->getCurrentBufferSizeSamples()) + " samples at "
                                      + juce::String (dev->getCurrentSampleRate(), 0) + " Hz"
                                : "failed: " + err.trim().upToFirstOccurrenceOf ("\n", false, false));
    return err;
}
} // namespace rh
