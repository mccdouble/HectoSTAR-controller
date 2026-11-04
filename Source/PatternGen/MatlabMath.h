/*
    ------------------------------------------------------------------

    MATLAB-compatible math used by the pattern generator and by formula
    waveforms, so both produce exactly what PatternGenGUI.m would.

    ------------------------------------------------------------------
*/

#pragma once

#include <algorithm>
#include <cfloat>
#include <cmath>

namespace PatternGen
{
namespace matlab
{

constexpr double kPi = 3.141592653589793; // MATLAB's pi
constexpr double kTwoPi = 2.0 * kPi;     // MATLAB's 2*pi (exact doubling)

/** MATLAB min/max ignore NaN. */
inline double min (double a, double b) { return std::isnan (a) ? b : (std::isnan (b) ? a : std::min (a, b)); }
inline double max (double a, double b) { return std::isnan (a) ? b : (std::isnan (b) ? a : std::max (a, b)); }

/** MATLAB round: half away from zero. */
inline double round (double x) { return std::round (x); }

/** MATLAB's floating-point mod (from MATLAB Coder's coder.internal.scalar.mod):
    fmod, except that a quotient within eps of an integer gives exactly 0 when
    y is not an integer, and the result takes the sign of y. */
inline double mod (double x, double y)
{
    if (y == 0)
        return x == 0 ? y : x;
    if (std::isnan (x) || std::isnan (y) || std::isinf (x))
        return NAN;
    if (std::isinf (y))
    {
        if (y > 0)
            return x > 0 ? x : (x < 0 ? y : 0.0);
        return x > 0 ? y : (x < 0 ? x : -0.0);
    }

    double r;
    if (y > std::floor (y))
    {
        const double q = std::fabs (x / y);
        r = (std::fabs (q - std::floor (q + 0.5)) > DBL_EPSILON * q) ? std::fmod (x, y) : 0.0;
    }
    else
    {
        r = std::fmod (x, y);
    }

    if (r == 0)
        return y * 0.0;
    if ((r < 0 && y > 0) || (r > 0 && y < 0))
        r += y;
    return r;
}

/** MATLAB's floating-point rem: same zero snapping as mod, sign of x. */
inline double rem (double x, double y)
{
    if (y == 0 || std::isnan (x) || std::isnan (y) || std::isinf (x))
        return NAN;
    if (std::isinf (y))
        return x;

    double r;
    if (y > std::floor (y))
    {
        const double q = std::fabs (x / y);
        r = (std::fabs (q - std::floor (q + 0.5)) > DBL_EPSILON * q) ? std::fmod (x, y) : 0.0;
    }
    else
    {
        r = std::fmod (x, y);
    }

    return r == 0 ? x * 0.0 : r;
}

/** Signal Processing Toolbox square(t, duty), duty in percent. */
inline double square (double t, double duty = 50.0)
{
    const double tmp = mod (t, kTwoPi);
    const double w0 = kTwoPi * duty / 100.0;
    return tmp < w0 ? 1.0 : -1.0;
}

/** Signal Processing Toolbox sawtooth(t) (width = 1). */
inline double sawtooth (double t)
{
    const double width = 1.0;
    const double c1 = 2.0;
    const double c2 = 2.0;
    const double rt = rem (t, kTwoPi) * (1.0 / kTwoPi);
    const double neg = t < 0 ? 1.0 : 0.0;

    if (rt > 0)
    {
        if (rt > width)
            return (-neg - rt + 1.0 - 0.5 * (1.0 - width)) * c2;
        return (neg + rt - 0.5 * width) * c1;
    }
    if (rt < 0)
    {
        if (rt < width - 1.0)
            return (neg + rt - 0.5 * width) * c1;
        return (-neg - rt + 1.0 - 0.5 * (1.0 - width)) * c2;
    }
    return (rt - 0.5 * width) * c1;
}

} // namespace matlab
} // namespace PatternGen
