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

} // namespace

const char* waveKindName (WaveKind kind)
{
    switch (kind)
    {
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

double naturalLength (const WaveSpec& wave)
{
    switch (wave.kind)
    {
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

            if (kind == "builtin")
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
