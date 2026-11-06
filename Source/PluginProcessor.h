#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>

#include <array>

#include "DrumChannelDSP.h"
#include "DrumChannel.h"

/**
 * Procesador de FARTOVEN DRUMS. Conecta el árbol de parámetros (APVTS) con el motor
 * DrumChannelDSP. Los parámetros están en un solo lugar y se leen una vez por bloque.
 */
class FartovenProcessor : public juce::AudioProcessor
{
public:
    FartovenProcessor();
    ~FartovenProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;

    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using juce::AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }

    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts;

    /** Pico de salida del último bloque, en amplitud lineal. Lo lee el medidor de la interfaz. */
    std::atomic<float> outputPeak { 0.0f };

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    DrumChannelDSP::Settings readSettings() const;
    DrumChannel::Params readVoiceParams (int index) const;

    /** Identificadores de parámetros de cada voz, construidos una vez para no reservar memoria en processBlock. */
    struct VoiceIds { juce::String gain, drive, sat, filter, low, mid, high; };
    static constexpr int numVoices = 5;
    std::array<VoiceIds, numVoices> voiceIds;

    DrumChannelDSP channelDSP;

    // Voces: cada una procesa la entrada y suma su resultado en voiceSum
    std::array<DrumChannel, numVoices> voices;
    juce::AudioBuffer<float> voiceSum;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FartovenProcessor)
};
