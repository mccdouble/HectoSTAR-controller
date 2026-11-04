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

/** Display name of a block's waveform. */
String waveDisplayName (const WaveSpec& wave);

/** Asks for a CSV/text file and reads every number in it (uA at 20 kHz).
    holder keeps the chooser alive; done gets the values and file name. */
void chooseSamplesFile (std::unique_ptr<FileChooser>& holder, std::function<void (std::vector<double>, std::string)> done);

/** Timing choices for placing blocks on several LEDs (ids 1..5). */
void addTimingChoices (ComboBox& box);

/** Order for a timing choice id; false for "All at once". */
bool timingIsStaggered (int id);

} // namespace PatternEditor
