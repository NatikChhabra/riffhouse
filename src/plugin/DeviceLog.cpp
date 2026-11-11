// Riffhouse - free, open-source guitar rig
// Copyright (C) 2026 Riffhouse contributors. Licensed under AGPL-3.0-or-later.
#include "DeviceLog.h"

#include <juce_events/juce_events.h>
#include <deque>

#if JUCE_WINDOWS
 #ifndef NOMINMAX
  #define NOMINMAX
 #endif
 #ifndef WIN32_LEAN_AND_MEAN
  #define WIN32_LEAN_AND_MEAN
 #endif
 #include <windows.h>
#endif

namespace rh::devlog
{
namespace
{
    constexpr int kKept = 400;                 // lines in memory
    constexpr juce::int64 kFileMax = 200 * 1024; // the file starts over past this
    constexpr double kFrozenMs = 1200.0;       // the window not answering for this long is written down
    constexpr double kSlowStartMs = 15000.0;   // a start that has not finished by then is written down

    double nowMs() { return juce::Time::getMillisecondCounterHiRes(); }

    struct Work { double began, ended; };

    struct State
    {
        juce::CriticalSection lock;
        std::deque<juce::String> lines;
        std::deque<Work> work;
        juce::File file, crashFile;
        juce::String doing; // what the sound card is being asked to do right now
        double doingSince = 0;
        LastRun lastRun = LastRun::fine;
    };

    // Read from the end of the diary: a run that closed by itself wrote "---- closed" last.
    LastRun howItEnded (const juce::StringArray& old)
    {
        bool sawFreezeLine = false, frozen = false;
        for (int i = old.size(); --i >= 0;)
        {
            const auto& l = old[i];
            if (l.contains ("---- closed")) return LastRun::fine;
            if (l.contains ("---- Riffhouse ")) return frozen ? LastRun::froze : LastRun::cutShort;
            if (sawFreezeLine) continue;
            if (l.contains ("The window answers again") || l.contains ("The window was ready")) sawFreezeLine = true;
            else if (l.contains ("The window stopped answering") || l.contains ("Starting is taking more than")) sawFreezeLine = frozen = true;
        }
        return LastRun::fine; // nothing written yet
    }

    State& state()
    {
        static State s;
        return s;
    }

    void write (const juce::String& line)
    {
        auto& s = state();
        const auto stamped = juce::Time::getCurrentTime().formatted ("%H:%M:%S ") + line;
        const juce::ScopedLock l (s.lock);
        s.lines.push_back (stamped);
        while ((int) s.lines.size() > kKept) s.lines.pop_front();
        if (s.file != juce::File()) s.file.appendText (stamped + "\n", false, false, "\n");
    }

    // The window draws and listens on one thread. This one only watches whether that thread
    // still answers, and writes down when it did not and what it was waiting for.
    class FreezeWatch : private juce::Thread, private juce::Timer
    {
    public:
        FreezeWatch() : juce::Thread ("Riffhouse freeze watch")
        {
            startedAt = nowMs();
            beat.store (startedAt);
            startTimer (250);
            startThread (juce::Thread::Priority::low);
        }
        ~FreezeWatch() override
        {
            stopTimer();
            signalThreadShouldExit();
            notify();
            stopThread (2000);
        }

    private:
        void timerCallback() override
        {
            const double now = nowMs();
            beat.store (now);
            // the first beat comes when everything is built and the window can be used
            if (! ready.exchange (true))
                write ("The window was ready " + juce::String (juce::roundToInt (now - startedAt)) + " ms after starting");
        }
        juce::String doingNow() const
        {
            auto& s = state();
            const juce::ScopedLock l (s.lock);
            return s.doing;
        }
        void run() override
        {
            bool frozen = false, slowStart = false;
            while (! threadShouldExit())
            {
                wait (400);
                if (! ready.load())
                {
                    // Starting is one long piece of work for the window, so it gets more time
                    // before anything is written down.
                    if (! slowStart && nowMs() - startedAt > kSlowStartMs)
                    {
                        slowStart = true;
                        const auto doing = doingNow();
                        write ("Starting is taking more than " + juce::String ((int) (kSlowStartMs / 1000.0)) + " seconds"
                               + (doing.isNotEmpty() ? " while: " + doing : juce::String()));
                    }
                    continue;
                }
                const double quiet = nowMs() - beat.load();
                if (! frozen && quiet > kFrozenMs)
                {
                    frozen = true;
                    const auto doing = doingNow();
                    write ("The window stopped answering" + (doing.isNotEmpty() ? " while: " + doing : juce::String (" (no sound card work in progress)")));
                }
                else if (frozen && quiet < 600.0)
                {
                    frozen = false;
                    write ("The window answers again");
                }
            }
        }
        std::atomic<double> beat { 0.0 };
        std::atomic<bool> ready { false };
        double startedAt = 0.0;
    };

    std::unique_ptr<FreezeWatch> watch;

   #if JUCE_WINDOWS
    wchar_t crashPath[1024] = {};
    char crashDoing[256] = {};

    // Runs inside a process that is already broken: no memory is asked for, nothing is locked.
    LONG WINAPI onCrash (EXCEPTION_POINTERS* info)
    {
        if (crashPath[0] != 0 && info != nullptr && info->ExceptionRecord != nullptr)
        {
            const HANDLE h = CreateFileW (crashPath, GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
            if (h != INVALID_HANDLE_VALUE)
            {
                char text[1100]; // wsprintf never writes more than 1024
                HMODULE where = nullptr;
                char inFile[260] = "?";
                if (GetModuleHandleExW (GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                                        (LPCWSTR) info->ExceptionRecord->ExceptionAddress, &where) && where != nullptr)
                    GetModuleFileNameA (where, inFile, sizeof (inFile));
                const char* name = inFile; // the file's name only: its folder can hold the person's name
                for (const char* c = inFile; *c != 0; ++c)
                    if (*c == '\\') name = c + 1;
                const int n = wsprintfA (text, "code %08lx at %p, app loaded at %p, in the file loaded at %p %s, thread %lu, doing: %s",
                                         info->ExceptionRecord->ExceptionCode, info->ExceptionRecord->ExceptionAddress,
                                         (void*) GetModuleHandleW (nullptr), (void*) where,
                                         name, GetCurrentThreadId(), crashDoing[0] != 0 ? crashDoing : "-");
                DWORD written = 0;
                if (n > 0) WriteFile (h, text, (DWORD) n, &written, nullptr);
                CloseHandle (h);
            }
        }
        return EXCEPTION_CONTINUE_SEARCH; // Windows then closes the app as it always would
    }
   #endif

    void setDoing (const juce::String& what)
    {
        auto& s = state();
        const juce::ScopedLock l (s.lock);
        s.doing = what;
        s.doingSince = nowMs();
       #if JUCE_WINDOWS
        what.copyToUTF8 (crashDoing, sizeof (crashDoing) - 1);
       #endif
    }
} // namespace

void start (const juce::File& folder, const juce::String& version)
{
    auto& s = state();
    {
        const juce::ScopedLock l (s.lock);
        folder.createDirectory();
        s.file = folder.getChildFile ("Riffhouse log.txt");
        s.crashFile = folder.getChildFile ("last-crash.txt");
        juce::StringArray old;
        s.file.readLines (old);
        old.removeEmptyStrings();
        s.lastRun = howItEnded (old);
        if (s.file.getSize() > kFileMax)
        {
            // keep the newer half, so what led up to today is still there
            old.removeRange (0, old.size() / 2);
            s.file.replaceWithText (old.joinIntoString ("\n") + "\n", false, false, "\n");
        }
    }
    write ("---- Riffhouse " + version + " started, " + juce::Time::getCurrentTime().formatted ("%Y-%m-%d")
           + ", " + juce::SystemStats::getOperatingSystemName());
    if (s.crashFile.existsAsFile())
    {
        s.lastRun = LastRun::crashed;
        write ("The last run ended in a crash: " + s.crashFile.loadFileAsString().trim());
        s.crashFile.deleteFile();
    }
    else if (s.lastRun == LastRun::froze) write ("The last run was closed while the window was not answering");
    else if (s.lastRun == LastRun::cutShort) write ("The last run did not close by itself");
   #if JUCE_WINDOWS
    s.crashFile.getFullPathName().copyToUTF16 (crashPath, sizeof (crashPath) - sizeof (wchar_t));
    SetUnhandledExceptionFilter (onCrash);
   #endif
    if (juce::MessageManager::getInstanceWithoutCreating() != nullptr) watch = std::make_unique<FreezeWatch>();
}

void stop()
{
    watch.reset();
    write ("---- closed");
    auto& s = state();
    const juce::ScopedLock l (s.lock);
    s.file = juce::File();
}

void note (const juce::String& line) { write (line); }

LastRun lastRun() { return state().lastRun; }

juce::String tail (int maxLines)
{
    auto& s = state();
    const juce::ScopedLock l (s.lock);
    juce::StringArray out;
    const int from = std::max (0, (int) s.lines.size() - maxLines);
    for (int i = from; i < (int) s.lines.size(); ++i) out.add (s.lines[(size_t) i]);
    return out.joinIntoString ("\n");
}

Timed::Timed (juce::String w) : what (std::move (w)), began (nowMs()) { setDoing (what); }

Timed::~Timed()
{
    const double ended = nowMs();
    {
        auto& s = state();
        const juce::ScopedLock l (s.lock);
        s.work.push_back ({ began, ended });
        while (s.work.size() > 64) s.work.pop_front();
    }
    setDoing ({});
    write (what + ": " + juce::String (juce::roundToInt (ended - began)) + " ms" + (outcome.isNotEmpty() ? ", " + outcome : juce::String()));
}

double Timed::ms() const { return nowMs() - began; }

double busyMs (double withinMs)
{
    auto& s = state();
    const double now = nowMs(), from = now - withinMs;
    const juce::ScopedLock l (s.lock);
    double sum = 0.0;
    for (const auto& w : s.work)
        if (w.ended > from) sum += w.ended - std::max (w.began, from);
    return sum;
}
} // namespace rh::devlog
