// Riffhouse - free, open-source guitar rig
// Copyright (C) 2026 Riffhouse contributors. Licensed under AGPL-3.0-or-later.
#pragma once

#include <algorithm>
#include <cmath>

namespace rh
{
// Setup sets the input level from the hardest playing it heard. People play harder still
// once a song is on, and a rig fed hotter than it was voiced for turns harsh. The guard
// watches the level going into the pedals and, when playing keeps arriving well over the
// mark, says by how much to turn the input down. It only ever turns down: playing softly
// for a while is not a reason to make the next hard chord too hot.
class InputGuard
{
public:
    static constexpr float kTargetDb = -12.0f;   // where setup puts the hardest peaks
    static constexpr float kHotDb = -6.0f;       // this far over the mark is too hot ...
    static constexpr int kHotTimes = 3;          // ... once it has happened this often
    static constexpr float kAtOnceDb = -1.5f;    // about to clip: no waiting
    static constexpr float kSettleDb = -10.0f;   // where the loudest peak heard is brought to
    static constexpr double kForgetSeconds = 20.0;

    void reset() noexcept { hot = 0; worstDb = -120.0f; sinceHot = 0.0; }

    // Call a few times a second with the loudest peak (linear) into the pedals since the last
    // call. Returns how many dB to turn the input down, or 0. After a non-zero answer it
    // starts afresh, so apply it.
    float heard (float peak, double secondsSinceLastCall) noexcept
    {
        const float db = 20.0f * std::log10 (std::max (peak, 1.0e-6f));
        sinceHot += secondsSinceLastCall;
        if (hot > 0 && sinceHot > kForgetSeconds) reset(); // a stray bang long ago does not count toward the next
        if (db > kHotDb)
        {
            ++hot;
            worstDb = std::max (worstDb, db);
            sinceHot = 0.0;
        }
        if (hot >= kHotTimes || db > kAtOnceDb)
        {
            const float down = std::max (1.0f, std::ceil ((worstDb - kSettleDb) * 2.0f) * 0.5f);
            reset();
            return down;
        }
        return 0.0f;
    }

private:
    int hot = 0;
    float worstDb = -120.0f;
    double sinceHot = 0.0;
};
} // namespace rh
