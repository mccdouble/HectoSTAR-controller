/*
    ------------------------------------------------------------------

    HectoSTAR pattern editor window: probe map and sequencing tools on the
    left, timeline in the middle, inspector below, and a top bar for the
    pattern settings, design files (.json), exporting the flash file (.bin)
    and uploading it to the board.

    Non-modal; closing it only hides it, so the pattern is kept while the
    plugin is open.

    ------------------------------------------------------------------
*/

#pragma once

#include "BlockInspector.h"
#include "PatternDocument.h"
#include "ProbePanel.h"
#include "TimelineView.h"

namespace PatternEditor
{

class PatternEditorComponent : public Component,
                               private ChangeListener
{
public:
    /** onUpload is called with the generated .bin and the pattern's repeat count. */
    PatternEditorComponent (ProbeMapping initialProbe, std::function<void (const File&, int)> onUpload);
    ~PatternEditorComponent() override;

    void paint (Graphics& g) override;
    void resized() override;
    bool keyPressed (const KeyPress& key) override;

    /** "name.json *" for the window title. */
    String documentTitle() const;
    std::function<void()> onTitleChanged;

private:
    PatternDocument doc;
    std::function<void (const File&, int)> uploadCallback;

    TextButton newButton { "New" }, openButton { "Open..." }, saveButton { "Save" }, saveAsButton { "Save as..." };
    TextButton undoButton { "Undo" }, redoButton { "Redo" }, fitButton { "Zoom to fit" };
    Label durationLabel { {}, "Duration (s)" }, probeLabel { {}, "Probe" }, repeatLabel { {}, "Repeats" };
    TextEditor durationEditor, repeatEditor;
    ComboBox probeBox;
    Label usageLabel;
    TextButton exportButton { "Export .bin..." }, uploadButton { "Upload to board" };

    ProbePanel probePanel;
    TimelineView timeline;
    BlockInspector inspector;
    Label statusLabel;

    std::unique_ptr<FileChooser> chooser;

    void changeListenerCallback (ChangeBroadcaster*) override;
    void refresh();

    void newPattern();
    void openPattern();
    void savePattern (bool askForFile);
    void exportBin();
    void upload();

    /** Checks the pattern, confirms an unverified probe mapping, then calls
        onReady with the encoded file. */
    void prepareOutput (const String& action, std::function<void (std::vector<std::uint8_t>)> onReady);

    void confirmDiscard (std::function<void()> then);
};

class PatternEditorWindow : public DocumentWindow
{
public:
    PatternEditorWindow (ProbeMapping initialProbe, std::function<void (const File&, int)> onUpload);

    void closeButtonPressed() override { setVisible (false); }

private:
    PatternEditorComponent* editor;
};

} // namespace PatternEditor
