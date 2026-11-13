// Riffhouse - free, open-source guitar rig
// Copyright (C) 2026 Riffhouse contributors. Licensed under AGPL-3.0-or-later.
#include "NeuralCapture.h"

#include "NAM/dsp.h"
#include "NAM/get_dsp.h"

#include <cmath>
#include <filesystem>
#include <fstream>
#include <limits>

namespace rh
{
struct NeuralCapture::Model
{
    std::unique_ptr<nam::DSP> dsp;
    double modelRate = 48000.0;
    float loudnessTrim = 1.0f;
    std::string name;
};

NeuralCapture::NeuralCapture() = default;

NeuralCapture::~NeuralCapture()
{
    delete active;
    delete pending.exchange (nullptr);
    delete retired.exchange (nullptr);
}

void NeuralCapture::prepare (double sampleRate, int block)
{
    sr = sampleRate;
    maxBlock = block;
    // a pending model is picked up on the next process call
    if (active != nullptr)
        active->dsp->Reset (active->dsp->SupportsArbitrarySampleRate() ? sr : active->modelRate, maxBlock * 4);
    setupResampling();
}

void NeuralCapture::setupResampling()
{
    const double modelRate = active ? active->modelRate : sr;
    const bool arbitrary = active && active->dsp->SupportsArbitrarySampleRate();
    resampling = active && ! arbitrary && std::abs (modelRate - sr) > 1.0;
    const int cap = (int) std::ceil (maxBlock * std::max (1.0, modelRate / sr)) + 64;
    modelIn.assign ((size_t) cap, 0.0f);
    modelOut.assign ((size_t) cap, 0.0f);
    fifo.assign ((size_t) (maxBlock * 4 + 256), 0.0f);
    if (resampling)
    {
        up.prepare (sr, modelRate);
        down.prepare (modelRate, sr);
        // prime the output fifo so we never run dry: fixed latency
        fifoCount = StreamResampler::kHalf + 4;
        std::fill (fifo.begin(), fifo.end(), 0.0f);
    }
}

// The NAM library reads a model's numbers one after another without looking where the list
// ends, so a cut-short file would be read past its end. Built first with a long run of NaNs
// behind its numbers, such a file reads some of them and its sound turns NaN; a whole file
// never reaches them.
bool NeuralCapture::isDamaged (const std::filesystem::path& file)
{
    std::ifstream in (file);
    auto j = nlohmann::json::parse (in, nullptr, false);
    if (j.is_discarded() || ! j.is_object() || ! j.contains ("weights") || ! j["weights"].is_array())
        return false; // not this kind of damage; the library says what is wrong with it
    auto& w = j["weights"];
    for (int k = 0; k < (1 << 20); ++k) w.push_back (std::numeric_limits<float>::quiet_NaN());
    try
    {
        auto probe = nam::get_dsp (j);
        if (! probe) return false;
        probe->Reset (48000.0, 256);
        std::vector<NAM_SAMPLE> x (256), y (256);
        for (int i = 0; i < 256; ++i) x[(size_t) i] = (NAM_SAMPLE) (0.1 * std::sin (0.05 * i));
        NAM_SAMPLE* xs[] = { x.data() };
        NAM_SAMPLE* ys[] = { y.data() };
        probe->process (xs, ys, 256);
        for (auto v : y)
            if (! std::isfinite ((double) v)) return true;
    }
    catch (...) {}
    return false;
}

std::unique_ptr<NeuralCapture::Model> NeuralCapture::makeModel (const std::string& path, std::string& error)
{
    try
    {
        auto m = std::make_unique<Model>();
        const std::filesystem::path fsPath (std::u8string (reinterpret_cast<const char8_t*> (path.data()), path.size()));
        if (isDamaged (fsPath))
        {
            error = "This capture file is damaged: it has fewer numbers than its model needs. Download it again.";
            return nullptr;
        }
        m->dsp = nam::get_dsp (fsPath);
        if (! m->dsp)
        {
            error = "This file could not be read as a Neural Amp Modeler capture.";
            return nullptr;
        }
        m->modelRate = m->dsp->GetExpectedSampleRate() > 0 ? m->dsp->GetExpectedSampleRate() : 48000.0;
        // Captures carry their own loudness; bring them all to about -18 dB so
        // switching captures does not jump in level.
        if (m->dsp->HasLoudness())
            m->loudnessTrim = (float) std::pow (10.0, (-18.0 - m->dsp->GetLoudness()) / 20.0);
        const double runRate = m->dsp->SupportsArbitrarySampleRate() ? sr : m->modelRate;
        m->dsp->Reset (runRate, maxBlock * 4);
        const auto stem = fsPath.stem().u8string();
        m->name = std::string (stem.begin(), stem.end());
        return m;
    }
    catch (const std::exception& e)
    {
        error = std::string ("Could not load capture: ") + e.what();
        return nullptr;
    }
}

std::string NeuralCapture::load (const std::string& path)
{
    std::string error;
    auto m = makeModel (path, error);
    if (! m) return error;
    name = m->name;
    delete pending.exchange (m.release());
    return {};
}

void NeuralCapture::unload()
{
    name.clear();
}

std::string NeuralCapture::modelName() const { return name; }

void NeuralCapture::collectGarbage() { delete retired.exchange (nullptr); }

void NeuralCapture::process (float* x, int n, float inputGainDb, float outputGainDb)
{
    if (Model* p = pending.exchange (nullptr))
    {
        Model* old = active;
        active = p;
        // hand the old model to the message thread to free
        if (old != nullptr)
            delete retired.exchange (old); // only reached if the UI never collected: rare
        setupResampling();
    }

    if (active == nullptr)
        return;

    const float inG = dbToGain (inputGainDb);
    for (int i = 0; i < n; ++i) x[i] *= inG;

    const float outG = dbToGain (outputGainDb) * active->loudnessTrim;

    if (! resampling)
    {
        float* in[] = { x };
        float* out[] = { modelOut.data() };
        int done = 0;
        while (done < n)
        {
            const int chunk = std::min (n - done, (int) modelOut.size());
            in[0] = x + done;
            active->dsp->process (in, out, chunk);
            for (int i = 0; i < chunk; ++i) x[done + i] = modelOut[(size_t) i] * outG;
            done += chunk;
        }
        return;
    }

    const int m = up.process (x, n, modelIn.data(), (int) modelIn.size());
    if (m > 0)
    {
        float* in[] = { modelIn.data() };
        float* out[] = { modelOut.data() };
        active->dsp->process (in, out, m);
        const int space = (int) fifo.size() - fifoCount;
        const int got = down.process (modelOut.data(), m, fifo.data() + fifoCount, space);
        fifoCount += got;
    }
    const int take = std::min (n, fifoCount);
    for (int i = 0; i < take; ++i) x[i] = fifo[(size_t) i] * outG;
    for (int i = take; i < n; ++i) x[i] = 0.0f;
    std::copy (fifo.begin() + take, fifo.begin() + fifoCount, fifo.begin());
    fifoCount -= take;
}
} // namespace rh
