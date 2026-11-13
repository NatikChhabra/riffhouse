// Riffhouse - free, open-source guitar rig
// Copyright (C) 2026 Riffhouse contributors. Licensed under AGPL-3.0-or-later.
#include "Cabinet.h"
#include "CabCurves.h"

#include <complex>

namespace rh
{
namespace
{
    // name, box resonance Hz and Q, closed back (0 open .. 1 sealed), dust cap lift dB, horn dB, seed
    // The speaker's own response is its row in CabCurves.h, in the same order.
    const CabDescription cabs[] = {
        { "1x12 Open Back", 95.0f, 0.75f, 0.0f, 3.0f, 0.0f, 11u },
        { "2x12 Blue", 88.0f, 0.85f, 0.3f, 3.0f, 0.0f, 23u },
        { "4x12 Vintage", 92.0f, 1.2f, 1.0f, 3.0f, 0.0f, 37u },
        { "4x12 Modern", 85.0f, 1.35f, 1.0f, 3.0f, 0.0f, 41u },
        { "2x12 American", 80.0f, 0.9f, 0.2f, 3.0f, 0.0f, 53u },
        { "1x8 Tiny Combo", 180.0f, 1.3f, 0.5f, 2.0f, 0.0f, 67u },
        { "4x10 Bass", 62.0f, 1.0f, 1.0f, 1.5f, -14.0f, 71u },
        { "1x15 Bass", 45.0f, 0.9f, 1.0f, 1.0f, 0.0f, 83u },
    };
    static_assert (sizeof (cabs) / sizeof (cabs[0]) == sizeof (kCabCurves) / sizeof (kCabCurves[0]), "a curve for every cab");

    using cd = std::complex<double>;

    void fft (std::vector<cd>& a, bool inverse)
    {
        const size_t n = a.size();
        for (size_t i = 1, j = 0; i < n; ++i)
        {
            size_t bit = n >> 1;
            for (; j & bit; bit >>= 1) j ^= bit;
            j ^= bit;
            if (i < j) std::swap (a[i], a[j]);
        }
        for (size_t len = 2; len <= n; len <<= 1)
        {
            const double ang = kTwoPi / (double) len * (inverse ? 1 : -1);
            const cd wl (std::cos (ang), std::sin (ang));
            for (size_t i = 0; i < n; i += len)
            {
                cd w (1);
                for (size_t j = 0; j < len / 2; ++j)
                {
                    const cd u = a[i + j], v = a[i + j + len / 2] * w;
                    a[i + j] = u + v;
                    a[i + j + len / 2] = u - v;
                    w *= wl;
                }
            }
        }
        if (inverse)
            for (auto& x : a) x /= (double) n;
    }

    // analog prototype responses (magnitude in dB)
    double hp2Db (double f, double f0, double q)
    {
        const double r = f / f0;
        const double re = 1.0 - r * r, im = r / q;
        return 20.0 * std::log10 ((r * r) / std::sqrt (re * re + im * im));
    }
    double lp2Db (double f, double f0, double q)
    {
        const double r = f / f0;
        const double re = 1.0 - r * r, im = r / q;
        return 20.0 * std::log10 (1.0 / std::sqrt (re * re + im * im));
    }
    double peakDb (double f, double f0, double db, double q)
    {
        // log-frequency bell, close to an analog peaking filter
        const double x = std::log2 (f / f0) * q * 1.5;
        return db * std::exp (-x * x);
    }
    double shelfDb (double f, double f0, double db, bool high)
    {
        const double x = std::log2 (f / f0) * 2.0;
        const double s = 1.0 / (1.0 + std::exp (-x));
        return db * (high ? s : 1.0 - s);
    }
} // namespace

int numCabs() { return (int) (sizeof (cabs) / sizeof (cabs[0])); }
const CabDescription& cabDescription (int i) { return cabs[std::clamp (i, 0, numCabs() - 1)]; }

std::vector<float> generateCabIr (int cabIndex, float mic, float distance, double sampleRate)
{
    const auto& c = cabDescription (cabIndex);
    mic = clamp01 (mic);
    distance = clamp01 (distance);

    const int N = sampleRate > 100000.0 ? 32768 : 16384;
    std::vector<double> logMag ((size_t) N / 2 + 1);

    // The speaker itself is its published curve (CabCurves.h). Below about 200 Hz that curve
    // shows the maker's test baffle, not a cabinet, so the box takes over there: a sealed
    // back gives the tight resonance hump, an open back loses more of the deep lows.
    const auto speakerDb = [&] (double f)
    {
        const auto& row = kCabCurves[std::clamp (cabIndex, 0, numCabs() - 1)];
        const double k = std::clamp (12.0 * std::log2 (f / kCabCurveLowHz), 0.0, (double) (kCabCurvePoints - 1));
        const int i = std::min ((int) k, kCabCurvePoints - 2);
        const double t = k - i;
        return (double) row[i] + t * (double) (row[i + 1] - row[i]);
    };
    const double joinHz = 200.0;
    const double boxAtJoin = hp2Db (joinHz, c.resonanceHz, c.resonanceQ) - speakerDb (joinHz);

    // The published curve is on axis at a metre, which sits about where the mic knob starts
    // (0.3). Toward the dust cap the top gets brighter; toward the cone edge it rolls off,
    // about 10 dB down at 5 kHz at the very edge, as a close mic does.
    const double toCap = std::max (0.0, (0.3 - mic) / 0.3);
    const double toEdge = std::max (0.0, ((double) mic - 0.3) / 0.7);
    // A directional mic's bass lift grows fast as it nears the grille: about 3 dB where the
    // distance knob starts (0.2), near 6 dB touching, gone a couple of feet back.
    const double proximityDb = 2.9 * std::pow ((1.0 - distance) / 0.8, 3.0);

    for (int i = 0; i <= N / 2; ++i)
    {
        const double f = std::max (1.0, (double) i * sampleRate / N);
        const double box = hp2Db (f, c.resonanceHz, c.resonanceQ) - boxAtJoin;
        const double w = std::clamp ((std::log2 (f) - std::log2 (120.0)) / (std::log2 (250.0) - std::log2 (120.0)), 0.0, 1.0);
        const double ws = w * w * (3.0 - 2.0 * w);
        double db = ws * speakerDb (f) + (1.0 - ws) * box;
        db += (1.0 - c.closedBack) * shelfDb (f, c.resonanceHz * 1.3, -3.0, false);
        db += shelfDb (f, 180.0, proximityDb, false);
        if (c.tweeterDb != 0.0f)
        {
            // a horn above the cone, padded down
            const double horn = c.tweeterDb + hp2Db (f, 3500.0, 0.7) + lp2Db (f, 12000.0, 0.7);
            const double cone = db;
            db = 10.0 * std::log10 (std::pow (10.0, cone / 10.0) + std::pow (10.0, horn / 10.0));
        }
        db += toCap * (peakDb (f, 4500.0, c.brightness, 0.9));
        db += toEdge * 0.6 * lp2Db (f, 1800.0, 0.7);
        // gentle infrasonic and ultrasonic limits
        db += hp2Db (f, 30.0, 0.7) + lp2Db (f, std::min (18000.0, 0.45 * sampleRate), 0.7);
        logMag[(size_t) i] = std::max (db, -120.0) * std::log (10.0) / 20.0;
    }

    // real cepstrum -> minimum phase spectrum
    std::vector<cd> buf ((size_t) N);
    for (int i = 0; i <= N / 2; ++i) buf[(size_t) i] = logMag[(size_t) i];
    for (int i = N / 2 + 1; i < N; ++i) buf[(size_t) i] = logMag[(size_t) (N - i)];
    fft (buf, true);
    for (int i = 1; i < N / 2; ++i) buf[(size_t) i] *= 2.0;
    for (int i = N / 2 + 1; i < N; ++i) buf[(size_t) i] = 0.0;
    fft (buf, false);
    for (auto& v : buf) v = std::exp (v);
    fft (buf, true);

    const int len = (int) std::round (0.045 * sampleRate);
    std::vector<float> ir ((size_t) len);
    for (int i = 0; i < len; ++i) ir[(size_t) i] = (float) buf[(size_t) i].real();

    // floor bounce and a small room as the mic moves back
    if (distance > 0.01f)
    {
        const int bounce = (int) ((0.4 + 1.6 * distance) * 0.001 * sampleRate);
        std::vector<float> early (ir);
        for (int i = bounce; i < len; ++i)
            ir[(size_t) i] += 0.45f * distance * early[(size_t) (i - bounce)];
        Noise rn (c.seed + 99u);
        OnePole lp;
        lp.setCutoff (sampleRate, 5000.0);
        const float roomLevel = 0.025f * distance;
        const int start = (int) (0.004 * sampleRate);
        for (int i = start; i < len; ++i)
        {
            const float t = (float) (i - start) / (float) sampleRate;
            ir[(size_t) i] += roomLevel * lp.lowpass (rn.next()) * std::exp (-t / 0.012f);
        }
    }

    // fade the tail
    const int fade = len / 4;
    for (int i = 0; i < fade; ++i)
        ir[(size_t) (len - fade + i)] *= 0.5f * (1.0f + std::cos ((float) kPi * (float) i / (float) fade));

    // normalise so every cab sits at the same loudness in the guitar's core range
    const std::vector<float> probe { 250, 500, 1000, 2000 };
    const auto mags = irMagnitudeDb (ir, sampleRate, probe);
    float mean = 0;
    for (float m : mags) mean += m;
    mean /= (float) mags.size();
    const float g = dbToGain (-mean);
    for (auto& s : ir) s *= g;
    return ir;
}

std::vector<float> irMagnitudeDb (const std::vector<float>& ir, double sampleRate, const std::vector<float>& freqs)
{
    std::vector<float> out;
    out.reserve (freqs.size());
    for (float f : freqs)
    {
        double re = 0, im = 0;
        const double w = kTwoPi * f / sampleRate;
        for (size_t i = 0; i < ir.size(); ++i)
        {
            re += ir[i] * std::cos (w * (double) i);
            im -= ir[i] * std::sin (w * (double) i);
        }
        out.push_back ((float) (10.0 * std::log10 (re * re + im * im + 1e-30)));
    }
    return out;
}

//==============================================================================
Cabinet::Cabinet() : juce::Thread ("Riffhouse cab builder") {}

Cabinet::~Cabinet() { stopThread (2000); }

void Cabinet::prepare (double sampleRate, int maxBlock)
{
    stopThread (2000);
    sr = sampleRate;
    spec = { sampleRate, (juce::uint32) maxBlock, 1 };
    prepared = false;
    rebuildNow();
    conv.prepare (spec);
    prepared = true;
    // A fresh start is silent: the two filters after the speaker would otherwise ring on with
    // whatever played before, and the next tone began on top of the last one's final samples.
    reset();
    lowHz = highHz = 0;
    glide = std::exp (-16.0 / (0.020 * sr)); // about 20 ms to most of the way
    cutsSnap = true;
    set (model.load(), mic.load(), distance.load(), 60.0f, 12000.0f);
    if (! offline) startThread (juce::Thread::Priority::low);
}

void Cabinet::reset()
{
    conv.reset();
    lowCut.reset();
    highCut.reset();
    // Until sound runs again, the cut knobs land where they are set: nothing to glide from.
    cutsSnap = true;
    if (lowHz > 0) { lowNow = lowHz; highNow = highHz; setCuts(); }
}

void Cabinet::set (int m, float mc, float dist, float lowCutHz, float highCutHz)
{
    model.store (m);
    mic.store (mc);
    distance.store (dist);
    if (m != lastModel || std::abs (mc - lastMic) > 1e-4f || std::abs (dist - lastDist) > 1e-4f)
    {
        lastModel = m; lastMic = mc; lastDist = dist;
        pending.store (true);
        notify();
    }
    if (lowCutHz != lowHz || highCutHz != highHz)
    {
        lowHz = lowCutHz; highHz = highCutHz;
        // A turned cut knob glides there: a filter that jumps clicks on a held note.
        if (cutsSnap) { lowNow = lowHz; highNow = highHz; setCuts(); }
    }
}

void Cabinet::setCuts()
{
    lowCut.set (SweepFilter::Type::highpass, sr, lowNow, 0.707);
    highCut.set (SweepFilter::Type::lowpass, sr, std::min (highNow, 0.45 * sr), 0.707);
}

void Cabinet::installIr (std::vector<float> ir)
{
    juce::AudioBuffer<float> b (1, (int) ir.size());
    std::copy (ir.begin(), ir.end(), b.getWritePointer (0));
    conv.loadImpulseResponse (std::move (b), sr, juce::dsp::Convolution::Stereo::no,
                              juce::dsp::Convolution::Trim::no, juce::dsp::Convolution::Normalise::no);
}

void Cabinet::installUserIr()
{
    juce::AudioBuffer<float> copy;
    double rate = 48000.0;
    {
        const juce::ScopedLock sl (userLock);
        if (! userIrLoaded) return;
        copy = userIr;
        rate = userIrRate;
    }
    // Normalise so a user IR sits near the level of the built-in cabs.
    conv.loadImpulseResponse (std::move (copy), rate, juce::dsp::Convolution::Stereo::no,
                              juce::dsp::Convolution::Trim::yes, juce::dsp::Convolution::Normalise::yes);
}

void Cabinet::rebuildNow()
{
    // What is built here is what set() last asked for, so nothing is left for the worker to
    // do. Without this the worker loaded the same IR a second time a moment after every
    // start, and the convolver crossfaded into a copy of itself with an empty tail.
    pending.store (false);
    const int m = model.load();
    lastModel = m; lastMic = mic.load(); lastDist = distance.load();
    if (m >= numCabs())
    {
        if (userIrLoaded) installUserIr();
        else installIr (generateCabIr (2, mic.load(), distance.load(), sr));
    }
    else
        installIr (generateCabIr (m, mic.load(), distance.load(), sr));
    if (prepared) conv.prepare (spec);
}

void Cabinet::rebuildCrossfaded()
{
    pending.store (false);
    const int m = model.load();
    lastModel = m; lastMic = mic.load(); lastDist = distance.load();
    if (m < numCabs()) installIr (generateCabIr (m, mic.load(), distance.load(), sr));
    else installUserIr();
    juce::Thread::sleep (60);
}

void Cabinet::run()
{
    while (! threadShouldExit())
    {
        wait (200);
        if (pending.exchange (false))
        {
            wait (30); // let fast knob moves settle
            pending.store (false);
            const int m = model.load();
            if (m < numCabs())
                installIr (generateCabIr (m, mic.load(), distance.load(), sr));
            else
                installUserIr();
        }
    }
}

bool Cabinet::loadUserIr (const juce::File& file)
{
    juce::AudioFormatManager fm;
    fm.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> reader (fm.createReaderFor (file));
    if (reader == nullptr || reader->lengthInSamples < 16)
        return false;
    const int len = (int) std::min<juce::int64> (reader->lengthInSamples, (juce::int64) (reader->sampleRate * 1.5));
    juce::AudioBuffer<float> b ((int) reader->numChannels, len);
    reader->read (&b, 0, len, 0, true, true);
    juce::AudioBuffer<float> mono (1, len);
    mono.clear();
    for (int ch = 0; ch < b.getNumChannels(); ++ch)
        mono.addFrom (0, 0, b, ch, 0, len, 1.0f / (float) b.getNumChannels());
    {
        const juce::ScopedLock sl (userLock);
        userIr = mono;
        userIrRate = reader->sampleRate;
        userIrLoaded = true;
        userName = file.getFileNameWithoutExtension();
    }
    if (model.load() >= numCabs())
        installUserIr();
    return true;
}

juce::String Cabinet::userIrName() const
{
    const juce::ScopedLock sl (userLock);
    return userName;
}

void Cabinet::process (float* x, int n)
{
    float* chans[] = { x };
    juce::dsp::AudioBlock<float> block (chans, 1, (size_t) n);
    juce::dsp::ProcessContextReplacing<float> ctx (block);
    conv.process (ctx);
    cutsSnap = false;
    for (int i = 0; i < n;)
    {
        const bool lowMoving = std::abs (std::log (lowNow / lowHz)) > 1e-4;
        const bool highMoving = std::abs (std::log (highNow / highHz)) > 1e-4;
        if (lowMoving || highMoving)
        {
            lowNow = lowMoving ? lowHz * std::pow (lowNow / lowHz, glide) : (double) lowHz;
            highNow = highMoving ? highHz * std::pow (highNow / highHz, glide) : (double) highHz;
            setCuts();
        }
        const int end = std::min (n, i + 16);
        for (; i < end; ++i)
            x[i] = highCut.process (lowCut.process (x[i]));
    }
}
} // namespace rh
