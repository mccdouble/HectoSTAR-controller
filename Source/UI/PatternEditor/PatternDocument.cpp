/*
    ------------------------------------------------------------------

    Pattern editor shared state. See PatternDocument.h.

    ------------------------------------------------------------------
*/

#include "PatternDocument.h"

namespace PatternEditor
{

PatternDocument::PatternDocument()
{
    // a sensible first block: a 10 Hz train of 5 ms, 50 uA pulses for 1 s
    newBlockTemplate.length = 1.0;
    newBlockTemplate.label = "10 Hz, 5 ms, 50 \xc2\xb5" "A";
    newBlockTemplate.wave.kind = WaveKind::Builtin;
    newBlockTemplate.wave.builtin.type = WaveType::FixedPulse;
    newBlockTemplate.wave.builtin.freq = 10;
    newBlockTemplate.wave.builtin.amp = 50;
    newBlockTemplate.wave.builtin.offset = 0.1;
    newBlockTemplate.wave.builtin.duty = 5;

    render();
}

void PatternDocument::pushUndo()
{
    undoStack.push_back (current);
    if (undoStack.size() > kMaxUndo)
        undoStack.erase (undoStack.begin());
    redoStack.clear();
}

void PatternDocument::modify (const std::function<void (Pattern&)>& change)
{
    pushUndo();
    change (current);
    changed();
}

void PatternDocument::beginGesture()
{
    pushUndo();
}

void PatternDocument::modifyLive (const std::function<void (Pattern&)>& change)
{
    change (current);
    changed();
}

void PatternDocument::undo()
{
    if (undoStack.empty())
        return;
    redoStack.push_back (current);
    current = undoStack.back();
    undoStack.pop_back();
    changed();
}

void PatternDocument::redo()
{
    if (redoStack.empty())
        return;
    undoStack.push_back (current);
    current = redoStack.back();
    redoStack.pop_back();
    changed();
}

void PatternDocument::reset (const Pattern& p, const File& f)
{
    current = p;
    file = f;
    undoStack.clear();
    redoStack.clear();
    blockSelection.clear();
    ledSelection.clear();
    render();
    dirty = false;
    sendChangeMessage();
}

void PatternDocument::changed()
{
    dirty = true;
    pruneSelection();
    startTimer (30); // coalesce re-renders while dragging
    sendChangeMessage();
}

void PatternDocument::timerCallback()
{
    stopTimer();
    render();
    sendChangeMessage();
}

void PatternDocument::render()
{
    lastRenderError.clear();
    if (! renderPattern (current, renderedData, lastRenderError))
        renderedData = PatternData();
}

void PatternDocument::pruneSelection()
{
    std::set<int> keep;
    for (int id : blockSelection)
        if (findBlock (id) != nullptr)
            keep.insert (id);
    blockSelection.swap (keep);
}

void PatternDocument::setSelectedBlocks (std::set<int> ids)
{
    blockSelection = std::move (ids);
    pruneSelection();
    sendChangeMessage();
}

void PatternDocument::setSelectedLeds (std::set<int> leds)
{
    ledSelection = std::move (leds);
    sendChangeMessage();
}

std::set<int> PatternDocument::linkGroup (int id) const
{
    const Block* b = findBlock (id);
    if (b == nullptr)
        return {};
    if (b->link == 0)
        return { id };

    std::set<int> group;
    for (const auto& other : current.blocks)
        if (other.link == b->link)
            group.insert (other.id);
    return group;
}

const Block* PatternDocument::findBlock (int id) const
{
    for (const auto& b : current.blocks)
        if (b.id == id)
            return &b;
    return nullptr;
}

std::vector<int> PatternDocument::laneLeds() const
{
    std::set<int> leds (ledSelection.begin(), ledSelection.end());
    for (int led : activeLeds (current))
        leds.insert (led);
    return std::vector<int> (leds.begin(), leds.end());
}

} // namespace PatternEditor
