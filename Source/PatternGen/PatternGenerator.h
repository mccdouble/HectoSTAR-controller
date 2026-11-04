/*
    ------------------------------------------------------------------

    HectoSTAR pattern stim generator.

    Renders per-LED stimulation waveforms and encodes them into the binary
    pattern file the FPGA plays back from flash. Port of the waveform
    backend and "Generate File" step of PatternGenGUI.m (tools/matlab/),
    reproducing its output byte for byte (see tests/pattern_parity).

    Plain C++17 with no JUCE or Open Ephys dependencies, so it can be built
    and tested on its own.

    ------------------------------------------------------------------
*/

#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace PatternGen
{

/** Pattern file format: one sample per LED every 1/20 kHz, each a DAC current in uA. */
constexpr int kNumLeds = 64;
constexpr double kSampleRateHz = 20000.0;
constexpr int kMaxCurrentUa = 100; // DAC range is 0..100 uA in 1 uA steps

/** The FPGA reads whole frames of 4 samples (4 x 64 = 256 bytes = one flash page). */
constexpr int kSamplesPerPage = 4;
constexpr std::size_t kBytesPerFrame = kNumLeds; // one byte per LED per sample

/** Pattern flash chip capacity (16 MB). */
constexpr std::size_t kFlashCapacityBytes = 16u * 1024u * 1024u;
constexpr std::size_t kMaxSamples = kFlashCapacityBytes / kBytesPerFrame;

/** Waveform types, matching the WaveType names in PatternGenGUI.m. */
enum class WaveType
{
    None,
    Pulse,
    FixedPulse,
    PulseStaircase,
    Sawtooth,
    FixedSawtooth,
    Sine,
    FixedSine,
    DoubleSine,
    Wavelet
};

/** MATLAB name ("fixedpulse", "pulse_staircase", ...) <-> WaveType. Unknown names map to None. */
const char* waveTypeName (WaveType type);
WaveType waveTypeFromName (const std::string& name);

/** One row of the MATLAB waveform table. Field meanings depend on the type
    (see paramGuideData in PatternGenGUI.m); all times are in seconds and
    amplitudes/offsets in uA. */
struct ChannelWaveform
{
    WaveType type = WaveType::None;
    double freq = 0;   // "Freq/#Pulses": Hz, or a count for fixed* / staircase / wavelet
    double amp = 0;    // "Amplitude" (uA)
    double offset = 0; // "Offset/Period": baseline (uA), period (s) or duration (s)
    double duty = 0;   // "Duty (%) / Cycles"
    double delay = 0;  // "Delay (s)": start time
    double step = 0;   // "Step": per-pulse amplitude step for pulse_staircase (uA)
};

/** Rendered pattern: DAC current (uA, 0..100) for each LED and sample, in
    GUI channel order (channel 1..64 -> index 0..63), before device remapping. */
struct PatternData
{
    int numSamples = 0;
    std::vector<std::uint8_t> current; // [led * numSamples + sample]

    std::uint8_t* led (int index) { return current.data() + (std::size_t) index * (std::size_t) numSamples; }
    const std::uint8_t* led (int index) const { return current.data() + (std::size_t) index * (std::size_t) numSamples; }
};

/** Number of samples for a duration, rounded like MATLAB's round(duration * fs). */
int numSamplesForDuration (double durationSec);

/** Renders one waveform into out[0..numSamples), clipped to 0..100 uA and
    rounded, exactly as generateWaveformsFromUITable does for one channel. */
void renderChannel (const ChannelWaveform& wave, int numSamples, std::uint8_t* out);

/** Renders samples [firstSample, endSample) of a waveform, before clipping,
    into out[0 .. endSample - firstSample). Values equal those of a full render
    at the same sample indices. */
void renderWaveformRange (const ChannelWaveform& wave, int firstSample, int endSample, double* out);

/** Clips a current to 0..100 uA and rounds it to a DAC step (NaN -> 0), like
    the MATLAB tool's y(y<0)=0; y(y>100)=100; uint8(round(y)). */
std::uint8_t toDacCurrent (double microamps);

/** Time of sample i in seconds, computed like MATLAB's (0:N-1)/fs. */
inline double sampleTimeSec (int i) { return (double) i / kSampleRateHz; }

/** Renders the MATLAB table: waves[i] drives GUI channel i + 1. Returns false
    (and sets error) for a non-positive duration. */
bool renderPattern (const std::array<ChannelWaveform, kNumLeds>& waves,
                    double durationSec,
                    PatternData& out,
                    std::string& error);

/** Target devices, in the same order as the plugin's probe selector (ids 1..4). */
enum class ProbeMapping
{
    FourShankChronic = 1,
    TwoShankChronic = 2,
    FourShankAcute = 3,
    DemoBoard = 4
};

const char* probeMappingName (ProbeMapping mapping); // "4 Shank Chronic", ...

/** False for mappings that have not been checked on a real device yet. */
bool isProbeMappingVerified (ProbeMapping mapping);

/** reorder[i] (1-based) is the GUI channel written to LED slot i of each frame. */
const std::array<int, kNumLeds>& reorderForMapping (ProbeMapping mapping);

/** Encodes a rendered pattern into the flash file: pads with zero samples to a
    whole number of pages, remaps channels for the device, and interleaves one
    64-byte frame per sample. */
std::vector<std::uint8_t> encodePatternFile (const PatternData& pattern, ProbeMapping mapping);

} // namespace PatternGen
