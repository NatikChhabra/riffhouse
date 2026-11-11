// Riffhouse - free, open-source guitar rig
// Copyright (C) 2026 Riffhouse contributors. Licensed under AGPL-3.0-or-later.
#pragma once

#include "../engine/FootControl.h"
#include "../engine/History.h"
#include "../engine/Looper.h"
#include "../engine/Song.h"
#include "../engine/Presets.h"
#include "../engine/Rig.h"
#include "LevelMatcher.h"
#include "Recorder.h"
#include "Takes.h"
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
    bool acceptsMidi() const override { return true; } // foot controllers; no notes are played
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

    // Undo and redo of the whole tone (message thread). The window calls historyTick a few times
    // a second; see ToneHistory for when a change becomes a step.
    void historyTick (bool handsOn) { history.tick (toneNow(), handsOn); }
    bool canUndo() const { return history.canUndo (toneNow()); }
    bool canRedo() const noexcept { return history.canRedo(); }
    juce::String undo(); // what it put back, in words for the user; empty when there was nothing to undo
    juce::String redo();

    // Neural captures and IRs (message thread)
    // `select`: also make the capture the amp in use (not when a saved session is coming back)
    void loadCaptureAsync (const juce::File& file, std::function<void (juce::String)> done, bool select = true);
    void refreshLatency(); // tell the host when the rig's delay changed; message thread

    // What setup found for this player's input (level, Input Fix, gate, hum filter) is kept
    // in a small file, so that a new copy of the plugin in a DAW starts from it and not
    // from factory settings. The app writes it; plugins only read it, and a saved project
    // keeps its own values.
    static juce::File setupFile();
    void rememberSetup() const;
    bool recallSetup();
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
    double audioFileSeconds (const juce::File& f); // 0 when it cannot be read
    void setJamPlaying (bool shouldPlay);
    bool isJamPlaying() const;
    juce::String jamTrackName() const { return jamFile.getFileNameWithoutExtension(); }
    std::atomic<float> jamLevel { 0.7f };
    double jamPosition() const;
    double jamLength() const;
    void setJamPosition (double seconds);
    // Practising: slower or faster at the same pitch, another key at the same speed,
    // and one part of the song going round.
    void setJamSpeed (float timesNormal);
    float jamSpeed() const noexcept { return songStretch.speed(); }
    void setJamKey (int semitones);
    int jamKey() const noexcept { return songStretch.key(); }
    bool setJamSection (double fromSeconds, double toSeconds); // false when the part is too short or cannot be read
    void clearJamSection();
    bool jamHasSection() const noexcept { return songSection.active(); }
    double jamSectionStart() const noexcept { return songSection.startSeconds(); }
    double jamSectionEnd() const noexcept { return songSection.endSeconds(); }

    // Looper: play a part, let it go round, play over it. Your ears only: takes do not contain it.
    Looper& looper() noexcept { return loop; }
    void loopPress(); // the looper's one big button (message thread); takes its memory the first time

    // Foot control: a MIDI foot controller, expression pedal or keyboard works the rig.
    enum class FootTeach { off, touch, move }; // touch: waiting for a control in Riffhouse; move: waiting for the controller
    const FootControl& footControl() const noexcept { return foot; }
    FootTeach footTeaching() const noexcept { return footTeach; }
    void footTeachTouch();               // the next control you touch in Riffhouse, then the next controller that moves
    void footTeachJob (FootJob job);     // a job such as "Next tone", then the next controller that moves
    void footTeachStop();
    void footRemove (size_t index);
    void footSetPress (size_t index, bool press);
    void footTick();                     // message thread, a few times a second while teaching
    void footMoved (int controller, int value); // message thread: what the controller sent
    void footProgram (int number);       // program change: your starred tones in order, or all tones when none is starred
    void stepTone (int by);              // through the starred tones, or all tones when none is starred
    void tapTempo();
    std::function<void (const juce::String&)> onFootMessage; // something worth a line on screen
    std::function<bool (FootJob)> onFootJob;                  // tuner and record belong to the window, when there is one

    // Takes: listen to a recording, or feed its clean guitar back through the rig
    TakePlayer& takes() noexcept { return takePlayer; }
    ReampJob::Setup reampSetup() const; // the rig as it is set right now

    // Auto level: the trim it holds, whether it has heard the player yet, and a way to start over
    LevelMatcher& levels() noexcept { return matcher; }

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
    LevelMatcher matcher { rig, [this] (ParamValues& v) { for (int i = 0; i < kNumParams; ++i) v[(size_t) i] = raw[(size_t) i]->load(); } };
    PresetLibrary library;
    int currentPreset = -1;
    juce::String externalName;
    ParamValues presetSnapshot {};

    ToneState toneNow() const;
    juce::String restoreTone (const ToneState& to);
    ToneHistory history;

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
    juce::TimeSliceThread disk { "Riffhouse song reader" }; // keeps file reading off the audio thread
    TakePlayer takePlayer;
    std::unique_ptr<juce::AudioFormatReaderSource> jamReader;
    juce::AudioTransportSource jam;
    juce::File jamFile;
    juce::AudioBuffer<float> jamBuffer, jamOut;
    SongStretch songStretch;
    SongSection songSection;
    juce::CriticalSection jamLock;
    FootControl foot;
    FootTeach footTeach = FootTeach::off;
    ParamValues footBefore {};
    void footSave();
    void footDo (const FootControl::Action&);
    std::vector<int> toneSteps() const;
    std::vector<double> taps;
    // what arrives on the audio thread waits here for the message thread
    struct MidiNote { int kind, number, value; };
    std::array<MidiNote, 256> midiQueue {};
    juce::AbstractFifo midiFifo { 256 };
    std::atomic<bool> writeTake { false };
    Looper loop;
    juce::SpinLock loopLock; // held while the loop's memory is (re)made; the audio thread only ever tries it
    bool loopReady = false;

    std::vector<float> mono, di, monoOut;
    std::atomic<int> latencyAmp { -1 };
    double sampleRate = 48000.0;
    int blockSize = 512;
    std::atomic<float> channelPick { 0.5f };
    float chanEnergy[2] {};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RiffhouseProcessor)
};
} // namespace rh
