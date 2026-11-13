// Riffhouse - free, open-source guitar rig
// Copyright (C) 2026 Riffhouse contributors. Licensed under AGPL-3.0-or-later.
#include "Rig.h"

#include "PhraseCatcher.h"

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
    modOld.prepare (sr);
    modXfadeLen = std::max (1, (int) (0.020 * sr));
    modXfade = 0;
    for (Delay* d : { &delayA, &delayB }) d->prepare (sr);
    for (Reverb* r : { &reverbA, &reverbB }) r->prepare (sr);
    dlyTailBusy = revTailBusy = false;
    tailFadeStep = (float) (1.0 / (0.020 * sr));
    dlyTailGain = revTailGain = 1.0f;
    dlyTailFading = revTailFading = false;
    swapOutStep = (float) (1.0 / (0.006 * sr));
    swapInStep = (float) (1.0 / (0.012 * sr));
    swapHoldLen = (int) (0.010 * sr);
    swap = Swap::none;
    swapWanted = false;
    swapGain = 1.0f;
    lofi.prepare (sr);
    doubler.prepare (sr);
    limiter.prepare (sr);
    for (Fade* f : { &fGate, &fComp, &fWah, &fPitch, &fDrive, &fAmp, &fCab, &fEq, &fMod, &fLofi })
        f->prepare (sr);
    inGain.prepare (sr, 20.0f);
    trimGain.prepare (sr, 30.0f);
    trimGain.reset (dbToGain (levelTrimDb.load()));
    outGain.prepare (sr, 20.0f);
    dry.assign ((size_t) maxBlock, 0.0f);
    gateGains.assign ((size_t) maxBlock, 1.0f);
    std::fill (std::begin (gateDelay), std::end (gateDelay), 1.0f);
    gateDelayAt = 0;
    dryR.assign ((size_t) maxBlock, 0.0f);
    tailL.assign ((size_t) maxBlock, 0.0f);
    tailR.assign ((size_t) maxBlock, 0.0f);
    modTmpL.assign ((size_t) maxBlock, 0.0f);
    modTmpR.assign ((size_t) maxBlock, 0.0f);
    firstParams = true;
    setParams (p);
    cab.prepare (sr, maxBlock);
    setParams (p);

    // A rig made from nothing has empty capacitors and filters: the drive circuits charge to their
    // bias, and a high-gain amp after them turns that into a thump (up to -40 dB) the moment the app
    // opens. Here there is time, so the whole rig runs on silence for a third of a second first, as
    // real gear is already warm when you start playing. The meters are left as they were.
    {
        std::vector<float> quiet ((size_t) maxBlock, 0.0f), l ((size_t) maxBlock), r ((size_t) maxBlock);
        const float inWas = meters().inPeak.load(), outWas = meters().outPeak.load(), gateWas = meters().gateInPeak.load();
        for (int done = 0, n = (int) (0.33 * sr); done < n; done += maxBlock)
        {
            std::fill (quiet.begin(), quiet.end(), 0.0f);
            setParams (p);
            process (quiet.data(), l.data(), r.data(), maxBlock);
        }
        meters().inPeak.store (inWas);
        meters().outPeak.store (outWas);
        meters().gateInPeak.store (gateWas);
    }
}

void Rig::reset()
{
    input.reset(); gate.reset(); comp.reset(); wah.reset(); pitch.reset(); drive.reset(); amp.reset(); cab.reset();
    eq.reset(); mod.reset(); modOld.reset(); modXfade = 0; lofi.reset(); doubler.reset(); limiter.reset();
    for (Delay* d : { &delayA, &delayB }) d->reset();
    for (Reverb* r : { &reverbA, &reverbB }) r->reset();
    dlyTailBusy = revTailBusy = false;
    if (swap != Swap::none) { swap = Swap::none; applyParams (swapTo, false); }
    swapGain = 1.0f;
    std::fill (std::begin (gateDelay), std::end (gateDelay), 1.0f);
    gateDelayAt = 0;
}

int Rig::latencySamples() const
{
    // one amp is in the path at a time: the modelled one, or the capture
    const bool neural = choice (p, P::ampModel) == choices::neuralCapture;
    return neural ? nam.latencySamples() : (int) std::lround (amp.latencySamples());
}

float Rig::delayTimeMs() const
{
    static const float beats[] = { 0.0f, 2.0f, 1.5f, 1.0f, 0.75f, 0.5f, 1.0f / 3.0f, 0.25f };
    const int s = choice (p, P::dlySync);
    if (s <= 0 || s >= (int) (sizeof (beats) / sizeof (beats[0])))
        return p[(size_t) P::dlyTime];
    return (float) std::min (2000.0, 60000.0 / tempo * beats[s]);
}

void Rig::snapParams (const ParamValues& v)
{
    firstParams = true;
    setParams (v);
    finishPendingWork();
    reset();
    trimGain.reset (dbToGain (levelTrimDb.load()));
}

void Rig::startFresh (const ParamValues& v, double sampleRate, int block)
{
    p = v;
    prepare (sampleRate, block);
    finishPendingWork();
    prepare (sampleRate, block); // the cabinet's response is built from the settings: once more with it in place
    finishPendingWork();
}

bool Rig::needsSwap (const ParamValues& next) const
{
    // Changes the rig can only make with a hard restart of that part: an amp model, a pedal
    // circuit, a delay or reverb type. Only where that part can be heard.
    auto moved = [&] (P id) { return choice (p, id) != choice (next, id); };
    auto heard = [&] (P onId) { return on (p, onId) || on (next, onId); };
    return (heard (P::ampOn) && moved (P::ampModel))
        || (heard (P::drvOn) && moved (P::drvType))
        || (heard (P::wahOn) && moved (P::wahMode))
        || (heard (P::pitchOn) && moved (P::pitchMode))
        || (heard (P::modOn) && moved (P::modType))
        || ((heard (P::dlyOn) || delayActive) && moved (P::dlyType))
        || ((heard (P::revOn) || reverbActive) && moved (P::revType));
}

void Rig::setParams (const ParamValues& v)
{
    if (firstParams) { applyParams (v, false); return; }
    if (swap == Swap::out || swapWanted || needsSwap (v))
    {
        swapTo = v;
        swapWanted = false;
        if (swap != Swap::out)
        {
            swap = Swap::out;
            // from wherever a fade back in had got to, so the dip never jumps
            swapOutLeft = (int) std::ceil (swapGain / swapOutStep);
            // a tail still ringing from the change before makes way, faded, before the swap
            if (dlyTailBusy) dlyTailFading = true;
            if (revTailBusy) revTailFading = true;
            if (dlyTailBusy || revTailBusy)
                swapOutLeft = std::max (swapOutLeft, (int) std::ceil (std::max (dlyTailGain, revTailGain) / tailFadeStep) + 1);
        }
        return;
    }
    applyParams (v, false);
}

void Rig::applyParams (const ParamValues& v, bool toneChange)
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
    {
        const int type = choice (p, P::modType);
        if (! firstParams && modTypeNow >= 0 && type != modTypeNow && ! fMod.idle())
        {
            modOld = mod; // same sizes as prepared, so this copies and never allocates
            modXfade = modXfadeLen;
        }
        modTypeNow = type;
        mod.set (type, get (P::modRate), get (P::modDepth), get (P::modMix));
    }

    const float space = get (P::space);
    if (toneChange)
    {
        // The old tone's echoes and room go on ringing in the spare pair, with the old
        // settings, while the new tone starts with empty ones. A tail still ringing from the
        // change before was faded out during the dip.
        auto spill = [&] (bool& active, bool& tailBusy, int& tailQuiet, float& tailGain, bool& tailFading, auto*& now, auto*& tail, std::initializer_list<P> ids)
        {
            bool same = true;
            for (P id : ids) same = same && old[(size_t) id] == p[(size_t) id];
            if (! active || same) return;
            tail->reset();
            std::swap (now, tail);
            now->reset();
            tailBusy = true;
            tailQuiet = 0;
            tailGain = 1.0f;
            tailFading = false;
            active = false;
        };
        spill (delayActive, dlyTailBusy, dlyTailQuiet, dlyTailGain, dlyTailFading, dly, dlyTail,
               { P::dlyOn, P::dlyType, P::dlyTime, P::dlySync, P::dlyFeedback, P::dlyTone, P::dlyMix, P::space });
        spill (reverbActive, revTailBusy, revTailQuiet, revTailGain, revTailFading, rev, revTail,
               { P::revOn, P::revType, P::revDecay, P::revPreDelay, P::revTone, P::revMix, P::space });
    }
    dly->set (choice (p, P::dlyType), delayTimeMs(), get (P::dlyFeedback), get (P::dlyTone), std::min (10.0f, get (P::dlyMix) * space));
    rev->set (choice (p, P::revType), get (P::revDecay), get (P::revPreDelay), get (P::revTone), std::min (10.0f, get (P::revMix) * space));
    lofi.set (get (P::lofiBits), get (P::lofiRate), get (P::lofiWobble), get (P::lofiNoise), get (P::lofiFilter), get (P::lofiMix));
    doubler.set (get (P::doubler));

    auto setFade = [&] (Fade& f, P id, auto&& onStart)
    {
        const bool want = on (p, id);
        if (firstParams) { f.snap (want); return; }
        if (want && f.idle()) onStart();
        // during a tone change the sound is down, so a pedal goes straight to where it is wanted
        if (toneChange) { f.snap (want); return; }
        f.setOn (want);
    };
    // Modulation and lo-fi sit after the dip, and their delay lines still hold the sound from
    // before it: snapped on, that sound jumps in at full level (a click), snapped off it stops
    // dead. They always fade.
    auto setFadeAfterDip = [&] (Fade& f, P id, auto&& onStart)
    {
        const bool want = on (p, id);
        if (firstParams) { f.snap (want); return; }
        if (want && f.idle()) onStart();
        f.setOn (want);
    };
    setFade (fGate, P::gateOn, [&] { gate.reset(); });
    setFade (fComp, P::compOn, [&] { comp.wake(); });
    setFade (fWah, P::wahOn, [&] { wah.reset(); });
    setFade (fPitch, P::pitchOn, [&] { pitch.reset(); });
    setFade (fDrive, P::drvOn, [&] { drive.reset(); });
    setFade (fAmp, P::ampOn, [&] { amp.reset(); });
    setFade (fCab, P::cabOn, [&] {});
    setFade (fEq, P::eqOn, [&] { eq.reset(); });
    setFadeAfterDip (fMod, P::modOn, [&] { mod.wake(); });
    setFadeAfterDip (fLofi, P::lofiOn, [&] { lofi.wake(); });

    // delay and reverb keep ringing after they are switched off
    if (on (p, P::dlyOn) && ! delayActive) { dly->reset(); delayActive = true; }
    if (on (p, P::revOn) && ! reverbActive) { rev->reset(); reverbActive = true; }
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
        if (pk > m.stagePeak.load (std::memory_order_relaxed)) m.stagePeak.store (pk, std::memory_order_relaxed);
    }
    if (fGate.idle())
        std::fill (gateGains.begin(), gateGains.begin() + n, 1.0f);
    else
    {
        // the same as runFaded, with the gain of each sample kept for after the amp
        const bool full = fGate.full();
        if (! full) std::copy (x, x + n, dry.begin());
        gate.process (x, n, gateGains.data());
        if (! full)
            for (int i = 0; i < n; ++i)
            {
                const float g = 1.0f + (gateGains[(size_t) i] - 1.0f) * fGate.next();
                gateGains[(size_t) i] = g;
                x[i] = dry[(size_t) i] * g;
            }
    }
    if (fComp.idle()) comp.listen (x, n);
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
    // The gate again, after the drive and the amp, in step with what comes out of them: they
    // squash a fade made in front of them, so a closing gate would leave crunch behind.
    {
        int late = neural ? nam.latencySamples() : (int) std::lround (amp.latencySamples());
        if (! fDrive.idle()) late += (int) std::lround (drive.latencySamples());
        late = juce::jlimit (0, kGateDelay - 1, late);
        for (int i = 0; i < n; ++i)
        {
            gateDelay[gateDelayAt] = gateGains[(size_t) i];
            x[i] *= gateDelay[(gateDelayAt - late + kGateDelay) % kGateDelay];
            gateDelayAt = (gateDelayAt + 1) % kGateDelay;
        }
    }
    runFaded (fCab, x, n, dry, [&] (float* b, int k) { cab.process (b, k); });
    runFaded (fEq, x, n, dry, [&] (float* b, int k) { eq.process (b, k); });
}

void Rig::processStereo (float* l, float* r, int n)
{
    if (fMod.idle()) mod.listen (l, r, n);
    runFadedStereo (fMod, l, r, n, dry, dryR, [&] (float* a, float* b, int k)
    {
        if (modXfade <= 0) { mod.process (a, b, k); return; }
        std::copy (a, a + k, modTmpL.begin());
        std::copy (b, b + k, modTmpR.begin());
        modOld.process (modTmpL.data(), modTmpR.data(), k);
        mod.process (a, b, k);
        for (int i = 0; i < k; ++i)
        {
            const float t = modXfade > 0 ? (float) modXfade / (float) modXfadeLen : 0.0f; // old one's share
            const float g = t * t * (3.0f - 2.0f * t);
            a[i] += (modTmpL[(size_t) i] - a[i]) * g;
            b[i] += (modTmpR[(size_t) i] - b[i]) * g;
            if (modXfade > 0) --modXfade;
        }
    });

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
    // the last tone's echoes and room, ringing on with its own settings (see applyParams)
    auto spillTail = [&] (bool& busy, int& quiet, float& gain, bool fading, auto& fx)
    {
        if (! busy) return;
        std::fill (tailL.begin(), tailL.begin() + n, 0.0f);
        std::fill (tailR.begin(), tailR.begin() + n, 0.0f);
        fx.process (tailL.data(), tailR.data(), n);
        float peak = 0.0f;
        for (int i = 0; i < n; ++i)
        {
            if (fading) gain = std::max (0.0f, gain - tailFadeStep);
            const float g = gain;
            l[i] += tailL[(size_t) i] * g;
            r[i] += tailR[(size_t) i] * g;
            peak = std::max (peak, g * std::max (std::abs (tailL[(size_t) i]), std::abs (tailR[(size_t) i])));
        }
        quiet = peak < 1.0e-5f ? quiet + n : 0;
        if (quiet > (int) sr / 4 || gain <= 0.0f) { busy = false; fx.reset(); gain = 1.0f; }
    };
    timeFx (on (p, P::dlyOn), delayActive, delayQuiet, *dly);
    timeFx (on (p, P::revOn), reverbActive, reverbQuiet, *rev);
    spillTail (dlyTailBusy, dlyTailQuiet, dlyTailGain, dlyTailFading, *dlyTail);
    spillTail (revTailBusy, revTailQuiet, revTailGain, revTailFading, *revTail);

    if (fLofi.idle()) lofi.listen (l, r, n);
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
        // the dip of a tone change: at its bottom, everything changes at once
        if (swap == Swap::out && swapOutLeft <= 0)
        {
            applyParams (swapTo, true);
            // held down 10 ms more while the new tone's own followers (the amp's sag, a
            // compressor, a wah) settle on what is playing, so it comes up already steady
            swap = Swap::hold;
            swapHoldLeft = swapHoldLen;
            swapGain = 0.0f;
        }
        if (swap == Swap::hold && swapHoldLeft <= 0) swap = Swap::in;
        int n = std::min (maxBlock, total - offset);
        if (swap == Swap::out) n = std::min (n, swapOutLeft); // stop exactly at the bottom
        if (swap == Swap::hold) n = std::min (n, swapHoldLeft);
        float* l = outL + offset;
        float* r = outR + offset;

        probe.process (in + offset, n); // raw input, before any gain or filtering
        if (tap != nullptr) tap->push (in + offset, n);
        float inPk = 0.0f;
        for (int i = 0; i < n; ++i)
        {
            l[i] = in[offset + i] * inGain.next();
            inPk = std::max (inPk, std::abs (l[i]));
        }

        processMono (l, n);
        if (swap != Swap::none)
        {
            // down in 6 ms (or as long as an old tail takes to fade), back up in 12 ms, on a
            // smooth curve; delay and reverb come after this, so their tails go on
            if (swap == Swap::hold)
            {
                std::fill (l, l + n, 0.0f);
                swapHoldLeft -= n;
            }
            else
            {
                for (int i = 0; i < n; ++i)
                {
                    if (swap == Swap::out) swapGain = std::max (0.0f, swapGain - swapOutStep);
                    else swapGain = std::min (1.0f, swapGain + swapInStep);
                    const float s = swapGain * swapGain * (3.0f - 2.0f * swapGain);
                    l[i] *= s;
                }
                if (swap == Swap::out) swapOutLeft -= n;
                else if (swapGain >= 1.0f) swap = Swap::none;
            }
        }
        std::copy (l, l + n, r);
        processStereo (l, r, n);

        float outPk = 0.0f;
        trimGain.setTarget (dbToGain (levelTrimDb.load()));
        for (int i = 0; i < n; ++i)
        {
            const float g = outGain.next() * trimGain.next();
            l[i] *= g;
            r[i] *= g;
            outPk = std::max (outPk, std::max (std::abs (l[i]), std::abs (r[i])));
        }
        if (outPk > 1.0f) m.clipEvents.fetch_add (1);
        if (! measuring) limiter.process (l, r, n);

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
        m.gateThreshNow.store (gate.thresholdNowDb());
        m.mains.store (input.detectedMains());
        m.whines.store (input.whines().removing());
        m.whineHz.store (input.whines().removingHz (0));
        offset += n;
    }
}
} // namespace rh
