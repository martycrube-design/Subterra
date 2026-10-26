#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace subterra
{
namespace Easing
{
    inline float outCubic (float t)   { t = 1.0f - t; return 1.0f - t * t * t; }
    inline float inOutCubic (float t) { return t < 0.5f ? 4.0f * t * t * t : 1.0f - std::pow (-2.0f * t + 2.0f, 3.0f) * 0.5f; }
    inline float outBack (float t)
    {
        constexpr float c1 = 1.4f, c3 = c1 + 1.0f;
        t -= 1.0f;
        return 1.0f + c3 * t * t * t + c1 * t * t;
    }
}

// A value that eases towards a target over time. Call update() once per frame.
class Tween
{
public:
    using EasingFn = float (*) (float);

    explicit Tween (float initial = 0.0f) : from (initial), to (initial), value (initial) {}

    void setImmediately (float v) noexcept
    {
        from = to = value = v;
        running = false;
    }

    void animateTo (float target, double durationMs, EasingFn fn = Easing::outCubic) noexcept
    {
        if (juce::exactlyEqual (target, to) && running)
            return;

        from = value;
        to = target;
        easing = fn;
        duration = durationMs;
        start = juce::Time::getMillisecondCounterHiRes();
        running = durationMs > 0.0 && std::abs (to - from) > 1.0e-6f;
        if (! running)
            value = to;
    }

    // Returns true while the value is still moving.
    bool update() noexcept
    {
        if (! running)
            return false;

        const auto t = (float) ((juce::Time::getMillisecondCounterHiRes() - start) / duration);
        if (t >= 1.0f)
        {
            value = to;
            running = false;
            return true;   // one last repaint at the final value
        }

        value = from + (to - from) * easing (juce::jmax (0.0f, t));
        return true;
    }

    float get() const noexcept       { return value; }
    float getTarget() const noexcept { return to; }
    bool isRunning() const noexcept  { return running; }

private:
    float from, to, value;
    double start = 0.0, duration = 0.0;
    EasingFn easing = Easing::outCubic;
    bool running = false;
};

// Calls a function on every display refresh while the component is on screen.
// The function returns true if it changed anything that needs a repaint.
class FrameDriver
{
public:
    FrameDriver (juce::Component& c, std::function<bool()> tick)
        : component (c), onTick (std::move (tick)),
          attachment (&c, [this] { if (onTick && onTick()) component.repaint(); })
    {
    }

private:
    juce::Component& component;
    std::function<bool()> onTick;
    juce::VBlankAttachment attachment;
};
}
