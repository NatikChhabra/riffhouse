// Riffhouse - free, open-source guitar rig
// Copyright (C) 2026 Riffhouse contributors. Licensed under AGPL-3.0-or-later.
#include "Motion.h"

#if JUCE_WINDOWS
 #ifndef NOMINMAX
  #define NOMINMAX
 #endif
 #ifndef WIN32_LEAN_AND_MEAN
  #define WIN32_LEAN_AND_MEAN
 #endif
 #include <windows.h>
#endif

namespace rh::ui::motion
{
namespace
{
    double handClock = -1.0;
    bool lessMotion = false, lessMotionSet = false, frozen = false;
    double jumpAt = -1.0e12;

    // y of a CSS cubic-bezier(x1, y1, x2, y2) at x = t
    float bezier (float x1, float y1, float x2, float y2, float t)
    {
        if (t <= 0.0f) return 0.0f;
        if (t >= 1.0f) return 1.0f;
        auto at = [] (float a, float b, float u) { const float v = 1.0f - u; return 3.0f * v * v * u * a + 3.0f * v * u * u * b + u * u * u; };
        float lo = 0.0f, hi = 1.0f, u = t;
        for (int i = 0; i < 24; ++i)
        {
            const float x = at (x1, x2, u);
            if (std::abs (x - t) < 1.0e-4f) break;
            if (x < t) lo = u; else hi = u;
            u = 0.5f * (lo + hi);
        }
        return at (y1, y2, u);
    }

    // The picture a component leaves behind while it fades.
    class Ghost : public juce::Component
    {
    public:
        Ghost (juce::Image i, float from, int ms) : image (std::move (i)), alpha (from)
        {
            setInterceptsMouseClicks (false, false);
            setOpaque (false);
            alpha.to (0.0f, ms, Curve::out, true);
        }
        void paint (juce::Graphics& g) override
        {
            g.setOpacity (alpha.get());
            g.drawImage (image, getLocalBounds().toFloat());
        }
        bool done() const { return ! alpha.moving(); }

    private:
        juce::Image image;
        Glide alpha;
    };

    struct Job
    {
        juce::Component::SafePointer<juce::Component> comp;
        juce::Component* key = nullptr;
        int channel = 0;
        juce::uint64 id = 0;
        std::function<bool()> moving;
        std::function<void()> onFrame;
    };

    struct Hub
    {
        std::vector<Job> jobs;
        std::vector<std::unique_ptr<Ghost>> ghosts;
        std::vector<std::pair<juce::Component*, std::unique_ptr<juce::VBlankAttachment>>> homes;
        juce::uint64 nextId = 1;
        double lastTick = -1.0;
        bool ticking = false;

        void add (juce::Component& c, int channel, std::function<bool()> moving, std::function<void()> onFrame)
        {
            for (auto& j : jobs)
                if (j.key == &c && j.channel == channel)
                {
                    j.comp = &c;
                    j.id = nextId++;
                    j.moving = std::move (moving);
                    j.onFrame = std::move (onFrame);
                    return;
                }
            jobs.push_back ({ &c, &c, channel, nextId++, std::move (moving), std::move (onFrame) });
        }

        void drop (juce::Component& c, int channel)
        {
            jobs.erase (std::remove_if (jobs.begin(), jobs.end(), [&] (const Job& j) { return j.key == &c && j.channel == channel; }), jobs.end());
        }

        void tick()
        {
            if (ticking) return;
            const juce::ScopedValueSetter<bool> guard (ticking, true);
            // a frame may start new moves, so it runs on a copy and removes by id
            const auto frame = jobs;
            std::vector<juce::uint64> over;
            for (const auto& j : frame)
            {
                if (j.comp == nullptr) { over.push_back (j.id); continue; }
                if (j.onFrame) j.onFrame();
                if (j.comp == nullptr) { over.push_back (j.id); continue; }
                const bool more = j.moving ? j.moving() : false;
                j.comp->repaint();
                if (! more) over.push_back (j.id);
            }
            if (! over.empty())
                jobs.erase (std::remove_if (jobs.begin(), jobs.end(),
                                            [&] (const Job& j) { return std::find (over.begin(), over.end(), j.id) != over.end(); }),
                            jobs.end());
            for (auto& g : ghosts) g->repaint();
            ghosts.erase (std::remove_if (ghosts.begin(), ghosts.end(), [] (const std::unique_ptr<Ghost>& g) { return g->done(); }), ghosts.end());
        }

        void frame()
        {
            if (jobs.empty() && ghosts.empty()) return;
            // two editors on screen both deliver frames: one tick per frame is enough
            const double t = juce::Time::getMillisecondCounterHiRes();
            if (t - lastTick < 2.0) return;
            lastTick = t;
            tick();
        }
    };

    Hub& hub()
    {
        static Hub h;
        return h;
    }

    constexpr int enterChannel = 1;

    void settle (juce::Component& c)
    {
        hub().drop (c, enterChannel);
        c.setAlpha (1.0f);
        c.setTransform ({});
    }

    float pictureScale (juce::Component& c)
    {
        float s = juce::Component::getApproximateScaleFactorForComponent (&c);
        if (const auto* d = juce::Desktop::getInstance().getDisplays().getDisplayForRect (c.getScreenBounds()))
            s *= (float) d->scale;
        return juce::jlimit (1.0f, 3.0f, s);
    }
} // namespace

double now() { return handClock >= 0.0 ? handClock : juce::Time::getMillisecondCounterHiRes(); }
void setClock (double ms) { handClock = std::max (0.0, ms); }
void freeClock() { handClock = -1.0; }
void step() { hub().tick(); }

bool systemPrefersReduced()
{
   #if JUCE_WINDOWS
    BOOL animations = TRUE;
    if (SystemParametersInfoW (0x1042 /* SPI_GETCLIENTAREAANIMATION */, 0, &animations, 0))
        return animations == FALSE;
   #endif
    return false;
}

bool reduced()
{
    if (! lessMotionSet) { lessMotion = systemPrefersReduced(); lessMotionSet = true; }
    return lessMotion;
}

void setReduced (bool on) { lessMotion = on; lessMotionSet = true; }
void freeze (bool on) { frozen = on; }
bool still() { return frozen || reduced(); }

float ease (Curve c, float t)
{
    switch (c)
    {
        case Curve::out:    return bezier (0.23f, 1.0f, 0.32f, 1.0f, t);
        case Curve::inOut:  return bezier (0.77f, 0.0f, 0.175f, 1.0f, t);
        case Curve::linear: break;
    }
    return juce::jlimit (0.0f, 1.0f, t);
}

float after (double since, int delay, int ms, Curve c)
{
    if (still() || ms <= 0) return 1.0f;
    const double t = (now() - since - (double) delay) / (double) ms;
    return t <= 0.0 ? 0.0f : (t >= 1.0 ? 1.0f : ease (c, (float) t));
}

float Glide::get() const
{
    if (length <= 0) return goal;
    const float t = (float) ((now() - start) / (double) length);
    if (t >= 1.0f) return goal;
    return from + (goal - from) * ease (curve, std::max (0.0f, t));
}

bool Glide::moving() const { return length > 0 && now() < start + (double) length; }

void Glide::to (float v, int ms, Curve c, bool fade)
{
    if (juce::exactlyEqual (v, goal)) return;
    if (frozen) ms = 0;
    else if (reduced()) ms = fade ? std::min (ms, 110) : 0;
    from = get();
    goal = v;
    start = now();
    length = std::max (0, ms);
    curve = c;
    if (length == 0) from = goal;
}

void run (juce::Component& c, std::function<bool()> moving, std::function<void()> onFrame)
{
    hub().add (c, 0, std::move (moving), std::move (onFrame));
}

void attach (juce::Component& home)
{
    auto& h = hub();
    for (auto& e : h.homes)
        if (e.first == &home) return;
    h.homes.emplace_back (&home, std::make_unique<juce::VBlankAttachment> (&home, [] { hub().frame(); }));
}

void detach (juce::Component& home)
{
    auto& h = hub();
    h.homes.erase (std::remove_if (h.homes.begin(), h.homes.end(), [&] (const auto& e) { return e.first == &home; }), h.homes.end());
    if (h.homes.empty())
    {
        // nothing left to draw on: whatever was in flight ends here, in its finished state
        const auto jobs = h.jobs;
        h.jobs.clear();
        for (const auto& j : jobs)
            if (j.comp != nullptr && j.channel == enterChannel) { j.comp->setAlpha (1.0f); j.comp->setTransform ({}); }
        h.ghosts.clear();
    }
}

void enter (juce::Component& c, Enter kind)
{
    if (frozen) { settle (c); return; }
    struct State { Glide a; };
    auto s = std::make_shared<State>();
    const bool less = reduced();
    s->a.to (1.0f, kind == Enter::fade ? ui : overlay, Curve::out, true);
    juce::Component::SafePointer<juce::Component> safe (&c);
    auto apply = [s, safe, kind, less]
    {
        if (safe == nullptr) return;
        const float v = s->a.get();
        safe->setAlpha (v);
        if (less || kind == Enter::fade || v >= 1.0f) { safe->setTransform ({}); return; }
        const auto b = safe->getBounds().toFloat();
        if (kind == Enter::rise) safe->setTransform (juce::AffineTransform::translation (0.0f, 6.0f * (1.0f - v)));
        else if (kind == Enter::card) { const float k = 0.98f + 0.02f * v; safe->setTransform (juce::AffineTransform::scale (k, k, b.getCentreX(), b.getCentreY())); }
        else { const float k = 0.97f + 0.03f * v; safe->setTransform (juce::AffineTransform::scale (k, k, b.getCentreX(), b.getY())); }
    };
    apply();
    hub().add (c, enterChannel, [s] { return s->a.moving(); }, apply);
}

void ghost (juce::Component& parent, juce::Rectangle<int> area, int ms, juce::Component* justAbove)
{
    area = area.getIntersection (parent.getLocalBounds());
    if (frozen || area.isEmpty() || ! parent.isShowing()) return;
    auto g = std::make_unique<Ghost> (parent.createComponentSnapshot (area, true, pictureScale (parent)), 1.0f, ms);
    g->setBounds (area);
    parent.addAndMakeVisible (*g, justAbove != nullptr ? parent.getIndexOfChildComponent (justAbove) + 1 : -1);
    hub().ghosts.push_back (std::move (g));
}

void ghost (juce::Component& c, int ms)
{
    auto* parent = c.getParentComponent();
    if (frozen || parent == nullptr || ! parent->isShowing() || c.getBounds().isEmpty()) return;
    const float from = c.getAlpha();
    c.setAlpha (1.0f);
    c.setTransform ({});
    auto g = std::make_unique<Ghost> (c.createComponentSnapshot (c.getLocalBounds(), true, pictureScale (*parent)), from, ms);
    g->setBounds (c.getBounds());
    parent->addAndMakeVisible (*g, parent->getIndexOfChildComponent (&c) + 1);
    hub().ghosts.push_back (std::move (g));
}

Presence::Presence (juce::Component& c, Enter k) : comp (c), kind (k), wasVisible (c.isVisible())
{
    comp.addComponentListener (this);
}

Presence::~Presence()
{
    comp.removeComponentListener (this);
    hub().drop (comp, enterChannel);
}

void Presence::componentVisibilityChanged (juce::Component& c)
{
    const bool vis = c.isVisible();
    if (vis == wasVisible) return;
    wasVisible = vis;
    if (! enabled) { settle (c); return; }
    if (vis) enter (c, kind);
    else
    {
        hub().drop (c, enterChannel);
        ghost (c, leave); // also puts the component itself back to full strength for next time
        c.setAlpha (1.0f);
        c.setTransform ({});
    }
}

Feel& Feel::of (juce::Component& c)
{
    static const juce::Identifier id ("feel");
    auto& props = c.getProperties();
    if (auto* f = dynamic_cast<Feel*> (props[id].getObject())) return *f;
    auto* f = new Feel();
    props.set (id, juce::var (f));
    return *f;
}

void Feel::follow (juce::Component& c, bool over, bool pressed, bool toggled)
{
    if (! primed)
    {
        hover.snap (over ? 1.0f : 0.0f);
        down.snap (pressed ? 1.0f : 0.0f);
        on.snap (toggled ? 1.0f : 0.0f);
        primed = true;
        return;
    }
    hover.to (over ? 1.0f : 0.0f, motion::hover, Curve::out, true);
    down.to (pressed ? 1.0f : 0.0f, pressed ? press : release);
    on.to (toggled ? 1.0f : 0.0f, toggle, Curve::out);
    if (hover.moving() || down.moving() || on.moving())
        run (c, [this] { return hover.moving() || down.moving() || on.moving(); });
}

void announceJump() { jumpAt = now(); }
bool jumpJustHappened() { return now() - jumpAt < 150.0; }
} // namespace rh::ui::motion
