/*
    ------------------------------------------------------------------

    Pattern editor waveform list. See WaveformCatalog.h.

    ------------------------------------------------------------------
*/

#include "WaveformCatalog.h"

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

    static const std::vector<WaveChoice> list = {
        // presets: ready-made settings
        { G::Preset, "10 Hz pulses, 5 ms, 50 uA, 1 s", "10 Hz, 5 ms, 50 uA", builtin (WaveType::FixedPulse, 10, 50, 0.1, 5) },
        { G::Preset, "20 Hz pulses, 10 ms, 50 uA, 1 s", "20 Hz, 10 ms, 50 uA", builtin (WaveType::FixedPulse, 20, 50, 0.05, 20) },
        { G::Preset, "40 Hz pulses, 2 ms, 50 uA, 0.5 s", "40 Hz, 2 ms, 50 uA", builtin (WaveType::FixedPulse, 20, 50, 0.025, 8) },
        { G::Preset, "Single 10 ms pulse, 50 uA", "10 ms pulse", builtin (WaveType::FixedPulse, 1, 50, 0.01, 100) },
        { G::Preset, "Staircase 10 to 80 uA, 8 x 100 ms", "Staircase 10-80 uA", builtin (WaveType::PulseStaircase, 8, 10, 0.1, 50, 10) },
        { G::Preset, "Sine 8 Hz, 0 to 100 uA", "Sine 8 Hz", builtin (WaveType::Sine, 8, 50, 50, 0) },
        { G::Preset, "Ramp 0 to 100 uA over 100 ms", "Ramp 100 ms", points ({ { 0, 0 }, { 0.1, 100 } }, false) },
        { G::Preset, "Triangle 0-80-0 uA every 20 ms", "Triangle 20 ms", points ({ { 0, 0 }, { 0.01, 80 }, { 0.02, 0 } }, true) },
        { G::Preset, "40 Hz bursts on an 8 Hz sine", "Theta-gamma", formula ("40*(1+sin(2*pi*8*t)) * (1+square(2*pi*40*t, 50))/2") },

        // built-in types with sensible defaults
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
            box.addSectionHeading (c.group == WaveChoice::Group::Preset ? "Presets" : (c.group == WaveChoice::Group::Type ? "Waveforms" : "Custom"));
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

String waveDisplayName (const WaveSpec& wave)
{
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
    box.addItem ("All at once", 1);
    box.addSectionHeading ("Staggered");
    box.addItem ("Bottom to top (by row)", 2);
    box.addItem ("Top to bottom (by row)", 3);
    box.addItem ("Channel order", 4);
    box.addItem ("Shank by shank", 5);
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
