#pragma once

#include "PluginProcessor.h"

/**
 * Estilo visual propio: grafito oscuro, acentos ámbar y tipografía sans legible.
 * Todos los colores están aquí para poder cambiarlos sin tocar los componentes.
 */
namespace FartovenStyle
{
    const juce::Colour background { 0xff101214 };
    const juce::Colour panel      { 0xff16181b };
    const juce::Colour panelEdge  { 0xff26292e };
    const juce::Colour accent     { 0xfff59e0b };
    const juce::Colour textMain   { 0xffd8dbe0 };
    const juce::Colour textDim    { 0xff7d838c };
}

/** Perillas con arco de color y aguja. */
class FartovenLookAndFeel : public juce::LookAndFeel_V4
{
public:
    FartovenLookAndFeel();

    void drawRotarySlider (juce::Graphics&, int x, int y, int width, int height,
                           float sliderPos, float startAngle, float endAngle,
                           juce::Slider&) override;

    void drawComboBox (juce::Graphics&, int width, int height, bool isButtonDown,
                       int buttonX, int buttonY, int buttonW, int buttonH, juce::ComboBox&) override;
};

/** Selector de modo con iconos vectoriales: Kick, Snare, Toms, Cymbals, Room. */
class ModeSelector : public juce::Component, private juce::Timer
{
public:
    explicit ModeSelector (juce::RangedAudioParameter& modeParam);

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;

private:
    void timerCallback() override;

    juce::RangedAudioParameter& param;
    int selected = 0;
    juce::Array<juce::Rectangle<float>> cells;

    static juce::Path iconFor (int mode, juce::Rectangle<float> area);
};

/** Medidor de pico de salida en dBFS, con caída suave. */
class PeakMeter : public juce::Component, private juce::Timer
{
public:
    explicit PeakMeter (std::atomic<float>& peakSource);

    void paint (juce::Graphics&) override;

private:
    void timerCallback() override;

    std::atomic<float>& source;
    float displayed = 0.0f;
};

class FartovenEditor : public juce::AudioProcessorEditor
{
public:
    explicit FartovenEditor (FartovenProcessor&);
    ~FartovenEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    FartovenProcessor& processor;
    FartovenLookAndFeel lookAndFeel;

    ModeSelector modeSelector;
    PeakMeter meter;

    // Perillas y selectores: el orden coincide con los bloques visuales
    juce::Slider low, mid, high, tight, smooth, punch, threshold, release, parallel,
                 drive, satMix, output;
    juce::ComboBox satType;
    juce::ToggleButton clipper { "Clipper" }, bypass { "Bypass" };

    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ComboAttachment  = juce::AudioProcessorValueTreeState::ComboBoxAttachment;
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;

    std::vector<std::unique_ptr<SliderAttachment>> sliderAttachments;
    std::unique_ptr<ComboAttachment> satTypeAttachment;
    std::unique_ptr<ButtonAttachment> clipperAttachment, bypassAttachment;

    juce::Rectangle<int> blockToneArea, blockTightArea, blockBodyArea, blockCharacterArea;
    juce::Rectangle<int> satTypeLabelArea;

    /** Cada perilla con la zona donde se dibuja su etiqueta. Se rehace en resized(). */
    std::vector<std::pair<juce::Slider*, juce::Rectangle<int>>> knobLabels;

    void addSlider (juce::Slider& s, const char* paramId, const juce::String& caption);
    void drawBlock (juce::Graphics&, juce::Rectangle<int> area, const juce::String& title);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FartovenEditor)
};
