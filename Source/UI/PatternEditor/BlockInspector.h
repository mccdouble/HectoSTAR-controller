/*
    ------------------------------------------------------------------

    Pattern editor inspector: edits the blocks selected on the timeline
    (all of them at once; linked copies are selected together). Nothing is
    shown when no block is selected.

    Currents are in uA, times in ms. Waveforms that decide their own length
    (pulse trains, cycles, a wavelet, one-shot shapes) get it automatically;
    continuous ones have a Length field. Built-in waveform parameters keep
    the meanings of PatternGenGUI.m's table, relabelled per type.

    ------------------------------------------------------------------
*/

#pragma once

#include "EditorStyle.h"
#include "PatternDocument.h"

#include <array>

namespace PatternEditor
{

class BlockInspector : public Component,
                       private ChangeListener
{
public:
    explicit BlockInspector (PatternDocument& doc);
    ~BlockInspector() override;

    void paint (Graphics& g) override;
    void resized() override;

    /** Height needed to show every field at this width (fields wrap when narrow). */
    int preferredHeight (int width);

    /** Called when the height it needs changes. */
    std::function<void()> onLayoutChanged;

private:
    PatternDocument& doc;

    Label title;
    EditorButton unlinkButton { "Unlink" };
    EditorButton deleteButton { "Delete", EditorButton::Style::Danger };

    ComboBox waveBox;

    // waveform parameters: shaped trains (amplitude, width, count, rate,
    // step) or built-in types (freq, amp, offset, duty, step)
    std::array<Label, 5> paramLabels;
    std::array<TextEditor, 5> paramEditors;
    ToggleButton chargeToggle { "Equal charge" };

    // custom waveforms
    Label pointsLabel;
    TextEditor pointsEditor;
    ComboBox interpBox;
    ToggleButton repeatToggle { "Repeat" };
    EditorButton importButton { "Import CSV..." };
    Label samplesInfo;
    TextEditor formulaEditor;

    // timing
    Label startLabel { {}, "Start (ms)" }, lengthLabel { {}, "Length (ms)" }, lengthInfo;
    TextEditor startEditor, lengthEditor;
    Label spacingLabel { {}, "Re-space" }, everyLabel { {}, "every" }, msLabel { {}, "ms" };
    ComboBox spacingBox;
    TextEditor spacingStep;
    EditorButton spacingApply { "Apply" };

    // more options
    EditorButton moreButton { "More options" };
    Label labelCaption { {}, "Label" }, fadeInLabel { {}, "Fade in (ms)" }, fadeOutLabel { {}, "Fade out (ms)" };
    TextEditor labelEditor, fadeInEditor, fadeOutEditor;
    bool showMore = false;

    Label help;

    std::unique_ptr<FileChooser> chooser;
    bool updating = false;
    int headerBottom = 0;
    int lastPreferredHeight = -1;

    void changeListenerCallback (ChangeBroadcaster*) override { refresh(); }
    void refresh();
    void updateVisibility();
    int doLayout (int width, bool apply);

    bool hasSelection() const { return ! doc.selectedBlocks().empty(); }
    const Block* first() const;
    bool spansSeveralLeds() const;

    /** Applies a change to every selected block as one undo step, then
        updates fixed lengths. */
    void apply (const std::function<void (Block&)>& change);

    void setEditorText (TextEditor& ed, const String& text);
    void chooseWave();
    void commitParam (int index);
    void commitStart();
    void commitLength();
    void commitPoints();
    void commitFormula();
    void respace();
};

} // namespace PatternEditor
