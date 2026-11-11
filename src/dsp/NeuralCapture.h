// Riffhouse - free, open-source guitar rig
// Copyright (C) 2026 Riffhouse contributors. Licensed under AGPL-3.0-or-later.
#pragma once

#include "Common.h"
#include "Resampler.h"

#include <atomic>
#include <memory>
#include <string>

namespace nam { class DSP; }

namespace rh
{
// Plays Neural Amp Modeler (.nam) captures. Loading happens off the audio
// thread; the audio thread swaps the new model in without blocking.
class NeuralCapture
{
public:
    NeuralCapture();
    ~NeuralCapture();

    void prepare (double sampleRate, int maxBlock);
    void process (float* x, int n, float inputGainDb, float outputGainDb);

    // Call from a non-audio thread. Returns an error message, or empty on success.
    std::string load (const std::string& path);
    void unload();
    bool hasModel() const noexcept { return active != nullptr || pending.load() != nullptr; }
    std::string modelName() const;
    int latencySamples() const noexcept { return resampling ? 2 * (StreamResampler::kHalf + 2) : 0; }

    // Frees models the audio thread has retired. Call from the message thread.
    void collectGarbage();

private:
    struct Model;
    std::unique_ptr<Model> makeModel (const std::string& path, std::string& error);
    void setupResampling();

    double sr = 48000.0;
    int maxBlock = 512;
    Model* active = nullptr;
    std::atomic<Model*> pending { nullptr };
    std::atomic<Model*> retired { nullptr };
    bool resampling = false;
    StreamResampler up, down;
    std::vector<float> modelIn, modelOut, fifo;
    int fifoCount = 0;
    std::string name;
};
} // namespace rh
