#pragma once

// Riffhouse: the cushion between a guitar input and a separate output on Windows Audio.
//
// Two sound cards never run at exactly the same speed: a USB guitar dongle and USB headphones
// each have their own clock, a few hundred parts per million apart, and each hands over sound in
// its own packets at its own moments. JUCE's Windows Audio read the input only when the output
// wanted sound, with no cushion: whenever the input packet had not arrived yet it played silence
// in its place (the crackle), and in Exclusive Mode it never read the input while it waited for
// the output, so input packets were lost (the "dropouts" that pushed setup up to 1024 samples).
// None of the silent gaps were counted, so setup thought the smallest buffer played clean.
//
// This keeps a small cushion of input and reads it a hair faster or slower, by a ratio that follows
// the cushion's average fill (averaged over time, so the moment packets happen to land does not
// steer it), so the two clocks never drift apart. The read is a 4-point Hermite interpolation; the
// ratio stays within 0.5% and in practice moves by the clocks' difference, well under one cent.
// Every real gap (ran dry, overflowed) is counted.
//
// Plain C++ with no JUCE, so the offline tests run it against simulated clocks (testElasticInput).
// cmake copies it next to juce_WASAPI_windows.cpp, which uses it for every input + output pair.

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

namespace riffhouse
{

class ElasticInput
{
public:
    // channels: input channels kept; block: frames per callback; inPacket: frames the input
    // arrives in; outBurst: frames the output takes at once (its period: in shared mode several
    // callbacks run back to back when Windows asks); sampleRate: the rate both run at.
    void prepare (int channels, int block, int inPacket, int outBurst, double sampleRate)
    {
        numChannels = std::max (1, channels);
        blockSize = std::max (1, block);
        rate = sampleRate > 0 ? sampleRate : 48000.0;
        const double pull = (double) std::max (blockSize, outBurst);
        const double packet = (double) std::max (1, inPacket);
        // The fill swings around its average by half an input packet and half an output burst;
        // just before a read it must still hold one read, plus a margin for the scheduler.
        target = pull + 0.5 * (packet + pull) + std::max (48.0, 0.25 * packet) + 3.0;
        // far past this after a stall: jump back instead of taking seconds to read it down
        tooFull = target + 2.0 * (pull + packet) + 256.0;
        // average over several whole packets and reads so their sawtooth does not wobble the ratio
        window = std::max (0.05, 6.0 * (pull + packet) / rate);
        capacity = 1;
        while (capacity < (int) (target * 4.0 + pull * 2.0 + packet * 2.0) + 64) capacity <<= 1;
        ring.assign ((size_t) capacity * (size_t) numChannels, 0.0f);
        gaps = 0;
        reset();
    }

    void reset()
    {
        writeAbs = 0;
        readAbs = 1;      // one frame of history behind the read point, for the curve
        frac = 0.0;
        priming = true;
        haveTime = false;
        area = areaTime = 0.0;
        avgFill = target;
        integral = 0.0;
        ratio = 1.0;
        std::fill (ring.begin(), ring.end(), 0.0f);
        writeAbs = 1;
    }

    // whole frames waiting at or after the read point
    int fill() const noexcept { return (int) (writeAbs - readAbs); }
    // the average delay the cushion adds, in frames
    int latencyFrames() const noexcept { return (int) std::lround (target); }
    double currentRatio() const noexcept { return ratio; }
    int glitches() const noexcept { return gaps; }
    double averageFill() const noexcept { return avgFill; }

    // add frames from the input (non-interleaved floats); now: seconds on any steady clock
    void push (const float* const* src, int numSrc, int frames, double now)
    {
        if (frames <= 0 || numSrc <= 0) return;
        track (now);
        // A stall (a frozen window, a driver hiccup) left more than the ring holds: drop the
        // oldest so the delay comes back to the cushion, and count it.
        if (writeAbs + frames - (readAbs - 1) > capacity)
        {
            readAbs = writeAbs + frames - (int64_t) std::lround (target);
            frac = 0.0;
            ++gaps;
        }
        for (int c = 0; c < numChannels; ++c)
        {
            const float* s = src[std::min (c, numSrc - 1)];
            float* r = ring.data() + (size_t) c * (size_t) capacity;
            for (int i = 0; i < frames; ++i)
                r[(size_t) ((writeAbs + i) & (capacity - 1))] = s != nullptr ? s[i] : 0.0f;
        }
        writeAbs += frames;
    }

    // produce frames for the callback
    void pull (float* const* dest, int numDest, int frames, double now)
    {
        if (frames <= 0) return;
        track (now);

        if (priming)
        {
            if ((double) fill() < target)
            {
                for (int c = 0; c < numDest; ++c) std::fill (dest[c], dest[c] + frames, 0.0f);
                return;
            }
            readAbs = writeAbs - (int64_t) std::lround (target);
            frac = 0.0;
            avgFill = target;
            integral = 0.0;
            area = areaTime = 0.0;
            priming = false;
            fadeIn = kFadeFrames;
        }

        if ((double) fill() > tooFull)
        {
            readAbs = writeAbs - (int64_t) std::lround (target);
            frac = 0.0;
            ++gaps;
            fadeIn = kFadeFrames; // a jump in the sound: come back in softly instead of a click
        }

        // positions read: frac + k * ratio for k < frames, each needing 2 frames after it
        int playable = frames;
        const double lastPos = frac + (double) (frames - 1) * ratio;
        if ((int64_t) std::floor (lastPos) + 2 >= fill())
        {
            const double room = (double) (fill() - 3) - frac;
            playable = room < 0.0 ? 0 : std::min (frames, (int) std::floor (room / ratio) + 1);
        }

        render (dest, numDest, playable);
        advance ((double) playable * ratio);

        if (fadeIn > 0 && playable > 0)
        {
            const int n = std::min (playable, fadeIn);
            for (int c = 0; c < numDest; ++c)
                for (int i = 0; i < n; ++i)
                    dest[c][i] *= (float) (kFadeFrames - fadeIn + i + 1) / (float) kFadeFrames;
            fadeIn -= n;
        }

        if (playable < frames)
        {
            // ran dry: fade out from the last sample, count it and build the cushion again
            for (int c = 0; c < numDest; ++c)
            {
                const float last = playable > 0 ? dest[c][playable - 1] : 0.0f;
                const int fade = std::min (frames - playable, 32);
                for (int i = 0; i < frames - playable; ++i)
                    dest[c][playable + i] = i < fade ? last * (1.0f - (float) (i + 1) / (float) fade) : 0.0f;
            }
            ++gaps;
            priming = true;
        }
    }

private:
    // gains on the average fill error in seconds: settles in ~2 s, damped, and the integral holds
    // a steady clock difference with no error left over
    static constexpr double kP = 0.5;       // 1 ms too full -> read 0.05% faster
    static constexpr double kI = 0.1;       // per second
    static constexpr double kMaxStep = 0.005;
    static constexpr int kFadeFrames = 64;

    void track (double now)
    {
        if (! haveTime) { lastTime = now; haveTime = true; return; }
        const double dt = std::max (0.0, now - lastTime);
        lastTime = now;
        if (priming) return;
        area += ((double) fill() - frac) * dt;
        areaTime += dt;
        if (areaTime < window) return;

        const double windowFill = area / areaTime;
        const double a = std::min (1.0, areaTime / 0.3);
        avgFill += a * (windowFill - avgFill);
        const double err = (avgFill - target) / rate;          // seconds; + = too full, read faster
        integral = std::clamp (integral + err * kI * areaTime, -kMaxStep, kMaxStep);
        ratio = 1.0 + std::clamp (err * kP + integral, -kMaxStep, kMaxStep);
        area = areaTime = 0.0;
    }

    float at (int c, int64_t abs) const noexcept
    {
        return ring[(size_t) c * (size_t) capacity + (size_t) (abs & (capacity - 1))];
    }

    void render (float* const* dest, int numDest, int frames) const
    {
        for (int c = 0; c < numDest; ++c)
        {
            const int sc = std::min (c, numChannels - 1);
            for (int k = 0; k < frames; ++k)
            {
                const double p = frac + (double) k * ratio;
                const double whole = std::floor (p);
                const int64_t i1 = readAbs + (int64_t) whole;
                const float t = (float) (p - whole);
                const float y0 = at (sc, i1 - 1), y1 = at (sc, i1), y2 = at (sc, i1 + 1), y3 = at (sc, i1 + 2);
                const float c1 = 0.5f * (y2 - y0);
                const float c2 = y0 - 2.5f * y1 + 2.0f * y2 - 0.5f * y3;
                const float c3 = 0.5f * (y3 - y0) + 1.5f * (y1 - y2);
                dest[c][k] = ((c3 * t + c2) * t + c1) * t + y1;
            }
        }
    }

    void advance (double by)
    {
        const double total = frac + by;
        const double whole = std::floor (total);
        readAbs += (int64_t) whole;
        frac = total - whole;
    }

    std::vector<float> ring;
    int numChannels = 1, blockSize = 256, capacity = 1, gaps = 0;
    double rate = 48000.0, target = 768.0, tooFull = 4096.0, window = 0.05;
    int64_t writeAbs = 1, readAbs = 1;
    double frac = 0.0, lastTime = 0.0, area = 0.0, areaTime = 0.0;
    double avgFill = 768.0, integral = 0.0, ratio = 1.0;
    int fadeIn = 0;
    bool priming = true, haveTime = false;
};

} // namespace riffhouse
