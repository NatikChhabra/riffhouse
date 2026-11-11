// Riffhouse - free, open-source guitar rig
// Copyright (C) 2026 Riffhouse contributors. Licensed under AGPL-3.0-or-later.
#include "LevelMatcher.h"

#include "../engine/History.h"

namespace rh
{
namespace
{
    constexpr int kBlock = 256;
    constexpr juce::uint32 kKnobRestMs = 160;        // a knob is measured once the hand has left it
    constexpr juce::uint32 kListenEveryMs = 500;
    constexpr juce::uint32 kPhraseKeptMs = 20000;    // a new phrase is not replaced again at once
    constexpr float kLouderByDb = 3.0f;              // how much harder the player must dig in to replace it

    // What every matcher in this process has measured: the same tone, phrase, files and
    // sample rate read the same wherever they are measured.
    struct Shared
    {
        juce::CriticalSection lock;
        std::unordered_map<juce::uint64, float> readings;
        juce::CriticalSection aheadTurn; // one matcher measures ahead at a time
    };
    Shared& shared()
    {
        static Shared s;
        return s;
    }
}

int LevelMatcher::tonesKnown()
{
    const juce::ScopedLock sl (shared().lock);
    return (int) shared().readings.size();
}

juce::File LevelMatcher::phraseFile()
{
    return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
        .getChildFile ("Riffhouse").getChildFile ("your-playing.rhp");
}

LevelMatcher::LevelMatcher (Rig& s, std::function<void (ParamValues&)> r)
    : juce::Thread ("Riffhouse auto level"), stage (s), readParams (std::move (r))
{
    stage.setInputTap (&ears);
}

LevelMatcher::~LevelMatcher()
{
    stopThread (4000);
    stage.setInputTap (nullptr);
}

void LevelMatcher::start (double sampleRate)
{
    {
        const juce::ScopedLock sl (earsLock);
        ears.prepare (sampleRate);
    }
    rate.store (sampleRate);
    if (! isThreadRunning()) startThread (juce::Thread::Priority::normal);
    notify();
}

void LevelMatcher::restore (float trimDb)
{
    {
        const juce::ScopedLock sl (queueLock);
        wantTrim = trimDb;
    }
    // heard at once: a session must not come back at the wrong volume for the time a measurement takes
    stage.setLevelTrimDb (trimDb);
    trimNow.store (trimDb);
    restoreFlag.store (true);
    notify();
}

void LevelMatcher::setFiles (const juce::File& c, const juce::File& ir)
{
    {
        const juce::ScopedLock sl (queueLock);
        wantCapture = c;
        wantIr = ir;
    }
    filesFlag.store (true);
    notify();
}

void LevelMatcher::measureAhead (std::vector<ParamValues> tones)
{
    const juce::ScopedLock sl (queueLock);
    ahead = std::move (tones);
    aheadAt = 0;
}

void LevelMatcher::adopt (LevelPhrase p, bool save)
{
    phrase = std::move (p);
    phraseAtRate = {};
    {
        const juce::ScopedLock sl (queueLock);
        aheadAt = 0; // everything measured ahead was measured with the old phrase
    }
    playersPhrase.store (phrase.fromPlayer);
    adoptedAt = juce::Time::getMillisecondCounter();
    phraseNew = true;
    if (save && phrase.fromPlayer)
    {
        const auto f = phraseFile();
        f.getParentDirectory().createDirectory();
        const auto bytes = phraseToBytes (phrase);
        f.replaceWithData (bytes.getData(), bytes.getSize());
    }
}

void LevelMatcher::listenForPhrase (float inGainDb)
{
    const bool now = learnNow.exchange (false);
    if (! learns) return;
    const auto t = juce::Time::getMillisecondCounter();
    if (! now && t - lastListen < kListenEveryMs) return;
    lastListen = t;
    std::vector<float> heard;
    float loud = -120.0f;
    double heardRate = 0.0;
    {
        const juce::ScopedLock sl (earsLock);
        if (! ears.latest (heard)) return;
        heardRate = ears.rate();
    }
    if (! worthKeeping (heard, heardRate, loud, inGainDb)) return;
    // The first playing heard becomes the phrase. After that only playing clearly harder
    // than it replaces it, so the phrase settles on the player digging in and stays there.
    if (! now && phrase.fromPlayer && (loud < phrase.loudDb + kLouderByDb || t - adoptedAt < kPhraseKeptMs)) return;
    LevelPhrase p;
    p.samples = std::move (heard);
    p.sampleRate = heardRate;
    p.loudDb = loud;
    p.fromPlayer = true;
    p.id = (juce::uint64) juce::Time::currentTimeMillis() * 1000003ull + (juce::uint64) p.samples.size();
    adopt (std::move (p), true);
    if (now) { keeper.rematch(); have = false; } // setup asked: the tone on stage is levelled with it straight away
}

float LevelMatcher::loudnessOf (Rig& m, const ParamValues& tone)
{
    const juce::uint64 key = toneHash (tone) ^ (phraseAtRate.id * 0x9E3779B97F4A7C15ull) ^ filesKey
                             ^ ((juce::uint64) std::llround (phraseAtRate.sampleRate) * 0xD6E8FEB86659FD93ull);
    auto& all = shared();
    {
        const juce::ScopedLock sl (all.lock);
        if (const auto it = all.readings.find (key); it != all.readings.end()) return it->second;
    }
    working.store (true);
    measuredSomething = true;
    const float loud = measureLoudnessDb (m, tone, phraseAtRate);
    working.store (false);
    const juce::ScopedLock sl (all.lock);
    if (all.readings.size() > 50000) all.readings.clear(); // hours of knob turning: start over rather than grow for ever
    all.readings[key] = loud;
    return loud;
}

void LevelMatcher::step (Rig& m, const ParamValues& v, bool wait)
{
    const double sr = rate.load();
    if (sr <= 0.0) return;
    if (phrase.samples.empty()) adopt (housePhrase (sr), false);
    if (phraseAtRate.samples.empty() || std::abs (phraseAtRate.sampleRate - sr) > 1.0)
        phraseAtRate = atRate (phrase, sr);
    if (filesFlag.exchange (false))
    {
        {
            const juce::ScopedLock sl (queueLock);
            capture = wantCapture;
            userIr = wantIr;
        }
        if (capture.existsAsFile()) m.neural().load (capture.getFullPathName().toStdString());
        if (userIr.existsAsFile()) m.cabinet().loadUserIr (userIr);
        filesKey = (juce::uint64) (capture.getFullPathName() + "|" + userIr.getFullPathName()).hashCode64();
    }

    const bool on = v[(size_t) P::autoLevel] > 0.5f;
    if (! on)
    {
        if (wasOn || restoreFlag.exchange (false)) { stage.setLevelTrimDb (0.0f); trimNow.store (0.0f); }
        wasOn = false;
        have = false;
        return;
    }
    if (! wasOn) { keeper.rematch(); have = false; wasOn = true; }
    if (restoreFlag.exchange (false))
    {
        const juce::ScopedLock sl (queueLock);
        keeper.restore (wantTrim);
        have = false;
        rematchFlag.store (false); // the session's own level stands
    }
    if (rematchFlag.exchange (false)) { keeper.rematch(); have = false; }

    // with the player's own phrase the input level and Input Fix are part of what is measured
    const auto tone = toneForMeasuring (v, phraseAtRate.fromPlayer);
    auto change = have ? changeBetween (measuredTone, tone) : ToneChange::none;
    // A new phrase reads every tone a little differently. The level under the fingers
    // stays where it is; the tone is read again so that the next switch is matched to it.
    if (have && phraseNew) change = std::max (change, ToneChange::knob);
    if (have && change == ToneChange::none) return;
    if (have && change == ToneChange::knob && wait)
    {
        // The trim does not move for a knob. It is measured, once the hand has left it, so
        // that the next switch knows how loud things were.
        const auto t = juce::Time::getMillisecondCounter();
        if (tone != lastSeen) { lastSeen = tone; lastMove = t; }
        if (t - lastMove < kKnobRestMs) return;
    }
    const float loud = loudnessOf (m, tone);
    phraseNew = false;
    const float trim = keeper.reading (loud, change);
    stage.setLevelTrimDb (trim);
    trimNow.store (trim);
    measuredTone = tone;
    have = true;
}

void LevelMatcher::matchNow (const ParamValues& v)
{
    const juce::ScopedLock sl (lock);
    if (meter == nullptr) { meter = std::make_unique<Rig>(); meter->setMeasuring (true); }
    step (*meter, v, false);
}

void LevelMatcher::run()
{
    {
        const juce::ScopedLock sl (lock);
        if (meter == nullptr) { meter = std::make_unique<Rig>(); meter->setMeasuring (true); }
        juce::MemoryBlock bytes;
        LevelPhrase saved;
        if (phraseFile().loadFileAsData (bytes) && phraseFromBytes (bytes.getData(), bytes.getSize(), saved))
            adopt (std::move (saved), false);
    }
    ParamValues v {};
    while (! threadShouldExit())
    {
        wait (12);
        if (threadShouldExit()) break;
        readParams (v);
        const juce::ScopedLock sl (lock);
        if (forget.exchange (false) && learns)
        {
            phraseFile().deleteFile();
            adopt (housePhrase (std::max (8000.0, rate.load())), false);
        }
        listenForPhrase (v[(size_t) P::inGain]);
        measuredSomething = false;
        step (*meter, v, true);
        if (measuredSomething) continue; // that took a while: look at the stage again first

        // nothing to do for the tone on stage: one of the tones that may come next
        const juce::ScopedTryLock turn (shared().aheadTurn);
        if (turn.isLocked() && v[(size_t) P::autoLevel] > 0.5f && rate.load() > 0.0)
        {
            ParamValues next {};
            bool any = false;
            {
                const juce::ScopedLock ql (queueLock);
                if (aheadAt < ahead.size()) { next = ahead[aheadAt++]; any = true; }
            }
            if (any)
            {
                // a preset holds a tone, not a setup: the player's input settings go with it
                for (const auto& s : paramSpecs())
                    if (belongsToSetup (s.id)) next[(size_t) s.id] = v[(size_t) s.id];
                loudnessOf (*meter, toneForMeasuring (next, phraseAtRate.fromPlayer));
            }
        }
    }
}
} // namespace rh
