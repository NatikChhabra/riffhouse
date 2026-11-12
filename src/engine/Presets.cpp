// Riffhouse - free, open-source guitar rig
// Copyright (C) 2026 Riffhouse contributors. Licensed under AGPL-3.0-or-later.
#include "Presets.h"

namespace rh
{
juce::String Preset::searchText() const
{
    return (name + " " + category + " " + inspiredBy + " " + band + " " + song + " " + description + " "
            + genres.joinIntoString (" ") + " " + tags.joinIntoString (" ")).toLowerCase();
}

juce::String Preset::title() const
{
    if (inspiredBy.isNotEmpty() && name.startsWith ("Inspired by ") && name.contains (": "))
        return name.fromFirstOccurrenceOf (": ", false, false);
    return name;
}

juce::String Preset::part() const
{
    const auto whole = title();
    if (song.isEmpty() || ! whole.startsWithIgnoreCase (song)) return whole;
    return whole.substring (song.length()).trim();
}

juce::String Preset::credit() const
{
    // the band stands in front of the title already: what is left to say is the player, when that is
    // someone else, and the song, when the title does not start with it
    if (inspiredBy.isEmpty()) return {};
    const auto who = inspiredBy != artist() ? inspiredBy : juce::String();
    const bool namesSong = song.isNotEmpty() && ! title().startsWithIgnoreCase (song);
    const auto quoted = juce::String (juce::CharPointer_UTF8 ("\xe2\x80\x9c")) + song + juce::String (juce::CharPointer_UTF8 ("\xe2\x80\x9d"));
    if (who.isNotEmpty()) return "Inspired by " + who + (namesSong ? " on " + quoted + ". " : juce::String (". "));
    return namesSong ? "After " + quoted + ". " : juce::String();
}

bool presetFromJson (const juce::String& json, Preset& out, juce::String& error)
{
    juce::var root;
    const auto result = juce::JSON::parse (json, root);
    if (result.failed() || ! root.isObject())
    {
        error = "Not valid preset JSON: " + result.getErrorMessage();
        return false;
    }
    Preset p;
    p.values = defaultParamValues();
    p.name = root.getProperty ("name", "Untitled").toString();
    p.category = root.getProperty ("category", "User").toString();
    p.inspiredBy = root.getProperty ("inspiredBy", "").toString();
    p.band = root.getProperty ("band", "").toString();
    p.song = root.getProperty ("song", "").toString();
    p.description = root.getProperty ("description", "").toString();
    p.author = root.getProperty ("author", "").toString();
    if (auto* g = root.getProperty ("genres", {}).getArray())
        for (auto& v : *g) p.genres.add (v.toString());
    if (auto* t = root.getProperty ("tags", {}).getArray())
        for (auto& v : *t) p.tags.add (v.toString());

    if (auto* obj = root.getProperty ("params", {}).getDynamicObject())
    {
        for (auto& prop : obj->getProperties())
        {
            const int idx = findParamByKey (prop.name.toString().toStdString());
            if (idx < 0)
            {
                error = "Unknown parameter \"" + prop.name.toString() + "\" in preset " + p.name;
                return false;
            }
            const auto& s = paramSpecs()[(size_t) idx];
            float value = 0.0f;
            if (s.kind == ParamKind::choice && prop.value.isString())
            {
                int found = -1;
                for (size_t c = 0; c < s.choices.size(); ++c)
                    if (prop.value.toString().equalsIgnoreCase (s.choices[c]))
                        found = (int) c;
                if (found < 0)
                {
                    error = "Unknown choice \"" + prop.value.toString() + "\" for " + s.key + " in preset " + p.name;
                    return false;
                }
                value = (float) found;
            }
            else if (prop.value.isBool())
                value = (bool) prop.value ? 1.0f : 0.0f;
            else
                value = (float) (double) prop.value;

            if (value < s.min - 1e-4f || value > s.max + 1e-4f)
            {
                error = "Value " + juce::String (value) + " out of range for " + s.key + " in preset " + p.name;
                return false;
            }
            p.values[(size_t) idx] = std::clamp (value, s.min, s.max);
        }
    }
    out = p;
    return true;
}

juce::String presetToJson (const Preset& p)
{
    auto* obj = new juce::DynamicObject();
    obj->setProperty ("name", p.name);
    obj->setProperty ("category", p.category);
    if (p.inspiredBy.isNotEmpty()) obj->setProperty ("inspiredBy", p.inspiredBy);
    if (p.band.isNotEmpty()) obj->setProperty ("band", p.band);
    if (p.song.isNotEmpty()) obj->setProperty ("song", p.song);
    if (p.description.isNotEmpty()) obj->setProperty ("description", p.description);
    if (p.author.isNotEmpty()) obj->setProperty ("author", p.author);
    juce::Array<juce::var> genres;
    for (auto& g : p.genres) genres.add (g);
    obj->setProperty ("genres", genres);

    auto* params = new juce::DynamicObject();
    const auto defaults = defaultParamValues();
    for (const auto& s : paramSpecs())
    {
        const float v = p.values[(size_t) s.id];
        if (std::abs (v - defaults[(size_t) s.id]) < 1e-5f) continue; // keep presets short
        if (s.kind == ParamKind::choice)
            params->setProperty (s.key, juce::String (s.choices[(size_t) std::lround (v)]));
        else if (s.kind == ParamKind::toggle)
            params->setProperty (s.key, v >= 0.5f);
        else
            params->setProperty (s.key, std::round (v * 100.0f) / 100.0f);
    }
    obj->setProperty ("params", juce::var (params));
    return juce::JSON::toString (juce::var (obj), false);
}

juce::String presetToShareCode (const Preset& p)
{
    const auto json = presetToJson (p);
    juce::MemoryOutputStream compressed;
    {
        juce::GZIPCompressorOutputStream gz (compressed, 9);
        gz.writeString (json);
    }
    return "riffhouse:" + juce::Base64::toBase64 (compressed.getData(), compressed.getDataSize());
}

namespace
{
bool decodeShareBody (const juce::String& body, Preset& out, juce::String& error)
{
    juce::MemoryOutputStream decoded;
    if (body.isEmpty() || ! juce::Base64::convertFromBase64 (decoded, body))
    {
        error = "The tone code is damaged.";
        return false;
    }
    juce::MemoryInputStream raw (decoded.getData(), decoded.getDataSize(), false);
    juce::GZIPDecompressorInputStream gz (raw);
    return presetFromJson (gz.readString(), out, error);
}

bool isBase64Char (juce::juce_wchar c)
{
    return (c < 128 && juce::CharacterFunctions::isLetterOrDigit (c)) || c == '+' || c == '/' || c == '=';
}
} // namespace

bool presetFromShareCode (const juce::String& text, Preset& out, juce::String& error)
{
    // The code can arrive inside a chat message, wrapped over lines by the app it went through,
    // or with words after it. Try every "riffhouse:" in the text: first with line breaks inside
    // the code joined up, then only up to the first space.
    const juce::String tag ("riffhouse:");
    int at = text.indexOf (tag);
    if (at < 0)
    {
        error = "That doesn't look like a Riffhouse tone code.";
        return false;
    }
    error = "The tone code is damaged.";
    for (; at >= 0; at = text.indexOf (at + 1, tag))
    {
        juce::String joined, firstWord;
        bool wordDone = false;
        for (auto p = text.getCharPointer() + (at + tag.length()); ! p.isEmpty(); ++p)
        {
            const auto c = *p;
            if (juce::CharacterFunctions::isWhitespace (c))
            {
                if (joined.isNotEmpty()) wordDone = true;
                continue;
            }
            if (! isBase64Char (c)) break;
            joined += c;
            if (! wordDone) firstWord += c;
        }
        juce::String why;
        if (decodeShareBody (joined, out, why)) return true;
        if (firstWord != joined && decodeShareBody (firstWord, out, why)) return true;
        if (why.isNotEmpty()) error = why;
    }
    return false;
}

juce::String presetToShareMessage (const Preset& p)
{
    const auto name = p.name.trim().isNotEmpty() ? p.name.trim() : juce::String ("My tone");
    // a tone named after an artist says it is a style; the line goes above the code so nothing
    // typed after the code can run into it
    const bool artist = p.artist().isNotEmpty() || name.startsWithIgnoreCase ("Inspired by ");
    return name + ", a guitar tone made in Riffhouse, the free amp app: https://natikchhabra.github.io/riffhouse/\n"
         + (artist ? "Inspired by means a style. Not affiliated with the artist.\n" : "")
         + "To play it, copy this whole message, open Riffhouse and press Ctrl+V.\n"
         + presetToShareCode (p);
}

void PresetLibrary::addFactory (const char* data, int size)
{
    Preset p;
    juce::String err;
    if (presetFromJson (juce::String::fromUTF8 (data, size), p, err))
    {
        p.factory = true;
        presets.push_back (p);
    }
    else
        DBG (err);
}

void PresetLibrary::scanFolder (const juce::File& folder, bool factory)
{
    for (const auto& f : folder.findChildFiles (juce::File::findFiles, true, "*.json"))
    {
        Preset p;
        juce::String err;
        if (presetFromJson (f.loadFileAsString(), p, err))
        {
            p.factory = factory;
            p.file = f.getFullPathName();
            presets.push_back (p);
        }
    }
}

std::vector<int> PresetLibrary::search (const juce::String& query, const juce::String& category) const
{
    juce::StringArray words;
    words.addTokens (query.toLowerCase(), " ", "\"");
    words.removeEmptyStrings();
    std::vector<int> out;
    for (int i = 0; i < (int) presets.size(); ++i)
    {
        const auto& p = presets[(size_t) i];
        if (category.isNotEmpty() && category != "All")
        {
            if (category == "Favourites") { if (! p.favourite) continue; }
            else if (category == "Inspired By") { if (p.inspiredBy.isEmpty()) continue; }
            else if (p.category != category && ! p.genres.contains (category)) continue;
        }
        const auto text = p.searchText();
        bool ok = true;
        for (auto& w : words)
            if (! text.contains (w)) { ok = false; break; }
        if (ok) out.push_back (i);
    }
    return out;
}

juce::StringArray PresetLibrary::categories() const
{
    juce::StringArray c;
    for (const auto& p : presets) c.addIfNotAlreadyThere (p.category);
    c.sort (true);
    return c;
}

int PresetLibrary::indexOf (const juce::String& name) const
{
    for (int i = 0; i < (int) presets.size(); ++i)
        if (presets[(size_t) i].name == name) return i;
    return -1;
}
} // namespace rh
