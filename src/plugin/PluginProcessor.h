// Riffhouse - free, open-source guitar rig
// Copyright (C) 2026 Riffhouse contributors. Licensed under AGPL-3.0-or-later.
#pragma once

#include "../engine/Presets.h"
#include "../engine/Rig.h"
#include "Recorder.h"
#include "Tuner.h"

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>

namespace rh
{
class RiffhouseProcessor : public juce::AudioProcessor,
                           private juce::AsyncUpdater
{
public:
    RiffhouseProcessor();
    ~RiffhouseProcessor() override;

    // AudioProcessor
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "Riffhouse"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 8.0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}
    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    // Parameters
    juce::AudioProcessorValueTreeState& state() noexcept { return apvts; }
    juce::RangedAudioParameter* param (P id) const noexcept { return params[(size_t) id]; }
    float value (P id) const noexcept { return raw[(size_t) id]->load(); }
    void setValue (P id, float v);

    // Presets (message thread)
    PresetLibrary& presets() noexcept { return library; }
    void reloadPresets();
    void loadPreset (int index);
    int currentPresetIndex() const noexcept { return currentPreset; }
    juce::String currentPresetName() const;
    bool isModified() const;
    juce::File savePreset (const juce::String& name, const juce::String& category);
    Preset captureCurrent (const juce::String& name, const juce::String& category) const;
    void applyPreset (const Preset& p);
    void applySharedPreset (const Preset& p); // a tone from outside the library (pasted code)
    void toggleFavourite (int index);
    static juce::File userFolder();

    // Neural captures and IRs (message thread)
    void loadCaptureAsync (const juce::File& file, std::function<void (juce::String)> done);
    juce::String captureName() const { return captureFile.getFileNameWithoutExtension(); }
    bool loadUserIr (const juce::File& file);
    juce::String userIrName() const { return irFile.getFileNameWithoutExtension(); }

    // Tools
    Recorder& recorder() noexcept { return rec; }
    bool startRecording (bool countIn);
    void stopRecording();
    bool isCountingIn() const noexcept { return countInRemaining.load() > 0; }
    Tuner& tuner() noexcept { return tun; }
    std::atomic<bool> tunerOpen { false }, tunerMute { true };
    std::atomic<bool> metronomeOn { false };
    std::atomic<float> metronomeBpm { 100.0f }, metronomeLevel { 0.5f };
    std::atomic<bool> saveDi { true };
    juce::File recordingFolder;

    // Jam track (play along with a song)
    bool loadJamTrack (const juce::File& f);
    void setJamPlaying (bool shouldPlay);
    bool isJamPlaying() const;
    juce::String jamTrackName() const { return jamFile.getFileNameWithoutExtension(); }
    std::atomic<float> jamLevel { 0.7f };
    double jamPosition() const;
    double jamLength() const;
    void setJamPosition (double seconds);

    RigMeters& meters() noexcept { return rig.meters(); }
    TiltProbe& tiltProbe() noexcept { return rig.tiltProbe(); }
    double currentSampleRate() const noexcept { return sampleRate; }
    int currentBlockSize() const noexcept { return blockSize; }
    float inputChannelBalance() const noexcept { return channelPick.load(); }

    // Auto setup: peak level of the raw input before any processing.
    std::atomic<float> rawInputPeak { 0.0f };
    std::atomic<bool> muteOutput { false }; // silent while the setup wizard probes devices
    std::atomic<juce::uint32> blockCounter { 0 }; // ticks once per audio block: proof that the device is alive

    // UI state that should survive closing the window
    juce::ValueTree uiState { "UI" };

private:
    void handleAsyncUpdate() override;
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    void renderClick (float* l, float* r, int n);

    juce::AudioProcessorValueTreeState apvts;
    std::array<juce::RangedAudioParameter*, kNumParams> params {};
    std::array<std::atomic<float>*, kNumParams> raw {};

    Rig rig;
    PresetLibrary library;
    int currentPreset = -1;
    juce::String externalName;
    ParamValues presetSnapshot {};

    Recorder rec;
    Tuner tun;
    std::atomic<int> countInRemaining { 0 };
    std::atomic<bool> startAfterCountIn { false };
    double clickPhase = 0.0;
    double samplesPerBeat = 24000.0, beatCounter = 0.0;
    int beatIndex = 0;
    float clickEnv = 0.0f, clickFreq = 1000.0f;

    juce::File captureFile, irFile;
    std::shared_ptr<std::atomic<bool>> alive = std::make_shared<std::atomic<bool>> (true);
    juce::ThreadPool loader { juce::ThreadPoolOptions{}.withThreadName ("Riffhouse loader").withNumberOfThreads (1) };

    juce::AudioFormatManager formats;
    std::unique_ptr<juce::AudioFormatReaderSource> jamReader;
    juce::AudioTransportSource jam;
    juce::File jamFile;
    juce::AudioBuffer<float> jamBuffer;
    juce::CriticalSection jamLock;

    std::vector<float> mono, di;
    double sampleRate = 48000.0;
    int blockSize = 512;
    std::atomic<float> channelPick { 0.5f };
    float chanEnergy[2] {};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RiffhouseProcessor)
};
} // namespace rh
