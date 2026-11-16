// Riffhouse - free, open-source guitar rig
// Copyright (C) 2026 Riffhouse contributors. Licensed under AGPL-3.0-or-later.
#include "NeuralCapture.h"

#include "NAM/activations.h"
#include "NAM/dsp.h"
#include "NAM/get_dsp.h"
#include "FactoryCaptures.h"

#include <cmath>
#include <cstdint>
#include <cstring>
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

NeuralCapture::NeuralCapture()
{
    // the fast tanh the official NAM plugin runs with: about a third of the CPU, and captures
    // sound as they do there (set once, before any model is built)
    static const bool fast = [] { nam::activations::Activation::enable_fast_tanh(); return true; }();
    (void) fast;
}

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

namespace
{
    // A model that counts its numbers (WaveNet, the usual kind) refuses a padded list, but only
    // after reading it, so the NaN probe below cannot judge it. Padded instead with a million
    // markers that differ from each other and from any real weight, its complaint ("assigned N
    // weights") names the first number it did not take: past the file's own count means it
    // wants more than the file has, and loading the file as it is would read past its end.
    bool wantsMoreThanItHas (nlohmann::json j, size_t have)
    {
        auto& w = j["weights"];
        w.erase (w.begin() + (std::ptrdiff_t) have, w.end());
        for (std::uint32_t k = 0; k < (1u << 20); ++k)
        {
            const std::uint32_t bits = 0x7E000000u + k; // huge, finite and all different
            float f;
            std::memcpy (&f, &bits, sizeof f);
            w.push_back (f);
        }
        try
        {
            nam::get_dsp (j);
            return false; // took every marker: it does not count, and the NaN probe has judged it
        }
        catch (const std::exception& e)
        {
            const std::string m = e.what();
            const auto at = m.find ("assigned ");
            if (at == std::string::npos) return true; // cannot tell: refuse rather than read past the end
            return std::stoull (m.substr (at + 9)) > have + 1;
        }
        catch (...) { return true; }
    }
}

// The NAM library reads a model's numbers one after another without looking where the list
// ends, so a cut-short file would be read past its end: a crash, or not, depending on what
// lies behind it in memory. Built first with a long run of NaNs behind its numbers, such a
// file reads some of them and its sound turns NaN; a whole file never reaches them.
bool NeuralCapture::isDamaged (const std::filesystem::path& file)
{
    std::ifstream in (file);
    auto j = nlohmann::json::parse (in, nullptr, false);
    if (j.is_discarded() || ! j.is_object() || ! j.contains ("weights") || ! j["weights"].is_array())
        return false; // not this kind of damage; the library says what is wrong with it
    auto& w = j["weights"];
    const size_t have = w.size();
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
    catch (const std::exception&) { return wantsMoreThanItHas (j, have); }
    catch (...) {}
    return false;
}

std::unique_ptr<NeuralCapture::Model> NeuralCapture::makeModel (const std::string& path, std::string& error)
{
    try
    {
        const std::filesystem::path fsPath (std::u8string (reinterpret_cast<const char8_t*> (path.data()), path.size()));
        if (isDamaged (fsPath))
        {
            error = "This capture file is damaged: it has fewer numbers than its model needs. Download it again.";
            return nullptr;
        }
        auto dsp = nam::get_dsp (fsPath);
        if (! dsp)
        {
            error = "This file could not be read as a Neural Amp Modeler capture.";
            return nullptr;
        }
        const auto stem = fsPath.stem().u8string();
        return finishModel (std::move (dsp), std::string (stem.begin(), stem.end()));
    }
    catch (const std::exception& e)
    {
        error = std::string ("Could not load capture: ") + e.what();
        return nullptr;
    }
}

std::unique_ptr<NeuralCapture::Model> NeuralCapture::finishModel (std::unique_ptr<nam::DSP> dsp, const std::string& modelName)
{
    auto m = std::make_unique<Model>();
    m->dsp = std::move (dsp);
    {
        m->modelRate = m->dsp->GetExpectedSampleRate() > 0 ? m->dsp->GetExpectedSampleRate() : 48000.0;
        // Captures carry their own loudness; bring them all to about -18 dB so
        // switching captures does not jump in level.
        if (m->dsp->HasLoudness())
            m->loudnessTrim = (float) std::pow (10.0, (-18.0 - m->dsp->GetLoudness()) / 20.0);
        const double runRate = m->dsp->SupportsArbitrarySampleRate() ? sr : m->modelRate;
        m->dsp->Reset (runRate, maxBlock * 4);
        m->name = modelName;
        return m;
    }
}

std::string NeuralCapture::loadFactory (int index)
{
    if (index < 0 || index >= factory_captures::count) return "No such built-in capture.";
    const auto& c = factory_captures::all[index];
    try
    {
        auto j = nlohmann::json::parse (c.config);
        auto& w = j["weights"] = nlohmann::json::array();
        for (unsigned long i = 0; i < c.count; ++i) w.push_back (c.weights[i]);
        auto dsp = nam::get_dsp (j);
        if (! dsp) return "The built-in capture could not be built.";
        auto m = finishModel (std::move (dsp), c.name);
        name = m->name;
        delete pending.exchange (m.release());
        return {};
    }
    catch (const std::exception& e)
    {
        return std::string ("Could not build the built-in capture: ") + e.what();
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
