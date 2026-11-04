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

    // fill the visible area, scrolling only when it is smaller than the stim panel needs
    const int contentW = jmax (channelList->getMinimumWidth(), getWidth() - scrollBarThickness);
    const int contentH = jmax (ChannelList::kMinHeight, getHeight() - scrollBarThickness);
    channelList->setBounds (0, 0, contentW, contentH);

    channelViewport->setScrollBarsShown (true, true);
    channelViewport->setScrollBarThickness (scrollBarThickness);
}