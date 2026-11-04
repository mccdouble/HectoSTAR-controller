/*
    ------------------------------------------------------------------

    On-screen pattern playback. See Playback.h.

    ------------------------------------------------------------------
*/

#include "Playback.h"

#include <cmath>

namespace PatternEditor
{

Playback::Playback (PatternDocument& d) : doc (d) {}

Playback::~Playback()
{
    stopTimer();
}

void Playback::play()
{
    if (playing)
        return;
    if (time < 0)
    {
        time = 0;
        run = 1;
    }
    playing = true;
    lastMs = Time::getMillisecondCounterHiRes();
    startTimerHz (60);
    sendChangeMessage();
}

void Playback::pause()
{
    if (! playing)
        return;
    playing = false;
    stopTimer();
    sendChangeMessage();
}

void Playback::stop()
{
    playing = false;
    stopTimer();
    time = -1;
    run = 1;
    levels.fill (0);
    sendChangeMessage();
}

void Playback::sampleLevels (double from, double to, bool accumulate)
{
    if (! accumulate)
        levels.fill (0);

    const PatternData& d = doc.rendered();
    if (d.numSamples <= 0)
        return;

    int s0 = (int) std::floor (from * kSampleRateHz);
    int s1 = (int) std::ceil (to * kSampleRateHz);
    s0 = jlimit (0, d.numSamples - 1, s0);
    s1 = jlimit (s0 + 1, d.numSamples, s1);

    for (int led = 0; led < kNumLeds; ++led)
    {
        const std::uint8_t* lane = d.led (led);
        int hi = levels[(std::size_t) led];
        for (int s = s0; s < s1; ++s)
            hi = jmax (hi, (int) lane[s]);
        levels[(std::size_t) led] = hi;
    }
}

void Playback::timerCallback()
{
    const double now = Time::getMillisecondCounterHiRes();
    const double dt = jmax (0.0, (now - lastMs) / 1000.0) * speed;
    lastMs = now;

    const Pattern& p = doc.pattern();
    const double duration = jmax (1.0 / kSampleRateHz, p.duration);
    const int runs = jmax (1, p.repeatCount);

    const double from = time;
    double to = time + dt;

    if (to < duration)
    {
        sampleLevels (from, to, false);
        time = to;
    }
    else
    {
        // the end of this run, then on into the next one (if any)
        sampleLevels (from, duration, false);
        if (run >= runs)
        {
            playing = false;
            stopTimer();
            time = -1;
            run = 1;
            levels.fill (0);
        }
        else
        {
            ++run;
            to = std::fmod (to - duration, duration);
            sampleLevels (0, to, true);
            time = to;
        }
    }

    sendChangeMessage();
}

} // namespace PatternEditor
