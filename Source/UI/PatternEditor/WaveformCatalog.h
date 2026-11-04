/*
    ------------------------------------------------------------------

    The single list of waveforms offered by the pattern editor: presets
    (ready-made settings), the built-in types of PatternGenGUI.m with
    sensible defaults, and the custom kinds.

    ------------------------------------------------------------------
*/

#pragma once

#include "PatternDocument.h"

namespace PatternEditor
{

extern const String kMicroAmps; // "µA"

struct WaveChoice
{
    enum class Group
    {
        Shape,  // pulse shapes (shaped trains): the main dictionary
        Preset,
        Type,
        Custom
    };

    Group group;
    String name;       // shown in the list
    std::string label; // put on blocks made from a preset
    WaveSpec wave;     // the settings it applies
};

const std::vector<WaveChoice>& waveChoices();

/** Fills a ComboBox with the list (ids = index + 1), with section headings. */
void addWaveChoices (ComboBox& box, bool includePresets);

/** The list entry describing a block's current waveform (its type or custom kind; never a preset). */
int choiceIdFor (const WaveSpec& wave);

/** True when the waveform decides its own length (pulse trains, cycles, a wavelet, one-shot shapes). */
bool hasFixedLength (const WaveSpec& wave);

/** Plain-language length, e.g. "lasts 1000 ms (10 x 100 ms)". */
String lengthDescription (const Block& b);

/** Sets the length of a fixed-length waveform from its parameters; others keep theirs. */
void updateFixedLength (Block& b);

/** One-line explanation of a built-in type. */
String typeGuide (WaveType type);

/** One-line explanation of a pulse shape. */
String shapeGuide (PulseShape shape);

/** Name of a pulse shape as listed, e.g. "Gaussian". */
String shapeName (PulseShape shape);

/** Display name of a block's waveform. */
String waveDisplayName (const WaveSpec& wave);

/** Asks for a CSV/text file and reads every number in it (uA at 20 kHz).
    holder keeps the chooser alive; done gets the values and file name. */
void chooseSamplesFile (std::unique_ptr<FileChooser>& holder, std::function<void (std::vector<double>, std::string)> done);

/** Timing choices for placing one waveform on several LEDs (ids 1..5):
    all at the same time, or one after another in an order. */
void addTimingChoices (ComboBox& box);

/** True for the "one after another" choices. */
bool timingIsStaggered (int id);

enum class SequenceOrder
{
    Together,     // all start at once
    ChannelOrder, // LED 1, 2, ...
    BottomToTop,  // row by row up the shanks; same row starts together
    TopToBottom,
    ShankByShank  // shank 1, then 2, ...; a whole shank starts together
};

SequenceOrder orderForTiming (int timingId);

/** Rank (0, 1, 2, ...) of each LED in the order; equal ranks start together. */
std::vector<int> sequenceRanks (const std::vector<int>& leds, SequenceOrder order);

/** One-line summary of a block, e.g. "Gaussian, 50 uA, 10 ms x 10 at 10 Hz". */
String describeWave (const Block& b);

/** Length (s) of one cycle of the waveform: one pulse period, one period of
    a continuous wave, one repeat of a custom shape. */
double cycleLength (const Block& b);

/** Renders the block alone from t = 0 for length s; uA per 20 kHz sample, as
    the DAC will output it. */
std::vector<double> renderAlone (const Block& b, double length);

} // namespace PatternEditor
