// Riffhouse - free, open-source guitar rig
// Copyright (C) 2026 Riffhouse contributors. Licensed under AGPL-3.0-or-later.
#include "Params.h"

#include <cassert>

namespace rh
{
namespace
{
    ParamSpec tog (P id, const char* key, const char* name, bool def)
    {
        return { id, key, name, ParamKind::toggle, 0.0f, 1.0f, def ? 1.0f : 0.0f, 0.0f, "", {} };
    }
    ParamSpec cont (P id, const char* key, const char* name, float mn, float mx, float def, const char* unit = "", float skewCentre = 0.0f)
    {
        return { id, key, name, ParamKind::continuous, mn, mx, def, skewCentre, unit, {} };
    }
    ParamSpec knob (P id, const char* key, const char* name, float def)
    {
        return { id, key, name, ParamKind::continuous, 0.0f, 10.0f, def, 0.0f, "", {} };
    }
    ParamSpec pick (P id, const char* key, const char* name, std::vector<const char*> c, int def = 0)
    {
        return { id, key, name, ParamKind::choice, 0.0f, (float) (c.size() - 1), (float) def, 0.0f, "", std::move (c) };
    }

    std::vector<ParamSpec> build()
    {
        std::vector<ParamSpec> s;
        s.reserve (kNumParams);

        s.push_back (cont (P::inGain, "in_gain", "Input", -24.0f, 36.0f, 0.0f, "dB"));
        s.push_back (tog (P::inputFix, "input_fix", "Input Fix", false));
        s.push_back (pick (P::humFilter, "hum_filter", "Hum Filter", { "Off", "Auto", "50 Hz", "60 Hz" }, 1));
        s.push_back (tog (P::gateOn, "gate_on", "Noise Gate", true));
        s.push_back (cont (P::gateThresh, "gate_thresh", "Gate Threshold", -96.0f, -20.0f, -62.0f, "dB"));
        s.push_back (cont (P::gateRelease, "gate_release", "Gate Release", 10.0f, 500.0f, 80.0f, "ms", 80.0f));
        s.push_back (cont (P::inputFixAmt, "input_fix_amt", "Input Fix Strength", 0.0f, 12.0f, 6.0f, "dB"));

        s.push_back (tog (P::compOn, "comp_on", "Compressor", false));
        s.push_back (knob (P::compSustain, "comp_sustain", "Sustain", 5.0f));
        s.push_back (knob (P::compAttack, "comp_attack", "Attack", 4.0f));
        s.push_back (knob (P::compLevel, "comp_level", "Level", 5.0f));
        s.push_back (knob (P::compBlend, "comp_blend", "Blend", 10.0f));

        s.push_back (tog (P::wahOn, "wah_on", "Wah", false));
        s.push_back (pick (P::wahMode, "wah_mode", "Wah Mode", { "Manual", "Auto" }, 1));
        s.push_back (knob (P::wahPos, "wah_pos", "Pedal", 5.0f));
        s.push_back (knob (P::wahSens, "wah_sens", "Sensitivity", 6.0f));
        s.push_back (knob (P::wahQ, "wah_q", "Peak", 5.0f));

        s.push_back (tog (P::pitchOn, "pitch_on", "Pitch", false));
        s.push_back (pick (P::pitchMode, "pitch_mode", "Pitch Mode", { "Sub Octave", "Octave Up", "Shift" }));
        s.push_back (cont (P::pitchSemis, "pitch_semis", "Semitones", -12.0f, 12.0f, -12.0f, "st"));
        s.push_back (knob (P::pitchMix, "pitch_mix", "Mix", 5.0f));

        s.push_back (tog (P::drvOn, "drv_on", "Drive Pedal", false));
        s.push_back (pick (P::drvType, "drv_type", "Pedal Type",
                           { "Clean Boost", "Treble Boost", "Green Drive", "Blues Drive", "Rodent Dist", "Big Fuzz", "Metal Box" }, 2));
        s.push_back (knob (P::drvGain, "drv_gain", "Drive", 4.0f));
        s.push_back (knob (P::drvTone, "drv_tone", "Tone", 5.0f));
        s.push_back (knob (P::drvLevel, "drv_level", "Level", 5.0f));

        s.push_back (tog (P::ampOn, "amp_on", "Amp", true));
        s.push_back (pick (P::ampModel, "amp_model", "Amp Model",
                           { "Glass Clean", "Tweed Edge", "Chime Top", "Brit Plexi", "Brit 800", "Hot Rod Lead",
                             "Modern High Gain", "Tight Metal", "Bass Station", "Studio Direct", "Neural Capture" }, 3));
        s.push_back (knob (P::ampGain, "amp_gain", "Gain", 5.0f));
        s.push_back (knob (P::ampBass, "amp_bass", "Bass", 5.0f));
        s.push_back (knob (P::ampMid, "amp_mid", "Mid", 5.0f));
        s.push_back (knob (P::ampTreble, "amp_treble", "Treble", 5.0f));
        s.push_back (knob (P::ampPresence, "amp_presence", "Presence", 5.0f));
        s.push_back (knob (P::ampDepth, "amp_depth", "Depth", 5.0f));
        s.push_back (knob (P::ampMaster, "amp_master", "Master", 5.0f));
        s.push_back (cont (P::ampLevel, "amp_level", "Amp Level", -24.0f, 12.0f, 0.0f, "dB"));

        s.push_back (tog (P::cabOn, "cab_on", "Cabinet", true));
        s.push_back (pick (P::cabModel, "cab_model", "Cabinet",
                           { "1x12 Open Back", "2x12 Blue", "4x12 Vintage", "4x12 Modern", "2x12 American",
                             "1x8 Tiny Combo", "4x10 Bass", "1x15 Bass", "Your IR" }, 2));
        s.push_back (knob (P::cabMic, "cab_mic", "Mic Position", 3.0f));
        s.push_back (knob (P::cabDistance, "cab_distance", "Distance", 2.0f));
        s.push_back (cont (P::cabLowCut, "cab_lowcut", "Low Cut", 20.0f, 300.0f, 60.0f, "Hz", 80.0f));
        s.push_back (cont (P::cabHighCut, "cab_highcut", "High Cut", 2000.0f, 20000.0f, 12000.0f, "Hz", 7000.0f));

        s.push_back (tog (P::eqOn, "eq_on", "EQ", false));
        s.push_back (cont (P::eq100, "eq_100", "100", -12.0f, 12.0f, 0.0f, "dB"));
        s.push_back (cont (P::eq200, "eq_200", "200", -12.0f, 12.0f, 0.0f, "dB"));
        s.push_back (cont (P::eq400, "eq_400", "400", -12.0f, 12.0f, 0.0f, "dB"));
        s.push_back (cont (P::eq800, "eq_800", "800", -12.0f, 12.0f, 0.0f, "dB"));
        s.push_back (cont (P::eq1k6, "eq_1k6", "1.6k", -12.0f, 12.0f, 0.0f, "dB"));
        s.push_back (cont (P::eq3k2, "eq_3k2", "3.2k", -12.0f, 12.0f, 0.0f, "dB"));
        s.push_back (cont (P::eq6k4, "eq_6k4", "6.4k", -12.0f, 12.0f, 0.0f, "dB"));
        s.push_back (cont (P::eqLevel, "eq_level", "EQ Level", -12.0f, 12.0f, 0.0f, "dB"));

        s.push_back (tog (P::modOn, "mod_on", "Modulation", false));
        s.push_back (pick (P::modType, "mod_type", "Mod Type", { "Chorus", "Flanger", "Phaser", "Tremolo", "Vibrato", "Vibe" }));
        s.push_back (knob (P::modRate, "mod_rate", "Rate", 4.0f));
        s.push_back (knob (P::modDepth, "mod_depth", "Depth", 5.0f));
        s.push_back (knob (P::modMix, "mod_mix", "Mix", 5.0f));

        s.push_back (tog (P::dlyOn, "dly_on", "Delay", false));
        s.push_back (pick (P::dlyType, "dly_type", "Delay Type", { "Digital", "Analog", "Tape", "Ping Pong" }));
        s.push_back (cont (P::dlyTime, "dly_time", "Time", 20.0f, 2000.0f, 380.0f, "ms", 400.0f));
        s.push_back (pick (P::dlySync, "dly_sync", "Sync", { "Off", "1/2", "1/4 dotted", "1/4", "1/8 dotted", "1/8", "1/8 triplet", "1/16" }));
        s.push_back (knob (P::dlyFeedback, "dly_feedback", "Feedback", 3.5f));
        s.push_back (knob (P::dlyTone, "dly_tone", "Tone", 6.0f));
        s.push_back (knob (P::dlyMix, "dly_mix", "Mix", 2.5f));

        s.push_back (tog (P::revOn, "rev_on", "Reverb", true));
        s.push_back (pick (P::revType, "rev_type", "Reverb Type", { "Room", "Hall", "Plate", "Spring", "Shimmer", "Cloud" }));
        s.push_back (knob (P::revDecay, "rev_decay", "Decay", 4.0f));
        s.push_back (cont (P::revPreDelay, "rev_predelay", "Pre-delay", 0.0f, 200.0f, 12.0f, "ms", 40.0f));
        s.push_back (knob (P::revTone, "rev_tone", "Tone", 5.0f));
        s.push_back (knob (P::revMix, "rev_mix", "Mix", 1.8f));

        s.push_back (tog (P::lofiOn, "lofi_on", "Lo-Fi", false));
        s.push_back (knob (P::lofiBits, "lofi_bits", "Crush", 0.0f));
        s.push_back (knob (P::lofiRate, "lofi_rate", "Downsample", 0.0f));
        s.push_back (knob (P::lofiWobble, "lofi_wobble", "Wobble", 4.0f));
        s.push_back (knob (P::lofiNoise, "lofi_noise", "Noise", 3.0f));
        s.push_back (knob (P::lofiFilter, "lofi_filter", "Radio", 3.0f));
        s.push_back (knob (P::lofiMix, "lofi_mix", "Mix", 10.0f));

        s.push_back (knob (P::doubler, "doubler", "Doubler", 0.0f));
        s.push_back (cont (P::space, "space", "Space", 0.0f, 2.0f, 1.0f, "x"));
        s.push_back (cont (P::outLevel, "out_level", "Output", -36.0f, 12.0f, 0.0f, "dB"));

        assert ((int) s.size() == kNumParams);
        for (int i = 0; i < (int) s.size(); ++i)
            assert ((int) s[(size_t) i].id == i);
        return s;
    }
} // namespace

const std::vector<ParamSpec>& paramSpecs()
{
    static const std::vector<ParamSpec> specs = build();
    return specs;
}

const ParamSpec& spec (P id) { return paramSpecs()[(size_t) id]; }

int findParamByKey (std::string_view key)
{
    const auto& s = paramSpecs();
    for (size_t i = 0; i < s.size(); ++i)
        if (key == s[i].key)
            return (int) i;
    return -1;
}

ParamValues defaultParamValues()
{
    ParamValues v {};
    for (const auto& s : paramSpecs())
        v[(size_t) s.id] = s.def;
    return v;
}
} // namespace rh
