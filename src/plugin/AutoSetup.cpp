// Riffhouse - free, open-source guitar rig
// Copyright (C) 2026 Riffhouse contributors. Licensed under AGPL-3.0-or-later.
#include "AutoSetup.h"

#include "../engine/InputGuard.h"

namespace rh
{
namespace
{
    constexpr float kHeardThreshold = 0.0032f; // -50 dBFS: something is playing
    constexpr float kSureThreshold = 0.063f;   // -24 dBFS: that is definitely a guitar
    constexpr double kDwellMs = 1100.0;
    constexpr double kTuneMs = 1600.0;
    constexpr double kQuietMs = 2600.0;
    constexpr double kPauseMs = 1500.0;        // how long a finished step's result stays up
    constexpr double kPlayMs = 4500.0;         // hard playing wanted
    constexpr double kToneMs = 1800.0;         // of which this much to judge the treble, the rest to set the level
    constexpr double kPlayGiveUpMs = 20000.0;
    constexpr double kWaitForQuietMs = 9000.0; // then the noise is measured whatever is heard
    constexpr float kNoisyFixDb = -48.0f;      // hiss peaks over this with Input Fix on: the fix is held back

    double nowMs() { return juce::Time::getMillisecondCounterHiRes(); }

    bool isLoopback (const juce::String& name)
    {
        const auto n = name.toLowerCase();
        for (auto* w : { "stereo mix", "loopback", "what u hear", "wave out", "cable output", "voicemeeter", "monitor of" })
            if (n.contains (w)) return true;
        return false;
    }

    juce::AudioIODeviceType* findType (juce::AudioDeviceManager& dm, const juce::String& name)
    {
        for (auto* t : dm.getAvailableDeviceTypes())
            if (t->getTypeName() == name) return t;
        return nullptr;
    }

    juce::String defaultOutputFor (juce::AudioIODeviceType* t)
    {
        if (t == nullptr) return {};
        const auto names = t->getDeviceNames (false);
        const int idx = t->getDefaultDeviceIndex (false);
        return juce::isPositiveAndBelow (idx, names.size()) ? names[idx] : names[0];
    }
} // namespace

bool AutoSetup::isAsioWrapper (const juce::String& driver)
{
    const auto n = driver.toLowerCase();
    for (auto* w : { "asio4all", "flexasio", "generic low latency", "asio2wasapi", "realtek asio", "voicemeeter", "fl studio", "ableton" })
        if (n.contains (w)) return true;
    return false;
}

bool AutoSetup::looksLikeOnboardInput (const juce::String& inputName, const juce::String& typeName)
{
    if (typeName == "ASIO" && ! isAsioWrapper (inputName)) return false;
    const auto n = inputName.toLowerCase();
    // Audio interfaces and guitar cables have a proper high-impedance
    // instrument input, so they don't need the fix.
    for (auto* w : { "focusrite", "scarlett", "behringer", "umc", "audient", "presonus", "steinberg", "motu", "universal audio",
                     "volt", "apollo", "arturia", "minifuse", "m-audio", "m-track", "native instruments", "komplete audio",
                     "irig", "line 6", "helix", "pod ", "boss", "zoom", "tascam", "roland", "rubix", "yamaha", "esi ",
                     "solid state logic", "ssl ", "ik multimedia", "mackie", "nux", "valeton", "guitar", "instrument",
                     "hi-z", "usb audio codec", "rme", "antelope", "apogee", "id4", "id14", "evo" })
        if (n.contains (w)) return false;
    return true;
}

AutoSetup::AutoSetup (juce::AudioDeviceManager& d, RiffhouseProcessor& p) : dm (d), proc (p) {}

AutoSetup::~AutoSetup()
{
    stopTimer();
    proc.muteOutput.store (false);
}

void AutoSetup::setPhase (Phase p)
{
    currentPhase = p;
    phaseProgress = 0.0f;
    if (onPhaseChanged) onPhaseChanged();
}

void AutoSetup::pauseThen (const juce::String& found, std::function<void()> next)
{
    foundText = found;
    phaseProgress = 1.0f;
    afterPause = std::move (next);
    pauseUntil = nowMs() + kPauseMs;
    if (onPhaseChanged) onPhaseChanged();
}

void AutoSetup::buildCandidates()
{
    candidates.clear();
    for (auto* t : dm.getAvailableDeviceTypes())
        t->scanForDevices();

   #if JUCE_WINDOWS
    if (auto* asio = findType (dm, "ASIO"))
        for (auto& name : asio->getDeviceNames (true))
            if (! isAsioWrapper (name))
                candidates.push_back ({ "ASIO", name });
    if (auto* wasapi = findType (dm, "Windows Audio"))
    {
        const auto names = wasapi->getDeviceNames (true);
        const int def = wasapi->getDefaultDeviceIndex (true);
        if (juce::isPositiveAndBelow (def, names.size()) && ! isLoopback (names[def]))
            candidates.push_back ({ "Windows Audio", names[def] });
        for (int i = 0; i < names.size(); ++i)
            if (i != def && ! isLoopback (names[i]))
                candidates.push_back ({ "Windows Audio", names[i] });
    }
    if (candidates.empty())
        if (auto* asio = findType (dm, "ASIO")) // only wrappers: still better than nothing
            for (auto& name : asio->getDeviceNames (true))
                candidates.push_back ({ "ASIO", name });
   #endif

    if (candidates.empty())
    {
        if (auto* t = dm.getCurrentDeviceTypeObject())
        {
            const auto names = t->getDeviceNames (true);
            const int def = t->getDefaultDeviceIndex (true);
            if (juce::isPositiveAndBelow (def, names.size()))
                candidates.push_back ({ t->getTypeName(), names[def] });
            for (int i = 0; i < names.size(); ++i)
                if (i != def && ! isLoopback (names[i]))
                    candidates.push_back ({ t->getTypeName(), names[i] });
        }
    }
    candidatePeak.assign (candidates.size(), 0.0f);
}

void AutoSetup::start()
{
    stopTimer();
    originalState = dm.createStateXml();
    originalType = dm.getCurrentAudioDeviceType();
    result = {};
    best = {};
    afterPause = nullptr;
    foundText = {};
    playPeak = 0.0f;
    noiseStarted = fixRetried = false;
    pass = 0;
    candidateIndex = 0;
    proc.muteOutput.store (true);
    buildCandidates();
    if (candidates.empty())
    {
        detailText = "No audio inputs found on this computer.";
        setPhase (Phase::noSignal);
        return;
    }
    setPhase (Phase::listening);
    openCandidate (candidates[0]);
    startTimerHz (25);
}

void AutoSetup::cancel()
{
    stopTimer();
    afterPause = nullptr;
    if (currentPhase != Phase::finished && currentPhase != Phase::idle)
    {
        if (originalType.isNotEmpty()) dm.setCurrentAudioDeviceType (originalType, true);
        dm.initialise (2, 2, originalState.get(), true);
    }
    proc.muteOutput.store (false);
    currentPhase = Phase::idle;
}

bool AutoSetup::openCandidate (const Candidate& c)
{
    proc.tiltProbe().restart(); // a new input: measure its tone from scratch
    if (dm.getCurrentAudioDeviceType() != c.type)
        dm.setCurrentAudioDeviceType (c.type, true);
    juce::AudioDeviceManager::AudioDeviceSetup s;
    dm.getAudioDeviceSetup (s);
    s.inputDeviceName = c.input;
    s.outputDeviceName = c.type == "ASIO" ? c.input : (s.outputDeviceName.isNotEmpty() ? s.outputDeviceName : defaultOutputFor (findType (dm, c.type)));
    s.useDefaultInputChannels = false;
    s.inputChannels.clear();
    s.inputChannels.setRange (0, 2, true);
    s.useDefaultOutputChannels = true;
    const auto err = dm.setAudioDeviceSetup (s, true);
    detailText = "Listening on " + c.input;
    dwellStart = nowMs();
    dwellPeak = 0.0f;
    proc.rawInputPeak.store (0.0f);
    return err.isEmpty() && dm.getCurrentAudioDevice() != nullptr;
}

void AutoSetup::timerCallback()
{
    const double t = nowMs();
    const float raw = proc.rawInputPeak.exchange (0.0f);
    level = std::max (raw, level * 0.85f);
    const double sinceTick = lastTick > 0 ? std::min (200.0, t - lastTick) : 40.0;
    lastTick = t;

    if (afterPause != nullptr)
    {
        if (t < pauseUntil) return;
        auto next = std::move (afterPause);
        afterPause = nullptr;
        foundText = {};
        next();
        return;
    }

    switch (currentPhase)
    {
        case Phase::listening:
        {
            const double elapsed = t - dwellStart;
            if (elapsed > 200.0) dwellPeak = std::max (dwellPeak, raw);
            const int n = (int) candidates.size();
            if (n == 1)
            {
                phaseProgress = (float) std::min (1.0, elapsed / 15000.0);
                if (dwellPeak > kHeardThreshold && elapsed > 1200.0)
                {
                    chosen = candidates[0];
                    chosenPeak = dwellPeak;
                    pauseThen ("Found your guitar on " + chosen.input, [this] { beginTuning(); });
                }
                else if (elapsed > 15000.0)
                {
                    stopTimer();
                    proc.muteOutput.store (false);
                    setPhase (Phase::noSignal);
                }
                return;
            }
            phaseProgress = (float) std::min (1.0, ((double) (pass * n + candidateIndex) + elapsed / kDwellMs) / (double) (3 * n));
            if (dwellPeak > kSureThreshold && elapsed > 500.0)
            {
                chosen = candidates[(size_t) candidateIndex];
                chosenPeak = dwellPeak;
                pauseThen ("Found your guitar on " + chosen.input, [this] { beginTuning(); });
                return;
            }
            if (elapsed < kDwellMs) return;
            candidatePeak[(size_t) candidateIndex] = std::max (candidatePeak[(size_t) candidateIndex], dwellPeak);
            if (++candidateIndex >= n)
            {
                candidateIndex = 0;
                ++pass;
                const auto it = std::max_element (candidatePeak.begin(), candidatePeak.end());
                if (*it > kHeardThreshold)
                {
                    chosen = candidates[(size_t) (it - candidatePeak.begin())];
                    chosenPeak = *it;
                    pauseThen ("Found your guitar on " + chosen.input, [this] { beginTuning(); });
                    return;
                }
                if (pass >= 3)
                {
                    stopTimer();
                    openCandidate (candidates[0]);
                    proc.muteOutput.store (false);
                    setPhase (Phase::noSignal);
                    return;
                }
            }
            openCandidate (candidates[(size_t) candidateIndex]);
            return;
        }

        case Phase::tuning:
        {
            const double elapsed = t - dwellStart;
            if (elapsed > 300.0)
            {
                if (xrunStart < 0) xrunStart = dm.getXRunCount();
                tunePeak = std::max (tunePeak, raw);
                strumPeak = std::max (strumPeak, raw);
                cpuMax = std::max (cpuMax, dm.getCpuUsage());
            }
            phaseProgress = (float) std::min (0.98, ((double) modeIndex + (double) bufferIndex / 6.0 + elapsed / kTuneMs / 6.0) / (double) std::max (1, modes.size()));
            if (elapsed < kTuneMs) return;

            const int xr = dm.getXRunCount() - std::max (0, xrunStart);
            auto* dev = dm.getCurrentAudioDevice();
            const bool ok = ! lastOpenFailed && dev != nullptr && xr <= 0 && cpuMax < 0.7 && tunePeak > 1.0e-7f;
            if (ok)
            {
                const double lat = measuredLatencyMs();
                if (lat < best.latency)
                    best = { modes[modeIndex], dev->getCurrentBufferSizeSamples(), lat, dev->getCurrentSampleRate() };
                ++modeIndex; // this mode's best is found, try the next mode
                bufferIndex = 0;
            }
            else
                ++bufferIndex;

            if (best.latency <= 12.0 || modeIndex >= modes.size())
            {
                finishTuning();
                return;
            }
            if (bufferIndex >= std::min (6, buffers.size()) || bufferIndex == 0)
            {
                if (bufferIndex != 0) { ++modeIndex; bufferIndex = 0; }
                // open the next mode and find its buffer sizes
                while (modeIndex < modes.size())
                {
                    if (openMode (modes[modeIndex], 0) && ! buffers.isEmpty())
                        break;
                    ++modeIndex;
                }
                if (modeIndex >= modes.size()) { finishTuning(); return; }
            }
            openMode (modes[modeIndex], buffers[bufferIndex]);
            return;
        }

        case Phase::playing:
        {
            const double elapsed = t - phaseStart;
            playPeak = std::max (playPeak, raw);
            // playing counts while it is loud: the ring-out between strums is not what the level is set by
            if (raw > std::max (2.0f * kHeardThreshold, 0.1f * playPeak)) playedMs += sinceTick;
            phaseProgress = (float) std::min (1.0, playedMs / kPlayMs);
            // First the treble: once that is judged Input Fix goes on, and the rest of the
            // playing is heard through it, because the fix lifts the peaks the level is set by.
            if (! fixSet && playedMs >= kToneMs)
            {
                setFixFromTone();
                fixSetAt = t;
            }
            if (fixSet)
            {
                const float stage = proc.meters().stagePeak.exchange (0.0f);
                if (t - fixSetAt > 250.0) fixedPeak = std::max (fixedPeak, stage); // once the fix has settled in
            }
            if (playedMs >= kPlayMs || elapsed > kPlayGiveUpMs) finishPlaying();
            return;
        }

        case Phase::quiet:
        {
            if (! noiseStarted)
            {
                // nothing is measured until the strings have really stopped
                const float still = std::max (2.0f * kHeardThreshold, 0.05f * std::max (playPeak, strumPeak));
                if (raw > still) quietSince = t;
                if (t - quietSince > 700.0 || t - phaseStart > kWaitForQuietMs) startNoise();
                return;
            }
            const double elapsed = t - noiseStart;
            phaseProgress = (float) std::min (1.0, elapsed / kQuietMs);
            if (elapsed > 300.0) noiseSamples.add (proc.meters().gateInPeak.load());
            if (elapsed >= kQuietMs) finish();
            return;
        }

        case Phase::idle:
        case Phase::finished:
        case Phase::noSignal:
            stopTimer();
            return;
    }
}

void AutoSetup::beginTuning()
{
    setPhase (Phase::tuning);
    strumPeak = chosenPeak;
    modes.clear();
    if (chosen.type == "ASIO")
        modes.add ("ASIO");
    else if (chosen.type.startsWith ("Windows Audio"))
    {
        for (auto* m : { "Windows Audio (Exclusive Mode)", "Windows Audio (Low Latency Mode)", "Windows Audio" })
            if (findType (dm, m) != nullptr) modes.add (m);
    }
    else
        modes.add (chosen.type);

    modeIndex = 0;
    bufferIndex = 0;
    while (modeIndex < modes.size() && ! (openMode (modes[modeIndex], 0) && ! buffers.isEmpty()))
        ++modeIndex;
    if (modeIndex >= modes.size()) { finishTuning(); return; }
    openMode (modes[modeIndex], buffers[0]);
}

bool AutoSetup::openMode (const juce::String& type, int bufferSize)
{
    if (dm.getCurrentAudioDeviceType() != type)
        dm.setCurrentAudioDeviceType (type, true);
    auto* t = findType (dm, type);
    if (t == nullptr) return false;
    juce::AudioDeviceManager::AudioDeviceSetup s;
    dm.getAudioDeviceSetup (s);
    s.inputDeviceName = chosen.input;
    if (type == "ASIO") s.outputDeviceName = chosen.input;
    else if (s.outputDeviceName.isEmpty() || ! t->getDeviceNames (false).contains (s.outputDeviceName))
        s.outputDeviceName = defaultOutputFor (t);
    s.useDefaultInputChannels = false;
    s.inputChannels.clear();
    s.inputChannels.setRange (0, 2, true);
    s.useDefaultOutputChannels = true;
    s.bufferSize = bufferSize;
    s.sampleRate = 0;
    if (auto* dev = dm.getCurrentAudioDevice())
    {
        const auto rates = dev->getAvailableSampleRates();
        if (rates.contains (48000.0)) s.sampleRate = 48000.0;
        else if (rates.contains (44100.0)) s.sampleRate = 44100.0;
    }
    const auto err = dm.setAudioDeviceSetup (s, true);
    auto* dev = dm.getCurrentAudioDevice();
    lastOpenFailed = err.isNotEmpty() || dev == nullptr;
    if (lastOpenFailed)
    {
        dwellStart = nowMs() - kTuneMs; // judge it right away as a failure
        return false;
    }

    if (bufferSize == 0)
    {
        buffers.clear();
        for (int b : dev->getAvailableBufferSizes())
            if (b >= 32 && b <= 1024) buffers.addIfNotAlreadyThere (b);
        buffers.sort();
        if (buffers.isEmpty()) buffers.add (dev->getCurrentBufferSizeSamples());
    }

    juce::String modeName = type;
    if (type == "Windows Audio (Exclusive Mode)") modeName = "exclusive mode";
    else if (type == "Windows Audio (Low Latency Mode)") modeName = "low latency mode";
    else if (type == "Windows Audio") modeName = "shared mode";
    detailText = "Trying " + juce::String (dev->getCurrentBufferSizeSamples()) + " samples in " + modeName;
    dwellStart = nowMs();
    xrunStart = -1;
    tunePeak = 0.0f;
    cpuMax = 0.0;
    return true;
}

double AutoSetup::measuredLatencyMs() const
{
    auto* dev = dm.getCurrentAudioDevice();
    if (dev == nullptr) return 1e9;
    const double sr = std::max (1.0, dev->getCurrentSampleRate());
    int lat = dev->getInputLatencyInSamples() + dev->getOutputLatencyInSamples();
    if (lat <= 0) lat = 2 * dev->getCurrentBufferSizeSamples();
    return 1000.0 * (lat + proc.getLatencySamples()) / sr;
}

void AutoSetup::finishTuning()
{
    if (best.buffer > 0)
        openMode (best.type, best.buffer);
    else
        openCandidate (chosen); // nothing was perfect: keep the safe default buffer

    if (auto* dev = dm.getCurrentAudioDevice())
    {
        juce::AudioDeviceManager::AudioDeviceSetup s;
        dm.getAudioDeviceSetup (s);
        result.deviceType = dev->getTypeName();
        result.inputName = s.inputDeviceName;
        result.outputName = s.outputDeviceName;
        result.bufferSize = dev->getCurrentBufferSizeSamples();
        result.sampleRate = dev->getCurrentSampleRate();
        result.latencyMs = measuredLatencyMs();
        result.exclusive = result.deviceType.contains ("Exclusive");
    }
    detailText = {};
    pauseThen ("About " + juce::String (result.latencyMs, 1) + " ms from string to ear", [this] { beginPlaying(); });
}

void AutoSetup::beginPlaying()
{
    proc.tiltProbe().restart(); // the tone is judged on this playing, on the device as it will run
    proc.rawInputPeak.store (0.0f);
    playPeak = fixedPeak = 0.0f;
    playedMs = 0;
    fixSet = false;
    proc.setValue (P::inGain, 0.0f); // the level into the pedals is read as it comes, then set
    phaseStart = nowMs();
    detailText = {};
    setPhase (Phase::playing);
}

void AutoSetup::setFixFromTone()
{
    // Input Fix: measure the treble that actually arrives when there was enough playing
    // to judge, otherwise go by what kind of input this is.
    const bool onboard = looksLikeOnboardInput (result.inputName, result.deviceType);
    result.toneSeconds = proc.tiltProbe().secondsMeasured();
    result.tiltDb = proc.tiltProbe().tiltDb();
    if (result.toneSeconds >= 1.0)
    {
        // A mic or line jack loads the pickup and every bit of lost treble is worth having
        // back. Through an interface a darker reading is mostly a dark guitar or pickup,
        // which is part of its sound: only a clear loss is mended there.
        result.inputFixDb = TiltProbe::fixAmountFor (result.tiltDb, onboard ? 3.0f : 5.0f);
        if (! onboard && result.inputFixDb < 9.0f) result.inputFixDb = 0.0f;
    }
    else
        result.inputFixDb = onboard ? 12.0f : 0.0f;
    result.inputFix = result.inputFixDb > 0.0f;
    proc.setValue (P::inputFix, result.inputFix ? 1.0f : 0.0f);
    proc.setValue (P::inputFixAmt, result.inputFix ? result.inputFixDb : 6.0f);
    proc.setValue (P::humFilter, 1.0f); // Auto
    proc.meters().stagePeak.store (0.0f);
    fixSet = true;
}

void AutoSetup::finishPlaying()
{
    if (! fixSet) setFixFromTone();
    // The level is set from the hardest playing, and when the player did play for this
    // step, from this step alone: the first strums that got the input noticed are careful
    // ones, and a rig set by them runs far too hot once the playing is for real. The peak
    // that counts is the one after Input Fix, which is what reaches the pedals.
    result.playedSeconds = playedMs / 1000.0;
    float peak = std::max (playPeak, fixedPeak);
    if (playedMs < 1500.0) peak = std::max (peak, std::max (chosenPeak, strumPeak));
    result.guitarPeakDb = juce::Decibels::gainToDecibels (peak, -100.0f);
    result.clipping = playPeak > 0.97f;
    // aim the hardest peaks at about -12 dBFS into the rig
    float gain = juce::jlimit (-12.0f, 30.0f, InputGuard::kTargetDb - result.guitarPeakDb);
    gain = std::round (gain * 2.0f) * 0.5f;
    result.inputGainDb = gain;
    proc.setValue (P::inGain, gain);
    // from here the input guard may turn it down further, for as long as the player leaves it alone
    proc.uiState.setProperty ("guardGain", gain, nullptr);
    // these seconds of digging in are what every tone is levelled with from now on
    proc.levels().learnPhraseNow();

    pauseThen ("Input level set to " + juce::String (gain >= 0 ? "+" : "") + juce::String (gain, 1) + " dB", [this] { beginQuiet(); });
}

void AutoSetup::beginQuiet()
{
    noiseSamples.clearQuick();
    noiseStarted = false;
    fixRetried = false;
    phaseStart = quietSince = nowMs();
    detailText = {};
    setPhase (Phase::quiet);
}

void AutoSetup::startNoise()
{
    noiseStarted = true;
    noiseStart = nowMs();
    noiseSamples.clearQuick();
    if (onPhaseChanged) onPhaseChanged();
}

void AutoSetup::finish()
{
    // a high percentile ignores a single accidental bump
    std::vector<float> v (noiseSamples.begin(), noiseSamples.end());
    std::sort (v.begin(), v.end());
    const float noise = v.empty() ? 0.0f : v[(size_t) ((double) (v.size() - 1) * 0.9)];
    result.noiseDb = juce::Decibels::gainToDecibels (noise, -100.0f);

    // Input Fix brings the input's hiss up with the treble. When that leaves more hiss than
    // a gate can hide without cutting notes short, some of the fix is given back and the
    // noise is measured once more.
    if (result.inputFix && result.inputFixDb > 12.0f && result.noiseDb > kNoisyFixDb && ! fixRetried)
    {
        fixRetried = true;
        result.fixHeldBack = true;
        result.inputFixDb = std::max (12.0f, std::round ((result.inputFixDb - (result.noiseDb - kNoisyFixDb)) * 2.0f) * 0.5f);
        proc.setValue (P::inputFixAmt, result.inputFixDb);
        startNoise();
        return;
    }

    stopTimer();
    float gate = juce::jlimit (-90.0f, -42.0f, result.noiseDb + 6.0f);
    gate = std::round (gate);
    result.gateDb = gate;
    proc.setValue (P::gateThresh, gate);
    proc.rememberSetup(); // new copies of the plugin in a DAW start from this
    proc.muteOutput.store (false);
    setPhase (Phase::finished);
}
} // namespace rh
