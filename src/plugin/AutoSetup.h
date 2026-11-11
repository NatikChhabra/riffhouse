// Riffhouse - free, open-source guitar rig
// Copyright (C) 2026 Riffhouse contributors. Licensed under AGPL-3.0-or-later.
#pragma once

#include "PluginProcessor.h"
#include "AudioSwitch.h"
#include <juce_audio_devices/juce_audio_devices.h>

namespace rh
{
// Finds the guitar and sets up audio with no questions asked:
//  1. listen  - cycles through every input while you strum and keeps the loudest
//  2. tune    - tries the fastest driver modes and the smallest buffers that run
//               without dropouts on this PC
//  3. play    - you play as hard as you ever will for a few seconds; the input level
//               is set from that and not from the first careful strums, the treble
//               your input loses is measured, and those seconds become the playing
//               every tone is levelled with
//  4. quiet   - you mute the strings; it waits until it hears them stop, measures
//               the noise as it is with the level and Input Fix now on, and sets the gate
// Between steps it stops for a moment and says what it found, so nobody is rushed along.
// It only touches the device manager from the message thread.
class AutoSetup : private juce::Timer
{
public:
    enum class Phase { idle, listening, tuning, playing, quiet, finished, noSignal };
    static constexpr int kSetupVersion = 2; // what a finished setup is stamped with; older ones are offered a new run

    AutoSetup (juce::AudioDeviceManager&, RiffhouseProcessor&);
    ~AutoSetup() override;

    // Every input a guitar could be on, in the order setup listens to them.
    struct Input { juce::String type, name; };
    static std::vector<Input> inputs (juce::AudioDeviceManager&);
    // `only`: the player said where the guitar is, so only that input is listened to.
    void start (const Input* only = nullptr);
    void cancel();
    bool listeningOnOne() const noexcept { return candidates.size() == 1; }
    Phase phase() const noexcept { return currentPhase; }
    float progress() const noexcept { return phaseProgress; }
    juce::String detail() const { return detailText; }
    float liveLevel() const noexcept { return level; } // linear peak of the raw input
    // A step has just finished: what it found, shown for a moment before the next one starts.
    bool pausing() const noexcept { return afterPause != nullptr; }
    juce::String found() const { return foundText; }
    // In the quiet step: still waiting for the strings to stop, or measuring already.
    bool measuringNoise() const noexcept { return noiseStarted; }
    float hardestPeak() const noexcept { return playPeak; } // in the playing step, linear

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
        double playedSeconds = 0;         // how much hard playing the level was taken from
        bool fixHeldBack = false;         // the full fix would have brought too much hiss up with it
        bool outputFollows = false;       // the input is an interface, and the sound was put on its output too
    };
    const Summary& summary() const noexcept { return result; }

    std::function<void()> onPhaseChanged;

    static bool looksLikeOnboardInput (const juce::String& inputName, const juce::String& typeName);
    static bool isAsioWrapper (const juce::String& driverName);

private:
    using Candidate = Input;
    struct Mode { juce::String type; };

    void timerCallback() override;
    void setPhase (Phase);
    bool openCandidate (const Candidate&);
    void buildCandidates();
    static std::vector<Candidate> findCandidates (juce::AudioDeviceManager&);
    void beginTuning();
    bool openMode (const juce::String& type, int bufferSize);
    void finishTuning();
    void pauseThen (const juce::String& found, std::function<void()> next);
    void beginPlaying();
    void finishPlaying();
    void beginQuiet();
    void startNoise();
    void finish();
    double measuredLatencyMs() const;

    juce::AudioDeviceManager& dm;
    RiffhouseProcessor& proc;
    Phase currentPhase = Phase::idle;
    float phaseProgress = 0.0f, level = 0.0f;
    juce::String detailText, foundText;
    std::function<void()> afterPause;
    double pauseUntil = 0;
    Summary result;

    AudioOpen original; // what was running before setup, to return to on Cancel

    // listening
    std::vector<Candidate> candidates;
    std::vector<float> candidatePeak;
    int candidateIndex = 0, pass = 0;
    double dwellStart = 0;
    float dwellPeak = 0.0f;
    Candidate chosen;
    float chosenPeak = 0.0f;
    juce::String followOutput; // the chosen interface's own output, while it opens
    juce::String outputFor (const Candidate&, juce::AudioIODeviceType*, const juce::String& current) const;

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

    // playing
    float playPeak = 0.0f, fixedPeak = 0.0f; // the hardest playing: off the cable, and after Input Fix
    double playedMs = 0, lastTick = 0, fixSetAt = 0;
    bool fixSet = false;
    void setFixFromTone();

    // quiet
    double phaseStart = 0, quietSince = 0, noiseStart = 0;
    bool noiseStarted = false, fixRetried = false;
    juce::Array<float> noiseSamples;
};
} // namespace rh
