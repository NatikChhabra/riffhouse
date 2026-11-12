// Riffhouse - free, open-source guitar rig
// Copyright (C) 2026 Riffhouse contributors. Licensed under AGPL-3.0-or-later.
#include "Export.h"

#include <lame.h>

namespace rh
{
juce::String exportExtension (ExportFormat f) { return f == ExportFormat::mp3 ? ".mp3" : ".flac"; }
juce::String exportName (ExportFormat f) { return f == ExportFormat::mp3 ? "MP3" : "FLAC"; }

juce::File exportFileFor (const juce::File& recordingsFolder, const juce::File& source, ExportFormat f)
{
    return recordingsFolder.getChildFile ("Copies to share (MP3 and FLAC)")
        .getChildFile (source.getFileNameWithoutExtension() + exportExtension (f));
}

namespace
{
    constexpr int block = 4096;

    // LAME takes text for its tags as Latin-1; anything else is left out rather than garbled
    juce::String latin1 (const juce::String& s)
    {
        for (auto c : s)
            if (c > 0xff) return {};
        return s;
    }

    juce::String writeMp3 (juce::AudioFormatReader& reader, juce::FileOutputStream& out, const juce::String& title,
                           std::atomic<float>* progress, const std::function<bool()>& shouldStop)
    {
        const int channels = reader.numChannels >= 2 ? 2 : 1;
        const int sr = juce::roundToInt (reader.sampleRate);
        auto* lame = lame_init();
        if (lame == nullptr) return "The MP3 encoder could not start.";
        struct Closer { lame_t l; ~Closer() { lame_close (l); } } closer { lame };

        lame_set_in_samplerate (lame, sr);
        lame_set_num_channels (lame, channels);
        // MP3 plays at 32, 44.1 or 48 kHz; anything else is converted to the nearest one above
        if (sr > 48000) lame_set_out_samplerate (lame, sr % 44100 == 0 ? 44100 : 48000);
        else if (sr < 32000) lame_set_out_samplerate (lame, 44100);
        lame_set_mode (lame, channels == 2 ? JOINT_STEREO : MONO);
        lame_set_VBR (lame, vbr_off);
        lame_set_brate (lame, 320);
        lame_set_quality (lame, 2); // the slowest settings that still make a difference
        lame_set_bWriteVbrTag (lame, 1); // the frame players read for the exact length and the gap at the start
        lame_set_write_id3tag_automatic (lame, 0);
        id3tag_init (lame);
        id3tag_add_v2 (lame);
        const auto t = latin1 (title);
        if (t.isNotEmpty()) id3tag_set_title (lame, t.toRawUTF8());
        id3tag_set_comment (lame, "Recorded with Riffhouse");
        if (lame_init_params (lame) < 0) return "The MP3 encoder does not take this sound's sample rate (" + juce::String (sr) + " Hz).";

        std::vector<unsigned char> mp3 ((size_t) (block * 5 / 4 + 7200));
        const auto put = [&out] (const unsigned char* data, size_t n) { return n == 0 || out.write (data, n); };
        {
            std::vector<unsigned char> tag (lame_get_id3v2_tag (lame, nullptr, 0));
            if (! tag.empty() && ! put (tag.data(), lame_get_id3v2_tag (lame, tag.data(), tag.size())))
                return "The disk is full or the folder cannot be written.";
        }
        const auto audioStart = out.getPosition();

        juce::AudioBuffer<float> buf ((int) reader.numChannels, block);
        const juce::int64 total = reader.lengthInSamples;
        for (juce::int64 done = 0; done < total;)
        {
            if (shouldStop && shouldStop()) return "stopped";
            const int n = (int) std::min<juce::int64> (block, total - done);
            if (! reader.read (&buf, 0, n, done, true, true)) return "The sound file could not be read to the end.";
            const float* l = buf.getReadPointer (0);
            const float* r = channels == 2 ? buf.getReadPointer (1) : l;
            const int bytes = lame_encode_buffer_ieee_float (lame, l, r, n, mp3.data(), (int) mp3.size());
            if (bytes < 0) return "The MP3 encoder stopped with an error (" + juce::String (bytes) + ").";
            if (! put (mp3.data(), (size_t) bytes)) return "The disk is full or the folder cannot be written.";
            done += n;
            if (progress != nullptr) progress->store ((float) ((double) done / (double) std::max<juce::int64> (1, total)));
        }
        const int last = lame_encode_flush (lame, mp3.data(), (int) mp3.size());
        if (last < 0 || ! put (mp3.data(), (size_t) last)) return "The disk is full or the folder cannot be written.";
        {
            std::vector<unsigned char> tag (lame_get_id3v1_tag (lame, nullptr, 0));
            if (! tag.empty() && ! put (tag.data(), lame_get_id3v1_tag (lame, tag.data(), tag.size())))
                return "The disk is full or the folder cannot be written.";
        }
        // the first frame was kept free for the length and the encoder's delay: fill it in
        std::vector<unsigned char> frame (lame_get_lametag_frame (lame, nullptr, 0));
        if (! frame.empty())
        {
            const auto size = lame_get_lametag_frame (lame, frame.data(), frame.size());
            const auto end = out.getPosition();
            if (! out.setPosition (audioStart) || ! put (frame.data(), size) || ! out.setPosition (end))
                return "The disk is full or the folder cannot be written.";
        }
        return {};
    }

    juce::String writeFlac (juce::AudioFormatReader& reader, std::unique_ptr<juce::FileOutputStream> stream,
                            std::atomic<float>* progress, const std::function<bool()>& shouldStop)
    {
        juce::FlacAudioFormat flac;
        const int channels = (int) std::min (2u, std::max (1u, reader.numChannels));
        std::unique_ptr<juce::OutputStream> os (std::move (stream));
        const auto options = juce::AudioFormatWriterOptions{}
                                 .withSampleRate (reader.sampleRate)
                                 .withNumChannels (channels)
                                 .withBitsPerSample (24)
                                 .withQualityOptionIndex (flac.getQualityOptions().size() - 1); // the smallest file; it plays the same
        auto writer = flac.createWriterFor (os, options);
        if (writer == nullptr) return "This sound cannot be written as FLAC (" + juce::String (juce::roundToInt (reader.sampleRate)) + " Hz).";
        juce::AudioBuffer<float> buf ((int) reader.numChannels, block);
        const juce::int64 total = reader.lengthInSamples;
        for (juce::int64 done = 0; done < total;)
        {
            if (shouldStop && shouldStop()) return "stopped";
            const int n = (int) std::min<juce::int64> (block, total - done);
            if (! reader.read (&buf, 0, n, done, true, true)) return "The sound file could not be read to the end.";
            const float* chans[2] = { buf.getReadPointer (0), buf.getReadPointer (channels == 2 ? 1 : 0) };
            if (! writer->writeFromFloatArrays (chans, channels, n)) return "The disk is full or the folder cannot be written.";
            done += n;
            if (progress != nullptr) progress->store ((float) ((double) done / (double) std::max<juce::int64> (1, total)));
        }
        writer.reset();
        return {};
    }
}

juce::String exportSound (const juce::File& source, const juce::File& out, ExportFormat format, const juce::String& title,
                          std::atomic<float>* progress, const std::function<bool()>& shouldStop)
{
    juce::AudioFormatManager formats;
    formats.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> reader (formats.createReaderFor (source));
    if (reader == nullptr) return source.getFileName() + " could not be read as a sound file.";
    if (reader->lengthInSamples <= 0 || reader->numChannels == 0) return source.getFileName() + " has no sound in it.";

    if (! out.getParentDirectory().createDirectory()) return "Could not make the folder " + out.getParentDirectory().getFileName() + ".";
    const auto temp = out.getSiblingFile (out.getFileName() + ".part");
    temp.deleteFile();
    juce::String err;
    {
        auto stream = std::make_unique<juce::FileOutputStream> (temp);
        if (! stream->openedOk()) return "Could not write to " + out.getParentDirectory().getFileName() + ".";
        err = format == ExportFormat::mp3 ? writeMp3 (*reader, *stream, title, progress, shouldStop)
                                          : writeFlac (*reader, std::move (stream), progress, shouldStop);
    }
    if (err.isNotEmpty()) { temp.deleteFile(); return err; }
    out.deleteFile();
    if (! temp.moveFileTo (out)) { temp.deleteFile(); return "Could not write " + out.getFileName() + "."; }
    if (progress != nullptr) progress->store (1.0f);
    return {};
}

ExportJob::ExportJob (juce::File s, juce::File o, ExportFormat f, juce::String t, std::function<void (juce::File, juce::String)> d)
    : juce::Thread ("Riffhouse export"), source (std::move (s)), out (std::move (o)), format (f), title (std::move (t)), onDone (std::move (d))
{
    startThread (juce::Thread::Priority::low);
}

ExportJob::~ExportJob()
{
    alive->store (false);
    stopThread (15000);
}

void ExportJob::run()
{
    const auto err = exportSound (source, out, format, title, &done01, [this] { return threadShouldExit(); });
    juce::MessageManager::callAsync ([cb = onDone, file = out, err, ok = alive] { if (ok->load() && cb) cb (err.isEmpty() ? file : juce::File(), err); });
}
} // namespace rh
