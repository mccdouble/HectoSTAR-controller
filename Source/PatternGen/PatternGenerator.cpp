/*
    ------------------------------------------------------------------

    HectoSTAR pattern stim generator. See PatternGenerator.h.

    Every expression below follows the evaluation order of PatternGenGUI.m
    so floating-point rounding, and therefore every output byte, matches the
    MATLAB tool.

    ------------------------------------------------------------------
*/

#include "PatternGenerator.h"

#include <algorithm>
#include <cfloat>
#include <cmath>

namespace PatternGen
{

namespace
{

// True when x is a whole number of y's to within rounding (only checked for
// non-integer y, as MATLAB does; integer y gives an exact fmod anyway).
bool isWholeMultiple (double x, double y)
{
    if (! (y > std::floor (y)))
        return false;
    const double q = std::fabs (x / y);
    return std::fabs (q - std::floor (q + 0.5)) <= DBL_EPSILON * q;
}

double sampleTime (int i) { return sampleTimeSec (i); } // t = (0:N-1)/fs

// Renders samples [first, end) of a waveform into out[0 .. end - first), before
// clipping. Each sample depends only on its own absolute time, so any window
// gives exactly the values a full-length render would.
void renderRangeDouble (const ChannelWaveform& cfg, int first, int end, double* out)
{
    std::fill (out, out + (end - first), 0.0);

    const double amp = std::fmin (cfg.amp, (double) kMaxCurrentUa); // min(data{r,4},100)

    switch (cfg.type)
    {
        case WaveType::None:
            break;

        case WaveType::Sine:
            for (int i = first; i < end; ++i)
            {
                const double t = sampleTime (i);
                if (t >= cfg.delay)
                    out[i - first] = cfg.offset + amp * std::sin (kTwoPi * cfg.freq * (t - cfg.delay));
            }
            break;

        case WaveType::FixedSine:
        {
            const double nCycles = std::round (cfg.freq);
            const double T = cfg.offset;
            const double tStart = cfg.delay;

            if (nCycles > 0 && std::isfinite (T) && T > 0)
            {
                const double duty = std::fmax (0.0, std::fmin (100.0, cfg.duty));
                const double activeT = (duty / 100.0) * T;
                const double tEnd = tStart + nCycles * T;

                for (int i = first; i < end; ++i)
                {
                    const double t = sampleTime (i);
                    if (t >= tStart && t < tEnd)
                    {
                        const double phase = mod (t - tStart, T);
                        if (activeT > 0 && phase < activeT)
                            out[i - first] = amp * std::sin ((phase / activeT) * kTwoPi);
                    }
                }
            }
            break;
        }

        case WaveType::Pulse:
            for (int i = first; i < end; ++i)
            {
                const double t = sampleTime (i);
                if (t >= cfg.delay)
                    out[i - first] = cfg.offset + amp * square (kTwoPi * cfg.freq * (t - cfg.delay), cfg.duty);
            }
            break;

        case WaveType::Sawtooth:
            for (int i = first; i < end; ++i)
            {
                const double t = sampleTime (i);
                if (t >= cfg.delay)
                    out[i - first] = cfg.offset + amp * sawtooth (kTwoPi * cfg.freq * (t - cfg.delay));
            }
            break;

        case WaveType::FixedSawtooth:
        {
            const double nCycles = std::round (cfg.freq);
            const double T = cfg.offset;
            const double tStart = cfg.delay;

            if (nCycles > 0 && std::isfinite (T) && T > 0)
            {
                const double duty = std::fmax (0.0, std::fmin (100.0, cfg.duty));
                const double rampT = (duty / 100.0) * T;
                const double tEnd = tStart + nCycles * T;

                for (int i = first; i < end; ++i)
                {
                    const double t = sampleTime (i);
                    if (t >= tStart && t < tEnd)
                    {
                        const double phase = mod (t - tStart, T);
                        if (rampT > 0 && phase < rampT)
                            out[i - first] = amp * (phase / rampT);
                    }
                }
            }
            break;
        }

        case WaveType::DoubleSine:
        {
            const double f1 = cfg.freq;
            const double f2 = cfg.offset; // repurposed
            const double base = cfg.duty; // repurposed

            for (int i = first; i < end; ++i)
            {
                const double t = sampleTime (i);
                if (t >= cfg.delay)
                {
                    const double td = t - cfg.delay;
                    out[i - first] = base + (amp / 2.0) * (std::sin (kTwoPi * f1 * td) + std::sin (kTwoPi * f2 * td));
                }
            }
            break;
        }

        case WaveType::FixedPulse:
        {
            const double nPulses = std::round (cfg.freq);
            const double period = cfg.offset;

            if (nPulses > 0 && period > 0)
            {
                const double totalTime = nPulses * period;

                for (int i = first; i < end; ++i)
                {
                    const double t = sampleTime (i);
                    if (t >= cfg.delay)
                    {
                        const double td = t - cfg.delay;
                        if (td < totalTime && mod (td, period) / period < (cfg.duty / 100.0))
                            out[i - first] = amp;
                    }
                }
            }
            break;
        }

        case WaveType::Wavelet:
        {
            double nPeaks = std::round (cfg.freq);
            const double D = cfg.offset;
            const double tStart = cfg.delay;

            if (! std::isfinite (D) || D <= 0 || ! std::isfinite (nPeaks) || nPeaks < 1)
                break;

            if (mod (nPeaks, 2.0) == 0)
                nPeaks = nPeaks + 1;

            const double tEnd = tStart + D;
            const double tc = tStart + D / 2.0;
            const double cycles = (nPeaks - 1.0) / 2.0;
            const double f = cycles / D;

            for (int i = first; i < end; ++i)
            {
                const double t = sampleTime (i);
                if (t >= tStart && t <= tEnd)
                {
                    const double u = (t - tc) / (D / 2.0);
                    const double w = 0.5 * (1.0 + std::cos (kPi * u));
                    const double c = std::cos (kTwoPi * f * (t - tc));
                    out[i - first] = amp * w * (c * c);
                }
            }
            break;
        }

        case WaveType::PulseStaircase:
        {
            const double nPulses = std::round (cfg.freq);
            const double A0 = amp;
            const double dA = cfg.step;
            const double period = cfg.offset;
            const double tStart = cfg.delay;

            if (nPulses > 0 && period > 0)
            {
                const double tEnd = tStart + nPulses * period;

                for (int i = first; i < end; ++i)
                {
                    const double t = sampleTime (i);
                    if (t >= tStart && t < tEnd)
                    {
                        const double tau = t - tStart;
                        const double pulseIdx = std::floor (tau / period);
                        if (mod (tau, period) / period < (cfg.duty / 100.0))
                            out[i - first] = A0 + dA * pulseIdx;
                    }
                }
            }
            break;
        }
    }
}

std::uint8_t toDacValue (double v)
{
    // y(y<0)=0; y(y>100)=100; uint8(round(y)) -- NaN becomes 0
    if (std::isnan (v))
        return 0;
    if (v < 0)
        v = 0;
    if (v > kMaxCurrentUa)
        v = kMaxCurrentUa;
    return (std::uint8_t) std::round (v);
}

struct NamedType
{
    WaveType type;
    const char* name;
};

constexpr NamedType kTypeNames[] = {
    { WaveType::None, "none" },
    { WaveType::Pulse, "pulse" },
    { WaveType::FixedPulse, "fixedpulse" },
    { WaveType::PulseStaircase, "pulse_staircase" },
    { WaveType::Sawtooth, "sawtooth" },
    { WaveType::FixedSawtooth, "fixedsawtooth" },
    { WaveType::Sine, "sine" },
    { WaveType::FixedSine, "fixedsine" },
    { WaveType::DoubleSine, "doublesine" },
    { WaveType::Wavelet, "wavelet" },
};

// Channel reorder tables from getReorderForDevice in PatternGenGUI.m (1-based)
constexpr std::array<int, kNumLeds> kFourShankChronic = {
    17, 34, 19, 36, 21, 38, 23, 40, 25, 42, 27, 44, 29, 46, 31, 48,
    32, 47, 30, 45, 28, 43, 26, 41, 24, 39, 22, 37, 20, 35, 18, 33,
    1, 50, 3, 52, 5, 54, 7, 56, 9, 58, 11, 60, 13, 62, 15, 64,
    16, 63, 14, 61, 12, 59, 10, 57, 8, 55, 6, 53, 4, 51, 2, 49
};

constexpr std::array<int, kNumLeds> kTwoShankChronic = {
    1, 34, 3, 36, 5, 38, 7, 40, 9, 42, 11, 44, 13, 46, 15, 48,
    17, 50, 19, 52, 21, 54, 23, 56, 25, 58, 27, 60, 29, 62, 31, 64,
    32, 63, 30, 61, 28, 59, 26, 57, 24, 55, 22, 53, 20, 51, 18, 49,
    16, 47, 14, 45, 12, 43, 10, 41, 8, 39, 6, 37, 4, 35, 2, 33
};

constexpr std::array<int, kNumLeds> kFourShankAcute = {
    33, 18, 35, 20, 37, 22, 39, 24, 41, 26, 43, 28, 45, 30, 47, 32,
    48, 31, 46, 29, 44, 27, 42, 25, 40, 23, 38, 21, 36, 19, 34, 17,
    50, 1, 52, 3, 54, 5, 56, 7, 58, 9, 60, 11, 62, 13, 64, 15,
    63, 16, 61, 14, 59, 12, 57, 10, 55, 8, 53, 6, 51, 4, 49, 2
};

constexpr std::array<int, kNumLeds> kDemoBoard = {
    16, 14, 12, 10, 8, 6, 4, 2, 64, 62, 60, 58, 56, 54, 52, 50,
    32, 30, 28, 26, 24, 22, 20, 18, 48, 46, 44, 42, 40, 38, 36, 34,
    15, 13, 11, 9, 7, 5, 3, 1, 63, 61, 59, 57, 55, 53, 51, 49,
    31, 29, 27, 25, 23, 21, 19, 17, 47, 45, 43, 41, 39, 37, 35, 33
};

} // namespace

double mod (double x, double y)
{
    if (y == 0)
        return x;
    if (isWholeMultiple (x, y))
        return 0.0;
    double r = std::fmod (x, y);
    if (r != 0 && (r < 0) != (y < 0))
        r += y;
    return r;
}

double rem (double x, double y)
{
    return isWholeMultiple (x, y) ? 0.0 : std::fmod (x, y);
}

double square (double t, double dutyPercent)
{
    return mod (t, kTwoPi) < kTwoPi * dutyPercent / 100.0 ? 1.0 : -1.0;
}

double sawtooth (double t)
{
    const double rt = rem (t, kTwoPi) * (1.0 / kTwoPi);
    if (rt == 0)
        return -1.0;
    return ((t < 0 ? 1.0 : 0.0) + rt - 0.5) * 2.0;
}

const char* waveTypeName (WaveType type)
{
    for (const auto& t : kTypeNames)
        if (t.type == type)
            return t.name;
    return "none";
}

WaveType waveTypeFromName (const std::string& name)
{
    for (const auto& t : kTypeNames)
        if (name == t.name)
            return t.type;
    return WaveType::None;
}

int numSamplesForDuration (double durationSec)
{
    return (int) std::round (durationSec * kSampleRateHz);
}

void renderWaveformRange (const ChannelWaveform& wave, int firstSample, int endSample, double* out)
{
    if (endSample > firstSample)
        renderRangeDouble (wave, firstSample, endSample, out);
}

std::uint8_t toDacCurrent (double microamps)
{
    return toDacValue (microamps);
}

void renderChannel (const ChannelWaveform& wave, int numSamples, std::uint8_t* out)
{
    std::vector<double> y ((std::size_t) numSamples);
    renderWaveformRange (wave, 0, numSamples, y.data());

    for (int i = 0; i < numSamples; ++i)
        out[i] = toDacValue (y[i]);
}

bool renderPattern (const std::array<ChannelWaveform, kNumLeds>& waves,
                    double durationSec,
                    PatternData& out,
                    std::string& error)
{
    if (std::isnan (durationSec) || durationSec <= 0)
    {
        error = "Invalid duration; must be positive.";
        return false;
    }

    out.numSamples = numSamplesForDuration (durationSec);
    out.current.assign ((std::size_t) kNumLeds * (std::size_t) out.numSamples, 0);

    for (int led = 0; led < kNumLeds; ++led)
        renderChannel (waves[led], out.numSamples, out.led (led));

    return true;
}

const char* probeMappingName (ProbeMapping mapping)
{
    switch (mapping)
    {
        case ProbeMapping::FourShankChronic: return "4 Shank Chronic";
        case ProbeMapping::TwoShankChronic: return "2 Shank Chronic";
        case ProbeMapping::FourShankAcute: return "4 Shank Acute";
        case ProbeMapping::DemoBoard: return "Demo Board";
    }
    return "";
}

bool isProbeMappingVerified (ProbeMapping mapping)
{
    // 2-shank chronic has not been checked on a real device yet
    return mapping != ProbeMapping::TwoShankChronic;
}

const std::array<int, kNumLeds>& reorderForMapping (ProbeMapping mapping)
{
    switch (mapping)
    {
        case ProbeMapping::TwoShankChronic: return kTwoShankChronic;
        case ProbeMapping::FourShankAcute: return kFourShankAcute;
        case ProbeMapping::DemoBoard: return kDemoBoard;
        case ProbeMapping::FourShankChronic: break;
    }
    return kFourShankChronic;
}

std::vector<std::uint8_t> encodePatternFile (const PatternData& pattern, ProbeMapping mapping)
{
    const auto& reorder = reorderForMapping (mapping);

    // pad with zero samples to a whole number of 256-byte pages
    const int remainder = pattern.numSamples % kSamplesPerPage;
    const int paddedSamples = pattern.numSamples + (remainder > 0 ? kSamplesPerPage - remainder : 0);

    std::vector<std::uint8_t> file ((std::size_t) paddedSamples * kBytesPerFrame, 0);

    // one 64-byte frame per sample; slot i carries GUI channel reorder[i]
    for (int slot = 0; slot < kNumLeds; ++slot)
    {
        const std::uint8_t* src = pattern.led (reorder[slot] - 1);

        for (int s = 0; s < pattern.numSamples; ++s)
            file[(std::size_t) s * kBytesPerFrame + (std::size_t) slot] = src[s];
    }

    return file;
}

} // namespace PatternGen
