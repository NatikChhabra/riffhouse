// Riffhouse - free, open-source guitar rig
// Copyright (C) 2026 Riffhouse contributors. Licensed under AGPL-3.0-or-later.
#include "FootControl.h"

#include <algorithm>
#include <charconv>
#include <utility>

namespace rh
{
namespace
{
struct JobWord { FootJob job; const char* key; const char* name; };
const JobWord jobWords[] = {
    { FootJob::nextTone, "next", "Next tone" },
    { FootJob::previousTone, "previous", "Previous tone" },
    { FootJob::looper, "looper", "Looper button" },
    { FootJob::looperStop, "looper-stop", "Looper stop" },
    { FootJob::tapTempo, "tap", "Tap tempo" },
    { FootJob::tuner, "tuner", "Tuner" },
    { FootJob::record, "record", "Record" },
    { FootJob::song, "song", "Song play and pause" },
};

// A footswitch that springs back sends its release this soon after its press.
constexpr double springBack = 0.6;
}

bool FootTarget::isSwitch() const noexcept
{
    if (job != FootJob::param) return true;
    return param >= 0 && param < kNumParams && spec ((P) param).kind == ParamKind::toggle;
}

FootControl::FootControl() { backToStart(); }

void FootControl::backToStart()
{
    list.clear();
    list.push_back ({ 11, { FootJob::param, (int) P::wahPos }, false }); // controller 11 is "expression" on every floor board
    last.fill (-1);
    learningNow = false;
    freshController = -1;
}

int FootControl::controllerFor (FootTarget t) const noexcept
{
    for (auto& b : list)
        if (b.target == t) return b.controller;
    return -1;
}

void FootControl::remove (size_t index)
{
    if (index < list.size()) list.erase (list.begin() + (std::ptrdiff_t) index);
}

void FootControl::setPress (size_t index, bool press)
{
    if (index < list.size() && list[index].target.isSwitch()) list[index].press = press;
}

bool FootControl::moved (int controller, int value, double now, Action& out, bool& learned)
{
    learned = false;
    if (controller < 0 || controller > 127) return false;
    value = std::clamp (value, 0, 127);
    const int before = last[(size_t) controller];
    last[(size_t) controller] = value;
    const bool down = value >= 64, wasDown = before >= 64;

    if (learningNow)
    {
        // one controller does one thing, and one thing listens to one controller
        list.erase (std::remove_if (list.begin(), list.end(), [&] (const FootBinding& b) { return b.controller == controller || b.target == wanted; }), list.end());
        list.push_back ({ controller, wanted, false });
        learningNow = false;
        learned = true;
        freshController = wanted.isSwitch() && down ? controller : -1;
        freshAt = now;
        out.target = wanted;
        if (wanted.job != FootJob::param) { out.value = 1.0f; out.flip = true; return true; } // the press that taught it also does it
        out.value = wanted.isSwitch() ? (down ? 1.0f : 0.0f) : (float) value / 127.0f;
        out.flip = false;
        return true;
    }

    for (auto& b : list)
    {
        if (b.controller != controller) continue;
        if (controller == freshController)
        {
            // The first thing a freshly taught switch does tells its kind: a release right after
            // the press means it springs back, so from now on every press flips.
            freshController = -1;
            if (! down && now - freshAt < springBack) { b.press = true; found = true; return false; }
        }
        out.target = b.target;
        if (! b.target.isSwitch())
        {
            out.value = (float) value / 127.0f;
            out.flip = false;
            return true;
        }
        if (b.press)
        {
            if (! down || (wasDown && before >= 0)) return false; // only the press counts, not the release or a repeat
            out.value = 1.0f;
            out.flip = true;
            return true;
        }
        // a switch that stays where it is put: on is on and off is off; for a job, every change is a press
        if (before >= 0 && down == wasDown) return false;
        out.value = down ? 1.0f : 0.0f;
        out.flip = b.target.job != FootJob::param;
        return true;
    }
    return false;
}

std::string footTargetName (FootTarget t)
{
    if (t.job == FootJob::param)
    {
        if (t.param < 0 || t.param >= kNumParams) return {};
        const auto& sp = spec ((P) t.param);
        const std::string name = sp.name;
        if (sp.kind == ParamKind::toggle) return name + " on and off";
        // "Mix" and "Level" are on half the pedals: say whose
        static const std::pair<const char*, const char*> owners[] = {
            { "comp_", "Compressor" }, { "wah_", "Wah" }, { "pitch_", "Pitch" }, { "drv_", "Drive" }, { "amp_", "Amp" },
            { "cab_", "Cabinet" }, { "eq_", "EQ" }, { "mod_", "Modulation" }, { "dly_", "Delay" }, { "rev_", "Reverb" }, { "lofi_", "Lo-fi" } };
        const std::string_view key = sp.key;
        for (auto& [prefix, owner] : owners)
            if (key.substr (0, std::string_view (prefix).size()) == prefix)
                return name.rfind (owner, 0) == 0 ? name : std::string (owner) + ": " + name;
        return name;
    }
    for (auto& w : jobWords)
        if (w.job == t.job) return w.name;
    return {};
}

std::string FootControl::toText() const
{
    // "11=wah_pos 64=looper! ..." : controller, what it does, and ! for a switch that springs back
    std::string s;
    for (auto& b : list)
    {
        std::string what;
        if (b.target.job == FootJob::param)
        {
            if (b.target.param < 0 || b.target.param >= kNumParams) continue;
            what = std::string ("p:") + spec ((P) b.target.param).key;
        }
        else
            for (auto& w : jobWords)
                if (w.job == b.target.job) what = w.key;
        if (what.empty()) continue;
        if (! s.empty()) s += ' ';
        s += std::to_string (b.controller) + "=" + what + (b.press ? "!" : "");
    }
    return s.empty() ? "none" : s;
}

void FootControl::fromText (std::string_view text)
{
    std::vector<FootBinding> read;
    size_t at = 0;
    while (at < text.size())
    {
        const size_t end = std::min (text.find (' ', at), text.size());
        std::string_view item = text.substr (at, end - at);
        at = end + 1;
        const size_t eq = item.find ('=');
        if (eq == std::string_view::npos) continue;
        int controller = -1;
        const auto number = item.substr (0, eq);
        if (std::from_chars (number.data(), number.data() + number.size(), controller).ec != std::errc() || controller < 0 || controller > 127) continue;
        std::string_view what = item.substr (eq + 1);
        FootBinding b;
        b.controller = controller;
        if (! what.empty() && what.back() == '!') { b.press = true; what.remove_suffix (1); }
        bool known = false;
        if (what.substr (0, 2) == "p:")
        {
            const int p = findParamByKey (what.substr (2));
            if (p >= 0) { b.target = { FootJob::param, p }; known = true; }
        }
        else
            for (auto& w : jobWords)
                if (what == w.key) { b.target = { w.job, 0 }; known = true; }
        if (! known) continue;
        if (! b.target.isSwitch()) b.press = false;
        // a file that names a controller or a target twice: the later line wins
        read.erase (std::remove_if (read.begin(), read.end(), [&] (const FootBinding& o) { return o.controller == b.controller || o.target == b.target; }), read.end());
        read.push_back (b);
    }
    if (read.empty() && text != "none") return; // nothing readable: keep what there is
    list = std::move (read);
    last.fill (-1);
    freshController = -1;
}
}
