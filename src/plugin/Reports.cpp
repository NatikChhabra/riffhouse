// Riffhouse - free, open-source guitar rig
// Copyright (C) 2026 Riffhouse contributors. Licensed under AGPL-3.0-or-later.
#include "Reports.h"

#include <regex>

namespace rh::reports
{
namespace
{
   #ifdef RIFFHOUSE_REPORT_URL
    const char* const kRelay = RIFFHOUSE_REPORT_URL;
   #else
    const char* const kRelay = "";
   #endif
    const char* const kKey = "riffhouse-1";
    constexpr int kPerDay = 3;

    // the folder after "Users" or "home" in a path is the person's name
    juce::String scrubUserFolders (juce::String t)
    {
        for (auto* marker : { "\\Users\\", "/Users/", "/home/" }) // found ignoring case
        {
            const juce::String m (marker);
            const auto sep = m.getLastCharacter();
            int from = 0;
            for (;;)
            {
                const int at = t.indexOfIgnoreCase (from, m);
                if (at < 0) break;
                const int start = at + m.length();
                int end = start;
                while (end < t.length() && t[end] != sep && t[end] != '\n' && t[end] != '"' && t[end] != '\'') ++end;
                if (end > start && t.substring (start, end) != "<you>")
                    t = t.substring (0, start) + "<you>" + t.substring (end);
                from = start + 5;
            }
        }
        return t;
    }

    // Names the player never typed into Riffhouse but that ride along in device names, such as
    // "Headphones (Asha's AirPods)", and any email address. Ordinary words before 's are kept.
    juce::String scrubNamesInDevices (const juce::String& text)
    {
        static const juce::StringArray ordinary { "it", "that", "there", "what", "here", "let", "he", "she", "who", "riffhouse",
                                                  "windows", "computer", "driver", "guitar", "today", "player", "device", "card" };
        std::string s = text.replace (juce::String::fromUTF8 ("\xe2\x80\x99"), "'").toStdString();
        static const std::regex email (R"([A-Za-z0-9._%+-]+@[A-Za-z0-9.-]+\.[A-Za-z]{2,})");
        s = std::regex_replace (s, email, "<email>");
        auto t = juce::String::fromUTF8 (s.c_str());
        // the word before 's, in any alphabet: "Zoë's" and "José's" go as surely as "Asha's"
        // A letter in any alphabet. Above ASCII everything counts except the Latin-1 signs, the
        // general punctuation block (dashes, curly quotes, spaces) and the full-width signs.
        auto letter = [] (juce::juce_wchar c)
        {
            if (c < 128) return juce::CharacterFunctions::isLetterOrDigit (c);
            if (c < 0xc0 || c == 0xd7 || c == 0xf7) return false;
            if (c >= 0x2000 && c <= 0x2bff) return false;
            if (c >= 0x3000 && c <= 0x303f) return false;
            return ! (c >= 0xff00 && c <= 0xff0f);
        };
        auto partOfName = [&letter] (juce::juce_wchar c) { return letter (c) || c == '-' || c == '_'; };
        juce::String out;
        int from = 0;
        for (int at = t.indexOfIgnoreCase ("'s"); at >= 0; at = t.indexOfIgnoreCase (at + 2, "'s"))
        {
            if (at + 2 < t.length() && letter (t[at + 2])) continue; // "'sound": not an owner
            int begin = at;
            while (begin > from && partOfName (t[begin - 1])) --begin;
            const auto word = t.substring (begin, at);
            if (word.isEmpty() || juce::CharacterFunctions::isDigit (word[0]) || ordinary.contains (word, true)) continue;
            out << t.substring (from, begin) << "<you>";
            from = at;
        }
        out << t.substring (from);
        return out;
    }

    juce::String post (const juce::String& json)
    {
        // The relay answers a POST with a redirect to its reply, which has to be fetched with GET.
        // JUCE would repeat the POST there and get a "not found" page, so the redirect is followed here.
        juce::URL url = juce::URL (kRelay).withPOSTData (json);
        int status = 0;
        juce::StringPairArray headers;
        auto stream = url.createInputStream (juce::URL::InputStreamOptions (juce::URL::ParameterHandling::inPostData)
                                                 .withExtraHeaders ("Content-Type: application/json")
                                                 .withConnectionTimeoutMs (20000)
                                                 .withStatusCode (&status)
                                                 .withResponseHeaders (&headers)
                                                 .withNumRedirectsToFollow (0));
        if (stream == nullptr) return "could not reach the report service";
        juce::String answer;
        if (status >= 300 && status < 400)
        {
            juce::String location;
            for (const auto& k : headers.getAllKeys())
                if (k.equalsIgnoreCase ("location")) location = headers[k];
            if (! location.startsWith ("https://")) return "the report service answered " + juce::String (status);
            stream.reset();
            int status2 = 0;
            auto reply = juce::URL (location).createInputStream (juce::URL::InputStreamOptions (juce::URL::ParameterHandling::inAddress)
                                                                     .withConnectionTimeoutMs (20000)
                                                                     .withStatusCode (&status2)
                                                                     .withNumRedirectsToFollow (3));
            if (reply == nullptr) return "the report service did not say whether it arrived";
            answer = reply->readEntireStreamAsString();
            status = status2;
        }
        else
            answer = stream->readEntireStreamAsString();
        const auto v = juce::JSON::parse (answer);
        if ((bool) v.getProperty ("ok", false)) return {};
        const auto why = v.getProperty ("why", juce::var()).toString();
        return why.isNotEmpty() ? why : "the report service answered " + juce::String (status);
    }
} // namespace

juce::String scrub (const juce::String& text, const juce::StringArray& privateWords)
{
    auto t = scrubNamesInDevices (scrubUserFolders (text));
    for (int i = 0; i < privateWords.size(); ++i)
    {
        const auto w = privateWords[i].trim();
        if (w.length() < 3) continue; // two letters would eat ordinary words
        t = t.replace (w, i == privateWords.size() - 1 ? "<pc>" : "<you>", true);
    }
    return t;
}

juce::String scrubForThisComputer (const juce::String& text)
{
    // the computer's name last: it is replaced with <pc>, the rest with <you>
    juce::StringArray words { juce::SystemStats::getLogonName(), juce::SystemStats::getFullUserName() };
    for (const auto& part : juce::StringArray::fromTokens (juce::SystemStats::getFullUserName(), " ", {}))
        words.add (part);
    words.add (juce::SystemStats::getComputerName());
    return scrub (text, words);
}

bool canSend() { return juce::String (kRelay).startsWith ("https://"); }

bool allowedToday (juce::PropertiesFile& s)
{
    const auto today = juce::Time::getCurrentTime().formatted ("%Y-%m-%d");
    return s.getValue ("reportsDay") != today || s.getIntValue ("reportsSent") < kPerDay;
}

void send (juce::PropertiesFile& s, const juce::String& title, const juce::String& body, std::function<void (bool, juce::String)> done)
{
    if (! canSend() || ! allowedToday (s))
    {
        const juce::String why = canSend() ? "Three reports already went today. Save a report keeps it for later." : "This build has nowhere to send reports.";
        juce::MessageManager::callAsync ([done, why] { if (done) done (false, why); });
        return;
    }
    const auto today = juce::Time::getCurrentTime().formatted ("%Y-%m-%d");
    s.setValue ("reportsSent", s.getValue ("reportsDay") == today ? s.getIntValue ("reportsSent") + 1 : 1);
    s.setValue ("reportsDay", today);
    s.saveIfNeeded();

    auto* o = new juce::DynamicObject();
    o->setProperty ("key", kKey);
    o->setProperty ("kind", "report");
    o->setProperty ("title", scrubForThisComputer (title));
    // the report's own first line says which version wrote it
    for (const auto& line : juce::StringArray::fromLines (body))
        if (line.startsWith ("Riffhouse: ")) { o->setProperty ("version", line.fromFirstOccurrenceOf (": ", false, false).trim()); break; }
    o->setProperty ("body", scrubForThisComputer (body));
    const auto json = juce::JSON::toString (juce::var (o), true);
    juce::Thread::launch ([json, done]
    {
        const auto err = post (json);
        juce::MessageManager::callAsync ([done, err] { if (done) done (err.isEmpty(), err); });
    });
}

juce::String sendNow (const juce::String& title, const juce::String& body)
{
    if (! canSend()) return "This build has nowhere to send reports.";
    auto* o = new juce::DynamicObject();
    o->setProperty ("key", kKey);
    o->setProperty ("kind", "report");
    o->setProperty ("title", scrubForThisComputer (title));
    o->setProperty ("version", "check");
    o->setProperty ("body", scrubForThisComputer (body));
    return post (juce::JSON::toString (juce::var (o), true));
}

// On unless the player switched it off (Natik, 12 Nov 2026): players who never open setup are counted too.
bool helloOn (juce::PropertiesFile& s) { return s.getBoolValue ("weeklyHello", true); }
void setHelloOn (juce::PropertiesFile& s, bool on) { s.setValue ("weeklyHello", on); s.saveIfNeeded(); }

juce::String weekOf (juce::Time t)
{
    // ISO 8601 week: the week with the year's first Thursday is week 1
    const int dow = (t.getDayOfWeek() + 6) % 7; // Monday 0
    const auto thursday = t + juce::RelativeTime::days (3 - dow);
    const int yday = thursday.getDayOfYear(); // 0-based
    return juce::String (thursday.getYear()) + "-W" + juce::String (yday / 7 + 1).paddedLeft ('0', 2);
}

juce::String installId (juce::PropertiesFile& s)
{
    // Random numbers only, made once: nothing from the player, the PC or its devices goes into it.
    auto id = s.getValue ("installId");
    if (! isInstallId (id))
    {
        id = juce::Uuid().toString(); // 32 lowercase hex digits from the system's random source
        s.setValue ("installId", id);
        s.saveIfNeeded();
    }
    return id;
}

bool isInstallId (const juce::String& id)
{
    return id.length() == 32 && id.containsOnly ("0123456789abcdef");
}

juce::String helloJson (const juce::String& week, const juce::String& id, const juce::String& version)
{
    auto* o = new juce::DynamicObject();
    o->setProperty ("key", kKey);
    o->setProperty ("kind", "hello");
    o->setProperty ("week", week);
    o->setProperty ("id", id);
    o->setProperty ("version", version);
    return juce::JSON::toString (juce::var (o), true);
}

void sayHelloIfDue (juce::PropertiesFile& s, const juce::String& version)
{
    if (! helloOn (s) || ! canSend()) return;
    const auto week = weekOf (juce::Time::getCurrentTime());
    if (s.getValue ("helloWeek") == week) return;
    s.setValue ("helloWeek", week); // once a week, even if this one does not arrive
    s.saveIfNeeded();
    const auto json = helloJson (week, installId (s), version);
    juce::Thread::launch ([json] { post (json); });
}
} // namespace rh::reports
