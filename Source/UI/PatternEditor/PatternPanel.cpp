/*
    ------------------------------------------------------------------

    Pattern stim mode panel. See PatternPanel.h.

    ------------------------------------------------------------------
*/

#include "PatternPanel.h"

#include <cmath>

namespace PatternEditor
{

namespace
{

String formatSeconds (double s)
{
    String t (s, 2);
    if (t.containsChar ('.'))
        t = t.trimCharactersAtEnd ("0").trimCharactersAtEnd (".");
    return t + " s";
}

String ledPlace (int led)
{
    const int row = rowFromBottom (led) + 1;
    return "shank " + String (shankOf (led)) + ", " + String (row) + (row == 1 ? "st" : row == 2 ? "nd" : row == 3 ? "rd" : "th") + " from tip";
}

} // namespace

//==============================================================================

LedList::LedList (PatternDocument& d) : doc (d)
{
    header.setText ("LEDs in the pattern", dontSendNotification);
    header.setFont (FontOptions (14.0f, Font::bold));
    addAndMakeVisible (header);

    list.setModel (this);
    list.setMultipleSelectionEnabled (true);
    list.setRowHeight (40);
    list.setOutlineThickness (1);
    addAndMakeVisible (list);

    allButton.setTooltip ("Select every LED that has a waveform");
    allButton.onClick = [this]
    {
        const auto used = activeLeds (doc.pattern());
        doc.setSelectedBlocks ({});
        doc.setSelectedLeds (std::set<int> (used.begin(), used.end()));
    };
    noneButton.onClick = [this]
    {
        doc.setSelectedBlocks ({});
        doc.setSelectedLeds ({});
    };
    addAndMakeVisible (allButton);
    addAndMakeVisible (noneButton);

    doc.addChangeListener (this);
    refresh();
}

LedList::~LedList()
{
    doc.removeChangeListener (this);
    list.setModel (nullptr);
}

void LedList::changeListenerCallback (ChangeBroadcaster*)
{
    refresh();
}

void LedList::refresh()
{
    const ScopedValueSetter<bool> guard (updating, true);

    rows = doc.laneLeds();
    list.updateContent();

    SparseSet<int> sel;
    for (int i = 0; i < (int) rows.size(); ++i)
        if (doc.selectedLeds().count (rows[(std::size_t) i]) > 0)
            sel.addRange ({ i, i + 1 });
    list.setSelectedRows (sel, dontSendNotification);
    list.repaint();
}

void LedList::paintListBoxItem (int row, Graphics& g, int width, int height, bool selected)
{
    if (row < 0 || row >= (int) rows.size())
        return;

    const int led = rows[(std::size_t) row];
    const Colour text = findColour (ThemeColours::defaultText);
    const Colour accent = findColour (ThemeColours::highlightedFill);

    if (selected)
        g.fillAll (accent.withAlpha (0.3f));

    std::vector<const Block*> blocks;
    for (const auto& b : doc.pattern().blocks)
        if (b.led == led)
            blocks.push_back (&b);
    std::sort (blocks.begin(), blocks.end(), [] (const Block* a, const Block* b) { return a->start < b->start; });

    // dot: filled when the LED has a waveform
    const juce::Rectangle<float> dot (8.0f, height / 2.0f - 6.0f, 12.0f, 12.0f);
    g.setColour (blocks.empty() ? text.withAlpha (0.3f) : accent);
    if (blocks.empty())
        g.drawEllipse (dot, 1.5f);
    else
        g.fillEllipse (dot);

    g.setColour (text);
    g.setFont (FontOptions (13.0f, Font::bold));
    g.drawText ("LED " + String (led), 28, 3, 70, 18, Justification::centredLeft);
    g.setFont (FontOptions (12.0f));
    g.setColour (text.withAlpha (0.7f));
    g.drawText (ledPlace (led), 92, 3, width - 96, 18, Justification::centredLeft);

    String summary;
    if (blocks.empty())
        summary = "no waveform yet";
    else
    {
        summary = describeWave (*blocks.front());
        const double startMs = blocks.front()->start * 1000.0;
        if (startMs > 0)
            summary << ", from " << String (startMs, 1).trimCharactersAtEnd ("0").trimCharactersAtEnd (".") << " ms";
        if (blocks.size() > 1)
            summary << " (+" << (int) blocks.size() - 1 << " more)";
    }
    g.setColour (text.withAlpha (blocks.empty() ? 0.5f : 0.9f));
    g.drawText (summary, 28, 20, width - 32, 17, Justification::centredLeft, true);
}

void LedList::selectedRowsChanged (int)
{
    if (updating)
        return;

    std::set<int> leds;
    for (int i = 0; i < list.getNumSelectedRows(); ++i)
    {
        const int r = list.getSelectedRow (i);
        if (r >= 0 && r < (int) rows.size())
            leds.insert (rows[(std::size_t) r]);
    }

    doc.setSelectedBlocks ({});
    doc.setSelectedLeds (leds);
}

void LedList::paint (Graphics& g)
{
    if (rows.empty())
    {
        g.setColour (findColour (ThemeColours::defaultText).withAlpha (0.7f));
        g.setFont (FontOptions (13.0f));
        g.drawFittedText ("Click LEDs on the probe (Ctrl-click or drag for more), pick a waveform on the right, then Apply.",
                          list.getBounds().reduced (10), Justification::centredTop, 4);
    }
}

void LedList::resized()
{
    auto b = getLocalBounds();
    auto top = b.removeFromTop (26);
    noneButton.setBounds (top.removeFromRight (56));
    top.removeFromRight (4);
    allButton.setBounds (top.removeFromRight (84));
    header.setBounds (top);
    b.removeFromTop (6);
    list.setBounds (b);
}

//==============================================================================

PatternPanel::PatternPanel (PatternDocument& d) : doc (d), ledList (d), designer (d), timeline (d)
{
    setWantsKeyboardFocus (true);

    fileButton.setTooltip ("New, open or save a pattern design (.json), export the pattern file (.bin), or upload an existing .bin");
    fileButton.onClick = [this] { showFileMenu(); };
    undoButton.setTooltip ("Undo (Ctrl+Z)");
    redoButton.setTooltip ("Redo (Ctrl+Y)");
    undoButton.onClick = [this] { doc.undo(); };
    redoButton.onClick = [this] { doc.redo(); };
    for (auto* b : { &fileButton, &undoButton, &redoButton })
        addAndMakeVisible (b);

    for (auto* l : { &lengthLabel, &repeatLabel, &usageLabel, &boardLabel })
    {
        l->setFont (FontOptions (13.0f));
        addAndMakeVisible (l);
    }
    lengthLabel.setJustificationType (Justification::centredRight);
    repeatLabel.setJustificationType (Justification::centredRight);
    boardLabel.setJustificationType (Justification::centredRight);

    lengthEditor.setInputRestrictions (10, "0123456789.");
    lengthEditor.setJustification (Justification::centredRight);
    lengthEditor.setTooltip ("Length of one run of the pattern. The board holds up to 13.1 s.");
    lengthEditor.onReturnKey = lengthEditor.onFocusLost = [this] { commitLength(); };
    repeatEditor.setInputRestrictions (6, "0123456789");
    repeatEditor.setJustification (Justification::centredRight);
    repeatEditor.setTooltip ("How many times the board plays the pattern each time stim starts");
    repeatEditor.onReturnKey = repeatEditor.onFocusLost = [this] { commitRepeats(); };
    addAndMakeVisible (lengthEditor);
    addAndMakeVisible (repeatEditor);

    addAndMakeVisible (ledList);
    addAndMakeVisible (designer);

    timeline.onLaneNameClicked = [this] (int led)
    {
        doc.setSelectedBlocks ({});
        doc.setSelectedLeds ({ led });
    };
    addAndMakeVisible (timeline);

    doc.addChangeListener (this);
    refresh();
}

PatternPanel::~PatternPanel()
{
    doc.removeChangeListener (this);
}

void PatternPanel::changeListenerCallback (ChangeBroadcaster*)
{
    // blocks picked on the timeline select their LEDs (so the designer loads them)
    if (doc.selectedBlocks() != lastBlockSelection)
    {
        lastBlockSelection = doc.selectedBlocks();
        if (! lastBlockSelection.empty())
        {
            std::set<int> leds;
            for (int id : lastBlockSelection)
                if (const Block* b = doc.findBlock (id))
                    leds.insert (b->led);
            if (leds != doc.selectedLeds())
                doc.setSelectedLeds (leds);
        }
    }

    refresh();
}

void PatternPanel::refresh()
{
    const Pattern& p = doc.pattern();

    if (! lengthEditor.hasKeyboardFocus (true))
        lengthEditor.setText (String (p.duration, 3).trimCharactersAtEnd ("0").trimCharactersAtEnd ("."), false);
    if (! repeatEditor.hasKeyboardFocus (true))
        repeatEditor.setText (String (p.repeatCount), false);

    const double maxSeconds = (double) kMaxSamples / kSampleRateHz;
    String usage;
    usage << "plays " << formatSeconds (p.duration * p.repeatCount) << " in total.  Board memory: "
          << formatSeconds (p.duration) << " of " << formatSeconds (maxSeconds);
    usageLabel.setText (usage, dontSendNotification);
    usageLabel.setColour (Label::textColourId, fitsInFlash (p) ? findColour (ThemeColours::defaultText) : Colours::red);

    undoButton.setActive (doc.canUndo());
    redoButton.setActive (doc.canRedo());

    resized();
}

void PatternPanel::setBoardStatus (const String& text, bool upToDate)
{
    boardLabel.setText (text, dontSendNotification);
    boardLabel.setColour (Label::textColourId, upToDate ? findColour (ThemeColours::defaultText) : Colour (0xffd35400));
}

void PatternPanel::commitLength()
{
    const double v = lengthEditor.getText().getDoubleValue();
    if (v <= 0 || v == doc.pattern().duration)
    {
        refresh();
        return;
    }
    doc.modify ([v] (Pattern& p) { p.duration = v; });
}

void PatternPanel::commitRepeats()
{
    const int v = jmax (1, repeatEditor.getText().getIntValue());
    if (v == doc.pattern().repeatCount)
    {
        refresh();
        return;
    }
    doc.modify ([v] (Pattern& p) { p.repeatCount = v; });
}

void PatternPanel::paint (Graphics& g)
{
    g.fillAll (findColour (ThemeColours::componentBackground));
    g.setColour (findColour (ThemeColours::outline));
    g.drawHorizontalLine (headerBottom, 0.0f, (float) getWidth());
    g.drawVerticalLine (ledList.getRight() + 7, (float) ledList.getY(), (float) ledList.getBottom());
    g.drawHorizontalLine (timeline.getY() - 6, 0.0f, (float) getWidth());
}

void PatternPanel::resized()
{
    auto b = getLocalBounds().reduced (10, 8);

    auto header = b.removeFromTop (26);
    fileButton.setBounds (header.removeFromLeft (64));
    header.removeFromLeft (6);
    undoButton.setBounds (header.removeFromLeft (56));
    header.removeFromLeft (4);
    redoButton.setBounds (header.removeFromLeft (56));
    header.removeFromLeft (10);
    lengthLabel.setBounds (header.removeFromLeft (textWidth (lengthLabel.getText()) + 10));
    lengthEditor.setBounds (header.removeFromLeft (56));
    header.removeFromLeft (6);
    repeatLabel.setBounds (header.removeFromLeft (textWidth (repeatLabel.getText()) + 10));
    repeatEditor.setBounds (header.removeFromLeft (50));
    header.removeFromLeft (8);
    boardLabel.setBounds (header.removeFromRight (jmin (300, jmax (0, header.getWidth() - 200))));
    usageLabel.setBounds (header);

    headerBottom = b.getY() + 4;
    b.removeFromTop (12);

    // timeline: just tall enough for the LEDs in use (it scrolls beyond that)
    const int lanes = jmax (1, (int) doc.laneLeds().size());
    const int timelineH = jlimit (110, jmax (110, b.getHeight() / 3), TimelineView::kRulerHeight + lanes * TimelineView::kLaneHeight + 16);
    timeline.setBounds (b.removeFromBottom (timelineH));
    b.removeFromBottom (12);

    ledList.setBounds (b.removeFromLeft (jlimit (220, 320, b.getWidth() / 3)));
    b.removeFromLeft (14);
    designer.setBounds (b);
}

bool PatternPanel::keyPressed (const KeyPress& key)
{
    const auto mods = key.getModifiers();
    if (! mods.isCommandDown())
        return false;
    const int c = key.getKeyCode();
    if (c == 'Z' || c == 'z')
    {
        if (mods.isShiftDown())
            doc.redo();
        else
            doc.undo();
        return true;
    }
    if (c == 'Y' || c == 'y')
    {
        doc.redo();
        return true;
    }
    if (c == 'S' || c == 's')
    {
        savePattern (mods.isShiftDown());
        return true;
    }
    return false;
}

void PatternPanel::showFileMenu()
{
    PopupMenu menu;
    menu.addItem (1, "New pattern");
    menu.addItem (2, "Open design (.json)...");
    menu.addSeparator();
    menu.addItem (3, "Save design" + String (doc.file != File() ? " (" + doc.file.getFileName() + ")" : String()));
    menu.addItem (4, "Save design as...");
    menu.addSeparator();
    menu.addItem (5, "Export pattern file (.bin)...");
    menu.addItem (6, "Upload an existing pattern file (.bin)...", onUploadBinFile != nullptr);

    menu.showMenuAsync (PopupMenu::Options().withTargetComponent (&fileButton),
                        [this] (int result)
                        {
                            switch (result)
                            {
                                case 1: newPattern(); break;
                                case 2: openPattern(); break;
                                case 3: savePattern (false); break;
                                case 4: savePattern (true); break;
                                case 5: exportBin(); break;
                                case 6: chooseBinToUpload(); break;
                                default: break;
                            }
                        });
}

void PatternPanel::confirmDiscard (std::function<void()> then)
{
    if (! doc.dirty || doc.pattern().blocks.empty())
    {
        then();
        return;
    }

    AlertWindow::showOkCancelBox (AlertWindow::QuestionIcon, "Pattern stim",
                                  "Discard the changes to the current pattern?",
                                  "Discard", "Cancel", this,
                                  ModalCallbackFunction::create ([then] (int result)
                                                                 {
                                                                     if (result == 1)
                                                                         then();
                                                                 }));
}

void PatternPanel::newPattern()
{
    confirmDiscard ([this]
                    {
                        Pattern p;
                        p.probe = doc.pattern().probe;
                        p.repeatCount = doc.pattern().repeatCount;
                        doc.reset (p, File());
                        if (onPatternReplaced)
                            onPatternReplaced();
                    });
}

void PatternPanel::openPattern()
{
    confirmDiscard ([this]
                    {
                        chooser = std::make_unique<FileChooser> ("Open pattern design", doc.file, "*.json");
                        chooser->launchAsync (FileBrowserComponent::openMode | FileBrowserComponent::canSelectFiles,
                                              [this] (const FileChooser& fc)
                                              {
                                                  const File f = fc.getResult();
                                                  if (! f.existsAsFile())
                                                      return;

                                                  Pattern p;
                                                  std::string error;
                                                  if (! fromJson (f.loadFileAsString().toStdString(), p, error))
                                                  {
                                                      AlertWindow::showMessageBoxAsync (AlertWindow::WarningIcon, "Open pattern", String (error));
                                                      return;
                                                  }
                                                  doc.reset (p, f);
                                                  timeline.zoomToFit();
                                                  if (onPatternReplaced)
                                                      onPatternReplaced();
                                              });
                    });
}

void PatternPanel::savePattern (bool askForFile)
{
    auto write = [this] (File f)
    {
        if (! f.hasFileExtension ("json"))
            f = f.withFileExtension ("json");
        if (! f.replaceWithText (String::fromUTF8 (toJson (doc.pattern()).c_str())))
        {
            AlertWindow::showMessageBoxAsync (AlertWindow::WarningIcon, "Save pattern", "Couldn't write " + f.getFullPathName());
            return;
        }
        doc.file = f;
        doc.dirty = false;
        refresh();
    };

    if (! askForFile && doc.file != File())
    {
        write (doc.file);
        return;
    }

    chooser = std::make_unique<FileChooser> ("Save pattern design",
                                             doc.file == File() ? File::getSpecialLocation (File::userDocumentsDirectory).getChildFile ("pattern.json") : doc.file,
                                             "*.json");
    chooser->launchAsync (FileBrowserComponent::saveMode | FileBrowserComponent::canSelectFiles | FileBrowserComponent::warnAboutOverwriting,
                          [write] (const FileChooser& fc)
                          {
                              if (fc.getResult() != File())
                                  write (fc.getResult());
                          });
}

void PatternPanel::prepareOutput (const String& action, std::function<void (std::vector<std::uint8_t>)> onReady)
{
    const Pattern& p = doc.pattern();

    if (p.blocks.empty())
    {
        AlertWindow::showMessageBoxAsync (AlertWindow::InfoIcon, action, "The pattern is empty: select LEDs on the probe and apply a waveform first.");
        return;
    }

    const auto problems = validate (p);
    if (! problems.empty())
    {
        String msg;
        for (const auto& s : problems)
            msg << "- " << String::fromUTF8 (s.c_str()).replace ("uA", kMicroAmps) << "\n";
        AlertWindow::showMessageBoxAsync (AlertWindow::WarningIcon, action, "Fix these first:\n\n" + msg);
        return;
    }

    auto generate = [this, action, onReady]
    {
        PatternData data;
        std::string error;
        if (! renderPattern (doc.pattern(), data, error))
        {
            AlertWindow::showMessageBoxAsync (AlertWindow::WarningIcon, action, String (error));
            return;
        }
        onReady (encodePatternFile (data, doc.pattern().probe));
    };

    if (isProbeMappingVerified (p.probe))
    {
        generate();
        return;
    }

    AlertWindow::showOkCancelBox (AlertWindow::WarningIcon, action,
                                  String ("The ") + probeMappingName (p.probe) + " LED mapping hasn't been verified on a device yet. Continue?",
                                  "Continue", "Cancel", this,
                                  ModalCallbackFunction::create ([generate] (int result)
                                                                 {
                                                                     if (result == 1)
                                                                         generate();
                                                                 }));
}

void PatternPanel::exportBin()
{
    prepareOutput ("Export pattern file", [this] (std::vector<std::uint8_t> bytes)
                   {
                       const File start = doc.file != File() ? doc.file.withFileExtension ("bin")
                                                             : File::getSpecialLocation (File::userDocumentsDirectory).getChildFile ("DACData.bin");
                       chooser = std::make_unique<FileChooser> ("Export pattern file", start, "*.bin");
                       chooser->launchAsync (FileBrowserComponent::saveMode | FileBrowserComponent::canSelectFiles | FileBrowserComponent::warnAboutOverwriting,
                                             [bytes] (const FileChooser& fc)
                                             {
                                                 File f = fc.getResult();
                                                 if (f == File())
                                                     return;
                                                 if (! f.hasFileExtension ("bin"))
                                                     f = f.withFileExtension ("bin");
                                                 f.deleteFile();
                                                 if (! f.appendData (bytes.data(), bytes.size()))
                                                     AlertWindow::showMessageBoxAsync (AlertWindow::WarningIcon, "Export pattern file", "Couldn't write " + f.getFullPathName());
                                             });
                   });
}

void PatternPanel::chooseBinToUpload()
{
    chooser = std::make_unique<FileChooser> ("Upload a pattern file", File::getSpecialLocation (File::userDocumentsDirectory), "*.bin");
    chooser->launchAsync (FileBrowserComponent::openMode | FileBrowserComponent::canSelectFiles,
                          [this] (const FileChooser& fc)
                          {
                              const File f = fc.getResult();
                              if (f.existsAsFile() && onUploadBinFile)
                                  onUploadBinFile (f);
                          });
}

} // namespace PatternEditor
