// Riffhouse - free, open-source guitar rig
// Copyright (C) 2026 Riffhouse contributors. Licensed under AGPL-3.0-or-later.
#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <vector>

namespace rh
{
constexpr double kPi = 3.14159265358979323846;
constexpr double kTwoPi = 2.0 * kPi;

inline float dbToGain (float db) noexcept { return std::pow (10.0f, db * 0.05f); }
inline float gainToDb (float g) noexcept { return 20.0f * std::log10 (std::max (g, 1.0e-9f)); }
inline float clamp01 (float x) noexcept { return std::clamp (x, 0.0f, 1.0f); }
inline float lerp (float a, float b, float t) noexcept { return a + (b - a) * t; }

// Map 0..1 to an exponential range, used for frequency and time knobs.
inline float expMap (float t, float lo, float hi) noexcept { return lo * std::pow (hi / lo, clamp01 (t)); }

// Fast tanh: Lambert continued fraction, within 1e-6 of std::tanh, exact clamp to +-1.
inline float fastTanh (float x) noexcept
{
    if (x > 4.97f) return 1.0f;
    if (x < -4.97f) return -1.0f;
    const float x2 = x * x;
    const float a = x * (135135.0f + x2 * (17325.0f + x2 * (378.0f + x2)));
    const float b = 135135.0f + x2 * (62370.0f + x2 * (3150.0f + x2 * 28.0f));
    return std::clamp (a / b, -1.0f, 1.0f);
}

// Flush denormals to zero in feedback paths.
inline float undenorm (float x) noexcept { return std::abs (x) < 1.0e-20f ? 0.0f : x; }

// One-pole parameter smoother (about 20 ms by default).
class Smoother
{
public:
    // After prepare() the first target is where the value starts: there is nothing to glide from.
    void prepare (double sampleRate, float timeMs = 20.0f) noexcept
    {
        coeff = (float) std::exp (-1.0 / (0.001 * timeMs * sampleRate));
        fresh = true;
    }
    void reset (float v) noexcept { current = target = v; fresh = false; }
    void setTarget (float v) noexcept
    {
        target = v;
        if (fresh) { current = v; fresh = false; }
    }
    float next() noexcept
    {
        current = target + (current - target) * coeff;
        return current;
    }
    float get() const noexcept { return current; }
    float getTarget() const noexcept { return target; }
    bool isSmoothing() const noexcept { return std::abs (current - target) > 1.0e-6f; }

private:
    float current = 0.0f, target = 0.0f, coeff = 0.0f;
    bool fresh = true;
};

// Topology-preserving one-pole filter (lowpass / highpass outputs).
class OnePole
{
public:
    void setCutoff (double sampleRate, double hz) noexcept
    {
        hz = std::clamp (hz, 1.0, 0.49 * sampleRate);
        const double g = std::tan (kPi * hz / sampleRate);
        G = (float) (g / (1.0 + g));
    }
    void reset() noexcept { s = 0.0f; }
    float lowpass (float x) noexcept
    {
        const float v = (x - s) * G;
        const float y = v + s;
        s = undenorm (y + v);
        return y;
    }
    float highpass (float x) noexcept { return x - lowpass (x); }

private:
    float G = 0.5f, s = 0.0f;
};

// RBJ biquad in transposed direct form II, double precision state.
// A low or high pass that can be swept while it plays: the state-variable form keeps its
// energy where it is when the cutoff moves, where a biquad's stored sums jump and click.
class SweepFilter
{
public:
    enum class Type { lowpass, highpass };
    void set (Type t, double sampleRate, double freq, double q) noexcept
    {
        type = t;
        freq = std::clamp (freq, 5.0, 0.49 * sampleRate);
        const double g = std::tan (kPi * freq / sampleRate);
        k = 1.0 / std::max (q, 0.05);
        a1 = 1.0 / (1.0 + g * (g + k));
        a2 = g * a1;
        a3 = g * a2;
    }
    void reset() noexcept { ic1 = ic2 = 0.0; }
    float process (float xf) noexcept
    {
        const double x = xf;
        const double v3 = x - ic2;
        const double v1 = a1 * ic1 + a2 * v3;
        const double v2 = ic2 + a2 * ic1 + a3 * v3;
        ic1 = 2.0 * v1 - ic1;
        ic2 = 2.0 * v2 - ic2;
        if (std::abs (ic1) < 1e-25) ic1 = 0;
        if (std::abs (ic2) < 1e-25) ic2 = 0;
        return (float) (type == Type::lowpass ? v2 : x - k * v1 - v2);
    }

private:
    Type type = Type::lowpass;
    double k = 1.414, a1 = 1, a2 = 0, a3 = 0, ic1 = 0, ic2 = 0;
};

class Biquad
{
public:
    enum class Type { lowpass, highpass, bandpass, peak, lowShelf, highShelf, notch, allpass };

    void set (Type type, double sampleRate, double freq, double q, double gainDb = 0.0) noexcept
    {
        freq = std::clamp (freq, 5.0, 0.49 * sampleRate);
        q = std::max (q, 0.05);
        const double A = std::pow (10.0, gainDb / 40.0);
        const double w0 = kTwoPi * freq / sampleRate;
        const double cw = std::cos (w0), sw = std::sin (w0);
        const double alpha = sw / (2.0 * q);
        double b0 = 1, b1 = 0, b2 = 0, a0 = 1, a1 = 0, a2 = 0;

        switch (type)
        {
            case Type::lowpass:  b0 = (1 - cw) / 2; b1 = 1 - cw; b2 = b0; a0 = 1 + alpha; a1 = -2 * cw; a2 = 1 - alpha; break;
            case Type::highpass: b0 = (1 + cw) / 2; b1 = -(1 + cw); b2 = b0; a0 = 1 + alpha; a1 = -2 * cw; a2 = 1 - alpha; break;
            case Type::bandpass: b0 = alpha; b1 = 0; b2 = -alpha; a0 = 1 + alpha; a1 = -2 * cw; a2 = 1 - alpha; break;
            case Type::notch:    b0 = 1; b1 = -2 * cw; b2 = 1; a0 = 1 + alpha; a1 = -2 * cw; a2 = 1 - alpha; break;
            case Type::allpass:  b0 = 1 - alpha; b1 = -2 * cw; b2 = 1 + alpha; a0 = 1 + alpha; a1 = -2 * cw; a2 = 1 - alpha; break;
            case Type::peak:
                b0 = 1 + alpha * A; b1 = -2 * cw; b2 = 1 - alpha * A;
                a0 = 1 + alpha / A; a1 = -2 * cw; a2 = 1 - alpha / A; break;
            case Type::lowShelf:
            {
                const double sq = 2 * std::sqrt (A) * alpha;
                b0 = A * ((A + 1) - (A - 1) * cw + sq);
                b1 = 2 * A * ((A - 1) - (A + 1) * cw);
                b2 = A * ((A + 1) - (A - 1) * cw - sq);
                a0 = (A + 1) + (A - 1) * cw + sq;
                a1 = -2 * ((A - 1) + (A + 1) * cw);
                a2 = (A + 1) + (A - 1) * cw - sq;
                break;
            }
            case Type::highShelf:
            {
                const double sq = 2 * std::sqrt (A) * alpha;
                b0 = A * ((A + 1) + (A - 1) * cw + sq);
                b1 = -2 * A * ((A - 1) + (A + 1) * cw);
                b2 = A * ((A + 1) + (A - 1) * cw - sq);
                a0 = (A + 1) - (A - 1) * cw + sq;
                a1 = 2 * ((A - 1) - (A + 1) * cw);
                a2 = (A + 1) - (A - 1) * cw - sq;
                break;
            }
        }
        c0 = b0 / a0; c1 = b1 / a0; c2 = b2 / a0; d1 = a1 / a0; d2 = a2 / a0;
    }

    void reset() noexcept { z1 = z2 = 0.0; }

    float process (float xf) noexcept
    {
        const double x = xf;
        const double y = c0 * x + z1;
        z1 = c1 * x - d1 * y + z2;
        z2 = c2 * x - d2 * y;
        if (std::abs (z1) < 1e-25) z1 = 0;
        if (std::abs (z2) < 1e-25) z2 = 0;
        return (float) y;
    }

    // Magnitude response at a frequency, used by tests and the UI.
    double magnitude (double sampleRate, double hz) const noexcept
    {
        const double w = kTwoPi * hz / sampleRate;
        const double cr1 = std::cos (w), ci1 = -std::sin (w), cr2 = std::cos (2 * w), ci2 = -std::sin (2 * w);
        const double nr = c0 + c1 * cr1 + c2 * cr2, ni = c1 * ci1 + c2 * ci2;
        const double dr = 1 + d1 * cr1 + d2 * cr2, di = d1 * ci1 + d2 * ci2;
        return std::sqrt ((nr * nr + ni * ni) / (dr * dr + di * di));
    }

private:
    double c0 = 1, c1 = 0, c2 = 0, d1 = 0, d2 = 0, z1 = 0, z2 = 0;
};

// DC blocker (~10 Hz).
class DcBlocker
{
public:
    void prepare (double sampleRate) noexcept { R = (float) (1.0 - kTwoPi * 10.0 / sampleRate); }
    void reset() noexcept { x1 = y1 = 0.0f; }
    float process (float x) noexcept
    {
        const float y = x - x1 + R * y1;
        x1 = x;
        y1 = undenorm (y);
        return y;
    }

private:
    float R = 0.999f, x1 = 0.0f, y1 = 0.0f;
};

// Fractional delay line with cubic (Hermite) interpolation.
class DelayLine
{
public:
    void prepare (int maxSamples)
    {
        int size = 1;
        while (size < maxSamples + 4) size <<= 1;
        buffer.assign ((size_t) size, 0.0f);
        mask = size - 1;
        writePos = 0;
    }
    void reset() { std::fill (buffer.begin(), buffer.end(), 0.0f); }
    void push (float x) noexcept
    {
        buffer[(size_t) writePos] = x;
        writePos = (writePos + 1) & mask;
    }
    // delay in samples, >= 1
    float read (float delay) const noexcept
    {
        delay = std::clamp (delay, 1.0f, (float) (mask - 3));
        const float rp = (float) writePos - delay;
        const int i = (int) std::floor (rp);
        const float f = rp - (float) i;
        const float xm1 = at (i - 1), x0 = at (i), x1 = at (i + 1), x2 = at (i + 2);
        const float c = (x1 - xm1) * 0.5f;
        const float v = x0 - x1;
        const float w = c + v;
        const float a = w + v + (x2 - x0) * 0.5f;
        const float bNeg = w + a;
        return ((((a * f) - bNeg) * f + c) * f + x0);
    }
    float readInt (int delay) const noexcept { return at (writePos - delay); }
    int capacity() const noexcept { return mask - 3; }

private:
    float at (int i) const noexcept { return buffer[(size_t) (i & mask)]; }
    std::vector<float> buffer { 0.0f, 0.0f, 0.0f, 0.0f };
    int mask = 3, writePos = 0;
};

// Envelope follower with separate attack and release.
class EnvelopeFollower
{
public:
    void prepare (double sampleRate, float attackMs, float releaseMs) noexcept
    {
        sr = sampleRate;
        setTimes (attackMs, releaseMs);
    }
    void setTimes (float attackMs, float releaseMs) noexcept
    {
        att = (float) std::exp (-1.0 / (0.001 * std::max (0.01f, attackMs) * sr));
        rel = (float) std::exp (-1.0 / (0.001 * std::max (0.01f, releaseMs) * sr));
    }
    void reset() noexcept { env = 0.0f; }
    float process (float x) noexcept
    {
        const float a = std::abs (x);
        env = a > env ? a + (env - a) * att : a + (env - a) * rel;
        env = undenorm (env);
        return env;
    }
    float get() const noexcept { return env; }

private:
    double sr = 48000.0;
    float att = 0.0f, rel = 0.0f, env = 0.0f;
};

// Cheap deterministic noise.
class Noise
{
public:
    explicit Noise (uint32_t seed = 0x1234567u) : state (seed) {}
    float next() noexcept
    {
        state = state * 1664525u + 1013904223u;
        return (float) ((int32_t) state) * (1.0f / 2147483648.0f);
    }

private:
    uint32_t state;
};

// Sine LFO with phase offset.
class Lfo
{
public:
    void prepare (double sampleRate) noexcept { sr = sampleRate; phase = 0.0f; }
    void setRate (float hz) noexcept { inc = (float) (hz / sr); }
    void setPhase (float p) noexcept { phase = p - std::floor (p); }
    float phaseValue() const noexcept { return phase; }
    float sine (float offset = 0.0f) const noexcept
    {
        float p = phase + offset;
        p -= std::floor (p);
        return (float) std::sin (kTwoPi * p);
    }
    float triangle (float offset = 0.0f) const noexcept
    {
        float p = phase + offset;
        p -= std::floor (p);
        return 4.0f * std::abs (p - 0.5f) - 1.0f;
    }
    void advance() noexcept
    {
        phase += inc;
        if (phase >= 1.0f) phase -= 1.0f;
    }

private:
    double sr = 48000.0;
    float phase = 0.0f, inc = 0.0f;
};
} // namespace rh
