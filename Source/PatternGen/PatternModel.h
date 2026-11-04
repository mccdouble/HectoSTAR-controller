/*
    ------------------------------------------------------------------

    HectoSTAR pattern design: blocks of stimulation placed on LED lanes.

    A pattern has a duration, a target probe and a list of blocks. Each block
    drives one LED with one waveform inside a time window
    [start, start + length). Blocks may overlap on the same LED; overlapping
    currents combine by taking the higher value (then clipped to 0..100 uA).

    Waveforms are a train of shaped pulses (the temporal shapes of Bansal,
    Pyari and Roy 2023, "Optogenetic generation of neural firing patterns
    with temporal shaping of light pulses", Photonics 10:571), one of the
    built-in types of PatternGenGUI.m, or a custom shape: breakpoints,
    imported samples, or a formula (Formula.h).

    Designs are saved as JSON (toJson / fromJson). Rendering goes through
    PatternGenerator, so a design with one built-in block per LED renders
    exactly what the MATLAB tool would for the same table.

    ------------------------------------------------------------------
*/

#pragma once

#include "PatternGenerator.h"

#include <cstddef>
#include <string>
#include <vector>

namespace PatternGen
{

enum class WaveKind
{
    Shaped,      // a train of identical shaped pulses or cycles (ShapedTrain)
    Builtin,     // one of the PatternGenGUI.m waveform types
    Breakpoints, // points joined by step / linear / smooth segments
    Samples,     // explicit values at 20 kHz (e.g. imported from CSV)
    Formula      // expression of t (Formula.h)
};

enum class Interpolation
{
    Step,   // hold each point's value until the next point
    Linear, // straight lines between points
    Smooth  // cosine ease between points
};

/** Shape of one pulse, as a function of x = 0..1 across the pulse width.
    The first twelve are Table 6 of Bansal et al. 2023 (their names in
    comments); "rising" shapes peak at the end of the pulse, "falling" ones
    at the start. */
enum class PulseShape
{
    Square,          // square
    RampUp,          // left-triangular: x
    RampDown,        // right-triangular: 1 - x
    Triangle,        // triangular: up over the first half, down over the second
    RampUpHold,      // backward-ramp: up over the first half, then hold
    HoldRampDown,    // forward-ramp: hold for the first half, then down
    Gaussian,        // gaussian: centred, sigma = width / 7
    RisingGaussian,  // left-gaussian: peak at the end, sigma = width / 3.5
    FallingGaussian, // right-gaussian: peak at the start, sigma = width / 3.5
    HalfSine,        // positive-sinusoidal: sin(pi x)
    RisingSine,      // left-positive-sinusoidal: sin(pi/2 x)
    FallingSine,     // right-positive-sinusoidal: cos(pi/2 x)
    SineCycle        // one full sine cycle from 0 up to the peak and back: (1 - cos 2 pi x) / 2
};

constexpr int kNumPulseShapes = 13;

/** A train of identical pulses, like the pulse output: count pulses, one
    starting every period s, each lasting width s with the given shape. Sine
    cycles run back to back (their period is their width). */
struct ShapedTrain
{
    PulseShape shape = PulseShape::Square;
    double amp = 50;      // uA: the peak, or with equalCharge the height of the matching square pulse
    double width = 0.005; // s, one pulse (or one cycle)
    double count = 1;     // pulses (rounded to a whole number, at least 1)
    double period = 0.1;  // s, from one pulse's start to the next (unused for one pulse and for sine cycles)
    double step = 0;      // uA added to the amplitude on each following pulse
    bool equalCharge = false; // scale the peak so each pulse carries the charge of a square pulse of amp
};

/** Shape value (0..1) at x in 0..1. */
double pulseShapeValue (PulseShape shape, double x);

/** Mean of the shape over one pulse of n samples (1 for a square pulse). */
double pulseShapeMean (PulseShape shape, int n);

/** Id used in saved designs ("square", "ramp-up", ...). */
const char* pulseShapeId (PulseShape shape);
bool pulseShapeFromId (const std::string& id, PulseShape& shape);

/** Samples in one pulse, and between pulse starts. */
int shapedPulseSamples (const ShapedTrain& train);
int shapedPeriodSamples (const ShapedTrain& train);
int shapedCount (const ShapedTrain& train);

/** Highest peak current (uA) the train asks for, after equal-charge scaling. */
double shapedMaxPeak (const ShapedTrain& train);

struct Breakpoint
{
    double time = 0;  // s from block start
    double value = 0; // uA
};

/** What a block plays. Only the fields for its kind are used. */
struct WaveSpec
{
    WaveKind kind = WaveKind::Builtin;

    // Shaped: pulse train settings.
    ShapedTrain shaped;

    // Builtin: MATLAB-table parameters. The delay field is ignored; the block's
    // start time is used instead.
    ChannelWaveform builtin;

    // Breakpoints: times ascending from the block start. Without repeat the
    // shape spans the first..last point and is 0 outside it; with repeat it
    // tiles every (last point time) seconds.
    std::vector<Breakpoint> points;
    Interpolation interpolation = Interpolation::Linear;

    // Samples: uA at 20 kHz from the block start; 0 after the last sample
    // unless repeat.
    std::vector<double> samples;

    // Breakpoints / Samples: tile across the block.
    bool repeat = false;

    // Formula: current in uA as a function of t (s from block start).
    std::string formula;
};

struct Block
{
    int id = 0;          // unique within the pattern
    int led = 1;         // GUI channel 1..64
    double start = 0;    // s
    double length = 0;   // s; the block only plays inside [start, start + length)
    int link = 0;        // blocks sharing a non-zero link are edited together
    std::string label;   // optional, e.g. the preset it came from
    double fadeIn = 0;   // s of linear ramp-up from the block start
    double fadeOut = 0;  // s of linear ramp-down to the block end
    WaveSpec wave;
};

struct Pattern
{
    double duration = 1.0; // s
    ProbeMapping probe = ProbeMapping::FourShankChronic;
    int repeatCount = 1; // times the FPGA plays the pattern
    std::vector<Block> blocks;
};

/** How long a waveform naturally lasts (s): to the end of the last pulse
    for shaped trains, count x period for the fixed built-in types, D for wavelet, the last point for non-repeating
    breakpoints, samples / fs for non-repeating samples. 0 when it runs until
    the block ends (continuous built-ins, formulas, repeating shapes). */
double naturalLength (const WaveSpec& wave);

/** LEDs (1..64, ascending) that have at least one block. */
std::vector<int> activeLeds (const Pattern& pattern);

/** Next free block id. */
int nextBlockId (const Pattern& pattern);

/** Problems that would stop the pattern being generated or uploaded (bad
    duration, LEDs outside 1..64, formula errors, too long for the flash).
    Empty when the pattern is OK. */
std::vector<std::string> validate (const Pattern& pattern);

/** Bytes the encoded pattern file will take, and whether it fits in flash. */
std::size_t encodedSizeBytes (const Pattern& pattern);
bool fitsInFlash (const Pattern& pattern);

/** Renders every block into per-LED currents (GUI channel order). Returns
    false and sets error if the duration is invalid or a formula does not
    parse. Does not check flash capacity (see validate). */
bool renderPattern (const Pattern& pattern, PatternData& out, std::string& error);

/** Reads a pattern file (.bin, as written by encodePatternFile or the
    MATLAB tool) back into a design: one block of samples per LED that has
    any current, from its first to its last non-zero sample. The probe
    mapping must be the one the file was made for. Fails (setting error)
    when the size is not whole 64-byte frames or exceeds the flash. */
bool decodePatternFile (const std::vector<std::uint8_t>& bytes, ProbeMapping mapping, Pattern& pattern, std::string& error);

/** Saves / loads a pattern design (.json). fromJson leaves pattern unchanged
    and sets error on failure. */
std::string toJson (const Pattern& pattern);
bool fromJson (const std::string& text, Pattern& pattern, std::string& error);

const char* waveKindName (WaveKind kind);       // "builtin", "breakpoints", ...
const char* interpolationName (Interpolation i); // "step", "linear", "smooth"

} // namespace PatternGen
