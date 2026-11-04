/*
    ------------------------------------------------------------------

    HectoSTAR pattern design. See PatternModel.h.

    ------------------------------------------------------------------
*/

#include "PatternModel.h"
#include "Formula.h"
#include "Json.h"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <locale>
#include <memory>
#include <set>
#include <sstream>

namespace PatternGen
{

namespace
{

constexpr const char* kFormatName = "hectostar-pattern";
constexpr int kFormatVersion = 1;

/** Index of the first sample whose time is >= time (numSamples if none). */
int firstSampleAtOrAfter (double time, int numSamples)
{
    if (std::isnan (time) || time == INFINITY)
        return numSamples;
    if (time <= 0)
        return 0;

    const double guess = std::ceil (time * kSampleRateHz);
    if (guess >= (double) numSamples)
        return numSamples;

    int i = (int) guess;
    while (i > 0 && sampleTimeSec (i - 1) >= time)
        --i;
    while (i < numSamples && sampleTimeSec (i) < time)
        ++i;
    return i;
}

/** Sample range [first, end) a block plays in. */
void blockWindow (const Block& b, int numSamples, int& first, int& end)
{
    first = firstSampleAtOrAfter (b.start, numSamples);

    if (! (b.length > 0))
        end = first; // empty (also NaN)
    else if (std::isinf (b.length))
        end = numSamples;
    else
        end = firstSampleAtOrAfter (b.start + b.length, numSamples);

    end = std::max (end, first);
}

double evalBreakpoints (const std::vector<Breakpoint>& p, Interpolation interp, bool repeat, double t)
{
    if (p.empty())
        return 0;

    double tt = t;

    if (repeat)
    {
        const double period = p.back().time;
        if (! (period > 0))
            return p.back().value;
        tt = mod (t, period);
    }
    else if (tt < p.front().time || tt > p.back().time)
    {
        return 0;
    }

    if (tt <= p.front().time)
        return p.front().value;
    if (tt >= p.back().time)
        return p.back().value;

    // p[k].time <= tt < p[k + 1].time
    const auto next = std::upper_bound (p.begin(), p.end(), tt, [] (double v, const Breakpoint& bp)
                                        { return v < bp.time; });
    const Breakpoint& b = *next;
    const Breakpoint& a = *(next - 1);

    const double span = b.time - a.time;
    if (! (span > 0))
        return b.value;

    const double u = (tt - a.time) / span;

    switch (interp)
    {
        case Interpolation::Step: return a.value;
        case Interpolation::Linear: return a.value + (b.value - a.value) * u;
        case Interpolation::Smooth: return a.value + (b.value - a.value) * (1.0 - std::cos (kPi * u)) / 2.0;
    }

    return a.value;
}

std::string blockName (const Block& b)
{
    return "Block " + std::to_string (b.id) + " (LED " + std::to_string (b.led) + ")";
}

std::string formatSeconds (double s)
{
    std::ostringstream os;
    os.imbue (std::locale::classic());
    os << std::fixed << std::setprecision (2) << s << " s";
    return os.str();
}

bool parseProbe (const std::string& name, ProbeMapping& probe)
{
    for (auto m : { ProbeMapping::FourShankChronic, ProbeMapping::TwoShankChronic, ProbeMapping::FourShankAcute, ProbeMapping::DemoBoard })
    {
        if (name == probeMappingName (m))
        {
            probe = m;
            return true;
        }
    }
    return false;
}

bool parseWaveType (const std::string& name, WaveType& type)
{
    type = waveTypeFromName (name);
    return type != WaveType::None || name == "none";
}

json::Value number (double v) { return json::Value::makeNumber (v); }

std::string formatNumberPlain (double v)
{
    std::ostringstream os;
    os.imbue (std::locale::classic());
    os << std::setprecision (6) << v;
    return os.str();
}

std::string formatMs (double s)
{
    return formatNumberPlain (s * 1000.0) + " ms";
}

} // namespace

const char* waveKindName (WaveKind kind)
{
    switch (kind)
    {
        case WaveKind::Shaped: return "shaped";
        case WaveKind::Builtin: return "builtin";
        case WaveKind::Breakpoints: return "breakpoints";
        case WaveKind::Samples: return "samples";
        case WaveKind::Formula: return "formula";
    }
    return "builtin";
}

const char* interpolationName (Interpolation i)
{
    switch (i)
    {
        case Interpolation::Step: return "step";
        case Interpolation::Linear: return "linear";
        case Interpolation::Smooth: return "smooth";
    }
    return "linear";
}

double pulseShapeValue (PulseShape shape, double x)
{
    x = std::min (1.0, std::max (0.0, x));

    auto gauss = [] (double d, double sigma) { return std::exp (-(d * d) / (2.0 * sigma * sigma)); };

    switch (shape)
    {
        case PulseShape::Square: return 1.0;
        case PulseShape::RampUp: return x;
        case PulseShape::RampDown: return 1.0 - x;
        case PulseShape::Triangle: return x < 0.5 ? 2.0 * x : 2.0 * (1.0 - x);
        case PulseShape::RampUpHold: return x < 0.5 ? 2.0 * x : 1.0;
        case PulseShape::HoldRampDown: return x < 0.5 ? 1.0 : 2.0 * (1.0 - x);
        case PulseShape::Gaussian: return gauss (x - 0.5, 1.0 / 7.0);
        case PulseShape::RisingGaussian: return gauss (x - 1.0, 1.0 / 3.5);
        case PulseShape::FallingGaussian: return gauss (x, 1.0 / 3.5);
        case PulseShape::HalfSine: return std::sin (kPi * x);
        case PulseShape::RisingSine: return std::sin (0.5 * kPi * x);
        case PulseShape::FallingSine: return std::cos (0.5 * kPi * x);
        case PulseShape::SineCycle: return 0.5 * (1.0 - std::cos (kTwoPi * x));
    }
    return 0.0;
}

namespace
{

/** x for sample j of an n-sample pulse: the middle of the sample, so shapes
    are symmetric in samples and a one-sample pulse still has a value. A
    rising shape ends at its peak and a falling one starts at it. */
double pulseX (PulseShape shape, int j, int n)
{
    if (n <= 1)
        return 0.5;
    switch (shape)
    {
        case PulseShape::RampUp:
        case PulseShape::RisingGaussian:
        case PulseShape::RisingSine:
            return (double) (j + 1) / n;
        case PulseShape::RampDown:
        case PulseShape::FallingGaussian:
        case PulseShape::FallingSine:
            return (double) j / n;
        default:
            return (j + 0.5) / n;
    }
}

struct ShapeId
{
    PulseShape shape;
    const char* id;
};

const ShapeId kShapeIds[] = {
    { PulseShape::Square, "square" },
    { PulseShape::RampUp, "ramp-up" },
    { PulseShape::RampDown, "ramp-down" },
    { PulseShape::Triangle, "triangle" },
    { PulseShape::RampUpHold, "ramp-up-hold" },
    { PulseShape::HoldRampDown, "hold-ramp-down" },
    { PulseShape::Gaussian, "gaussian" },
    { PulseShape::RisingGaussian, "rising-gaussian" },
    { PulseShape::FallingGaussian, "falling-gaussian" },
    { PulseShape::HalfSine, "half-sine" },
    { PulseShape::RisingSine, "rising-sine" },
    { PulseShape::FallingSine, "falling-sine" },
    { PulseShape::SineCycle, "sine-cycle" },
};

} // namespace

double pulseShapeMean (PulseShape shape, int n)
{
    n = std::max (1, n);
    double sum = 0;
    for (int j = 0; j < n; ++j)
        sum += pulseShapeValue (shape, pulseX (shape, j, n));
    return sum / n;
}

const char* pulseShapeId (PulseShape shape)
{
    for (const auto& s : kShapeIds)
        if (s.shape == shape)
            return s.id;
    return "square";
}

bool pulseShapeFromId (const std::string& id, PulseShape& shape)
{
    for (const auto& s : kShapeIds)
    {
        if (id == s.id)
        {
            shape = s.shape;
            return true;
        }
    }
    return false;
}

int shapedPulseSamples (const ShapedTrain& t)
{
    if (! std::isfinite (t.width) || t.width <= 0)
        return 1;
    return std::max (1, (int) std::llround (std::min (t.width, 3600.0) * kSampleRateHz));
}

int shapedCount (const ShapedTrain& t)
{
    if (! std::isfinite (t.count))
        return 1;
    return (int) std::max (1.0, std::min (1.0e7, std::round (t.count)));
}

int shapedPeriodSamples (const ShapedTrain& t)
{
    if (shapedCount (t) <= 1 || t.shape == PulseShape::SineCycle || ! std::isfinite (t.period) || t.period <= 0)
        return shapedPulseSamples (t);
    return std::max (1, (int) std::llround (std::min (t.period, 3600.0) * kSampleRateHz));
}

double shapedMaxPeak (const ShapedTrain& t)
{
    const double last = t.amp + (shapedCount (t) - 1) * t.step;
    const double highest = std::max (t.amp, last);
    return t.equalCharge ? highest / pulseShapeMean (t.shape, shapedPulseSamples (t)) : highest;
}

std::string pulseShapeMaths (PulseShape shape)
{
    switch (shape)
    {
        case PulseShape::Square: return "1";
        case PulseShape::RampUp: return "x";
        case PulseShape::RampDown: return "1 - x";
        case PulseShape::Triangle: return "2x for x < 0.5, else 2(1 - x)";
        case PulseShape::RampUpHold: return "2x for x < 0.5, else 1";
        case PulseShape::HoldRampDown: return "1 for x < 0.5, else 2(1 - x)";
        case PulseShape::Gaussian: return "exp(-(x - 0.5)^2 / (2 s^2)),  s = 1/7 of the width";
        case PulseShape::RisingGaussian: return "exp(-(x - 1)^2 / (2 s^2)),  s = 1/3.5 of the width (peak at the end)";
        case PulseShape::FallingGaussian: return "exp(-x^2 / (2 s^2)),  s = 1/3.5 of the width (peak at the start)";
        case PulseShape::HalfSine: return "sin(pi x)";
        case PulseShape::RisingSine: return "sin(pi x / 2)";
        case PulseShape::FallingSine: return "cos(pi x / 2)";
        case PulseShape::SineCycle: return "(1 - cos(2 pi x)) / 2";
    }
    return "1";
}

std::string pulseShapePaperName (PulseShape shape)
{
    switch (shape)
    {
        case PulseShape::Square: return "square";
        case PulseShape::RampUp: return "left-triangular";
        case PulseShape::RampDown: return "right-triangular";
        case PulseShape::Triangle: return "triangular";
        case PulseShape::RampUpHold: return "backward-ramp";
        case PulseShape::HoldRampDown: return "forward-ramp";
        case PulseShape::Gaussian: return "Gaussian";
        case PulseShape::RisingGaussian: return "left-Gaussian";
        case PulseShape::FallingGaussian: return "right-Gaussian";
        case PulseShape::HalfSine: return "positive-sinusoidal";
        case PulseShape::RisingSine: return "left-positive-sinusoidal";
        case PulseShape::FallingSine: return "right-positive-sinusoidal";
        case PulseShape::SineCycle: return "";
    }
    return "";
}

std::string shapedTrainFormula (const ShapedTrain& t)
{
    auto num = [] (double v)
    {
        std::ostringstream os;
        os.imbue (std::locale::classic());
        os << std::setprecision (17) << v;
        return os.str();
    };

    const int width = shapedPulseSamples (t);
    const int period = shapedPeriodSamples (t);
    const int pulseLen = std::min (width, period);
    const int count = shapedCount (t);
    const double scale = t.equalCharge ? 1.0 / pulseShapeMean (t.shape, width) : 1.0;

    // sample index from the block start (the epsilon guards t*fs landing just below a whole number)
    const std::string k = "floor(t*fs + 0.000001)";
    const std::string j = "mod(" + k + ", " + std::to_string (period) + ")";
    const std::string N = std::to_string (width);

    // x across the pulse, sampled as the renderer does (pulseX)
    std::string x;
    switch (t.shape)
    {
        case PulseShape::RampUp:
        case PulseShape::RisingGaussian:
        case PulseShape::RisingSine:
            x = width <= 1 ? "0.5" : "((" + j + " + 1)/" + N + ")";
            break;
        case PulseShape::RampDown:
        case PulseShape::FallingGaussian:
        case PulseShape::FallingSine:
            x = width <= 1 ? "0.5" : "(" + j + "/" + N + ")";
            break;
        default:
            x = width <= 1 ? "0.5" : "((" + j + " + 0.5)/" + N + ")";
            break;
    }

    std::string f;
    switch (t.shape)
    {
        case PulseShape::Square: f = "1"; break;
        case PulseShape::RampUp: f = x; break;
        case PulseShape::RampDown: f = "(1 - " + x + ")"; break;
        case PulseShape::Triangle: f = "if(" + x + " < 0.5, 2*" + x + ", 2*(1 - " + x + "))"; break;
        case PulseShape::RampUpHold: f = "if(" + x + " < 0.5, 2*" + x + ", 1)"; break;
        case PulseShape::HoldRampDown: f = "if(" + x + " < 0.5, 1, 2*(1 - " + x + "))"; break;
        case PulseShape::Gaussian: f = "exp(-((" + x + " - 0.5)*(" + x + " - 0.5))/(2*(1/7)*(1/7)))"; break;
        case PulseShape::RisingGaussian: f = "exp(-((" + x + " - 1)*(" + x + " - 1))/(2*(1/3.5)*(1/3.5)))"; break;
        case PulseShape::FallingGaussian: f = "exp(-(" + x + "*" + x + ")/(2*(1/3.5)*(1/3.5)))"; break;
        case PulseShape::HalfSine: f = "sin(pi*" + x + ")"; break;
        case PulseShape::RisingSine: f = "sin(0.5*pi*" + x + ")"; break;
        case PulseShape::FallingSine: f = "cos(0.5*pi*" + x + ")"; break;
        case PulseShape::SineCycle: f = "0.5*(1 - cos(2*pi*" + x + "))"; break;
    }

    std::string amp = num (t.amp);
    if (t.step != 0)
        amp = "max(0, " + num (t.amp) + " + floor(" + k + "/" + std::to_string (period) + ")*" + num (t.step) + ")";
    else if (t.amp < 0)
        amp = "0";

    std::string value = amp + "*(" + f + (scale != 1.0 ? "*" + num (scale) : "") + ")";

    return "if(" + k + " < " + std::to_string ((long long) count * period) + " && " + j + " < " + std::to_string (pulseLen) + ", " + value + ", 0)";
}

namespace
{

/** Renders n samples of a shaped train from its first sample. */
void renderShaped (const ShapedTrain& t, int n, double* out)
{
    const int width = shapedPulseSamples (t);
    const int period = shapedPeriodSamples (t);
    const int pulseLen = std::min (width, period);
    const int count = shapedCount (t);
    const double scale = t.equalCharge ? 1.0 / pulseShapeMean (t.shape, width) : 1.0;

    // the shape of one pulse, once
    std::vector<double> shape ((std::size_t) pulseLen);
    for (int j = 0; j < pulseLen; ++j)
        shape[(std::size_t) j] = pulseShapeValue (t.shape, pulseX (t.shape, j, width)) * scale;

    for (int k = 0; k < n; ++k)
    {
        const int pulse = k / period;
        const int j = k - pulse * period;
        if (pulse >= count || j >= pulseLen)
        {
            out[k] = 0.0;
            continue;
        }
        const double amp = std::max (0.0, t.amp + pulse * t.step);
        out[k] = amp * shape[(std::size_t) j];
    }
}

} // namespace

double naturalLength (const WaveSpec& wave)
{
    switch (wave.kind)
    {
        case WaveKind::Shaped:
        {
            const ShapedTrain& t = wave.shaped;
            const int last = (shapedCount (t) - 1) * shapedPeriodSamples (t) + std::min (shapedPulseSamples (t), shapedPeriodSamples (t));
            return (double) last / kSampleRateHz;
        }
        case WaveKind::Builtin:
        {
            const ChannelWaveform& w = wave.builtin;
            const double count = std::round (w.freq);

            switch (w.type)
            {
                case WaveType::FixedPulse:
                case WaveType::PulseStaircase:
                case WaveType::FixedSawtooth:
                case WaveType::FixedSine:
                    return (count > 0 && w.offset > 0 && std::isfinite (w.offset)) ? count * w.offset : 0.0;
                case WaveType::Wavelet:
                    return (w.offset > 0 && std::isfinite (w.offset)) ? w.offset : 0.0;
                default:
                    return 0.0;
            }
        }
        case WaveKind::Breakpoints:
            return (! wave.repeat && ! wave.points.empty()) ? std::max (0.0, wave.points.back().time) : 0.0;
        case WaveKind::Samples:
            return wave.repeat ? 0.0 : (double) wave.samples.size() / kSampleRateHz;
        case WaveKind::Formula:
            return 0.0;
    }
    return 0.0;
}

std::vector<int> activeLeds (const Pattern& pattern)
{
    std::set<int> leds;
    for (const auto& b : pattern.blocks)
        if (b.led >= 1 && b.led <= kNumLeds)
            leds.insert (b.led);
    return std::vector<int> (leds.begin(), leds.end());
}

int nextBlockId (const Pattern& pattern)
{
    int id = 0;
    for (const auto& b : pattern.blocks)
        id = std::max (id, b.id);
    return id + 1;
}

std::size_t encodedSizeBytes (const Pattern& pattern)
{
    if (! (pattern.duration > 0) || ! std::isfinite (pattern.duration))
        return 0;
    const double n = std::round (pattern.duration * kSampleRateHz);
    const double padded = std::ceil (n / kSamplesPerPage) * kSamplesPerPage;
    return (std::size_t) padded * kBytesPerFrame;
}

bool fitsInFlash (const Pattern& pattern)
{
    return encodedSizeBytes (pattern) <= kFlashCapacityBytes;
}

std::vector<std::string> validate (const Pattern& pattern)
{
    std::vector<std::string> problems;

    if (! (pattern.duration > 0) || ! std::isfinite (pattern.duration))
        problems.push_back ("The duration must be a positive number of seconds.");
    else if (! fitsInFlash (pattern))
        problems.push_back ("The pattern is " + formatSeconds (pattern.duration) + " long; the flash holds at most "
                            + formatSeconds ((double) kMaxSamples / kSampleRateHz) + ".");

    if (pattern.repeatCount < 1)
        problems.push_back ("The repeat count must be at least 1.");

    std::set<int> ids;
    for (const auto& b : pattern.blocks)
    {
        if (! ids.insert (b.id).second)
            problems.push_back (blockName (b) + ": duplicate block id.");
        if (b.led < 1 || b.led > kNumLeds)
            problems.push_back (blockName (b) + ": LED must be 1-64.");
        if (std::isnan (b.start) || std::isnan (b.length) || b.length < 0)
            problems.push_back (blockName (b) + ": start and length must be valid times.");

        switch (b.wave.kind)
        {
            case WaveKind::Formula:
            {
                std::string error;
                if (Formula::compile (b.wave.formula, error) == nullptr)
                    problems.push_back (blockName (b) + ": formula error: " + error + ".");
                break;
            }
            case WaveKind::Breakpoints:
                if (b.wave.points.empty())
                    problems.push_back (blockName (b) + ": add at least one point.");
                for (std::size_t i = 1; i < b.wave.points.size(); ++i)
                {
                    if (b.wave.points[i].time < b.wave.points[i - 1].time)
                    {
                        problems.push_back (blockName (b) + ": point times must increase.");
                        break;
                    }
                }
                break;
            case WaveKind::Samples:
                if (b.wave.samples.empty())
                    problems.push_back (blockName (b) + ": no samples.");
                break;
            case WaveKind::Shaped:
            {
                const ShapedTrain& t = b.wave.shaped;
                if (! std::isfinite (t.amp) || ! std::isfinite (t.width) || t.width <= 0 || ! std::isfinite (t.count) || t.count < 1
                    || ! std::isfinite (t.step))
                    problems.push_back (blockName (b) + ": amplitude, width and count must be valid numbers (width above 0, count 1 or more).");
                else if (shapedCount (t) > 1 && t.shape != PulseShape::SineCycle && (! std::isfinite (t.period) || t.period <= 0))
                    problems.push_back (blockName (b) + ": the period must be above 0 ms.");
                else if (shapedCount (t) > 1 && shapedPulseSamples (t) > shapedPeriodSamples (t))
                    problems.push_back (blockName (b) + ": the pulse width (" + formatMs (t.width) + ") is longer than the period ("
                                        + formatMs (t.period) + ").");
                else if (shapedMaxPeak (t) > kMaxCurrentUa + 0.5)
                    problems.push_back (blockName (b) + ": needs a peak of " + formatNumberPlain (std::round (shapedMaxPeak (t)))
                                        + " uA, above the 100 uA limit" + (t.equalCharge ? " (to match the square-pulse charge)." : "."));
                break;
            }
            case WaveKind::Builtin:
                break;
        }
    }

    return problems;
}

bool renderPattern (const Pattern& pattern, PatternData& out, std::string& error)
{
    if (std::isnan (pattern.duration) || pattern.duration <= 0)
    {
        error = "Invalid duration; must be positive.";
        return false;
    }

    const int numSamples = numSamplesForDuration (pattern.duration);
    PatternData result;
    result.numSamples = numSamples;
    result.current.assign ((std::size_t) kNumLeds * (std::size_t) numSamples, 0);

    std::vector<double> buf;

    for (const Block& b : pattern.blocks)
    {
        if (b.led < 1 || b.led > kNumLeds)
        {
            error = blockName (b) + ": LED must be 1-64.";
            return false;
        }

        int first, end;
        blockWindow (b, numSamples, first, end);
        if (end <= first)
            continue;

        const int n = end - first;
        buf.assign ((std::size_t) n, 0.0);

        switch (b.wave.kind)
        {
            case WaveKind::Shaped:
            {
                // the train starts at the block's first sample
                renderShaped (b.wave.shaped, n, buf.data());
                break;
            }

            case WaveKind::Builtin:
            {
                ChannelWaveform cfg = b.wave.builtin;
                cfg.delay = b.start; // MATLAB's delay is the block start
                renderWaveformRange (cfg, first, end, buf.data());
                break;
            }

            case WaveKind::Breakpoints:
            {
                std::vector<Breakpoint> points = b.wave.points;
                std::stable_sort (points.begin(), points.end(), [] (const Breakpoint& x, const Breakpoint& y)
                                  { return x.time < y.time; });
                for (int k = 0; k < n; ++k)
                    buf[k] = evalBreakpoints (points, b.wave.interpolation, b.wave.repeat, sampleTimeSec (first + k) - b.start);
                break;
            }

            case WaveKind::Samples:
            {
                const auto& s = b.wave.samples;
                if (s.empty())
                    break;
                for (int k = 0; k < n; ++k)
                {
                    if (b.wave.repeat)
                        buf[k] = s[(std::size_t) k % s.size()];
                    else if ((std::size_t) k < s.size())
                        buf[k] = s[(std::size_t) k];
                }
                break;
            }

            case WaveKind::Formula:
            {
                std::string formulaError;
                const auto f = Formula::compile (b.wave.formula, formulaError);
                if (f == nullptr)
                {
                    error = blockName (b) + ": formula error: " + formulaError + ".";
                    return false;
                }
                for (int k = 0; k < n; ++k)
                    buf[k] = f->evaluate (sampleTimeSec (first + k) - b.start, b.length);
                break;
            }
        }

        // linear fades; skipped entirely when unused so values stay exact
        if (b.fadeIn > 0 || (b.fadeOut > 0 && std::isfinite (b.length)))
        {
            for (int k = 0; k < n; ++k)
            {
                const double t = sampleTimeSec (first + k) - b.start;
                double gain = 1.0;
                if (b.fadeIn > 0 && t < b.fadeIn)
                    gain *= std::max (0.0, t / b.fadeIn);
                if (b.fadeOut > 0 && std::isfinite (b.length) && b.length - t < b.fadeOut)
                    gain *= std::max (0.0, (b.length - t) / b.fadeOut);
                buf[k] *= gain;
            }
        }

        // overlapping blocks: the higher current wins
        std::uint8_t* lane = result.led (b.led - 1) + first;
        for (int k = 0; k < n; ++k)
            lane[k] = std::max (lane[k], toDacCurrent (buf[k]));
    }

    out = std::move (result);
    return true;
}

bool decodePatternFile (const std::vector<std::uint8_t>& bytes, ProbeMapping mapping, Pattern& pattern, std::string& error)
{
    if (bytes.empty() || bytes.size() % kBytesPerFrame != 0)
    {
        error = "Not a pattern file: the size must be a whole number of 64-byte frames.";
        return false;
    }

    const std::size_t numSamples = bytes.size() / kBytesPerFrame;
    if (numSamples > kMaxSamples)
    {
        error = "The file is larger than the board's 16 MB flash.";
        return false;
    }

    const auto& reorder = reorderForMapping (mapping);

    Pattern p;
    p.probe = mapping;
    p.duration = (double) numSamples / kSampleRateHz;

    for (int slot = 0; slot < kNumLeds; ++slot)
    {
        const int led = reorder[(std::size_t) slot];

        std::size_t first = numSamples, last = 0;
        for (std::size_t s = 0; s < numSamples; ++s)
        {
            if (bytes[s * kBytesPerFrame + (std::size_t) slot] != 0)
            {
                first = std::min (first, s);
                last = s;
            }
        }
        if (first == numSamples)
            continue; // never on

        Block b;
        b.id = nextBlockId (p);
        b.led = led;
        b.start = (double) first / kSampleRateHz;
        b.wave.kind = WaveKind::Samples;
        b.wave.samples.reserve (last - first + 1);
        for (std::size_t s = first; s <= last; ++s)
            b.wave.samples.push_back (bytes[s * kBytesPerFrame + (std::size_t) slot]);
        b.length = (double) b.wave.samples.size() / kSampleRateHz;
        p.blocks.push_back (std::move (b));
    }

    std::sort (p.blocks.begin(), p.blocks.end(), [] (const Block& a, const Block& b) { return a.led < b.led; });
    for (std::size_t i = 0; i < p.blocks.size(); ++i)
        p.blocks[i].id = (int) i + 1;

    pattern = std::move (p);
    return true;
}

std::string toJson (const Pattern& pattern)
{
    json::Value root = json::Value::makeObject();
    root.set ("format", json::Value::makeString (kFormatName));
    root.set ("version", number (kFormatVersion));
    root.set ("duration", number (pattern.duration));
    root.set ("probe", json::Value::makeString (probeMappingName (pattern.probe)));
    root.set ("repeatCount", number (pattern.repeatCount));

    json::Value blocks = json::Value::makeArray();

    for (const Block& b : pattern.blocks)
    {
        json::Value jb = json::Value::makeObject();
        jb.set ("id", number (b.id));
        jb.set ("led", number (b.led));
        jb.set ("start", number (b.start));
        jb.set ("length", number (b.length));
        if (b.link != 0)
            jb.set ("link", number (b.link));
        if (! b.label.empty())
            jb.set ("label", json::Value::makeString (b.label));
        if (b.fadeIn != 0)
            jb.set ("fadeIn", number (b.fadeIn));
        if (b.fadeOut != 0)
            jb.set ("fadeOut", number (b.fadeOut));

        json::Value w = json::Value::makeObject();
        w.set ("kind", json::Value::makeString (waveKindName (b.wave.kind)));

        switch (b.wave.kind)
        {
            case WaveKind::Shaped:
                w.set ("shape", json::Value::makeString (pulseShapeId (b.wave.shaped.shape)));
                w.set ("amp", number (b.wave.shaped.amp));
                w.set ("width", number (b.wave.shaped.width));
                w.set ("count", number (b.wave.shaped.count));
                w.set ("period", number (b.wave.shaped.period));
                if (b.wave.shaped.step != 0)
                    w.set ("step", number (b.wave.shaped.step));
                if (b.wave.shaped.equalCharge)
                    w.set ("equalCharge", json::Value::makeBool (true));
                break;

            case WaveKind::Builtin:
                w.set ("type", json::Value::makeString (waveTypeName (b.wave.builtin.type)));
                w.set ("freq", number (b.wave.builtin.freq));
                w.set ("amp", number (b.wave.builtin.amp));
                w.set ("offset", number (b.wave.builtin.offset));
                w.set ("duty", number (b.wave.builtin.duty));
                w.set ("step", number (b.wave.builtin.step));
                break;

            case WaveKind::Breakpoints:
            {
                w.set ("interpolation", json::Value::makeString (interpolationName (b.wave.interpolation)));
                w.set ("repeat", json::Value::makeBool (b.wave.repeat));
                json::Value pts = json::Value::makeArray();
                for (const auto& p : b.wave.points)
                {
                    json::Value pair = json::Value::makeArray();
                    pair.array.push_back (number (p.time));
                    pair.array.push_back (number (p.value));
                    pts.array.push_back (pair);
                }
                w.set ("points", pts);
                break;
            }

            case WaveKind::Samples:
            {
                w.set ("repeat", json::Value::makeBool (b.wave.repeat));
                json::Value vals = json::Value::makeArray();
                for (double v : b.wave.samples)
                    vals.array.push_back (number (v));
                w.set ("values", vals);
                break;
            }

            case WaveKind::Formula:
                w.set ("expression", json::Value::makeString (b.wave.formula));
                break;
        }

        jb.set ("wave", w);
        blocks.array.push_back (jb);
    }

    root.set ("blocks", blocks);
    return json::write (root);
}

bool fromJson (const std::string& text, Pattern& pattern, std::string& error)
{
    json::Value root;
    if (! json::parse (text, root, error))
    {
        error = "Not a valid pattern file: " + error;
        return false;
    }

    if (! root.isObject() || root.getString ("format", "") != kFormatName)
    {
        error = "Not a HectoSTAR pattern file.";
        return false;
    }

    if (root.getNumber ("version", 0) > kFormatVersion)
    {
        error = "This pattern was saved by a newer version of the plugin.";
        return false;
    }

    Pattern p;
    p.duration = root.getNumber ("duration", 1.0);
    p.repeatCount = (int) root.getNumber ("repeatCount", 1);

    if (! parseProbe (root.getString ("probe", probeMappingName (ProbeMapping::FourShankChronic)), p.probe))
    {
        error = "Unknown probe '" + root.getString ("probe", "") + "'.";
        return false;
    }

    const json::Value* blocks = root.get ("blocks");
    if (blocks != nullptr && blocks->isArray())
    {
        for (const json::Value& jb : blocks->array)
        {
            Block b;
            b.id = (int) jb.getNumber ("id", (double) nextBlockId (p));
            b.led = (int) jb.getNumber ("led", 1);
            b.start = jb.getNumber ("start", 0);
            b.length = jb.getNumber ("length", 0);
            b.link = (int) jb.getNumber ("link", 0);
            b.label = jb.getString ("label", "");
            b.fadeIn = jb.getNumber ("fadeIn", 0);
            b.fadeOut = jb.getNumber ("fadeOut", 0);

            const json::Value* w = jb.get ("wave");
            if (w == nullptr || ! w->isObject())
            {
                error = blockName (b) + ": missing waveform.";
                return false;
            }

            const std::string kind = w->getString ("kind", "builtin");

            if (kind == "shaped")
            {
                b.wave.kind = WaveKind::Shaped;
                const std::string shape = w->getString ("shape", "square");
                if (! pulseShapeFromId (shape, b.wave.shaped.shape))
                {
                    error = blockName (b) + ": unknown pulse shape '" + shape + "'.";
                    return false;
                }
                b.wave.shaped.amp = w->getNumber ("amp", 0);
                b.wave.shaped.width = w->getNumber ("width", 0.005);
                b.wave.shaped.count = w->getNumber ("count", 1);
                if (w->get ("period") != nullptr)
                    b.wave.shaped.period = w->getNumber ("period", 0.1);
                else if (w->getNumber ("rate", 0) > 0) // early designs saved a rate in Hz
                    b.wave.shaped.period = 1.0 / w->getNumber ("rate", 10);
                b.wave.shaped.step = w->getNumber ("step", 0);
                b.wave.shaped.equalCharge = w->getBool ("equalCharge", false);
            }
            else if (kind == "builtin")
            {
                b.wave.kind = WaveKind::Builtin;
                const std::string type = w->getString ("type", "none");
                if (! parseWaveType (type, b.wave.builtin.type))
                {
                    error = blockName (b) + ": unknown waveform type '" + type + "'.";
                    return false;
                }
                b.wave.builtin.freq = w->getNumber ("freq", 0);
                b.wave.builtin.amp = w->getNumber ("amp", 0);
                b.wave.builtin.offset = w->getNumber ("offset", 0);
                b.wave.builtin.duty = w->getNumber ("duty", 0);
                b.wave.builtin.step = w->getNumber ("step", 0);
            }
            else if (kind == "breakpoints")
            {
                b.wave.kind = WaveKind::Breakpoints;
                const std::string interp = w->getString ("interpolation", "linear");
                if (interp == "step")
                    b.wave.interpolation = Interpolation::Step;
                else if (interp == "smooth")
                    b.wave.interpolation = Interpolation::Smooth;
                else
                    b.wave.interpolation = Interpolation::Linear;
                b.wave.repeat = w->getBool ("repeat", false);

                const json::Value* pts = w->get ("points");
                if (pts != nullptr && pts->isArray())
                {
                    for (const json::Value& pair : pts->array)
                    {
                        if (! pair.isArray() || pair.array.size() != 2 || ! pair.array[0].isNumber() || ! pair.array[1].isNumber())
                        {
                            error = blockName (b) + ": each point must be [time, value].";
                            return false;
                        }
                        b.wave.points.push_back ({ pair.array[0].number, pair.array[1].number });
                    }
                }
            }
            else if (kind == "samples")
            {
                b.wave.kind = WaveKind::Samples;
                b.wave.repeat = w->getBool ("repeat", false);
                const json::Value* vals = w->get ("values");
                if (vals != nullptr && vals->isArray())
                    for (const json::Value& v : vals->array)
                        b.wave.samples.push_back (v.isNumber() ? v.number : NAN);
            }
            else if (kind == "formula")
            {
                b.wave.kind = WaveKind::Formula;
                b.wave.formula = w->getString ("expression", "");
            }
            else
            {
                error = blockName (b) + ": unknown waveform kind '" + kind + "'.";
                return false;
            }

            p.blocks.push_back (std::move (b));
        }
    }

    pattern = std::move (p);
    return true;
}

} // namespace PatternGen
