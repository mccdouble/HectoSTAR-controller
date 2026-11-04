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
    // a sensible first block: ten 5 ms, 50 uA square pulses at 10 Hz
    newBlockTemplate.wave.kind = WaveKind::Shaped;
    newBlockTemplate.wave.shaped.shape = PulseShape::Square;
    newBlockTemplate.wave.shaped.amp = 50;
    newBlockTemplate.wave.shaped.width = 0.005;
    newBlockTemplate.wave.shaped.count = 10;
    newBlockTemplate.wave.shaped.period = 0.1;
    newBlockTemplate.length = naturalLength (newBlockTemplate.wave);

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
    ++version;
    file = f;
    undoStack.clear();
    redoStack.clear();
    blockSelection.clear();
    ledSelection.clear();
    render();
    dirty = false;
    sendChangeMessage();
}

void PatternDocument::setRepeatCount (int repeats)
{
    repeats = std::max (1, repeats);
    if (current.repeatCount == repeats)
        return;
    current.repeatCount = repeats;
    dirty = true;
    sendChangeMessage();
}

void PatternDocument::setProbe (ProbeMapping probe)
{
    if (current.probe == probe)
        return;
    current.probe = probe;
    ++version;
    for (auto& p : undoStack)
        p.probe = probe;
    for (auto& p : redoStack)
        p.probe = probe;
    sendChangeMessage();
}

void PatternDocument::changed()
{
    dirty = true;
    ++version;
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
