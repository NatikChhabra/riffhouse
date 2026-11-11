// Riffhouse - free, open-source guitar rig
// Copyright (C) 2026 Riffhouse contributors. Licensed under AGPL-3.0-or-later.
#pragma once

#include "PhraseCatcher.h"
#include "Rig.h"

namespace rh
{
// Auto level. Every tone is played the same few seconds of guitar off stage (the player's own
// once they have been heard, see LevelPhrase), its loudness is measured, and a trim after
// the rig makes up the difference. The rule the player hears:
//   - a tone you load comes up at the house loudness, whatever is in it;
//   - another amp, cabinet or kind of pedal comes up as loud as the one before it;
//   - a pedal kicked on or off may lift or drop the level a little, as a boost should, never a lot;
//   - a knob you turn still does what a knob does.

constexpr float kHouseLoudnessDb = -20.0f;                 // where a loaded tone lands (loud half, weighted, dBFS)
constexpr float kSwitchLeewayDb = 3.0f;                    // how much a pedal coming on or going off may change it
constexpr float kMostCutDb = -36.0f, kMostBoostDb = 18.0f; // a dead quiet chain is not turned up into hiss
constexpr float kSilentDb = -70.0f;                        // below this there is nothing to match

// The tone as it is measured. The player's own phrase is their guitar as it comes off the
// cable, so it is measured through their input level and Input Fix as they are now; the
// house phrase needs neither. The hum filter and the gate are left out either way. The
// volume knob stays in: tones are saved with it.
ParamValues toneForMeasuring (const ParamValues&, bool playersPhrase = false);
// What separates two tones as they are measured: nothing, knobs only (the input level and
// Input Fix count as knobs), something switched on or off, or another model picked (amp,
// cabinet, kind of pedal, kind of reverb...). The biggest of these wins.
enum class ToneChange { none, knob, toggled, swapped };
ToneChange changeBetween (const ParamValues& measuringToneA, const ParamValues& measuringToneB);
juce::uint64 toneHash (const ParamValues& measuringTone);

// How loud `rig` is with this tone, playing the phrase. The rig is prepared afresh at the
// phrase's rate and set to measure; what it held before is gone.
float measureLoudnessDb (Rig& rig, const ParamValues& measuringTone, const LevelPhrase& phrase);

// The rule itself, apart from threads and rigs.
class LevelKeeper
{
public:
    // The next reading sets the level afresh: a tone was loaded, or auto level was switched on.
    void rematch() noexcept { fresh = true; }
    // A reading of the tone as it now is, and what changed since the last reading.
    // Returns the trim to use from now on.
    float reading (float measuredDb, ToneChange change) noexcept;
    float trimDb() const noexcept { return trim; }
    // What a saved session sounded like comes back exactly: the trim is kept until the next switch.
    void restore (float savedTrimDb) noexcept { trim = savedTrimDb; fresh = false; haveTarget = false; }

private:
    float trim = 0.0f, target = kHouseLoudnessDb;
    bool fresh = true, haveTarget = false;
};
} // namespace rh
