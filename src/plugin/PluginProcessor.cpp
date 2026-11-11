// Riffhouse - free, open-source guitar rig
// Copyright (C) 2026 Riffhouse contributors. Licensed under AGPL-3.0-or-later.
#include "PluginProcessor.h"
#include "../ui/Editor.h"

#if __has_include("PresetData.h")
 #include "PresetData.h"
 #define RIFFHOUSE_HAS_BINARY 1
#endif

namespace rh
{
juce::AudioProcessorValueTreeState::ParameterLayout RiffhouseProcessor::createLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    for (const auto& s : paramSpecs())
    {
        const juce::ParameterID id { s.key, 1 };
        switch (s.kind)
        {
            case ParamKind::toggle:
                layout.add (std::make_unique<juce::AudioParameterBool> (id, s.name, s.def >= 0.5f));
                break;
            case ParamKind::choice:
            {
                juce::StringArray c;
                for (auto* name : s.choices) c.add (name);
                layout.add (std::make_unique<juce::AudioParameterChoice> (id, s.name, c, (int) s.def));
                break;
            }
            case ParamKind::continuous:
            {
                juce::NormalisableRange<float> range (s.min, s.max);
                if (s.skewCentre > s.min && s.skewCentre < s.max)
                    range.setSkewForCentre (s.skewCentre);
                juce::String unit (s.unit);
                auto attrs = juce::AudioParameterFloatAttributes().withLabel (unit).withStringFromValueFunction (
                    [unit] (float v, int)
                    {
                        if (unit == "Hz") return v >= 1000.0f ? juce::String (v / 1000.0f, 1) + " kHz" : juce::String (juce::roundToInt (v)) + " Hz";
                        if (unit == "ms") return juce::String (juce::roundToInt (v)) + " ms";
                        // a real minus sign, as wide as the plus, so values do not jump sideways
                        const auto signedText = [] (const juce::String& magnitude, float value)
                        {
                            const juce::String minus (juce::CharPointer_UTF8 ("\xe2\x88\x92"));
                            return (value > 0 ? juce::String ("+") : (value < 0 ? minus : juce::String())) + magnitude;
                        };
                        if (unit == "dB") return signedText (juce::String (std::abs (v), 1), std::abs (v) < 0.05f ? 0.0f : v) + " dB";
                        if (unit == "st") return signedText (juce::String (std::abs (juce::roundToInt (v))), (float) juce::roundToInt (v));
                        if (unit == "x") return juce::String (juce::roundToInt (v * 100.0f)) + "%";
                        return juce::String (v, 1);
                    });
                layout.add (std::make_unique<juce::AudioParameterFloat> (id, s.name, range, s.def, attrs));
                break;
            }
        }
    }
    return layout;
}

RiffhouseProcessor::RiffhouseProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Guitar", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "Riffhouse", createLayout())
{
    for (const auto& s : paramSpecs())
    {
        params[(size_t) s.id] = apvts.getParameter (s.key);
        raw[(size_t) s.id] = apvts.getRawParameterValue (s.key);
        jassert (params[(size_t) s.id] != nullptr);
    }
    formats.registerBasicFormats();
    disk.startThread();
    recordingFolder = Recorder::defaultFolder();
    reloadPresets();
    const int start = library.indexOf ("First Light Clean");
    if (start >= 0) loadPreset (start);
}

RiffhouseProcessor::~RiffhouseProcessor()
{
    alive->store (false);
    loader.removeAllJobs (true, 10000);
    rec.stop();
    takePlayer.unload();
    jam.setSource (nullptr);
    disk.stopThread (2000);
}

juce::File RiffhouseProcessor::userFolder()
{
    return juce::File::getSpecialLocation (juce::File::userDocumentsDirectory).getChildFile ("Riffhouse");
}

bool RiffhouseProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto in = layouts.getMainInputChannelSet(), out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::stereo() && out != juce::AudioChannelSet::mono()) return false;
    return in == juce::AudioChannelSet::mono() || in == juce::AudioChannelSet::stereo();
}

void RiffhouseProcessor::prepareToPlay (double sr, int block)
{
    sampleRate = sr;
    blockSize = block;
    mono.assign ((size_t) std::max (block, 16), 0.0f);
    di.assign ((size_t) std::max (block, 16), 0.0f);
    ParamValues v;
    for (int i = 0; i < kNumParams; ++i) v[(size_t) i] = raw[(size_t) i]->load();
    rig.setParams (v);
    rig.prepare (sr, block);
    setLatencySamples (rig.latencySamples());
    tun.prepare (sr);
    // a song played faster than it was recorded asks for more than a block of it at a time
    const int songBlock = SongStretch::most (std::max (block, 16));
    jam.prepareToPlay (songBlock, sr);
    takePlayer.prepare (sr, block);
    {
        const juce::ScopedLock sl (jamLock);
        jamBuffer.setSize (2, songBlock);
        jamOut.setSize (2, std::max (block, 16));
        songStretch.prepare (sr);
    }
    if (songSection.active()) // the part in memory was made for the old sample rate
        setJamSection (songSection.startSeconds(), songSection.endSeconds());
    if (loopReady) // a loop recorded at another sample rate would play at the wrong speed: start clean
    {
        const juce::SpinLock::ScopedLockType sl (loopLock);
        loop.prepare (sr);
    }
}

void RiffhouseProcessor::loopPress()
{
    if (! loopReady)
    {
        // two minutes of stereo is a lot of memory to take from someone who never loops,
        // so it is taken on the first press
        const juce::SpinLock::ScopedLockType sl (loopLock);
        loop.prepare (sampleRate > 0.0 ? sampleRate : 48000.0);
        loopReady = true;
    }
    loop.press();
}

void RiffhouseProcessor::releaseResources() { jam.releaseResources(); takePlayer.release(); }

void RiffhouseProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    blockCounter.fetch_add (1, std::memory_order_relaxed);
    const int n = buffer.getNumSamples();
    const int numIn = getTotalNumInputChannels();
    if ((int) mono.size() < n) { buffer.clear(); return; } // host broke its promise about block size

    // Pick the input channel that actually has the guitar. Many mic jacks
    // deliver audio on only one side, some duplicate it on both.
    if (numIn >= 2)
    {
        const float* a = buffer.getReadPointer (0);
        const float* b = buffer.getReadPointer (1);
        float ea = 0, eb = 0;
        for (int i = 0; i < n; ++i) { ea += a[i] * a[i]; eb += b[i] * b[i]; }
        chanEnergy[0] = 0.95f * chanEnergy[0] + 0.05f * ea;
        chanEnergy[1] = 0.95f * chanEnergy[1] + 0.05f * eb;
        const float total = chanEnergy[0] + chanEnergy[1] + 1e-12f;
        const float target = chanEnergy[1] / total; // 0 = left only, 1 = right only
        float pick = channelPick.load();
        pick += 0.02f * (target - pick);
        channelPick.store (pick);
        // weights: favour the stronger side, sum when both are equal
        const float wl = juce::jlimit (0.0f, 1.0f, 2.0f * (1.0f - pick));
        const float wr = juce::jlimit (0.0f, 1.0f, 2.0f * pick);
        const float norm = 1.0f / std::max (1.0f, wl + wr);
        for (int i = 0; i < n; ++i) mono[(size_t) i] = (a[i] * wl + b[i] * wr) * norm;
    }
    else if (numIn == 1)
        std::copy (buffer.getReadPointer (0), buffer.getReadPointer (0) + n, mono.begin());
    else
        std::fill (mono.begin(), mono.begin() + n, 0.0f);

    // Re-amping a take: its clean guitar stands in for the live input.
    takePlayer.readClean (mono.data(), n);

    float pk = 0.0f;
    for (int i = 0; i < n; ++i) pk = std::max (pk, std::abs (mono[(size_t) i]));
    rawInputPeak.store (std::max (pk, rawInputPeak.load() * 0.9f));

    tun.push (mono.data(), n);
    std::copy (mono.begin(), mono.begin() + n, di.begin());

    ParamValues v;
    for (int i = 0; i < kNumParams; ++i) v[(size_t) i] = raw[(size_t) i]->load();
    rig.setParams (v);

    double bpm = metronomeBpm.load();
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
            if (auto hostBpm = pos->getBpm())
                if (wrapperType != wrapperType_Standalone) bpm = *hostBpm;
    rig.setTempo (bpm);

    if (buffer.getNumChannels() < 2)
    {
        // mono output: render stereo, fold down
        juce::HeapBlock<float> tmp ((size_t) n);
        rig.process (mono.data(), buffer.getWritePointer (0), tmp.get(), n);
        for (int i = 0; i < n; ++i) buffer.getWritePointer (0)[i] = 0.5f * (buffer.getReadPointer (0)[i] + tmp[i]);
        return;
    }

    float* l = buffer.getWritePointer (0);
    float* r = buffer.getWritePointer (1);
    rig.process (mono.data(), l, r, n);
    for (int ch = 2; ch < buffer.getNumChannels(); ++ch) buffer.clear (ch, 0, n);

    // the recording gets the guitar only: no click, no jam track
    if (countInRemaining.load() <= 0)
        rec.push (di.data(), l, r, n);

    if (tunerOpen.load() && tunerMute.load())
        buffer.clear();

    // Listening to a take as it was recorded: that is all you hear while it plays.
    takePlayer.readSound (l, r, n);

    // the looper hears the rig and plays its loop back on top, for your ears only
    {
        const juce::SpinLock::ScopedTryLockType sl (loopLock);
        if (sl.isLocked()) loop.process (l, r, n);
    }

    // jam track and metronome go to your ears only
    {
        const juce::ScopedTryLock sl (jamLock);
        if (sl.isLocked() && jam.isPlaying() && jamOut.getNumSamples() >= n && jamBuffer.getNumSamples() >= SongStretch::most (n))
        {
            // slower takes less of the song per block, faster takes more
            const int need = songStretch.wants (n);
            if (songSection.active())
                songSection.read (jamBuffer.getWritePointer (0), jamBuffer.getWritePointer (1), need);
            else if (need > 0)
            {
                juce::AudioSourceChannelInfo info (&jamBuffer, 0, need);
                jam.getNextAudioBlock (info);
            }
            songStretch.process (jamBuffer.getReadPointer (0), jamBuffer.getReadPointer (1), need,
                                 jamOut.getWritePointer (0), jamOut.getWritePointer (1), n);
            const float g = jamLevel.load();
            buffer.addFrom (0, 0, jamOut, 0, 0, n, g);
            buffer.addFrom (1, 0, jamOut, 1, 0, n, g);
        }
    }
    renderClick (buffer.getWritePointer (0), buffer.getWritePointer (1), n);
    if (muteOutput.load()) buffer.clear();
}

void RiffhouseProcessor::renderClick (float* l, float* r, int n)
{
    const bool counting = countInRemaining.load() > 0;
    if (! metronomeOn.load() && ! counting) { beatCounter = 0; beatIndex = 0; return; }
    samplesPerBeat = sampleRate * 60.0 / juce::jlimit (30.0, 300.0, (double) metronomeBpm.load());
    const float level = counting ? std::max (0.5f, metronomeLevel.load()) : metronomeLevel.load();
    for (int i = 0; i < n; ++i)
    {
        if (beatCounter <= 0.0)
        {
            beatCounter += samplesPerBeat;
            clickEnv = 1.0f;
            clickFreq = (beatIndex % 4 == 0) ? 2000.0f : 1400.0f;
            clickPhase = 0.0;
            ++beatIndex;
            if (counting)
            {
                // count-in: one bar of four, then recording starts
                if (countInRemaining.fetch_sub (1) == 1 && startAfterCountIn.exchange (false))
                    triggerAsyncUpdate();
            }
        }
        beatCounter -= 1.0;
        if (clickEnv > 0.0005f)
        {
            const float s = (float) std::sin (clickPhase) * clickEnv * 0.25f * level;
            clickPhase += juce::MathConstants<double>::twoPi * clickFreq / sampleRate;
            clickEnv *= 0.9985f;
            l[i] += s;
            r[i] += s;
        }
    }
}

void RiffhouseProcessor::handleAsyncUpdate()
{
    // count-in finished: start writing
    rec.start (recordingFolder, sampleRate, saveDi.load(), currentPresetName());
}

bool RiffhouseProcessor::startRecording (bool countIn)
{
    if (countIn)
    {
        beatCounter = 0;
        beatIndex = 0;
        startAfterCountIn.store (true);
        countInRemaining.store (4);
        return true;
    }
    return rec.start (recordingFolder, sampleRate, saveDi.load(), currentPresetName());
}

void RiffhouseProcessor::stopRecording()
{
    startAfterCountIn.store (false);
    countInRemaining.store (0);
    rec.stop();
}

void RiffhouseProcessor::setValue (P id, float v)
{
    if (auto* p = param (id))
        p->setValueNotifyingHost (p->convertTo0to1 (v));
}

//==============================================================================
void RiffhouseProcessor::reloadPresets()
{
    const juce::String current = currentPresetName();
    library.clear();
   #if RIFFHOUSE_HAS_BINARY
    for (int i = 0; i < RiffhousePresets::namedResourceListSize; ++i)
    {
        int size = 0;
        if (auto* data = RiffhousePresets::getNamedResource (RiffhousePresets::namedResourceList[i], size))
            library.addFactory (data, size);
    }
   #endif
    library.scanFolder (userFolder().getChildFile ("Presets"), false);
    library.scanFolder (userFolder().getChildFile ("Packs"), false);

    juce::StringArray favs;
    favs.addLines (userFolder().getChildFile ("favourites.txt").loadFileAsString());
    for (auto& p : const_cast<std::vector<Preset>&> (library.all()))
        p.favourite = favs.contains (p.name);
    currentPreset = library.indexOf (current);
}

void RiffhouseProcessor::toggleFavourite (int index)
{
    auto& all = const_cast<std::vector<Preset>&> (library.all());
    if (index < 0 || index >= (int) all.size()) return;
    all[(size_t) index].favourite = ! all[(size_t) index].favourite;
    juce::StringArray favs;
    for (const auto& p : all) if (p.favourite) favs.add (p.name);
    userFolder().createDirectory();
    userFolder().getChildFile ("favourites.txt").replaceWithText (favs.joinIntoString ("\n"));
}

void RiffhouseProcessor::applyPreset (const Preset& p)
{
    // Input-side settings belong to your setup, not to the tone, so presets
    // never touch them (like a "global lock").
    for (const auto& s : paramSpecs())
        if (! belongsToSetup (s.id))
            setValue (s.id, p.values[(size_t) s.id]);
    // high-gain presets need the gate: keep the user's calibrated threshold,
    // but switch the gate on if the preset asks for it
}

void RiffhouseProcessor::loadPreset (int index)
{
    if (index < 0 || index >= (int) library.all().size()) return;
    applyPreset (library.all()[(size_t) index]);
    currentPreset = index;
    externalName.clear();
    for (int i = 0; i < kNumParams; ++i) presetSnapshot[(size_t) i] = raw[(size_t) i]->load();
}

juce::String RiffhouseProcessor::currentPresetName() const
{
    if (currentPreset >= 0 && currentPreset < (int) library.all().size())
        return library.all()[(size_t) currentPreset].name;
    return externalName.isNotEmpty() ? externalName : juce::String ("Custom Tone");
}

void RiffhouseProcessor::applySharedPreset (const Preset& p)
{
    applyPreset (p);
    currentPreset = -1;
    externalName = p.name;
    for (int i = 0; i < kNumParams; ++i) presetSnapshot[(size_t) i] = raw[(size_t) i]->load();
}

//==============================================================================
ToneState RiffhouseProcessor::toneNow() const
{
    ToneState t;
    for (int i = 0; i < kNumParams; ++i) t.values[(size_t) i] = raw[(size_t) i]->load();
    t.snapshot = presetSnapshot;
    if (currentPreset >= 0 && currentPreset < (int) library.all().size())
        t.libraryName = library.all()[(size_t) currentPreset].name;
    t.external = externalName;
    return t;
}

juce::String RiffhouseProcessor::restoreTone (const ToneState& to)
{
    // say what is being put back before it happens: the tone's name, the one control, or how many
    const auto from = toneNow();
    juce::String what;
    int changed = 0;
    const ParamSpec* one = nullptr;
    for (const auto& s : paramSpecs())
        if (! belongsToSetup (s.id) && std::abs (from.values[(size_t) s.id] - to.values[(size_t) s.id]) > 1.0e-4f * std::max (1.0f, std::abs (s.max - s.min)))
        {
            ++changed;
            one = &s;
        }
    for (const auto& s : paramSpecs())
        if (! belongsToSetup (s.id))
            setValue (s.id, to.values[(size_t) s.id]);
    currentPreset = to.libraryName.isNotEmpty() ? library.indexOf (to.libraryName) : -1;
    externalName = currentPreset >= 0 ? juce::String() : (to.external.isNotEmpty() ? to.external : to.libraryName);
    presetSnapshot = to.snapshot;
    if (from.libraryName != to.libraryName || from.external != to.external)
        what = currentPresetName();
    else if (changed == 1 && one != nullptr)
        what = juce::String (one->name) + " " + param (one->id)->getCurrentValueAsText();
    else
        what = juce::String (changed) + " controls";
    return what;
}

juce::String RiffhouseProcessor::undo()
{
    ToneState to;
    if (! history.undo (toneNow(), to)) return {};
    const auto what = restoreTone (to);
    history.settle (toneNow());
    return what;
}

juce::String RiffhouseProcessor::redo()
{
    ToneState to;
    if (! history.redo (toneNow(), to)) return {};
    const auto what = restoreTone (to);
    history.settle (toneNow());
    return what;
}

bool RiffhouseProcessor::isModified() const
{
    if (currentPreset < 0 && externalName.isEmpty()) return true;
    for (int i = 0; i < kNumParams; ++i)
        if (std::abs (presetSnapshot[(size_t) i] - raw[(size_t) i]->load()) > 1e-4f) return true;
    return false;
}

Preset RiffhouseProcessor::captureCurrent (const juce::String& name, const juce::String& category) const
{
    Preset p;
    p.name = name;
    p.category = category;
    for (int i = 0; i < kNumParams; ++i) p.values[(size_t) i] = raw[(size_t) i]->load();
    return p;
}

juce::File RiffhouseProcessor::savePreset (const juce::String& name, const juce::String& category)
{
    auto p = captureCurrent (name, category);
    p.author = juce::SystemStats::getFullUserName();
    auto folder = userFolder().getChildFile ("Presets");
    folder.createDirectory();
    auto f = folder.getChildFile (juce::File::createLegalFileName (name) + ".json");
    f.replaceWithText (presetToJson (p));
    reloadPresets();
    currentPreset = library.indexOf (name);
    for (int i = 0; i < kNumParams; ++i) presetSnapshot[(size_t) i] = raw[(size_t) i]->load();
    return f;
}

//==============================================================================
void RiffhouseProcessor::loadCaptureAsync (const juce::File& file, std::function<void (juce::String)> done)
{
    // Loading a capture can take a moment, so it happens on a worker thread.
    // The processor waits for the worker in its destructor, and the result is
    // only applied if the processor is still alive.
    auto aliveFlag = alive;
    loader.addJob ([this, file, done, aliveFlag]
    {
        const auto err = rig.neural().load (file.getFullPathName().toStdString());
        juce::MessageManager::callAsync ([this, file, err, done, aliveFlag]
        {
            if (! aliveFlag->load()) return;
            if (err.empty())
            {
                captureFile = file;
                setValue (P::ampModel, (float) choices::neuralCapture);
                setValue (P::ampOn, 1.0f);
            }
            rig.neural().collectGarbage();
            if (done) done (juce::String (err));
        });
    });
}

bool RiffhouseProcessor::loadUserIr (const juce::File& file)
{
    if (! rig.cabinet().loadUserIr (file)) return false;
    irFile = file;
    setValue (P::cabModel, (float) choices::cabUserIr);
    setValue (P::cabOn, 1.0f);
    return true;
}

ReampJob::Setup RiffhouseProcessor::reampSetup() const
{
    ReampJob::Setup s;
    for (int i = 0; i < kNumParams; ++i) s.params[(size_t) i] = raw[(size_t) i]->load();
    s.capture = captureFile;
    s.userIr = irFile;
    s.tempo = metronomeBpm.load();
    s.toneName = currentPresetName();
    return s;
}

//==============================================================================
bool RiffhouseProcessor::loadJamTrack (const juce::File& f)
{
    std::unique_ptr<juce::AudioFormatReader> reader (formats.createReaderFor (f));
    if (reader == nullptr) return false;
    const double rate = reader->sampleRate;
    auto src = std::make_unique<juce::AudioFormatReaderSource> (reader.release(), true);
    src->setLooping (true);
    const juce::ScopedLock sl (jamLock);
    jam.stop();
    jam.setSource (src.get(), 32768, &disk, rate);
    jamReader = std::move (src);
    jamFile = f;
    // a new song starts as it was recorded: the speed, key and part belonged to the last one
    songSection.clear();
    songStretch.setSpeed (1.0f);
    songStretch.setKey (0);
    songStretch.restart();
    return true;
}

double RiffhouseProcessor::audioFileSeconds (const juce::File& f)
{
    std::unique_ptr<juce::AudioFormatReader> reader (formats.createReaderFor (f));
    if (reader == nullptr || reader->sampleRate <= 0.0) return 0.0;
    return (double) reader->lengthInSamples / reader->sampleRate;
}

void RiffhouseProcessor::setJamPlaying (bool shouldPlay)
{
    const juce::ScopedLock sl (jamLock);
    if (shouldPlay) { songStretch.restart(); jam.start(); } else jam.stop();
}

bool RiffhouseProcessor::isJamPlaying() const { return jam.isPlaying(); }
double RiffhouseProcessor::jamPosition() const { return songSection.active() ? songSection.positionSeconds() : jam.getCurrentPosition(); }
double RiffhouseProcessor::jamLength() const { return jam.getLengthInSeconds(); }
void RiffhouseProcessor::setJamPosition (double s)
{
    const juce::ScopedLock sl (jamLock);
    if (songSection.active()) songSection.seek (s); // inside the part: a part going round stays a part
    else jam.setPosition (s);
    songStretch.restart();
}

void RiffhouseProcessor::setJamSpeed (float timesNormal) { songStretch.setSpeed (timesNormal); }
void RiffhouseProcessor::setJamKey (int semitones) { songStretch.setKey (semitones); }

bool RiffhouseProcessor::setJamSection (double from, double to)
{
    const double length = jamLength();
    if (jamFile == juce::File() || sampleRate <= 0.0 || length <= 0.0) return false;
    from = juce::jlimit (0.0, length, from);
    to = juce::jlimit (0.0, std::min (length, from + SongSection::longest), to);
    if (to - from < SongSection::shortest) return false;

    // The part is read into memory at the sample rate the device runs at, through the same
    // resampler the whole song plays through, so going into the part changes nothing you can hear.
    std::unique_ptr<juce::AudioFormatReader> reader (formats.createReaderFor (jamFile));
    if (reader == nullptr || reader->sampleRate <= 0.0) return false;
    const double fileRate = reader->sampleRate;
    juce::AudioFormatReaderSource file (reader.release(), true);
    file.setNextReadPosition ((juce::int64) (from * fileRate));
    juce::ResamplingAudioSource atDeviceRate (&file, false, 2);
    atDeviceRate.setResamplingRatio (fileRate / sampleRate);
    atDeviceRate.prepareToPlay (4096, sampleRate);
    const int total = (int) std::llround ((to - from) * sampleRate);
    std::vector<float> l ((size_t) total), r ((size_t) total);
    {
        float* channels[2] = { l.data(), r.data() };
        juce::AudioBuffer<float> into (channels, 2, total);
        for (int done = 0; done < total;)
        {
            const int count = std::min (4096, total - done);
            juce::AudioSourceChannelInfo info (&into, done, count);
            atDeviceRate.getNextAudioBlock (info);
            done += count;
        }
    }
    {
        const juce::ScopedLock sl (jamLock);
        songSection.swapIn (l, r, from, sampleRate);
        songStretch.restart();
    }
    return true; // the part that was going round before, if any, is freed here, outside the lock
}

void RiffhouseProcessor::clearJamSection()
{
    const juce::ScopedLock sl (jamLock);
    if (! songSection.active()) return;
    jam.setPosition (songSection.positionSeconds()); // the song carries on from where the part was
    songSection.clear();
    songStretch.restart();
}

//==============================================================================
void RiffhouseProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto st = apvts.copyState();
    st.setProperty ("preset", currentPresetName(), nullptr);
    st.setProperty ("capture", captureFile.getFullPathName(), nullptr);
    st.setProperty ("ir", irFile.getFullPathName(), nullptr);
    st.setProperty ("metronomeBpm", metronomeBpm.load(), nullptr);
    st.setProperty ("saveDi", saveDi.load(), nullptr);
    st.setProperty ("recordingFolder", recordingFolder.getFullPathName(), nullptr);
    st.removeChild (st.getChildWithName ("UI"), nullptr);
    st.appendChild (uiState.createCopy(), nullptr);
    if (auto xml = st.createXml())
        copyXmlToBinary (*xml, destData);
}

void RiffhouseProcessor::setStateInformation (const void* data, int size)
{
    auto xml = getXmlFromBinary (data, size);
    if (xml == nullptr) return;
    auto st = juce::ValueTree::fromXml (*xml);
    if (! st.isValid()) return;
    if (auto ui = st.getChildWithName ("UI"); ui.isValid()) uiState.copyPropertiesAndChildrenFrom (ui, nullptr);
    apvts.replaceState (st);
    currentPreset = library.indexOf (st.getProperty ("preset").toString());
    for (int i = 0; i < kNumParams; ++i) presetSnapshot[(size_t) i] = raw[(size_t) i]->load();
    metronomeBpm.store ((float) st.getProperty ("metronomeBpm", 100.0));
    saveDi.store ((bool) st.getProperty ("saveDi", true));
    const juce::String folder = st.getProperty ("recordingFolder").toString();
    if (folder.isNotEmpty()) recordingFolder = juce::File (folder);
    const juce::File cap (st.getProperty ("capture").toString());
    if (cap.existsAsFile()) loadCaptureAsync (cap, {});
    const juce::File ir (st.getProperty ("ir").toString());
    if (ir.existsAsFile()) { rig.cabinet().loadUserIr (ir); irFile = ir; }
}

juce::AudioProcessorEditor* RiffhouseProcessor::createEditor() { return new RiffhouseEditor (*this); }
} // namespace rh

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new rh::RiffhouseProcessor(); }
