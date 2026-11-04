/*
    ------------------------------------------------------------------

    Pattern editor waveform list. See WaveformCatalog.h.

    ------------------------------------------------------------------
*/

#include "WaveformCatalog.h"

#include <algorithm>
#include <cmath>

namespace PatternEditor
{

const String kMicroAmps = String::fromUTF8 ("\xc2\xb5" "A");

namespace
{

std::string micro (const char* s)
{
    return String (s).replace ("uA", kMicroAmps).toStdString();
}

WaveSpec builtin (WaveType type, double freq, double amp, double offset, double duty, double step = 0)
{
    WaveSpec w;
    w.kind = WaveKind::Builtin;
    w.builtin.type = type;
    w.builtin.freq = freq;
    w.builtin.amp = amp;
    w.builtin.offset = offset;
    w.builtin.duty = duty;
    w.builtin.step = step;
    return w;
}

WaveSpec shaped (PulseShape shape, double amp, double widthMs, int count, double rate, double step = 0, bool equalCharge = false)
{
    WaveSpec w;
    w.kind = WaveKind::Shaped;
    w.shaped.shape = shape;
    w.shaped.amp = amp;
    w.shaped.width = widthMs / 1000.0;
    w.shaped.count = count;
    w.shaped.rate = rate;
    w.shaped.step = step;
    w.shaped.equalCharge = equalCharge;
    return w;
}

struct ShapeInfo
{
    PulseShape shape;
    const char* name;
    const char* guide;
};

// names in quotes are those of Bansal, Pyari and Roy 2023 (Photonics 10:571, Table 6)
const ShapeInfo kShapeInfo[] = {
    { PulseShape::Square, "Square pulses", "Constant current for the pulse width (\"square\")." },
    { PulseShape::RampUp, "Ramp up", "Rises linearly from 0 to the peak, then stops (\"left-triangular\")." },
    { PulseShape::RampDown, "Ramp down", "Starts at the peak and falls linearly to 0 (\"right-triangular\")." },
    { PulseShape::Triangle, "Triangle", "Rises to the peak at mid-pulse, then falls back to 0 (\"triangular\")." },
    { PulseShape::RampUpHold, "Ramp up, then hold", "Rises over the first half, holds the peak for the second (\"backward-ramp\")." },
    { PulseShape::HoldRampDown, "Hold, then ramp down", "Holds the peak for the first half, falls to 0 over the second (\"forward-ramp\")." },
    { PulseShape::Gaussian, "Gaussian", "Bell-shaped, peak at mid-pulse, sigma = width / 7 (\"Gaussian\")." },
    { PulseShape::RisingGaussian, "Rising half-Gaussian", "Slow Gaussian rise to the peak at the end of the pulse, sigma = width / 3.5 (\"left-Gaussian\")." },
    { PulseShape::FallingGaussian, "Falling half-Gaussian", "Starts at the peak and decays like a Gaussian, sigma = width / 3.5 (\"right-Gaussian\")." },
    { PulseShape::HalfSine, "Half-sine", "One positive half of a sine: 0, up to the peak, back to 0 (\"positive-sinusoidal\")." },
    { PulseShape::RisingSine, "Rising quarter-sine", "Quarter sine from 0 up to the peak at the end (\"left-positive-sinusoidal\")." },
    { PulseShape::FallingSine, "Falling quarter-sine", "Quarter sine from the peak down to 0 (\"right-positive-sinusoidal\")." },
    { PulseShape::SineCycle, "Sine cycles", "Whole sine cycles between 0 and the peak. Set the cycle length to 1 / rate for a continuous sine." },
};

WaveSpec points (std::vector<Breakpoint> p, bool repeat)
{
    WaveSpec w;
    w.kind = WaveKind::Breakpoints;
    w.interpolation = Interpolation::Linear;
    w.repeat = repeat;
    w.points = std::move (p);
    return w;
}

WaveSpec formula (const char* expr)
{
    WaveSpec w;
    w.kind = WaveKind::Formula;
    w.formula = expr;
    return w;
}

struct TypeInfo
{
    WaveType type;
    const char* name;
    const char* guide;
};

const TypeInfo kTypeInfo[] = {
    { WaveType::FixedPulse, "Pulse train", "A fixed number of pulses at the amplitude, on for the duty fraction of each period." },
    { WaveType::PulseStaircase, "Pulse staircase", "A pulse train whose amplitude changes by the step on every pulse." },
    { WaveType::Pulse, "Square wave", "Continuous square wave: offset + amplitude, -amplitude outside the duty fraction." },
    { WaveType::Sine, "Sine", "Continuous sine: offset + amplitude x sin(2 pi f t)." },
    { WaveType::FixedSine, "Sine cycles", "A fixed number of sine cycles, each squeezed into the active fraction of its period." },
    { WaveType::DoubleSine, "Double sine", "Baseline + amplitude/2 x (sin f1 + sin f2), continuous." },
    { WaveType::Sawtooth, "Sawtooth", "Continuous sawtooth around the offset." },
    { WaveType::FixedSawtooth, "Ramp train", "A fixed number of ramps from 0 up to the peak; the ramp takes the duty fraction of each period." },
    { WaveType::Wavelet, "Wavelet burst", "A positive-only burst that fits the duration; peaks are rounded up to an odd number." },
    { WaveType::None, "Off", "No current (0 uA)." },
};

} // namespace

const std::vector<WaveChoice>& waveChoices()
{
    using G = WaveChoice::Group;

    static const std::vector<WaveChoice> list = [] {
        std::vector<WaveChoice> l;

        // pulse shapes: pick one, then set amplitude, width, count and rate
        for (const auto& s : kShapeInfo)
        {
            if (s.shape == PulseShape::Square)
                l.push_back ({ G::Shape, s.name, "", shaped (s.shape, 50, 5, 10, 10) });
            else if (s.shape == PulseShape::SineCycle)
                l.push_back ({ G::Shape, s.name, "", shaped (s.shape, 50, 125, 8, 8) });
            else
                l.push_back ({ G::Shape, s.name, "", shaped (s.shape, 50, 10, 10, 10) });
        }

        const std::vector<WaveChoice> rest = {
        // presets: ready-made settings
        { G::Preset, "10 Hz square, 5 ms, 50 uA, 1 s", "10 Hz, 5 ms, 50 uA", shaped (PulseShape::Square, 50, 5, 10, 10) },
        { G::Preset, "20 Hz square, 10 ms, 50 uA, 1 s", "20 Hz, 10 ms, 50 uA", shaped (PulseShape::Square, 50, 10, 20, 20) },
        { G::Preset, "40 Hz square, 2 ms, 50 uA, 0.5 s", "40 Hz, 2 ms, 50 uA", shaped (PulseShape::Square, 50, 2, 20, 40) },
        { G::Preset, "Single 10 ms pulse, 50 uA", "10 ms pulse", shaped (PulseShape::Square, 50, 10, 1, 10) },
        { G::Preset, "Staircase 10 to 80 uA, 8 x 50 ms pulses", "Staircase 10-80 uA", shaped (PulseShape::Square, 10, 50, 8, 10, 10) },
        { G::Preset, "10 Hz Gaussian, 10 ms, charge of a 20 uA square", "Gaussian, 20 uA equiv.", shaped (PulseShape::Gaussian, 20, 10, 10, 10, 0, true) },
        { G::Preset, "Sine 8 Hz, 0 to 100 uA, 1 s", "Sine 8 Hz", shaped (PulseShape::SineCycle, 100, 125, 8, 8) },
        { G::Preset, "Ramp 0 to 100 uA over 100 ms", "Ramp 100 ms", shaped (PulseShape::RampUp, 100, 100, 1, 10) },
        { G::Preset, "Triangle 0-80-0 uA every 20 ms, 1 s", "Triangle 20 ms", shaped (PulseShape::Triangle, 80, 20, 50, 50) },
        { G::Preset, "40 Hz bursts on an 8 Hz sine", "Theta-gamma", formula ("40*(1+sin(2*pi*8*t)) * (1+square(2*pi*40*t, 50))/2") },

        // built-in types of the MATLAB tool, with sensible defaults
        { G::Type, "Pulse train", "", builtin (WaveType::FixedPulse, 10, 50, 0.1, 5) },
        { G::Type, "Pulse staircase", "", builtin (WaveType::PulseStaircase, 8, 10, 0.1, 50, 10) },
        { G::Type, "Square wave", "", builtin (WaveType::Pulse, 10, 25, 25, 50) },
        { G::Type, "Sine", "", builtin (WaveType::Sine, 8, 50, 50, 0) },
        { G::Type, "Sine cycles", "", builtin (WaveType::FixedSine, 5, 50, 0.1, 50) },
        { G::Type, "Double sine", "", builtin (WaveType::DoubleSine, 8, 50, 40, 50) },
        { G::Type, "Sawtooth", "", builtin (WaveType::Sawtooth, 5, 50, 50, 0) },
        { G::Type, "Ramp train", "", builtin (WaveType::FixedSawtooth, 5, 80, 0.1, 50) },
        { G::Type, "Wavelet burst", "", builtin (WaveType::Wavelet, 5, 80, 0.1, 0) },
        { G::Type, "Off", "", builtin (WaveType::None, 0, 0, 0, 0) },

        // custom shapes
        { G::Custom, "Custom: points", "", points ({ { 0, 0 }, { 0.005, 80 }, { 0.01, 0 } }, true) },
        { G::Custom, "Custom: formula", "", formula ("50*(1+sin(2*pi*8*t))") },
        { G::Custom, "Custom: samples from CSV...", "", [] {
             WaveSpec w;
             w.kind = WaveKind::Samples;
             return w;
         }() },
        };

        l.insert (l.end(), rest.begin(), rest.end());
        return l;
    }();

    // labels and names use the micro sign
    static std::vector<WaveChoice> withMicro = [] {
        std::vector<WaveChoice> out = list;
        for (auto& c : out)
        {
            c.name = c.name.replace ("uA", kMicroAmps);
            c.label = micro (c.label.c_str());
        }
        return out;
    }();

    return withMicro;
}

void addWaveChoices (ComboBox& box, bool includePresets)
{
    const auto& list = waveChoices();
    WaveChoice::Group current = WaveChoice::Group::Type;
    bool first = true;

    for (std::size_t i = 0; i < list.size(); ++i)
    {
        const auto& c = list[i];
        if (c.group == WaveChoice::Group::Preset && ! includePresets)
            continue;

        if (first || c.group != current)
        {
            if (! first)
                box.addSeparator();
            const char* heading = c.group == WaveChoice::Group::Shape    ? "Pulse shapes"
                                  : c.group == WaveChoice::Group::Preset ? "Presets"
                                  : c.group == WaveChoice::Group::Type   ? "Classic waveforms (MATLAB tool)"
                                                                         : "Custom";
            box.addSectionHeading (heading);
            current = c.group;
            first = false;
        }

        box.addItem (c.name, (int) i + 1);
    }
}

int choiceIdFor (const WaveSpec& wave)
{
    const auto& list = waveChoices();
    for (std::size_t i = 0; i < list.size(); ++i)
    {
        const auto& c = list[i];
        if (c.group == WaveChoice::Group::Preset || c.wave.kind != wave.kind)
            continue;
        if (wave.kind == WaveKind::Shaped)
        {
            if (c.wave.shaped.shape == wave.shaped.shape)
                return (int) i + 1;
            continue;
        }
        if (wave.kind != WaveKind::Builtin || c.wave.builtin.type == wave.builtin.type)
            return (int) i + 1;
    }
    return 0;
}

bool hasFixedLength (const WaveSpec& wave)
{
    return naturalLength (wave) > 0;
}

void updateFixedLength (Block& b)
{
    const double n = naturalLength (b.wave);
    if (n > 0)
        b.length = n;
}

String typeGuide (WaveType type)
{
    for (const auto& t : kTypeInfo)
        if (t.type == type)
            return String (t.guide).replace ("uA", kMicroAmps);
    return {};
}

String shapeGuide (PulseShape shape)
{
    for (const auto& s : kShapeInfo)
        if (s.shape == shape)
            return String (s.guide) + "  Equal charge scales the peak so each pulse carries the charge of a square pulse at the set amplitude.";
    return {};
}

String shapeName (PulseShape shape)
{
    for (const auto& s : kShapeInfo)
        if (s.shape == shape)
            return s.name;
    return "Pulses";
}

String waveDisplayName (const WaveSpec& wave)
{
    if (wave.kind == WaveKind::Shaped)
        return shapeName (wave.shaped.shape);
    if (wave.kind == WaveKind::Builtin)
    {
        for (const auto& t : kTypeInfo)
            if (t.type == wave.builtin.type)
                return t.name;
        return "Off";
    }
    if (wave.kind == WaveKind::Breakpoints)
        return "Points";
    if (wave.kind == WaveKind::Samples)
        return "Samples";
    return "Formula";
}

void chooseSamplesFile (std::unique_ptr<FileChooser>& holder, std::function<void (std::vector<double>, std::string)> done)
{
    holder = std::make_unique<FileChooser> ("Import samples (" + kMicroAmps + " at 20 kHz)", File(), "*.csv;*.txt");
    holder->launchAsync (FileBrowserComponent::openMode | FileBrowserComponent::canSelectFiles,
                         [done] (const FileChooser& fc)
                         {
                             const File f = fc.getResult();
                             if (! f.existsAsFile())
                                 return;

                             // every number in the file, in order (headers and other text are skipped)
                             StringArray tokens;
                             tokens.addTokens (f.loadFileAsString(), ",; \t\r\n", "\"");
                             std::vector<double> values;
                             for (const auto& t : tokens)
                                 if (t.containsAnyOf ("0123456789") && t.trim().containsOnly ("0123456789.-+eE"))
                                     values.push_back (t.getDoubleValue());

                             if (values.empty())
                             {
                                 AlertWindow::showMessageBoxAsync (AlertWindow::WarningIcon, "Import samples", "No numbers found in " + f.getFileName() + ".");
                                 return;
                             }
                             done (values, f.getFileNameWithoutExtension().toStdString());
                         });
}

void addTimingChoices (ComboBox& box)
{
    box.addItem ("All LEDs at the same time", 1);
    box.addSectionHeading ("One LED after another");
    box.addItem ("Tip to base (row by row)", 2);
    box.addItem ("Base to tip (row by row)", 3);
    box.addItem ("By LED number", 4);
    box.addItem ("One shank after another", 5);
}

SequenceOrder orderForTiming (int timingId)
{
    switch (timingId)
    {
        case 2: return SequenceOrder::BottomToTop;
        case 3: return SequenceOrder::TopToBottom;
        case 4: return SequenceOrder::ChannelOrder;
        case 5: return SequenceOrder::ShankByShank;
        default: return SequenceOrder::Together;
    }
}

std::vector<int> sequenceRanks (const std::vector<int>& leds, SequenceOrder order)
{
    // key per LED; equal keys share a rank, ranks are dense in key order
    std::vector<int> keys;
    for (const int led : leds)
    {
        switch (order)
        {
            case SequenceOrder::Together: keys.push_back (0); break;
            case SequenceOrder::ChannelOrder: keys.push_back (led); break;
            case SequenceOrder::BottomToTop: keys.push_back (rowFromBottom (led)); break;
            case SequenceOrder::TopToBottom: keys.push_back (15 - rowFromBottom (led)); break;
            case SequenceOrder::ShankByShank: keys.push_back (shankOf (led)); break;
        }
    }

    std::vector<int> sorted (keys);
    std::sort (sorted.begin(), sorted.end());
    sorted.erase (std::unique (sorted.begin(), sorted.end()), sorted.end());

    std::vector<int> ranks;
    for (int k : keys)
        ranks.push_back ((int) (std::lower_bound (sorted.begin(), sorted.end(), k) - sorted.begin()));
    return ranks;
}

namespace
{

String num (double v, int decimals = 3)
{
    String s (v, decimals);
    if (s.containsChar ('.'))
        s = s.trimCharactersAtEnd ("0").trimCharactersAtEnd (".");
    return s;
}

String msText (double s)
{
    return num (s * 1000.0) + " ms";
}

} // namespace

String describeWave (const Block& b)
{
    const String uA = " " + kMicroAmps;

    switch (b.wave.kind)
    {
        case WaveKind::Shaped:
        {
            const auto& t = b.wave.shaped;
            const int n = shapedCount (t);
            String s = shapeName (t.shape) + ", " + num (t.amp) + uA + (t.equalCharge ? " equiv." : "") + ", " + msText (t.width);
            if (n > 1)
                s << " x " << n << " at " << num (t.rate) << " Hz";
            if (t.step != 0)
                s << ", " << (t.step > 0 ? "+" : "") << num (t.step) << uA << "/pulse";
            return s;
        }
        case WaveKind::Builtin:
        {
            const auto& w = b.wave.builtin;
            String s = waveDisplayName (b.wave);
            switch (w.type)
            {
                case WaveType::FixedPulse:
                case WaveType::PulseStaircase:
                case WaveType::FixedSawtooth:
                case WaveType::FixedSine:
                    s << ", " << num (w.amp) << uA << ", " << (int) std::round (w.freq) << " x " << msText (w.offset);
                    break;
                case WaveType::Wavelet:
                    s << ", " << num (w.amp) << uA << ", " << msText (w.offset);
                    break;
                case WaveType::None:
                    break;
                default:
                    s << ", " << num (w.freq) << " Hz, " << num (w.amp) << uA;
                    break;
            }
            return s;
        }
        case WaveKind::Breakpoints:
            return "Points (" + String ((int) b.wave.points.size()) + ")" + (b.wave.repeat ? ", repeating" : "");
        case WaveKind::Samples:
            return "Samples (" + String ((int) b.wave.samples.size()) + ")" + (b.wave.repeat ? ", repeating" : "");
        case WaveKind::Formula:
            return "Formula: " + String::fromUTF8 (b.wave.formula.c_str());
    }
    return {};
}

double cycleLength (const Block& b)
{
    const double minLen = 4.0 / kSampleRateHz;
    auto clampLen = [minLen] (double v) { return std::isfinite (v) && v > minLen ? std::min (v, 13.0) : 0.1; };

    switch (b.wave.kind)
    {
        case WaveKind::Shaped:
        {
            const auto& t = b.wave.shaped;
            if (shapedCount (t) > 1)
                return clampLen ((double) shapedPeriodSamples (t) / kSampleRateHz);
            return clampLen (1.25 * shapedPulseSamples (t) / kSampleRateHz);
        }
        case WaveKind::Builtin:
        {
            const auto& w = b.wave.builtin;
            switch (w.type)
            {
                case WaveType::FixedPulse:
                case WaveType::PulseStaircase:
                case WaveType::FixedSawtooth:
                case WaveType::FixedSine:
                case WaveType::Wavelet:
                    return clampLen (w.offset);
                case WaveType::DoubleSine:
                    return clampLen (1.0 / std::max (1.0e-3, std::min (std::fabs (w.freq), std::fabs (w.offset))));
                case WaveType::None:
                    return 0.1;
                default:
                    return clampLen (1.0 / std::max (1.0e-3, std::fabs (w.freq)));
            }
        }
        case WaveKind::Breakpoints:
            return clampLen (b.wave.points.empty() ? 0.0 : b.wave.points.back().time);
        case WaveKind::Samples:
            return clampLen ((double) b.wave.samples.size() / kSampleRateHz);
        case WaveKind::Formula:
            return clampLen (std::min (b.length > 0 ? b.length : 1.0, 1.0));
    }
    return 0.1;
}

std::vector<double> renderAlone (const Block& b, double length)
{
    length = std::max (1.0 / kSampleRateHz, std::min (length, 13.0));

    Pattern p;
    p.duration = length;
    Block x = b;
    x.id = 1;
    x.led = 1;
    x.start = 0;
    x.link = 0;
    if (x.length <= 0)
        x.length = length;
    p.blocks.push_back (x);

    PatternData d;
    std::string error;
    if (! renderPattern (p, d, error))
        return {};

    std::vector<double> out ((std::size_t) d.numSamples);
    const std::uint8_t* lane = d.led (0);
    for (int i = 0; i < d.numSamples; ++i)
        out[(std::size_t) i] = lane[i];
    return out;
}


bool timingIsStaggered (int id)
{
    return id >= 2;
}

String lengthDescription (const Block& b)
{
    auto ms = [] (double s)
    {
        String t (s * 1000.0, 3);
        return t.trimCharactersAtEnd ("0").trimCharactersAtEnd (".") + " ms";
    };

    const double n = naturalLength (b.wave);
    if (n <= 0)
        return {};

    if (b.wave.kind == WaveKind::Shaped)
    {
        const auto& t = b.wave.shaped;
        const int count = shapedCount (t);
        const bool cycles = t.shape == PulseShape::SineCycle;
        String s = "lasts " + ms (n);
        if (count == 1)
            s << (cycles ? "  (one cycle)" : "  (one pulse)");
        else
            s << "  (" << count << (cycles ? " cycles" : " pulses") << " at "
              << String (t.rate, 3).trimCharactersAtEnd ("0").trimCharactersAtEnd (".") << " Hz)";
        if (t.equalCharge)
            s << ",  peak " << String (shapedMaxPeak (t), 1).trimCharactersAtEnd ("0").trimCharactersAtEnd (".") << " " << kMicroAmps;
        return s;
    }

    if (b.wave.kind == WaveKind::Builtin)
    {
        const auto& w = b.wave.builtin;
        const int count = (int) std::round (w.freq);
        if (w.type == WaveType::Wavelet)
            return "lasts " + ms (n);
        return "lasts " + ms (n) + "  (" + String (count) + " x " + ms (w.offset) + ")";
    }
    if (b.wave.kind == WaveKind::Breakpoints)
        return "lasts " + ms (n) + " (to the last point)";
    return "lasts " + ms (n) + " (" + String ((int) b.wave.samples.size()) + " samples)";
}

} // namespace PatternEditor
