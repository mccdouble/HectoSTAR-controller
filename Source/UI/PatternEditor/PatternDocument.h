/*
    ------------------------------------------------------------------

    State shared by the pattern editor's views: the pattern being edited,
    what is selected, undo history, and the rendered output (re-rendered
    shortly after each change so dragging stays responsive).

    ------------------------------------------------------------------
*/

#pragma once

#include <VisualizerEditorHeaders.h>

#include "../../PatternGen/PatternModel.h"

#include <functional>
#include <set>
#include <vector>

namespace PatternEditor
{

using namespace PatternGen;

/** Channel layout used by the editor (same as PatternGenGUI.m): 4 shanks of
    16, shank s holds channels 16(s-1)+1 .. 16s from bottom to top. */
inline int shankOf (int led) { return (led - 1) / 16 + 1; }
inline int rowFromBottom (int led) { return (led - 1) % 16; }

class PatternDocument : public ChangeBroadcaster,
                        private Timer
{
public:
    PatternDocument();

    const Pattern& pattern() const { return current; }

    /** Applies a change as one undo step and re-renders. */
    void modify (const std::function<void (Pattern&)>& change);

    /** For drags: snapshot once at mouse-down, then apply live changes without
        adding undo steps. */
    void beginGesture();
    void modifyLive (const std::function<void (Pattern&)>& change);

    void undo();
    void redo();
    bool canUndo() const { return ! undoStack.empty(); }
    bool canRedo() const { return ! redoStack.empty(); }

    /** Replaces the whole pattern (new / open); clears undo and selection. */
    void reset (const Pattern& p, const File& file);

    // selection
    const std::set<int>& selectedBlocks() const { return blockSelection; }
    const std::set<int>& selectedLeds() const { return ledSelection; }
    void setSelectedBlocks (std::set<int> ids);
    void setSelectedLeds (std::set<int> leds);
    bool isBlockSelected (int id) const { return blockSelection.count (id) > 0; }

    /** All blocks sharing id's link (or just id when unlinked). */
    std::set<int> linkGroup (int id) const;

    const Block* findBlock (int id) const;

    /** LEDs that get a lane: those with blocks plus any selected on the probe map. */
    std::vector<int> laneLeds() const;

    // rendered output (may lag edits by a few ms)
    const PatternData& rendered() const { return renderedData; }
    const std::string& renderError() const { return lastRenderError; }

    /** Settings for newly added blocks, edited in the inspector when nothing is selected. */
    Block newBlockTemplate;

    File file;
    bool dirty = false;

private:
    Pattern current;
    std::vector<Pattern> undoStack;
    std::vector<Pattern> redoStack;
    std::set<int> blockSelection;
    std::set<int> ledSelection;

    PatternData renderedData;
    std::string lastRenderError;

    void pushUndo();
    void changed();
    void render();
    void pruneSelection();
    void timerCallback() override;

    static constexpr std::size_t kMaxUndo = 200;
};

} // namespace PatternEditor
