// Riffhouse - free, open-source guitar rig
// Copyright (C) 2026 Riffhouse contributors. Licensed under AGPL-3.0-or-later.
#include "Looper.h"

#include <algorithm>
#include <cmath>

namespace rh
{
void Looper::prepare (double sampleRate, double maxSecs)
{
    rate = sampleRate;
    capacity = std::max (1, (int) std::lround (sampleRate * maxSecs));
    fade = std::max (1, (int) std::lround (sampleRate * 0.006)); // 6 ms: long enough to hide the join, too short to hear
    bufL.assign ((size_t) capacity, 0.0f);
    bufR.assign ((size_t) capacity, 0.0f);
    st.store ((int) State::empty);
    requests.store (0);
    length.store (0);
    pos.store (0);
    layering = 0.0f;
}

void Looper::press() { requests.fetch_or (pressed); }
void Looper::stop()  { requests.fetch_or (stopped); }
void Looper::clear() { requests.fetch_or (cleared); }

// The first pass is over: what was recorded becomes the loop. Its two ends are faded so the
// join does not click.
void Looper::closeLoop() noexcept
{
    const int len = pos.load();
    const int f = std::min (fade, len / 2);
    for (int i = 0; i < f; ++i)
    {
        const float g = (float) i / (float) f;
        bufL[(size_t) i] *= g;            bufR[(size_t) i] *= g;
        bufL[(size_t) (len - 1 - i)] *= g; bufR[(size_t) (len - 1 - i)] *= g;
    }
    length.store (len);
    pos.store (0);
}

void Looper::process (float* l, float* r, int n) noexcept
{
    if (capacity <= 1 || bufL.empty()) return;
    auto state = (State) st.load();
    const int req = requests.exchange (0);
    if ((req & cleared) != 0)
    {
        state = State::empty;
        length.store (0);
        pos.store (0);
        layering = 0.0f;
    }
    else if ((req & stopped) != 0)
    {
        if (state == State::recording)
        {
            if (pos.load() >= (int) (rate * 0.25)) { closeLoop(); state = State::stopped; }
            else { state = State::empty; pos.store (0); }
        }
        else if (state == State::playing || state == State::overdubbing)
            state = State::stopped;
    }
    else if ((req & pressed) != 0)
    {
        switch (state)
        {
            case State::empty:       state = State::recording; pos.store (0); length.store (0); break;
            case State::recording:
                // a tap shorter than a quarter of a second was not a phrase: forget it
                if (pos.load() >= (int) (rate * 0.25)) { closeLoop(); state = State::playing; }
                else { state = State::empty; pos.store (0); }
                break;
            case State::playing:     state = State::overdubbing; break;
            case State::overdubbing: state = State::playing; break;
            case State::stopped:     state = State::playing; pos.store (0); break;
        }
    }

    const float g = level.load();
    int p = pos.load();
    if (state == State::recording)
    {
        for (int i = 0; i < n; ++i)
        {
            bufL[(size_t) p] = l[i];
            bufR[(size_t) p] = r[i];
            if (++p >= capacity) // as long as a loop can be: close it here and carry on playing
            {
                pos.store (p);
                closeLoop();
                p = 0;
                state = State::playing;
                st.store ((int) state);
                // the rest of this block plays the start of the loop
                const int len = length.load();
                for (int k = i + 1; k < n; ++k)
                {
                    l[k] += bufL[(size_t) p] * g;
                    r[k] += bufR[(size_t) p] * g;
                    if (++p >= len) p = 0;
                }
                pos.store (p);
                return;
            }
        }
    }
    else if (state == State::playing || state == State::overdubbing)
    {
        const int len = std::max (1, length.load());
        const float step = 1.0f / (float) fade;
        const float target = state == State::overdubbing ? 1.0f : 0.0f;
        for (int i = 0; i < n; ++i)
        {
            // layering fades in and out, so switching it never cuts a note in half with a click
            layering += std::clamp (target - layering, -step, step);
            const float oldL = bufL[(size_t) p], oldR = bufR[(size_t) p];
            if (layering > 0.0f)
            {
                bufL[(size_t) p] = oldL + l[i] * layering;
                bufR[(size_t) p] = oldR + r[i] * layering;
            }
            l[i] += oldL * g;
            r[i] += oldR * g;
            if (++p >= len) p = 0;
        }
    }
    else
        layering = 0.0f;
    pos.store (p);
    st.store ((int) state);
}
} // namespace rh
