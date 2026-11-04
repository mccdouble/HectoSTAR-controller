/*
    ------------------------------------------------------------------

    This file is part of the Open Ephys GUI
    Copyright (C) 2014 Open Ephys

    ------------------------------------------------------------------

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.

*/

#include "ChannelCanvas.h"

#include "ChannelList.h"

#include "../DeviceThread.h"

using namespace RhythmNode;

/**********************************************/

ChannelCanvas::ChannelCanvas (DeviceThread* board_,
                              DeviceEditor* editor_) : board (board_),
                                                       editor (editor_)
{
    channelViewport = std::make_unique<Viewport>();

    channelList = std::make_unique<ChannelList> (board, editor);

    channelList->setOwnerCanvas (this); //lets us call repaint() function on probe geometry selection
  
    channelViewport->setViewedComponent (channelList.get(), false);
    channelViewport->setScrollBarsShown (true, true);
    channelViewport->setScrollBarThickness (10);
    addAndMakeVisible (channelViewport.get());

    update();

    resized();
}

void ChannelCanvas::setProbeGeometry (int newShankCount)
{
   shankCount = juce::jlimit (1, 4, newShankCount);
   resized(); // IMPORTANT: updates viewport content bounds
   repaint();
}

void ChannelCanvas::paint (Graphics& g)
{


    g.fillAll (juce::Colours::grey);
   
}

void ChannelCanvas::refresh()
{
    repaint();
}

void ChannelCanvas::refreshState()
{
    resized();
}

void ChannelCanvas::updateSettings()
{
    channelList->update();
    resized();
}

void ChannelCanvas::updateAsync()
{
    Timer::callAfterDelay (5, [this]
                           { updateSettings(); });
}

void ChannelCanvas::beginAnimation()
{
    channelList->disableAll();
}

void ChannelCanvas::endAnimation()
{
    channelList->enableAll();
}

void ChannelCanvas::resized()
{
    const int scrollBarThickness = channelViewport->getScrollBarThickness();
    channelViewport->setBounds (0, 0, getWidth(), getHeight());

    // --- MUST match your layout constants used in ChannelList::update() and ChannelCanvas::paint() ---
    const float startX = 30.0f;
    const float startY = 170.0f;

    float shankLength = 500.0f;
    const float shankH = 80.0f;

    const float shankThickness = 2.0f;
    const float gap = shankThickness * 8.0f;

    const int shanks = getShankCount();

    // your rule: 1- and 2-shank are longer
    if (shanks == 1 || shanks == 2)
        shankLength *= 2.0f;

    // Content size big enough to include:
    // - left labels margin
    // - full shank length to the right
    // - some extra margin
    // ...and never narrower than the controls along the top
    const int contentW = jmax ((int) std::ceil (startX + shankLength + 250.0f), channelList->getRequiredWidth());

    // Height big enough for stacked shanks + some UI above + disclaimer below
    const float probeBottom = startY + (shanks - 1) * (shankH + gap) + shankH;
    const int contentH = (int) std::ceil (probeBottom + 120.0f);

    channelList->setBounds (0, 0, contentW, contentH);

    // Optional: keep scrollbars visible
    channelViewport->setScrollBarsShown (true, true);
    channelViewport->setScrollBarThickness (scrollBarThickness);
}