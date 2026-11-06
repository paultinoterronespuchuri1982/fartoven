#include "PluginEditor.h"

namespace
{
    const char* const modeNames[] = { "KICK", "SNARE", "TOMS", "CYMBALS", "ROOM" };
    constexpr int numModes = 5;
    constexpr int captionHeight = 18;
    constexpr int blockTitleHeight = 22;
    constexpr float knobStartAngle = juce::MathConstants<float>::pi * 1.25f;
    constexpr float knobEndAngle   = juce::MathConstants<float>::pi * 2.75f;
}

//==============================================================================
FartovenLookAndFeel::FartovenLookAndFeel()
{
    setColour (juce::ComboBox::backgroundColourId, FartovenStyle::panel);
    setColour (juce::ComboBox::outlineColourId, FartovenStyle::panelEdge);
    setColour (juce::ComboBox::textColourId, FartovenStyle::textMain);
    setColour (juce::ComboBox::arrowColourId, FartovenStyle::accent);
    setColour (juce::PopupMenu::backgroundColourId, FartovenStyle::panel);
    setColour (juce::PopupMenu::textColourId, FartovenStyle::textMain);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, FartovenStyle::accent.withAlpha (0.3f));
    setColour (juce::ToggleButton::textColourId, FartovenStyle::textMain);
    setColour (juce::ToggleButton::tickColourId, FartovenStyle::accent);
    setColour (juce::ToggleButton::tickDisabledColourId, FartovenStyle::textDim);
}

void FartovenLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                                            float sliderPos, float startAngle, float endAngle,
                                            juce::Slider&)
{
    auto bounds = juce::Rectangle<float> ((float) x, (float) y, (float) width, (float) height).reduced (6.0f);
    const auto radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const auto centre = bounds.getCentre();
    const auto angle  = startAngle + sliderPos * (endAngle - startAngle);
    const juce::PathStrokeType stroke (4.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded);

    juce::Path track;
    track.addCentredArc (centre.x, centre.y, radius, radius, 0.0f, startAngle, endAngle, true);
    g.setColour (FartovenStyle::panelEdge);
    g.strokePath (track, stroke);

    juce::Path valueArc;
    valueArc.addCentredArc (centre.x, centre.y, radius, radius, 0.0f, startAngle, angle, true);
    g.setColour (FartovenStyle::accent);
    g.strokePath (valueArc, stroke);

    g.setColour (FartovenStyle::panel);
    g.fillEllipse (centre.x - radius * 0.68f, centre.y - radius * 0.68f, radius * 1.36f, radius * 1.36f);

    const juce::Line<float> pointer (centre.x + radius * 0.25f * std::sin (angle),
                                     centre.y - radius * 0.25f * std::cos (angle),
                                     centre.x + radius * 0.65f * std::sin (angle),
                                     centre.y - radius * 0.65f * std::cos (angle));
    g.setColour (FartovenStyle::textMain);
    g.drawLine (pointer, 2.0f);
}

void FartovenLookAndFeel::drawComboBox (juce::Graphics& g, int width, int height, bool,
                                        int, int, int, int, juce::ComboBox& box)
{
    auto bounds = juce::Rectangle<float> (0.0f, 0.0f, (float) width, (float) height);
    g.setColour (box.findColour (juce::ComboBox::backgroundColourId));
    g.fillRoundedRectangle (bounds, 4.0f);
    g.setColour (box.findColour (juce::ComboBox::outlineColourId));
    g.drawRoundedRectangle (bounds.reduced (0.5f), 4.0f, 1.0f);

    g.setColour (box.findColour (juce::ComboBox::arrowColourId));
    juce::Path arrow;
    arrow.addTriangle (width - 14.0f, height * 0.4f, width - 6.0f, height * 0.4f, width - 10.0f, height * 0.65f);
    g.fillPath (arrow);
}

//==============================================================================
ModeSelector::ModeSelector (juce::RangedAudioParameter& modeParam) : param (modeParam)
{
    startTimerHz (30);
}

void ModeSelector::resized()
{
    cells.clear();
    const auto area = getLocalBounds().toFloat();
    const float w = area.getWidth() / (float) numModes;
    for (int i = 0; i < numModes; ++i)
        cells.add (area.withX (i * w).withWidth (w).reduced (4.0f));
}

void ModeSelector::paint (juce::Graphics& g)
{
    for (int i = 0; i < cells.size(); ++i)
    {
        const bool active = (i == selected);
        const auto cell = cells.getReference (i);

        g.setColour (active ? FartovenStyle::accent.withAlpha (0.14f) : FartovenStyle::panel);
        g.fillRoundedRectangle (cell, 6.0f);
        g.setColour (active ? FartovenStyle::accent : FartovenStyle::panelEdge);
        g.drawRoundedRectangle (cell.reduced (0.5f), 6.0f, active ? 1.5f : 1.0f);

        auto iconArea = cell.withTrimmedBottom (26.0f).reduced (8.0f);
        g.setColour (active ? FartovenStyle::accent : FartovenStyle::textDim);
        g.strokePath (iconFor (i, iconArea), juce::PathStrokeType (2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        g.setColour (active ? FartovenStyle::textMain : FartovenStyle::textDim);
        g.setFont (juce::FontOptions (12.0f, juce::Font::bold));
        g.drawText (modeNames[i], cell.withTop (cell.getBottom() - 24.0f), juce::Justification::centred, false);
    }
}

void ModeSelector::mouseDown (const juce::MouseEvent& e)
{
    for (int i = 0; i < cells.size(); ++i)
        if (cells.getReference (i).contains (e.position))
        {
            // Para AudioParameterChoice el valor normalizado es índice / (n - 1).
            param.setValueNotifyingHost (param.convertTo0to1 ((float) i));
            return;
        }
}

void ModeSelector::timerCallback()
{
    const int index = juce::jlimit (0, numModes - 1, juce::roundToInt (param.getValue() * (numModes - 1)));
    if (index != selected)
    {
        selected = index;
        repaint();
    }
}

juce::Path ModeSelector::iconFor (int mode, juce::Rectangle<float> area)
{
    juce::Path p;
    const auto c = area.getCentre();
    const float r = juce::jmin (area.getWidth(), area.getHeight()) * 0.42f;

    switch (mode)
    {
        case 0: // Kick: aro con centro
            p.addEllipse (c.x - r, c.y - r, 2 * r, 2 * r);
            p.addEllipse (c.x - r * 0.3f, c.y - r * 0.3f, 0.6f * r, 0.6f * r);
            break;
        case 1: // Snare: parche y bordones
            p.addEllipse (c.x - r * 1.2f, c.y - r * 0.6f, 2.4f * r, 1.2f * r);
            for (int k = -1; k <= 1; ++k)
            {
                p.startNewSubPath (c.x - r * 0.9f, c.y + r * 0.25f * k + r * 0.2f);
                p.lineTo (c.x + r * 0.9f, c.y + r * 0.25f * k + r * 0.2f);
            }
            break;
        case 2: // Toms: dos aros
            p.addEllipse (c.x - 1.3f * r, c.y - 0.6f * r, 1.2f * r, 1.2f * r);
            p.addEllipse (c.x + 0.1f * r, c.y - 0.2f * r, 1.2f * r, 1.2f * r);
            break;
        case 3: // Platos: campana y tallo
            p.addEllipse (c.x - r * 1.2f, c.y - r * 0.5f, 2.4f * r, 0.8f * r);
            p.startNewSubPath (c.x, c.y + 0.3f * r);
            p.lineTo (c.x, c.y + r);
            break;
        case 4: // Sala: rectángulo con onda
            p.addRoundedRectangle (c.x - r, c.y - r * 0.8f, 2 * r, 1.6f * r, 3.0f);
            p.startNewSubPath (c.x - r * 0.6f, c.y);
            p.quadraticTo (c.x, c.y - r * 0.5f, c.x + r * 0.6f, c.y);
            break;
        default:
            break;
    }
    return p;
}

//==============================================================================
PeakMeter::PeakMeter (std::atomic<float>& peakSource) : source (peakSource)
{
    startTimerHz (30);
}

void PeakMeter::timerCallback()
{
    // Subida instantánea, caída suave hacia el nuevo pico
    displayed = juce::jmax (source.load(), displayed * 0.92f);
    repaint();
}

void PeakMeter::paint (juce::Graphics& g)
{
    auto area = getLocalBounds().toFloat().reduced (2.0f);
    g.setColour (FartovenStyle::panelEdge);
    g.fillRoundedRectangle (area, 4.0f);

    // Escala lineal en dB: -60 dBFS a 0 dBFS
    const float db = juce::Decibels::gainToDecibels (displayed, -60.0f);
    const float fraction = juce::jlimit (0.0f, 1.0f, (db + 60.0f) / 60.0f);

    auto fill = area.withWidth (area.getWidth() * fraction);
    g.setColour (db > -1.0f ? juce::Colours::orangered : FartovenStyle::accent);
    g.fillRoundedRectangle (fill, 4.0f);

    g.setColour (FartovenStyle::textDim);
    g.setFont (juce::FontOptions (11.0f));
    g.drawText (juce::String (db, 1) + " dBFS", area, juce::Justification::centred, false);
}

//==============================================================================
FartovenEditor::FartovenEditor (FartovenProcessor& p)
    : AudioProcessorEditor (&p),
      processor (p),
      modeSelector (*p.apvts.getParameter ("mode")),
      meter (p.outputPeak)
{
    setLookAndFeel (&lookAndFeel);
    setSize (1000, 620);

    addAndMakeVisible (modeSelector);
    addAndMakeVisible (meter);

    addSlider (low,       "low",       "Low");
    addSlider (mid,       "mid",       "Mid");
    addSlider (high,      "high",      "High");
    addSlider (tight,     "tight",     "Tight");
    addSlider (smooth,    "smooth",    "Smooth");
    addSlider (punch,     "punch",     "Punch");
    addSlider (threshold, "threshold", "Thresh");
    addSlider (release,   "release",   "Release");
    addSlider (parallel,  "parallel",  "Parallel");
    addSlider (drive,     "drive",     "Drive");
    addSlider (satMix,    "satMix",    "Sat Mix");
    addSlider (output,    "output",    "Output");

    satType.addItem ("Tape", 1);
    satType.addItem ("Tube", 2);
    satType.addItem ("Iron", 3);
    addAndMakeVisible (satType);
    satTypeAttachment = std::make_unique<ComboAttachment> (p.apvts, "satMode", satType);

    clipper.setLookAndFeel (&lookAndFeel);
    addAndMakeVisible (clipper);
    clipperAttachment = std::make_unique<ButtonAttachment> (p.apvts, "clipper", clipper);

    bypass.setLookAndFeel (&lookAndFeel);
    addAndMakeVisible (bypass);
    bypassAttachment = std::make_unique<ButtonAttachment> (p.apvts, "bypass", bypass);
}

FartovenEditor::~FartovenEditor()
{
    clipper.setLookAndFeel (nullptr);
    bypass.setLookAndFeel (nullptr);
    setLookAndFeel (nullptr);
}

void FartovenEditor::addSlider (juce::Slider& s, const char* paramId, const juce::String& caption)
{
    s.setSliderStyle (juce::Slider::RotaryVerticalDrag);
    s.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 64, 16);
    s.setColour (juce::Slider::textBoxTextColourId, FartovenStyle::textMain);
    s.setColour (juce::Slider::textBoxBackgroundColourId, FartovenStyle::background);
    s.setColour (juce::Slider::textBoxOutlineColourId, FartovenStyle::panelEdge);
    s.setName (caption);
    addAndMakeVisible (s);
    sliderAttachments.push_back (std::make_unique<SliderAttachment> (processor.apvts, paramId, s));
}

void FartovenEditor::paint (juce::Graphics& g)
{
    g.fillAll (FartovenStyle::background);

    g.setColour (FartovenStyle::textMain);
    g.setFont (juce::FontOptions (20.0f, juce::Font::bold));
    g.drawText ("FARTOVEN DRUMS", 16, 10, 400, 30, juce::Justification::left, false);

    drawBlock (g, blockToneArea,      "TONO");
    drawBlock (g, blockTightArea,     "LIMPIEZA");
    drawBlock (g, blockBodyArea,      "CUERPO");
    drawBlock (g, blockCharacterArea, "CARÁCTER Y SALIDA");

    // Captions de cada perilla, encima de su celda
    g.setFont (juce::FontOptions (12.0f));
    g.setColour (FartovenStyle::textDim);
    for (auto& [slider, area] : knobLabels)
        g.drawText (slider->getName(), area, juce::Justification::centred, false);

    // Etiqueta del selector de carácter
    g.setColour (FartovenStyle::textDim);
    g.drawText ("Sat Type", satTypeLabelArea, juce::Justification::centred, false);
}

void FartovenEditor::drawBlock (juce::Graphics& g, juce::Rectangle<int> area, const juce::String& title)
{
    g.setColour (FartovenStyle::panel);
    g.fillRoundedRectangle (area.toFloat(), 8.0f);
    g.setColour (FartovenStyle::panelEdge);
    g.drawRoundedRectangle (area.toFloat().reduced (0.5f), 8.0f, 1.0f);

    g.setColour (FartovenStyle::accent);
    g.setFont (juce::FontOptions (12.0f, juce::Font::bold));
    g.drawText (title, area.withHeight (blockTitleHeight).reduced (12, 0), juce::Justification::left, false);
}

void FartovenEditor::resized()
{
    auto bounds = getLocalBounds().reduced (14);
    bounds.removeFromTop (34);                 // Cabecera con el nombre

    modeSelector.setBounds (bounds.removeFromTop (84));
    bounds.removeFromTop (10);

    auto footer = bounds.removeFromBottom (44);
    bounds.removeFromBottom (10);

    const int gap = 10;
    auto topRow    = bounds.removeFromTop (bounds.getHeight() / 2 - gap / 2);
    bounds.removeFromTop (gap);
    auto bottomRow = bounds;

    const int halfW = (topRow.getWidth() - gap) / 2;
    blockToneArea  = topRow.removeFromLeft (halfW);
    topRow.removeFromLeft (gap);
    blockTightArea = topRow;

    const int halfW2 = (bottomRow.getWidth() - gap) / 2;
    blockBodyArea      = bottomRow.removeFromLeft (halfW2);
    bottomRow.removeFromLeft (gap);
    blockCharacterArea = bottomRow;

    knobLabels.clear();

    // Coloca N perillas en una fila dentro del bloque (debajo del título)
    auto layoutKnobs = [this] (juce::Rectangle<int> block, std::initializer_list<juce::Slider*> knobs)
    {
        auto row = block.withTrimmedTop (blockTitleHeight + 4).reduced (8, 0);
        const int w = row.getWidth() / (int) knobs.size();
        for (auto* k : knobs)
        {
            auto cell = row.removeFromLeft (w).reduced (4, 0);
            auto labelArea = cell.removeFromTop (captionHeight);
            k->setBounds (cell.reduced (2, 2));
            knobLabels.emplace_back (k, labelArea);
        }
    };

    layoutKnobs (blockToneArea,  { &low, &mid, &high });
    layoutKnobs (blockTightArea, { &tight, &smooth });
    layoutKnobs (blockBodyArea,  { &punch, &threshold, &release, &parallel });

    // Bloque de carácter: tres perillas arriba, selector y clipper abajo
    auto charArea = blockCharacterArea.withTrimmedTop (blockTitleHeight + 4).reduced (8, 0);
    auto charBottom = charArea.removeFromBottom (58);
    layoutKnobs (charArea.withSize (charArea.getWidth(), charArea.getHeight()), { &drive, &satMix, &output });

    satTypeLabelArea = charBottom.removeFromLeft (charBottom.getWidth() / 2 - 8).removeFromTop (captionHeight);
    satType.setBounds (charBottom.removeFromLeft (charBottom.getWidth() / 2 - 8).withTrimmedTop (captionHeight).reduced (0, 4));
    clipper.setBounds (charBottom.reduced (6, 10));

    // Pie: bypass a la izquierda, medidor a la derecha
    bypass.setBounds (footer.removeFromLeft (150).reduced (0, 4));
    meter.setBounds (footer.removeFromRight (360).reduced (0, 6));
}
