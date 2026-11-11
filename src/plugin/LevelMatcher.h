// Riffhouse - free, open-source guitar rig
// Copyright (C) 2026 Riffhouse contributors. Licensed under AGPL-3.0-or-later.
#pragma once

#include "../engine/LevelMatch.h"

#include <juce_core/juce_core.h>

#include <functional>
#include <unordered_map>

namespace rh
{
// Auto level at work. A thread with a rig of its own watches the tone on stage; when it
// changes, the thread plays the new tone the phrase off stage, measures it and sets the
// trim on the stage rig (see LevelMatch.h for the rule). Between changes it measures the
// tones the player is likely to pick next, so that most switches find their answer waiting.
//
// It also listens for the player's own phrase (PhraseCatcher) and keeps it between sessions.
// That is the standalone app's job. Inside a DAW the plugin uses the phrase the app saved
// (or the house phrase) and learns nothing: whatever is on the track, a project must render
// the same file tomorrow as today. Several plugins in one project share what they measured,
// and only one of them measures ahead at a time.
class LevelMatcher : private juce::Thread
{
public:
    LevelMatcher (Rig& stage, std::function<void (ParamValues&)> readParams);
    ~LevelMatcher() override;

    // Audio is about to run at this rate (prepareToPlay). Starts the thread the first time.
    void start (double sampleRate);

    void rematch() noexcept { rematchFlag.store (true); notify(); }          // a tone was loaded
    void restore (float trimDb);                                             // a saved session comes back
    void setFiles (const juce::File& capture, const juce::File& userIr);     // what the amp and cab slots hold
    void measureAhead (std::vector<ParamValues> tones);                      // e.g. every preset
    // Setup just had the player play their loudest: take that as their phrase now.
    void learnPhraseNow() noexcept { learnNow.store (true); notify(); }
    void forgetPhrase() noexcept { forget.store (true); notify(); }
    // Whether playing heard on stage may become the phrase, and be saved. Before start().
    void setLearns (bool l) noexcept { learns = l; }

    // Offline renders cannot wait for a thread: the tone is measured where it is asked for.
    void matchNow (const ParamValues& tone);

    PhraseCatcher& catcher() noexcept { return ears; }
    float trimDb() const noexcept { return trimNow.load(); }
    bool hasPlayersPhrase() const noexcept { return playersPhrase.load(); }
    bool measuring() const noexcept { return working.load(); }
    static int tonesKnown();

    static juce::File phraseFile();

private:
    void run() override;
    void step (Rig& meter, const ParamValues& now, bool wait);
    float loudnessOf (Rig& meter, const ParamValues& measuringTone);
    void listenForPhrase (float inGainDb);
    void adopt (LevelPhrase p, bool save);

    Rig& stage;
    std::function<void (ParamValues&)> readParams;
    PhraseCatcher ears;

    juce::CriticalSection lock; // one measurement at a time: the thread's, or an offline render's
    std::unique_ptr<Rig> meter;
    LevelKeeper keeper;
    LevelPhrase phrase, phraseAtRate;
    ParamValues measuredTone {}, lastSeen {};
    bool have = false, wasOn = false, measuredSomething = false, phraseNew = false;
    juce::uint32 lastMove = 0, lastListen = 0, adoptedAt = 0;
    juce::File capture, userIr;
    juce::uint64 filesKey = 0;

    juce::CriticalSection earsLock; // the catcher is made anew when the sample rate changes
    bool learns = true;

    juce::CriticalSection queueLock;
    std::vector<ParamValues> ahead;
    size_t aheadAt = 0;
    juce::File wantCapture, wantIr;
    float wantTrim = 0.0f;

    std::atomic<double> rate { 0.0 };
    std::atomic<bool> rematchFlag { false }, restoreFlag { false }, filesFlag { false }, learnNow { false }, forget { false };
    std::atomic<bool> playersPhrase { false }, working { false };
    std::atomic<float> trimNow { 0.0f };
};
} // namespace rh
