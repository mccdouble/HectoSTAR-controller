/*
    ------------------------------------------------------------------

    Pattern editor inspector: edits the selected blocks (all of them at
    once; linked copies are selected together), or the settings used for
    new blocks when nothing is selected.

    Currents are in uA, times in ms. Built-in waveform parameters keep the
    meanings of PatternGenGUI.m's table, relabelled per type.

    ------------------------------------------------------------------
*/

#pragma once

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

private:
    PatternDocument& doc;

    Label title;
    ComboBox presetBox, kindBox, typeBox;
    TextEditor labelEditor;

    Label startLabel { {}, "Start (ms)" }, lengthLabel { {}, "Length (ms)" }, fadeInLabel { {}, "Fade in (ms)" },
        fadeOutLabel { {}, "Fade out (ms)" };
    TextEditor startEditor, lengthEditor, fadeInEditor, fadeOutEditor;
    TextButton fitButton { "Fit" };

    // built-in parameters: freq, amp, offset, duty, step
    std::array<Label, 5> paramLabels;
    std::array<TextEditor, 5> paramEditors;
    Label guide;

    // custom waveforms
    Label pointsLabel { {}, "Points (ms:uA)" };
    TextEditor pointsEditor;
    ComboBox interpBox;
    ToggleButton repeatToggle { "Repeat" };
    TextButton importButton { "Import CSV..." };
    Label samplesInfo;
    TextEditor formulaEditor;
    Label formulaHelp;

    TextButton unlinkButton { "Unlink" }, deleteButton { "Delete" };

    std::unique_ptr<FileChooser> chooser;
    bool updating = false;

    void changeListenerCallback (ChangeBroadcaster*) override { refresh(); }
    void refresh();
    void layoutForKind();

    /** The block shown in the fields: the first selected block, or the new-block template. */
    const Block& shown() const;
    bool editingTemplate() const { return doc.selectedBlocks().empty(); }

    /** Applies a change to every selected block (one undo step), or to the template. */
    void apply (const std::function<void (Block&)>& change);

    void setEditorText (TextEditor& ed, const String& text);
    void commitParam (int index);
    void commitTiming (TextEditor& ed);
    void commitPoints();
    void commitFormula();
    void applyPreset (int index);
    void changeKind (WaveKind kind);
    void importCsv();
};

} // namespace PatternEditor
