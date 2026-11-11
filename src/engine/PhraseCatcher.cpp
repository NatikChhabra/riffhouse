// Riffhouse - free, open-source guitar rig
// Copyright (C) 2026 Riffhouse contributors. Licensed under AGPL-3.0-or-later.
#include "PhraseCatcher.h"

#include "ReferenceGuitar.h"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace rh
{
namespace
{
    juce::uint64 hashOf (const std::vector<float>& x)
    {
        juce::uint64 h = 1469598103934665603ull;
        for (size_t i = 0; i < x.size(); i += 7)
        {
            juce::uint32 bits = 0;
            std::memcpy (&bits, &x[i], sizeof (bits));
            h = (h ^ bits) * 1099511628211ull;
        }
        return h ^ (juce::uint64) x.size();
    }

    // power of each 0.1 s of the signal
    std::vector<double> frames (const float* l, const float* r, size_t n, double sr)
    {
        const size_t len = (size_t) std::max (1.0, 0.1 * sr);
        std::vector<double> f;
        for (size_t at = 0; at + len <= n; at += len)
        {
            double e = 0.0;
            for (size_t i = at; i < at + len; ++i)
                e += r != nullptr ? 0.5 * ((double) l[i] * l[i] + (double) r[i] * r[i]) : (double) l[i] * l[i];
            f.push_back (e / (double) len);
        }
        return f;
    }
} // namespace

float loudHalfDb (const float* l, const float* r, size_t n, double sr)
{
    const auto f = frames (l, r, n, sr);
    if (f.size() < 4) return -120.0f;
    std::vector<double> stretch; // 0.4 s, moving on by 0.1 s
    for (size_t i = 0; i + 4 <= f.size(); ++i)
        stretch.push_back (0.25 * (f[i] + f[i + 1] + f[i + 2] + f[i + 3]));
    std::sort (stretch.begin(), stretch.end(), std::greater<double>());
    const size_t half = std::max<size_t> (1, stretch.size() / 2);
    double sum = 0.0;
    for (size_t i = 0; i < half; ++i) sum += stretch[i];
    if (! std::isfinite (sum)) return -120.0f;
    return (float) (10.0 * std::log10 (sum / (double) half + 1.0e-12));
}

LevelPhrase housePhrase (double sr)
{
    LevelPhrase p;
    p.samples = referencePhrase (sr);
    p.sampleRate = sr;
    p.loudDb = loudHalfDb (p.samples.data(), nullptr, p.samples.size(), sr);
    p.id = hashOf (p.samples);
    return p;
}

LevelPhrase atRate (const LevelPhrase& p, double sr)
{
    if (p.samples.empty() || std::abs (p.sampleRate - sr) < 1.0) return p;
    if (! p.fromPlayer) return housePhrase (sr);
    LevelPhrase out = p;
    const double step = p.sampleRate / sr;
    out.samples.resize ((size_t) ((double) p.samples.size() / step));
    for (size_t i = 0; i < out.samples.size(); ++i)
    {
        const double at = (double) i * step;
        const size_t a = std::min ((size_t) at, p.samples.size() - 1), b = std::min (a + 1, p.samples.size() - 1);
        const float frac = (float) (at - (double) a);
        out.samples[i] = p.samples[a] + frac * (p.samples[b] - p.samples[a]);
    }
    out.sampleRate = sr;
    return out;
}

bool worthKeeping (const std::vector<float>& x, double sr, float& loudDb, float inGainDb)
{
    loudDb = -120.0f;
    if (x.size() < (size_t) (2.0 * sr)) return false;
    float peak = 0.0f;
    for (float s : x) peak = std::max (peak, std::abs (s));
    const double gain = std::pow (10.0, inGainDb / 20.0), power = gain * gain;
    if (peak > 0.98f || peak * gain < 0.02) return false; // clipped, or far too quiet to be the player digging in
    const auto f = frames (x.data(), nullptr, x.size(), sr);
    // sounding: above -55 dBFS at the pedals and within 30 dB of the loudest tenth of a
    // second, so that a chord left to ring counts for as long as it can be heard
    const double sounding = std::max (3.2e-6 / power, 1.0e-3 * *std::max_element (f.begin(), f.end()));
    int active = 0, struck = 0;
    for (size_t i = 0; i < f.size(); ++i)
    {
        if (f[i] > sounding) ++active;
        // a string struck: four times the power of the tenth of a second before, and well above the floor
        if (f[i] > 16.0 * sounding && f[i] > 4.0 * (i > 0 ? f[i - 1] : 0.0)) ++struck;
    }
    // one pop is struck once and gone; a guitar goes on sounding
    if (active * 10 < (int) f.size() * 6 || struck < 1) return false;
    loudDb = loudHalfDb (x.data(), nullptr, x.size(), sr);
    return loudDb + inGainDb > -45.0f;
}

void PhraseCatcher::prepare (double sampleRate)
{
    sr = sampleRate;
    written.store (0);
    ring.assign ((size_t) (2.0 * kSeconds * sr), 0.0f); // twice what is read, so a reader is never overtaken
}

void PhraseCatcher::push (const float* x, int n) noexcept
{
    if (ring.empty()) return;
    const juce::int64 w = written.load (std::memory_order_relaxed);
    const size_t size = ring.size();
    size_t at = (size_t) (w % (juce::int64) size);
    for (int i = 0; i < n; ++i)
    {
        ring[at] = x[i];
        if (++at == size) at = 0;
    }
    written.store (w + n, std::memory_order_release);
}

bool PhraseCatcher::latest (std::vector<float>& out) const
{
    const size_t want = (size_t) (kSeconds * sr), size = ring.size();
    const juce::int64 w = written.load (std::memory_order_acquire);
    if (size == 0 || w < (juce::int64) want) return false;
    out.resize (want);
    size_t at = (size_t) ((w - (juce::int64) want) % (juce::int64) size);
    for (size_t i = 0; i < want; ++i)
    {
        out[i] = ring[at];
        if (++at == size) at = 0;
    }
    // the audio thread wrote on meanwhile: fine unless it came all the way round
    return written.load (std::memory_order_acquire) - w < (juce::int64) (size - want);
}

juce::MemoryBlock phraseToBytes (const LevelPhrase& p)
{
    juce::MemoryOutputStream out;
    out.writeInt (0x52485031); // "RHP1"
    out.writeDouble (p.sampleRate);
    out.writeFloat (p.inGainDb);
    out.writeInt ((int) p.samples.size());
    for (float s : p.samples)
        out.writeShort ((short) std::lround (std::clamp (s, -1.0f, 1.0f) * 32767.0f));
    return out.getMemoryBlock();
}

bool phraseFromBytes (const void* data, size_t size, LevelPhrase& p)
{
    juce::MemoryInputStream in (data, size, false);
    if (size < 20 || in.readInt() != 0x52485031) return false;
    const double sr = in.readDouble();
    const float gain = in.readFloat();
    const int n = in.readInt();
    if (sr < 8000.0 || sr > 400000.0 || n < (int) sr || n > (int) (20.0 * sr) || (size_t) n * 2 > size - 20) return false;
    p = {};
    p.samples.resize ((size_t) n);
    for (auto& s : p.samples) s = (float) in.readShort() / 32767.0f;
    p.sampleRate = sr;
    p.inGainDb = gain;
    p.fromPlayer = true;
    p.id = hashOf (p.samples);
    float loud = -120.0f;
    worthKeeping (p.samples, sr, loud);
    p.loudDb = loudHalfDb (p.samples.data(), nullptr, p.samples.size(), sr);
    return std::isfinite (gain) && p.loudDb > -60.0f;
}
} // namespace rh
