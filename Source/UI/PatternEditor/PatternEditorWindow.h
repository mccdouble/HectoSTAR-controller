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
#include "EditorStyle.h"
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
    static constexpr int kToolbarHeight = 44;
    static constexpr int kSettingsHeight = 40;
    static constexpr int kStatusHeight = 26;
    static constexpr int kInspectorHeight = 178;
    static constexpr int kProbePanelWidth = 290;

    PatternDocument doc;
    std::function<void (const File&, int)> uploadCallback;

    EditorButton newButton { "New" }, openButton { "Open..." }, saveButton { "Save" }, saveAsButton { "Save as..." };
    EditorButton undoButton { "Undo" }, redoButton { "Redo" }, fitButton { "Zoom to fit" };
    Label durationLabel { {}, "Duration (s)" }, probeLabel { {}, "Probe" }, repeatLabel { {}, "Repeats" };
    TextEditor durationEditor, repeatEditor;
    ComboBox probeBox;
    Label usageLabel;
    EditorButton exportButton { "Export .bin..." };
    EditorButton uploadButton { "Upload to board", EditorButton::Style::Accent };

    ProbePanel probePanel;
    TimelineView timeline;
    BlockInspector inspector;
    Label statusLabel;
    TooltipWindow tooltips { this, 600 };

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
    /** host: the plugin component opening the editor; the window uses its look
        and feel and opens on the same screen. */
    PatternEditorWindow (Component& host, ProbeMapping initialProbe, std::function<void (const File&, int)> onUpload);
    ~PatternEditorWindow() override;

    void closeButtonPressed() override { setVisible (false); }

private:
    PatternEditorComponent* editor;
};

} // namespace PatternEditor
