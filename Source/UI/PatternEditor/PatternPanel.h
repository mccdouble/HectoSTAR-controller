/*
    ------------------------------------------------------------------

    Pattern stim mode of the HectoSTAR stim panel (shown beside the probe
    drawing when the output mode is Pattern).

    Header:   File menu, undo/redo, pattern length, repeats, flash use and
              what is on the board.
    Designer: choose and tune a waveform, apply it to the selected LEDs.
    Timeline: the whole pattern, one lane per LED in use, for checking and
              fine timing (drag blocks to move them), with the on-screen playback
              controls. Docked along the bottom (drag the bar above it to resize);
              Pop out moves it to its own window, Dock (or closing that window)
              brings it back.

    ------------------------------------------------------------------
*/

#pragma once

#include "Playback.h"
#include "TimelineView.h"
#include "WaveformDesigner.h"

namespace PatternEditor
{

/** The timeline with its playback toolbar (Play/Pause, Stop, speed, time)
    and the dock / pop-out button. Lives in the pattern panel or in its own
    window. */
class TimelinePane : public Component,
                     private ChangeListener
{
public:
    TimelinePane (PatternDocument& doc, Playback& playback);
    ~TimelinePane() override;

    TimelineView& getTimeline() { return timeline; }

    /** Shows "Pop out" when docked, "Dock" when in its own window. */
    void setDocked (bool docked);

    std::function<void()> onDockToggle;

    void paint (Graphics& g) override;
    void resized() override;

    static constexpr int kToolbarHeight = 34;

private:
    PatternDocument& doc;
    Playback& playback;

    Label title { {}, "Timeline" };
    EditorButton playButton { "Play" }, stopButton { "Stop" };
    ComboBox speedBox;
    Label playTime;
    EditorButton dockButton { "Pop out" };
    EditorButton inUseButton { "Select LEDs in use" };
    TimelineView timeline;

    void changeListenerCallback (ChangeBroadcaster*) override { updatePlayControls(); }
    void updatePlayControls();
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

    /** On-screen playback (the probe drawing lights its LEDs from it). */
    Playback& getPlayback() { return playback; }

private:
    PatternDocument& doc;

    EditorButton fileButton { "File" }, undoButton { "Undo" }, redoButton { "Redo" };

    Playback playback;
    Label lengthLabel { {}, "Pattern length (s)" }, repeatLabel { {}, "Repeats" };
    TextEditor lengthEditor, repeatEditor;
    Label usageLabel, boardLabel;

    WaveformDesigner designer;

    // the whole pattern over time: docked at the bottom, or in its own window
    TimelinePane timelinePane;
    bool timelineDocked = true;
    int timelineHeight = 260;
    class Divider;
    std::unique_ptr<Divider> divider;
    class TimelineWindow;
    std::unique_ptr<TimelineWindow> timelineWindow;
    void setTimelineDocked (bool docked);

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
