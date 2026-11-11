// Riffhouse - free, open-source guitar rig
// Copyright (C) 2026 Riffhouse contributors. Licensed under AGPL-3.0-or-later.
#include "Rig.h"

namespace rh
{
namespace
{
    template <typename Fn>
    void runFaded (Fade& f, float* x, int n, std::vector<float>& scratch, Fn&& fn)
    {
        if (f.idle()) return;
        if (f.full()) { fn (x, n); return; }
        std::copy (x, x + n, scratch.begin());
        fn (x, n);
        for (int i = 0; i < n; ++i)
        {
            const float g = f.next();
            x[i] = scratch[(size_t) i] + (x[i] - scratch[(size_t) i]) * g;
        }
    }

    template <typename Fn>
    void runFadedStereo (Fade& f, float* l, float* r, int n, std::vector<float>& sl, std::vector<float>& sr, Fn&& fn)
    {
        if (f.idle()) return;
        if (f.full()) { fn (l, r, n); return; }
        std::copy (l, l + n, sl.begin());
        std::copy (r, r + n, sr.begin());
        fn (l, r, n);
        for (int i = 0; i < n; ++i)
        {
            const float g = f.next();
            l[i] = sl[(size_t) i] + (l[i] - sl[(size_t) i]) * g;
            r[i] = sr[(size_t) i] + (r[i] - sr[(size_t) i]) * g;
        }
    }

    bool on (const ParamValues& p, P id) { return p[(size_t) id] >= 0.5f; }
    int choice (const ParamValues& p, P id) { return (int) std::lround (p[(size_t) id]); }
} // namespace

Rig::Rig() { p = defaultParamValues(); }

void Rig::prepare (double sampleRate, int block)
{
    sr = sampleRate;
    maxBlock = std::max (block, 16);
    input.prepare (sr);
    probe.prepare (sr);
    gate.prepare (sr);
    comp.prepare (sr);
    wah.prepare (sr);
    pitch.prepare (sr);
    drive.prepare (sr, maxBlock);
    amp.prepare (sr, maxBlock);
    nam.prepare (sr, maxBlock);
    eq.prepare (sr);
    mod.prepare (sr);
    delay.prepare (sr);
    reverb.prepare (sr);
    lofi.prepare (sr);
    doubler.prepare (sr);
    limiter.prepare (sr);
    for (Fade* f : { &fGate, &fComp, &fWah, &fPitch, &fDrive, &fAmp, &fCab, &fEq, &fMod, &fLofi })
        f->prepare (sr);
    inGain.prepare (sr, 20.0f);
    outGain.prepare (sr, 20.0f);
    dry.assign ((size_t) maxBlock, 0.0f);
    dryR.assign ((size_t) maxBlock, 0.0f);
    tailL.assign ((size_t) maxBlock, 0.0f);
    tailR.assign ((size_t) maxBlock, 0.0f);
    firstParams = true;
    setParams (p);
    cab.prepare (sr, maxBlock);
    setParams (p);
}

void Rig::reset()
{
    input.reset(); gate.reset(); comp.reset(); wah.reset(); pitch.reset(); drive.reset(); amp.reset(); cab.reset();
    eq.reset(); mod.reset(); delay.reset(); reverb.reset(); lofi.reset(); doubler.reset(); limiter.reset();
}

int Rig::latencySamples() const
{
    return (int) std::lround (amp.latencySamples()) + nam.latencySamples();
}

float Rig::delayTimeMs() const
{
    static const float beats[] = { 0.0f, 2.0f, 1.5f, 1.0f, 0.75f, 0.5f, 1.0f / 3.0f, 0.25f };
    const int s = choice (p, P::dlySync);
    if (s <= 0 || s >= (int) (sizeof (beats) / sizeof (beats[0])))
        return p[(size_t) P::dlyTime];
    return (float) std::min (2000.0, 60000.0 / tempo * beats[s]);
}

void Rig::setParams (const ParamValues& v)
{
    const ParamValues old = p;
    p = v;
    auto changed = [&] (P id) { return firstParams || old[(size_t) id] != p[(size_t) id]; };
    auto get = [&] (P id) { return p[(size_t) id]; };

    inGain.setTarget (dbToGain (get (P::inGain)));
    outGain.setTarget (dbToGain (get (P::outLevel)));
    if (firstParams) { inGain.reset (dbToGain (get (P::inGain))); outGain.reset (dbToGain (get (P::outLevel))); }

    input.set (choice (p, P::humFilter), on (p, P::inputFix), get (P::inputFixAmt));
    if (changed (P::gateThresh) || changed (P::gateRelease)) gate.set (get (P::gateThresh), get (P::gateRelease));
    comp.set (get (P::compSustain), get (P::compAttack), get (P::compLevel), get (P::compBlend));
    wah.set (choice (p, P::wahMode), get (P::wahPos), get (P::wahSens), get (P::wahQ));
    pitch.set (choice (p, P::pitchMode), get (P::pitchSemis), get (P::pitchMix));
    drive.set (choice (p, P::drvType), get (P::drvGain), get (P::drvTone), get (P::drvLevel));

    AmpSettings as;
    as.model = std::min (choice (p, P::ampModel), numAmpVoicings() - 1);
    as.gain = get (P::ampGain); as.bass = get (P::ampBass); as.mid = get (P::ampMid); as.treble = get (P::ampTreble);
    as.presence = get (P::ampPresence); as.depth = get (P::ampDepth); as.master = get (P::ampMaster); as.levelDb = get (P::ampLevel);
    amp.setSettings (as);

    namBass.set (Biquad::Type::lowShelf, sr, 120.0, 0.7, (get (P::ampBass) - 5.0f) * 2.0f);
    namMid.set (Biquad::Type::peak, sr, 700.0, 0.8, (get (P::ampMid) - 5.0f) * 1.6f);
    namTreble.set (Biquad::Type::highShelf, sr, 2800.0, 0.7, (get (P::ampTreble) - 5.0f) * 2.0f);

    cab.set (choice (p, P::cabModel), get (P::cabMic) / 10.0f, get (P::cabDistance) / 10.0f, get (P::cabLowCut), get (P::cabHighCut));

    const float eqGains[7] = { get (P::eq100), get (P::eq200), get (P::eq400), get (P::eq800), get (P::eq1k6), get (P::eq3k2), get (P::eq6k4) };
    eq.set (eqGains, get (P::eqLevel));
    mod.set (choice (p, P::modType), get (P::modRate), get (P::modDepth), get (P::modMix));

    const float space = get (P::space);
    delay.set (choice (p, P::dlyType), delayTimeMs(), get (P::dlyFeedback), get (P::dlyTone), std::min (10.0f, get (P::dlyMix) * space));
    reverb.set (choice (p, P::revType), get (P::revDecay), get (P::revPreDelay), get (P::revTone), std::min (10.0f, get (P::revMix) * space));
    lofi.set (get (P::lofiBits), get (P::lofiRate), get (P::lofiWobble), get (P::lofiNoise), get (P::lofiFilter), get (P::lofiMix));
    doubler.set (get (P::doubler));

    auto setFade = [&] (Fade& f, P id, auto&& onStart)
    {
        const bool want = on (p, id);
        if (firstParams) { f.snap (want); return; }
        if (want && f.idle()) onStart();
        f.setOn (want);
    };
    setFade (fGate, P::gateOn, [&] { gate.reset(); });
    setFade (fComp, P::compOn, [&] { comp.reset(); });
    setFade (fWah, P::wahOn, [&] { wah.reset(); });
    setFade (fPitch, P::pitchOn, [&] { pitch.reset(); });
    setFade (fDrive, P::drvOn, [&] { drive.reset(); });
    setFade (fAmp, P::ampOn, [&] { amp.reset(); });
    setFade (fCab, P::cabOn, [&] {});
    setFade (fEq, P::eqOn, [&] { eq.reset(); });
    setFade (fMod, P::modOn, [&] { mod.reset(); });
    setFade (fLofi, P::lofiOn, [&] { lofi.reset(); });

    // delay and reverb keep ringing after they are switched off
    if (on (p, P::dlyOn) && ! delayActive) { delay.reset(); delayActive = true; }
    if (on (p, P::revOn) && ! reverbActive) { reverb.reset(); reverbActive = true; }
    if (firstParams)
    {
        delayActive = on (p, P::dlyOn);
        reverbActive = on (p, P::revOn);
    }
    firstParams = false;
}

void Rig::processMono (float* x, int n)
{
    input.process (x, n);
    {
        float pk = 0.0f;
        for (int i = 0; i < n; ++i) pk = std::max (pk, std::abs (x[i]));
        m.gateInPeak.store (pk);
    }
    runFaded (fGate, x, n, dry, [&] (float* b, int k) { gate.process (b, k); });
    runFaded (fComp, x, n, dry, [&] (float* b, int k) { comp.process (b, k); });
    runFaded (fWah, x, n, dry, [&] (float* b, int k) { wah.process (b, k); });
    runFaded (fPitch, x, n, dry, [&] (float* b, int k) { pitch.process (b, k); });
    runFaded (fDrive, x, n, dry, [&] (float* b, int k) { drive.process (b, k); });

    const bool neural = choice (p, P::ampModel) == choices::neuralCapture;
    runFaded (fAmp, x, n, dry, [&] (float* b, int k)
    {
        if (neural)
        {
            // gain knob trims the capture's input, level trims its output
            nam.process (b, k, (p[(size_t) P::ampGain] - 5.0f) * 2.4f, p[(size_t) P::ampLevel]);
            // the tone knobs still work on captures, as a post EQ
            for (int i = 0; i < k; ++i)
                b[i] = namTreble.process (namMid.process (namBass.process (b[i])));
        }
        else
            amp.process (b, k);
    });
    runFaded (fCab, x, n, dry, [&] (float* b, int k) { cab.process (b, k); });
    runFaded (fEq, x, n, dry, [&] (float* b, int k) { eq.process (b, k); });
}

void Rig::processStereo (float* l, float* r, int n)
{
    runFadedStereo (fMod, l, r, n, dry, dryR, [&] (float* a, float* b, int k) { mod.process (a, b, k); });

    auto timeFx = [&] (bool enabled, bool& active, int& quiet, auto& fx)
    {
        if (! active) return;
        if (enabled)
        {
            fx.process (l, r, n);
            quiet = 0;
            return;
        }
        // switched off: feed silence and let the tail ring out
        std::fill (tailL.begin(), tailL.begin() + n, 0.0f);
        std::fill (tailR.begin(), tailR.begin() + n, 0.0f);
        fx.process (tailL.data(), tailR.data(), n);
        float peak = 0.0f;
        for (int i = 0; i < n; ++i)
        {
            l[i] += tailL[(size_t) i];
            r[i] += tailR[(size_t) i];
            peak = std::max (peak, std::max (std::abs (tailL[(size_t) i]), std::abs (tailR[(size_t) i])));
        }
        quiet = peak < 1.0e-5f ? quiet + n : 0;
        if (quiet > (int) sr) { active = false; fx.reset(); }
    };
    timeFx (on (p, P::dlyOn), delayActive, delayQuiet, delay);
    timeFx (on (p, P::revOn), reverbActive, reverbQuiet, reverb);

    runFadedStereo (fLofi, l, r, n, dry, dryR, [&] (float* a, float* b, int k) { lofi.process (a, b, k); });
    doubler.process (l, r, n);
}

void Rig::process (const float* in, float* outL, float* outR, int total)
{
    // Filter tails decay into numbers so small the processor handles them many times
    // slower. Whoever calls this (the sound card's thread, an offline render) gets them
    // rounded to zero for the length of the call.
    juce::ScopedNoDenormals noDenormals;
    int offset = 0;
    while (offset < total)
    {
        const int n = std::min (maxBlock, total - offset);
        float* l = outL + offset;
        float* r = outR + offset;

        probe.process (in + offset, n); // raw input, before any gain or filtering
        float inPk = 0.0f;
        for (int i = 0; i < n; ++i)
        {
            l[i] = in[offset + i] * inGain.next();
            inPk = std::max (inPk, std::abs (l[i]));
        }

        processMono (l, n);
        std::copy (l, l + n, r);
        processStereo (l, r, n);

        float outPk = 0.0f;
        for (int i = 0; i < n; ++i)
        {
            const float g = outGain.next();
            l[i] *= g;
            r[i] *= g;
            outPk = std::max (outPk, std::max (std::abs (l[i]), std::abs (r[i])));
        }
        if (outPk > 1.0f) m.clipEvents.fetch_add (1);
        limiter.process (l, r, n);

        // sanity: never let a NaN reach the speakers
        for (int i = 0; i < n; ++i)
            if (! std::isfinite (l[i]) || ! std::isfinite (r[i]))
            {
                reset();
                std::fill (l, l + n, 0.0f);
                std::fill (r, r + n, 0.0f);
                break;
            }

        m.inPeak.store (std::max (inPk, m.inPeak.load() * 0.0f + inPk));
        m.outPeak.store (outPk);
        m.compReduction.store (comp.gainReductionDb());
        m.gateOpen.store (gate.isOpen());
        m.mains.store (input.detectedMains());
        offset += n;
    }
}
} // namespace rh
