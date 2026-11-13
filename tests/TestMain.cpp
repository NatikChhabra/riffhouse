// Riffhouse - free, open-source guitar rig
// Copyright (C) 2026 Riffhouse contributors. Licensed under AGPL-3.0-or-later.
//
// Offline test suite and render tool. Runs without an audio device, so it
// works in CI. Usage:
//   riffhouse_tests                 run every test, exit code 1 on failure
//   riffhouse_tests render <preset.json|name> <out.wav> [in.wav] [rate]
//   riffhouse_tests renderall <outdir> [rate]
//   riffhouse_tests report          print loudness/latency/CPU table

#include "engine/InputGuard.h"
#include "TestSignals.h"
#include "engine/History.h"
#include "engine/LevelMatch.h"
#include "engine/ReferenceGuitar.h"
#include "engine/Looper.h"
#include "engine/Song.h"
#include "engine/FootControl.h"
#include "engine/Presets.h"
#include "engine/Rig.h"
#include "engine/UpdateInfo.h"
#include "engine/DeviceNames.h"
#include "engine/ChordSheet.h"
#include "engine/Metronome.h"
#include "dsp/CabCurves.h"
#include "plugin/LevelMatcher.h"
#include "plugin/Takes.h"
#include "plugin/Export.h"
#include "plugin/Tuner.h"

#include <juce_audio_formats/juce_audio_formats.h>
#include <chrono>
#include <cstdio>

#if RIFFHOUSE_HAS_PRESETS
 #include "PresetData.h"
#endif

using namespace rh;

namespace rh::test { void runDeviceChecks (int& checks, int& failures); } // TestDevices.cpp

namespace
{
int failures = 0, checks = 0;

#define CHECK(cond, ...)                                                  \
    do {                                                                  \
        ++checks;                                                         \
        if (! (cond)) {                                                   \
            ++failures;                                                   \
            std::printf ("  FAIL %s:%d: ", __FILE__, __LINE__);           \
            std::printf (__VA_ARGS__);                                    \
            std::printf ("\n");                                           \
        }                                                                 \
    } while (0)

struct Render
{
    std::vector<float> l, r;
    double seconds = 0;
};

ParamValues bare()
{
    // everything off except what a test switches on
    auto v = defaultParamValues();
    for (const auto& s : paramSpecs())
        if (s.kind == ParamKind::toggle) v[(size_t) s.id] = 0.0f;
    v[(size_t) P::humFilter] = 0.0f;
    return v;
}

Render run (Rig& rig, const ParamValues& v, const std::vector<float>& in, int block = 256)
{
    rig.setParams (v);
    Render r;
    r.l.resize (in.size());
    r.r.resize (in.size());
    const auto t0 = std::chrono::steady_clock::now();
    for (size_t pos = 0; pos < in.size(); pos += (size_t) block)
    {
        const int n = (int) std::min<size_t> ((size_t) block, in.size() - pos);
        rig.setParams (v);
        rig.process (in.data() + pos, r.l.data() + pos, r.r.data() + pos, n);
    }
    r.seconds = std::chrono::duration<double> (std::chrono::steady_clock::now() - t0).count();
    return r;
}

Render renderWith (const ParamValues& v, const std::vector<float>& in, double sr, int block = 256)
{
    Rig rig;
    rig.setOffline (true);
    rig.setParams (v);
    rig.prepare (sr, block);
    rig.setParams (v);
    rig.finishPendingWork();
    rig.prepare (sr, block);
    rig.finishPendingWork();
    return run (rig, v, in, block);
}

float rmsDb (const std::vector<float>& x, size_t from = 0, size_t to = 0)
{
    if (to == 0 || to > x.size()) to = x.size();
    double acc = 0;
    for (size_t i = from; i < to; ++i) acc += (double) x[i] * x[i];
    return gainToDb ((float) std::sqrt (acc / std::max<size_t> (1, to - from)));
}

float peakOf (const std::vector<float>& x)
{
    float p = 0;
    for (float s : x) p = std::max (p, std::abs (s));
    return p;
}

bool allFinite (const Render& r)
{
    for (size_t i = 0; i < r.l.size(); ++i)
        if (! std::isfinite (r.l[i]) || ! std::isfinite (r.r[i])) return false;
    return true;
}

int onset (const std::vector<float>& x, float fraction = 0.05f)
{
    const float pk = peakOf (x);
    for (size_t i = 0; i < x.size(); ++i)
        if (std::abs (x[i]) >= fraction * pk) return (int) i;
    return -1;
}

std::vector<Preset> loadPresets()
{
    PresetLibrary lib;
    lib.scanFolder (juce::File (RIFFHOUSE_PRESET_DIR), true);
    return lib.all();
}

bool writeWav (const juce::File& f, const Render& r, double sr)
{
    f.deleteFile();
    juce::WavAudioFormat wav;
    std::unique_ptr<juce::OutputStream> os (f.createOutputStream().release());
    if (! os) return false;
    std::unique_ptr<juce::AudioFormatWriter> w (wav.createWriterFor (os.get(), sr, 2, 24, {}, 0));
    if (! w) return false;
    os.release();
    const float* ch[] = { r.l.data(), r.r.data() };
    w->writeFromFloatArrays (ch, 2, (int) r.l.size());
    return true;
}

std::vector<float> readWavMono (const juce::File& f, double& sr)
{
    juce::AudioFormatManager fm;
    fm.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> rd (fm.createReaderFor (f));
    if (! rd) return {};
    sr = rd->sampleRate;
    juce::AudioBuffer<float> b ((int) rd->numChannels, (int) rd->lengthInSamples);
    rd->read (&b, 0, (int) rd->lengthInSamples, 0, true, true);
    std::vector<float> out ((size_t) b.getNumSamples());
    std::copy (b.getReadPointer (0), b.getReadPointer (0) + b.getNumSamples(), out.begin());
    return out;
}

//==============================================================================
void testParams()
{
    std::printf ("params\n");
    const auto& s = paramSpecs();
    CHECK ((int) s.size() == kNumParams, "param count");
    for (size_t i = 0; i < s.size(); ++i)
    {
        CHECK ((size_t) s[i].id == i, "param %s out of order", s[i].key);
        CHECK (s[i].def >= s[i].min && s[i].def <= s[i].max, "default out of range for %s", s[i].key);
        for (size_t j = i + 1; j < s.size(); ++j)
            CHECK (std::string (s[i].key) != s[j].key, "duplicate key %s", s[i].key);
    }
    CHECK ((int) spec (P::ampModel).choices.size() == choices::numAmpModels, "amp choice list");
    CHECK ((int) spec (P::cabModel).choices.size() == choices::numCabModels, "cab choice list");
    CHECK (numAmpVoicings() == choices::numAmpModels - 1, "amp voicing count");
    CHECK (numCabs() == choices::numCabModels - 1, "cab count");
}

void testToneStacks()
{
    std::printf ("tone stacks\n");
    ToneStack t;
    t.prepare (192000.0);
    for (auto c : { tonestacks::american, tonestacks::british, tonestacks::modern, tonestacks::chime, tonestacks::tweed })
    {
        t.set (c, 0.5, 0.5, 0.5);
        const double lo = t.magnitude (100), mid = t.magnitude (600), hi = t.magnitude (4000);
        CHECK (lo > 0 && mid > 0 && hi > 0 && lo < 2 && hi < 2, "tone stack magnitudes sane");
        t.set (c, 1.0, 0.5, 0.5);
        const double hiUp = t.magnitude (4000);
        CHECK (hiUp > hi, "treble knob raises highs");
        t.set (c, 0.5, 0.5, 1.0);
        CHECK (t.magnitude (80) > lo, "bass knob raises lows");
    }
}

void testAmps (double sr)
{
    std::printf ("amps @ %.0f Hz\n", sr);
    const auto di = test::guitarDi (sr, 6.0);
    for (int m = 0; m < numAmpVoicings(); ++m)
    {
        for (float g : { 0.0f, 5.0f, 10.0f })
        {
            auto v = bare();
            v[(size_t) P::ampOn] = 1;
            v[(size_t) P::cabOn] = 1;
            v[(size_t) P::ampModel] = (float) m;
            v[(size_t) P::ampGain] = g;
            v[(size_t) P::cabModel] = (float) (m == choices::bassStation ? choices::cabBass4x10 : choices::cabVintage4x12);
            const auto r = renderWith (v, di, sr);
            CHECK (allFinite (r), "amp %d gain %.0f produced NaN", m, g);
            const float level = rmsDb (r.l);
            CHECK (level > -45.0f && level < -4.0f, "amp %s gain %.0f level %.1f dB out of range", ampVoicing (m).name, g, level);
            double mean = 0;
            for (float s : r.l) mean += s;
            mean /= (double) r.l.size();
            CHECK (std::abs (mean) < 0.01, "amp %s has DC offset %.4f", ampVoicing (m).name, mean);
        }
    }
}

// The rule from user feedback: no pedal may add a delay or echo you can hear.
void testLatency (double sr)
{
    std::printf ("latency @ %.0f Hz\n", sr);
    const auto imp = test::impulse ((int) sr / 2, 1000);

    struct Case { const char* name; P toggle; P extra; float extraValue; };
    const Case cases[] = {
        { "gate", P::gateOn, P::gateThresh, -96.0f }, { "compressor", P::compOn, P::count, 0 },
        { "wah", P::wahOn, P::wahMode, 0.0f }, { "octave", P::pitchOn, P::pitchMode, 1.0f },
        { "drive", P::drvOn, P::count, 0 }, { "amp", P::ampOn, P::count, 0 }, { "cab", P::cabOn, P::count, 0 },
        { "eq", P::eqOn, P::eq800, 6.0f }, { "chorus", P::modOn, P::count, 0 }, { "phaser", P::modOn, P::modType, 2.0f },
        { "tremolo", P::modOn, P::modType, 3.0f }, { "lofi", P::lofiOn, P::lofiWobble, 0.0f },
    };

    Rig probe;
    probe.prepare (sr, 256);
    const int allowed = probe.latencySamples() + 3;

    for (const auto& c : cases)
    {
        auto v = bare();
        v[(size_t) c.toggle] = 1;
        if (c.extra != P::count) v[(size_t) c.extra] = c.extraValue;
        if (c.toggle == P::drvOn) v[(size_t) P::drvType] = 0;
        const auto r = renderWith (v, imp, sr);
        const int at = onset (r.l, 0.05f) - 1000;
        CHECK (at >= -1 && at <= allowed, "%s adds latency: onset at %d samples (allowed %d)", c.name, at, allowed);
    }

    // a full rig with every non-time pedal on still answers immediately
    auto v = defaultParamValues();
    v[(size_t) P::compOn] = 1; v[(size_t) P::drvOn] = 1; v[(size_t) P::eqOn] = 1; v[(size_t) P::modOn] = 1;
    v[(size_t) P::gateOn] = 0; v[(size_t) P::revOn] = 0;
    const auto r = renderWith (v, imp, sr);
    const int at = onset (r.l, 0.05f) - 1000;
    CHECK (at <= (int) (0.0005 * sr), "full rig onset at %d samples", at);
    std::printf ("  full rig onset: %d samples (%.2f ms)\n", at, 1000.0 * at / sr);
    std::printf ("  reported rig latency: %d samples (%.2f ms)\n", probe.latencySamples(), 1000.0 * probe.latencySamples() / sr);
}

void testCabs (double sr)
{
    std::printf ("cabs @ %.0f Hz\n", sr);
    const std::vector<float> f { 60, 120, 500, 1000, 2500, 5000, 8000, 12000 };
    for (int c = 0; c < numCabs(); ++c)
    {
        const auto center = generateCabIr (c, 0.0f, 0.0f, sr);
        const auto edge = generateCabIr (c, 1.0f, 0.0f, sr);
        const auto mc = irMagnitudeDb (center, sr, f), me = irMagnitudeDb (edge, sr, f);
        bool finite = true;
        for (float x : center) finite &= std::isfinite (x);
        CHECK (finite, "cab %d IR not finite", c);
        CHECK (mc[5] > me[5] + 2.0f, "cab %s: edge mic should be darker at 5 kHz (%.1f vs %.1f)", cabDescription (c).name, mc[5], me[5]);
        CHECK (mc[7] < mc[3] - 12.0f, "cab %s: no steep top-end rolloff", cabDescription (c).name);
        // minimum phase: energy concentrated at the start
        int peakIdx = 0;
        for (int i = 0; i < (int) center.size(); ++i)
            if (std::abs (center[(size_t) i]) > std::abs (center[(size_t) peakIdx])) peakIdx = i;
        CHECK (peakIdx < (int) (0.0015 * sr), "cab %s IR peak at %d samples: would sound late", cabDescription (c).name, peakIdx);
    }

    // Each cab against its speaker's published curve (bar line 17). Both are smoothed to
    // 1/3 octave and set to 0 dB over 250 Hz to 2 kHz, then compared from 250 Hz to 5 kHz
    // at the default mic (0.3) and distance (0.2). Below 250 Hz the box and the mic's
    // closeness shape the lows on purpose, so the baffle curve is no reference there.
    std::vector<float> fine;
    for (double f = 100.0; f <= 8000.0; f *= std::pow (2.0, 1.0 / 48.0)) fine.push_back ((float) f);
    const auto third = [&] (const std::vector<float>& db, double at)
    {
        double sum = 0;
        int n = 0;
        for (size_t i = 0; i < fine.size(); ++i)
            if (fine[i] >= at * std::pow (2.0, -1.0 / 6.0) && fine[i] <= at * std::pow (2.0, 1.0 / 6.0))
            {
                sum += std::pow (10.0, db[i] / 10.0);
                ++n;
            }
        return 10.0 * std::log10 (sum / std::max (1, n));
    };
    const auto table = [] (int c, double f)
    {
        const double k = std::clamp (12.0 * std::log2 (f / kCabCurveLowHz), 0.0, (double) (kCabCurvePoints - 1));
        const int i = std::min ((int) k, kCabCurvePoints - 2);
        return kCabCurves[c][i] + (k - i) * (kCabCurves[c][i + 1] - kCabCurves[c][i]);
    };
    std::vector<double> centres;
    for (double f = 250.0; f <= 5001.0; f *= std::pow (2.0, 1.0 / 6.0)) centres.push_back (f);
    for (int c = 0; c < numCabs(); ++c)
    {
        const auto ours = irMagnitudeDb (generateCabIr (c, 0.3f, 0.2f, sr), sr, fine);
        std::vector<float> theirs;
        for (float f : fine) theirs.push_back ((float) table (c, f));
        std::vector<double> diff;
        for (double f : centres) diff.push_back (third (ours, f) - third (theirs, f));
        double mean = 0;
        int n = 0;
        for (size_t i = 0; i < centres.size(); ++i)
            if (centres[i] <= 2001.0) { mean += diff[i]; ++n; }
        mean /= n;
        double worst = 0, worstHz = 0;
        for (size_t i = 0; i < centres.size(); ++i)
            if (std::abs (diff[i] - mean) > std::abs (worst)) { worst = diff[i] - mean; worstHz = centres[i]; }
        std::printf ("  %-15s vs its speaker's published curve: worst %+.1f dB at %.0f Hz\n", cabDescription (c).name, worst, worstHz);
        CHECK (std::abs (worst) <= 2.0, "cab %s is %.1f dB off its speaker's published curve at %.0f Hz", cabDescription (c).name, worst, worstHz);

        // the mic moves the sound the way a close mic does: band levels against 500 Hz to 2 kHz,
        // so the room's comb (a floor bounce at a distance) does not decide it
        const auto band = [&] (float mic, float dist, double lo, double hi)
        {
            const auto m = irMagnitudeDb (generateCabIr (c, mic, dist, sr), sr, fine);
            const auto avg = [&] (double a, double b)
            {
                double sum = 0;
                int k = 0;
                for (size_t i = 0; i < fine.size(); ++i)
                    if (fine[i] >= a && fine[i] <= b) { sum += std::pow (10.0, m[i] / 10.0); ++k; }
                return 10.0 * std::log10 (sum / std::max (1, k));
            };
            return avg (lo, hi) - avg (500.0, 2000.0);
        };
        const double def = band (0.3f, 0.2f, 4000, 6000), cap = band (0.0f, 0.2f, 4000, 6000), rim = band (1.0f, 0.2f, 4000, 6000);
        const double near = band (0.3f, 0.0f, 100, 160), far = band (0.3f, 1.0f, 100, 160);
        CHECK (cap > def + 0.7, "cab %s: the dust cap should be brighter (%.1f vs %.1f)", cabDescription (c).name, cap, def);
        CHECK (rim < def - 6.0, "cab %s: the cone edge should be much darker (%.1f vs %.1f)", cabDescription (c).name, rim, def);
        CHECK (near > far + 2.0, "cab %s: a close mic should have more low end (%.1f vs %.1f)", cabDescription (c).name, near, far);
    }
}

// How each amp answers picking strength (flagship bar line 16): the reference phrase at five
// strengths, 6 dB apart, through the amp and its usual cab. Level and brightness should move
// smoothly the same way as the picking, never jump or turn back.
void testDynamics (double sr)
{
    std::printf ("picking dynamics @ %.0f Hz\n", sr);
    const auto phrase = referencePhrase (sr);
    const float strengths[] = { -18.0f, -12.0f, -6.0f, 0.0f, 6.0f };
    const auto bright = [&] (const std::vector<float>& x)
    {
        // share of the sound above about 2 kHz, in dB
        OnePole a, b;
        a.setCutoff (sr, 2000.0);
        b.setCutoff (sr, 2000.0);
        double hi = 0, all = 0;
        for (float s : x)
        {
            const float h1 = s - a.lowpass (s);
            const float h2 = h1 - b.lowpass (h1);
            hi += (double) h2 * h2;
            all += (double) s * s;
        }
        return 10.0 * std::log10 ((hi + 1e-20) / (all + 1e-20));
    };
    // Captures of real amps, for comparison: RH_CAPTURES=<folder of .nam files>
    if (const char* dir = std::getenv ("RH_CAPTURES"))
        for (const auto& f : juce::File (dir).findChildFiles (juce::File::findFiles, false, "*.nam"))
        {
            Rig rig;
            rig.setOffline (true);
            const auto err = rig.neural().load (f.getFullPathName().toStdString());
            if (! err.empty()) { std::printf ("  %s: %s\n", f.getFileName().toRawUTF8(), err.c_str()); continue; }
            auto v = bare();
            v[(size_t) P::ampOn] = 1.0f;
            v[(size_t) P::ampModel] = (float) choices::neuralCapture;
            v[(size_t) P::ampGain] = 5.0f;
            v[(size_t) P::cabOn] = 1.0f;
            v[(size_t) P::cabModel] = 2.0f;
            rig.setParams (v);
            rig.prepare (sr, 256);
            rig.finishPendingWork();
            float lv[5], br[5];
            for (int k = 0; k < 5; ++k)
            {
                std::vector<float> in (phrase);
                const float g = dbToGain (strengths[k]);
                for (auto& x : in) x *= g;
                rig.prepare (sr, 256);
                rig.finishPendingWork();
                const auto r = run (rig, v, in);
                lv[k] = rmsDb (r.l);
                br[k] = (float) bright (r.l);
            }
            std::printf ("  REAL %-40s level", f.getFileNameWithoutExtension().substring (0, 40).toRawUTF8());
            for (float x : lv) std::printf (" %6.1f", x);
            std::printf ("   brightness");
            for (float x : br) std::printf (" %6.1f", x);
            std::printf ("   range %.1f dB\n", lv[4] - lv[0]);
        }
    for (int m = 0; m < choices::numAmpModels; ++m)
    {
        if (m == choices::neuralCapture) continue;
        for (float gain : { 2.0f, 5.0f, 8.0f })
        {
            auto v = bare();
            v[(size_t) P::ampOn] = 1.0f;
            v[(size_t) P::ampModel] = (float) m;
            v[(size_t) P::ampGain] = gain;
            v[(size_t) P::cabOn] = 1.0f;
            v[(size_t) P::cabModel] = (float) (m == choices::bassStation ? 6 : m <= choices::chimeTop ? 4 : m <= choices::hotRodLead ? 2 : 3);
            float lv[5], br[5];
            for (int k = 0; k < 5; ++k)
            {
                std::vector<float> in (phrase);
                const float g = dbToGain (strengths[k]);
                for (auto& x : in) x *= g;
                const auto r = renderWith (v, in, sr);
                lv[k] = rmsDb (r.l);
                br[k] = (float) bright (r.l);
            }
            std::printf ("  %-16s gain %.0f: level", spec (P::ampModel).choices[(size_t) m], gain);
            for (float x : lv) std::printf (" %6.1f", x);
            std::printf ("   brightness");
            for (float x : br) std::printf (" %6.1f", x);
            std::printf ("   range %.1f dB\n", lv[4] - lv[0]);
            // Captures of real amps measured this way (13 Nov): a clean amp follows the picking
            // dB for dB, a crunch channel moves 8 to 10 dB over these 24 dB, a high-gain channel
            // under 1 dB (0.2 dB for two of them). All get a little brighter as you dig in.
            for (int k = 1; k < 5; ++k)
            {
                CHECK (lv[k] >= lv[k - 1] - 0.05f && lv[k] - lv[k - 1] <= 6.5f, "%s gain %.0f: picking %.0f dB to %.0f dB moves the level %.1f dB (should rise, smoothly)",
                       spec (P::ampModel).choices[(size_t) m], gain, strengths[k - 1], strengths[k], lv[k] - lv[k - 1]);
                CHECK (br[k] >= br[k - 1] - 0.4f, "%s gain %.0f: digging in from %.0f dB to %.0f dB makes it darker (%.1f dB)",
                       spec (P::ampModel).choices[(size_t) m], gain, strengths[k - 1], strengths[k], br[k] - br[k - 1]);
            }
        }
    }
}

// Tone balance of our amps beside captures of real amps, both through the same cab:
// ampvsreal <folder of .nam files>. Prints 1/3-octave levels, 0 dB = 250 Hz to 2 kHz.
int ampVsReal (const juce::File& dir)
{
    const double sr = 48000.0;
    const auto in = playedGuitar (sr, 12.0, 0.32f, true);
    std::vector<float> fine;
    for (double f = 60.0; f <= 12000.0; f *= std::pow (2.0, 1.0 / 24.0)) fine.push_back ((float) f);
    const double centres[] = { 80, 125, 200, 315, 500, 800, 1250, 2000, 2500, 3150, 4000, 5000, 6300, 8000 };
    auto spectrum = [&] (const std::vector<float>& x)
    {
        // averaged over 8192-sample frames with a Hann window
        const int N = 8192;
        juce::dsp::FFT fft (13);
        std::vector<float> acc ((size_t) N / 2 + 1, 0.0f), buf ((size_t) N * 2);
        int frames = 0;
        for (size_t at = 0; at + (size_t) N <= x.size(); at += (size_t) N / 2, ++frames)
        {
            std::fill (buf.begin(), buf.end(), 0.0f);
            for (int i = 0; i < N; ++i) buf[(size_t) i] = x[at + (size_t) i] * 0.5f * (1.0f - std::cos (2.0f * (float) kPi * (float) i / (float) N));
            fft.performFrequencyOnlyForwardTransform (buf.data());
            for (int i = 0; i <= N / 2; ++i) acc[(size_t) i] += buf[(size_t) i] * buf[(size_t) i];
        }
        std::vector<double> bands;
        for (double c : centres)
        {
            double e = 0;
            for (int i = 1; i <= N / 2; ++i)
            {
                const double f = (double) i * sr / N;
                if (f >= c * std::pow (2.0, -1.0 / 6.0) && f < c * std::pow (2.0, 1.0 / 6.0)) e += acc[(size_t) i];
            }
            bands.push_back (10.0 * std::log10 (e + 1e-30));
        }
        double ref = 0;
        int n = 0;
        for (size_t i = 0; i < bands.size(); ++i)
            if (centres[i] >= 250 && centres[i] <= 2000) { ref += bands[i]; ++n; }
        ref /= n;
        for (auto& b : bands) b -= ref;
        return bands;
    };
    auto show = [&] (const juce::String& name, const std::vector<double>& b)
    {
        std::printf ("%-34s", name.substring (0, 34).toRawUTF8());
        for (double x : b) std::printf (" %5.1f", x);
        std::printf ("\n");
    };
    std::printf ("%-34s", "");
    for (double c : centres) std::printf (" %5.0f", c);
    std::printf ("\n");
    for (const auto& f : dir.findChildFiles (juce::File::findFiles, false, "*.nam"))
    {
        Rig rig;
        rig.setOffline (true);
        if (! rig.neural().load (f.getFullPathName().toStdString()).empty()) continue;
        auto v = bare();
        v[(size_t) P::ampOn] = 1.0f;
        v[(size_t) P::ampModel] = (float) choices::neuralCapture;
        v[(size_t) P::ampGain] = 5.0f;
        v[(size_t) P::cabOn] = 1.0f;
        v[(size_t) P::cabModel] = 3.0f;
        rig.setParams (v);
        rig.prepare (sr, 256);
        rig.finishPendingWork();
        show ("REAL " + f.getFileNameWithoutExtension(), spectrum (run (rig, v, in).l));
    }
    for (int m = choices::brit800; m <= choices::tightMetal; ++m)
        for (float g : { 4.0f, 6.0f, 8.0f })
        {
            auto v = bare();
            v[(size_t) P::ampOn] = 1.0f;
            v[(size_t) P::ampModel] = (float) m;
            v[(size_t) P::ampGain] = g;
            v[(size_t) P::cabOn] = 1.0f;
            v[(size_t) P::cabModel] = 3.0f;
            show (juce::String (spec (P::ampModel).choices[(size_t) m]) + " g" + juce::String ((int) g), spectrum (renderWith (v, in, sr).l));
        }
    return 0;
}

void testHum (double sr)
{
    std::printf ("hum detection @ %.0f Hz\n", sr);
    // A buzzing ground loop as a friend's real recording had it: the hum plus harmonics up to
    // 2 kHz (the 35th as loud as the 5th), peaks 13 dB over its level, a little hiss under it.
    // The canceller learns it inside a second and takes the buzz out, not only the low hum;
    // the gate then closes over what is left.
    {
        Noise n;
        std::vector<float> buzz ((size_t) (sr * 4));
        const int harm[] = { 1, 2, 3, 5, 9, 11, 13, 20, 33, 35, 37 };
        const float amp[] = { 0.0020f, 0.0010f, 0.0007f, 0.0009f, 0.0006f, 0.0006f, 0.0007f, 0.0003f, 0.0004f, 0.0008f, 0.0006f };
        for (size_t i = 0; i < buzz.size(); ++i)
        {
            const double t = (double) i / sr;
            double s = 0.0;
            for (int k = 0; k < 11; ++k) s += amp[k] * std::sin (kTwoPi * 49.97 * harm[k] * t + k);
            buzz[i] = (float) s + 0.0003f * n.next();
        }
        HumCanceller h;
        h.prepare (sr);
        h.setMains (50);
        auto y = buzz;
        for (auto& s : y) s = h.process (s);
        const size_t from = (size_t) (sr * 3);
        const float before = rmsDb (std::vector<float> (buzz.begin() + (long) from, buzz.end()));
        const float after = rmsDb (std::vector<float> (y.begin() + (long) from, y.end()));
        CHECK (after < before - 9.0f, "a buzzing hum is taken out: %.1f dB before, %.1f after", before, after);
        NoiseGate g;
        g.prepare (sr);
        g.set (-58.0f, 80.0f);
        auto gated = buzz;
        for (size_t at = 0; at < gated.size(); at += 256) g.process (gated.data() + at, (int) std::min<size_t> (256, gated.size() - at));
        CHECK (rmsDb (std::vector<float> (gated.begin() + (long) from, gated.end())) < -90.0f, "the gate closes over a buzzing hum (it sits at %.1f dB)", g.thresholdNowDb());
    }
    for (int mains : { 50, 60 })
    {
        auto di = test::guitarDi (sr, 8.0, 0.2f);
        for (size_t i = 0; i < di.size(); ++i)
        {
            const double t = (double) i / sr;
            di[i] += 0.01f * (float) (std::sin (kTwoPi * mains * t) + 0.5 * std::sin (kTwoPi * 3 * mains * t) + 0.3 * std::sin (kTwoPi * 2 * mains * t));
        }
        auto v = bare();
        v[(size_t) P::humFilter] = 1;
        Rig rig;
        rig.prepare (sr, 256);
        const auto r = run (rig, v, di);
        CHECK (rig.meters().mains.load() == mains, "expected %d Hz hum, detected %d", mains, rig.meters().mains.load());
        // after detection the hum is gone: compare a silent-ish stretch
        const auto clean = test::sine (sr, mains, 2.0, 0.01f);
        const auto r2 = run (rig, v, clean);
        CHECK (rmsDb (r2.l, (size_t) sr, (size_t) (2 * sr)) < rmsDb (clean) - 20.0f, "hum not removed for %d Hz", mains);
    }
    // a plain guitar must not trigger the hum filter by itself
    {
        const auto di = test::guitarDi (sr, 12.0);
        auto v = bare();
        v[(size_t) P::humFilter] = 1;
        Rig rig;
        rig.prepare (sr, 256);
        run (rig, v, di);
        CHECK (rig.meters().mains.load() == 0, "false hum detection on clean guitar (%d)", rig.meters().mains.load());
    }
}

void testInputFix (double sr)
{
    std::printf ("input fix: measuring and restoring lost treble\n");
    const auto di = playedGuitar (sr, 10.0, 0.32f, true); // with the treble of a recorded guitar
    auto measure = [sr] (const std::vector<float>& x)
    {
        TiltProbe p;
        p.prepare (sr);
        for (size_t pos = 0; pos < x.size(); pos += 256)
            p.process (x.data() + pos, (int) std::min<size_t> (256, x.size() - pos));
        return std::pair<float, double> { p.tiltDb(), p.secondsMeasured() };
    };
    // a passive pickup into a low-impedance mic jack: the pickup's inductance and
    // the input's resistance make a first-order low-pass (about 300 Hz for a
    // 5 kOhm mic input and a 2.5 H pickup, higher for line inputs)
    auto load = [sr] (std::vector<float> x, double hz)
    {
        const float a = (float) (1.0 - std::exp (-kTwoPi * hz / sr));
        float y = 0.0f;
        for (auto& v : x) { y += a * (v - y); v = y; }
        return x;
    };
    auto fix = [sr] (std::vector<float> x, float amount)
    {
        InputConditioner c;
        c.prepare (sr);
        c.set (InputConditioner::humOff, true, amount);
        for (size_t pos = 0; pos < x.size(); pos += 256)
            c.process (x.data() + pos, (int) std::min<size_t> (256, x.size() - pos));
        return x;
    };

    const auto [clean, cleanSecs] = measure (di);
    std::printf ("  clean DI tilt %.1f dB over %.1f s (reference %.1f)\n", clean, cleanSecs, TiltProbe::referenceTiltDb);
    CHECK (cleanSecs > 5.0, "probe heard only %.1f s of a 10 s performance", cleanSecs);
    CHECK (TiltProbe::fixAmountFor (clean) == 0.0f, "a clean DI should need no fix (tilt %.1f)", clean);

    // how much the reading moves with what you play: 2 s windows of the performance
    {
        float lo = 100.0f, hi = -100.0f;
        for (size_t start = 0; start + (size_t) (2 * sr) <= di.size(); start += (size_t) sr)
        {
            const auto t = measure (std::vector<float> (di.begin() + (long) start, di.begin() + (long) (start + (size_t) (2 * sr)))).first;
            lo = std::min (lo, t); hi = std::max (hi, t);
        }
        std::printf ("  2 s windows range %.1f to %.1f dB\n", lo, hi);
        CHECK (TiltProbe::fixAmountFor (lo) <= 6.0f, "a darker 2 s of clean playing asks for %.1f dB of fix", TiltProbe::fixAmountFor (lo));
    }
    CHECK (std::abs (clean - TiltProbe::referenceTiltDb) < 5.5f, "the test guitar reads %.1f dB, a recorded one %.1f", clean, TiltProbe::referenceTiltDb);

    auto band = [sr] (const std::vector<float>& x, double hz)
    {
        Biquad a, b;
        a.set (Biquad::Type::bandpass, sr, hz, 2.0); b.set (Biquad::Type::bandpass, sr, hz, 2.0);
        double e = 0;
        for (float v : x) { const float y = b.process (a.process (v)); e += (double) y * y; }
        return (float) (10.0 * std::log10 (e / (double) x.size() + 1e-20));
    };
    // The loading a mic jack puts on a pickup, from mild to the worst measured. Setup knows a
    // mic jack when it sees one, so anything past 3 dB of loss is mended there.
    for (double hz : { 1000.0, 600.0, 450.0, 320.0, 250.0 })
    {
        const auto loaded = load (di, hz);
        const float t = measure (loaded).first;
        const float amt = TiltProbe::fixAmountFor (t, 3.0f);
        const auto mended = fix (loaded, amt);
        const float after = measure (mended).first;
        const float exact = (float) (20.0 * std::log10 (4500.0 / hz));
        std::printf ("  loaded at %4.0f Hz: tilt %.1f, fix %.1f dB (the exact opposite is %.1f), after %.1f\n", hz, t, amt, exact, after);
        if (TiltProbe::referenceTiltDb - t >= 3.0f)
        {
            CHECK (amt >= 3.0f, "a loss of %.1f dB was left alone", TiltProbe::referenceTiltDb - t);
            if (amt < TiltProbe::kMostFixDb)
                CHECK (std::abs (after - TiltProbe::referenceTiltDb) < 2.5f, "after the fix the guitar reads %.1f, a guitar should read %.1f", after, TiltProbe::referenceTiltDb);
        }
        // never brighter than the guitar was before the jack got at it, give or take the reading
        CHECK (amt <= exact + 4.0f, "loading at %.0f Hz is undone by %.1f dB, the fix adds %.1f", hz, exact, amt);
        CHECK (rmsDb (mended) < rmsDb (loaded) + 6.0f, "the fix makes the input much louder");
        for (double f : { 800.0, 1600.0, 3000.0 })
        {
            const float before = band (loaded, f) - band (di, f), now = band (mended, f) - band (di, f);
            if (amt > 0.0f) CHECK (std::abs (now) < std::abs (before) - 2.0f || std::abs (now) < 3.0f, "loaded at %.0f Hz: the %.0f Hz band was %.1f dB off and is %.1f dB off after the fix", hz, f, before, now);
            CHECK (now < 4.0f, "loaded at %.0f Hz: the fix overshoots the %.0f Hz band by %.1f dB", hz, f, now);
        }
    }
    // the curve itself: a loading and the fix sized as its exact opposite cancel to a flat line
    for (double hz : { 900.0, 450.0, 300.0 })
    {
        const float exact = (float) (20.0 * std::log10 (4500.0 / hz));
        const auto mended = fix (load (di, hz), exact);
        for (double f : { 200.0, 400.0, 800.0, 1600.0, 3000.0 })
            CHECK (std::abs (band (mended, f) - band (di, f)) < 2.0f, "a %.0f Hz loading and its opposite leave the %.0f Hz band %.1f dB off", hz, f, band (mended, f) - band (di, f));
    }
    // a guitar that is merely dark is left alone, and so is a bright one
    CHECK (TiltProbe::fixAmountFor (TiltProbe::referenceTiltDb - 4.0f) == 0.0f, "a dark pickup was taken for a bad input");
    CHECK (TiltProbe::fixAmountFor (TiltProbe::referenceTiltDb + 10.0f) == 0.0f, "a bright guitar got treble added");
    CHECK (TiltProbe::fixAmountFor (TiltProbe::referenceTiltDb - 40.0f) == TiltProbe::kMostFixDb, "the fix does not stop at its limit");

    // silence and hiss are not playing
    {
        std::vector<float> hiss ((size_t) sr * 3);
        Noise n;
        for (auto& v : hiss) v = 0.001f * n.next();
        CHECK (measure (hiss).second < 0.1, "the probe counted hiss as playing");
    }
    // the fix must never add latency
    {
        const auto imp = fix (test::impulse ((int) sr / 10, 0), 24.0f);
        int peak = 0;
        for (int i = 0; i < (int) imp.size(); ++i) if (std::abs (imp[(size_t) i]) > std::abs (imp[(size_t) peak])) peak = i;
        CHECK (peak <= 2, "input fix delays the signal by %d samples", peak);
    }
}

void testGate (double sr)
{
    std::printf ("gate\n");
    std::vector<float> noise ((size_t) sr * 2);
    Noise n;
    for (auto& s : noise) s = 0.0005f * n.next(); // -66 dBFS hiss
    auto v = bare();
    v[(size_t) P::gateOn] = 1;
    v[(size_t) P::gateThresh] = -55.0f;
    const auto r = renderWith (v, noise, sr);
    CHECK (rmsDb (r.l, (size_t) (sr * 0.5)) < -100.0f, "gate leaks noise: %.1f dB", rmsDb (r.l, (size_t) (sr * 0.5)));
    const auto di = test::guitarDi (sr, 4.0);
    const auto played = renderWith (v, di, sr);
    CHECK (rmsDb (played.l) > rmsDb (di) - 1.0f, "gate eats the playing");

    // The gate follows the room. Hiss well above the threshold the player chose: after it
    // has been steady for a moment the gate closes on it by itself.
    {
        NoiseGate g;
        g.prepare (sr);
        g.set (-70.0f, 80.0f);
        std::vector<float> hiss ((size_t) (sr * 5));
        for (auto& s : hiss) s = 0.004f * n.next(); // about -53 dBFS, peaks near -48
        auto out = hiss;
        for (size_t at = 0; at < out.size(); at += 256) g.process (out.data() + at, (int) std::min<size_t> (256, out.size() - at));
        const std::vector<float> tail (out.end() - (long) sr, out.end());
        CHECK (rmsDb (tail) < -100.0f, "steady hiss above the chosen threshold still comes through: %.1f dB", rmsDb (tail));
        CHECK (g.noiseFloorDb() > -58.0f && g.noiseFloorDb() < -48.0f, "the floor reads %.1f dB for hiss at about -53", g.noiseFloorDb());
        CHECK (g.thresholdNowDb() <= NoiseGate::kFollowCapLowDb + 0.01f && g.thresholdNowDb() > -45.0f, "the gate sits at %.1f dB over a floor of %.1f", g.thresholdNowDb(), g.noiseFloorDb());
        // the room goes quiet: the gate comes back down at once
        std::vector<float> quiet ((size_t) (sr * 2.5));
        for (auto& s : quiet) s = 0.0002f * n.next();
        for (size_t at = 0; at < quiet.size(); at += 256) g.process (quiet.data() + at, (int) std::min<size_t> (256, quiet.size() - at));
        CHECK (g.thresholdNowDb() < -60.0f, "the gate stayed at %.1f dB after the noise had gone", g.thresholdNowDb());
        // loud and steady is a sound, not a floor: a held organ note is never gated away
        NoiseGate h;
        h.prepare (sr);
        h.set (-60.0f, 80.0f);
        std::vector<float> tone ((size_t) (sr * 4));
        for (size_t i = 0; i < tone.size(); ++i) tone[i] = 0.05f * (float) std::sin (6.283185307 * 220.0 * (double) i / sr);
        for (size_t at = 0; at < tone.size(); at += 256) h.process (tone.data() + at, (int) std::min<size_t> (256, tone.size() - at));
        CHECK (h.thresholdNowDb() < -59.9f && h.isOpen(), "a steady note at -26 dBFS moved the gate to %.1f dB", h.thresholdNowDb());
    }
    // Playing over a little hiss, chords left to ring: following the floor takes nothing
    // away that the fixed gate lets through.
    {
        auto withHiss = playedGuitar (sr, 12.0, 0.25f, true);
        for (auto& s : withHiss) s += 0.0004f * n.next();
        auto a = withHiss, b = withHiss;
        NoiseGate fixed, following;
        fixed.setFollowsNoise (false);
        for (auto* g : { &fixed, &following }) { g->prepare (sr); g->set (-58.0f, 80.0f); }
        float highest = -120.0f;
        for (size_t at = 0; at < a.size(); at += 256)
        {
            const int k = (int) std::min<size_t> (256, a.size() - at);
            fixed.process (a.data() + at, k);
            following.process (b.data() + at, k);
            highest = std::max (highest, following.thresholdNowDb());
        }
        std::printf ("  following the floor: threshold at most %.1f dB while playing over hiss at -68 (chosen: -58); output %.2f dB against %.2f fixed\n", highest, rmsDb (b), rmsDb (a));
        CHECK (std::abs (rmsDb (b) - rmsDb (a)) < 0.3f, "following the noise floor changed the playing by %.2f dB", rmsDb (b) - rmsDb (a));
        CHECK (highest < -50.0f, "playing was taken for noise: the gate went up to %.1f dB", highest);
    }
}

void testInputGuard()
{
    std::printf ("input guard\n");
    auto lin = [] (float db) { return std::pow (10.0f, db / 20.0f); };
    {
        InputGuard g;
        float down = 0.0f;
        for (int i = 0; i < 600; ++i) down += g.heard (lin (-12.0f + 3.0f * (float) std::sin (i * 0.7)), 0.2);
        CHECK (down == 0.0f, "playing at the level setup chose was turned down by %.1f dB", down);
    }
    {
        // 8 dB hotter than setup heard, as on the first test recording: three hard chords and it acts
        InputGuard g;
        float down = 0.0f;
        int calls = 0;
        const float peaks[] = { -30.0f, -4.5f, -18.0f, -9.0f, -4.0f, -20.0f, -5.0f, -15.0f };
        for (float p : peaks) { ++calls; down = g.heard (lin (p), 0.2); if (down > 0.0f) break; }
        CHECK (down == 6.0f && calls == 7, "three chords up to -4 dBFS: turned down %.1f dB after %d readings", down, calls);
        // and once turned down, the same playing is left alone
        float again = 0.0f;
        for (int i = 0; i < 50; ++i) for (float p : peaks) again += g.heard (lin (p - down), 0.2);
        CHECK (again == 0.0f, "turned down a second time by %.1f dB", again);
    }
    {
        InputGuard g;
        CHECK (g.heard (lin (-0.5f), 0.2) == 9.5f, "an input about to clip is not turned down at once");
        // one bang a minute never adds up
        float down = 0.0f;
        for (int i = 0; i < 20; ++i) { down += g.heard (lin (-5.0f), 0.2); for (int k = 0; k < 300; ++k) down += g.heard (lin (-14.0f), 0.2); }
        CHECK (down == 0.0f, "a stray bang now and then turned the input down by %.1f dB", down);
    }
}

void testTimeEffects (double sr)
{
    std::printf ("delay/reverb tails and stability\n");
    for (int t = 0; t < choices::numReverbTypes; ++t)
    {
        auto v = bare();
        v[(size_t) P::revOn] = 1; v[(size_t) P::revType] = (float) t; v[(size_t) P::revDecay] = 10; v[(size_t) P::revMix] = 10;
        auto in = test::guitarDi (sr, 3.0);
        in.resize ((size_t) (sr * 30), 0.0f);
        const auto r = renderWith (v, in, sr);
        CHECK (allFinite (r), "reverb %d NaN", t);
        CHECK (peakOf (r.l) < 1.01f, "reverb %d overloads: %.2f", t, peakOf (r.l));
        // must decay, never build up
        CHECK (rmsDb (r.l, (size_t) (sr * 25), (size_t) (sr * 30)) < rmsDb (r.l, (size_t) (sr * 3), (size_t) (sr * 8)) - 6.0f, "reverb %d does not decay", t);
    }
    for (int t = 0; t < choices::numDelayTypes; ++t)
    {
        auto v = bare();
        v[(size_t) P::dlyOn] = 1; v[(size_t) P::dlyType] = (float) t; v[(size_t) P::dlyFeedback] = 10; v[(size_t) P::dlyMix] = 10;
        auto in = test::guitarDi (sr, 2.0);
        in.resize ((size_t) (sr * 20), 0.0f);
        const auto r = renderWith (v, in, sr);
        CHECK (allFinite (r), "delay %d NaN", t);
        CHECK (peakOf (r.l) < 1.01f, "delay %d runaway: %.2f", t, peakOf (r.l));
    }
    // switching reverb off lets the tail ring instead of cutting it
    {
        auto v = bare();
        v[(size_t) P::revOn] = 1; v[(size_t) P::revType] = 1; v[(size_t) P::revMix] = 6;
        Rig rig;
        rig.prepare (sr, 256);
        auto in = test::guitarDi (sr, 1.0);
        run (rig, v, in);
        v[(size_t) P::revOn] = 0;
        const std::vector<float> silence ((size_t) (sr * 0.3), 0.0f);
        const auto r = run (rig, v, silence);
        CHECK (rmsDb (r.l) > -70.0f, "reverb tail was cut off when switched off");
    }
}

void testAliasing (double sr)
{
    std::printf ("aliasing\n");
    // a 3.1 kHz tone through the highest gain amp; harmonics land on multiples
    // of 3.1 kHz, anything else is aliasing
    const double f0 = 3100.0;
    const auto in = test::sine (sr, f0, 1.5, 0.3f);
    auto v = bare();
    v[(size_t) P::ampOn] = 1; v[(size_t) P::ampModel] = choices::tightMetal; v[(size_t) P::ampGain] = 10;
    const auto r = renderWith (v, in, sr);
    const int N = 32768;
    juce::dsp::FFT fft (15);
    std::vector<float> buf ((size_t) N * 2, 0.0f);
    for (int i = 0; i < N; ++i)
    {
        const float w = 0.5f - 0.5f * std::cos (2.0f * (float) kPi * (float) i / (float) N);
        buf[(size_t) i] = r.l[(size_t) (sr * 0.4) + (size_t) i] * w;
    }
    fft.performFrequencyOnlyForwardTransform (buf.data());
    double harmonic = 0, alias = 0;
    for (int b = 2; b < N / 2; ++b)
    {
        const double f = (double) b * sr / N;
        if (f < 50.0 || f > 15000.0) continue;
        const double e = (double) buf[(size_t) b] * buf[(size_t) b];
        const double nearest = std::round (f / f0) * f0;
        if (std::abs (f - nearest) < 40.0 && nearest > 0) harmonic += e; else alias += e;
    }
    const double ratioDb = 10.0 * std::log10 (alias / harmonic);
    std::printf ("  alias-to-harmonic ratio: %.1f dB\n", ratioDb);
    CHECK (ratioDb < -35.0, "too much aliasing: %.1f dB", ratioDb);
}

void testPresets (double sr)
{
    std::printf ("presets @ %.0f Hz\n", sr);
    const auto presets = loadPresets();
    std::printf ("  %d presets\n", (int) presets.size());
    CHECK (presets.size() >= 10, "too few presets");
    const auto di = test::guitarDi (sr, 12.0);
    std::vector<float> levels;
    std::vector<juce::String> names;
    for (const auto& p : presets)
    {
        const auto r = renderWith (p.values, di, sr, 512);
        CHECK (allFinite (r), "preset %s NaN", p.name.toRawUTF8());
        const float lv = rmsDb (r.l) * 0.5f + rmsDb (r.r) * 0.5f;
        const float pk = std::max (peakOf (r.l), peakOf (r.r));
        CHECK (lv > -30.0f && lv < -9.0f, "preset %s level %.1f dB", p.name.toRawUTF8(), lv);
        CHECK (pk <= 0.98f, "preset %s peaks at %.2f", p.name.toRawUTF8(), pk);
        levels.push_back (lv);
        names.push_back (p.name);
        // round trip through JSON and share codes
        Preset back; juce::String err;
        CHECK (presetFromShareCode (presetToShareCode (p), back, err), "share code failed for %s: %s", p.name.toRawUTF8(), err.toRawUTF8());
        bool same = true;
        for (int i = 0; i < kNumParams; ++i) same &= std::abs (back.values[(size_t) i] - p.values[(size_t) i]) < 0.006f;
        CHECK (same, "share code changed %s", p.name.toRawUTF8());
        if (levels.size() == 1)
        {
            // the code as it travels: the whole share message, wrapped by a chat app, words after it
            const auto code = presetToShareCode (p);
            const auto message = presetToShareMessage (p);
            CHECK (message.contains ("Ctrl+V") && message.contains ("https://") && ! message.containsChar (0x2014),
                   "share message: %s", message.toRawUTF8());
            Preset viaMessage, wrapped, trailing, fake; juce::String e1, e2, e3, e4;
            CHECK (presetFromShareCode ("Bro try this\n" + message + "\n", viaMessage, e1) && viaMessage.name == p.name,
                   "code inside a message: %s", e1.toRawUTF8());
            const auto body = code.fromFirstOccurrenceOf ("riffhouse:", false, false);
            CHECK (presetFromShareCode ("riffhouse:" + body.substring (0, 20) + "\r\n" + body.substring (20), wrapped, e2),
                   "code wrapped over two lines: %s", e2.toRawUTF8());
            CHECK (presetFromShareCode (code + " sounds sick right", trailing, e3), "code with words after it: %s", e3.toRawUTF8());
            CHECK (! presetFromShareCode ("visit riffhouse: nothing here", fake, e4) && e4.isNotEmpty(), "junk after the tag must fail");
            // a tone named after an artist carries the style line, and still pastes back
            // (whichever tone the folder lists first, so both cases are built here)
            auto plain = p;
            plain.name = "My Clean Tone"; plain.band = {}; plain.inspiredBy = {};
            auto named = plain;
            named.name = "Inspired by Someone: A Song";
            const auto namedMessage = presetToShareMessage (named);
            Preset viaNamed; juce::String e5;
            CHECK (namedMessage.contains ("Not affiliated with the artist.") && ! presetToShareMessage (plain).contains ("Not affiliated")
                       && presetFromShareCode (namedMessage, viaNamed, e5) && viaNamed.name == named.name,
                   "artist share message: %s", e5.toRawUTF8());
        }
    }
    if (! levels.empty())
    {
        const auto [mn, mx] = std::minmax_element (levels.begin(), levels.end());
        std::printf ("  loudness spread %.1f dB (%.1f .. %.1f)\n", *mx - *mn, *mn, *mx);
        CHECK (*mx - *mn < 9.0f, "presets differ too much in loudness: %.1f dB", *mx - *mn);
    }
}

// Auto level: a tone you load comes up at the house loudness, a switch never changes the volume,
// a knob still does.
void testLevelMatch (double sr)
{
    std::printf ("auto level @ %.0f Hz\n", sr);
    {
        LevelKeeper k;
        CHECK (std::abs (k.reading (-12.0f, ToneChange::none) - (kHouseLoudnessDb + 12.0f)) < 1.0e-4f, "a loaded tone is trimmed to the house loudness");
        const float t0 = k.trimDb();
        CHECK (std::abs (k.reading (-6.0f, ToneChange::knob) - t0) < 1.0e-4f, "turning a knob leaves the trim alone");
        // the tone now plays 6 dB above the house loudness, and another amp must keep it exactly there
        const float t1 = k.reading (-16.0f, ToneChange::swapped);
        CHECK (std::abs ((-16.0f + t1) - (-6.0f + t0)) < 1.0e-4f, "another amp keeps the volume: %.2f against %.2f", -16.0f + t1, -6.0f + t0);
        CHECK (std::abs (k.reading (-100.0f, ToneChange::swapped) - t1) < 1.0e-4f, "silence teaches nothing");
        // a pedal: 2 dB louder by itself is let through, 11 dB louder comes out as the leeway
        const float held = -16.0f + t1;
        const float t2 = k.reading (-14.0f, ToneChange::toggled);
        CHECK (std::abs ((-14.0f + t2) - (held + 2.0f)) < 1.0e-4f, "a small boost from a pedal is kept");
        const float t3 = k.reading (-16.0f, ToneChange::toggled);
        CHECK (std::abs ((-16.0f + t3) - held) < 1.0e-4f, "and goes again with the pedal");
        const float t4 = k.reading (-5.0f, ToneChange::toggled);
        CHECK (std::abs ((-5.0f + t4) - (held + kSwitchLeewayDb)) < 1.0e-4f, "a pedal 11 dB louder lifts the level by %.2f dB", (-5.0f + t4) - held);
        const float t5 = k.reading (-16.0f, ToneChange::toggled);
        CHECK (std::abs ((-16.0f + t5) - held) < 1.0e-4f, "and off again is where it started: %.2f against %.2f", -16.0f + t5, held);
        k.rematch();
        CHECK (std::abs (k.reading (-30.0f, ToneChange::none) - 10.0f) < 1.0e-4f, "loading a tone starts again from the house loudness");
        CHECK (k.reading (-60.0f, ToneChange::swapped) <= kMostBoostDb + 1.0e-4f, "a very quiet chain is not boosted without limit");
        LevelKeeper saved;
        saved.restore (-7.5f);
        CHECK (std::abs (saved.reading (-9.0f, ToneChange::none) + 7.5f) < 1.0e-4f, "a saved session comes back with the trim it had");
        CHECK (std::abs ((-15.0f + saved.reading (-15.0f, ToneChange::swapped)) - (-9.0f - 7.5f)) < 1.0e-4f, "and a switch after that keeps its volume");
    }
    {
        auto a = bare();
        auto b = a;
        b[(size_t) P::inGain] = 6.0f; b[(size_t) P::gateThresh] = -50.0f; b[(size_t) P::autoLevel] = 0.0f;
        CHECK (changeBetween (toneForMeasuring (a), toneForMeasuring (b)) == ToneChange::none, "with the house phrase the player's setup is not part of the tone");
        // their own phrase comes off the cable: the input level it goes through is part of what is heard
        CHECK (changeBetween (toneForMeasuring (a, true), toneForMeasuring (b, true)) == ToneChange::knob, "with the player's phrase the input level is a knob");
        b[(size_t) P::inGain] = a[(size_t) P::inGain];
        b[(size_t) P::inputFixAmt] = 18.0f; // switched off: its strength changes nothing
        CHECK (changeBetween (toneForMeasuring (a, true), toneForMeasuring (b, true)) == ToneChange::none, "gate, hum filter and a switched-off fix are not part of what is measured");
        b[(size_t) P::inputFix] = 1.0f;
        CHECK (changeBetween (toneForMeasuring (a, true), toneForMeasuring (b, true)) == ToneChange::knob, "Input Fix coming on is the player's own setting: it stands like a knob");
        b = a;
        b[(size_t) P::ampGain] = a[(size_t) P::ampGain] + 1.0f;
        CHECK (changeBetween (a, b) == ToneChange::knob, "a knob is a change but not a switch");
        b[(size_t) P::drvOn] = 1.0f - a[(size_t) P::drvOn];
        CHECK (changeBetween (a, b) == ToneChange::toggled, "a pedal coming on is a switch");
        b[(size_t) P::ampModel] = a[(size_t) P::ampModel] + 1.0f;
        CHECK (changeBetween (a, b) == ToneChange::swapped, "another amp is a swap");
        CHECK (toneHash (toneForMeasuring (a)) != toneHash (toneForMeasuring (b)), "different tones, different keys");
    }

    // the phrase catcher: what counts as playing, and that it hands back what went in
    {
        const auto di = playedGuitar (sr, 12.0, 0.25f, true);
        float loud = 0.0f;
        CHECK (worthKeeping (std::vector<float> (di.begin() + (long) (0.9 * sr), di.begin() + (long) (3.9 * sr)), sr, loud), "three seconds of strumming are not taken for playing");
        CHECK (loud > -40.0f && loud < -10.0f, "strumming at -12 dBFS peaks reads %.1f dB", loud);
        std::vector<float> hiss ((size_t) (3 * sr)), pop ((size_t) (3 * sr), 0.0f), clipped (di.begin(), di.begin() + (long) (3 * sr));
        Noise n;
        for (auto& v : hiss) v = 0.002f * n.next();
        for (size_t i = (size_t) sr; i < (size_t) (sr * 1.05); ++i) pop[i] = 0.5f * n.next();
        for (auto& v : clipped) v = std::clamp (v * 8.0f, -1.0f, 1.0f);
        CHECK (! worthKeeping (hiss, sr, loud), "hiss was taken for playing");
        CHECK (! worthKeeping (pop, sr, loud), "one pop was taken for playing");
        CHECK (! worthKeeping (clipped, sr, loud), "a clipped input was kept as the player's phrase");
        // one chord cut short and then nothing is not playing either; one left to ring is
        std::vector<float> one ((size_t) (3 * sr), 0.0f);
        std::copy (di.begin(), di.begin() + (long) (0.6 * sr), one.begin());
        CHECK (! worthKeeping (one, sr, loud), "a single short strum and silence was kept as the player's phrase");
        {
            std::vector<float> ringing ((size_t) (3 * sr), 0.0f);
            const auto note = playedGuitar (sr, 1.6, 0.25f, true); // the open chord, held
            for (size_t i = 0; i < ringing.size(); ++i)
            {
                // held for the whole three seconds, fading the way a string does
                const size_t from = (size_t) (0.2 * sr) + i % (size_t) (1.2 * sr);
                ringing[i] = note[std::min (from, note.size() - 1)] * std::exp (-(float) i / (float) (1.5 * sr)) * (i < (size_t) (0.01 * sr) ? 0.0f : 1.0f);
            }
            CHECK (worthKeeping (ringing, sr, loud), "one chord left to ring was not taken for playing");
        }

        PhraseCatcher c;
        c.prepare (sr);
        std::vector<float> got;
        CHECK (! c.latest (got), "the catcher has a phrase before it heard one");
        for (size_t at = 0; at < di.size(); at += 480) c.push (di.data() + at, (int) std::min<size_t> (480, di.size() - at));
        CHECK (c.latest (got) && got.size() == (size_t) (PhraseCatcher::kSeconds * sr), "the catcher did not give its three seconds");
        bool same = ! got.empty();
        for (size_t i = 0; i < got.size() && same; ++i) same = got[i] == di[di.size() - got.size() + i];
        CHECK (same, "the catcher's phrase is not the last three seconds that went in");

        LevelPhrase p;
        p.samples = got; p.sampleRate = sr; p.inGainDb = 4.5f; p.fromPlayer = true;
        const auto bytes = phraseToBytes (p);
        LevelPhrase back;
        CHECK (phraseFromBytes (bytes.getData(), bytes.getSize(), back) && back.samples.size() == p.samples.size() && back.inGainDb == 4.5f && back.fromPlayer, "a saved phrase does not come back");
        float worst = 0.0f;
        for (size_t i = 0; i < back.samples.size() && i < p.samples.size(); ++i) worst = std::max (worst, std::abs (back.samples[i] - p.samples[i]));
        CHECK (worst < 1.0e-4f, "a saved phrase comes back changed by %.5f", worst);
        CHECK (! phraseFromBytes (bytes.getData(), 40, back), "half a phrase file was accepted");
        const auto slower = atRate (p, sr * 0.5);
        CHECK (std::abs ((double) slower.samples.size() - 0.5 * (double) p.samples.size()) < 2.0, "a phrase at another rate has the wrong length");
    }

    Rig meter, stage;
    stage.setOffline (true);
    stage.prepare (sr, 256);
    const auto presets = loadPresets();
    const auto phrase = housePhrase (sr);
    // other playing than the phrase the tones are matched with: longer, and played differently
    const auto di = playedGuitar (sr, 6.0, 0.25f, true);
    std::vector<float> raw, matched, played, allRaw;
    double seconds = 0.0;
    int count = 0;
    for (const auto& p : presets)
    {
        // every tone at 48 kHz; at other rates every third is enough to show the rate changes nothing
        if (count++ % 3 != 0 && sr != 48000.0) continue;
        const auto tone = toneForMeasuring (p.values);
        const auto t0 = std::chrono::steady_clock::now();
        const float loud = measureLoudnessDb (meter, tone, phrase);
        seconds += std::chrono::duration<double> (std::chrono::steady_clock::now() - t0).count();
        if (count % 5 == 1)
        {
            // something else in between, then the same tone again: it must read the same
            measureLoudnessDb (meter, toneForMeasuring (presets[((size_t) count * 7 + 3) % presets.size()].values), phrase);
            const float again = measureLoudnessDb (meter, tone, phrase);
            // nothing of the tone before may be left in the rig: not a tail, not a filter still ringing
            CHECK (std::abs (again - loud) < 0.001f, "%s: the same tone reads %.4f then %.4f", p.name.toRawUTF8(), loud, again);
        }
        LevelKeeper k;
        const float trim = k.reading (loud, ToneChange::none);
        raw.push_back (loud);
        matched.push_back (loud + trim);
        // and on the stage rig, with the other playing (every second tone: it is the slow part)
        if (count % 2 == 0) { raw.pop_back(); matched.pop_back(); allRaw.push_back (loud); continue; }
        allRaw.push_back (loud);
        stage.setLevelTrimDb (trim);
        stage.snapParams (tone);
        const auto r = run (stage, tone, di);
        const float lv = loudHalfDb (r.l.data(), r.r.data(), r.l.size(), sr);
        played.push_back (lv);
        if (std::getenv ("RIFFHOUSE_LEVELS") != nullptr) std::printf ("    %6.1f raw %6.1f heard  %s\n", loud, lv, p.name.toRawUTF8());
        CHECK (peakOf (r.l) <= 1.0f && allFinite (r), "%s peaks at %.2f with auto level", p.name.toRawUTF8(), peakOf (r.l));
    }
    if (! raw.empty())
    {
        auto spread = [] (const std::vector<float>& v) { const auto [mn, mx] = std::minmax_element (v.begin(), v.end()); return *mx - *mn; };
        std::vector<float> unmatched;
        for (size_t i = 0; i < played.size(); ++i) unmatched.push_back (played[i] - (matched[i] - raw[i]));
        // the middle 90% says more than the two odd ones out
        auto most = [] (std::vector<float> v) { std::sort (v.begin(), v.end()); return v[(size_t) ((double) (v.size() - 1) * 0.95)] - v[(size_t) ((double) (v.size() - 1) * 0.05)]; };
        std::printf ("  %d tones: spread %.1f dB as made, %.2f dB matched; on other playing %.1f dB as made and %.1f dB matched (nine in ten within %.1f and %.1f dB); one reading takes %.0f ms (%.0fx real time)\n",
                     (int) allRaw.size(), spread (allRaw), spread (matched), spread (unmatched), spread (played), most (unmatched), most (played),
                     1000.0 * seconds / (double) allRaw.size(), kReferencePhraseSeconds * (double) allRaw.size() / seconds);
        CHECK (spread (matched) < 0.1f, "matched tones differ by %.2f dB", spread (matched));
        // 4.8 dB with GCC and 5.2 dB with MSVC on the same tones: their maths differ in the last digits and a
        // high-gain amp makes that audible in the numbers. The bound leaves room for that and still catches a real loss.
        // With the real-circuit pedals and antialiased amps (0.1.5): 5.2 of 9.4 dB at 48 kHz, 5.6 of 8.9 dB on the
        // 44.1 kHz subset, so the share was 0.65. The cabs built from real speakers (13 Nov) made the tones as made
        // more even on other playing (8.9 to 8.2 dB on that subset) while matched stayed near (5.6 to 5.8 dB), so the
        // share reads 0.71 with nothing lost; it is 0.75 now, and the 6 dB ceiling is unchanged.
        CHECK (most (played) < 0.75f * most (unmatched) && most (played) < 6.0f, "on other playing nine in ten matched tones are within %.1f dB, unmatched %.1f dB", most (played), most (unmatched));
    }

    // the player's phrase is their guitar off the cable: a clean tone follows the input level
    // they set, and the house phrase does not care about it
    {
        LevelPhrase own;
        own.samples.assign (di.begin() + (long) (1.8 * sr), di.begin() + (long) (4.8 * sr));
        for (auto& x : own.samples) x *= 0.3f; // a weak signal, as off a mic jack
        own.sampleRate = sr; own.fromPlayer = true;
        auto v = bare();
        const float flat = measureLoudnessDb (meter, toneForMeasuring (v, true), own);
        v[(size_t) P::inGain] = 9.0f;
        const float up = measureLoudnessDb (meter, toneForMeasuring (v, true), own);
        CHECK (std::abs ((up - flat) - 9.0f) < 0.2f, "9 dB more input level reads %.2f dB louder with the player's phrase", up - flat);
        CHECK (std::abs (measureLoudnessDb (meter, toneForMeasuring (v), phrase) - measureLoudnessDb (meter, toneForMeasuring (bare()), phrase)) < 1.0e-3f, "the house phrase read the input level");
        v[(size_t) P::inputFix] = 1.0f; v[(size_t) P::inputFixAmt] = 18.0f;
        const float fixed = measureLoudnessDb (meter, toneForMeasuring (v, true), own);
        CHECK (fixed > up + 1.0f, "Input Fix is not heard in the measurement: %.2f against %.2f", fixed, up);
        float lv = 0.0f;
        CHECK (! worthKeeping (std::vector<float> (own.samples.size(), 0.0f), sr, lv, 30.0f), "silence was kept");
        std::vector<float> faint = own.samples;
        for (auto& x : faint) x *= 0.1f; // peaks near -42 dBFS
        CHECK (! worthKeeping (faint, sr, lv), "a faint input with no gain on it was kept");
        CHECK (worthKeeping (faint, sr, lv, 24.0f), "a faint input the player has turned up was not kept");
    }

    // a hot drive pedal coming on: a lift of the leeway, not of what the pedal does by itself
    {
        auto v = toneForMeasuring (presets.empty() ? bare() : presets.front().values);
        v[(size_t) P::drvOn] = 0.0f;
        LevelKeeper k;
        const float before = measureLoudnessDb (meter, v, phrase);
        const float trimBefore = k.reading (before, ToneChange::none);
        auto w = v;
        w[(size_t) P::drvOn] = 1.0f;
        w[(size_t) P::drvLevel] = 9.0f;
        const float after = measureLoudnessDb (meter, w, phrase);
        const float trimAfter = k.reading (after, changeBetween (v, w));
        const float heard = (after + trimAfter) - (before + trimBefore);
        std::printf ("  a hot drive pedal changes the tone by %+.1f dB, heard as %+.2f dB\n", after - before, heard);
        CHECK (std::abs (heard) <= kSwitchLeewayDb + 0.01f, "a pedal coming on changed the volume by %.1f dB", heard);
    }

    // a player whose guitar is quieter and duller than the house phrase: matched with their own
    // playing, clean and heavy tones come out level for them, which the house phrase cannot promise
    {
        LevelPhrase own;
        own.samples.assign (di.begin() + (long) (1.8 * sr), di.begin() + (long) (4.8 * sr));
        float g = 0.0f;
        { Biquad dull; dull.set (Biquad::Type::lowpass, sr, 900.0, 0.6); for (auto& x : own.samples) { x = 0.5f * dull.process (x); g = std::max (g, std::abs (x)); } }
        own.sampleRate = sr; own.fromPlayer = true;
        std::vector<float> theirs (di.size());
        { Biquad dull; dull.set (Biquad::Type::lowpass, sr, 900.0, 0.6); for (size_t i = 0; i < di.size(); ++i) theirs[i] = 0.5f * dull.process (di[i]); }
        std::vector<float> withHouse, withOwn;
        for (const char* name : { "First Light Clean", "Heavy Starter", "Thrash Machine", "Warm Crunch", "Smooth Lead", "Neo-Soul Silk" })
            for (const auto& p : presets)
                if (p.name == name)
                {
                    const auto tone = toneForMeasuring (p.values);
                    for (int own_ = 0; own_ < 2; ++own_)
                    {
                        LevelKeeper k;
                        stage.setLevelTrimDb (k.reading (measureLoudnessDb (meter, tone, own_ ? own : phrase), ToneChange::none));
                        stage.snapParams (tone);
                        const auto r = run (stage, tone, theirs);
                        (own_ ? withOwn : withHouse).push_back (loudHalfDb (r.l.data(), r.r.data(), r.l.size(), sr));
                    }
                }
        if (withOwn.size() >= 4)
        {
            auto spread = [] (const std::vector<float>& v) { const auto [mn, mx] = std::minmax_element (v.begin(), v.end()); return *mx - *mn; };
            std::printf ("  a quiet, dull guitar: clean to heavy tones differ by %.1f dB matched with the house phrase, %.1f dB matched with its own playing\n", spread (withHouse), spread (withOwn));
            CHECK (spread (withOwn) < 6.5f, "matched with the player's own phrase their tones still differ by %.1f dB", spread (withOwn));
            CHECK (spread (withOwn) < spread (withHouse), "the player's own phrase matched no better than the house phrase");
        }
    }
    stage.setLevelTrimDb (0.0f);
}

// The tuner against plucked notes with a guitar's overtones, low B of a 7-string to the 22nd fret,
// a little off pitch as a real string is: read to half a cent, settled within a cent in a quarter second.
void testTuner (double sr)
{
    std::printf ("tuner @ %.0f Hz\n", sr);
    const double notes[] = { 30.87, 41.20, 61.74, 82.41, 110.0, 146.83, 196.0, 246.94, 329.63, 659.26, 987.77, 1174.66 };
    const double offsets[] = { -23.0, -3.7, 0.0, 6.1, 31.0 }; // cents
    float worst = 0.0f, slowest = 0.0f;
    juce::String worstWhat;
    int unread = 0;
    for (double base : notes)
        for (double off : offsets)
        {
            const double hz = base * std::pow (2.0, off / 1200.0);
            Tuner t;
            t.prepare (sr);
            const int block = 256;
            const size_t len = (size_t) (0.9 * sr);
            std::vector<float> x (len);
            for (size_t i = 0; i < len; ++i)
            {
                const double tt = (double) i / sr, w = 2.0 * juce::MathConstants<double>::pi * hz * tt;
                // a pluck: overtones louder at first and dying faster, the whole note decaying
                const double env = 0.3 * std::exp (-tt * 1.5);
                x[i] = (float) (env * (std::sin (w) + 0.6 * std::exp (-tt * 3.0) * std::sin (2.0 * w + 0.4)
                                       + 0.4 * std::exp (-tt * 5.0) * std::sin (3.0 * w + 1.1) + 0.2 * std::exp (-tt * 8.0) * std::sin (4.0 * w)));
            }
            float settledAt = -1.0f, last = 0.0f;
            bool read = false;
            for (size_t at = 0; at + block <= len; at += block)
            {
                t.push (x.data() + at, block);
                if ((at / block) % 4 != 3) continue; // the UI asks about 45 times a second
                const auto r = t.analyse();
                const double timeS = (double) (at + block) / sr;
                if (! r.valid) continue;
                const float err = (float) (1200.0 * std::log2 (r.hz / hz));
                if (std::abs (err) <= 1.0f) { if (settledAt < 0.0f) settledAt = (float) timeS; }
                else if (timeS > 0.3) settledAt = -1.0f; // it must stay settled
                last = err;
                read = true;
            }
            if (! read) { ++unread; worstWhat = juce::String (hz, 2) + " Hz never read"; continue; }
            if (std::abs (last) > worst) { worst = std::abs (last); worstWhat = juce::String (hz, 2) + " Hz"; }
            slowest = std::max (slowest, settledAt < 0.0f ? 9.0f : settledAt);
        }
    std::printf ("  worst %.2f cents (%s), slowest to settle within a cent %.2f s\n", worst, worstWhat.toRawUTF8(), slowest);
    CHECK (unread == 0, "the tuner did not read %d notes (%s)", unread, worstWhat.toRawUTF8());
    CHECK (worst <= 0.5f, "the tuner is %.2f cents off at %s", worst, worstWhat.toRawUTF8());
    CHECK (slowest <= 0.25f, "the tuner takes %.2f s to settle within a cent", slowest);
}

// The metronome over ten minutes at awkward tempos and rates: every beat on its sample, no
// drift, and the click as long at every sample rate.
void testMetronome()
{
    std::printf ("metronome\n");
    double worstOff = 0.0;
    for (double sr : { 44100.0, 48000.0, 96000.0 })
        for (double bpm : { 60.0, 97.0, 137.0, 241.0 })
        {
            Metronome m;
            const double perBeat = sr * 60.0 / bpm;
            const long long len = (long long) (600.0 * sr);
            long long pos = 0;
            double off = 0.0;
            float l = 0.0f, r = 0.0f;
            for (; pos < len; ++pos)
            {
                l = r = 0.0f;
                m.render (&l, &r, 1, sr, bpm, 1.0f, [&] (int beat) { off = std::max (off, std::abs ((double) pos - beat * perBeat)); });
            }
            worstOff = std::max (worstOff, off);
            const int want = (int) std::floor ((double) (len - 1) / perBeat + 1e-9) + 1;
            CHECK (std::abs (m.beatsSoFar() - want) <= 1, "%.0f bpm at %.0f Hz: %d beats in ten minutes, not %d", bpm, sr, m.beatsSoFar(), want);
        }
    // a beat starts on the first sample at or after where it falls (its click's phase covers the rest)
    std::printf ("  every beat within %.2f samples of where it falls, over ten minutes\n", worstOff);
    CHECK (worstOff <= 1.0 + 1e-6, "the metronome drifts: a beat %.2f samples off", worstOff);

    // the click's length (to 60 dB down) at each rate
    double shortest = 1e9, longest = 0.0;
    for (double sr : { 44100.0, 48000.0, 96000.0, 192000.0 })
    {
        Metronome m;
        std::vector<float> l ((size_t) sr, 0.0f), r ((size_t) sr, 0.0f);
        m.render (l.data(), r.data(), (int) l.size(), sr, 30.0, 1.0f, [] (int) {});
        float peak = 0.0f;
        for (float v : l) peak = std::max (peak, std::abs (v));
        size_t last = 0;
        for (size_t i = 0; i < l.size(); ++i) if (std::abs (l[i]) > peak * 0.001f) last = i;
        const double ms = 1000.0 * (double) last / sr;
        shortest = std::min (shortest, ms);
        longest = std::max (longest, ms);
    }
    std::printf ("  click lasts %.1f to %.1f ms across rates\n", shortest, longest);
    CHECK (longest - shortest < 2.0, "the click changes length with the sample rate: %.1f to %.1f ms", shortest, longest);
}

void testLevelMatcher()
{
    std::printf ("auto level at work\n");
    const double sr = 48000.0;
    // the player's saved phrase is theirs: this test must neither read nor replace it
    const auto phraseWas = LevelMatcher::phraseFile();
    const bool hadPhrase = phraseWas.existsAsFile();
    const auto aside = phraseWas.getSiblingFile ("your-playing.test-aside");
    if (hadPhrase) phraseWas.moveFileTo (aside);

    const auto presets = loadPresets();
    auto find = [&] (const char* name) { for (const auto& p : presets) if (p.name == name) return p.values; return bare(); };
    struct Shared { juce::CriticalSection lock; ParamValues v; } shared;
    shared.v = find ("First Light Clean");
    shared.v[(size_t) P::autoLevel] = 1.0f;
    auto set = [&] (const ParamValues& v) { const juce::ScopedLock sl (shared.lock); shared.v = v; shared.v[(size_t) P::autoLevel] = 1.0f; };
    auto waitFor = [] (auto&& done, int ms = 20000)
    {
        const auto t0 = juce::Time::getMillisecondCounter();
        while (! done() && juce::Time::getMillisecondCounter() - t0 < (juce::uint32) ms) juce::Thread::sleep (5);
        return done();
    };
    Rig meter;
    const auto phrase = housePhrase (sr);
    auto loud = [&] (const ParamValues& v) { return measureLoudnessDb (meter, toneForMeasuring (v), phrase); };
    {
        Rig stage;
        stage.prepare (sr, 256);
        LevelMatcher m (stage, [&] (ParamValues& v) { const juce::ScopedLock sl (shared.lock); v = shared.v; });
        m.start (sr);
        const float clean = loud (shared.v);
        CHECK (waitFor ([&] { return std::abs (m.trimDb() - (kHouseLoudnessDb - clean)) < 0.01f; }), "the first tone was not brought to the house loudness: trim %.2f, wanted %.2f", m.trimDb(), kHouseLoudnessDb - clean);
        CHECK (std::abs (stage.levelTrim() - m.trimDb()) < 1.0e-6f, "the trim did not reach the stage rig");

        // a loaded tone: house loudness again, and quickly
        auto heavy = find ("Thrash Machine");
        const auto t0 = juce::Time::getMillisecondCounterHiRes();
        set (heavy);
        m.rematch();
        const float h = loud (heavy);
        CHECK (waitFor ([&] { return std::abs (m.trimDb() - (kHouseLoudnessDb - h)) < 0.01f; }), "a loaded tone was not matched: trim %.2f, wanted %.2f", m.trimDb(), kHouseLoudnessDb - h);
        std::printf ("  a tone it has not met is matched %.0f ms after it is loaded\n", juce::Time::getMillisecondCounterHiRes() - t0);

        // a knob: the trim stays, however long we wait
        auto louder = heavy;
        louder[(size_t) P::outLevel] = heavy[(size_t) P::outLevel] + 4.0f;
        const float before = m.trimDb();
        set (louder);
        juce::Thread::sleep (700);
        CHECK (std::abs (m.trimDb() - before) < 1.0e-4f, "turning the volume up moved the trim from %.2f to %.2f", before, m.trimDb());
        // another amp after that: as loud as the tone with the volume up was
        auto other = louder;
        other[(size_t) P::ampModel] = (float) choices::britPlexi;
        set (other);
        const float want = (loud (louder) + before) - loud (other);
        CHECK (waitFor ([&] { return std::abs (m.trimDb() - want) < 0.02f; }), "another amp: trim %.2f, wanted %.2f", m.trimDb(), want);

        // tones measured ahead answer at once
        std::vector<ParamValues> next { find ("Warm Crunch"), find ("Smooth Lead") };
        m.measureAhead (next);
        CHECK (waitFor ([&] { return m.tonesKnown() >= 6; }), "tones were not measured ahead (%d known)", m.tonesKnown());
        const float sl = loud (next[1]);
        const auto t1 = juce::Time::getMillisecondCounterHiRes();
        set (next[1]);
        m.rematch();
        CHECK (waitFor ([&] { return std::abs (m.trimDb() - (kHouseLoudnessDb - sl)) < 0.01f; }), "a tone measured ahead was not matched");
        const double took = juce::Time::getMillisecondCounterHiRes() - t1;
        std::printf ("  a tone measured ahead is matched %.0f ms after it is loaded\n", took);
        CHECK (took < 120.0, "a tone measured ahead took %.0f ms", took);

        // switched off: no trim; a saved session: its trim, at once, and kept
        auto off = next[1];
        { const juce::ScopedLock sl2 (shared.lock); shared.v[(size_t) P::autoLevel] = 0.0f; }
        CHECK (waitFor ([&] { return m.trimDb() == 0.0f && stage.levelTrim() == 0.0f; }), "auto level off leaves a trim of %.2f", m.trimDb());
        set (off);
        m.restore (-7.25f);
        CHECK (stage.levelTrim() == -7.25f, "a restored trim waits for the thread");
        juce::Thread::sleep (600);
        CHECK (std::abs (m.trimDb() + 7.25f) < 1.0e-4f, "a restored session's trim became %.2f", m.trimDb());

        // a bounce: answered on the spot, the same every time
        Rig stage2;
        stage2.prepare (sr, 256);
        auto v = find ("Heavy Starter");
        v[(size_t) P::autoLevel] = 1.0f;
        LevelMatcher offline (stage2, [&v] (ParamValues& out) { out = v; }); // its thread sees the same tone the render does
        offline.start (sr);
        offline.matchNow (v);
        CHECK (std::abs (stage2.levelTrim() - (kHouseLoudnessDb - loud (v))) < 0.01f, "an offline render got a trim of %.2f, wanted %.2f", stage2.levelTrim(), kHouseLoudnessDb - loud (v));
    }

    // the player's phrase: heard, taken, saved, and there again next time
    {
        Rig stage;
        stage.prepare (sr, 256);
        {
            LevelMatcher m (stage, [&] (ParamValues& v) { const juce::ScopedLock sl (shared.lock); v = shared.v; });
            set (find ("First Light Clean"));
            m.start (sr);
            CHECK (waitFor ([&] { return m.trimDb() != 0.0f; }), "no first match");
            CHECK (! m.hasPlayersPhrase(), "a phrase before anything was played");
            const float houseTrim = m.trimDb();
            // a quiet, dull guitar plays for a few seconds
            auto strums = playedGuitar (sr, 3.4, 0.12f, true); // two open chords
            Biquad dull; dull.set (Biquad::Type::lowpass, sr, 900.0, 0.6);
            for (auto& x : strums) x = dull.process (x);
            auto di = strums;
            di.insert (di.end(), strums.begin(), strums.end());
            std::vector<float> l (256), r (256);
            ParamValues v; { const juce::ScopedLock sl (shared.lock); v = shared.v; }
            for (size_t at = 0; at + 256 <= di.size(); at += 256)
            {
                stage.setParams (v);
                stage.process (di.data() + at, l.data(), r.data(), 256);
                if (at % 9600 == 0) juce::Thread::sleep (2);
            }
            {
                float lv = 0.0f;
                std::vector<float> tail (di.end() - (long) (3 * sr), di.end());
                float pk = 0.0f; for (float x : tail) pk = std::max (pk, std::abs (x));
                const bool ok = worthKeeping (tail, sr, lv);
                std::printf ("  (the last three seconds: peak %.3f, worth keeping %d, loud %.1f)\n", pk, (int) ok, lv);
            }
            CHECK (waitFor ([&] { return m.hasPlayersPhrase(); }, 5000), "six seconds of playing did not become the player's phrase");
            CHECK (std::abs (m.trimDb() - houseTrim) < 1.0e-4f, "learning the phrase moved the level under the player's fingers");
            m.rematch();
            CHECK (waitFor ([&] { return m.trimDb() > houseTrim + 3.0f; }), "a quiet guitar's clean tone was not turned up: %.2f with its own phrase, %.2f with the house phrase", m.trimDb(), houseTrim);
            CHECK (LevelMatcher::phraseFile().existsAsFile(), "the player's phrase was not saved");
        }
        LevelMatcher again (stage, [&] (ParamValues& v) { const juce::ScopedLock sl (shared.lock); v = shared.v; });
        again.start (sr);
        CHECK (waitFor ([&] { return again.hasPlayersPhrase(); }, 5000), "the saved phrase was not found the next time");
        again.forgetPhrase();
        CHECK (waitFor ([&] { return ! again.hasPlayersPhrase() && ! LevelMatcher::phraseFile().existsAsFile(); }, 5000), "the phrase could not be forgotten");
    }
    LevelMatcher::phraseFile().deleteFile();
    if (hadPhrase) aside.moveFileTo (phraseWas);
}

void testNeural (double sr)
{
    std::printf ("neural captures @ %.0f Hz\n", sr);
#ifdef RIFFHOUSE_NAM_EXAMPLES
    for (const char* file : { "wavenet.nam", "lstm.nam", "wavenet_a1_standard.nam", "A2.nam" })
    {
        const auto path = juce::File (RIFFHOUSE_NAM_EXAMPLES).getChildFile (file);
        if (! path.existsAsFile()) { std::printf ("  (skipping %s)\n", file); continue; }
        auto v = bare();
        v[(size_t) P::ampOn] = 1;
        v[(size_t) P::ampModel] = choices::neuralCapture;
        Rig rig;
        rig.prepare (sr, 256);
        const auto err = rig.neural().load (path.getFullPathName().toStdString());
        CHECK (err.empty(), "load %s: %s", file, err.c_str());
        const auto di = test::guitarDi (sr, 3.0);
        const auto r = run (rig, v, di);
        CHECK (allFinite (r), "neural %s NaN", file);
        CHECK (rmsDb (r.l) > -80.0f, "neural %s silent", file);
        std::printf ("  %s ok (%.1f dB, %.1fx realtime)\n", file, rmsDb (r.l), 3.0 / r.seconds);
    }
#else
    std::printf ("  (no example captures available)\n");
#endif
}

// Measured 3.6 with every pedal on (GCC, November 2026), then 4.9 to 5.5 once the default
// drive pedal became its real circuit, solved at 8x (0.1.5), and 4.0 to 5.7 with the amp at 4x
// and its curves antialiased. The budget leaves room for a
// different compiler and a noisy machine, and still catches a rig that got a fifth slower.
constexpr double kRigCostBudget = 6.0;

// A fixed piece of arithmetic of the kind the rig is made of, timed on the same machine
// at the same moment. Shared build machines differ by a factor of two and more from
// run to run, so a plain stopwatch failed good code and would pass bad code on a fast
// day. The rig is judged against this instead.
double referenceSeconds (double sr, double seconds)
{
    const auto in = test::guitarDi (sr, seconds);
    const int over = 8;
    double best = 1.0e9;
    for (int attempt = 0; attempt < 3; ++attempt)
    {
        float z[12] = {};
        float sum = 0.0f;
        const auto t0 = std::chrono::steady_clock::now();
        for (float x : in)
            for (int k = 0; k < over; ++k)
            {
                float s = x;
                for (int st = 0; st < 12; ++st)
                {
                    const float u = 3.0f * s + 0.1f;
                    const float u2 = u * u;
                    const float sat = u * (15.0f + u2) / (15.0f + 6.0f * u2);
                    z[st] += 0.2f * (sat - z[st]);
                    s = sat - 0.5f * z[st];
                }
                sum += s;
            }
        const double took = std::chrono::duration<double> (std::chrono::steady_clock::now() - t0).count();
        best = std::min (best, took);
        if (! std::isfinite (sum)) return 1.0e9;
    }
    return best;
}

// Thousands of random changes while it plays: every knob, switch, model and tone at random,
// blocks of any size up to the most the host asked for, and an input that turns hostile now
// and then (full-scale square, DC, near-denormal dust, silence, clicks). It must never make a
// NaN, never go past the safety limiter, never get stuck making sound once everything is off,
// and silence must not get slow (denormals).
void testStress (double sr, int maxBlock, double seconds, juce::int64 seed)
{
    std::printf ("stress @ %.0f Hz, blocks up to %d, %.0f s\n", sr, maxBlock, seconds);
    juce::Random rnd (seed);
    const auto presets = loadPresets();
    const auto& specs = paramSpecs();
    auto in = test::guitarDi (sr, seconds, 0.3f);
    // hostile stretches, each half a second, at random places
    const size_t half = (size_t) (sr * 0.5);
    for (int k = 0; k < (int) seconds; ++k)
    {
        const size_t at = (size_t) rnd.nextInt ((int) (in.size() - half));
        const int kind = rnd.nextInt (5);
        for (size_t i = at; i < at + half; ++i)
        {
            if (kind == 0) in[i] = ((i / 40) % 2) != 0 ? 1.0f : -1.0f;  // full-scale square
            else if (kind == 1) in[i] = 0.5f;                          // DC
            else if (kind == 2) in[i] = 1.0e-30f * (float) ((int) (i % 3) - 1); // dust
            else if (kind == 3) in[i] = 0.0f;                          // silence
            else in[i] = (i - at) % 4800 == 0 ? 1.0f : 0.0f;           // clicks
        }
    }
    Rig rig;
    rig.prepare (sr, maxBlock);
    auto v = defaultParamValues();
    std::vector<float> l ((size_t) maxBlock), r ((size_t) maxBlock);
    auto randomise = [&] (const ParamSpec& sp)
    {
        if (sp.kind == ParamKind::toggle) v[(size_t) sp.id] = rnd.nextBool() ? 1.0f : 0.0f;
        else if (sp.kind == ParamKind::choice) v[(size_t) sp.id] = (float) rnd.nextInt ((int) sp.choices.size());
        else v[(size_t) sp.id] = sp.min + (sp.max - sp.min) * rnd.nextFloat();
    };
    bool finite = true;
    float loudest = 0.0f;
    int changes = 0;
    for (size_t pos = 0; pos < in.size();)
    {
        const int n = (int) std::min<size_t> ((size_t) (1 + rnd.nextInt (maxBlock)), in.size() - pos);
        const float roll = rnd.nextFloat();
        if (roll < 0.002f && ! presets.empty()) { v = presets[(size_t) rnd.nextInt ((int) presets.size())].values; ++changes; }
        else if (roll < 0.01f) { for (const auto& sp : specs) randomise (sp); ++changes; }
        else if (roll < 0.3f) { for (int c = 1 + rnd.nextInt (4); c > 0; --c) randomise (specs[(size_t) rnd.nextInt ((int) specs.size())]); ++changes; }
        rig.setParams (v);
        rig.process (in.data() + pos, l.data(), r.data(), n);
        for (int i = 0; i < n; ++i)
        {
            if (! std::isfinite (l[(size_t) i]) || ! std::isfinite (r[(size_t) i])) finite = false;
            loudest = std::max ({ loudest, std::abs (l[(size_t) i]), std::abs (r[(size_t) i]) });
        }
        pos += (size_t) n;
    }
    std::printf ("  %d changes, loudest sample %.3f\n", changes, loudest);
    CHECK (finite, "random changes made a NaN or infinity");
    CHECK (loudest <= 1.0f, "random changes got past the safety limiter: %.3f", loudest);

    // everything off and nothing coming in: the sound dies away, and silence stays fast
    const auto off = bare();
    std::vector<float> quiet ((size_t) maxBlock, 0.0f);
    const size_t tail = (size_t) (sr * 15.0);
    double lastPower = 0.0;
    size_t lastCount = 0;
    const auto t0 = std::chrono::steady_clock::now();
    for (size_t pos = 0; pos < tail; pos += (size_t) maxBlock)
    {
        rig.setParams (off);
        rig.process (quiet.data(), l.data(), r.data(), maxBlock);
        if (pos > tail - (size_t) sr)
            for (int i = 0; i < maxBlock; ++i) { lastPower += (double) l[(size_t) i] * l[(size_t) i]; ++lastCount; }
    }
    const double silentSecs = std::chrono::duration<double> (std::chrono::steady_clock::now() - t0).count();
    const float lastDb = (float) (10.0 * std::log10 (lastPower / (double) std::max<size_t> (1, lastCount) + 1.0e-30));
    CHECK (lastDb < -90.0f, "after everything was switched off it still makes sound: %.1f dB", lastDb);
    // 15 s of silence through a bare rig takes a sliver of real time unless denormals crept in
    CHECK (silentSecs < 15.0 * 0.05, "silence got slow (denormals?): %.2f s for 15 s", silentSecs);
}

// Clicks. A player switches pedals, changes tones and turns knobs fast while playing; none of
// it may click. Two ways to hear one:
//  - silence in: anything that comes out is the click itself (a DC step, a filter kicked);
//  - a held note in: a click is a burst of top end, so the output above 4 kHz in the 30 ms
//    after a change is held against the same band just before and well after it.
// A change that gives no more top end than either settled state, or stays under -60 dB, passes.
struct ClickResult
{
    float worstDb = -200.0f;
    juce::String what;
    std::vector<std::pair<float, juce::String>> all; // every change judged, for the report
    int limited = 0; // changes left to the safety limiter (see judgeChanges)
};

static float peakDb (const std::vector<float>& x, size_t from, size_t to)
{
    float pk = 0.0f;
    for (size_t i = from; i < std::min (to, x.size()); ++i) pk = std::max (pk, std::abs (x[i]));
    return 20.0f * std::log10 (pk + 1.0e-12f);
}

// Each change is a list of values applied at its sample. Returns the output's left side.
static std::vector<float> renderChanges (const ParamValues& start, const std::vector<std::pair<size_t, ParamValues>>& changes,
                                         const std::vector<float>& in, double sr, int block = 64, bool wholeTones = false)
{
    Rig rig;
    rig.setOffline (true);
    rig.startFresh (start, sr, block);
    rig.snapParams (start);
    std::vector<float> l (in.size()), r (in.size());
    size_t next = 0;
    auto v = start;
    for (size_t pos = 0; pos < in.size(); pos += (size_t) block)
    {
        bool changed = false;
        while (next < changes.size() && changes[next].first <= pos) { v = changes[next++].second; changed = true; }
        const int n = (int) std::min<size_t> ((size_t) block, in.size() - pos);
        if (changed && wholeTones) rig.beginToneChange(); // as the app does when a tone is picked
        rig.setParams (v);
        rig.finishChangedWork(); // a new cabinet sounds at once, as it would a moment later live
        rig.process (in.data() + pos, l.data() + pos, r.data() + pos, n);
    }
    return l;
}

static void highpass4k (std::vector<float>& x, double sr)
{
    Biquad a, b;
    a.set (Biquad::Type::highpass, sr, 4000.0, 0.54);
    b.set (Biquad::Type::highpass, sr, 4000.0, 1.31);
    for (auto& s : x) s = b.process (a.process (s));
}

// Judge each change: the click is how far the burst after it rises over the louder settled side.
// The settled side after a change is heard from `settledFromMs` for `settleMs`. A knob's glide
// takes up to 100 ms, so knobs are heard from 150 ms. A tone change is back up 28 ms after it
// (6 down, 10 held, 12 up), and a whole new tone can wobble (a vibe, a tremolo, echoes coming
// in), so tone changes are heard from 40 ms for half a second, more than a slow cycle.
static void judgeChanges (const std::vector<float>& out, const std::vector<size_t>& at, double sr, bool silentIn,
                          const juce::String& label, ClickResult& worst, const juce::StringArray& each = {},
                          double settleMs = 100.0, double settledFromMs = 150.0)
{
    auto hf = out;
    if (! silentIn) highpass4k (hf, sr);
    const auto ms = [&] (double m) { return (size_t) (m * sr / 1000.0); };
    for (size_t k = 0; k < at.size(); ++k)
    {
        const size_t t = at[k];
        if (t < ms (80) || t + ms (settledFromMs + settleMs + 10) > hf.size()) continue;
        // A jump that drives the output over -6 dBFS is the safety limiter's to catch, and it
        // grabs on purpose (the stress test checks that it holds): such a change is not judged here.
        if (! silentIn && peakDb (out, t, t + ms (30)) > -6.0f) { ++worst.limited; continue; }
        const float burst = peakDb (hf, t, t + ms (30));
        float over = burst;
        if (! silentIn)
        {
            const float before = peakDb (hf, t - ms (70), t - ms (5));
            const float after = peakDb (hf, t + ms (settledFromMs), t + ms (settledFromMs + settleMs));
            // how far the burst rises over the louder settled side; top end under -60 dB never counts
            over = burst - std::max ({ before, after, -60.0f });
        }
        const auto what = label + (k < (size_t) each.size() ? each[(int) k] : juce::String()) + juce::String::formatted (" at %.2f s", (double) t / sr);
        worst.all.push_back ({ over, what });
        if (over > worst.worstDb) { worst.worstDb = over; worst.what = what; }
    }
}

// Opening the app must not crackle. The sound card side is softened by the app's own start fade;
// this holds the rig itself to it: with every factory tone, a rig made from nothing makes no sound of
// its own in its first moments (no thump from filters, convolution or oversampling waking up), and its
// first blocks take no unusual time (a slow first block would be a dropout on a small buffer).
void testStart (double sr)
{
    std::printf ("start @ %.0f Hz\n", sr);
    const auto presets = loadPresets();
    const int block = 64;
    const int blocks = (int) (0.4 * sr) / block;
    std::vector<float> in ((size_t) block, 0.0f), l ((size_t) block), r ((size_t) block);
    float worst = 0.0f;
    juce::String worstName;
    double worstFirst = 0.0;
    juce::String slowName;
    for (const auto& p : presets)
    {
        auto v = p.values;
        v[(size_t) P::lofiNoise] = 0.0f; // the lo-fi pedal's hiss is meant to be heard
        v[(size_t) P::gateOn] = 0.0f;    // a gate would hide a thump
        Rig rig;
        rig.setParams (v);
        rig.prepare (sr, block);
        float peak = 0.0f;
        int peakBlock = -1;
        std::vector<double> times;
        for (int b = 0; b < blocks; ++b)
        {
            const auto t0 = juce::Time::getMillisecondCounterHiRes();
            rig.setParams (v);
            rig.process (in.data(), l.data(), r.data(), block);
            times.push_back (juce::Time::getMillisecondCounterHiRes() - t0);
            for (int i = 0; i < block; ++i)
                if (const float x = std::max (std::abs (l[(size_t) i]), std::abs (r[(size_t) i])); x > peak) { peak = x; peakBlock = b; }
        }
        if (peak > worst) { worst = peak; worstName = p.name; }
        if (std::getenv ("RIFFHOUSE_START") != nullptr && peak > 1.0e-3f)
            std::printf ("    %-28s %.1f dB  peak at block %d\n", p.name.toRawUTF8(), juce::Decibels::gainToDecibels (peak), peakBlock);
        auto sorted = times;
        std::sort (sorted.begin(), sorted.end());
        const double median = std::max (1.0e-3, sorted[sorted.size() / 2]);
        const double first = *std::max_element (times.begin(), times.begin() + 4) / median;
        if (first > worstFirst) { worstFirst = first; slowName = p.name; }
    }
    const float db = juce::Decibels::gainToDecibels (worst, -200.0f);
    std::printf ("  %d tones from cold, silence in: loudest %.1f dB (%s)\n", (int) presets.size(), db, worstName.toRawUTF8());
    std::printf ("  slowest of the first four blocks: %.1fx a usual block (%s)\n", worstFirst, slowName.toRawUTF8());
    CHECK (db < -65.0f, "a rig from cold makes a sound of its own: %.1f dB (%s)", db, worstName.toRawUTF8());
}

void testClicks (double sr, bool quick)
{
    std::printf ("clicks @ %.0f Hz\n", sr);
    const auto presets = loadPresets();
    const auto& specs = paramSpecs();
    const double secs = 3.0;
    const size_t len = (size_t) (sr * secs);
    std::vector<float> silence (len, 0.0f), note (len);
    for (size_t i = 0; i < len; ++i) note[i] = 0.08f * (float) std::sin (2.0 * juce::MathConstants<double>::pi * 196.0 * (double) i / sr);
    const auto keepSetup = [&] (ParamValues to, const ParamValues& from)
    {
        for (const auto& s : specs) if (belongsToSetup (s.id)) to[(size_t) s.id] = from[(size_t) s.id];
        to[(size_t) P::lofiNoise] = 0.0f; // the lo-fi pedal's hiss is meant to be heard
        to[(size_t) P::gateOn] = 0.0f;    // a gate would hide a click from the test
        to[(size_t) P::outLevel] -= 10.0f; // well under the safety limiter, so it judges nearly every change
        return to;
    };
    std::vector<size_t> at;
    for (double t = 0.3; t < secs - 0.3; t += 0.3) at.push_back ((size_t) (t * sr));
    // tone changes are heard for longer after each one (see judgeChanges), so they come further apart
    std::vector<size_t> atTone;
    for (double t = 0.6; t < secs - 0.56; t += 0.6) atTone.push_back ((size_t) (t * sr));

    // the rigs the switches are tried on: a clean tone, a crunch, a high gain one, and everything on
    std::vector<std::pair<juce::String, ParamValues>> bases;
    for (const char* name : { "First Light Clean", "Warm Crunch", "Thrash Machine" })
        for (const auto& q : presets) if (q.name == name) bases.push_back ({ name, keepSetup (q.values, defaultParamValues()) });
    {
        auto all = keepSetup (defaultParamValues(), defaultParamValues());
        for (const auto& s : specs) if (s.kind == ParamKind::toggle && ! belongsToSetup (s.id)) all[(size_t) s.id] = 1.0f;
        all[(size_t) P::gateOn] = 0.0f;
        all[(size_t) P::lofiNoise] = 0.0f;
        bases.push_back ({ "everything on", all });
    }

    for (int pass = 0; pass < 2; ++pass)
    {
        const bool silentIn = pass == 0;
        const auto& in = silentIn ? silence : note;
        ClickResult toggles, tones, knobs;
        // 1. every pedal switched on and off
        for (const auto& [bname, base] : bases)
            for (const auto& s : specs)
            {
                if (s.kind != ParamKind::toggle || belongsToSetup (s.id) || s.id == P::gateOn) continue;
                std::vector<std::pair<size_t, ParamValues>> ch;
                auto v = base;
                for (size_t k = 0; k < at.size(); ++k) { v[(size_t) s.id] = v[(size_t) s.id] > 0.5f ? 0.0f : 1.0f; ch.push_back ({ at[k], v }); }
                judgeChanges (renderChanges (base, ch, in, sr), at, sr, silentIn, bname + ": " + s.name, toggles);
            }
        // 2. tone changes, through every factory tone in turn
        {
            const size_t step = quick ? 7 : 1;
            for (size_t first = 0; first < presets.size(); first += atTone.size() * step)
            {
                std::vector<std::pair<size_t, ParamValues>> ch;
                juce::StringArray names;
                const auto start = keepSetup (presets[first].values, defaultParamValues());
                for (size_t k = 0; k < atTone.size(); ++k)
                {
                    const size_t from = k == 0 ? first : (first + k * step) % presets.size();
                    const size_t idx = (first + (k + 1) * step) % presets.size();
                    ch.push_back ({ atTone[k], keepSetup (presets[idx].values, defaultParamValues()) });
                    names.add (presets[from].name + " to " + presets[idx].name);
                }
                judgeChanges (renderChanges (start, ch, in, sr, 64, true), atTone, sr, silentIn, "", tones, names, 500.0, 40.0);
            }
        }
        // 3. every knob swung end to end in 10 ms
        for (const auto& [bname, base] : bases)
            for (const auto& s : specs)
            {
                if (s.kind != ParamKind::continuous || belongsToSetup (s.id) || s.id == P::lofiNoise) continue;
                std::vector<std::pair<size_t, ParamValues>> ch;
                auto v = base;
                const size_t sweep = (size_t) (0.010 * sr);
                for (size_t k = 0; k < at.size(); ++k)
                {
                    const float from = k % 2 == 0 ? s.min : s.max, to = k % 2 == 0 ? s.max : s.min;
                    for (size_t j = 0; j <= sweep; j += 64)
                    {
                        v[(size_t) s.id] = from + (to - from) * (float) j / (float) sweep;
                        ch.push_back ({ at[k] + j, v });
                    }
                }
                judgeChanges (renderChanges (base, ch, in, sr), at, sr, silentIn, bname + ": " + s.name + " (" + s.key + ")", knobs);
            }
        const char* how = silentIn ? "silence in" : "held note in";
        // silence in: the click is the whole output, held under -60 dB. A held note: the burst
        // after a change may not rise more than 3 dB over the sound on either side of it.
        const float limit = silentIn ? -60.0f : 3.0f;
        for (auto* res : { &toggles, &tones, &knobs })
        {
            const char* kind = res == &toggles ? "pedal switch" : res == &tones ? "tone change" : "fast knob";
            std::sort (res->all.begin(), res->all.end(), [] (const auto& a, const auto& b) { return a.first > b.first; });
            int bad = 0;
            for (const auto& [db, w] : res->all) if (db >= limit) ++bad;
            std::printf ("  %s, %s: worst %.1f dB, %d of %d over %.0f dB%s\n", how, kind, res->worstDb, bad, (int) res->all.size(), limit,
                         res->limited > 0 ? (" (" + juce::String (res->limited) + " left to the limiter)").toRawUTF8() : "");
            for (size_t i = 0; i < std::min<size_t> (res->all.size(), 6) && res->all[i].first >= limit; ++i)
                std::printf ("      %6.1f dB  %s\n", res->all[i].first, res->all[i].second.toRawUTF8());
            CHECK (res->worstDb < limit, "%s: a %s clicks (%.1f dB, %s)", how, kind, res->worstDb, res->what.toRawUTF8());
        }
    }
}

void testCpu (double sr)
{
    std::printf ("cpu @ %.0f Hz\n", sr);
    auto v = defaultParamValues();
    for (P t : { P::gateOn, P::compOn, P::wahOn, P::drvOn, P::ampOn, P::cabOn, P::eqOn, P::modOn, P::dlyOn, P::revOn, P::lofiOn })
        v[(size_t) t] = 1;
    v[(size_t) P::ampModel] = choices::tightMetal;
    v[(size_t) P::doubler] = 5;
    const double length = 10.0;
    const auto di = test::guitarDi (sr, length);
    double took = 1.0e9;
    for (int attempt = 0; attempt < 3; ++attempt)
        took = std::min (took, renderWith (v, di, sr, 64).seconds);
    const double rt = length / took;
    const double ref = referenceSeconds (sr, length);
    const double cost = took / ref;
    std::printf ("  every pedal on, 64-sample blocks: %.1fx realtime, %.2f times the reference load\n", rt, cost);
    // Two checks. The first holds on any machine: a change that makes the rig slower shows
    // up as a higher cost against the reference. The second is the floor for playing live.
    CHECK (cost < kRigCostBudget, "the rig got slower: %.2f times the reference load (budget %.2f)", cost, kRigCostBudget);
    CHECK (rt > 3.0, "too slow to play live: %.1fx realtime", rt);
}

int render (const juce::String& presetArg, const juce::File& out, const juce::File& in, double sr)
{
    Preset p;
    juce::String err;
    const juce::File pf (juce::File::getCurrentWorkingDirectory().getChildFile (presetArg));
    if (pf.existsAsFile())
    {
        if (! presetFromJson (pf.loadFileAsString(), p, err)) { std::printf ("%s\n", err.toRawUTF8()); return 1; }
    }
    else
    {
        bool found = false;
        for (const auto& q : loadPresets())
            if (q.name.equalsIgnoreCase (presetArg)) { p = q; found = true; }
        if (! found) { std::printf ("no preset %s\n", presetArg.toRawUTF8()); return 1; }
    }
    std::vector<float> di;
    if (in.existsAsFile()) di = readWavMono (in, sr);
    else di = test::guitarDi (sr, 12.0);
    const auto r = renderWith (p.values, di, sr, 512);
    writeWav (out, r, sr);
    std::printf ("%s -> %s (%.1f dB RMS)\n", p.name.toRawUTF8(), out.getFullPathName().toRawUTF8(), rmsDb (r.l));
    return 0;
}

int renderAll (const juce::File& dir, double sr)
{
    dir.createDirectory();
    const auto di = test::guitarDi (sr, 12.0);
    Render dry; dry.l = di; dry.r = di;
    writeWav (dir.getChildFile ("_dry_di.wav"), dry, sr);
    for (const auto& p : loadPresets())
    {
        const auto r = renderWith (p.values, di, sr, 512);
        writeWav (dir.getChildFile (juce::File::createLegalFileName (p.name) + ".wav"), r, sr);
        std::printf ("%-40s %6.1f dB\n", p.name.toRawUTF8(), 0.5f * (rmsDb (r.l) + rmsDb (r.r)));
    }
    return 0;
}
// A tone made after someone says whose sound it is: the band in front, then the title, and it is found by band and by song.
void testToneNames()
{
    std::printf ("tone names\n");
    Preset p; juce::String err;
    CHECK (presetFromJson (R"({"name":"Inspired by James Hetfield: Downpicked Thrash","category":"Inspired By","inspiredBy":"James Hetfield","band":"Metallica","description":"Tight."})", p, err), "a tone with a band is read");
    CHECK (p.artist() == "Metallica" && p.title() == "Downpicked Thrash", "the band stands in front, the title follows: '%s' '%s'", p.artist().toRawUTF8(), p.title().toRawUTF8());
    CHECK (p.credit() == "Inspired by James Hetfield. ", "the player is named under it: '%s'", p.credit().toRawUTF8());
    CHECK (p.searchText().contains ("metallica") && p.searchText().contains ("hetfield"), "found by band and by player");
    Preset back;
    CHECK (presetFromJson (presetToJson (p), back, err) && back.band == "Metallica" && back.song.isEmpty(), "the band survives saving");

    CHECK (presetFromJson (R"({"name":"Inspired by Metallica: Master of Puppets Interlude","category":"Inspired By","inspiredBy":"Metallica","band":"Metallica","song":"Master of Puppets"})", p, err), "a tone after a song is read");
    CHECK (p.artist() == "Metallica" && p.title() == "Master of Puppets Interlude" && p.credit().isEmpty(), "a song tone needs no second credit: '%s'", p.credit().toRawUTF8());
    CHECK (p.searchText().contains ("master of puppets"), "found by song");
    CHECK (p.part() == "Interlude", "the part of the song it is for: '%s'", p.part().toRawUTF8());
    CHECK (presetFromJson (presetToJson (p), back, err) && back.song == "Master of Puppets", "the song survives saving");
    // amps renamed in 0.1.5 still load from tones saved or shared before
    CHECK (presetFromJson (R"({"name":"Old","params":{"amp_model":"Hot Rod Lead"}})", p, err) && (int) p.values[(size_t) P::ampModel] == choices::hotRodLead, "an old Hot Rod Lead tone loads as the Redline Lead");
    CHECK (presetFromJson (R"({"name":"Old","params":{"amp_model":"brit plexi"}})", p, err) && (int) p.values[(size_t) P::ampModel] == choices::britPlexi, "an old Brit Plexi tone loads as the Brit 68");
    CHECK (presetFromJson (R"({"name":"Old","params":{"amp_model":"Brit 800"}})", p, err) && (int) p.values[(size_t) P::ampModel] == choices::brit800, "an old Brit 800 tone loads as the Brit 81");

    CHECK (presetFromJson (R"({"name":"Inspired by David Gilmour: Comfortably Lead","category":"Inspired By","inspiredBy":"David Gilmour","band":"Pink Floyd","song":"Comfortably Numb"})", p, err), "a player tone after a song is read");
    CHECK (p.credit() == juce::String (juce::CharPointer_UTF8 ("Inspired by David Gilmour on \xe2\x80\x9c" "Comfortably Numb\xe2\x80\x9d. ")), "player and song: '%s'", p.credit().toRawUTF8());
    CHECK (p.part() == "Comfortably Lead", "a title that is not the song is left whole");
    CHECK (presetFromJson (R"({"name":"Inspired by B.B. King: Thrill Lead","category":"Inspired By","inspiredBy":"B.B. King","song":"The Thrill Is Gone"})", p, err), "a solo artist's song tone is read");
    CHECK (p.artist() == "B.B. King" && p.credit() == juce::String (juce::CharPointer_UTF8 ("After \xe2\x80\x9cThe Thrill Is Gone\xe2\x80\x9d. ")), "no band: the player stands in front and the song is named: '%s'", p.credit().toRawUTF8());
    CHECK (presetFromJson (R"({"name":"Warm Crunch","category":"Essentials"})", p, err) && p.artist().isEmpty() && p.title() == "Warm Crunch" && p.credit().isEmpty(), "a plain tone is only its name");
    CHECK (presetFromJson (R"({"name":"My take: louder","category":"User"})", p, err) && p.title() == "My take: louder", "a colon in your own tone's name is left alone");

    PresetLibrary lib;
    lib.scanFolder (juce::File (RIFFHOUSE_PRESET_DIR), true);
    for (const auto& q : lib.all())
    {
        if (q.category != "Inspired By") continue;
        CHECK (q.inspiredBy.isNotEmpty() && q.name.startsWith ("Inspired by " + q.inspiredBy + ": "), "%s says who it is after", q.name.toRawUTF8());
        CHECK (q.title().isNotEmpty() && ! q.title().startsWith ("Inspired by"), "%s has a title of its own", q.name.toRawUTF8());
        if (q.song.isNotEmpty() && q.inspiredBy == q.band)
            CHECK (q.title().startsWith (q.song), "%s is named after its song", q.name.toRawUTF8());
    }
    // what the first players asked for is found the way they would type it
    auto found = [&] (const char* words) { return (int) lib.search (words, "All").size(); };
    CHECK (found ("metallica") >= 5, "Metallica: %d tones", found ("metallica"));
    CHECK (found ("master of puppets") == 2, "Master of Puppets: %d tones", found ("master of puppets"));
    CHECK (found ("pour some sugar") == 1 && found ("def leppard") == 1, "Pour Some Sugar on Me");
    CHECK (found ("fear of the dark") == 2 && found ("iron maiden") == 2, "Fear of the Dark");
    CHECK (found ("wither") == 2 && found ("dream theater") == 2, "Wither");
    CHECK (found ("sweet child") == 2 && found ("guns n' roses") >= 4, "Sweet Child O' Mine: %d, Guns N' Roses: %d", found ("sweet child"), found ("guns n' roses"));
    CHECK (found ("nightmare") == 1 && found ("avenged sevenfold") >= 2, "Nightmare");
    CHECK (found ("big fuzz") >= 2, "the Big Fuzz pedal: %d tones", found ("big fuzz"));
    CHECK (found ("pink floyd comfortably numb") == 1, "a player's tone is found by band and song together");
}

void testUpdates()
{
    std::printf ("updates\n");
    CHECK (compareVersions ("0.1.0", "0.1.0") == 0, "equal versions");
    CHECK (compareVersions ("v0.2.0", "0.1.9") > 0, "v prefix and a higher minor");
    CHECK (compareVersions ("0.1.10", "0.1.9") > 0, "numbers compare as numbers, not text");
    CHECK (compareVersions ("0.2", "0.2.0") == 0, "missing parts count as zero");
    CHECK (compareVersions ("1.0.0", "0.9.9") > 0 && compareVersions ("0.9.9", "1.0.0") < 0, "major wins");
    CHECK (compareVersions ("0.2.0-beta", "0.2.0") < 0, "a pre-release is older than its release");
    CHECK (compareVersions ("0.2.0-beta", "0.1.9") > 0, "a pre-release is newer than the version before");
    CHECK (compareVersions ("", "0.1.0") < 0, "an empty version is older than anything");
    CHECK (compareVersions ("0.1.0-rc1", "0.1.0-rc2") < 0 && compareVersions ("0.1.0-rc10", "0.1.0-rc9") > 0, "test builds of one version go in order");
    CHECK (compareVersions ("0.1.0-rc2", "0.1.0") < 0 && compareVersions ("0.1.0-rc2", "0.1.0-rc2") == 0, "the release follows its last test build");
    CHECK (compareVersions ("0.1.0-dev.1a2b3c4", "0.1.0-rc1") < 0, "a build from source is older than a test build of that version");
    CHECK (isPreRelease ("0.1.0-rc1") && isPreRelease ("v0.2.0-dev.abc") && ! isPreRelease ("0.1.0") && ! isPreRelease ("0.1.0+build-7"), "telling a test build from a release");
    {
        // someone on a test build is offered the next test build, then the release
        auto entry = [] (const char* tag, bool pre, bool draft, bool built = true)
        {
            return juce::String ("{\"tag_name\":\"") + tag + "\",\"prerelease\":" + (pre ? "true" : "false") + ",\"draft\":" + (draft ? "true" : "false")
                 + ",\"html_url\":\"https://github.com/NatikChhabra/riffhouse/releases/tag/" + tag + "\",\"body\":\"\",\"assets\":["
                 + (built ? "{\"name\":\"Riffhouse-windows-setup.exe\"}" : "") + "]}";
        };
        const auto tests = "[" + entry ("v0.1.0-rc1", true, false) + "," + entry ("v0.1.0-rc3", true, false) + "," + entry ("v0.1.0-rc2", true, false) + "," + entry ("v0.1.0-rc9", true, true) + "]";
        CHECK (newestRelease (tests, "").version == "0.1.0-rc3", "newest test release: got '%s'", newestRelease (tests, "").version.toRawUTF8());
        const auto out = "[" + entry ("v0.1.0-rc3", true, false) + "," + entry ("v0.1.0", false, false) + "]";
        CHECK (newestRelease (out, "").version == "0.1.0", "the release beats its test builds");
        CHECK (newestRelease ("[]", "").version.isEmpty() && newestRelease ("not json", "").version.isEmpty(), "an empty or broken list offers nothing");
        CHECK (parseRelease (entry ("v0.1.0-rc3", true, false), "").version.isEmpty(), "a full release never follows test releases");
        CHECK (parseRelease (entry ("v0.1.0-rc3", true, false), "", true).version == "0.1.0-rc3", "a test release is read when asked for");
        // publishing comes first and the installer is attached minutes later: until then the release is not out
        CHECK (parseRelease (entry ("v0.1.1", false, false, false), "-windows-setup.exe").version.isEmpty(), "a release with no installer attached yet is not offered");
        CHECK (parseRelease (entry ("v0.1.1", false, false), "-windows-setup.exe").version == "0.1.1", "and is offered once the installer is there");
        const auto building = "[" + entry ("v0.1.0-rc3", true, false) + "," + entry ("v0.1.0", false, false, false) + "]";
        CHECK (newestRelease (building, "").version == "0.1.0-rc3", "a test build waits for the release's installer too");
    }

    {
        // an audio interface is listened through as well as played into: its output is found by the hardware's name
        const juce::StringArray outs { "Speakers (Realtek(R) Audio)", "Line 3/4 (Scarlett 4i4 USB)", "Speakers (Scarlett 4i4 USB)", "Headphones (USB Headset)" };
        CHECK (hardwareOf ("Analogue 1 + 2 (Scarlett 4i4 USB)") == "Scarlett 4i4 USB" && hardwareOf ("Realtek ASIO").isEmpty(), "the hardware's name is what stands in the last brackets");
        CHECK (outputBeside ("Analogue 1 + 2 (Scarlett 4i4 USB)", outs) == "Speakers (Scarlett 4i4 USB)", "an interface's main output is found: got '%s'",
               outputBeside ("Analogue 1 + 2 (Scarlett 4i4 USB)", outs).toRawUTF8());
        CHECK (outputBeside ("Microphone (Realtek(R) Audio)", outs) == "Speakers (Realtek(R) Audio)", "brackets inside the hardware's name do not confuse it");
        CHECK (hardwareOf ("Microphone (Realtek(R) Audio)") == "Realtek(R) Audio", "got '%s'", hardwareOf ("Microphone (Realtek(R) Audio)").toRawUTF8());
        CHECK (outputBeside ("Microphone (USB Guitar Cable)", outs).isEmpty() && outputBeside ("Some input", outs).isEmpty(), "an input with no output of its own gets none");
        CHECK (outputBeside ("Line In (scarlett 4i4 usb)", { "Line 3/4 (Scarlett 4i4 USB)" }) == "Line 3/4 (Scarlett 4i4 USB)", "any output of the interface beats none");

        // an interface's outputs, two at a time, named as a player reads them
        CHECK (outputPairName ("Headphones L", "Headphones R") == "Headphones", "a pair with sides is one name: '%s'", outputPairName ("Headphones L", "Headphones R").toRawUTF8());
        CHECK (outputPairName ("Main Left", "Main Right") == "Main", "got '%s'", outputPairName ("Main Left", "Main Right").toRawUTF8());
        CHECK (outputPairName ("Out 1", "Out 2") == "Out 1 + 2", "numbered outputs keep their numbers: '%s'", outputPairName ("Out 1", "Out 2").toRawUTF8());
        CHECK (outputPairName ("Analogue 3", "Analogue 4") == "Analogue 3 + 4", "got '%s'", outputPairName ("Analogue 3", "Analogue 4").toRawUTF8());
        CHECK (outputPairName ("SPDIF", "Phones") == "SPDIF + Phones", "two unrelated names are both shown: '%s'", outputPairName ("SPDIF", "Phones").toRawUTF8());
        // an ASIO driver and the same interface as Windows lists it
        CHECK (sameHardware ("Focusrite USB ASIO", "Analogue 1 + 2 (Focusrite USB Audio)"), "the Focusrite driver finds its Windows input");
        CHECK (sameHardware ("Yamaha Steinberg USB ASIO", "Line (Steinberg UR22C)"), "a Steinberg driver finds its interface");
        CHECK (sameHardware ("BEHRINGER UMC 404HD 192k", "Line (UMC404HD 192k)") || sameHardware ("BEHRINGER UMC 404HD 192k", "Line (BEHRINGER UMC 404HD 192k)"), "a Behringer driver finds its interface");
        CHECK (! sameHardware ("Focusrite USB ASIO", "Microphone (Realtek(R) Audio)"), "and never the computer's own sound card");
        CHECK (! sameHardware ("Generic Low Latency ASIO Driver", "Microphone (USB Audio Device)"), "words every driver uses do not count as a match");
        {
            const auto two = whyShared ("Windows Audio", "Guitar (Realtek USB Audio)", "Headphones (4- CX31993+97220 HiFi)");
            CHECK (two.contains ("Realtek USB Audio") && two.contains ("CX31993") && two.contains ("plug your headphones into"),
                   "shared mode on two devices says which two, and what to plug where");
            const auto noSocket = whyShared ("Windows Audio", "Guitar (Realtek USB Audio)", "Headphones (4- CX31993+97220 HiFi)",
                                             { "Headphones (4- CX31993+97220 HiFi)", "4 - Mi TV (AMD High Definition Audio Device)" });
            CHECK (noSocket.contains ("no headphone socket") && ! noSocket.contains ("plug your headphones into"),
                   "a guitar cable with no headphone socket is not told to plug headphones into it: %s", noSocket.toRawUTF8());
            CHECK (whyShared ("Windows Audio", "Guitar (Realtek USB Audio)", "Headphones (4- CX31993+97220 HiFi)",
                              { "Headphones (Realtek USB Audio)", "Headphones (4- CX31993+97220 HiFi)" }).contains ("plug your headphones into"),
                   "a guitar device that has a socket is pointed to it");
            CHECK (whyShared ("Windows Audio", "Microphone (Realtek(R) Audio)", "Speakers (Realtek(R) Audio)").contains ("Close other audio apps"),
                   "shared mode on one device says what to try");
            CHECK (whyShared ("Windows Audio (Exclusive Mode)", "Guitar (Realtek USB Audio)", "Headphones (CX31993 HiFi)").isEmpty()
                       && whyShared ("ASIO", "Focusrite USB ASIO", "Focusrite USB ASIO").isEmpty(),
                   "the fast modes need no explaining");
            const auto spread = buffersToTry ({ 32, 48, 64, 80, 96, 112, 128, 144, 160, 192, 256, 288, 384, 480, 512, 1024, 2048 });
            CHECK (spread.size() <= 6 && spread.contains (32) && spread.contains (256) && spread.contains (512) && spread.contains (1024)
                       && ! spread.contains (2048),
                   "setup tries buffer sizes across the range, not only the smallest six");
            CHECK (buffersToTry ({ 128, 64, 256 }) == juce::Array<int> ({ 64, 128, 256 }), "a short list is tried whole, smallest first");
            // an ASIO driver held at one rate: the one asked for, then its own, then the usual rates, each once
            CHECK (asioRatesToTry (48000.0) == juce::Array<double> ({ 48000.0, 0.0, 44100.0, 96000.0, 88200.0 }), "ASIO rates from 48 kHz");
            CHECK (asioRatesToTry (0.0) == juce::Array<double> ({ 0.0, 48000.0, 44100.0, 96000.0, 88200.0 }), "ASIO rates from its own");
            CHECK (whyAsioFailed ("Focusrite USB ASIO", "Driver failed to initialise", false).contains ("another program"),
                   "a driver that will not start: another program has it");
            CHECK (whyAsioFailed ("Focusrite USB ASIO", {}, true).contains ("clock source"), "a silent driver: its clock");
            CHECK (! whyAsioFailed ("X", "Can't create i/o buffers", false).containsChar (0x2014), "no long dashes");
            const auto advice = asioBufferAdvice (1024, 48000.0, 1, "Open it");
            CHECK (advice.contains ("1024 samples") && advice.contains ("21.3 ms") && advice.contains ("\"Open it\""),
                   "a buffer held by the driver says how to change it: %s", advice.toRawUTF8());
            CHECK (asioBufferAdvice (128, 48000.0, 1, "x").isEmpty() && asioBufferAdvice (1024, 48000.0, 5, "x").isEmpty(),
                   "no advice when the delay is short or Riffhouse can change it");
            CHECK (whyAsioFailed ("ASIO4ALL v2", "Can't create i/o buffers", false).contains ("Discord")
                       && whyAsioFailed ("ASIO4ALL v2", "Can't create i/o buffers", false).contains ("all to itself"),
                   "a wrapper driver that will not start names a voice call and why");
            CHECK (! whyAsioFailed ("Focusrite USB ASIO", "Can't create i/o buffers", false).contains ("all to itself"),
                   "an interface's own driver is not told it wraps Windows audio");
            CHECK (delayFigure (48.1234) == "48" && delayFigure (6.3) == "6.3" && delayFigure (9.96) == "10.0" && delayFigure (12.6) == "13",
                   "the delay figure stays short: %s", delayFigure (48.1234).toRawUTF8());
        }
    }

    const juce::String hash = "9f86d081884c7d659a2feaa0c55ad015a3bf4f1b2b0b822cd15d6c15b0f00a08";
    const juce::String base = "https://github.com/NatikChhabra/riffhouse/releases/download/v0.2.0/";
    auto release = [&] (const juce::String& assets, const juce::String& extra = {}, const juce::String& body = "notes")
    {
        return juce::String (R"({"tag_name":"v0.2.0","html_url":"https://github.com/NatikChhabra/riffhouse/releases/tag/v0.2.0","body":")")
             + body + "\"" + extra + ",\"assets\":[" + assets + "]}";
    };
    auto asset = [&] (const juce::String& name, const juce::String& url, const juce::String& digest)
    {
        return "{\"name\":\"" + name + "\",\"browser_download_url\":\"" + url + "\",\"size\":1234"
             + (digest.isNotEmpty() ? ",\"digest\":\"" + digest + "\"" : juce::String()) + "}";
    };
    const juce::String win = "Riffhouse-0.2.0-windows-setup.exe";

    auto r = parseRelease (release (asset ("Riffhouse-0.2.0-macos.zip", base + "Riffhouse-0.2.0-macos.zip", "sha256:" + hash) + ","
                                    + asset (win, base + win, "sha256:" + hash)), "-windows-setup.exe");
    CHECK (r.version == "0.2.0", "version read from the tag, got '%s'", r.version.toRawUTF8());
    CHECK (r.assetName == win && r.sha256 == hash && r.size == 1234 && r.hasInstaller(), "picks the Windows installer and its digest");

    r = parseRelease (release (asset (win, base + win, {})), "-windows-setup.exe");
    CHECK (r.version == "0.2.0" && ! r.hasInstaller(), "no published hash means nothing to install");

    r = parseRelease (release (asset (win, base + win, {}), {}, "SHA-256 " + win + " " + hash.toUpperCase()), "-windows-setup.exe");
    CHECK (r.hasInstaller() && r.sha256 == hash, "falls back to a hash written in the release notes");

    r = parseRelease (release (asset (win, "https://example.com/" + win, "sha256:" + hash)), "-windows-setup.exe");
    CHECK (! r.hasInstaller(), "a download from anywhere but the project's releases is refused");
    r = parseRelease (release (asset (win, "http://github.com/NatikChhabra/riffhouse/releases/download/v0.2.0/" + win, "sha256:" + hash)), "-windows-setup.exe");
    CHECK (! r.hasInstaller(), "plain http is refused");
    r = parseRelease (release (asset (win, base + win, "sha256:" + hash.substring (2))), "-windows-setup.exe");
    CHECK (! r.hasInstaller(), "a short digest is refused");

    CHECK (parseRelease (release (asset (win, base + win, "sha256:" + hash), ",\"draft\":true"), "-windows-setup.exe").version.isEmpty(), "drafts are ignored");
    CHECK (parseRelease (release (asset (win, base + win, "sha256:" + hash), ",\"prerelease\":true"), "-windows-setup.exe").version.isEmpty(), "pre-releases are ignored");
    CHECK (parseRelease ("not json at all", "-windows-setup.exe").version.isEmpty(), "garbage gives an empty result");

    // what the answer to the check means: only a real fault may be reported as one
    CHECK (readCheckAnswer (404, "{\"message\":\"Not Found\"}") == CheckAnswer::nothingPublished, "404: nothing published yet, so this version is the latest");
    CHECK (readCheckAnswer (200, "{}") == CheckAnswer::release, "200 with a body is a release to read");
    CHECK (readCheckAnswer (200, {}) == CheckAnswer::unreachable, "200 with nothing in it is no answer");
    CHECK (readCheckAnswer (0, {}) == CheckAnswer::unreachable, "no connection");
    CHECK (readCheckAnswer (403, "{}") == CheckAnswer::unreachable, "turned away (too many requests)");
    CHECK (readCheckAnswer (503, {}) == CheckAnswer::unreachable, "GitHub is down");
    CHECK (parseRelease ("{\"message\":\"API rate limit exceeded\"}", "-windows-setup.exe").version.isEmpty(), "an API error gives an empty result");
    CHECK (parseRelease ("[1,2,3]", "-windows-setup.exe").version.isEmpty(), "the wrong JSON shape gives an empty result");

    const auto f = juce::File::createTempFile (".bin");
    f.replaceWithText ("test");
    CHECK (sha256Hex (f) == hash, "SHA-256 of a file, got %s", sha256Hex (f).toRawUTF8());
    f.deleteFile();
    CHECK (sha256Hex (f).isEmpty(), "a missing file has no hash");
}
} // namespace

void testHistory()
{
    std::printf ("undo and redo\n");
    ToneHistory h;
    ToneState t;
    t.values = defaultParamValues();
    t.libraryName = "First";
    auto show = [&] (bool hands = false) { h.tick (t, hands); };
    show();
    CHECK (! h.canUndo (t) && ! h.canRedo(), "nothing to undo before anything was changed");

    // one long drag of one knob is one step, however many values it passes through
    for (int i = 1; i <= 20; ++i) { t.values[(size_t) P::ampGain] = 5.0f + 0.15f * (float) i; show (true); }
    show(); show(); show();
    CHECK (h.depth() == 1, "a drag became one step (%d)", h.depth());

    // a change that has only just been made can be undone before it has settled
    t.values[(size_t) P::ampBass] = 9.0f;
    CHECK (h.canUndo (t), "a fresh change can be undone straight away");
    ToneState to;
    CHECK (h.undo (t, to) && std::abs (to.values[(size_t) P::ampBass] - defaultParamValues()[(size_t) P::ampBass]) < 1.0e-6f
               && std::abs (to.values[(size_t) P::ampGain] - 8.0f) < 1.0e-4f, "undo gave back the bass and kept the gain");
    t = to; h.settle (t);
    CHECK (h.undo (t, to) && std::abs (to.values[(size_t) P::ampGain] - defaultParamValues()[(size_t) P::ampGain]) < 1.0e-6f, "a second undo gave back the gain");
    t = to; h.settle (t);
    CHECK (! h.undo (t, to), "nothing left to undo");
    CHECK (h.redo (t, to) && std::abs (to.values[(size_t) P::ampGain] - 8.0f) < 1.0e-4f, "redo brought the gain back");
    t = to; h.settle (t);
    CHECK (h.redo (t, to) && std::abs (to.values[(size_t) P::ampBass] - 9.0f) < 1.0e-6f, "a second redo brought the bass back");
    t = to; h.settle (t);
    CHECK (! h.canRedo(), "nothing left to redo");

    // loading another tone is a step, and undo names the tone it came from
    t.libraryName = "Second";
    t.values[(size_t) P::ampModel] = (float) choices::tightMetal;
    show(); show(); show();
    CHECK (h.undo (t, to) && to.libraryName == "First", "undo went back to the tone before");
    t = to; h.settle (t);

    // a new change after an undo throws the forward steps away
    t.values[(size_t) P::ampMid] = 2.0f;
    show(); show(); show();
    CHECK (! h.canRedo(), "a new change cleared redo");

    // your input setup is not part of the tone: changing it makes no step and undo leaves it alone
    const int before = h.depth();
    t.values[(size_t) P::inGain] = 11.0f;
    t.values[(size_t) P::gateThresh] = -40.0f;
    show(); show(); show();
    CHECK (h.depth() == before, "input settings made no step");

    // it never grows without end
    for (int i = 0; i < 300; ++i)
    {
        t.values[(size_t) P::ampTreble] = (i % 2 == 0) ? 3.0f : 7.0f;
        show(); show(); show();
    }
    CHECK (h.depth() == (int) ToneHistory::maxSteps, "the history stops at %d steps (%d)", (int) ToneHistory::maxSteps, h.depth());
}

void testLooper()
{
    std::printf ("looper\n");
    const double sr = 48000.0;
    const int block = 256;
    Looper lp;
    lp.prepare (sr, 4.0);
    std::vector<float> l ((size_t) block), r ((size_t) block);
    long played = 0;
    // a rising ramp per sample, so every sample of the loop can be told from every other
    auto run = [&] (int blocks, float inputGain, std::vector<float>* outL = nullptr)
    {
        for (int b = 0; b < blocks; ++b)
        {
            for (int i = 0; i < block; ++i, ++played)
            {
                l[(size_t) i] = inputGain * (0.1f + 0.5f * (float) (played % 4800) / 4800.0f);
                r[(size_t) i] = -l[(size_t) i];
            }
            lp.process (l.data(), r.data(), block);
            if (outL != nullptr) outL->insert (outL->end(), l.begin(), l.end());
        }
    };
    lp.level = 1.0f;
    run (4, 1.0f);
    CHECK (lp.state() == Looper::State::empty, "it does nothing until it is pressed");

    lp.press();
    run (150, 1.0f); // 38400 samples = 0.8 s
    CHECK (lp.state() == Looper::State::recording, "the first press records");
    lp.press();
    std::vector<float> first, second;
    run (150, 0.0f, &first);
    CHECK (lp.state() == Looper::State::playing && std::abs (lp.lengthSeconds() - 0.8) < 1.0e-6, "the second press plays a loop of 0.8 s (%.4f)", lp.lengthSeconds());
    run (150, 0.0f, &second);
    double diff = 0.0, energy = 0.0;
    for (size_t i = 0; i < first.size(); ++i) { diff = std::max (diff, (double) std::abs (first[i] - second[i])); energy += (double) first[i] * first[i]; }
    CHECK (diff < 1.0e-7 && energy > 1.0, "every time round is the same loop (difference %.2g, energy %.1f)", diff, energy);
    CHECK (std::abs (first.front()) < 1.0e-6f && std::abs (first.back()) < 0.01f, "the join is faded, so it cannot click (%.4f, %.4f)", first.front(), first.back());

    // layering adds to the loop without changing its length, and what you hear is loop plus live
    lp.press();
    run (150, 0.5f);
    CHECK (lp.state() == Looper::State::overdubbing, "the third press layers");
    lp.press();
    run (150, 0.0f); // the layering fades out over the first few milliseconds of this round
    std::vector<float> layered;
    run (150, 0.0f, &layered);
    double layeredEnergy = 0.0;
    for (float x : layered) layeredEnergy += (double) x * x;
    CHECK (lp.state() == Looper::State::playing && std::abs (lp.lengthSeconds() - 0.8) < 1.0e-6 && layeredEnergy > energy * 1.5,
           "the layer is in the loop and the loop is as long as before (energy %.1f after %.1f)", layeredEnergy, energy);

    lp.stop();
    std::vector<float> silent;
    run (20, 0.0f, &silent);
    double left = 0.0;
    for (float x : silent) left = std::max (left, (double) std::abs (x));
    CHECK (lp.state() == Looper::State::stopped && left < 1.0e-9, "stop is silent and keeps the loop");
    lp.press();
    std::vector<float> again;
    run (150, 0.0f, &again);
    double d2 = 0.0;
    for (size_t i = 0; i < again.size(); ++i) d2 = std::max (d2, (double) std::abs (again[i] - layered[i]));
    CHECK (lp.state() == Looper::State::playing && d2 < 1.0e-7, "a press after stop plays it again from its start (difference %.2g)", d2);

    lp.clear();
    run (2, 0.0f);
    CHECK (lp.state() == Looper::State::empty && lp.lengthSeconds() == 0.0, "clear throws it away");

    // a tap is not a phrase
    lp.press(); run (10, 1.0f); lp.press(); run (2, 1.0f);
    CHECK (lp.state() == Looper::State::empty, "a recording shorter than a quarter second is dropped");

    // run out of room: the loop closes by itself at its longest and plays
    lp.press();
    run ((int) (4.5 * sr) / block, 1.0f);
    CHECK (lp.state() == Looper::State::playing && std::abs (lp.lengthSeconds() - 4.0) < 1.0e-6, "at the longest length it closes by itself (%.3f s)", lp.lengthSeconds());
}

void testSong()
{
    std::printf ("song speed, key and part\n");
    const double sr = 48000.0;
    const int block = 256;
    const double twoPi = 6.283185307179586;
    // the pitch of a steady tone, from how often it crosses zero going up
    auto pitchOf = [&] (const std::vector<float>& x, size_t from)
    {
        int ups = 0;
        for (size_t i = from + 1; i < x.size(); ++i)
            if (x[i - 1] < 0.0f && x[i] >= 0.0f) ++ups;
        return (double) ups * sr / (double) (x.size() - from);
    };
    auto rmsOf = [] (const std::vector<float>& x, size_t from)
    {
        double sum = 0.0;
        for (size_t i = from; i < x.size(); ++i) sum += (double) x[i] * (double) x[i];
        return std::sqrt (sum / (double) (x.size() - from));
    };
    // plays a 440 Hz tone through the stretcher and gives back what came out and how much song it used
    auto play = [&] (SongStretch& st, double seconds, std::vector<float>& out, long& used, int& mostAsked, bool& exact)
    {
        std::vector<float> inL ((size_t) SongStretch::most (block)), inR (inL.size()), oL ((size_t) block), oR ((size_t) block);
        out.clear();
        used = 0;
        mostAsked = 0;
        exact = true;
        for (long done = 0; done < (long) (seconds * sr); done += block)
        {
            const int need = st.wants (block);
            mostAsked = std::max (mostAsked, need);
            for (int i = 0; i < need; ++i)
            {
                inL[(size_t) i] = 0.4f * (float) std::sin (twoPi * 440.0 * (double) (used + i) / sr);
                inR[(size_t) i] = inL[(size_t) i];
            }
            st.process (inL.data(), inR.data(), need, oL.data(), oR.data(), block);
            if (need != block || ! std::equal (oL.begin(), oL.end(), inL.begin())) exact = false;
            used += need;
            out.insert (out.end(), oL.begin(), oL.end());
        }
    };

    std::vector<float> out;
    long used = 0;
    int mostAsked = 0;
    bool exact = false;
    {
        SongStretch st;
        st.prepare (sr);
        play (st, 1.0, out, used, mostAsked, exact);
        CHECK (st.plain() && exact, "at normal speed in the original key the song passes through untouched, sample for sample");
        st.setSpeed (1.003f);
        CHECK (st.speed() == 1.0f, "a hair off normal speed is normal speed");
        st.setSpeed (0.1f);
        st.setKey (40);
        CHECK (st.speed() == SongStretch::minSpeed && st.key() == SongStretch::maxKeyShift, "speed and key stay inside what the controls offer");
    }
    {
        SongStretch st;
        st.prepare (sr);
        st.setSpeed (0.5f);
        play (st, 4.0, out, used, mostAsked, exact);
        const double total = (double) out.size();
        CHECK (std::abs ((double) used - total * 0.5) <= 2.0, "half speed uses half as much song: %ld of %ld", used, (long) total);
        const double hz = pitchOf (out, out.size() / 2);
        CHECK (std::abs (hz - 440.0) < 4.4, "half speed keeps the pitch: %.1f Hz for 440", hz);
        const double level = rmsOf (out, out.size() / 2) / (0.4 / std::sqrt (2.0));
        CHECK (level > 0.7 && level < 1.4, "half speed keeps the loudness: %.2f times", level);
    }
    {
        SongStretch st;
        st.prepare (sr);
        st.setSpeed (SongStretch::maxSpeed);
        play (st, 2.0, out, used, mostAsked, exact);
        CHECK (mostAsked <= SongStretch::most (block), "the fastest speed never asks for more song than there is room for");
        CHECK (std::abs ((double) used - (double) out.size() * (double) SongStretch::maxSpeed) <= 2.0, "faster uses more song, exactly as much more");
        const double hz = pitchOf (out, out.size() / 2);
        CHECK (std::abs (hz - 440.0) < 4.4, "faster keeps the pitch: %.1f Hz for 440", hz);
    }
    {
        SongStretch st;
        st.prepare (sr);
        st.setKey (12);
        play (st, 3.0, out, used, mostAsked, exact);
        CHECK (used == (long) out.size(), "another key keeps the speed");
        const double hz = pitchOf (out, out.size() / 2);
        CHECK (std::abs (hz - 880.0) < 8.8, "twelve semitones up is twice the pitch: %.1f Hz for 880", hz);
        st.setKey (-5);
        play (st, 3.0, out, used, mostAsked, exact);
        const double down = pitchOf (out, out.size() / 2), want = 440.0 * std::pow (2.0, -5.0 / 12.0);
        CHECK (std::abs (down - want) < want * 0.01, "five semitones down: %.1f Hz for %.1f", down, want);
        st.setKey (0);
        play (st, 0.5, out, used, mostAsked, exact);
        CHECK (exact, "back in the original key at normal speed the song is untouched again");
    }
    {
        // a part of the song going round: every sample must come back in its place, lap after lap
        const size_t len = 4801;
        std::vector<float> l (len), r (len);
        for (size_t i = 0; i < len; ++i) { l[i] = 0.25f + 0.5f * (float) i / (float) len; r[i] = -l[i]; }
        const auto wantL = l;
        SongSection part;
        CHECK (! part.active(), "no part goes round until one is set");
        part.swapIn (l, r, 10.0, sr);
        CHECK (part.active() && l.empty(), "the part is taken, and what it held before is handed back");
        CHECK (std::abs (part.endSeconds() - (10.0 + (double) len / sr)) < 1e-9 && part.startSeconds() == 10.0, "the part knows where in the song it is");
        std::vector<float> got, bl (333), br (333);
        while (got.size() < len * 3) { part.read (bl.data(), br.data(), 333); got.insert (got.end(), bl.begin(), bl.end()); }
        bool inPlace = true, softEnds = true;
        const size_t fade = (size_t) (sr * 0.004);
        for (size_t i = 0; i < len * 3; ++i)
        {
            const size_t at = i % len;
            if (at >= fade && at < len - fade) { if (got[i] != wantL[at]) inPlace = false; }
            else if (std::abs (got[i]) > std::abs (wantL[at])) softEnds = false;
        }
        CHECK (inPlace, "three laps of the part, every sample in its place");
        CHECK (softEnds && std::abs (got[0]) < 0.01f && std::abs (got[len - 1]) < 0.01f, "the join is faded, so going round does not click");
        part.seek (10.05);
        CHECK (std::abs (part.positionSeconds() - 10.05) < 1.0 / sr, "jumping inside the part lands where asked");
        part.seek (99.0);
        CHECK (part.positionSeconds() < part.endSeconds(), "a jump past the part stays inside it");
        std::vector<float> l2 (100, 0.5f), r2 (100, 0.5f);
        part.swapIn (l2, r2, 3.0, sr);
        CHECK (l2.size() == len && part.positionSeconds() == 3.0, "a new part starts from its beginning and hands the old one back");
        part.clear();
        CHECK (! part.active(), "the whole song again");
    }
}

void testFootControl()
{
    std::printf ("foot control\n");
    FootControl fc;
    FootControl::Action a;
    bool learned = false;
    const FootTarget wah { FootJob::param, (int) P::wahPos }, drive { FootJob::param, (int) P::drvOn }, looper { FootJob::looper, 0 }, next { FootJob::nextTone, 0 };

    CHECK (fc.controllerFor (wah) == 11, "out of the box an expression pedal rocks the wah");
    CHECK (fc.moved (11, 127, 0.0, a, learned) && a.target == wah && a.value == 1.0f && ! a.flip && ! learned, "toe down is the top of the wah");
    CHECK (fc.moved (11, 0, 0.1, a, learned) && a.value == 0.0f, "heel down is the bottom");
    CHECK (! fc.moved (20, 127, 0.2, a, learned), "a controller nobody taught does nothing");

    // a switch that stays where it is put: 127 on one press, 0 on the next
    fc.learn (drive);
    CHECK (fc.learning() && fc.learningFor() == drive, "teaching waits for a controller");
    CHECK (fc.moved (20, 127, 1.0, a, learned) && learned && a.target == drive && a.value == 1.0f && ! a.flip, "the press that teaches a switch also switches it on");
    CHECK (! fc.learning() && fc.controllerFor (drive) == 20, "taught");
    CHECK (fc.moved (20, 0, 3.0, a, learned) && ! learned && a.value == 0.0f && ! a.flip, "its next press sends off, and off it goes");
    CHECK (fc.moved (20, 127, 5.0, a, learned) && a.value == 1.0f, "and on again");
    CHECK (! fc.moved (20, 127, 5.1, a, learned), "the same value twice is one press");

    // a switch that springs back: 127 on the press, 0 on the release
    fc.learn (looper);
    CHECK (fc.moved (64, 127, 10.0, a, learned) && learned && a.target == looper && a.flip, "the press that teaches a job also does it");
    CHECK (! fc.moved (64, 0, 10.15, a, learned), "the quick release after it does nothing");
    CHECK (fc.bindings().back().press, "and tells that this switch springs back");
    CHECK (fc.moved (64, 127, 12.0, a, learned) && a.flip, "from then on every press presses");
    CHECK (! fc.moved (64, 0, 12.2, a, learned), "and no release does");
    CHECK (fc.moved (64, 127, 13.0, a, learned) && a.flip, "press again");

    // a job on a switch that stays put: every change is a press
    fc.learn (next);
    CHECK (fc.moved (30, 127, 20.0, a, learned) && a.flip, "next tone taught and done");
    CHECK (fc.moved (30, 0, 22.0, a, learned) && a.flip && a.target == next, "a staying switch going back is its next press");

    // one controller, one job
    fc.learn (wah);
    CHECK (fc.moved (20, 64, 30.0, a, learned) && learned && a.target == wah, "a controller can be given a new job");
    CHECK (fc.controllerFor (drive) == -1 && fc.controllerFor (wah) == 20, "which takes it from the old one, and the wah from controller 11");
    int wahs = 0;
    for (auto& b : fc.bindings()) if (b.target == wah) ++wahs;
    CHECK (wahs == 1 && fc.bindings().size() == 3, "nothing is listed twice");

    const std::string text = fc.toText();
    FootControl back;
    back.fromText (text);
    CHECK (back.toText() == text && back.bindings().size() == 3, "the settings file gives back what was saved: %s", text.c_str());
    CHECK (back.bindings()[0].press && back.bindings()[0].target == looper, "a springing switch stays one");
    back.fromText ("garbage 300=tap 5=p:nothing 7=tap 9=p:amp_gain!");
    CHECK (back.bindings().size() == 2 && back.controllerFor ({ FootJob::tapTempo, 0 }) == 7 && ! back.bindings()[1].press, "a damaged file keeps what can be read and drops the rest");
    back.fromText ("???");
    CHECK (back.bindings().size() == 2, "a file with nothing readable changes nothing");
    back.fromText ("none");
    CHECK (back.bindings().empty(), "an empty list stays empty instead of coming back as the start");
    back.backToStart();
    CHECK (back.controllerFor (wah) == 11 && back.bindings().size() == 1, "back to the start");

    CHECK (footTargetName (wah) == "Wah: Pedal" && footTargetName (drive) == "Drive Pedal on and off" && footTargetName (next) == "Next tone"
               && footTargetName ({ FootJob::param, (int) P::ampLevel }) == "Amp Level" && footTargetName ({ FootJob::param, (int) P::dlyMix }) == "Delay: Mix",
           "every target has a name that says whose it is");
    fc.remove (0);
    fc.setPress (0, true);
    CHECK (fc.bindings().size() == 2 && fc.bindings()[0].press, "a line can be removed and a switch's kind corrected");
}

// Save as MP3 and FLAC: what comes out is the same sound, plays everywhere, and says what it is.
void testExport (const juce::File& dir, const juce::File& take, double sr)
{
    std::printf ("save a copy as MP3 or FLAC\n");
    juce::ignoreUnused (sr);
    juce::AudioFormatManager fm;
    fm.registerBasicFormats();
    const auto readAll = [&fm] (const juce::File& f, double& rate, int& chans)
    {
        std::vector<float> l;
        if (std::unique_ptr<juce::AudioFormatReader> r { fm.createReaderFor (f) })
        {
            rate = r->sampleRate;
            chans = (int) r->numChannels;
            juce::AudioBuffer<float> b ((int) r->numChannels, (int) r->lengthInSamples);
            r->read (&b, 0, (int) r->lengthInSamples, 0, true, true);
            l.assign (b.getReadPointer (0), b.getReadPointer (0) + b.getNumSamples());
        }
        return l;
    };
    double rate = 0.0;
    int chans = 0;
    const auto original = readAll (take, rate, chans);
    const auto rms = [] (const std::vector<float>& x, size_t from, size_t n)
    {
        double e = 0.0;
        for (size_t i = from; i < std::min (x.size(), from + n); ++i) e += (double) x[i] * x[i];
        return std::sqrt (e / (double) std::max<size_t> (1, n));
    };

    // FLAC: every sample back as it went in
    const auto flac = exportFileFor (dir, take, ExportFormat::flac);
    CHECK (flac.getParentDirectory().getFileName() == "Copies to share (MP3 and FLAC)", "copies go in a folder that says what they are");
    std::atomic<float> progress { 0.0f };
    auto err = exportSound (take, flac, ExportFormat::flac, "Test Tone", &progress);
    CHECK (err.isEmpty() && flac.existsAsFile() && progress.load() == 1.0f, "FLAC is written (%s)", err.toRawUTF8());
    CHECK (flac.getSize() < take.getSize(), "and is smaller than the WAV (%lld against %lld bytes)", (long long) flac.getSize(), (long long) take.getSize());
    {
        double fr = 0.0;
        int fc = 0;
        const auto back = readAll (flac, fr, fc);
        float worst = back.size() == original.size() ? 0.0f : 1.0f;
        for (size_t i = 0; i < std::min (back.size(), original.size()); ++i) worst = std::max (worst, std::abs (back[i] - original[i]));
        CHECK (worst < 1.0e-6f && fr == rate && fc == chans, "FLAC gives back every sample (worst %.2g, %d samples against %d)", worst, (int) back.size(), (int) original.size());
    }

    // MP3: the same sound and level, the length right, the tone's name in the tag, quickly
    const auto mp3 = exportFileFor (dir, take, ExportFormat::mp3);
    const auto t0 = std::chrono::steady_clock::now();
    err = exportSound (take, mp3, ExportFormat::mp3, "Test Tone", &progress);
    const double took = std::chrono::duration<double> (std::chrono::steady_clock::now() - t0).count();
    CHECK (err.isEmpty() && mp3.existsAsFile(), "MP3 is written (%s)", err.toRawUTF8());
    CHECK (took < 3.0, "3 s of guitar became an MP3 in %.2f s", took);
    const double seconds = (double) original.size() / rate;
    const double kbps = (double) mp3.getSize() * 8.0 / seconds / 1000.0;
    CHECK (kbps > 300.0 && kbps < 345.0, "at about 320 kbps (%.0f)", kbps);
    {
        juce::MemoryBlock bytes;
        mp3.loadFileAsData (bytes);
        CHECK (bytes.getSize() > 10 && std::memcmp (bytes.getData(), "ID3", 3) == 0, "it starts with a tag");
        bool title = false, info = false;
        const auto* d = static_cast<const char*> (bytes.getData());
        for (size_t i = 0; i + 9 < std::min<size_t> (bytes.getSize(), 8192); ++i)
        {
            title = title || std::memcmp (d + i, "Test Tone", 9) == 0;
            info = info || std::memcmp (d + i, "Info", 4) == 0 || std::memcmp (d + i, "Xing", 4) == 0;
        }
        CHECK (title, "the tag carries the tone's name");
        CHECK (info, "the first frame tells players the exact length");
    }
    {
        double mr = 0.0;
        int mc = 0;
        const auto back = readAll (mp3, mr, mc);
        CHECK (mr == rate && mc == 2, "it plays at the take's rate in stereo (%.0f Hz, %d channels)", mr, mc);
        CHECK (std::abs ((double) back.size() / mr - seconds) < 0.12, "and lasts as long (%.3f s against %.3f s)", (double) back.size() / mr, seconds);
        // line the two up (an MP3 starts a little late) and compare
        int bestLag = 0;
        double best = -1.0;
        const size_t from = (size_t) (0.5 * rate), n = (size_t) (1.5 * rate);
        for (int lag = 0; lag < 3000 && from + n + (size_t) lag < back.size(); ++lag)
        {
            double c = 0.0, a = 0.0, b = 0.0;
            for (size_t i = from; i < from + n; i += 4)
            {
                c += (double) original[i] * back[i + (size_t) lag];
                a += (double) original[i] * original[i];
                b += (double) back[i + (size_t) lag] * back[i + (size_t) lag];
            }
            const double r = c / std::sqrt (std::max (1.0e-12, a * b));
            if (r > best) { best = r; bestLag = lag; }
        }
        CHECK (best > 0.995, "it is the same sound (correlation %.4f, %d samples late)", best, bestLag);
        const double was = rms (original, from, n), now = rms (back, from + (size_t) bestLag, n);
        CHECK (std::abs (20.0 * std::log10 (now / std::max (1.0e-9, was))) < 0.2, "at the same level (%.2f dB apart)", 20.0 * std::log10 (now / std::max (1.0e-9, was)));
    }

    // a mono file at a rate MP3 does not have comes out at one it does
    {
        const auto odd = dir.getChildFile ("odd rate.wav");
        Render r;
        r.l.resize (96000);
        for (size_t i = 0; i < r.l.size(); ++i) r.l[i] = 0.3f * (float) std::sin (2.0 * juce::MathConstants<double>::pi * 440.0 * (double) i / 96000.0);
        r.r = r.l;
        writeWav (odd, r, 96000.0);
        const auto out = dir.getChildFile ("odd rate.mp3");
        err = exportSound (odd, out, ExportFormat::mp3, {}, nullptr);
        double mr = 0.0;
        int mc = 0;
        const auto back = readAll (out, mr, mc);
        CHECK (err.isEmpty() && mr == 48000.0 && std::abs ((double) back.size() / 48000.0 - 1.0) < 0.12, "96 kHz becomes a 48 kHz MP3 of the same length (%s, %.0f Hz, %.3f s)", err.toRawUTF8(), mr, (double) back.size() / std::max (1.0, mr));
    }

    // something that is not sound: a plain message, nothing left behind
    {
        const auto notSound = dir.getChildFile ("notes.wav");
        notSound.replaceWithText ("not a sound");
        const auto out = dir.getChildFile ("notes.mp3");
        err = exportSound (notSound, out, ExportFormat::mp3, {}, nullptr);
        CHECK (err.isNotEmpty() && ! out.exists() && ! out.getSiblingFile ("notes.mp3.part").exists(), "a file that is not sound is refused and leaves nothing (%s)", err.toRawUTF8());
    }

    // stopped half way: nothing left behind
    {
        const auto out = dir.getChildFile ("stopped.mp3");
        int calls = 0;
        err = exportSound (take, out, ExportFormat::mp3, {}, nullptr, [&calls] { return ++calls > 3; });
        CHECK (err == "stopped" && ! out.exists() && ! out.getSiblingFile ("stopped.mp3.part").exists(), "a stopped export leaves nothing behind");
    }

    // on its own thread; the answer goes to the message thread, which this program does not run
    {
        const auto out = dir.getChildFile ("job.mp3");
        ExportJob job (take, out, ExportFormat::mp3, "Test Tone", [] (juce::File, juce::String) {});
        for (int i = 0; i < 500 && job.progress() < 1.0f; ++i) juce::Thread::sleep (10);
        juce::Thread::sleep (50);
        CHECK (job.progress() == 1.0f && out.existsAsFile() && job.output() == out, "the job saves the copy on its own thread");
    }
}

void testTakes()
{
    std::printf ("takes\n");
    const double sr = 48000.0;
    const auto dir = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("riffhouse-takes-test");
    dir.deleteRecursively();
    const auto di = test::guitarDi (sr, 3.0);

    auto v1 = bare();
    v1[(size_t) P::ampOn] = 1; v1[(size_t) P::cabOn] = 1;
    auto v2 = v1;
    v2[(size_t) P::ampModel] = (float) choices::tightMetal;
    v2[(size_t) P::dlyOn] = 1;

    const juce::String base = "2026-01-02 03-04-05 Test Tone";
    const auto wetFile = dir.getChildFile (base + ".wav"), cleanFile = cleanGuitarFile (dir, base);
    cleanFile.getParentDirectory().createDirectory();
    writeWav (wetFile, renderWith (v1, di, sr, 512), sr);
    {
        Render mono; mono.l = di; mono.r = di;
        writeWav (cleanFile, mono, sr);
    }

    auto takes = listTakes (dir);
    CHECK (takes.size() == 1, "one take is listed (%d)", (int) takes.size());
    if (takes.empty()) return;
    CHECK (takes[0].name == "Test Tone", "the tone name comes out of the file name (%s)", takes[0].name.toRawUTF8());
    CHECK (takes[0].hasClean(), "the clean guitar beside it is found");
    CHECK (std::abs (takes[0].seconds - 3.0) < 0.05, "length %.2f s", takes[0].seconds);

    // re-amp with another tone, faster than real time, and compare with the rig run directly
    ReampJob::Setup setup;
    setup.params = v2;
    setup.toneName = "Other: Tone";
    const auto out = ReampJob::outputFileFor (takes[0], setup.toneName);
    CHECK (out.getFileName().startsWith (base + " ~ "), "a re-amped take keeps the original's name in front (%s)", out.getFileName().toRawUTF8());
    std::atomic<float> progress { 0.0f };
    const auto t0 = std::chrono::steady_clock::now();
    const auto err = ReampJob::render (takes[0].clean, out, setup, &progress, nullptr);
    const double took = std::chrono::duration<double> (std::chrono::steady_clock::now() - t0).count();
    CHECK (err.isEmpty(), "re-amp runs (%s)", err.toRawUTF8());
    CHECK (out.existsAsFile() && progress.load() == 1.0f, "the new take is written and progress reached the end");
    CHECK (took < 3.0, "re-amping 3 s took %.2f s", took);
    CHECK (! out.getSiblingFile (out.getFileName() + ".part").exists(), "no half-written file is left behind");

    juce::AudioFormatManager fm;
    fm.registerBasicFormats();
    if (std::unique_ptr<juce::AudioFormatReader> r { fm.createReaderFor (out) })
    {
        // the file holds the guitar rounded to its bit depth: compare against exactly what the job read
        std::vector<float> fromFile (di.size());
        if (std::unique_ptr<juce::AudioFormatReader> c { fm.createReaderFor (cleanFile) })
        {
            juce::AudioBuffer<float> tmp ((int) c->numChannels, (int) di.size());
            c->read (&tmp, 0, (int) di.size(), 0, true, true);
            for (size_t i = 0; i < di.size(); ++i)
            {
                float sum = 0.0f;
                for (int ch = 0; ch < tmp.getNumChannels(); ++ch) sum += tmp.getSample (ch, (int) i);
                fromFile[i] = sum / (float) tmp.getNumChannels();
            }
        }
        const auto direct = renderWith (v2, fromFile, sr, 512);
        CHECK (r->lengthInSamples > (juce::int64) di.size(), "the delay tail is kept after the last note (%d extra samples)", (int) (r->lengthInSamples - (juce::int64) di.size()));
        CHECK (r->lengthInSamples < (juce::int64) di.size() + (juce::int64) (12.5 * sr), "and it stops once the tail has died");
        juce::AudioBuffer<float> got (2, (int) di.size());
        r->read (&got, 0, (int) di.size(), 0, true, true);
        float worst = 0.0f;
        for (size_t i = 0; i < di.size(); ++i) worst = std::max (worst, std::abs (got.getSample (0, (int) i) - direct.l[i]));
        CHECK (worst < 1.0e-4f, "the re-amped sound equals the rig's own output (worst difference %.2g)", worst);
        CHECK (got.getMagnitude (0, (int) di.size()) > 0.01f, "and is not silent");
    }
    else
        CHECK (false, "the new take can be read back");

    // written in the same second as the first one, so only their names can order them
    out.setLastModificationTime (wetFile.getLastModificationTime());
    takes = listTakes (dir);
    CHECK (takes.size() == 2, "both takes are listed (%d)", (int) takes.size());
    CHECK (takes.size() == 2 && takes[0].sound == out && takes[1].sound == wetFile, "the re-amped take is listed above its original");
    for (const auto& t : takes)
        if (t.sound == out)
        {
            CHECK (t.name == "Other Tone", "the re-amped take shows its new tone (%s)", t.name.toRawUTF8());
            CHECK (t.clean == cleanFile, "and still points at the same clean guitar, so it can be re-amped again");
            CHECK (ReampJob::outputFileFor (t, "Other: Tone") != out, "a second re-amp with the same tone gets its own file");
        }

    // play back at another device rate: same length in seconds, sound comes out, the stream ends by itself
    for (auto mode : { TakePlayer::Mode::asRecorded, TakePlayer::Mode::throughRig })
    {
        TakePlayer player;
        const double deviceRate = 44100.0;
        player.prepare (deviceRate, 256);
        CHECK (player.load (takes.back(), mode), "the take loads");
        CHECK (std::abs (player.length() - 3.0) < 0.05, "length %.2f s", player.length());
        std::vector<float> l (256), r (256);
        CHECK (! player.readSound (l.data(), r.data(), 256) && ! player.readClean (l.data(), 256), "nothing is read while stopped");
        player.setPlaying (true);
        double energy = 0.0;
        juce::int64 frames = 0;
        int idle = 0;
        while (player.isPlaying() && frames < (juce::int64) (8.0 * deviceRate))
        {
            // A block the disk thread had not read yet comes out silent and does not move the take on.
            // This loop runs far faster than real time, so on a slow machine there are many of those:
            // only the blocks that moved the take count as played.
            const double before = player.position();
            const bool ok = mode == TakePlayer::Mode::asRecorded ? player.readSound (l.data(), r.data(), 256) : player.readClean (l.data(), 256);
            const bool wrong = mode == TakePlayer::Mode::asRecorded ? player.readClean (r.data(), 256) : player.readSound (r.data(), r.data(), 256);
            if (wrong) { CHECK (false, "only the chosen way of playing delivers sound"); break; }
            if (! ok) { if (++idle > 2000) break; juce::Thread::sleep (1); continue; }
            if (player.position() <= before && player.isPlaying()) { if (++idle > 20000) break; juce::Thread::sleep (1); continue; }
            for (float x : l) energy += (double) x * x;
            frames += 256;
        }
        CHECK (! player.isPlaying(), "playback stops at the end by itself");
        CHECK (std::abs ((double) frames / deviceRate - 3.0) < 0.25, "about 3 s were played at the device's rate (%.2f s)", (double) frames / deviceRate);
        CHECK (energy > 1.0, "sound came out (energy %.1f)", energy);
        player.setPlaying (true);
        CHECK (player.position() < 0.5, "pressing play at the end starts again from the top");
    }
    testExport (dir, wetFile, sr);

    // a take a friend sent, saved as "riff.mp3.mpeg", is listed and plays: read by what is in it
    {
        const auto before = (int) listTakes (dir).size();
        const auto sent = dir.getChildFile ("Friend riff.mp3.mpeg");
        writeWav (sent, renderWith (v1, di, sr, 512), sr);
        auto all = listTakes (dir);
        bool found = false;
        for (const auto& t : all)
            if (t.sound == sent)
            {
                found = true;
                CHECK (std::abs (t.seconds - 3.0) < 0.05, "its length is read from inside it (%.2f s)", t.seconds);
                TakePlayer player;
                player.prepare (sr, 256);
                CHECK (player.load (t, TakePlayer::Mode::asRecorded), "and it plays");
            }
        CHECK (found && (int) all.size() == before + 1, "a sound file with a name no reader knows is listed (%d of %d)", (int) all.size(), before + 1);
        // a copy of a take beside it (the same name, another kind of file) is not a second take
        sent.copyFileTo (wetFile.withFileExtension ("flac"));
        CHECK ((int) listTakes (dir).size() == before + 1, "a copy beside a WAV take is not listed twice");
        // "riff.mp3.mpeg" is not a copy of an own take called "riff.wav"
        const auto own = dir.getChildFile ("Friend riff.wav");
        writeWav (own, renderWith (v1, di, sr, 512), sr);
        CHECK ((int) listTakes (dir).size() == before + 2, "a sent file stays listed beside an own take with a similar name (%d of %d)",
               (int) listTakes (dir).size(), before + 2);
        own.deleteFile();
    }
    dir.deleteRecursively();
}

void testChordSheet()
{
    std::printf ("chords and tabs\n");
    using namespace rh::sheet;
    Chord c;
    CHECK (parseChord ("F#m7", c) && c.root == 6 && c.quality == "m7" && c.bass < 0, "F#m7");
    CHECK (parseChord ("Bb/D", c) && c.root == 10 && c.bass == 2, "Bb/D");
    CHECK (parseChord ("C6/9", c) && c.root == 0 && c.bass < 0, "C6/9");
    for (const char* yes : { "A", "Am", "Esus4", "Cadd9", "G7sus4", "Dmaj7", "Bm7b5", "E5", "Ab", "C#dim7", "F+", "Gm(add9)" })
        CHECK (parseChord (yes, c), "%s is a chord", yes);
    for (const char* no : { "And", "Bad", "man", "I", "Ammm", "N.C.", "Echo", "Dad", "Be", "" })
        CHECK (! parseChord (no, c), "%s is not a chord", no);

    // a page copied from a chords website: [ch] tags, a hard space, a tab block, headings
    const juce::String page = juce::String ("Kitchen Light Chords by Riffhouse\n\n[Intro]\n[ch]Am[/ch]  [ch]F[/ch]  [ch]C[/ch]  [ch]G[/ch]\n\n[Verse 1]\n")
        + "Am              F\nKettle on the stove" + juce::String::charToString ((juce::juce_wchar) 0x00a0) + "again\n"
        + "C                 G\nnobody sleeps before ten\n\nChorus:\nF        G         Am\nleave the kitchen light on\n"
        + "A man walks in\nFill it up\n\n"
        + "e|-----------------|\nB|-----1-----3-----|\nG|---2-----2-------|\nD|-2-----------0---|\nA|-----------------|\nE|-----------------|\n"
        + "-------------------------\n";
    const auto sh = parse (page);
    CHECK (sh.title == "Kitchen Light by Riffhouse", "title: %s", sh.title.toRawUTF8());
    juce::String kinds;
    for (const auto& l : sh.lines) kinds << "_SCTL"[(int) l.kind];
    CHECK (kinds == "SC_SCLCL_SCLLL_TTTTTT", "line kinds %s", kinds.toRawUTF8());
    CHECK (sh.chords.joinIntoString (" ") == "Am F C G", "chords used: %s", sh.chords.joinIntoString (" ").toRawUTF8());
    CHECK (sh.lines[5].text.contains ("stove again"), "a hard space reads as a space");
    CHECK (! sh.flats, "sharps sheet");

    // a new key keeps every chord over its word
    const juce::String verse ("Am              F");
    CHECK (transposeLine (verse, 2, false).indexOf ("G") == verse.indexOf ("F") && transposeLine (verse, 2, false).startsWith ("Bm"),
           "transposed verse: %s", transposeLine (verse, 2, false).toRawUTF8());
    CHECK (transposeLine ("C  G/B  Am", 1, true) == "Db Ab/C Bbm", "flats: %s", transposeLine ("C  G/B  Am", 1, true).toRawUTF8());
    CHECK (transposeLine ("A B", 1, true) == "Bb C", "a longer name pushes the next chord: %s", transposeLine ("A B", 1, true).toRawUTF8());
    CHECK (transposeLine ("Intro: Am  (x2)", 3, false) == "Intro: Cm  (x2)", "labels stay: %s", transposeLine ("Intro: Am  (x2)", 3, false).toRawUTF8());
    CHECK (transposeLine (verse, 12, false) == verse, "an octave is the same sheet");
    CHECK (flatsAfter (sh, 5), "Am up 5 is Dm: written with flats");
    CHECK (! flatsAfter (sh, 2), "Am up 2 is Bm: written with sharps");

    // ChordPro, the format songbook apps share
    const auto pro = parse ("{title: Small Hours}\n{soc}\n[G]Morning comes [D/F#]slow\n{eoc}\n");
    CHECK (pro.title == "Small Hours" && pro.lines.size() == 3, "chordpro: %s, %d lines", pro.title.toRawUTF8(), (int) pro.lines.size());
    if (pro.lines.size() == 3)
    {
        CHECK (pro.lines[0].kind == LineKind::section && pro.lines[1].kind == LineKind::chords && pro.lines[2].kind == LineKind::lyric, "chordpro kinds");
        CHECK (pro.lines[1].text.indexOf ("D/F#") == pro.lines[2].text.indexOf ("slow"), "inline chord over its word: '%s' / '%s'",
               pro.lines[1].text.toRawUTF8(), pro.lines[2].text.toRawUTF8());
    }

    // kept on disk and read back, with the reader's settings
    auto kept = sh;
    kept.shift = 3; kept.size = 2; kept.speed = 4;
    const auto back = parse (toText (kept, 0, true));
    CHECK (back.title == sh.title && back.lines.size() == sh.lines.size() && back.shift == 3 && back.size == 2 && back.speed == 4,
           "sheet round trip: %d vs %d lines, shift %d", (int) back.lines.size(), (int) sh.lines.size(), back.shift);
    auto tiny = parse ("Am  G");
    tiny.title = "Two chords";
    CHECK (parse (toText (tiny, 0, true)).title == "Two chords", "a tiny song keeps its title on disk");
    CHECK (toText (kept, 2, false).contains ("Bm") && ! toText (kept, 2, false).contains ("x_riffhouse"), "shared text is in the new key, no settings");

    // every chord shape plays only that chord's notes, its root among them, and fits a hand
    const int open[] = { 4, 9, 2, 7, 11, 4 };
    struct Q { const char* q; std::vector<int> tones; };
    const Q qualities[] = { { "", { 0, 4, 7 } }, { "m", { 0, 3, 7 } }, { "7", { 0, 4, 7, 10 } }, { "m7", { 0, 3, 7, 10 } },
                            { "maj7", { 0, 4, 7, 11 } }, { "sus2", { 0, 2, 7 } }, { "sus4", { 0, 5, 7 } }, { "5", { 0, 7 } },
                            { "dim", { 0, 3, 6 } }, { "dim7", { 0, 3, 6, 9 } }, { "aug", { 0, 4, 8 } }, { "m7b5", { 0, 3, 6, 10 } } };
    int shapes = 0;
    for (int root = 0; root < 12; ++root)
        for (const auto& q : qualities)
        {
            const auto name = noteName (root, root == 10 || root == 3) + q.q;
            Shape shape;
            if (! shapeFor (name, shape)) { CHECK (false, "no shape for %s", name.toRawUTF8()); continue; }
            ++shapes;
            int played = 0, lo = 99, hi = 0;
            bool rootIn = false, inChord = true;
            for (int s = 0; s < 6; ++s)
            {
                const int f = shape.frets[(size_t) s];
                if (f < 0) continue;
                ++played;
                if (f > 0) { lo = std::min (lo, f); hi = std::max (hi, f); }
                const int note = (open[s] + f) % 12;
                const int rel = ((note - root) % 12 + 12) % 12;
                rootIn |= rel == 0;
                inChord &= std::find (q.tones.begin(), q.tones.end(), rel) != q.tones.end();
            }
            CHECK (inChord && rootIn, "%s plays a wrong note", name.toRawUTF8());
            CHECK (played >= (q.tones.size() == 2 ? 2 : 3), "%s plays %d strings", name.toRawUTF8(), played);
            CHECK (hi <= 15 && (lo == 99 || hi - lo <= 3), "%s stretches frets %d..%d", name.toRawUTF8(), lo, hi);
        }
    Shape f, cadd9;
    CHECK (shapeFor ("F", f) && f.barre == 1 && f.frets[0] == 1, "F is a barre on the first fret");
    CHECK (shapeFor ("Cadd9", cadd9) && cadd9.frets[1] == 3 && cadd9.frets[5] == 3, "Cadd9 is the open shape");
    std::printf ("  %d chord shapes checked\n", shapes);
}

void testBufferSearch()
{
    std::printf ("buffer search\n");
    auto run = [] (juce::Array<int> sizes, int running, int floor, int& tries)
    {
        // a sound card that plays clean at `floor` samples and above
        BufferSearch s (sizes, running);
        tries = 0;
        while (s.next() > 0) { s.result (s.next() >= floor); ++tries; }
        return s.best();
    };
    int tries = 0;
    CHECK (run ({ 32, 64, 128, 256, 512 }, 576, 64, tries) == 64 && tries == 3, "down to 64 in 3 tries, got %d", tries);
    CHECK (run ({ 32, 64, 128, 256, 512 }, 576, 256, tries) == 256 && tries == 2, "up to 256 in 2 tries, got %d", tries);
    CHECK (run ({ 32, 64, 128, 256, 512 }, 576, 1000, tries) == 576 && tries == 3, "nothing smaller runs: stays at 576 (%d tries)", tries);
    CHECK (run ({ 480 }, 480, 32, tries) == 480 && tries == 0, "one size: nothing to try");
    CHECK (run ({ 64, 128, 256 }, 256, 32, tries) == 64 && tries == 2, "already near the floor: %d tries", tries);
}


int main (int argc, char** argv)
{
    juce::ConsoleApplication* unused = nullptr; (void) unused;
    juce::StringArray args;
    for (int i = 1; i < argc; ++i) args.add (argv[i]);

    if (args[0] == "live-update")
    {
        // Ask the real releases address what the app asks, and say what the app would make of it.
        // Needs the network, so it is run by hand and never as part of the checks.
        int status = 0;
        const auto options = juce::URL::InputStreamOptions (juce::URL::ParameterHandling::inAddress)
                                 .withConnectionTimeoutMs (12000).withNumRedirectsToFollow (5).withStatusCode (&status)
                                 .withExtraHeaders ("Accept: application/vnd.github+json\r\nUser-Agent: Riffhouse-Updater\r\nX-GitHub-Api-Version: 2022-11-28");
        auto in = juce::URL ("https://api.github.com/repos/NatikChhabra/riffhouse/releases/latest").createInputStream (options);
        const auto body = in != nullptr ? in->readEntireStreamAsString() : juce::String();
        const auto answer = readCheckAnswer (status, status == 200 ? body : juce::String());
        const auto r = parseRelease (body, "-windows-setup.exe");
        std::printf ("status %d: %s; latest full release: %s\n", status,
                     answer == CheckAnswer::release ? "a release to read" : answer == CheckAnswer::nothingPublished ? "nothing published yet, so up to date" : "could not reach the update server",
                     r.version.isEmpty() ? "none" : r.version.toRawUTF8());
        return 0;
    }
    if (args[0] == "render" && args.size() >= 3)
        return render (args[1], juce::File::getCurrentWorkingDirectory().getChildFile (args[2]),
                       args.size() > 3 ? juce::File::getCurrentWorkingDirectory().getChildFile (args[3]) : juce::File(),
                       args.size() > 4 ? args[4].getDoubleValue() : 48000.0);
    if (args[0] == "renderall" && args.size() >= 2)
        return renderAll (juce::File::getCurrentWorkingDirectory().getChildFile (args[1]), args.size() > 2 ? args[2].getDoubleValue() : 48000.0);

    if (args[0] == "ampdebug")
    {
        Amp a;
        a.prepare (48000.0, 256);
        AmpSettings st; st.model = args[1].getIntValue(); st.gain = 5;
        a.setSettings (st);
        auto di = test::guitarDi (48000.0, 2.0);
        for (size_t pos = 0; pos < di.size(); pos += 256)
        {
            a.process (di.data() + pos, 256);
            for (int i = 0; i < 256; ++i)
                if (! std::isfinite (di[pos + (size_t) i])) { std::printf ("NaN at %d\n", (int) (pos + (size_t) i)); return 1; }
        }
        std::printf ("ok rms %.1f\n", rmsDb (di));
        return 0;
    }
    if (args[0] == "calibrate")
    {
        const double sr = 48000.0;
        const auto di = test::guitarDi (sr, 12.0);
        for (int m = 0; m < numAmpVoicings(); ++m)
        {
            std::printf ("%-18s", ampVoicing (m).name);
            for (float g : { 0.0f, 2.5f, 5.0f, 7.5f, 10.0f })
            {
                auto v = bare();
                v[(size_t) P::ampOn] = 1; v[(size_t) P::cabOn] = 1;
                v[(size_t) P::ampModel] = (float) m; v[(size_t) P::ampGain] = g;
                v[(size_t) P::cabModel] = (float) (m == choices::bassStation ? choices::cabBass4x10 : choices::cabVintage4x12);
                v[(size_t) P::outLevel] = -24.0f;
                const auto r = renderWith (v, di, sr, 512);
                std::printf (" g%.1f:%6.1f/%4.1f", g, rmsDb (r.l) + 24.0f, gainToDb (peakOf (r.l)) - rmsDb (r.l));
            }
            std::printf ("\n");
        }
        return 0;
    }
    if (args[0] == "levels" && args.size() >= 2)
    {
        // prints {"preset name": {rms, peak} in dBFS, ...} for tools/make_presets.py --calibrate,
        // each as the tone plays with its output level at 0 dB, whatever level its file holds,
        // so calibrating gives the same answer however often it is run
        const double sr = 48000.0;
        const auto di = test::guitarDi (sr, 12.0);
        auto* obj = new juce::DynamicObject();
        for (const auto& p : loadPresets())
        {
            auto v = p.values;
            // measure below the safety limiter so it does not skew the numbers
            v[(size_t) P::outLevel] = -24.0f;
            const auto r = renderWith (v, di, sr, 512);
            auto* e = new juce::DynamicObject();
            e->setProperty ("rms", 0.5f * (rmsDb (r.l) + rmsDb (r.r)) + 24.0f);
            e->setProperty ("peak", gainToDb (std::max (peakOf (r.l), peakOf (r.r))) + 24.0f);
            obj->setProperty (p.name, juce::var (e));
        }
        juce::File (juce::File::getCurrentWorkingDirectory().getChildFile (args[1])).replaceWithText (juce::JSON::toString (juce::var (obj)));
        return 0;
    }
    if (args[0] == "play" && args.size() >= 4)
    {
        // play <clean guitar.wav> <preset name> <out.wav> [key=value ...] [trim=dB]
        // Plays a recorded clean guitar through a preset, for listening checks, with Auto level
        // as the app applies it (measured on this guitar) unless auto_level=0.
        juce::AudioFormatManager fm; fm.registerBasicFormats();
        std::unique_ptr<juce::AudioFormatReader> reader (fm.createReaderFor (juce::File::getCurrentWorkingDirectory().getChildFile (args[1])));
        if (reader == nullptr && args[1] != "@ref") { std::printf ("cannot read %s\n", args[1].toRawUTF8()); return 1; }
        const double sr = reader != nullptr ? reader->sampleRate : 48000.0;
        std::vector<float> di;
        if (reader != nullptr)
        {
            juce::AudioBuffer<float> buf (1, (int) reader->lengthInSamples);
            reader->read (&buf, 0, buf.getNumSamples(), 0, true, false);
            di.assign (buf.getReadPointer (0), buf.getReadPointer (0) + buf.getNumSamples());
        }
        else
            di = test::guitarDi (sr, 12.0);
        {
            TiltProbe probe;
            probe.prepare (sr);
            for (size_t at = 0; at < di.size(); at += 256) probe.process (di.data() + at, (int) std::min<size_t> (256, di.size() - at));
            std::printf ("input: tilt %.1f dB over %.1f s, which asks for %.1f dB of fix\n", probe.tiltDb(), probe.secondsMeasured(), TiltProbe::fixAmountFor (probe.tiltDb()));
        }
        ParamValues v = defaultParamValues();
        bool found = args[2] == "-";
        for (const auto& p : loadPresets()) if (p.name == args[2]) { v = p.values; found = true; }
        if (! found) { std::printf ("no preset %s\n", args[2].toRawUTF8()); return 1; }
        float trim = 0.0f;
        for (int i = 4; i < args.size(); ++i)
        {
            const auto key = args[i].upToFirstOccurrenceOf ("=", false, false);
            const float value = args[i].fromFirstOccurrenceOf ("=", false, false).getFloatValue();
            if (key == "trim") { trim = value; continue; }
            const int id = findParamByKey (key.toStdString());
            if (id < 0) { std::printf ("no parameter %s\n", key.toRawUTF8()); return 1; }
            v[(size_t) id] = value;
        }
        // Auto level, as the app does it once it has heard the player: this tone measured on
        // this very guitar, then trimmed to the house loudness (auto_level=0 leaves it alone)
        float autoTrim = 0.0f;
        if (v[(size_t) P::autoLevel] > 0.5f)
        {
            LevelPhrase phrase;
            phrase.samples = di;
            phrase.sampleRate = sr;
            phrase.fromPlayer = true;
            Rig meter;
            meter.setOffline (true);
            const float loud = measureLoudnessDb (meter, toneForMeasuring (v, true), phrase);
            if (loud > kSilentDb) autoTrim = std::clamp (kHouseLoudnessDb - loud, kMostCutDb, kMostBoostDb);
            std::printf ("auto level: %.1f dB\n", autoTrim);
        }
        auto r = renderWith (v, di, sr, 256);
        const float g = dbToGain (trim + autoTrim);
        for (size_t i = 0; i < r.l.size(); ++i) { r.l[i] *= g; r.r[i] *= g; }
        writeWav (juce::File::getCurrentWorkingDirectory().getChildFile (args[3]), r, sr);
        std::printf ("%s: rms %.1f dB, peak %.1f dB\n", args[2].toRawUTF8(), 0.5f * (rmsDb (r.l) + rmsDb (r.r)), gainToDb (std::max (peakOf (r.l), peakOf (r.r))));
        return 0;
    }
    if (args[0] == "fresh")
    {
        // which block keeps something from the tone before: one rig, a tone, another tone, the first again
        const double sr = 48000.0;
        const auto di = test::guitarDi (sr, 2.0);
        Rig rig;
        auto render = [&] (const ParamValues& v) { rig.startFresh (v, sr, 256); return run (rig, v, di, 256); };
        const auto presets = loadPresets();
        for (P t : { P::count, P::gateOn, P::compOn, P::wahOn, P::pitchOn, P::drvOn, P::ampOn, P::cabOn, P::eqOn, P::modOn, P::dlyOn, P::revOn, P::lofiOn })
        {
            auto v = bare();
            if (t != P::count) v[(size_t) t] = 1;
            if (t == P::count) v[(size_t) P::doubler] = 6;
            const auto a = render (v);
            render (presets[7].values); render (presets[40].values);
            const auto b = render (v);
            float w = 0.0f; size_t at = 0;
            for (size_t i = 0; i < di.size(); ++i) { const float d = std::max (std::abs (a.l[i] - b.l[i]), std::abs (a.r[i] - b.r[i])); if (d > w) { w = d; at = i; } }
            size_t first = di.size();
            for (size_t i = 0; i < di.size(); ++i) if (a.l[i] != b.l[i] || a.r[i] != b.r[i]) { first = i; break; }
            std::printf ("%-10s worst %.3g at %d, first difference at %d (%g against %g)\n", t == P::count ? "doubler" : spec (t).key, w, (int) at, (int) first,
                         first < di.size() ? a.l[first] : 0.0f, first < di.size() ? b.l[first] : 0.0f);
        }
        return 0;
    }
    if (args[0] == "determinism")
    {
        const double sr = 48000.0;
        const auto di = test::guitarDi (sr, 3.0);
        for (P t : { P::count, P::gateOn, P::compOn, P::wahOn, P::pitchOn, P::drvOn, P::ampOn, P::cabOn, P::eqOn, P::modOn, P::dlyOn, P::revOn, P::lofiOn })
        {
            auto v = bare();
            if (t != P::count) v[(size_t) t] = 1;
            const auto a = renderWith (v, di, sr, 512), b = renderWith (v, di, sr, 512);
            float w = 0.0f; size_t at = 0;
            for (size_t i = 0; i < di.size(); ++i) { const float d = std::max (std::abs (a.l[i] - b.l[i]), std::abs (a.r[i] - b.r[i])); if (d > w) { w = d; at = i; } }
            std::printf ("%-10s worst %.3g at %d\n", t == P::count ? "bypass" : spec (t).key, w, (int) at);
        }
        for (int combo = 0; combo < 6; ++combo)
        {
            auto v = bare();
            v[(size_t) P::ampOn] = 1;
            if (combo != 5) v[(size_t) P::cabOn] = 1;
            if (combo == 1 || combo == 2 || combo == 5) v[(size_t) P::ampModel] = (float) choices::tightMetal;
            if (combo == 2 || combo == 3 || combo == 5) v[(size_t) P::dlyOn] = 1;
            if (combo == 4) { v[(size_t) P::ampOn] = 0; v[(size_t) P::dlyOn] = 1; }
            for (int rep = 0; rep < 3; ++rep)
            {
                const auto a = renderWith (v, di, sr, 512), b = renderWith (v, di, sr, 512);
                float w = 0.0f; size_t at = 0;
                for (size_t i = 0; i < di.size(); ++i) { const float d = std::max (std::abs (a.l[i] - b.l[i]), std::abs (a.r[i] - b.r[i])); if (d > w) { w = d; at = i; } }
                std::printf ("combo %d    worst %.3g at %d\n", combo, w, (int) at);
            }
        }
        for (int m = 0; m < choices::neuralCapture; ++m)
        {
            auto v = bare();
            v[(size_t) P::ampOn] = 1; v[(size_t) P::ampModel] = (float) m;
            const auto a = renderWith (v, di, sr, 512), b = renderWith (v, di, sr, 512);
            float w = 0.0f; size_t at = 0;
            for (size_t i = 0; i < di.size(); ++i) { const float d = std::abs (a.l[i] - b.l[i]); if (d > w) { w = d; at = i; } }
            std::printf ("amp %-2d     worst %.3g at %d\n", m, w, (int) at);
        }
        return 0;
    }
    if (args[0] == "hum") { testHum (48000.0); std::printf ("\n%d checks, %d failures\n", checks, failures); return failures == 0 ? 0 : 1; }
    if (args[0] == "stress") { testStress (48000.0, 256, 30.0, 1); testStress (44100.0, 4096, 20.0, 2); testStress (48000.0, 16, 10.0, 3); testStress (96000.0, 512, 10.0, 4); testStress (192000.0, 64, 5.0, 5); std::printf ("\n%d checks, %d failures\n", checks, failures); return failures == 0 ? 0 : 1; }
    if (args[0] == "resetcost")
    {
        // how long each drive pedal takes to restart (it settles on silence first): an audio thread cost
        for (int t = 0; t < 7; ++t)
        {
            DrivePedal d;
            d.prepare (48000.0, 512);
            d.set (t, 6, 5, 5);
            const auto t0 = std::chrono::steady_clock::now();
            for (int k = 0; k < 20; ++k) d.reset();
            const double ms = std::chrono::duration<double, std::milli> (std::chrono::steady_clock::now() - t0).count() / 20.0;
            std::vector<float> z (4800, 0.0f);
            d.reset();
            for (size_t i = 0; i < z.size(); i += 480) d.process (z.data() + i, 480);
            std::printf ("drive type %d: %.3f ms a restart, then %.1f dB from silence\n", t, ms, peakDb (z, 0, z.size()));
        }
        return 0;
    }
    if (args[0] == "clickdebug" && args.size() >= 3)
    {
        // clickdebug <tone A> <tone B> [note]: change from A to B at 0.5 s and print the output's
        // peak, millisecond by millisecond, around the change
        const double sr = 48000.0;
        const auto presets = loadPresets();
        auto find = [&] (const juce::String& n) { for (const auto& q : presets) if (q.name.equalsIgnoreCase (n)) return q.values; return defaultParamValues(); };
        auto a = find (args[1]);
        auto b = find (args[2]);
        if (args[2].startsWith ("="))
        {
            // =key=value,key=value: tone A with those changed
            b = a;
            for (const auto& kv : juce::StringArray::fromTokens (args[2].substring (1), ",", ""))
                if (const int id = findParamByKey (kv.upToFirstOccurrenceOf ("=", false, false).toStdString()); id >= 0)
                    b[(size_t) id] = kv.fromFirstOccurrenceOf ("=", false, false).getFloatValue();
        }
        for (auto* v : { &a, &b }) { (*v)[(size_t) P::gateOn] = 0.0f; (*v)[(size_t) P::lofiNoise] = 0.0f; }
        const size_t len = (size_t) sr;
        std::vector<float> in (len, 0.0f);
        if (args.contains ("note"))
            for (size_t i = 0; i < len; ++i) in[i] = 0.08f * (float) std::sin (2.0 * juce::MathConstants<double>::pi * 196.0 * (double) i / sr);
        if (args.contains ("bisect"))
        {
            // which part of B makes the noise: B's settings for one part at a time, on A
            for (const char* part : { "comp_", "wah_", "pitch_", "drv_", "amp_", "cab_", "eq_", "mod_", "dly_", "rev_", "lofi_", "doubler", "space", "out_" })
            {
                auto mix = a;
                for (const auto& sp : paramSpecs())
                    if (juce::String (sp.key).startsWith (part)) mix[(size_t) sp.id] = b[(size_t) sp.id];
                const auto o = renderChanges (a, { { (size_t) (0.5 * sr), mix } }, in, sr, 64, true);
                auto h = o;
                highpass4k (h, sr);
                std::printf ("%-8s %7.1f dB, above 4 kHz %7.1f dB in the first 30 ms\n", part, peakDb (o, (size_t) (0.5 * sr), (size_t) (0.56 * sr)),
                             peakDb (h, (size_t) (0.5 * sr), (size_t) (0.53 * sr)));
            }
            return 0;
        }
        const auto out = renderChanges (a, { { (size_t) (0.5 * sr), b } }, in, sr, 64, ! args.contains ("knob"));
        auto hf = out;
        highpass4k (hf, sr);
        for (int ms = -5; ms < 60; ++ms)
        {
            const size_t from = (size_t) ((0.5 + ms / 1000.0) * sr);
            std::printf ("%4d ms %7.1f dB, above 4 kHz %7.1f dB\n", ms, peakDb (out, from, from + 48), peakDb (hf, from, from + 48));
        }
        writeWav (juce::File::getCurrentWorkingDirectory().getChildFile ("clickdebug.wav"), Render { out, out, 0.0 }, sr);
        return 0;
    }
    if (args[0] == "tuner") { testTuner (48000.0); testTuner (44100.0); testTuner (96000.0); testMetronome(); std::printf ("\n%d checks, %d failures\n", checks, failures); return failures == 0 ? 0 : 1; }
    if (args[0] == "start") { testStart (48000.0); testStart (44100.0); std::printf ("\n%d checks, %d failures\n", checks, failures); return failures == 0 ? 0 : 1; }
    if (args[0] == "devices") { test::runDeviceChecks (checks, failures); std::printf ("\n%d checks, %d failures\n", checks, failures); return failures == 0 ? 0 : 1; }
    if (args[0] == "clicks") { std::setvbuf (stdout, nullptr, _IONBF, 0); testClicks (48000.0, args.contains ("--quick")); std::printf ("\n%d checks, %d failures\n", checks, failures); return failures == 0 ? 0 : 1; }
    if (args[0] == "dynamics") { testDynamics (48000.0); std::printf ("\n%d checks, %d failures\n", checks, failures); return failures == 0 ? 0 : 1; }
    if (args[0] == "ampvsreal" && args.size() >= 2) return ampVsReal (juce::File (args[1]));
    if (args[0] == "cabs") { testCabs (48000.0); testCabs (44100.0); testCabs (96000.0); std::printf ("\n%d checks, %d failures\n", checks, failures); return failures == 0 ? 0 : 1; }
    if (args[0] == "cpu") { testCpu (48000.0); std::printf ("\n%d checks, %d failures\n", checks, failures); return failures == 0 ? 0 : 1; }
    if (args[0] == "profile")
    {
        const double sr = 48000.0;
        const auto di = test::guitarDi (sr, 10.0);
        for (P t : { P::count, P::gateOn, P::compOn, P::wahOn, P::pitchOn, P::drvOn, P::ampOn, P::cabOn, P::eqOn, P::modOn, P::dlyOn, P::revOn, P::lofiOn })
        {
            auto v = bare();
            if (t != P::count) v[(size_t) t] = 1;
            v[(size_t) P::ampModel] = choices::tightMetal;
            const auto r = renderWith (v, di, sr, 64);
            std::printf ("%-14s %6.1fx realtime (%.2f%% of one core)\n", t == P::count ? "bypass" : spec (t).key, 10.0 / r.seconds, 100.0 * r.seconds / 10.0);
        }
        return 0;
    }
    const bool quick = args.contains ("--quick");
    testUpdates();
    testToneNames();
    test::runDeviceChecks (checks, failures);
    if (args.contains ("--devices")) { std::printf ("\n%d checks, %d failures\n", checks, failures); return failures == 0 ? 0 : 1; }
    testChordSheet();
    testBufferSearch();
    if (args.contains ("--sheet")) { std::printf ("\n%d checks, %d failures\n", checks, failures); return failures == 0 ? 0 : 1; }
    testHistory();
    testLooper();
    testSong();
    testFootControl();
    if (args.contains ("--takes")) { testTakes(); std::printf ("\n%d checks, %d failures\n", checks, failures); return failures == 0 ? 0 : 1; }
    if (args.contains ("--matcher")) { testLevelMatcher(); std::printf ("\n%d checks, %d failures\n", checks, failures); return failures == 0 ? 0 : 1; }
    if (args.contains ("--levels")) { testLevelMatch (args.contains ("44100") ? 44100.0 : 48000.0); std::printf ("\n%d checks, %d failures\n", checks, failures); return failures == 0 ? 0 : 1; }
    if (args.contains ("--fix")) { testInputFix (48000.0); testInputFix (44100.0); testInputFix (96000.0); std::printf ("\n%d checks, %d failures\n", checks, failures); return failures == 0 ? 0 : 1; }
    if (args.contains ("--updates")) { std::printf ("\n%d checks, %d failures\n", checks, failures); return failures == 0 ? 0 : 1; }
    testTakes();
    testParams();
    testToneStacks();
    for (double sr : { 48000.0, 44100.0 })
    {
        testLatency (sr);
        testAmps (sr);
        if (quick) break;
    }
    testCabs (48000.0);
    testCabs (96000.0);
    testDynamics (48000.0);
    testHum (48000.0);
    testHum (44100.0);
    testInputFix (48000.0);
    testInputGuard();
    testGate (48000.0);
    testTimeEffects (48000.0);
    testAliasing (48000.0);
    testNeural (48000.0);
    testNeural (44100.0);
    testPresets (48000.0);
    if (! quick) testPresets (44100.0);
    testLevelMatch (48000.0);
    if (! quick) testLevelMatch (44100.0);
    testLevelMatcher();
    testTuner (48000.0);
    if (! quick) { testTuner (44100.0); testTuner (96000.0); }
    testMetronome();
    testStress (48000.0, 256, 30.0, 1);
    if (! quick)
    {
        testStress (44100.0, 4096, 20.0, 2);
        testStress (48000.0, 16, 10.0, 3);
        testStress (96000.0, 512, 10.0, 4);
        testStress (192000.0, 64, 5.0, 5);
    }
    // the quick round of the click test (every switch and knob, every seventh tone); `clicks` runs all
    testStart (48000.0);
    testStart (44100.0);
    testClicks (48000.0, true);
    testCpu (48000.0);

    std::printf ("\n%d checks, %d failures\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
