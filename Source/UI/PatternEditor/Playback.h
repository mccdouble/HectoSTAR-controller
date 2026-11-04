/*
    ------------------------------------------------------------------

    On-screen playback of a pattern, for checking and demonstrating what the
    board will do. Nothing is sent to the board.

    The clock runs at a chosen speed (real time or slowed down) and plays
    the pattern as many times as its repeat count, like the board. The
    screen refreshes at about 60 Hz, far slower than the 20 kHz output, so
    each LED's level is the highest current it reaches since the previous
    refresh: a 5 ms pulse still shows as a flash instead of being missed.

    ------------------------------------------------------------------
*/

#pragma once

#include "PatternDocument.h"

#include <array>

namespace PatternEditor
{

class Playback : public ChangeBroadcaster,
                 private Timer
{
public:
    explicit Playback (PatternDocument& doc);
    ~Playback() override;

    void play();
    void pause();
    void stop();

    bool isPlaying() const { return playing; }

    /** True while playing or paused (the LEDs show playback levels). */
    bool isActive() const { return time >= 0; }

    /** Position in the current run (s), -1 when stopped. */
    double getTime() const { return time; }

    /** Current run, 1..repeat count. */
    int getRun() const { return run; }

    /** 1 = real time, 0.1 = ten times slower, ... */
    void setSpeed (double s) { speed = s; }
    double getSpeed() const { return speed; }

    /** Highest current (uA) of LED 1..64 since the previous refresh. */
    int level (int led) const { return led >= 1 && led <= kNumLeds ? levels[(std::size_t) led - 1] : 0; }

private:
    PatternDocument& doc;
    bool playing = false;
    double time = -1;
    int run = 1;
    double speed = 1.0;
    double lastMs = 0;
    std::array<int, kNumLeds> levels {};

    void timerCallback() override;
    void sampleLevels (double from, double to, bool accumulate);
};

} // namespace PatternEditor
