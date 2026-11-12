// Riffhouse - free, open-source guitar rig
// Copyright (C) 2026 Riffhouse contributors. Licensed under AGPL-3.0-or-later.
#include "Reports.h"


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
        for (auto* marker : { "\\Users\\", "/Users/", "/home/", "\\users\\" })
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

    juce::String post (const juce::String& json)
    {
        juce::URL url (kRelay);
        url = url.withPOSTData (json);
        int status = 0;
        auto stream = url.createInputStream (juce::URL::InputStreamOptions (juce::URL::ParameterHandling::inPostData)
                                                 .withExtraHeaders ("Content-Type: application/json")
                                                 .withConnectionTimeoutMs (15000)
                                                 .withStatusCode (&status)
                                                 .withNumRedirectsToFollow (5));
        if (stream == nullptr) return "could not reach the report service";
        const auto answer = stream->readEntireStreamAsString();
        const auto v = juce::JSON::parse (answer);
        if ((bool) v.getProperty ("ok", false)) return {};
        const auto why = v.getProperty ("why", juce::var()).toString();
        return why.isNotEmpty() ? why : "the report service answered " + juce::String (status);
    }
} // namespace

juce::String scrub (const juce::String& text, const juce::StringArray& privateWords)
{
    auto t = scrubUserFolders (text);
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

bool helloOn (juce::PropertiesFile& s) { return s.getBoolValue ("weeklyHello", false); }
void setHelloOn (juce::PropertiesFile& s, bool on) { s.setValue ("weeklyHello", on); s.saveIfNeeded(); }

juce::String weekOf (juce::Time t)
{
    // ISO 8601 week: the week with the year's first Thursday is week 1
    const int dow = (t.getDayOfWeek() + 6) % 7; // Monday 0
    const auto thursday = t + juce::RelativeTime::days (3 - dow);
    const int yday = thursday.getDayOfYear(); // 0-based
    return juce::String (thursday.getYear()) + "-W" + juce::String (yday / 7 + 1).paddedLeft ('0', 2);
}

void sayHelloIfDue (juce::PropertiesFile& s)
{
    if (! helloOn (s) || ! canSend()) return;
    const auto week = weekOf (juce::Time::getCurrentTime());
    if (s.getValue ("helloWeek") == week) return;
    s.setValue ("helloWeek", week); // once a week, even if this one does not arrive
    s.saveIfNeeded();
    auto* o = new juce::DynamicObject();
    o->setProperty ("key", kKey);
    o->setProperty ("kind", "hello");
    o->setProperty ("week", week);
    const auto json = juce::JSON::toString (juce::var (o), true);
    juce::Thread::launch ([json] { post (json); });
}
} // namespace rh::reports
