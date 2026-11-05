/*
    ------------------------------------------------------------------

    Pattern stim waveform designer: choose a waveform (pulse shapes,
    classic MATLAB-tool types, or custom), set its parameters while
    watching a one-cycle preview, then apply it to the LEDs selected on the
    probe or in the LED list (the MATLAB tool's bulk update).

    Nothing changes in the pattern until Apply / Add after / Remove.
    Selecting LEDs that already have a waveform loads it here, so it can be
    adjusted and applied again.

    Apply replaces whatever the selected LEDs had. With several LEDs the
    timing choice starts them together or one after another (tip to base,
    by LED number, ...) a fixed number of ms apart. Add after puts the
    waveform after what each LED already has.

    ------------------------------------------------------------------
*/

#pragma once

#include "EditorStyle.h"
#include "WaveformViews.h"

#include <array>

namespace PatternEditor
{

class WaveformDesigner : public Component,
                         private ChangeListener
{
public:
    explicit WaveformDesigner (PatternDocument& doc);
    ~WaveformDesigner() override;

    void paint (Graphics& g) override;
    void resized() override;

    /** The waveform Apply will use (also what double-clicking a timeline lane adds). */
    const Block& workingBlock() const { return working; }

private:
    PatternDocument& doc;
    Block working;
    std::set<int> loadedFor;
    int tab = 0; // 0 pulse shapes, 1 classic, 2 custom

    Label title;
    EditorButton tabShapes { "Pulse shapes" }, tabClassic { "Classic" }, tabCustom { "Custom" };
    EditorButton presetsButton { "Presets" };
    ShapePalette palette;

    // parameters: shaped trains (peak, width, count, rate, step) or classic
    // types (freq, amp, offset, duty, step)
    std::array<Label, 5> paramLabels;
    std::array<TextEditor, 5> paramEditors;
    ToggleButton chargeToggle { "Equal charge" };

    Label pointsLabel;
    TextEditor pointsEditor;
    ComboBox interpBox;
    ToggleButton repeatToggle { "Repeat" };
    EditorButton importButton { "Import CSV..." };
    Label samplesInfo;
    TextEditor formulaEditor;
    Label lengthLabel { {}, "Length (ms)" };
    TextEditor lengthEditor;

    Label help;

    // pulse shapes: the maths behind the train, and a way to adapt it
    Label maths;
    EditorButton asFormulaButton { "Edit as formula" };
    HyperlinkButton paperLink { "Source: Bansal, Pyari & Roy 2023, Photonics 10(5):571",
                                URL ("https://doi.org/10.3390/photonics10050571") };
    void updateMaths();
    void convertToFormula();

    WaveformPreview preview;
    EditorButton cycleButton { "One cycle" }, wholeButton { "Whole" };
    Label info;

    Label startLabel { {}, "Start at (ms)" };
    TextEditor startEditor;
    ComboBox timingBox;
    Label everyLabel { {}, "every" }, msLabel { {}, "ms" };
    TextEditor stepEditor;

    EditorButton applyButton { "Apply", EditorButton::Style::Accent };
    EditorButton appendButton { "Add after" };
    EditorButton removeButton { "Remove", EditorButton::Style::Danger };

    std::unique_ptr<FileChooser> chooser;
    bool updating = false;

    void changeListenerCallback (ChangeBroadcaster*) override;

    void loadFromSelection();
    void setTab (int t);
    void choose (int choiceIndex);
    void showPresets();
    void workingChanged(); // refresh fields, preview and template
    void refreshFields();
    void updateVisibility();

    void commitParam (int index);
    void commitPoints();
    void commitFormula();
    void commitLength();

    std::vector<int> selectedLeds() const;
    void applyToLeds (bool append);
    void removeFromLeds();
};

} // namespace PatternEditor
