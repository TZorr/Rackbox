//
//  BlockPanel.cpp
//  Rackbox
//

#include "BlockPanel.h"

#include "Engine/Dsp.h"
#include "Engine/EffectCatalog.h"
#include "Palette.h"
#include "TextUtf8.h"

namespace
{
    constexpr int padding = 8;
    constexpr int titleHeight = 22;
    constexpr int rowHeight = 28, rowGap = 6;
    constexpr int dialSize = 50;   // the dials are as small as still reads well

    /** The full names do not fit four across; the title and the display keep them. */
    juce::String shortName (int category)
    {
        static const char* names[] { "Mod", "Reverb", "Delay", "Dist" };
        return names[juce::jlimit (0, EffectCatalog::numCategories - 1, category)];
    }
}

void BlockPanel::setChoice (const char* suffix, int value)
{
    if (auto* parameter = processor.parameters.getParameter (RackParams::blockId (block, suffix)))
    {
        parameter->beginChangeGesture();
        parameter->setValueNotifyingHost (parameter->convertTo0to1 ((float) value));
        parameter->endChangeGesture();
    }
}

BlockPanel::BlockPanel (RackboxProcessor& processorToUse, int blockIndex)
    : processor (processorToUse), block (blockIndex)
{
    auto& state = processor.parameters;
    const auto id = [this] (const char* suffix) { return RackParams::blockId (block, suffix); };

    a.attach    (state, id (RackParams::Block::a));
    b.attach    (state, id (RackParams::Block::b));
    mix.attach  (state, id (RackParams::Block::mix));

    bypassAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (state, id (RackParams::Block::bypass), bypass);

    syncAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (state, id (RackParams::Block::sync), sync);

    pre.onStep = [this] (int direction)
    {
        const auto current = processor.getBlockSettings (block).preDelay;
        setChoice (RackParams::Block::pre, (current + direction + Dsp::Sync::numPreDelays) % Dsp::Sync::numPreDelays);
    };

    juce::StringArray categories;
    for (int c = 0; c < EffectCatalog::numCategories; ++c)
        categories.add (shortName (c));

    type.setLabels (categories);
    type.onSelect = [this] (int i) { setChoice (RackParams::Block::type, i); };
    sub.onSelect  = [this] (int i) { setChoice (RackParams::Block::sub, i); };

    // Set after attach(): the attachment installs its own text functions.
    a.textFromValueFunction = [this] (double v)
    {
        const auto s = processor.getBlockSettings (block);
        const auto unit = EffectCatalog::info (s.category, s.subtype).unitA;

        // With Sync on, a time or a rate reads as the note value it will play.
        const auto timing = unit == EffectCatalog::Unit::TimeMs || unit == EffectCatalog::Unit::RateHz;
        return EffectCatalog::format (s.sync && timing ? EffectCatalog::Unit::Division : unit, (float) v);
    };

    b.textFromValueFunction = [this] (double v)
    {
        const auto s = processor.getBlockSettings (block);
        return EffectCatalog::format (EffectCatalog::info (s.category, s.subtype).unitB, (float) v);
    };

    for (auto* c : { (juce::Component*) &type, (juce::Component*) &sub, (juce::Component*) &a,
                     (juce::Component*) &b, (juce::Component*) &mix, (juce::Component*) &bypass,
                     (juce::Component*) &sync, (juce::Component*) &pre })
        addAndMakeVisible (c);

    refresh();
}

void BlockPanel::refresh()
{
    const auto s = processor.getBlockSettings (block);

    if (s.category != shownCategory)
    {
        juce::StringArray names;
        for (int i = 0; i < EffectCatalog::numSubtypes (s.category); ++i)
            names.add (EffectCatalog::info (s.category, i).name);

        sub.setLabels (names);
    }

    type.setSelected (s.category);
    sub.setSelected (s.subtype);

    if (s.category != shownCategory || s.subtype != shownSubtype)
    {
        const auto& info = EffectCatalog::info (s.category, s.subtype);
        a.setLabel (info.labelA);
        b.setLabel (info.labelB);

        shownCategory = s.category;
        shownSubtype = s.subtype;

        a.repaint();
        b.repaint();
        repaint();
    }

    // Sync exists only where the loaded effect has a time or a rate to follow. While it is on, the
    // knob steps through the note values instead of sliding.
    const auto syncing = s.sync && EffectCatalog::canSync (s.category, s.subtype);
    sync.setVisible (EffectCatalog::canSync (s.category, s.subtype));

    // A reverb has a pre-delay where the others have Sync. The tooltip carries the milliseconds
    // the note value comes to at the host's tempo: the one number the button itself cannot show.
    const auto hasPre = EffectCatalog::hasPreDelay (s.category);
    pre.setVisible (hasPre);

    if (hasPre)
    {
        pre.setText ("PRE " + juce::String (Dsp::Sync::preDelayNames[s.preDelay]), s.preDelay > 0);

        const auto bpm = processor.getHostBpm();
        const auto ms = Dsp::Sync::preDelaySeconds (s.preDelay, bpm, processor.getHostBeatsPerBar()) * 1000.0f;
        const auto tempo = juce::String (bpm, std::abs (bpm - std::round (bpm)) < 0.005 ? 0 : 1);
        const auto tip = s.preDelay == 0 ? juce::String ("Pre-delay off. Click for 1/128 up to one bar, right-click to go back.")
                                         : "Pre-delay " + juce::String (Dsp::Sync::preDelayNames[s.preDelay]).toLowerCase()
                                           + " = " + juce::String (ms, 1) + " ms at " + tempo + " BPM";

        if (tip != shownTip)
        {
            shownTip = tip;
            pre.setTooltip (tip);
        }
    }

    if (syncing != shownSync)
    {
        shownSync = syncing;
        a.setRange (0.0, 1.0, syncing ? 1.0 / (Dsp::Sync::numDivisions - 1) : 0.0);
        a.repaint();
    }

    if (s.bypass != shownBypass)
    {
        shownBypass = s.bypass;
        repaint();
    }
}

void BlockPanel::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();

    g.setColour (Palette::section);
    g.fillRoundedRectangle (bounds, 6.0f);

    auto titleRow = getLocalBounds().reduced (padding + 2, 0).removeFromTop (titleHeight).toFloat().withTrimmedTop (3.0f);

    g.setColour (Palette::blockColour (block));
    g.fillEllipse (titleRow.getX(), titleRow.getCentreY() - 3.0f, 6.0f, 6.0f);

    g.setColour (Palette::ink);
    g.setFont (Palette::label (10.0f));
    g.drawText ("BLOCK " + juce::String (block + 1), titleRow.withTrimmedLeft (12.0f), juce::Justification::centredLeft);
}

void BlockPanel::resized()
{
    auto area = getLocalBounds().reduced (padding, 0);

    // Bypass sits in the title row, at the right: the effect's name is already on
    // the buttons, so the row's right half was free.
    auto titleRow = area.removeFromTop (titleHeight);
    bypass.setBounds (titleRow.removeFromRight (68).withSizeKeepingCentre (68, 18).translated (0, 1));
    titleRow.removeFromRight (6);
    auto slot = titleRow.removeFromRight (68).withSizeKeepingCentre (68, 18).translated (0, 1);
    sync.setBounds (slot);
    pre.setBounds (slot);

    area.removeFromBottom (4);

    // Two halves: the selectors stacked on the left, the three knobs on the right.
    auto selectors = area.removeFromLeft (area.getWidth() / 2 - 6);
    area.removeFromLeft (12);

    selectors = selectors.withSizeKeepingCentre (selectors.getWidth(), 2 * rowHeight + rowGap);
    type.setBounds (selectors.removeFromTop (rowHeight));
    selectors.removeFromTop (rowGap);
    sub.setBounds (selectors.removeFromTop (rowHeight));

    auto knobRow = area.withSizeKeepingCentre (area.getWidth(), dialSize + Knob::labelHeight);
    const auto cell = knobRow.getWidth() / 3;

    for (auto* k : { &a, &b, &mix })
        k->setBounds (knobRow.removeFromLeft (cell));
}
