// Riffhouse - free, open-source guitar rig
// Copyright (C) 2026 Riffhouse contributors. Licensed under AGPL-3.0-or-later.
#include "Amp.h"

namespace rh
{
namespace
{
    constexpr double kPlateUnit = 80.0; // volts of plate swing that make one unit after the preamp

    using namespace tonestacks;

    // name, inHP, bright, gainMin, gainMax, stages, stageGain[5], stageBias[5], stageHp[5], stageLp[5],
    // baxandall, stack, midHz, powerDrive, powerBias, sag, presenceHz, depthHz, outHp, outLp, trim
    const AmpVoicing voicings[] = {
        { "Glass Clean", 20.0f, 5.0f, -14.0f, 14.0f, 2,
          { 1, 6, 1, 1, 1 }, { 0.10f, 0.08f, 0, 0, 0 }, { 20, 30, 0, 0, 0 }, { 14000, 13750, 0, 0, 0 },
          false, american, 600.0f, 0.9f, 0.00f, 0.10f, 4500.0f, 95.0f, 45.0f, 11000.0f, 3.3f },
        { "Tweed Edge", 20.0f, 2.0f, -12.0f, 20.0f, 2,
          { 1, 14, 1, 1, 1 }, { 0.20f, 0.15f, 0, 0, 0 }, { 20, 40, 0, 0, 0 }, { 11250, 8750, 0, 0, 0 },
          false, tweed, 800.0f, 2.2f, 0.05f, 0.45f, 3000.0f, 85.0f, 50.0f, 8000.0f, -5.5f },
        { "Chime Top", 25.0f, 3.0f, -14.0f, 20.0f, 3,
          { 1, 10, 1.5f, 1, 1 }, { 0.15f, 0.10f, 0.05f, 0, 0 }, { 25, 60, 30, 0, 0 }, { 14000, 12000, 15000, 0, 0 },
          false, chime, 900.0f, 1.8f, 0.18f, 0.30f, 3500.0f, 110.0f, 70.0f, 10000.0f, -4.3f },
        { "Brit 68", 30.0f, 6.0f, -14.0f, 22.0f, 3,
          { 1, 16, 1.2f, 1, 1 }, { 0.20f, 0.18f, 0.05f, 0, 0 }, { 30, 90, 40, 0, 0 }, { 12000, 11250, 14000, 0, 0 },
          false, british, 750.0f, 1.6f, 0.02f, 0.25f, 3500.0f, 100.0f, 60.0f, 10500.0f, -3.2f },
        { "Brit 81", 40.0f, 4.0f, -40.0f, 20.0f, 4,
          { 1, 22, 12, 1.2f, 1 }, { 0.25f, 0.35f, 0.15f, 0.05f, 0 }, { 40, 120, 90, 40, 0 }, { 16000, 14000, 12500, 16000, 0 },
          false, british, 700.0f, 1.3f, 0.02f, 0.20f, 3200.0f, 100.0f, 70.0f, 12000.0f, -2.9f, 210.0f, -7.5f },
        { "Redline Lead", 40.0f, 2.0f, -46.0f, 16.0f, 5,
          { 1, 20, 14, 8, 1.2f }, { 0.25f, 0.30f, 0.20f, 0.10f, 0.05f }, { 40, 150, 120, 80, 40 }, { 17000, 15000, 14000, 14000, 17000 },
          false, britishHot, 650.0f, 1.2f, 0.02f, 0.20f, 3000.0f, 95.0f, 70.0f, 13000.0f, -3.4f, 300.0f, -9.0f },
        { "Modern High Gain", 30.0f, 1.0f, -48.0f, 16.0f, 5,
          { 1, 22, 16, 10, 1.2f }, { 0.20f, 0.30f, 0.25f, 0.10f, 0.05f }, { 30, 70, 60, 50, 30 }, { 18000, 17000, 16000, 16000, 18000 },
          false, modern, 550.0f, 1.6f, 0.0f, 0.40f, 3500.0f, 80.0f, 45.0f, 14000.0f, -7.6f, 230.0f, -5.0f, 5000.0f, 7.0f, 110.0f, 4.0f },
        { "Tight Metal", 60.0f, 2.0f, -46.0f, 18.0f, 5,
          { 1, 25, 18, 10, 1.2f }, { 0.20f, 0.30f, 0.20f, 0.10f, 0.05f }, { 60, 130, 100, 70, 40 }, { 18000, 17000, 16000, 16000, 18000 },
          false, modern, 650.0f, 1.3f, 0.0f, 0.15f, 3800.0f, 90.0f, 60.0f, 14000.0f, -6.1f, 240.0f, -4.0f, 4500.0f, 6.0f },
        { "Bass Station", 15.0f, 0.0f, -10.0f, 18.0f, 2,
          { 1, 8, 1, 1, 1 }, { 0.15f, 0.10f, 0, 0, 0 }, { 15, 20, 0, 0, 0 }, { 12000, 11250, 0, 0, 0 },
          true, american, 500.0f, 1.2f, 0.0f, 0.30f, 5000.0f, 60.0f, 28.0f, 9000.0f, 1.8f },
        { "Studio Direct", 15.0f, 0.0f, -12.0f, 12.0f, 1,
          { 1, 1, 1, 1, 1 }, { 0.05f, 0, 0, 0, 0 }, { 15, 0, 0, 0, 0 }, { 18000, 0, 0, 0, 0 },
          true, american, 800.0f, 0.5f, 0.0f, 0.0f, 6000.0f, 90.0f, 25.0f, 16000.0f, 27.1f },
    };
} // namespace

const AmpVoicing& ampVoicing (int model)
{
    model = std::clamp (model, 0, numAmpVoicings() - 1);
    return voicings[model];
}

int numAmpVoicings() { return (int) (sizeof (voicings) / sizeof (voicings[0])); }

void Amp::prepare (double sampleRate, int maxBlock)
{
    sr = sampleRate;
    // 4x oversampling below 64 kHz, 2x above, with every curve antialiased (below). Measured at
    // full gain on the high-gain models: less fold-back than the plain curves at 8x (0.1.4) for
    // the same CPU and one sample less delay. Plain 8x with the antialiasing would take 20 dB
    // more off but doubles the amp's CPU, too much for a modest laptop.
    const int stagesOs = sampleRate < 64000.0 ? 2 : 1;
    oversampler = std::make_unique<juce::dsp::Oversampling<float>> (1, (size_t) stagesOs,
                                                                    juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR,
                                                                    true, false);
    oversampler->initProcessing ((size_t) maxBlock);
    osr = sampleRate * (double) (1 << stagesOs);

    for (auto& s : stages)
        s.blocking.prepare (osr, 2.0f, 60.0f);
    tube::standardLine(); // built here, never on the audio thread
    supply.prepare (osr, 4.0f, 180.0f);
    stack.prepare (osr);
    dcPre.prepare (osr);
    dcPost.prepare (osr);
    preGain.prepare (osr, 30.0f);
    masterGain.prepare (osr, 30.0f);
    outGain.prepare (sr, 30.0f);
    toneGlide = (float) std::exp (-32.0 / (0.020 * osr));
    toneSnap = true;
    dirty = true;
    updateFilters();
    preGain.reset (preGain.getTarget());
    reset();
}

void Amp::reset()
{
    // a fresh start: the knobs are where they are, nothing to glide from
    toneSnap = true;
    std::copy (std::begin (toneGoal), std::end (toneGoal), std::begin (toneNow));
    updateTone();
    if (oversampler) oversampler->reset();
    inputHp.reset();
    bright.reset(); midBoost.reset(); lowMid.reset(); top.reset(); low.reset(); presence.reset(); depth.reset(); outHp.reset(); outLp.reset(); antiFizz.reset();
    baxLow.reset(); baxMid.reset(); baxHigh.reset();
    stack.reset();
    for (auto& s : stages) { s.hp.reset(); s.lp.reset(); s.blocking.reset(); s.last = {}; }
    powerLast = {};
    for (auto& t : tubes) t.reset();
    supply.reset();
    dcPre.reset(); dcPost.reset();
}

float Amp::latencySamples() const { return oversampler ? oversampler->getLatencyInSamples() : 0.0f; }

void Amp::setSettings (const AmpSettings& s)
{
    if (s.model != settings.model || s.gain != settings.gain || s.bass != settings.bass || s.mid != settings.mid
        || s.treble != settings.treble || s.presence != settings.presence || s.depth != settings.depth
        || s.master != settings.master || s.levelDb != settings.levelDb || v == nullptr)
    {
        const bool modelChanged = (v == nullptr) || s.model != settings.model;
        settings = s;
        dirty = true;
        updateFilters();
        if (modelChanged)
        {
            reset();
            preGain.reset (preGain.getTarget());
        }
    }
}

void Amp::updateFilters()
{
    if (! dirty) return;
    dirty = false;
    v = &ampVoicing (settings.model);

    const float g01 = clamp01 (settings.gain / 10.0f);
    preGain.setTarget (dbToGain (lerp (v->gainMinDb, v->gainMaxDb, g01)));
    masterGain.setTarget (dbToGain (lerp (-18.0f, 6.0f, clamp01 (settings.master / 10.0f))));
    outGain.setTarget (dbToGain (v->trimDb + settings.levelDb));

    inputHp.setCutoff (osr, v->inputHpHz);

    for (int i = 0; i < 5; ++i)
    {
        auto& st = stages[i];
        st.gain = v->stageGain[i];
        st.bias = v->stageBias[i];
        st.hp.setCutoff (osr, std::max (5.0f, v->stageHpHz[i]));
        st.lp.setCutoff (osr, v->stageLpHz[i] > 0 ? v->stageLpHz[i] : 20000.0f);

        // The same stage as a real triode. The coupling capacitor into a 1 MOhm grid leak sets
        // how tight it is; the grid stopper against the tube's input capacitance sets where the
        // top rolls off; the cathode sets the character: fully bypassed (warm, round), a small
        // bypass cap (bright, a little harder) or unbypassed and biased cold (the hard, uneven
        // clipping of high-gain preamps).
        tube::StageParts parts;
        parts.cCoupling = 1.0 / (kTwoPi * 1.0e6 * std::max (5.0f, v->stageHpHz[i]));
        const double cin = 1.6e-12 + 51.0 * 1.7e-12;
        const double lpHz = v->stageLpHz[i] > 0 ? v->stageLpHz[i] : 20000.0f;
        parts.rStopper = std::clamp (1.0 / (kTwoPi * lpHz * cin), 10000.0, 1.0e6);
        if (v->stageBias[i] < 0.18f) { parts.rCathode = 1500.0; parts.cCathode = 22e-6; }
        else if (v->stageBias[i] < 0.28f) { parts.rCathode = 2700.0; parts.cCathode = 0.68e-6; }
        else { parts.rCathode = 10000.0; parts.cCathode = 0.0; }
        auto& t = tubes[i];
        t.prepare (osr, parts);
        // the drive between stages, matched to the curve model so the presets keep their gain:
        // one unit there clipped where about 1.5 V at a grid does, and came out as about 80 V of
        // plate (a 12AX7 stage's swing); the same small-signal gain, stage for stage
        const double g = std::max (1.0, t.gain());
        into[i] = i == 0 ? 2.0 * kPlateUnit / g : v->stageGain[i] / g;
    }

    if (! v->baxandall)
    {
        // Make up the passive stack's insertion loss, measured with the knobs at noon
        // so turning knobs still changes the level the way a real amp does.
        ToneStack probe;
        probe.prepare (osr);
        probe.set (v->stack, 0.5, 0.5, 0.5);
        double sum = 0.0;
        const double freqs[] = { 150.0, 400.0, 1000.0, 2500.0 };
        for (double f : freqs) sum += probe.magnitude (f);
        stackMakeup = (float) (4.0 / sum);
    }
    else
        stackMakeup = 1.0f;

    toneGoal[kBass] = settings.bass / 10.0f;
    toneGoal[kMid] = settings.mid / 10.0f;
    toneGoal[kTreble] = settings.treble / 10.0f;
    toneGoal[kPresence] = settings.presence / 10.0f;
    toneGoal[kDepth] = settings.depth / 10.0f;
    toneGoal[kGain] = g01;
    if (toneSnap) std::copy (std::begin (toneGoal), std::end (toneGoal), std::begin (toneNow));
    updateTone();
    outHp.set (Biquad::Type::highpass, osr, v->outputHpHz, 0.6);
    outLp.set (Biquad::Type::lowpass, osr, v->outputLpHz, 0.6);
    antiFizz.set (Biquad::Type::lowpass, osr, std::min (16000.0, 0.45 * sr), 0.5);
    lowMid.set (Biquad::Type::peak, osr, v->lowMidHz, 0.8, v->lowMidDb);
    top.set (Biquad::Type::highShelf, osr, v->topHz, 0.7, v->topDb);
    low.set (Biquad::Type::peak, osr, v->lowHz, 0.9, v->lowDb);
}

void Amp::updateTone()
{
    if (v == nullptr) return;
    const float b = toneNow[kBass], m = toneNow[kMid], t = toneNow[kTreble];
    // The bright cap matters most at low gain settings, as on a real amp.
    bright.set (Biquad::Type::highShelf, osr, 1800.0, 0.6, v->brightDb * (1.0f - toneNow[kGain]));
    if (v->baxandall)
    {
        baxLow.set (Biquad::Type::lowShelf, osr, 110.0, 0.7, (b - 0.5f) * 24.0f);
        baxMid.set (Biquad::Type::peak, osr, v->midBoostHz, 0.8, (m - 0.5f) * 20.0f);
        baxHigh.set (Biquad::Type::highShelf, osr, 2800.0, 0.7, (t - 0.5f) * 24.0f);
    }
    else
    {
        stack.set (v->stack, t, m, b);
        midBoost.set (Biquad::Type::peak, osr, v->midBoostHz, 0.7, (m - 0.5f) * 8.0f);
    }
    presence.set (Biquad::Type::highShelf, osr, v->presenceHz, 0.6, (toneNow[kPresence] - 0.5f) * 12.0f);
    depth.set (Biquad::Type::peak, osr, v->depthHz, 0.8, (toneNow[kDepth] - 0.5f) * 12.0f);
}

// Asymmetric triode-style transfer curve: the positive side (grid conduction)
// clips earlier and harder than the negative side (cutoff), which gives even
// harmonics. The stage inverts like a common-cathode stage.
inline float Amp::tube (float x, float bias) noexcept
{
    auto f = [] (float u) { return u >= 0.0f ? fastTanh (u) : fastTanh (0.6f * u) * (1.0f / 0.6f); };
    return -(f (x + bias) - f (bias));
}

// The same curve, antialiased (first-order antiderivative): the average of the curve over the
// path the signal took since the last sample, rather than its value at one point. Where a
// stage bends hard, that takes most of the fold-back (the digital fizz on high notes) away.
namespace
{
    // log cosh u = |u| + log (1 + e^-2|u|) - log 2, kept as its straight part and its small
    // bend, so the difference of two nearby values stays exact in single precision
    inline float bend (float a) noexcept { return std::log1p (std::exp (-2.0f * a)) - 0.693147181f; }
    inline float tubeCurve (float u) noexcept { return u >= 0.0f ? fastTanh (u) : fastTanh (0.6f * u) * (1.0f / 0.6f); }
    constexpr float kAaStep = 1.0e-3f; // smaller steps than this are near enough straight
}

// Both take the curve's input (bias included) as the path: a bias that moves with the grid
// blocking or the supply sag is part of where the signal went. The bend at the last sample is
// kept, so each sample costs one exp and one log, not two of each.
float Amp::tubeAA (float x, float bias, AaState& last) noexcept
{
    const float u = x + bias, u0 = last.fresh ? u : last.u;
    const float bendU = u >= 0.0f ? bend (u) : bend (-0.6f * u) * (1.0f / 0.36f);
    const float d = u - u0;
    float y;
    if (std::abs (d) > kAaStep)
    {
        const bool same = (u >= 0.0f) == (u0 >= 0.0f);
        auto straight = [] (float v) { return v >= 0.0f ? v : -v * (1.0f / 0.6f); };
        const float lin = same ? (u >= 0.0f ? d : -d * (1.0f / 0.6f)) : straight (u) - straight (u0);
        y = (lin + (bendU - last.bend)) / d;
    }
    else
        y = tubeCurve (0.5f * (u + u0));
    last = { u, bendU, false };
    return -(y - tubeCurve (bias));
}

float Amp::powerAA (float x, float bias, float head, AaState& last) noexcept
{
    // head * tanh ((x + bias) / head), averaged the same way (in the curve's own units)
    const float u = (x + bias) / head, u0 = last.fresh ? u : last.u;
    const float bendU = bend (std::abs (u));
    const float d = u - u0;
    float y;
    if (std::abs (d) * head > kAaStep)
    {
        const float lin = (u >= 0.0f) == (u0 >= 0.0f) ? (u >= 0.0f ? d : -d) : std::abs (u) - std::abs (u0);
        y = head * (lin + (bendU - last.bend)) / d;
    }
    else
        y = head * fastTanh (0.5f * (u + u0));
    last = { u, bendU, false };
    return y - head * fastTanh (bias / head);
}

void Amp::process (float* data, int n)
{
    if (v == nullptr || oversampler == nullptr) return;

    float* chans[] = { data };
    juce::dsp::AudioBlock<float> block (chans, 1, (size_t) n);
    auto os = oversampler->processSamplesUp (block);
    float* x = os.getChannelPointer (0);
    const int on = (int) os.getNumSamples();

    const int ns = v->numStages;
    const bool bax = v->baxandall;
    const float sag = v->sag;
    const float pBias = v->powerBias;

    toneSnap = false;
    for (int i = 0; i < on; ++i)
    {
        if ((i & 31) == 0)
        {
            bool moving = false;
            for (int k = 0; k < kTone; ++k)
                if (std::abs (toneNow[k] - toneGoal[k]) > 1e-4f)
                {
                    toneNow[k] = toneGoal[k] + (toneNow[k] - toneGoal[k]) * toneGlide;
                    moving = true;
                }
                else toneNow[k] = toneGoal[k];
            if (moving) updateTone();
        }
        float s = x[i];
        s = inputHp.highpass (s);
        s = bright.process (s);

        // Stage 1 has a fixed gain; the gain knob sits after it (as the volume
        // pot does on most amps). Single-stage voicings take the gain first.
        if (ns == 1)
            s *= preGain.next();

        if (tubeStages)
        {
            double vt = s;
            for (int k = 0; k < ns; ++k)
            {
                if (k == 1) vt *= preGain.next();
                vt = tubes[k].process (vt * into[k]);
            }
            s = (float) (vt * (1.0 / kPlateUnit));
        }
        else
        for (int k = 0; k < ns; ++k)
        {
            auto& st = stages[k];
            if (k == 1)
                s *= preGain.next() * st.gain;
            else if (k > 1)
                s *= st.gain;
            else
                s *= 2.0f;

            // grid blocking: big positive swings shift the bias, which gives the
            // squashy, dynamic feel when you dig in. Kept gentle: more than this and a
            // high-gain amp turns darker and quieter as you dig in, which real ones never do
            // (measured on captures, 13 Nov)
            const float env = st.blocking.process (std::max (0.0f, s));
            s = tubeAA (s, st.bias - 0.15f * std::min (env, 2.0f), st.last);
            s = st.hp.highpass (s);
            s = st.lp.lowpass (s);
        }

        s = dcPre.process (s);

        if (bax)
            s = baxHigh.process (baxMid.process (baxLow.process (s)));
        else
            s = midBoost.process (stack.process (s) * stackMakeup);

        // power amp: presence and depth act through the negative feedback loop
        s = depth.process (presence.process (s));
        s *= 0.35f * v->powerDrive * masterGain.next();
        const float env = supply.process (s);
        const float head = 1.0f / (1.0f + sag * std::min (env, 4.0f));
        s = powerAA (s, pBias, head, powerLast);
        s = dcPost.process (s);
        // the voicing against real amps sits after the power amp, so its feel under the
        // fingers (how it compresses and sags) stays as it was
        if (v->lowMidDb != 0.0f) s = lowMid.process (s);
        if (v->topDb != 0.0f) s = top.process (s);
        if (v->lowDb != 0.0f) s = low.process (s);
        s = outLp.process (outHp.process (s));
        x[i] = antiFizz.process (s);
    }

    oversampler->processSamplesDown (block);

    for (int i = 0; i < n; ++i)
        data[i] *= outGain.next();
}
} // namespace rh
