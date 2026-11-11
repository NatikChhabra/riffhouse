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
#include "plugin/LevelMatcher.h"
#include "plugin/Takes.h"

#include <juce_audio_formats/juce_audio_formats.h>
#include <chrono>
#include <cstdio>

#if RIFFHOUSE_HAS_PRESETS
 #include "PresetData.h"
#endif

using namespace rh;

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
}

void testHum (double sr)
{
    std::printf ("hum detection @ %.0f Hz\n", sr);
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
        CHECK (g.thresholdNowDb() <= NoiseGate::kHighestFollowDb + 0.01f && g.thresholdNowDb() > -45.0f, "the gate sits at %.1f dB over a floor of %.1f", g.thresholdNowDb(), g.noiseFloorDb());
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
            CHECK (std::abs (again - loud) < 0.005f, "%s: the same tone reads %.4f then %.4f", p.name.toRawUTF8(), loud, again);
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
        CHECK (most (played) < 0.6f * most (unmatched) && most (played) < 5.0f, "on other playing nine in ten matched tones are within %.1f dB, unmatched %.1f dB", most (played), most (unmatched));
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

// Measured 3.6 with every pedal on (GCC, November 2026). The budget leaves room for a
// different compiler and a noisy machine, and still catches a rig that got a third slower.
constexpr double kRigCostBudget = 5.0;

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
        auto entry = [] (const char* tag, bool pre, bool draft)
        {
            return juce::String ("{\"tag_name\":\"") + tag + "\",\"prerelease\":" + (pre ? "true" : "false") + ",\"draft\":" + (draft ? "true" : "false")
                 + ",\"html_url\":\"https://github.com/NatikChhabra/riffhouse/releases/tag/" + tag + "\",\"body\":\"\",\"assets\":[]}";
        };
        const auto tests = "[" + entry ("v0.1.0-rc1", true, false) + "," + entry ("v0.1.0-rc3", true, false) + "," + entry ("v0.1.0-rc2", true, false) + "," + entry ("v0.1.0-rc9", true, true) + "]";
        CHECK (newestRelease (tests, "").version == "0.1.0-rc3", "newest test release: got '%s'", newestRelease (tests, "").version.toRawUTF8());
        const auto out = "[" + entry ("v0.1.0-rc3", true, false) + "," + entry ("v0.1.0", false, false) + "]";
        CHECK (newestRelease (out, "").version == "0.1.0", "the release beats its test builds");
        CHECK (newestRelease ("[]", "").version.isEmpty() && newestRelease ("not json", "").version.isEmpty(), "an empty or broken list offers nothing");
        CHECK (parseRelease (entry ("v0.1.0-rc3", true, false), "").version.isEmpty(), "a full release never follows test releases");
        CHECK (parseRelease (entry ("v0.1.0-rc3", true, false), "", true).version == "0.1.0-rc3", "a test release is read when asked for");
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

void testTakes()
{
    std::printf ("takes\n");
    const double sr = 48000.0;
    const auto dir = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("riffhouse-takes-test");
    dir.deleteRecursively();
    dir.getChildFile ("DI").createDirectory();
    const auto di = test::guitarDi (sr, 3.0);

    auto v1 = bare();
    v1[(size_t) P::ampOn] = 1; v1[(size_t) P::cabOn] = 1;
    auto v2 = v1;
    v2[(size_t) P::ampModel] = (float) choices::tightMetal;
    v2[(size_t) P::dlyOn] = 1;

    const juce::String base = "2026-01-02 03-04-05 Test Tone";
    const auto wetFile = dir.getChildFile (base + ".wav"), cleanFile = dir.getChildFile ("DI").getChildFile (base + " (DI).wav");
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
    dir.deleteRecursively();
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
        // prints {"preset name": rms dBFS, ...} for tools/make_presets.py --calibrate
        const double sr = 48000.0;
        const auto di = test::guitarDi (sr, 12.0);
        auto* obj = new juce::DynamicObject();
        for (const auto& p : loadPresets())
        {
            auto v = p.values;
            // measure below the safety limiter so it does not skew the numbers
            const float out = v[(size_t) P::outLevel];
            v[(size_t) P::outLevel] = -24.0f;
            const auto r = renderWith (v, di, sr, 512);
            auto* e = new juce::DynamicObject();
            e->setProperty ("rms", 0.5f * (rmsDb (r.l) + rmsDb (r.r)) + 24.0f + out);
            e->setProperty ("peak", gainToDb (std::max (peakOf (r.l), peakOf (r.r))) + 24.0f + out);
            obj->setProperty (p.name, juce::var (e));
        }
        juce::File (juce::File::getCurrentWorkingDirectory().getChildFile (args[1])).replaceWithText (juce::JSON::toString (juce::var (obj)));
        return 0;
    }
    if (args[0] == "play" && args.size() >= 4)
    {
        // play <clean guitar.wav> <preset name> <out.wav> [key=value ...] [trim=dB]
        // Plays a recorded clean guitar through a preset, for listening checks.
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
        auto r = renderWith (v, di, sr, 256);
        const float g = dbToGain (trim);
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
    testHistory();
    testLooper();
    testSong();
    testFootControl();
    if (args.contains ("--takes")) { testTakes(); std::printf ("\n%d checks, %d failures\n", checks, failures); return failures == 0 ? 0 : 1; }
    if (args.contains ("--matcher")) { testLevelMatcher(); std::printf ("\n%d checks, %d failures\n", checks, failures); return failures == 0 ? 0 : 1; }
    if (args.contains ("--levels")) { testLevelMatch (48000.0); std::printf ("\n%d checks, %d failures\n", checks, failures); return failures == 0 ? 0 : 1; }
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
    testCpu (48000.0);

    std::printf ("\n%d checks, %d failures\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
