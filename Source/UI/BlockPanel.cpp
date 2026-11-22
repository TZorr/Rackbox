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
    // The mockup's measures.
    constexpr int stripWidth = 26, stripInset = 6;
    constexpr int choiceColumn = 300;
    constexpr int typeHeight = 30, subHeight = 28, choiceGap = 8;
    constexpr int nameRow = 30;

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

BlockPanel::Power::Power (int blockIndex)
    : juce::Button ("Block " + juce::String (blockIndex + 1)), title ("BLOCK " + juce::String (blockIndex + 1))
{
    setClickingTogglesState (true);
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
}

void BlockPanel::Power::paintButton (juce::Graphics& g, bool highlighted, bool down)
{
    // The toggle is the bypass: on means off.
    const auto bounds = getLocalBounds().toFloat().reduced (0.5f);
    const auto on = ! getToggleState();
    const auto colours = Palette::zone (zone);

    g.setColour (highlighted || down ? Palette::powerFace.brighter (0.07f) : Palette::powerFace);
    g.fillRoundedRectangle (bounds, 3.0f);
    g.setColour (Palette::powerLine);
    g.drawRoundedRectangle (bounds, 3.0f, 1.0f);

    // The dot over the title, the title read bottom to top; centred as a pair.
    const auto font = Palette::font (11.0f, true, 0.14f);
    const auto length = juce::GlyphArrangement::getStringWidth (font, title);
    const auto top = bounds.getCentreY() - (6.0f + 6.0f + length) * 0.5f;

    g.setColour (on ? colours.line : Palette::powerOffDot);
    g.fillEllipse (bounds.getCentreX() - 3.0f, top, 6.0f, 6.0f);

    const auto centre = juce::Point<float> (bounds.getCentreX(), top + 12.0f + length * 0.5f);
    juce::Graphics::ScopedSaveState state (g);
    g.addTransform (juce::AffineTransform::rotation (-juce::MathConstants<float>::halfPi, centre.x, centre.y));
    g.setColour (on ? colours.label : Palette::powerOffInk);
    g.setFont (font);
    g.drawText (title, juce::Rectangle<float> (length + 4.0f, 14.0f).withCentre (centre), juce::Justification::centred, false);
}

BlockPanel::BlockPanel (RackboxProcessor& processorToUse, int blockIndex)
    : processor (processorToUse), block (blockIndex), bypass (blockIndex)
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
    type.zoneFor = [] (int category) { return Palette::categoryZone (category); };
    sub.setStyle (ChoiceButtons::Style::outlined);
    mix.setZone (Palette::ZoneId::neutral);
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

    if (s.category != shownCategory)
    {
        const auto zone = Palette::categoryZone (s.category);
        sub.setZone (zone);
        a.setZone (zone);
        b.setZone (zone);
        bypass.zone = zone;
        bypass.repaint();
    }

    if (s.category != shownCategory || s.subtype != shownSubtype)
    {
        const auto& info = EffectCatalog::info (s.category, s.subtype);
        a.setLabel (info.labelA);
        b.setLabel (info.labelB);

        shownCategory = s.category;
        shownSubtype = s.subtype;

        shownName = juce::String (EffectCatalog::categoryName (s.category)) + utf8 (" \xc2\xb7 ") + info.name;

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
        // A bypassed block's effect side fades back; it still turns.
        shownBypass = s.bypass;

        for (auto* c : { (juce::Component*) &a, (juce::Component*) &b, (juce::Component*) &mix,
                         (juce::Component*) &sync, (juce::Component*) &pre })
            c->setAlpha (s.bypass ? 0.4f : 1.0f);

        repaint();
    }

    // SYNC and PRE sit at the right of the name, as wide as their word.
    {
        const auto right = getWidth() - 10;
        const auto slot = [&] (int width) { return juce::Rectangle<int> (right - width, nameArea.getCentreY() - 11, width, 22); };
        sync.setBounds (slot (sync.getIdealWidth()));
        pre.setBounds (slot (pre.getIdealWidth()));
    }
}

void BlockPanel::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();

    g.setColour (Palette::rackEdge);
    g.fillRoundedRectangle (bounds, 5.0f);
    g.setColour (Palette::rackPanel);
    g.fillRoundedRectangle (bounds.reduced (1.0f), 4.0f);

    // The upright between the buttons and the effect.
    g.setColour (Palette::rackDivider);
    g.fillRect (nameArea.getX() - 1, 1, 1, getHeight() - 2);
}

void BlockPanel::paintOverChildren (juce::Graphics& g)
{
    // The effect's name, faded with its knobs while the block is bypassed;
    // the upright before Mix.
    const auto alpha = shownBypass ? 0.4f : 1.0f;
    const auto zone = Palette::zone (Palette::categoryZone (shownCategory));

    g.setColour (zone.label.withMultipliedAlpha (alpha));
    g.setFont (Palette::font (11.0f, true, 0.06f));
    auto name = nameArea.withTrimmedLeft (12).withTrimmedRight (10);
    if (sync.isVisible())
        name.setRight (sync.getX() - 8);
    if (pre.isVisible())
        name.setRight (pre.getX() - 8);
    g.drawFittedText (shownName.toUpperCase(), name, juce::Justification::centredLeft, 1, 0.8f);

    g.setColour (Palette::rackDivider.withMultipliedAlpha (alpha));
    g.fillRect (mixDivider);
}

void BlockPanel::resized()
{
    auto area = getLocalBounds().reduced (1);

    // The strip down the left edge.
    bypass.setBounds (area.removeFromLeft (stripInset + stripWidth).withTrimmedLeft (stripInset).reduced (0, 7));

    // The buttons, 12 in from either side, centred upright.
    auto choices = area.removeFromLeft (choiceColumn).reduced (12, 0);
    choices = choices.withSizeKeepingCentre (choices.getWidth(), typeHeight + choiceGap + subHeight);
    type.setBounds (choices.removeFromTop (typeHeight));
    choices.removeFromTop (choiceGap);
    sub.setBounds (choices);

    // The effect: its name over the knobs, after a 1-point upright.
    area.removeFromLeft (1);
    nameArea = area.removeFromTop (nameRow);
    area.removeFromBottom (6);
    area = area.reduced (4, 0);

    const auto height = a.getStackedHeight();
    const auto cell = (float) area.getWidth() / 3.0f;
    const auto y = area.getCentreY() - height / 2;
    int column = 0;

    for (auto* k : { &a, &b, &mix })
    {
        const auto x = area.getX() + juce::roundToInt (cell * (float) column);
        const auto w = area.getX() + juce::roundToInt (cell * (float) (column + 1)) - x;
        k->setBounds (x, y, w, height);
        ++column;
    }

    mixDivider = { mix.getX(), y, 1, height };
    refresh();   // places SYNC and PRE beside the name
}
