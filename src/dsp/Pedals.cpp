// Riffhouse - free, open-source guitar rig
// Copyright (C) 2026 Riffhouse contributors. Licensed under AGPL-3.0-or-later.
#include "Pedals.h"

namespace rh
{
//==============================================================================
// InputConditioner
void InputConditioner::prepare (double sampleRate)
{
    sr = sampleRate;
    dc.prepare (sr);
    const double freqs[6] = { 50, 100, 150, 60, 120, 180 };
    for (int i = 0; i < 6; ++i)
        gz[i].coeff = 2.0 * std::cos (kTwoPi * freqs[i] / sr);
    const float amount = fixAmount < 0.0f ? 6.0f : fixAmount;
    fixAmount = -1.0f;
    setFixAmount (amount);
    activeMains = -1;
    setNotches (0);
    reset();
}

void InputConditioner::reset()
{
    dc.reset();
    for (auto& b : notches) b.reset();
    fixShelf.reset(); fixPeak.reset(); fixLow.reset(); fixHiss.reset();
    for (auto& g : gz) g.s1 = g.s2 = 0;
    g50 = g60 = total = 0;
    counted = 0;
}

void InputConditioner::set (int humMode, bool inputFix, float fixAmountDb)
{
    mode = humMode;
    fix = inputFix;
    setFixAmount (fixAmountDb);
    int want = 0;
    if (mode == hum50) want = 50;
    else if (mode == hum60) want = 60;
    else if (mode == humAuto) want = detected.load();
    if (want != activeMains)
        setNotches (want);
}

// Input Fix, sized by how much treble the input is missing. Measured on a
// soundhole magnetic pickup into a PC mic jack: the highs fall away above
// about 300 Hz and need up to +12 dB back around 1.7 to 4 kHz, while above
// 5 kHz there is mostly hiss. So: lift the presence range by the amount, and
// past 6 dB of lift start rolling off the hiss band instead of boosting it.
void InputConditioner::setFixAmount (float db)
{
    db = std::clamp (db, 0.0f, 12.0f);
    if (std::abs (db - fixAmount) < 0.01f) return;
    fixAmount = db;
    fixShelf.set (Biquad::Type::highShelf, sr, 1800.0, 0.6, 0.6 * db);
    fixPeak.set (Biquad::Type::peak, sr, 3000.0, 0.8, 0.5 * db);
    fixLow.set (Biquad::Type::peak, sr, 250.0, 0.9, -0.2 * db);
    fixHiss.set (Biquad::Type::highShelf, sr, 7500.0, 0.7, -std::max (0.0f, db - 6.0f) * 1.5f);
}

void InputConditioner::setNotches (int mainsHz)
{
    activeMains = mainsHz;
    if (mainsHz == 0) return;
    for (int k = 0; k < kHarmonics; ++k)
    {
        // narrow notches: low enough Q to catch drift of +-0.5 Hz, high enough
        // to leave the low E string (82 Hz) and its overtones alone
        const double f = mainsHz * (k + 1);
        notches[k].set (Biquad::Type::notch, sr, f, k == 0 ? 12.0 : 18.0);
        notches[k].reset();
    }
}

void InputConditioner::analyse (const float* x, int n)
{
    for (int i = 0; i < n; ++i)
    {
        const double s = x[i];
        total += s * s;
        for (auto& g : gz)
        {
            const double s0 = s + g.coeff * g.s1 - g.s2;
            g.s2 = g.s1;
            g.s1 = s0;
        }
    }
    counted += n;

    // evaluate every half second
    if (counted >= (int) (sr * 0.5))
    {
        auto power = [] (const Goertzel& g) { return g.s1 * g.s1 + g.s2 * g.s2 - g.coeff * g.s1 * g.s2; };
        const double p50 = power (gz[0]) + power (gz[1]) + power (gz[2]);
        const double p60 = power (gz[3]) + power (gz[4]) + power (gz[5]);
        const double norm = (double) counted * (double) counted * 0.25; // sine of amplitude A gives A^2 * N^2 / 4
        const double rms2 = total / (double) counted;
        const double hum50 = p50 / norm, hum60 = p60 / norm;

        // only count windows where hum is clearly present and stands out, so a
        // played note near 50/60 Hz harmonics does not fool the detector
        const double floor = 1.0e-10; // about -100 dBFS
        if (std::max (hum50, hum60) > floor && std::max (hum50, hum60) > 0.02 * rms2)
        {
            if (hum50 > 4.0 * hum60) ++votes50;
            else if (hum60 > 4.0 * hum50) ++votes60;
        }
        if (votes50 >= 3 && votes50 > votes60 + 2) detected.store (50);
        else if (votes60 >= 3 && votes60 > votes50 + 2) detected.store (60);
        votes50 = std::min (votes50, 20);
        votes60 = std::min (votes60, 20);

        for (auto& g : gz) g.s1 = g.s2 = 0;
        total = 0;
        counted = 0;
    }
}

void InputConditioner::process (float* x, int n)
{
    for (int i = 0; i < n; ++i)
        x[i] = dc.process (x[i]);

    if (mode == humAuto)
    {
        analyse (x, n);
        if (detected.load() != activeMains)
            setNotches (detected.load());
    }

    if (mode != humOff && activeMains > 0)
        for (int i = 0; i < n; ++i)
        {
            float s = x[i];
            for (auto& b : notches) s = b.process (s);
            x[i] = s;
        }

    if (fix)
        for (int i = 0; i < n; ++i)
            x[i] = fixHiss.process (fixLow.process (fixPeak.process (fixShelf.process (x[i]))));
}

//==============================================================================
// TiltProbe
void TiltProbe::prepare (double sampleRate)
{
    sr = sampleRate;
    lowBand.set (Biquad::Type::bandpass, sr, 250.0, 0.9);
    highBand.set (Biquad::Type::bandpass, sr, 3000.0, 0.9);
    lowBand.reset();
    highBand.reset();
}

void TiltProbe::process (const float* x, int n)
{
    if (restartRequested.exchange (false))
    {
        lowE = highE = 0;
        seconds.store (0.0);
        tilt.store (0.0f);
    }
    double sum = 0, lo = 0, hi = 0;
    for (int i = 0; i < n; ++i)
    {
        const float s = x[i];
        const float a = lowBand.process (s), b = highBand.process (s);
        sum += (double) s * s;
        lo += (double) a * a;
        hi += (double) b * b;
    }
    // only count blocks where something is being played, and skip clipped ones
    const double rms = std::sqrt (sum / std::max (1, n));
    float pk = 0.0f;
    for (int i = 0; i < n; ++i) pk = std::max (pk, std::abs (x[i]));
    if (rms < 0.004 || pk > 0.97f) return; // about -48 dBFS
    lowE += lo;
    highE += hi;
    seconds.store (seconds.load() + n / sr);
    if (lowE > 1e-12)
        tilt.store ((float) (10.0 * std::log10 (std::max (highE, 1e-20) / lowE)));
}

float TiltProbe::fixAmountFor (float measuredTiltDb) noexcept
{
    // Small deficits are normal playing variation. Past that, the fix wins
    // back about 0.8 dB of band level per dB of amount, so scale up to match.
    const float deficit = referenceTiltDb - measuredTiltDb;
    if (deficit < 3.0f) return 0.0f;
    return std::clamp (std::round (deficit * 1.25f * 2.0f) * 0.5f, 3.0f, 12.0f);
}

//==============================================================================
// NoiseGate
void NoiseGate::prepare (double sampleRate)
{
    sr = sampleRate;
    env.prepare (sr, 0.3f, 25.0f);
    attackCoeff = (float) std::exp (-1.0 / (0.0008 * sr));
    holdSamples = (int) (0.045 * sr);
    set (-62.0f, 80.0f);
    reset();
}

void NoiseGate::reset() { env.reset(); gain = 0.0f; open = false; holdCounter = 0; }

void NoiseGate::set (float thresholdDb, float releaseMs)
{
    thrOpen = dbToGain (thresholdDb);
    thrClose = dbToGain (thresholdDb - 6.0f); // hysteresis stops chattering
    releaseCoeff = (float) std::exp (-1.0 / (0.001 * std::max (5.0f, releaseMs) * sr));
}

void NoiseGate::process (float* x, int n)
{
    for (int i = 0; i < n; ++i)
    {
        const float e = env.process (x[i]);
        if (e > thrOpen) { open = true; holdCounter = holdSamples; }
        else if (e < thrClose)
        {
            if (holdCounter > 0) --holdCounter;
            else open = false;
        }
        const float target = open ? 1.0f : 0.0f;
        const float c = target > gain ? attackCoeff : releaseCoeff;
        gain = target + (gain - target) * c;
        x[i] *= gain;
    }
}

//==============================================================================
// Compressor
void Compressor::prepare (double sampleRate)
{
    sr = sampleRate;
    set (5, 4, 5, 10);
    reset();
}

void Compressor::reset() { envDb = -120.0f; grDb = 0.0f; }

void Compressor::set (float sustain, float attack, float level, float blend)
{
    const float s = clamp01 (sustain / 10.0f);
    thresholdDb = lerp (-12.0f, -42.0f, s);
    ratio = lerp (2.0f, 10.0f, s);
    // auto makeup: about 60% of the reduction a typical note gets
    const float typical = -18.0f;
    const float reduction = std::max (0.0f, (typical - thresholdDb) * (1.0f - 1.0f / ratio));
    makeup = dbToGain (0.6f * reduction + (level / 10.0f - 0.5f) * 24.0f);
    const float atMs = expMap (attack / 10.0f, 0.5f, 40.0f);
    att = (float) std::exp (-1.0 / (0.001 * atMs * sr));
    rel = (float) std::exp (-1.0 / (0.180 * sr));
    mix = clamp01 (blend / 10.0f);
}

void Compressor::process (float* x, int n)
{
    for (int i = 0; i < n; ++i)
    {
        const float in = x[i];
        const float lvl = gainToDb (std::abs (in));
        envDb = lvl > envDb ? lvl + (envDb - lvl) * att : lvl + (envDb - lvl) * rel;
        // soft knee of 6 dB
        const float over = envDb - thresholdDb;
        float gr = 0.0f;
        if (over > 3.0f) gr = over * (1.0f - 1.0f / ratio);
        else if (over > -3.0f) gr = (1.0f - 1.0f / ratio) * (over + 3.0f) * (over + 3.0f) / 12.0f;
        grDb = gr;
        const float wet = in * dbToGain (-gr) * makeup;
        x[i] = lerp (in, wet, mix);
    }
}

//==============================================================================
// Wah: resonant state-variable filter swept 350 Hz to 2.2 kHz.
void Wah::prepare (double sampleRate)
{
    sr = sampleRate;
    env.prepare (sr, 4.0f, 120.0f);
    posSmooth.prepare (sr, 25.0f);
    posSmooth.reset (0.5f);
    reset();
}

void Wah::reset() { ic1 = ic2 = 0.0f; env.reset(); }

void Wah::set (int m, float p, float s, float q)
{
    mode = m;
    pos = clamp01 (p / 10.0f);
    sens = clamp01 (s / 10.0f);
    res = clamp01 (q / 10.0f);
}

void Wah::process (float* x, int n)
{
    const float k = 1.0f / lerp (2.0f, 9.0f, res); // damping
    for (int i = 0; i < n; ++i)
    {
        float p = pos;
        if (mode == 1)
        {
            const float e = env.process (x[i]);
            p = clamp01 (std::sqrt (e * lerp (4.0f, 60.0f, sens)));
        }
        posSmooth.setTarget (p);
        const float f = expMap (posSmooth.next(), 350.0f, 2200.0f);
        const float g = (float) std::tan (kPi * f / sr);
        const float a1 = 1.0f / (1.0f + g * (g + k));
        const float a2 = g * a1;
        const float a3 = g * a2;
        const float v3 = x[i] - ic2;
        const float v1 = a1 * ic1 + a2 * v3;
        const float v2 = ic2 + a2 * ic1 + a3 * v3;
        ic1 = undenorm (2.0f * v1 - ic1);
        ic2 = undenorm (2.0f * v2 - ic2);
        const float band = v1 * k;
        const float low = v2;
        // a real wah is mostly a resonant bandpass with some low end left in
        x[i] = 2.2f * band + 0.25f * low;
    }
}

//==============================================================================
// PitchPedal
void PitchPedal::prepare (double sampleRate)
{
    sr = sampleRate;
    trackLp.setCutoff (sr, 500.0);
    subLp1.setCutoff (sr, 300.0);
    subLp2.setCutoff (sr, 300.0);
    upHp.setCutoff (sr, 120.0);
    subEnv.prepare (sr, 3.0f, 60.0f);
    window = (int) (0.022 * sr); // 22 ms grains: about 11 ms of shifter latency
    line.prepare (window * 2 + 8);
    reset();
}

void PitchPedal::reset()
{
    trackLp.reset(); subLp1.reset(); subLp2.reset(); upHp.reset(); subEnv.reset();
    flip = 1.0f; lastSign = 0.0f;
    line.reset();
    phase = 0.0f;
}

void PitchPedal::set (int m, float semitones, float mixKnob)
{
    mode = m;
    ratio = std::pow (2.0f, semitones / 12.0f);
    mix = clamp01 (mixKnob / 10.0f);
}

void PitchPedal::process (float* x, int n)
{
    const float dryG = mode == 2 ? std::cos (mix * 1.5707963f) : 1.0f;
    const float wetG = mode == 2 ? std::sin (mix * 1.5707963f) : mix * 1.6f;

    for (int i = 0; i < n; ++i)
    {
        const float in = x[i];
        float wet = 0.0f;
        if (mode == 0)
        {
            // flip-flop divider on the lowpassed signal, like analog octave pedals
            const float t = trackLp.lowpass (in);
            const float sign = t > 0.0f ? 1.0f : -1.0f;
            if (sign > 0.0f && lastSign < 0.0f) flip = -flip;
            lastSign = sign;
            const float e = subEnv.process (t);
            wet = subLp2.lowpass (subLp1.lowpass (flip * e * 1.4f));
        }
        else if (mode == 1)
        {
            // full-wave rectification doubles the frequency
            wet = upHp.highpass (std::abs (in)) * 1.5f;
        }
        else
        {
            // two crossfaded read heads sweeping through a short buffer
            line.push (in);
            phase += (1.0f - ratio) / (float) window;
            phase -= std::floor (phase);
            const float p2 = phase + 0.5f - std::floor (phase + 0.5f);
            const float d1 = 1.0f + phase * (float) window;
            const float d2 = 1.0f + p2 * (float) window;
            const float w1 = std::sin (phase * (float) kPi);
            const float w2 = std::sin (p2 * (float) kPi);
            wet = (line.read (d1) * w1 * w1 + line.read (d2) * w2 * w2);
        }
        x[i] = in * dryG + wet * wetG;
    }
}

//==============================================================================
// DrivePedal
void DrivePedal::prepare (double sampleRate, int maxBlock)
{
    sr = sampleRate;
    const int st = sampleRate < 64000.0 ? 2 : 1;
    os = std::make_unique<juce::dsp::Oversampling<float>> (1, (size_t) st, juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true, false);
    os->initProcessing ((size_t) maxBlock);
    osr = sr * (double) (1 << st);
    dc.prepare (osr);
    gainS.prepare (osr, 30.0f);
    levelS.prepare (osr, 30.0f);
    dirty = true;
    update();
    gainS.reset (gainS.getTarget());
    reset();
}

void DrivePedal::reset()
{
    if (os) os->reset();
    preHp.reset(); preLp.reset(); midHump.reset(); toneLp.reset(); toneHp.reset(); postLp.reset(); scoop.reset();
    clipHp.reset(); gbwLp.reset(); dc.reset();
}

float DrivePedal::latencySamples() const { return os ? os->getLatencyInSamples() : 0.0f; }

void DrivePedal::set (int t, float d, float tn, float lv)
{
    if (t != type || d != drive * 10.0f || tn != tone * 10.0f || lv != level * 10.0f)
    {
        const bool typeChanged = t != type;
        type = t; drive = clamp01 (d / 10.0f); tone = clamp01 (tn / 10.0f); level = clamp01 (lv / 10.0f);
        dirty = true;
        update();
        if (typeChanged) reset();
    }
}

void DrivePedal::update()
{
    if (! dirty) return;
    dirty = false;
    float gainDb = 0.0f, outDb = 0.0f;
    switch (type)
    {
        case 0: // clean boost
            gainDb = lerp (0.0f, 22.0f, drive);
            preHp.set (Biquad::Type::highpass, osr, 25.0, 0.7);
            toneLp.set (Biquad::Type::highShelf, osr, 3000.0, 0.7, (tone - 0.5f) * 12.0f);
            outDb = -3.0f;
            break;
        case 1: // treble booster
            gainDb = lerp (6.0f, 30.0f, drive);
            preHp.set (Biquad::Type::highpass, osr, lerp (400.0f, 1200.0f, 1.0f - tone), 0.6);
            toneLp.set (Biquad::Type::lowpass, osr, 9000.0, 0.7);
            outDb = -6.0f;
            break;
        case 2: // green mid-hump overdrive
            gainDb = lerp (8.0f, 40.0f, drive);
            clipHp.setCutoff (osr, 720.0);
            preHp.set (Biquad::Type::highpass, osr, 120.0, 0.7);
            toneLp.set (Biquad::Type::lowpass, osr, expMap (tone, 700.0f, 6000.0f), 0.7);
            midHump.set (Biquad::Type::peak, osr, 750.0, 0.8, 4.0);
            clean = 0.35f;
            outDb = -2.0f;
            break;
        case 3: // transparent blues drive
            gainDb = lerp (4.0f, 34.0f, drive);
            preHp.set (Biquad::Type::highpass, osr, 80.0, 0.7);
            toneLp.set (Biquad::Type::highShelf, osr, 2000.0, 0.7, (tone - 0.5f) * 14.0f);
            clean = 0.25f;
            outDb = -3.0f;
            break;
        case 4: // rodent op-amp distortion
            gainDb = lerp (20.0f, 62.0f, drive);
            preHp.set (Biquad::Type::highpass, osr, 60.0, 0.7);
            // op-amp bandwidth shrinks as gain rises
            gbwLp.setCutoff (osr, expMap (1.0f - drive, 3500.0f, 30000.0f));
            toneLp.set (Biquad::Type::lowpass, osr, expMap (1.0f - tone, 475.0f, 18000.0f), 0.7);
            outDb = -10.0f;
            break;
        case 5: // big fuzz
            gainDb = lerp (24.0f, 54.0f, drive);
            preHp.set (Biquad::Type::highpass, osr, 90.0, 0.7);
            toneLp.set (Biquad::Type::lowpass, osr, 1000.0, 0.7);
            toneHp.set (Biquad::Type::highpass, osr, 400.0, 0.7);
            outDb = -6.0f;
            break;
        default: // metal box
            gainDb = lerp (26.0f, 60.0f, drive);
            preHp.set (Biquad::Type::highpass, osr, 150.0, 0.7);
            midHump.set (Biquad::Type::peak, osr, 1200.0, 0.9, 6.0);
            scoop.set (Biquad::Type::peak, osr, 650.0, 0.7, lerp (2.0f, -14.0f, tone));
            toneLp.set (Biquad::Type::highShelf, osr, 3500.0, 0.6, lerp (-6.0f, 8.0f, tone));
            outDb = -12.0f;
            break;
    }
    postLp.set (Biquad::Type::lowpass, osr, 12000.0, 0.6);
    gainS.setTarget (dbToGain (gainDb));
    levelS.setTarget (dbToGain (outDb + (level - 0.5f) * 30.0f));
}

void DrivePedal::process (float* data, int n)
{
    if (! os) return;
    float* chans[] = { data };
    juce::dsp::AudioBlock<float> block (chans, 1, (size_t) n);
    auto up = os->processSamplesUp (block);
    float* x = up.getChannelPointer (0);
    const int on = (int) up.getNumSamples();

    for (int i = 0; i < on; ++i)
    {
        const float in = preHp.process (x[i]);
        const float g = gainS.next();
        float y = 0.0f;
        switch (type)
        {
            case 0:
            {
                const float v = in * g;
                y = toneLp.process (v / (1.0f + 0.15f * std::abs (v))); // op-amp rail softness
                break;
            }
            case 1:
            {
                // germanium transistor: asymmetric and soft
                const float v = in * g;
                y = v > 0 ? fastTanh (v) : fastTanh (1.6f * v) / 1.6f;
                y = toneLp.process (dc.process (y));
                break;
            }
            case 2:
            {
                // diodes in the feedback loop clip only the boosted highpassed part,
                // the clean signal passes alongside it
                const float boosted = clipHp.highpass (in) * g;
                const float clipped = boosted / std::sqrt (1.0f + boosted * boosted);
                y = toneLp.process (midHump.process (in * clean + clipped));
                break;
            }
            case 3:
            {
                const float v = in * g;
                const float c = v > 0 ? fastTanh (v) : fastTanh (0.8f * v) / 0.8f;
                y = toneLp.process (dc.process (c + in * clean * 2.0f));
                break;
            }
            case 4:
            {
                const float v = gbwLp.lowpass (in * g);
                // silicon diodes to ground: hard-ish knee
                const float v2 = v * v;
                y = v / std::sqrt (std::sqrt (1.0f + v2 * v2));
                y = toneLp.process (y);
                break;
            }
            case 5:
            {
                float v = fastTanh (in * g * 0.3f);
                v = fastTanh (v * 6.0f);
                // tone: blend of a lowpass and a highpass leaves a mid scoop
                const float lo = toneLp.process (v), hi = toneHp.process (v);
                y = lerp (lo, hi, tone) * 1.4f;
                break;
            }
            default:
            {
                float v = midHump.process (in) * g;
                const float v2 = v * v;
                v = v / std::sqrt (std::sqrt (1.0f + v2 * v2));
                v = fastTanh (v * 2.0f);
                y = toneLp.process (scoop.process (v));
                break;
            }
        }
        x[i] = postLp.process (y) * levelS.next();
    }
    os->processSamplesDown (block);
}

//==============================================================================
// GraphicEq
void GraphicEq::prepare (double sampleRate)
{
    sr = sampleRate;
    const float z[7] {};
    initialised = false;
    set (z, 0.0f);
    reset();
}

void GraphicEq::reset() { for (auto& b : bands) b.reset(); }

void GraphicEq::set (const float* g, float levelDb)
{
    static const double freqs[7] = { 100, 200, 400, 800, 1600, 3200, 6400 };
    for (int i = 0; i < 7; ++i)
        if (g[i] != gains[i] || ! initialised)
        {
            gains[i] = g[i];
            bands[i].set (Biquad::Type::peak, sr, freqs[i], 1.4, g[i]);
        }
    initialised = true;
    level = dbToGain (levelDb);
}

void GraphicEq::process (float* x, int n)
{
    for (int i = 0; i < n; ++i)
    {
        float s = x[i];
        for (int b = 0; b < 7; ++b)
            if (gains[b] != 0.0f) s = bands[b].process (s);
        x[i] = s * level;
    }
}

//==============================================================================
// Modulation
void Modulation::prepare (double sampleRate)
{
    sr = sampleRate;
    lfo.prepare (sr);
    dl.prepare ((int) (0.05 * sr));
    dr.prepare ((int) (0.05 * sr));
    depthS.prepare (sr, 40.0f);
    mixS.prepare (sr, 40.0f);
    reset();
}

void Modulation::reset()
{
    dl.reset(); dr.reset();
    fbL = fbR = 0.0f;
    std::fill (std::begin (apL), std::end (apL), 0.0f);
    std::fill (std::begin (apR), std::end (apR), 0.0f);
}

void Modulation::set (int t, float rate, float d, float m)
{
    if (t != type) { type = t; reset(); }
    // vibe and tremolo like faster top speeds
    rateHz = expMap (rate / 10.0f, 0.05f, type == 3 ? 14.0f : 10.0f);
    lfo.setRate (rateHz);
    depthS.setTarget (clamp01 (d / 10.0f));
    mixS.setTarget (clamp01 (m / 10.0f));
}

void Modulation::process (float* l, float* r, int n)
{
    const float msToS = (float) (0.001 * sr);
    for (int i = 0; i < n; ++i)
    {
        const float d = depthS.next(), m = mixS.next();
        const float inL = l[i], inR = r[i];
        float outL = inL, outR = inR;

        switch (type)
        {
            case 0: // chorus: two modulated voices in quadrature
            case 4: // vibrato: the same, fully wet
            {
                dl.push (inL); dr.push (inR);
                const float base = type == 0 ? 8.0f : 5.0f;
                const float sweep = (type == 0 ? 4.0f : 3.0f) * d;
                const float wl = dl.read ((base + sweep * (0.5f + 0.5f * lfo.sine (0.0f))) * msToS);
                const float wr = dr.read ((base + sweep * (0.5f + 0.5f * lfo.sine (0.25f))) * msToS);
                if (type == 0)
                {
                    const float wet = m * 0.8f;
                    outL = inL * (1.0f - 0.35f * m) + wl * wet;
                    outR = inR * (1.0f - 0.35f * m) + wr * wet;
                }
                else
                {
                    outL = lerp (inL, wl, 0.3f + 0.7f * m);
                    outR = lerp (inR, wr, 0.3f + 0.7f * m);
                }
                break;
            }
            case 1: // flanger, mix knob sets the feedback
            {
                const float fb = m * 0.85f;
                dl.push (inL + fbL * fb);
                dr.push (inR + fbR * fb);
                const float tri = lfo.triangle (0.0f), tri2 = lfo.triangle (0.25f);
                fbL = dl.read ((0.4f + d * 5.0f * (0.5f + 0.5f * tri)) * msToS);
                fbR = dr.read ((0.4f + d * 5.0f * (0.5f + 0.5f * tri2)) * msToS);
                fbL = fastTanh (fbL); fbR = fastTanh (fbR);
                outL = 0.6f * inL + 0.6f * fbL;
                outR = 0.6f * inR + 0.6f * fbR;
                break;
            }
            case 2: // phaser: six first-order allpass stages
            case 5: // vibe: four staggered stages with a lopsided sweep
            {
                const int stagesN = type == 2 ? 6 : 4;
                auto run = [&] (float in, float* st, float phaseOff, float& fbState)
                {
                    float lv = lfo.sine (phaseOff);
                    if (type == 5) lv = lv > 0 ? std::sqrt (lv) : -lv * lv; // photocell lag shape
                    const float f = expMap (0.5f + 0.5f * lv * d, 180.0f, 3200.0f);
                    float s = in + fbState * (type == 2 ? m * 0.7f : 0.2f);
                    for (int k = 0; k < stagesN; ++k)
                    {
                        const float fk = type == 5 ? f * (0.6f + 0.35f * (float) k) : f;
                        const float t = (float) std::tan (kPi * std::min (fk, 0.45f * (float) sr) / sr);
                        const float a = (t - 1.0f) / (t + 1.0f);
                        const float y = a * s + st[k];
                        st[k] = undenorm (s - a * y);
                        s = y;
                    }
                    fbState = s;
                    return s;
                };
                const float pl = run (inL, apL, 0.0f, fbL);
                const float pr = run (inR, apR, type == 2 ? 0.2f : 0.0f, fbR);
                const float wet = type == 2 ? 0.5f : (0.35f + 0.3f * m);
                outL = inL * (1.0f - wet) + pl * wet;
                outR = inR * (1.0f - wet) + pr * wet;
                break;
            }
            case 3: // tremolo, mix knob morphs sine into square
            {
                auto shape = [m] (float s)
                {
                    const float k = 1.0f + m * 12.0f;
                    return fastTanh (s * k) / fastTanh (k);
                };
                const float gl = 1.0f - d * (0.5f + 0.5f * shape (lfo.sine (0.0f)));
                const float gr = 1.0f - d * (0.5f + 0.5f * shape (lfo.sine (m > 0.5f ? 0.5f : 0.0f)));
                outL = inL * gl * (1.0f + 0.5f * d);
                outR = inR * gr * (1.0f + 0.5f * d);
                break;
            }
            default: break;
        }
        lfo.advance();
        l[i] = outL;
        r[i] = outR;
    }
}

//==============================================================================
// Delay
void Delay::prepare (double sampleRate)
{
    sr = sampleRate;
    dl.prepare ((int) (2.2 * sr));
    dr.prepare ((int) (2.2 * sr));
    timeS.prepare (sr, 120.0f);
    timeS.reset ((float) (0.38 * sr));
    mixS.prepare (sr, 30.0f);
    wow.prepare (sr); wow.setRate (0.55f);
    flutter.prepare (sr); flutter.setRate (6.3f);
    hpL.setCutoff (sr, 120.0); hpR.setCutoff (sr, 120.0);
    reset();
}

void Delay::reset()
{
    dl.reset(); dr.reset();
    lpL.reset(); lpR.reset(); hpL.reset(); hpR.reset();
}

void Delay::set (int t, float timeMs, float fbKnob, float toneKnob, float mixKnob)
{
    type = t;
    timeS.setTarget ((float) (std::clamp (timeMs, 10.0f, 2000.0f) * 0.001 * sr));
    feedback = std::min (0.95f, (fbKnob / 10.0f) * 0.95f);
    mixS.setTarget (clamp01 (mixKnob / 10.0f));
    toneHz = expMap (toneKnob / 10.0f, 1200.0f, 16000.0f);
    if (type == 1) toneHz = std::min (toneHz, 3200.0f); // bucket-brigade bandwidth
    if (type == 2) toneHz = std::min (toneHz, 6500.0f);
    lpL.setCutoff (sr, toneHz);
    lpR.setCutoff (sr, toneHz);
}

void Delay::process (float* l, float* r, int n)
{
    for (int i = 0; i < n; ++i)
    {
        float t = timeS.next();
        if (type == 1) t *= 1.0f + 0.0015f * wow.sine();
        if (type == 2) t *= 1.0f + 0.0025f * wow.sine() + 0.0008f * flutter.sine();
        wow.advance(); flutter.advance();

        float wl = dl.read (t), wr = dr.read (t);
        // keep the repeats musical and bounded
        auto loop = [this] (float s, OnePole& lp, OnePole& hp)
        {
            s = lp.lowpass (s);
            if (type >= 1) s = hp.highpass (s);
            if (type == 1 || type == 2) s = fastTanh (s * 1.2f) / 1.2f;
            return s;
        };
        wl = loop (wl, lpL, hpL);
        wr = loop (wr, lpR, hpR);

        const float mono = 0.5f * (l[i] + r[i]);
        if (type == 3)
        {
            // ping-pong: input enters left, repeats cross over
            dl.push (undenorm (mono + wr * feedback));
            dr.push (undenorm (wl * feedback));
        }
        else
        {
            dl.push (undenorm (l[i] + wl * feedback));
            dr.push (undenorm (r[i] + wr * feedback));
        }
        const float m = mixS.next();
        l[i] += wl * m;
        r[i] += wr * m;
    }
}

//==============================================================================
// Reverb: 8-line feedback delay network with input diffusion.
void Reverb::prepare (double sampleRate)
{
    sr = sampleRate;
    pre.prepare ((int) (0.25 * sr));
    for (auto& d : diff) d.prepare ((int) (0.05 * sr));
    for (auto& l : lines) l.prepare ((int) (0.4 * sr));
    springLine.prepare ((int) (0.1 * sr));
    shimmerLine.prepare ((int) (0.2 * sr));
    mod.prepare (sr);
    mod.setRate (0.31f);
    mixS.prepare (sr, 30.0f);
    dirty = true;
    configure();
    reset();
}

void Reverb::reset()
{
    pre.reset();
    for (auto& d : diff) d.reset();
    for (auto& l : lines) l.reset();
    for (auto& d : damp) d.reset();
    for (auto& d : lowCut) d.reset();
    springLine.reset(); shimmerLine.reset();
    std::fill (std::begin (springAp), std::end (springAp), 0.0f);
    std::fill (std::begin (lastOut), std::end (lastOut), 0.0f);
    bandwidth.reset(); springLp.reset(); springHp.reset();
}

void Reverb::set (int t, float d, float p, float tn, float m)
{
    const float dd = clamp01 (d / 10.0f), tt = clamp01 (tn / 10.0f);
    if (t != type || dd != decay || p != preMs || tt != tone)
    {
        if (t != type) { type = t; reset(); }
        decay = dd; preMs = p; tone = tt;
        dirty = true;
        configure();
    }
    mixS.setTarget (clamp01 (m / 10.0f));
}

void Reverb::configure()
{
    if (! dirty) return;
    dirty = false;
    float rt60 = 1.0f;
    switch (type)
    {
        case 0: size = 0.32f; rt60 = lerp (0.25f, 1.6f, decay); diffusion = 0.6f; modDepth = 0.1f; inputLp = 9000; break;
        case 1: size = 1.0f;  rt60 = lerp (1.0f, 7.0f, decay);  diffusion = 0.7f; modDepth = 0.35f; inputLp = 8000; break;
        case 2: size = 0.55f; rt60 = lerp (0.6f, 5.0f, decay);  diffusion = 0.8f; modDepth = 0.15f; inputLp = 14000; break;
        case 3: size = 0.3f;  rt60 = lerp (0.8f, 3.5f, decay);  diffusion = 0.65f; modDepth = 0.2f; inputLp = 5000; break;
        case 4: size = 1.1f;  rt60 = lerp (2.0f, 10.0f, decay); diffusion = 0.75f; modDepth = 0.5f; inputLp = 9000; break;
        default: size = 1.6f; rt60 = lerp (4.0f, 25.0f, decay); diffusion = 0.8f; modDepth = 0.9f; inputLp = 7000; break;
    }
    static const float baseMs[N] = { 29.7f, 37.1f, 41.1f, 43.7f, 53.0f, 59.9f, 67.7f, 73.1f };
    static const float diffMs[4] = { 4.7f, 3.6f, 12.7f, 9.3f };
    for (int i = 0; i < N; ++i)
    {
        lineLen[i] = baseMs[i] * size * 0.001f * (float) sr;
        lineGain[i] = std::pow (10.0f, -3.0f * (lineLen[i] / (float) sr) / rt60);
        // high frequencies die faster: tone sets the damping cutoff
        damp[i].setCutoff (sr, expMap (tone, 1500.0f, type == 2 ? 16000.0f : 11000.0f));
        lowCut[i].setCutoff (sr, type == 3 ? 250.0 : 60.0);
    }
    for (int i = 0; i < 4; ++i)
        diffLen[i] = diffMs[i] * std::max (0.4f, size) * 0.001f * (float) sr;
    bandwidth.setCutoff (sr, inputLp);
    springLp.setCutoff (sr, 4200.0);
    springHp.setCutoff (sr, 300.0);
    shimFb = type == 4 ? lerp (0.25f, 0.45f, decay) : 0.0f;
}

void Reverb::process (float* l, float* r, int n)
{
    const float preSamples = std::max (1.0f, preMs * 0.001f * (float) sr);
    const float modAmt = modDepth * 0.0008f * (float) sr;
    const int shimWin = (int) (0.08 * sr);

    for (int i = 0; i < n; ++i)
    {
        float in = 0.5f * (l[i] + r[i]);
        pre.push (in);
        float s = bandwidth.lowpass (pre.read (preSamples));

        if (type == 3)
        {
            // spring tank: a dispersive allpass chain inside a short feedback loop
            // gives the classic "drip" and boing
            float sp = s + springLine.read (0.033f * (float) sr) * 0.55f;
            for (int k = 0; k < 12; ++k)
            {
                const float a = 0.62f;
                const float y = -a * sp + springAp[k];
                springAp[k] = undenorm (sp + a * y);
                sp = y;
            }
            sp = springHp.highpass (springLp.lowpass (sp));
            springLine.push (undenorm (sp));
            s = s * 0.3f + sp;
        }

        if (type == 4)
        {
            // shimmer: an octave-up copy of the tail is fed back into the input
            float tail = 0.0f;
            for (int k = 0; k < N; ++k) tail += lastOut[k];
            shimmerLine.push (tail * 0.25f);
            shimPhase += (1.0f - 2.0f) / (float) shimWin;
            shimPhase -= std::floor (shimPhase);
            const float p2 = shimPhase + 0.5f - std::floor (shimPhase + 0.5f);
            const float w1 = std::sin (shimPhase * (float) kPi), w2 = std::sin (p2 * (float) kPi);
            const float shifted = shimmerLine.read (1.0f + shimPhase * shimWin) * w1 * w1
                                  + shimmerLine.read (1.0f + p2 * shimWin) * w2 * w2;
            s += shifted * shimFb;
        }

        // input diffusion
        for (int k = 0; k < 4; ++k)
        {
            const float g = diffusion;
            const float delayed = diff[k].read (diffLen[k]);
            const float v = s + g * delayed;
            diff[k].push (undenorm (v));
            s = delayed - g * v;
        }

        // read the delay lines (two of them gently modulated)
        float y[N];
        for (int k = 0; k < N; ++k)
        {
            float len = lineLen[k];
            if (k < 4) len += modAmt * mod.sine (0.25f * (float) k) + modAmt;
            y[k] = lines[k].read (std::max (1.0f, len));
        }
        mod.advance();

        // Householder feedback matrix: y - 2/N * sum(y)
        float sum = 0.0f;
        for (int k = 0; k < N; ++k) sum += y[k];
        sum *= 2.0f / (float) N;
        for (int k = 0; k < N; ++k)
        {
            float fb = (y[k] - sum) * lineGain[k];
            fb = damp[k].lowpass (fb);
            fb = lowCut[k].highpass (fb);
            const float sign = (k & 1) ? -1.0f : 1.0f;
            lines[k].push (undenorm (fb + s * sign * 0.35f));
            lastOut[k] = y[k];
        }

        const float outL = (y[0] - y[2] + y[4] - y[6] + 0.5f * (y[1] + y[7])) * 0.55f;
        const float outR = (y[1] - y[3] + y[5] - y[7] + 0.5f * (y[2] + y[6])) * 0.55f;
        const float m = mixS.next();
        l[i] += outL * m;
        r[i] += outR * m;
    }
}

//==============================================================================
// LoFi
void LoFi::prepare (double sampleRate)
{
    sr = sampleRate;
    dl.prepare ((int) (0.05 * sr));
    dr.prepare ((int) (0.05 * sr));
    wow.prepare (sr); wow.setRate (0.7f);
    flutter.prepare (sr); flutter.setRate (5.5f);
    hissLp.setCutoff (sr, 6000.0);
    crackleLp.setCutoff (sr, 3000.0);
    reset();
}

void LoFi::reset()
{
    dl.reset(); dr.reset();
    radioHpL.reset(); radioHpR.reset(); radioLpL.reset(); radioLpR.reset(); preLpL.reset(); preLpR.reset();
    holdCounter = 0; heldL = heldR = 0;
}

void LoFi::set (float crush, float downsample, float wob, float nz, float radio, float m)
{
    bits = lerp (16.0f, 4.0f, clamp01 (crush / 10.0f));
    const float targetRate = expMap (1.0f - clamp01 (downsample / 10.0f), 2000.0f, (float) sr);
    holdRatio = std::max (1.0f, (float) sr / targetRate);
    preLpL.set (Biquad::Type::lowpass, sr, std::min (0.45 * targetRate, 0.45 * sr), 0.7);
    preLpR.set (Biquad::Type::lowpass, sr, std::min (0.45 * targetRate, 0.45 * sr), 0.7);
    wobble = clamp01 (wob / 10.0f);
    noiseAmt = clamp01 (nz / 10.0f);
    const float rd = clamp01 (radio / 10.0f);
    const float hp = expMap (rd, 20.0f, 700.0f), lp = expMap (1.0f - rd, 2500.0f, 20000.0f);
    radioHpL.set (Biquad::Type::highpass, sr, hp, 0.8); radioHpR.set (Biquad::Type::highpass, sr, hp, 0.8);
    radioLpL.set (Biquad::Type::lowpass, sr, lp, 0.8);  radioLpR.set (Biquad::Type::lowpass, sr, lp, 0.8);
    mix = clamp01 (m / 10.0f);
}

void LoFi::process (float* l, float* r, int n)
{
    const float q = std::pow (2.0f, bits - 1.0f);
    const float msToS = (float) (0.001 * sr);
    for (int i = 0; i < n; ++i)
    {
        const float inL = l[i], inR = r[i];
        float sl = inL, sr2 = inR;

        // tape wow and flutter with a slow random drift
        drift = 0.9995f * drift + 0.0005f * noise.next();
        const float wob = wobble * (2.5f * wow.sine() + 0.35f * flutter.sine() + 30.0f * drift);
        wow.advance(); flutter.advance();
        dl.push (sl); dr.push (sr2);
        if (wobble > 0.0f)
        {
            sl = dl.read ((6.0f + wob) * msToS);
            sr2 = dr.read ((6.0f + wob * 0.9f) * msToS);
        }

        // sample-rate reduction (sample and hold) and bit crushing
        if (holdRatio > 1.0f)
        {
            sl = preLpL.process (sl); sr2 = preLpR.process (sr2);
            holdCounter += 1.0f;
            if (holdCounter >= holdRatio) { holdCounter -= holdRatio; heldL = sl; heldR = sr2; }
            sl = heldL; sr2 = heldR;
        }
        if (bits < 15.9f)
        {
            sl = std::round (sl * q) / q;
            sr2 = std::round (sr2 * q) / q;
        }

        sl = radioLpL.process (radioHpL.process (sl));
        sr2 = radioLpR.process (radioHpR.process (sr2));

        if (noiseAmt > 0.0f)
        {
            const float hiss = hissLp.lowpass (noise.next()) * 0.02f * noiseAmt;
            float crackle = 0.0f;
            if (std::abs (noise.next()) > 1.0f - 0.0004f * noiseAmt) crackle = noise.next() * 0.3f * noiseAmt;
            crackle = crackleLp.lowpass (crackle);
            sl += hiss + crackle;
            sr2 += hiss * 0.9f + crackle;
        }

        l[i] = lerp (inL, sl, mix);
        r[i] = lerp (inR, sr2, mix);
    }
}

//==============================================================================
// Doubler: a second, slightly late and drifting "performance" on the right.
void Doubler::prepare (double sampleRate)
{
    sr = sampleRate;
    line.prepare ((int) (0.06 * sr));
    lfo1.prepare (sr); lfo1.setRate (0.13f);
    lfo2.prepare (sr); lfo2.setRate (0.71f);
    tone.setCutoff (sr, 7000.0);
    amountS.prepare (sr, 50.0f);
    reset();
}

void Doubler::reset() { line.reset(); tone.reset(); }

void Doubler::set (float a) { amountS.setTarget (clamp01 (a / 10.0f)); }

void Doubler::process (float* l, float* r, int n)
{
    const float msToS = (float) (0.001 * sr);
    for (int i = 0; i < n; ++i)
    {
        const float a = amountS.next();
        const float mono = 0.5f * (l[i] + r[i]);
        line.push (mono);
        if (a <= 0.0001f) { lfo1.advance(); lfo2.advance(); continue; }
        const float d = (16.0f + 4.0f * lfo1.sine() + 0.6f * lfo2.sine()) * msToS;
        const float dbl = tone.lowpass (line.read (d));
        // equal-power spread: original leans left, double leans right
        l[i] = l[i] * (1.0f - 0.25f * a) + dbl * 0.15f * a;
        r[i] = r[i] * (1.0f - 0.85f * a) + dbl * 0.95f * a;
    }
}

//==============================================================================
// SafetyLimiter
void SafetyLimiter::prepare (double sampleRate)
{
    rel = (float) std::exp (-1.0 / (0.15 * sampleRate));
    reset();
}

void SafetyLimiter::reset() { env = 0.0f; }

void SafetyLimiter::process (float* l, float* r, int n)
{
    const float threshold = 0.5f; // -6 dBFS
    for (int i = 0; i < n; ++i)
    {
        const float peak = std::max (std::abs (l[i]), std::abs (r[i]));
        env = peak > env ? peak : peak + (env - peak) * rel;
        const float g = env > threshold ? threshold / env : 1.0f;
        // gain riding catches sustained level, the soft clip catches the attack
        auto clip = [] (float x)
        {
            const float knee = 0.5f, ceiling = 0.966f;
            const float a = std::abs (x);
            if (a <= knee) return x;
            const float over = (a - knee) / (ceiling - knee);
            return std::copysign (knee + (ceiling - knee) * fastTanh (over), x);
        };
        const float gg = std::max (g, 0.25f);
        l[i] = clip (l[i] * (0.5f + 0.5f * gg));
        r[i] = clip (r[i] * (0.5f + 0.5f * gg));
    }
}
} // namespace rh
