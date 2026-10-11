//
//  BlockPanel.h
//  Rackbox
//
//  One effect block's section of the panel: a row of Type buttons over a row of
//  Subtype buttons on the left, the three knobs (A, B, Mix) whose names follow
//  the effect that is loaded on the right, and Bypass in the title row - with Sync beside it when the effect has a time or rate knob that can follow the host tempo -
//  or, on a reverb, a PRE button in Sync's place that steps a tempo-synced pre-delay.
//
//  Two things here are not plain wiring.
//
//  The A and B knobs are the same two parameters whatever the effect, so their
//  name and the way their value is printed ("0.40 Hz", "250 ms", "x8.0") are
//  taken from the catalog every time the loaded effect changes.
//
//  The Subtype row shows the *current category's* list. Its parameter always
//  runs 0-3, but a category with three entries shows three buttons, not four
//  with one dead. The stored value is never rewritten; the block clamps it on
//  its own side, and the row lights the clamped one (a stored 3 in Reverb lights
//  Hall). The buttons write their parameter as a change gesture, so a host
//  records a click as automation, and refresh() reads the parameter back, so a
//  host or a preset that changes it moves the lit button.
//

#pragma once

#include "ChoiceButtons.h"
#include "Knob.h"
#include "FlatToggle.h"
#include "StepButton.h"
#include "PluginProcessor.h"

class BlockPanel : public juce::Component
{
public:
    BlockPanel (RackboxProcessor& processorToUse, int blockIndex);

    /** Follows the parameters: names, texts and the subtype range. Call from the UI timer. */
    void refresh();

    void paint (juce::Graphics&) override;
    void resized() override;

    /** What the knobs currently print; the checks read these instead of looking at pixels. */
    juce::String subtypeText()    { return sub.getSelectedText(); }
    juce::String labelAText()     { return a.getTextFromValue (a.getValue()); }
    juce::String labelBText()     { return b.getTextFromValue (b.getValue()); }
    /** What a click on the n-th Type or Subtype button does. */
    void clickType (int index)      { type.onSelect (index); }
    void clickSubtype (int index)   { sub.onSelect (index); }

    bool syncVisible() const            { return sync.isVisible(); }
    bool preVisible() const             { return pre.isVisible(); }
    juce::String preText() const        { return pre.getText(); }
    juce::String preTooltip()           { return pre.getTooltip(); }
    void clickPre (int direction)       { pre.onStep (direction); }
    juce::String bypassText()     { return bypass.getToggleState() ? "bypassed" : "active"; }

private:
    RackboxProcessor& processor;
    int block;

    void setChoice (const char* suffix, int value);

    ChoiceButtons type, sub;
    Knob a { "A" }, b { "B" }, mix { "Mix" };
    FlatToggle bypass { "Bypass" }, sync { "Sync" };
    StepButton pre;   // a reverb's pre-delay, in Sync's place
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> bypassAttachment, syncAttachment;

    int shownCategory = -1, shownSubtype = -1;
    juce::String shownTip;
    bool shownBypass = false, shownSync = false;
};
