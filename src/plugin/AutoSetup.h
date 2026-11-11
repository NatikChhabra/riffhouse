// Riffhouse - free, open-source guitar rig
// Copyright (C) 2026 Riffhouse contributors. Licensed under AGPL-3.0-or-later.
#pragma once

#include "PluginProcessor.h"
#include <juce_audio_devices/juce_audio_devices.h>

namespace rh
{
// Finds the guitar and sets up audio with no questions asked:
//  1. listen  - cycles through every input while you strum and keeps the loudest
//  2. tune    - tries the fastest driver modes and the smallest buffers that run
//               without dropouts on this PC
//  3. quiet   - you mute the strings; it measures the noise and sets the input
//               level and gate from what it heard
// It only touches the device manager from the message thread.
class AutoSetup : private juce::Timer
{
public:
    enum class Phase { idle, listening, tuning, quiet, finished, noSignal };

    AutoSetup (juce::AudioDeviceManager&, RiffhouseProcessor&);
    ~AutoSetup() override;

    void start();
    void cancel();
    Phase phase() const noexcept { return currentPhase; }
    float progress() const noexcept { return phaseProgress; }
    juce::String detail() const { return detailText; }
    float liveLevel() const noexcept { return level; } // linear peak of the raw input

    struct Summary
    {
        juce::String deviceType, inputName, outputName;
        int bufferSize = 0;
        double sampleRate = 0;
        double latencyMs = 0;
        float inputGainDb = 0, gateDb = -62, guitarPeakDb = -100, noiseDb = -100;
        bool inputFix = false, clipping = false, exclusive = false;
        float inputFixDb = 0, tiltDb = 0; // how much treble was restored, and the measured tone
        double toneSeconds = 0;           // how much playing the tone measurement heard
    };
    const Summary& summary() const noexcept { return result; }

    std::function<void()> onPhaseChanged;

    static bool looksLikeOnboardInput (const juce::String& inputName, const juce::String& typeName);
    static bool isAsioWrapper (const juce::String& driverName);

private:
    struct Candidate { juce::String type, input; };
    struct Mode { juce::String type; };

    void timerCallback() override;
    void setPhase (Phase);
    bool openCandidate (const Candidate&);
    void buildCandidates();
    void beginTuning();
    bool openMode (const juce::String& type, int bufferSize);
    void finishTuning();
    void beginQuiet();
    void finish();
    double measuredLatencyMs() const;

    juce::AudioDeviceManager& dm;
    RiffhouseProcessor& proc;
    Phase currentPhase = Phase::idle;
    float phaseProgress = 0.0f, level = 0.0f;
    juce::String detailText;
    Summary result;

    std::unique_ptr<juce::XmlElement> originalState;
    juce::String originalType;

    // listening
    std::vector<Candidate> candidates;
    std::vector<float> candidatePeak;
    int candidateIndex = 0, pass = 0;
    double dwellStart = 0;
    float dwellPeak = 0.0f;
    Candidate chosen;
    float chosenPeak = 0.0f;

    // tuning
    juce::StringArray modes;
    int modeIndex = 0;
    juce::Array<int> buffers;
    int bufferIndex = 0;
    int xrunStart = 0;
    float tunePeak = 0.0f;
    double cpuMax = 0.0;
    bool lastOpenFailed = false;
    struct Best { juce::String type; int buffer = 0; double latency = 1e9; double rate = 0; } best;
    float strumPeak = 0.0f; // the loudest playing heard on the chosen input, across listening and tuning

    // quiet
    float noisePeak = 0.0f;
    double phaseStart = 0;
    juce::Array<float> noiseSamples;
};
} // namespace rh
