/*
    ------------------------------------------------------------------

    Pattern stim mode of the HectoSTAR stim panel (shown beside the probe
    drawing when the output mode is Pattern).

    Header:   File menu, undo/redo, pattern length, repeats, flash use and
              what is on the board.
    LED list: the LEDs in the pattern (plus any selected), one row each with
              a summary of its waveform; selection is shared with the probe.
    Designer: choose and tune a waveform, apply it to the selected LEDs.
    Timeline: the whole pattern, one lane per LED in use, for checking and
              fine timing (drag blocks to move them).

    ------------------------------------------------------------------
*/

#pragma once

#include "TimelineView.h"
#include "WaveformDesigner.h"

namespace PatternEditor
{

/** List of the LEDs in the pattern; selecting rows selects LEDs. */
class LedList : public Component,
                private ListBoxModel,
                private ChangeListener
{
public:
    explicit LedList (PatternDocument& doc);
    ~LedList() override;

    void paint (Graphics& g) override;
    void resized() override;

private:
    PatternDocument& doc;
    Label header;
    ListBox list;
    EditorButton allButton { "All in use" }, noneButton { "None" };
    std::vector<int> rows;
    bool updating = false;

    int getNumRows() override { return (int) rows.size(); }
    void paintListBoxItem (int row, Graphics& g, int width, int height, bool selected) override;
    void selectedRowsChanged (int lastRowSelected) override;
    void changeListenerCallback (ChangeBroadcaster*) override;
    void refresh();
};

class PatternPanel : public Component,
                     private ChangeListener
{
public:
    explicit PatternPanel (PatternDocument& doc);
    ~PatternPanel() override;

    void paint (Graphics& g) override;
    void resized() override;
    bool keyPressed (const KeyPress& key) override;

    /** Checks, renders and encodes the pattern; calls onReady with the file
        bytes (asks first if the probe mapping is unverified). */
    void prepareOutput (const String& action, std::function<void (std::vector<std::uint8_t>)> onReady);

    /** Shown at the right of the header, e.g. "On the board: this pattern". */
    void setBoardStatus (const String& text, bool upToDate);

    /** Called after a design is opened or a new one started. */
    std::function<void()> onPatternReplaced;

private:
    PatternDocument& doc;

    EditorButton fileButton { "File" }, undoButton { "Undo" }, redoButton { "Redo" };
    Label lengthLabel { {}, "Pattern length (s)" }, repeatLabel { {}, "Repeats" };
    TextEditor lengthEditor, repeatEditor;
    Label usageLabel, boardLabel;

    LedList ledList;
    WaveformDesigner designer;
    TimelineView timeline;

    std::unique_ptr<FileChooser> chooser;
    std::set<int> lastBlockSelection;
    int headerBottom = 0;

    void changeListenerCallback (ChangeBroadcaster*) override;
    void refresh();
    void commitLength();
    void commitRepeats();

    void showFileMenu();
    void newPattern();
    void openPattern();
    void savePattern (bool askForFile);
    void exportBin();
    void loadBin();
    void confirmDiscard (std::function<void()> then);
};

} // namespace PatternEditor
