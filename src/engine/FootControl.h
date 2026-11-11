// Riffhouse - free, open-source guitar rig
// Copyright (C) 2026 Riffhouse contributors. Licensed under AGPL-3.0-or-later.
#pragma once

#include "Params.h"

#include <array>
#include <string>
#include <string_view>
#include <vector>

namespace rh
{
// Hands on the guitar, feet on the floor: any MIDI foot controller, expression pedal or
// keyboard can turn a knob, stomp a pedal on and off, step through tones or press the looper.
// Nothing is typed in. You point at the thing in Riffhouse, then move the thing on the floor.

// What a controller can be given to do.
enum class FootJob { param, nextTone, previousTone, looper, looperStop, tapTempo, tuner, record, song, count };

struct FootTarget
{
    FootJob job = FootJob::param;
    int param = 0; // which control of the rig, when the job is one
    bool operator== (const FootTarget& o) const noexcept { return job == o.job && (job != FootJob::param || param == o.param); }
    bool isSwitch() const noexcept; // on and off, or a press: not something that sweeps
};

struct FootBinding
{
    int controller = 0;  // MIDI controller number, 0 to 127
    FootTarget target;
    bool press = false;  // the switch sends a press and a release, so each press flips
};

class FootControl
{
public:
    // What a moved controller asks for.
    struct Action
    {
        FootTarget target;
        float value = 0.0f; // 0 to 1 along a knob's travel; 1 or 0 for a switch
        bool flip = false;  // a switch: flip it instead of setting it; a job: do it
    };

    FootControl(); // starts with what most floors already send: an expression pedal rocks the wah

    // Teaching. The next controller that moves takes the target.
    void learn (FootTarget t) noexcept { learningNow = true; wanted = t; }
    void stopLearning() noexcept { learningNow = false; }
    bool learning() const noexcept { return learningNow; }
    FootTarget learningFor() const noexcept { return wanted; }

    // A controller moved (value 0 to 127). now is seconds on any steady clock.
    // True when there is something to do; learned says the message was the one being waited for.
    bool moved (int controller, int value, double now, Action& out, bool& learned);

    const std::vector<FootBinding>& bindings() const noexcept { return list; }
    int controllerFor (FootTarget t) const noexcept; // -1 when it has none
    void remove (size_t index);
    void setPress (size_t index, bool press);
    void backToStart();
    // True once after a switch showed, by springing back, what kind it is: worth saving.
    bool kindFound() noexcept { const bool f = found; found = false; return f; }

    // One line of text for the settings file.
    std::string toText() const;
    void fromText (std::string_view text);

private:
    std::vector<FootBinding> list;
    std::array<int, 128> last {};
    bool learningNow = false;
    FootTarget wanted;
    int freshController = -1; // just taught: is it a switch that springs back?
    double freshAt = 0.0;
    bool found = false;
};

// "Wah: Position", "Next tone": what the settings screen and the messages call a target.
std::string footTargetName (FootTarget t);
}
