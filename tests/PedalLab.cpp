// Riffhouse - free, open-source guitar rig
// Copyright (C) 2026 Riffhouse contributors. Licensed under AGPL-3.0-or-later.
//
// The pedal lab. Each drive pedal is built here a second time as its circuit, run sixteen
// times oversampled in double precision with Newton's method run to convergence: that is the
// "original" the app's pedal is held against. The score says how close the app's pedal comes,
// across knob settings and playing strengths:
//   - harmonics: the level of each harmonic for soft, medium and hard notes, low and high
//   - tone: the spectrum of a played guitar part through both, level matched
//   - level: how loud it is with the knobs at the same place
//   - aliasing: tones that are not harmonics of the note (the digital grit real pedals lack)
// 100 is identical. Run: riffhouse_pedal_lab [pedal number] [--wav folder]
#include "dsp/Circuits.h"
#include "dsp/Amp.h"
#include "dsp/Pedals.h"
#include "engine/ReferenceGuitar.h"
#include "engine/Presets.h"
#include "engine/Rig.h"

#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_dsp/juce_dsp.h>

#include <cstdio>
#include <functional>
#include <memory>

using namespace rh;
namespace c = rh::circuit;

namespace
{
constexpr double kSr = 48000.0;
constexpr int kOsStages = 4; // 16x: 768 kHz

// ---------------------------------------------------------------------------------------------
// The originals, as circuits. Input and output in volts; knobs 0..1.
struct Original
{
    virtual ~Original() = default;
    virtual void prepare (double fs) = 0;
    virtual void set (double drive, double tone) = 0;
    virtual double process (double v) = 0;
};

template <typename Circuit>
struct Exact : Original
{
    Circuit circuit;
    void prepare (double fs) override { circuit.prepare (fs, 40); }
    void set (double drive, double tone) override { circuit.set (drive, tone); }
    double process (double v) override { return circuit.process (v); }
};

std::unique_ptr<Original> makeOriginal (int type)
{
    switch (type)
    {
        case 2: return std::make_unique<Exact<c::GreenDrive>>();
        case 4: return std::make_unique<Exact<c::RodentDist>>();
        case 5: return std::make_unique<Exact<c::BigFuzz>>();
        default: return {};
    }
}

const char* pedalName (int type)
{
    static const char* names[] = { "Clean Boost", "Treble Boost", "Green Drive", "Blues Drive", "Rodent Dist", "Big Fuzz", "Metal Box" };
    return names[std::clamp (type, 0, 6)];
}

// Run the original over a 48 kHz signal, sixteen times oversampled.
std::vector<float> runOriginal (Original& o, const std::vector<float>& in, double drive, double tone)
{
    const int block = 512;
    juce::dsp::Oversampling<float> os (1, kOsStages, juce::dsp::Oversampling<float>::filterHalfBandFIREquiripple, true, false);
    os.initProcessing ((size_t) block);
    o.prepare (kSr * (1 << kOsStages));
    o.set (drive, tone);
    std::vector<float> out (in);
    for (size_t at = 0; at < out.size(); at += block)
    {
        const int n = (int) std::min<size_t> (block, out.size() - at);
        float* ch[] = { out.data() + at };
        juce::dsp::AudioBlock<float> b (ch, 1, (size_t) n);
        auto up = os.processSamplesUp (b);
        float* x = up.getChannelPointer (0);
        for (size_t i = 0; i < up.getNumSamples(); ++i)
            x[i] = (float) (o.process ((double) x[i] * c::kVoltsPerUnit) / c::kVoltsPerUnit);
        os.processSamplesDown (b);
    }
    return out;
}

std::vector<float> runApp (int type, const std::vector<float>& in, double drive, double tone)
{
    DrivePedal p;
    p.prepare (kSr, 512);
    p.set (type, (float) drive * 10.0f, (float) tone * 10.0f, 5.0f);
    std::vector<float> out (in);
    for (size_t at = 0; at < out.size(); at += 512)
        p.process (out.data() + at, (int) std::min<size_t> (512, out.size() - at));
    return out;
}

// ---------------------------------------------------------------------------------------------
// Measuring
std::vector<float> sine (double hz, double amp, double seconds)
{
    std::vector<float> s ((size_t) (seconds * kSr));
    for (size_t i = 0; i < s.size(); ++i)
    {
        const double fade = std::min (1.0, (double) i / (0.01 * kSr));
        s[i] = (float) (amp * fade * std::sin (kTwoPi * hz * (double) i / kSr));
    }
    return s;
}

// Magnitude spectrum of the last `n` samples (Blackman-Harris window), as power per bin.
std::vector<double> power (const std::vector<float>& x, int order)
{
    const int n = 1 << order;
    juce::dsp::FFT fft (order);
    std::vector<float> buf ((size_t) n * 2, 0.0f);
    const size_t from = x.size() - (size_t) n;
    for (int i = 0; i < n; ++i)
    {
        const double t = (double) i / (n - 1);
        const double w = 0.35875 - 0.48829 * std::cos (kTwoPi * t) + 0.14128 * std::cos (2 * kTwoPi * t) - 0.01168 * std::cos (3 * kTwoPi * t);
        buf[(size_t) i] = (float) (x[from + (size_t) i] * w);
    }
    fft.performFrequencyOnlyForwardTransform (buf.data());
    std::vector<double> p ((size_t) n / 2);
    for (int i = 0; i < n / 2; ++i) p[(size_t) i] = (double) buf[(size_t) i] * buf[(size_t) i];
    return p;
}

double bandPower (const std::vector<double>& p, double hz, int order, int halfWidth = 4)
{
    const double binHz = kSr / (1 << order);
    const int c0 = (int) std::round (hz / binHz);
    double s = 0;
    for (int k = std::max (1, c0 - halfWidth); k <= std::min ((int) p.size() - 1, c0 + halfWidth); ++k) s += p[(size_t) k];
    return s;
}

double db (double p) { return 10.0 * std::log10 (std::max (p, 1e-30)); }

struct Score
{
    double harmonics = 0, tone = 0, level = 0, alias = 0;
    double total() const { return std::max (0.0, 100.0 - 1.6 * harmonics - 2.0 * tone - 0.8 * level - 0.5 * alias); }
};

// Harmonics 1..10 of a sine through both, each heard against the original's fundamental;
// what is more than 70 dB down on both is too quiet to matter.
void harmonicError (const std::vector<float>& orig, const std::vector<float>& app, double hz, double& err, double& levelErr)
{
    const int order = 15;
    const auto po = power (orig, order), pa = power (app, order);
    const double ref = db (bandPower (po, hz, order));
    double sum = 0; int count = 0;
    for (int h = 1; h <= 10; ++h)
    {
        if (hz * h > 20000.0) break;
        const double o = db (bandPower (po, hz * h, order)) - ref, a = db (bandPower (pa, hz * h, order)) - ref;
        if (o < -70.0 && a < -70.0) continue;
        sum += std::min (20.0, std::abs (std::max (o, -70.0) - std::max (a, -70.0)));
        ++count;
    }
    err = count > 0 ? sum / count : 0.0;
    levelErr = std::min (20.0, std::abs (db (bandPower (pa, hz, order)) - ref));
}

// Everything that is not a harmonic of `hz`, against the whole output, in dB.
double aliasDb (const std::vector<float>& x, double hz)
{
    const int order = 15;
    const auto p = power (x, order);
    const double binHz = kSr / (1 << order);
    double total = 0, harm = 0;
    for (size_t k = 1; k < p.size(); ++k)
    {
        const double f = (double) k * binHz;
        total += p[k];
        const double ratio = f / hz;
        if (std::abs (ratio - std::round (ratio)) * hz < 6.0 * binHz) harm += p[k];
    }
    return db (std::max (total - harm, 1e-30)) - db (total);
}

// Third-octave spectra of a played part, level matched; the mean distance in dB.
double spectrumError (const std::vector<float>& orig, const std::vector<float>& app)
{
    const int order = 13, n = 1 << order;
    std::vector<double> po (n / 2, 0.0), pa (n / 2, 0.0);
    for (size_t at = 0; at + (size_t) n <= orig.size(); at += (size_t) n / 2)
    {
        std::vector<float> a (orig.begin() + (long) at, orig.begin() + (long) at + n), b (app.begin() + (long) at, app.begin() + (long) at + n);
        const auto x = power (a, order), y = power (b, order);
        for (int k = 0; k < n / 2; ++k) { po[(size_t) k] += x[(size_t) k]; pa[(size_t) k] += y[(size_t) k]; }
    }
    double so = 0, sa = 0;
    for (int k = 0; k < n / 2; ++k) { so += po[(size_t) k]; sa += pa[(size_t) k]; }
    const double binHz = kSr / n;
    double sum = 0; int count = 0;
    for (double f = 63.0; f < 12000.0; f *= std::pow (2.0, 1.0 / 3.0))
    {
        const int lo = (int) (f / std::pow (2.0, 1.0 / 6.0) / binHz), hi = (int) (f * std::pow (2.0, 1.0 / 6.0) / binHz);
        double bo = 0, ba = 0;
        for (int k = lo; k <= hi; ++k) { bo += po[(size_t) k]; ba += pa[(size_t) k]; }
        sum += std::min (20.0, std::abs (db (bo / so) - db (ba / sa)));
        ++count;
    }
    return sum / count;
}

void writeWav (const juce::File& f, const std::vector<float>& x)
{
    juce::WavAudioFormat wav;
    f.deleteFile();
    std::unique_ptr<juce::OutputStream> stream = std::make_unique<juce::FileOutputStream> (f);
    auto w = wav.createWriterFor (stream, juce::AudioFormatWriterOptions{}.withSampleRate (kSr).withNumChannels (1).withBitsPerSample (24));
    if (w == nullptr) return;
    float peak = 1e-6f;
    for (auto v : x) peak = std::max (peak, std::abs (v));
    std::vector<float> y (x);
    for (auto& v : y) v *= 0.5f / peak;
    const float* ch[] = { y.data() };
    w->writeFromFloatArrays (ch, 1, (int) y.size());
}

Score scorePedal (int type, bool verbose, const juce::File& wavDir)
{
    auto original = makeOriginal (type);
    struct Setting { double drive, tone; };
    const Setting settings[] = { { 0.2, 0.5 }, { 0.5, 0.5 }, { 0.85, 0.5 }, { 0.5, 0.15 }, { 0.5, 0.85 } };
    const double freqs[] = { 110.0, 330.0, 880.0 };
    const double amps[] = { 0.02, 0.08, 0.3 }; // soft, medium and hard notes: 40, 160 and 600 mV
    const auto guitar = playedGuitar (kSr, 6.0, 0.25f, true);

    Score total;
    int nSettings = 0;
    for (const auto& s : settings)
    {
        Score sc;
        int n = 0;
        for (double hz : freqs)
            for (double a : amps)
            {
                const auto in = sine (hz, a, 1.0);
                double e, l;
                harmonicError (runOriginal (*original, in, s.drive, s.tone), runApp (type, in, s.drive, s.tone), hz, e, l);
                sc.harmonics += e; sc.level += l; ++n;
            }
        sc.harmonics /= n; sc.level /= n;
        const auto hi = sine (2637.0, 0.3, 1.0);
        sc.alias = std::max (0.0, aliasDb (runApp (type, hi, s.drive, s.tone), 2637.0) + 90.0);
        const auto og = runOriginal (*original, guitar, s.drive, s.tone), ag = runApp (type, guitar, s.drive, s.tone);
        sc.tone = spectrumError (og, ag);
        if (verbose)
            std::printf ("  drive %.2f tone %.2f: harmonics %5.2f dB, tone %5.2f dB, level %5.2f dB, aliasing %+6.1f dB -> %5.1f\n",
                         s.drive, s.tone, sc.harmonics, sc.tone, sc.level, sc.alias - 90.0, sc.total());
        if (wavDir != juce::File())
        {
            const auto stem = juce::String (pedalName (type)).replaceCharacter (' ', '-') + "-d" + juce::String (s.drive, 2) + "-t" + juce::String (s.tone, 2);
            writeWav (wavDir.getChildFile (stem + "-original.wav"), og);
            writeWav (wavDir.getChildFile (stem + "-app.wav"), ag);
        }
        total.harmonics += sc.harmonics; total.tone += sc.tone; total.level += sc.level; total.alias += sc.alias;
        ++nSettings;
    }
    total.harmonics /= nSettings; total.tone /= nSettings; total.level /= nSettings; total.alias /= nSettings;
    return total;
}

// ---------------------------------------------------------------------------------------------
// The gate lab. A riff a metal player really plays: palm-muted chugs, the hand stopping the
// strings dead, a chord left to ring, rests, all over the hiss and hum of a real guitar lead.
// It goes through a whole tone (gate, drive, amp, cab) and the numbers say:
//   - rest: how loud the rests are, against the playing (a real rig with a good gate: silent)
//   - stop: how long after the hand stops the strings until the sound is 40 dB down
//   - chop: where a ringing chord gets cut, as the level going in (it should ring to the threshold)
//   - soft: soft palm-muted notes with a loud pick click must come through whole
struct Riff
{
    std::vector<float> x;
    std::vector<double> stops;          // the hand stops the strings here
    std::vector<std::pair<double, double>> rests, chugs; // (from, to) seconds
    double ringFrom = 0, ringTo = 0;
    std::vector<double> soft;           // soft notes with a sharp pick: the gate must leave them whole
};

Riff makeRiff (double sr, float noiseDb)
{
    Riff r;
    const double seconds = 11.0;
    r.x.assign ((size_t) (seconds * sr), 0.0f);
    juce::Random rnd (7);
    // a note: a power chord (root, fifth, octave) with the pick's click, each partial decaying
    auto note = [&] (double t0, double t1, double f0, float amp, bool palm, bool handStop, float pick = 0.15f)
    {
        const size_t a = (size_t) (t0 * sr), b = std::min (r.x.size(), (size_t) ((t1 + (handStop ? 0.08 : 0.0)) * sr));
        const double ratios[] = { 1.0, 1.5, 2.0 };
        for (double ratio : ratios)
            for (int k = 1; k <= 24; ++k)
            {
                const double f = f0 * ratio * k * (1.0 + 0.0004 * k * k); // a little inharmonic, as strings are
                if (f > 9000.0) break;
                const double tau = palm ? 0.09 / (1.0 + 0.7 * k) : 1.4 / (1.0 + 0.12 * k);
                const double g = amp / (double) k / 3.0 * (palm ? 1.0 : 0.8);
                const double ph = rnd.nextDouble() * 6.2832;
                for (size_t i = a; i < b; ++i)
                {
                    const double t = (double) (i - a) / sr;
                    double env = std::exp (-t / tau) * std::min (1.0, t / 0.0015);
                    const double tAfter = (double) i / sr - t1;
                    if (tAfter > 0) env *= handStop ? std::exp (-tAfter / 0.006) : 0.0;
                    r.x[i] += (float) (g * env * std::sin (6.2832 * f * t + ph));
                }
            }
        for (size_t i = a; i < std::min (b, a + (size_t) (0.003 * sr)); ++i) // the pick
            r.x[i] += (float) (amp * pick * (rnd.nextDouble() * 2 - 1) * (1.0 - (double) (i - a) / (0.003 * sr)));
    };
    const double c2 = 65.41, step = 60.0 / 180.0 / 2.0; // drop C, eighths at 180 bpm
    double t = 2.0; // two seconds of the guitar just sitting there first, as before anyone plays
    auto chugs = [&] (int n)
    {
        const double from = t;
        for (int i = 0; i < n; ++i, t += step)
            note (t, t + step, c2, 0.22f, true, i == n - 1);
        r.chugs.push_back ({ from, t });
        r.stops.push_back (t);
    };
    chugs (8);  r.rests.push_back ({ t + 0.1, t + 0.4 }); t += 0.4;
    chugs (4);  r.rests.push_back ({ t + 0.1, t + 0.3 }); t += 0.3;
    r.ringFrom = t; note (t, t + 2.2, c2, 0.32f, false, true); t += 2.2; r.ringTo = t;
    r.stops.push_back (t);
    r.rests.push_back ({ t + 0.15, t + 0.9 });
    t += 1.0;
    for (int i = 0; i < 6; ++i, t += 0.25) // soft palm-muted notes whose pick click stands 20 dB over them
    {
        note (t, t + 0.25, 110.0, 0.012f, true, false, 12.0f);
        r.soft.push_back (t);
    }
    r.rests.insert (r.rests.begin(), { 1.8, 1.95 });
    // the lead's noise: hiss and mains hum with its odd harmonics, as a single coil picks it up
    const float n = juce::Decibels::decibelsToGain (noiseDb);
    for (size_t i = 0; i < r.x.size(); ++i)
    {
        const double tt = (double) i / sr;
        r.x[i] += n * (0.5f * (float) (rnd.nextDouble() * 2 - 1) * 1.7f
                       + 0.6f * (float) (std::sin (6.2832 * 50 * tt) + 0.5 * std::sin (6.2832 * 150 * tt) + 0.3 * std::sin (6.2832 * 250 * tt)));
    }
    return r;
}

std::vector<float> runTone (const ParamValues& v, const std::vector<float>& in, double sr)
{
    Rig rig;
    rig.setOffline (true);
    rig.setParams (v);
    rig.prepare (sr, 256);
    rig.setParams (v);
    rig.finishPendingWork();
    std::vector<float> l (in.size()), rr (in.size());
    for (size_t pos = 0; pos < in.size(); pos += 256)
    {
        const int n = (int) std::min<size_t> (256, in.size() - pos);
        rig.setParams (v);
        rig.process (in.data() + pos, l.data() + pos, rr.data() + pos, n);
    }
    return l;
}

double rmsDbOf (const std::vector<float>& x, double from, double to, double sr)
{
    const size_t a = (size_t) std::max (0.0, from * sr), b = std::min (x.size(), (size_t) (to * sr));
    double acc = 0;
    for (size_t i = a; i < b; ++i) acc += (double) x[i] * x[i];
    return 10.0 * std::log10 (acc / std::max<size_t> (1, b - a) + 1e-20);
}

int gateLab (const juce::String& presetName, const juce::File& wavDir)
{
    PresetLibrary lib;
    lib.scanFolder (juce::File (RIFFHOUSE_PRESET_DIR), true);
    const int at = lib.indexOf (presetName);
    if (at < 0) { std::printf ("no preset %s\n", presetName.toRawUTF8()); return 1; }
    const auto& preset = lib.all()[(size_t) at];
    double sumScore = 0; int nScores = 0;
    for (float noiseDb : { -74.0f, -62.0f })
    {
        const auto riff = makeRiff (kSr, noiseDb);
        auto v = preset.values;
        v[(size_t) P::outLevel] = -24.0f; // under the safety limiter, so it does not squash the numbers
        const auto out = runTone (v, riff.x, kSr);
        v[(size_t) P::gateOn] = 0.0f;
        const auto open = runTone (v, riff.x, kSr);
        double play = -200;
        for (auto c : riff.chugs) play = std::max (play, rmsDbOf (out, c.first, c.second, kSr));
        double rest = -200;
        for (auto rr : riff.rests) rest = std::max (rest, rmsDbOf (out, rr.first, rr.second, kSr));
        // stop: 5 ms windows after each stop until 40 dB under the playing
        double stopMs = 0;
        for (double s : riff.stops)
        {
            double ms = 400;
            for (double w = s; w < s + 0.4; w += 0.001)
                if (rmsDbOf (out, w, w + 0.005, kSr) < play - 40.0) { ms = (w - s) * 1000.0; break; }
            stopMs = std::max (stopMs, ms);
        }
        // chop: the first 10 ms where the gated chord is 6 dB under the ungated one; the level going in there
        double chopIn = -200;
        for (double w = riff.ringFrom + 0.05; w < riff.ringTo - 0.01; w += 0.005)
            if (rmsDbOf (out, w, w + 0.01, kSr) < rmsDbOf (open, w, w + 0.01, kSr) - 6.0) { chopIn = rmsDbOf (riff.x, w - 0.03, w, kSr); break; }
        const double restBelow = play - rest;
        double softLoss = 0;
        for (double t0 : riff.soft) softLoss = std::max (softLoss, rmsDbOf (open, t0, t0 + 0.15, kSr) - rmsDbOf (out, t0, t0 + 0.15, kSr));
        // what a tight rig does: rests silent (60 dB down), stops in 40 ms, chords ring to the threshold
        const double thr = preset.values[(size_t) P::gateThresh];
        const double score = 100.0 - 0.6 * std::max (0.0, 60.0 - restBelow) - 0.25 * std::max (0.0, stopMs - 40.0)
                           - 1.0 * std::max (0.0, chopIn - (thr + 6.0)) - 5.0 * std::max (0.0, softLoss - 0.5);
        std::printf ("noise %.0f dB: playing %.1f dB, rests %.1f dB under it, stop %.0f ms to -40 dB, chord cut at %.1f dB in (threshold %.0f), soft notes lose %.1f dB -> %.1f\n",
                     noiseDb, play, restBelow, stopMs, chopIn, thr, softLoss, score);
        sumScore += score; ++nScores;
        if (wavDir != juce::File())
        {
            const auto stem = "gate-noise" + juce::String ((int) -noiseDb);
            writeWav (wavDir.getChildFile (stem + "-in.wav"), riff.x);
            writeWav (wavDir.getChildFile (stem + "-out.wav"), out);
            writeWav (wavDir.getChildFile (stem + "-nogate.wav"), open);
            NoiseGate g; g.prepare (kSr); g.set (preset.values[(size_t) P::gateThresh], preset.values[(size_t) P::gateRelease]);
            auto y = riff.x; std::vector<float> gains (y.size());
            for (size_t i = 0; i < y.size(); i += 256) g.process (y.data() + i, (int) std::min<size_t> (256, y.size() - i), gains.data() + i);
            juce::File (wavDir.getChildFile (stem + "-gains.txt")).replaceWithText ([&] { juce::String t; for (size_t i = 0; i < gains.size(); i += 48) t << gains[i] << "\n"; return t; }());
        }
    }
    std::printf ("Gate score %.1f\n", sumScore / nScores);
    return 0;
}
} // namespace

// ---------------------------------------------------------------------------------------------
// The amp lab: each amp with its preamp as real triodes against the earlier curve model, on a
// played guitar, at low, middle and high gain. It shows how far the new amps moved (level and
// tone, which presets were made on) and what each one costs and folds back.
std::vector<float> runAmp (int model, float gain, const std::vector<float>& in, bool tubes)
{
    Amp a;
    a.prepare (kSr, 512);
    a.setTubeStages (tubes);
    AmpSettings st; st.model = model; st.gain = gain;
    a.setSettings (st);
    a.reset();
    std::vector<float> out (in);
    for (size_t at = 0; at < out.size(); at += 512)
        a.process (out.data() + at, (int) std::min<size_t> (512, out.size() - at));
    return out;
}

double rmsDbAll (const std::vector<float>& x)
{
    double acc = 0;
    for (float v : x) acc += (double) v * v;
    return 10.0 * std::log10 (acc / std::max<size_t> (1, x.size()) + 1e-20);
}

int ampLab (const juce::File& wavDir, const std::vector<float>& guitar)
{
    for (int model = 0; model < numAmpVoicings(); ++model)
    {
        std::printf ("%s\n", ampVoicing (model).name);
        for (float gain : { 2.0f, 5.0f, 8.0f })
        {
            const auto o = runAmp (model, gain, guitar, false), t = runAmp (model, gain, guitar, true);
            const auto so = runAmp (model, gain, sine (2637.0, 0.3, 1.0), false), stt = runAmp (model, gain, sine (2637.0, 0.3, 1.0), true);
            std::printf ("  gain %.0f: level %+5.1f dB, tone %.2f dB apart, fold-back %.1f dB (was %.1f)\n",
                         gain, rmsDbAll (t) - rmsDbAll (o), spectrumError (o, t), aliasDb (stt, 2637.0), aliasDb (so, 2637.0));
            if (wavDir != juce::File())
            {
                const auto stem = juce::String (ampVoicing (model).name).replaceCharacter (' ', '-') + "-g" + juce::String ((int) gain);
                writeWav (wavDir.getChildFile (stem + "-curve.wav"), o);
                writeWav (wavDir.getChildFile (stem + "-tubes.wav"), t);
            }
        }
    }
    {
        const auto g = playedGuitar (kSr, 10.0, 0.25f, true);
        for (bool tubes : { false, true })
        {
            const double t0 = juce::Time::getMillisecondCounterHiRes();
            runAmp (7, 7.0f, g, tubes);
            const double ms = juce::Time::getMillisecondCounterHiRes() - t0;
            std::printf ("%s: %.0fx realtime\n", tubes ? "tubes" : "curve", 10000.0 / ms);
        }
    }
    return 0;
}

int main (int argc, char** argv)
{
    int only = -1;
    juce::File wavDir;
    for (int i = 1; i < argc; ++i)
    {
        const juce::String a (argv[i]);
        if (a == "--wav" && i + 1 < argc) { wavDir = juce::File (juce::String (argv[++i])); wavDir.createDirectory(); }
        else if (a.containsOnly ("0123456789")) only = a.getIntValue();
    }
    if (argc > 1 && juce::String (argv[1]) == "--amp")
    {
        // a real dry guitar when one is given (--amp file.wav from to), else the built-in one
        std::vector<float> guitar = playedGuitar (kSr, 8.0, 0.32f, true);
        if (argc > 4 && juce::File (juce::String (argv[2])).existsAsFile())
        {
            juce::AudioFormatManager fm; fm.registerBasicFormats();
            std::unique_ptr<juce::AudioFormatReader> r (fm.createReaderFor (juce::File (juce::String (argv[2]))));
            const auto from = (juce::int64) (std::atof (argv[3]) * r->sampleRate), to = (juce::int64) (std::atof (argv[4]) * r->sampleRate);
            juce::AudioBuffer<float> b (1, (int) (to - from));
            r->read (&b, 0, (int) (to - from), from, true, false);
            guitar.assign (b.getReadPointer (0), b.getReadPointer (0) + b.getNumSamples());
            // as the input level knob would set it: loudest note at about -8 dBFS
            float pk = 1.0e-6f;
            for (float v : guitar) pk = std::max (pk, std::abs (v));
            for (auto& v : guitar) v *= 0.4f / pk;
        }
        return ampLab (wavDir, guitar);
    }
    if (argc > 1 && juce::String (argv[1]) == "--humdebug")
    {
        const auto riff = makeRiff (kSr, -74.0f);
        HumCanceller h; h.prepare (kSr); h.setMains (50);
        int learnt = 0; double acc = 0;
        for (size_t i = 0; i < riff.x.size(); ++i)
        {
            const float y = h.process (riff.x[i]);
            acc += (double) (riff.x[i] - y) * (riff.x[i] - y);
            learnt += h.learning() ? 1 : 0;
            if ((i + 1) % 4800 == 0) { std::printf ("%.1fs removed %.1f dB learning %d%% drift %.3f\n", (double) (i + 1) / kSr, 10 * std::log10 (acc / 4800 + 1e-20), learnt * 100 / 4800, h.driftHz()); acc = 0; learnt = 0; }
        }
        return 0;
    }
    if (argc > 1 && juce::String (argv[1]) == "--gatedebug")
    {
        const auto riff = makeRiff (kSr, -62.0f);
        if (argc > 2)
        {
            PresetLibrary lib; lib.scanFolder (juce::File (RIFFHOUSE_PRESET_DIR), true);
            auto v = lib.all()[(size_t) lib.indexOf ("Inspired by Metallica: Master of Puppets")].values;
            v[(size_t) P::outLevel] = -24.0f;
            Rig rig; rig.setOffline (true); rig.setParams (v); rig.prepare (kSr, 2400); rig.setParams (v); rig.finishPendingWork();
            std::vector<float> l (2400), r (2400);
            for (size_t i = 0; i + 2400 <= riff.x.size(); i += 2400)
            {
                rig.setParams (v);
                rig.process (riff.x.data() + i, l.data(), r.data(), 2400);
                double acc = 0; for (float q : l) acc += (double) q * q;
                std::printf ("%.2fs out %.1f in-peak %.1f thr %.1f mains %d %s\n", (double) i / kSr, 10 * std::log10 (acc / 2400 + 1e-20), 20 * std::log10 (rig.meters().gateInPeak.load() + 1e-9), rig.meters().gateThreshNow.load(), rig.meters().mains.load(), rig.meters().gateOpen.load() ? "OPEN" : "-");
            }
            return 0;
        }
        HumCanceller h; h.prepare (kSr); h.setMains (50);
        NoiseGate g; g.prepare (kSr); g.set (-54.0f, 50.0f);
        auto y = riff.x;
        for (auto& v : y) v = h.process (v);
        for (size_t i = 0; i < y.size(); i += 2400)
        {
            float pk = 0; double acc = 0;
            for (size_t j = i; j < i + 2400 && j < y.size(); ++j) { pk = std::max (pk, std::abs (y[j])); acc += (double) y[j] * y[j]; }
            g.process (y.data() + i, (int) std::min<size_t> (2400, y.size() - i));
            std::printf ("%.2fs peak %.1f rms %.1f thr %.1f floor %.1f %s\n", (double) i / kSr, 20 * std::log10 (pk + 1e-9), 10 * std::log10 (acc / 2400 + 1e-20), g.thresholdNowDb(), g.noiseFloorDb(), g.isOpen() ? "OPEN" : "-");
        }
        return 0;
    }
    if (argc > 1 && juce::String (argv[1]) == "--gate")
        return gateLab (argc > 2 && ! juce::String (argv[2]).startsWith ("--") ? juce::String (argv[2]) : juce::String ("Inspired by Metallica: Master of Puppets"), wavDir);
    if (argc > 1 && juce::String (argv[1]) == "--cpu")
    {
        const auto g = playedGuitar (kSr, 10.0, 0.25f, true);
        for (int type = 0; type < 7; ++type)
        {
            const double t0 = juce::Time::getMillisecondCounterHiRes();
            runApp (type, g, 0.7, 0.5);
            const double ms = juce::Time::getMillisecondCounterHiRes() - t0;
            std::printf ("%-12s %6.1f ms for 10 s of audio: %5.0fx realtime\n", pedalName (type), ms, 10000.0 / ms);
        }
        return 0;
    }
    double sum = 0; int count = 0;
    for (int type : { 2, 4, 5 })
    {
        if (only >= 0 && type != only) continue;
        std::printf ("%s\n", pedalName (type));
        const auto s = scorePedal (type, true, wavDir);
        std::printf ("%s: score %.1f (harmonics %.2f dB, tone %.2f dB, level %.2f dB, aliasing %+.1f dB)\n\n",
                     pedalName (type), s.total(), s.harmonics, s.tone, s.level, s.alias - 90.0);
        sum += s.total(); ++count;
    }
    if (count > 1) std::printf ("Overall %.1f\n", sum / count);
    return 0;
}
