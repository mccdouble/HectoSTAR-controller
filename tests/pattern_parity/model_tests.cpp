// Tests for the pattern model (PatternModel, Formula, Json).
//
// Usage: model_tests <case dir> <matlab out dir>
//
// The MATLAB-equivalence test turns every parity case into a pattern with one
// built-in block per table row and checks the encoded file is byte-identical
// to the MATLAB reference files from run_reference.m, directly and after a
// JSON save/load round trip.

#include "test_cases.h"

#include "../../Source/PatternGen/Formula.h"
#include "../../Source/PatternGen/PatternModel.h"

#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iterator>

namespace fs = std::filesystem;
using namespace PatternGen;

static int failures = 0;
static int checks = 0;

#define CHECK(cond, ...)                                     \
    do                                                       \
    {                                                        \
        ++checks;                                            \
        if (! (cond))                                        \
        {                                                    \
            ++failures;                                      \
            std::printf ("  FAIL %s:%d: ", __FILE__, __LINE__); \
            std::printf (__VA_ARGS__);                       \
            std::printf ("\n");                              \
        }                                                    \
    } while (0)

static Block builtinBlock (int id, int led, double start, double length, const ChannelWaveform& w)
{
    Block b;
    b.id = id;
    b.led = led;
    b.start = start;
    b.length = length;
    b.wave.kind = WaveKind::Builtin;
    b.wave.builtin = w;
    return b;
}

static Block formulaBlock (int id, int led, double start, double length, const std::string& expr)
{
    Block b;
    b.id = id;
    b.led = led;
    b.start = start;
    b.length = length;
    b.wave.kind = WaveKind::Formula;
    b.wave.formula = expr;
    return b;
}

static std::vector<std::uint8_t> renderLane (const Pattern& p, int led)
{
    PatternData d;
    std::string error;
    if (! renderPattern (p, d, error))
    {
        std::printf ("  render failed: %s\n", error.c_str());
        return {};
    }
    return std::vector<std::uint8_t> (d.led (led - 1), d.led (led - 1) + d.numSamples);
}

static std::vector<std::uint8_t> encode (const Pattern& p)
{
    PatternData d;
    std::string error;
    if (! renderPattern (p, d, error))
        return {};
    return encodePatternFile (d, p.probe);
}

static void testMatlabEquivalence (const fs::path& caseDir, const fs::path& refDir)
{
    std::printf ("MATLAB equivalence (one block per table row, plus JSON round trip)\n");
    int cases = 0, identical = 0, identicalAfterJson = 0;

    for (const auto& entry : fs::directory_iterator (caseDir))
    {
        if (entry.path().extension() != ".csv")
            continue;

        TestCase tc;
        std::string error;
        if (! readTestCase (entry.path().string(), tc, error))
        {
            CHECK (false, "%s", error.c_str());
            continue;
        }

        Pattern p;
        p.duration = tc.duration;
        p.probe = tc.mapping;
        int id = 1;
        for (int ch : tc.channels)
            p.blocks.push_back (builtinBlock (id++, ch, tc.waves[ch - 1].delay, 1.0e6, tc.waves[ch - 1]));

        std::ifstream refFile (refDir / entry.path().stem().concat (".bin"), std::ios::binary);
        const std::vector<std::uint8_t> ref ((std::istreambuf_iterator<char> (refFile)), std::istreambuf_iterator<char>());

        ++cases;
        const auto bytes = encode (p);
        if (bytes == ref)
            ++identical;
        else
            CHECK (false, "%s differs from MATLAB", entry.path().filename().string().c_str());

        Pattern reloaded;
        if (! fromJson (toJson (p), reloaded, error))
        {
            CHECK (false, "%s: JSON reload failed: %s", entry.path().filename().string().c_str(), error.c_str());
            continue;
        }
        if (encode (reloaded) == ref)
            ++identicalAfterJson;
        else
            CHECK (false, "%s differs from MATLAB after JSON round trip", entry.path().filename().string().c_str());
    }

    CHECK (cases > 0, "no cases found in %s", caseDir.string().c_str());
    std::printf ("  %d of %d identical, %d of %d after JSON round trip\n", identical, cases, identicalAfterJson, cases);
}

static void testFormulaMatchesBuiltins()
{
    std::printf ("Formulas written like built-ins render the same bytes\n");

    struct Pair
    {
        ChannelWaveform builtin;
        const char* formula;
    };

    ChannelWaveform sine;
    sine.type = WaveType::Sine;
    sine.freq = 10;
    sine.amp = 40;
    sine.offset = 50;

    ChannelWaveform saw;
    saw.type = WaveType::Sawtooth;
    saw.freq = 7.3;
    saw.amp = 45;
    saw.offset = 50;

    ChannelWaveform pulse;
    pulse.type = WaveType::Pulse;
    pulse.freq = 15;
    pulse.amp = 20;
    pulse.offset = 30;
    pulse.duty = 25;

    const Pair pairs[] = {
        { sine, "50 + 40*sin(2*pi*10*t)" },
        { saw, "50 + 45*sawtooth(2*pi*7.3*t)" },
        { pulse, "30 + 20*square(2*pi*15*t, 25)" },
    };

    for (const auto& pr : pairs)
    {
        Pattern a, b;
        a.duration = b.duration = 0.25;
        a.blocks.push_back (builtinBlock (1, 1, 0.0123, 0.2, pr.builtin));
        b.blocks.push_back (formulaBlock (1, 1, 0.0123, 0.2, pr.formula));
        const auto la = renderLane (a, 1);
        const auto lb = renderLane (b, 1);
        CHECK (! la.empty() && la == lb, "'%s' does not match the built-in %s", pr.formula, waveTypeName (pr.builtin.type));
    }
}

static void testOverlapAndWindow()
{
    std::printf ("Overlapping blocks take the higher value; blocks stop at their length\n");

    ChannelWaveform train;
    train.type = WaveType::FixedPulse;
    train.freq = 5;
    train.amp = 80;
    train.offset = 0.02;
    train.duty = 30;

    Pattern a, b, both;
    a.duration = b.duration = both.duration = 0.2;
    a.blocks.push_back (formulaBlock (1, 3, 0.01, 0.15, "30 + 0*t"));
    b.blocks.push_back (builtinBlock (2, 3, 0.03, 0.1, train));
    both.blocks = a.blocks;
    both.blocks.push_back (b.blocks[0]);

    const auto la = renderLane (a, 3), lb = renderLane (b, 3), lab = renderLane (both, 3);
    bool ok = ! lab.empty();
    for (std::size_t i = 0; ok && i < lab.size(); ++i)
        ok = lab[i] == std::max (la[i], lb[i]);
    CHECK (ok, "combined lane is not the per-sample max");
    CHECK (la[199] == 0 && la[200] == 30, "formula block should start at sample 200 (t = 0.01)");
    CHECK (la[3199] == 30 && la[3200] == 0, "formula block should end before sample 3200 (t = 0.16)");

    ChannelWaveform s;
    s.type = WaveType::Sine;
    s.freq = 20;
    s.amp = 40;
    s.offset = 50;
    Pattern full, cut;
    full.duration = cut.duration = 0.2;
    full.blocks.push_back (builtinBlock (1, 1, 0.02, 1.0e6, s));
    cut.blocks.push_back (builtinBlock (1, 1, 0.02, 0.05, s));
    const auto lf = renderLane (full, 1), lc = renderLane (cut, 1);
    bool same = true, zeroAfter = true;
    for (int i = 400; i < 1400; ++i)
        same = same && lf[i] == lc[i];
    for (std::size_t i = 1400; i < lc.size(); ++i)
        zeroAfter = zeroAfter && lc[i] == 0;
    CHECK (same, "truncated block differs from the full render inside its window");
    CHECK (zeroAfter, "truncated block should be 0 after its length");
}

static void testBreakpointsSamplesFades()
{
    std::printf ("Breakpoints, samples and fades\n");

    auto bpBlock = [] (Interpolation interp, bool repeat)
    {
        Block b;
        b.id = 1;
        b.led = 1;
        b.start = 0;
        b.length = 0.05;
        b.wave.kind = WaveKind::Breakpoints;
        b.wave.interpolation = interp;
        b.wave.repeat = repeat;
        b.wave.points = { { 0, 0 }, { 0.01, 100 } };
        return b;
    };

    Pattern p;
    p.duration = 0.05;

    p.blocks = { bpBlock (Interpolation::Linear, false) };
    auto l = renderLane (p, 1);
    CHECK (l[100] == 50, "linear midpoint: %d", l[100]);
    CHECK (l[200] == 100, "linear at last point: %d", l[200]);
    CHECK (l[201] == 0, "after the last point (no repeat): %d", l[201]);

    p.blocks = { bpBlock (Interpolation::Step, false) };
    l = renderLane (p, 1);
    CHECK (l[100] == 0 && l[200] == 100, "step: %d %d", l[100], l[200]);

    p.blocks = { bpBlock (Interpolation::Smooth, false) };
    l = renderLane (p, 1);
    CHECK (l[100] == 50 && l[50] < 25, "smooth: mid %d, quarter %d", l[100], l[50]);

    p.blocks = { bpBlock (Interpolation::Linear, true) };
    l = renderLane (p, 1);
    CHECK (l[250] == 25 && l[650] == 25, "repeat tiles every 10 ms: %d %d", l[250], l[650]);

    Block s;
    s.id = 1;
    s.led = 2;
    s.start = 0.001;
    s.length = 0.01;
    s.wave.kind = WaveKind::Samples;
    s.wave.samples = { 10, 20, 30 };
    p.blocks = { s };
    l = renderLane (p, 2);
    CHECK (l[19] == 0 && l[20] == 10 && l[21] == 20 && l[22] == 30 && l[23] == 0, "samples placement");
    p.blocks[0].wave.repeat = true;
    l = renderLane (p, 2);
    CHECK (l[23] == 10 && l[24] == 20, "repeating samples");

    Block f = formulaBlock (1, 4, 0, 0.05, "100");
    f.fadeIn = 0.01;
    f.fadeOut = 0.01;
    p.blocks = { f };
    l = renderLane (p, 4);
    CHECK (l[0] == 0 && l[100] == 50 && l[500] == 100 && l[900] == 50, "fades: %d %d %d %d", l[0], l[100], l[500], l[900]);
}

static void testFormulaLanguage()
{
    std::printf ("Formula language\n");

    struct Case
    {
        const char* expr;
        double t, T, expected;
    };

    const Case cases[] = {
        { "1+2*3", 0, 0, 7 },
        { "(1+2)*3", 0, 0, 9 },
        { "-2^2", 0, 0, -4 },
        { "2^3^2", 0, 0, 64 },
        { "2^-1", 0, 0, 0.5 },
        { "3 .* 2 ./ 4 .^ 1", 0, 0, 1.5 },
        { "mod(-1,3)", 0, 0, 2 },
        { "rem(-1,3)", 0, 0, -1 },
        { "t", 0.5, 0, 0.5 },
        { "T", 0, 2, 2 },
        { "fs", 0, 0, 20000 },
        { "if(t < 0.5, 1, 2)", 0.25, 0, 1 },
        { "if(t < 0.5, 1, 2)", 0.75, 0, 2 },
        { "~0 + !1", 0, 0, 1 },
        { "1 ~= 2", 0, 0, 1 },
        { "1 != 1", 0, 0, 0 },
        { "1 && 0 || 1", 0, 0, 1 },
        { "clamp(150, 0, 100)", 0, 0, 100 },
        { "step(-0.1) + step(0)", 0, 0, 1 },
        { "round(2.5) + round(-2.5)", 0, 0, 0 },
        { ".5*2 + 1e-3*1000", 0, 0, 2 },
        { "max(3, 4) - min(3, 4)", 0, 0, 1 },
        { "abs(-3) + sign(-2) + floor(1.7) + ceil(1.2) + fix(-1.7)", 0, 0, 4 },
        { "sqrt(16) + log(e) + log10(100) + log2(8)", 0, 0, 10 },
        { "50*(1+sin(2*pi*8*t))", 0, 0, 50 },
    };

    for (const auto& c : cases)
    {
        std::string error;
        const auto f = Formula::compile (c.expr, error);
        CHECK (f != nullptr, "'%s' did not compile: %s", c.expr, error.c_str());
        if (f != nullptr)
        {
            const double v = f->evaluate (c.t, c.T);
            CHECK (std::fabs (v - c.expected) < 1e-12, "'%s' = %.17g, expected %.17g", c.expr, v, c.expected);
        }
    }

    const char* bad[] = { "", "sin(", "foo", "bar(1)", "sin(1,2)", "1 +", "2 3", "(1", "1.2.3", "square()", "if(1,2)" };
    for (const char* b : bad)
    {
        std::string error;
        CHECK (Formula::compile (b, error) == nullptr && ! error.empty(), "'%s' should not compile", b);
    }
}

static void testJson()
{
    std::printf ("JSON save and load\n");

    Pattern p;
    p.duration = 0.1234;
    p.probe = ProbeMapping::FourShankAcute;
    p.repeatCount = 7;

    ChannelWaveform w;
    w.type = WaveType::PulseStaircase;
    w.freq = 4;
    w.amp = 10.1;
    w.offset = 0.02;
    w.duty = 40;
    w.step = -2.5;
    p.blocks.push_back (builtinBlock (1, 5, 0.001, 0.08, w));
    p.blocks.back().link = 3;
    p.blocks.back().label = "staircase \"A\" \xC2\xB5" "A";

    ChannelWaveform nanWave;
    nanWave.type = WaveType::Sine;
    nanWave.freq = NAN;
    nanWave.amp = 40;
    p.blocks.push_back (builtinBlock (2, 6, 0, 0.1, nanWave));

    Block bp;
    bp.id = 3;
    bp.led = 64;
    bp.start = 0.0105;
    bp.length = 0.05;
    bp.fadeIn = 0.002;
    bp.fadeOut = 0.003;
    bp.wave.kind = WaveKind::Breakpoints;
    bp.wave.interpolation = Interpolation::Smooth;
    bp.wave.repeat = true;
    bp.wave.points = { { 0, 0 }, { 0.003, 77.7 }, { 0.007, 12.345678901234567 } };
    p.blocks.push_back (bp);

    Block sm;
    sm.id = 4;
    sm.led = 1;
    sm.start = 0.02;
    sm.length = 0.01;
    sm.wave.kind = WaveKind::Samples;
    sm.wave.samples = { 1, 2.5, 99, 0.1 };
    p.blocks.push_back (sm);

    p.blocks.push_back (formulaBlock (5, 2, 0, 0.1, "50*(1+sin(2*pi*8*t))*(t<T/2)"));

    const std::string text = toJson (p);
    Pattern q;
    std::string error;
    CHECK (fromJson (text, q, error), "reload failed: %s", error.c_str());
    CHECK (toJson (q) == text, "JSON text changed after a round trip");
    CHECK (encode (p) == encode (q) && ! encode (p).empty(), "rendering changed after a round trip");
    CHECK (q.repeatCount == 7 && q.probe == ProbeMapping::FourShankAcute && q.blocks.size() == 5, "pattern fields");
    CHECK (q.blocks[0].label == p.blocks[0].label && q.blocks[0].link == 3, "label/link");
    CHECK (std::isnan (q.blocks[1].wave.builtin.freq), "NaN parameter should survive as NaN");
    CHECK (q.blocks[2].wave.points[2].value == 12.345678901234567, "double precision lost");

    const char* bad[] = {
        "not json",
        "{\"format\":\"other\"}",
        "{\"format\":\"hectostar-pattern\",\"version\":99}",
        "{\"format\":\"hectostar-pattern\",\"blocks\":[{\"wave\":{\"kind\":\"magic\"}}]}",
        "{\"format\":\"hectostar-pattern\",\"blocks\":[{\"wave\":{\"type\":\"nope\"}}]}",
        "{\"format\":\"hectostar-pattern\",\"blocks\":[{\"wave\":{\"kind\":\"breakpoints\",\"points\":[[1]]}}]}",
        "{\"format\":\"hectostar-pattern\",\"probe\":\"8 shank\"}",
    };
    for (const char* b : bad)
    {
        Pattern untouched = q;
        std::string err;
        CHECK (! fromJson (b, untouched, err) && ! err.empty(), "should reject: %s", b);
        CHECK (toJson (untouched) == text, "a failed load must not change the pattern");
    }
}

static void testHelpers()
{
    std::printf ("Lengths, active LEDs, sizes and validation\n");

    WaveSpec w;
    w.kind = WaveKind::Builtin;
    w.builtin.type = WaveType::FixedPulse;
    w.builtin.freq = 5;
    w.builtin.offset = 0.02;
    CHECK (std::fabs (naturalLength (w) - 0.1) < 1e-15, "fixedpulse length");
    w.builtin.type = WaveType::Wavelet;
    w.builtin.offset = 0.3;
    CHECK (naturalLength (w) == 0.3, "wavelet length");
    w.builtin.type = WaveType::Sine;
    CHECK (naturalLength (w) == 0, "sine runs to the block end");
    w.kind = WaveKind::Breakpoints;
    w.points = { { 0, 0 }, { 0.04, 1 } };
    CHECK (naturalLength (w) == 0.04, "breakpoints length");
    w.repeat = true;
    CHECK (naturalLength (w) == 0, "repeating breakpoints");
    w.kind = WaveKind::Samples;
    w.repeat = false;
    w.samples.assign (100, 1.0);
    CHECK (naturalLength (w) == 0.005, "samples length");

    Pattern p;
    p.duration = 1;
    p.blocks.push_back (formulaBlock (1, 9, 0, 1, "1"));
    p.blocks.push_back (formulaBlock (2, 3, 0, 1, "1"));
    p.blocks.push_back (formulaBlock (3, 9, 0.5, 1, "1"));
    const auto leds = activeLeds (p);
    CHECK (leds == std::vector<int> ({ 3, 9 }), "active LEDs");
    CHECK (nextBlockId (p) == 4, "next block id");
    CHECK (encodedSizeBytes (p) == 1280000, "1 s = 1.28 MB");
    CHECK (validate (p).empty(), "valid pattern reported problems");

    Pattern tiny;
    tiny.duration = 1.0 / 20000.0;
    CHECK (encodedSizeBytes (tiny) == 256, "1 sample pads to one page");

    Pattern big = p;
    big.duration = 14;
    CHECK (! fitsInFlash (big) && ! validate (big).empty(), "14 s should not fit in 16 MB");

    Pattern broken = p;
    broken.blocks.push_back (formulaBlock (4, 65, 0, 1, "sin("));
    CHECK (validate (broken).size() == 2, "LED 65 and the formula error should both be reported");
}

int main (int argc, char** argv)
{
    if (argc != 3)
    {
        std::fprintf (stderr, "usage: model_tests <case dir> <matlab out dir>\n");
        return 2;
    }

    testMatlabEquivalence (argv[1], argv[2]);
    testFormulaMatchesBuiltins();
    testOverlapAndWindow();
    testBreakpointsSamplesFades();
    testFormulaLanguage();
    testJson();
    testHelpers();

    std::printf ("%d of %d checks passed\n", checks - failures, checks);
    return failures == 0 ? 0 : 1;
}
