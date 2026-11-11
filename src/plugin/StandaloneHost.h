// Riffhouse - free, open-source guitar rig
// Copyright (C) 2026 Riffhouse contributors. Licensed under AGPL-3.0-or-later.
#pragma once

#include <juce_audio_devices/juce_audio_devices.h>
#include <juce_data_structures/juce_data_structures.h>

namespace rh
{
class Updater;

// Implemented by the standalone app so the editor can show audio settings
// and run the automatic setup. Null when running as a plugin in a DAW.
class StandaloneHost
{
public:
    virtual ~StandaloneHost() = default;
    virtual juce::AudioDeviceManager& deviceManager() = 0;
    virtual void saveAudioState() = 0;
    virtual juce::PropertiesFile& settings() = 0;

    // Something about the audio connection the player should know: the input was unplugged,
    // audio is being restarted, Bluetooth headphones are in use. Empty text means all is well.
    struct Notice { juce::String text, action; bool warning = false; };
    virtual Notice audioNotice() const { return {}; }
    virtual void audioNoticeAction() {}
    virtual void setupRunning (bool) {} // the setup wizard is switching devices itself
    virtual Updater* updater() { return nullptr; }
    virtual void restartToUpdate() {}

    static StandaloneHost* get() noexcept { return instance; }

protected:
    inline static StandaloneHost* instance = nullptr;
};
} // namespace rh
