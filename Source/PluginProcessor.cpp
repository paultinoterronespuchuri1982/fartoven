#include "PluginProcessor.h"

namespace
{
    const juce::String gainParamID  = "gain";
    const juce::String driveParamID = "drive";
}

FartovenProcessor::FartovenProcessor()
    : AudioProcessor (BusesProperties()
                        .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMETERS", createParameterLayout())
{
}

juce::AudioProcessorValueTreeState::ParameterLayout FartovenProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { gainParamID, 1 }, "Gain",
        juce::NormalisableRange<float> (-12.0f, 12.0f, 0.01f), 0.0f, "dB"));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { driveParamID, 1 }, "Drive",
        juce::NormalisableRange<float> (0.0f, 100.0f, 0.01f), 25.0f, "%"));

    return { params.begin(), params.end() };
}

void FartovenProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    juce::dsp::ProcessSpec spec;
    spec.sampleRate       = sampleRate;
    spec.maximumBlockSize = static_cast<juce::uint32> (samplesPerBlock);
    spec.numChannels      = static_cast<juce::uint32> (getTotalNumOutputChannels());

    gain.prepare (spec);
    gain.setRampDurationSeconds (0.02);
}

void FartovenProcessor::releaseResources()
{
    gain.reset();
}

bool FartovenProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& mainOut = layouts.getMainOutputChannelSet();
    if (mainOut != juce::AudioChannelSet::mono() && mainOut != juce::AudioChannelSet::stereo())
        return false;

    return mainOut == layouts.getMainInputChannelSet();
}

void FartovenProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const auto totalNumInputChannels  = getTotalNumInputChannels();
    const auto totalNumOutputChannels = getTotalNumOutputChannels();

    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear (i, 0, buffer.getNumSamples());

    const auto gainDb = apvts.getRawParameterValue (gainParamID)->load();
    const auto drive  = apvts.getRawParameterValue (driveParamID)->load() / 100.0f; // 0..1

    gain.setGainDecibels (gainDb);

    juce::dsp::AudioBlock<float> block (buffer);
    juce::dsp::ProcessContextReplacing<float> context (block);
    gain.process (context);

    // Saturación suave: el drive escala la entrada a la tanh; se compensa con
    // una ganancia de salida para que el nivel no crezca de forma desmedida.
    const auto preGain  = 1.0f + drive * 9.0f;           // 1x .. 10x
    const auto postGain = 1.0f / std::tanh (preGain);    // normaliza la salida

    for (auto channel = 0; channel < totalNumOutputChannels; ++channel)
    {
        auto* data = buffer.getWritePointer (channel);

        for (auto sample = 0; sample < buffer.getNumSamples(); ++sample)
            data[sample] = std::tanh (data[sample] * preGain) * postGain;
    }
}

juce::AudioProcessorEditor* FartovenProcessor::createEditor()
{
    return new juce::GenericAudioProcessorEditor (*this);
}

void FartovenProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    copyXmlToBinary (*xml, destData);
}

void FartovenProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState (getXmlFromBinary (data, sizeInBytes));

    if (xmlState != nullptr && xmlState->hasTagName (apvts.state.getType()))
        apvts.replaceState (juce::ValueTree::fromXml (*xmlState));
}

// Punto de entrada que JUCE usa para crear la instancia del plugin.
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new FartovenProcessor();
}
