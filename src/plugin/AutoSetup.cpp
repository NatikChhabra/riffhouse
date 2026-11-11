// Riffhouse - free, open-source guitar rig
// Copyright (C) 2026 Riffhouse contributors. Licensed under AGPL-3.0-or-later.
#include "AutoSetup.h"

namespace rh
{
namespace
{
    constexpr float kHeardThreshold = 0.0032f; // -50 dBFS: something is playing
    constexpr float kSureThreshold = 0.063f;   // -24 dBFS: that is definitely a guitar
    constexpr double kDwellMs = 1100.0;
    constexpr double kTuneMs = 1600.0;
    constexpr double kGetReadyMs = 1800.0, kQuietMs = 2600.0;

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
                    beginTuning();
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
                beginTuning();
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
                    beginTuning();
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

        case Phase::quiet:
        {
            const double elapsed = t - phaseStart;
            phaseProgress = (float) std::min (1.0, elapsed / (kGetReadyMs + kQuietMs));
            if (elapsed < kGetReadyMs) return;
            noiseSamples.add (proc.meters().gateInPeak.load());
            if (elapsed >= kGetReadyMs + kQuietMs) finish();
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
    beginQuiet();
}

void AutoSetup::beginQuiet()
{
    // The level is set from the hardest strum heard in the whole test, not just the first one
    // that got the input noticed: one soft opening strum would leave the rig running too hot.
    const float peak = std::max (chosenPeak, strumPeak);
    result.guitarPeakDb = juce::Decibels::gainToDecibels (peak, -100.0f);
    result.clipping = peak > 0.97f;
    // aim strummed peaks at about -12 dBFS into the rig
    float gain = juce::jlimit (-12.0f, 30.0f, -12.0f - result.guitarPeakDb);
    gain = std::round (gain * 2.0f) * 0.5f;
    result.inputGainDb = gain;
    proc.setValue (P::inGain, gain);
    noiseSamples.clearQuick();
    phaseStart = nowMs();
    detailText = {};
    setPhase (Phase::quiet);
}

void AutoSetup::finish()
{
    stopTimer();
    // a high percentile ignores a single accidental bump
    std::vector<float> v (noiseSamples.begin(), noiseSamples.end());
    std::sort (v.begin(), v.end());
    const float noise = v.empty() ? 0.0f : v[(size_t) ((double) (v.size() - 1) * 0.9)];
    result.noiseDb = juce::Decibels::gainToDecibels (noise, -100.0f);
    float gate = juce::jlimit (-90.0f, -42.0f, result.noiseDb + 6.0f);
    gate = std::round (gate);
    result.gateDb = gate;
    proc.setValue (P::gateThresh, gate);

    // Input Fix: measure the treble you actually have when there was enough
    // playing to judge, otherwise go by what kind of input this is.
    const bool onboard = looksLikeOnboardInput (result.inputName, result.deviceType);
    result.toneSeconds = proc.tiltProbe().secondsMeasured();
    result.tiltDb = proc.tiltProbe().tiltDb();
    if (result.toneSeconds >= 1.0)
    {
        result.inputFixDb = TiltProbe::fixAmountFor (result.tiltDb);
        // through an interface a slightly dark reading is just a dark guitar
        // or pickup, which is part of its sound: only fix clear treble loss
        if (! onboard && result.inputFixDb < 7.0f) result.inputFixDb = 0.0f;
        result.inputFix = result.inputFixDb > 0.0f;
    }
    else
    {
        result.inputFix = onboard;
        result.inputFixDb = 6.0f;
    }
    proc.setValue (P::inputFix, result.inputFix ? 1.0f : 0.0f);
    proc.setValue (P::inputFixAmt, result.inputFix ? result.inputFixDb : 6.0f);
    proc.setValue (P::humFilter, 1.0f); // Auto
    proc.muteOutput.store (false);
    setPhase (Phase::finished);
}
} // namespace rh
