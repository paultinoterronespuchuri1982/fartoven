#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
    // Identificadores de parámetro: no cambiar, los hosts los guardan en los proyectos.
    namespace ids
    {
        const char* const mode      = "mode";
        const char* const low       = "low";
        const char* const mid       = "mid";
        const char* const high      = "high";
        const char* const tight     = "tight";
        const char* const smooth    = "smooth";
        const char* const punch     = "punch";
        const char* const threshold = "threshold";
        const char* const release   = "release";
        const char* const parallel  = "parallel";
        const char* const drive     = "drive";
        const char* const satMix    = "satMix";
        const char* const satMode   = "satMode";
        const char* const clipper   = "clipper";
        const char* const output    = "output";
        const char* const bypass    = "bypass";
    }

    std::unique_ptr<juce::RangedAudioParameter> makeFloat (const char* id, const char* name,
                                                           float min, float max, float def)
    {
        return std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { id, 1 }, name,
                                                            juce::NormalisableRange<float> (min, max, 0.01f), def);
    }
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

    params.push_back (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { ids::mode, 1 }, "Mode",
        juce::StringArray { "Kick", "Snare", "Toms", "Cymbals", "Room" }, 0));

    params.push_back (makeFloat (ids::low,  "Low",  -100.0f, 100.0f, 0.0f));
    params.push_back (makeFloat (ids::mid,  "Mid",  -100.0f, 100.0f, 0.0f));
    params.push_back (makeFloat (ids::high, "High", -100.0f, 100.0f, 0.0f));

    params.push_back (makeFloat (ids::tight,  "Tight",  0.0f, 100.0f, 40.0f));
    params.push_back (makeFloat (ids::smooth, "Smooth", 0.0f, 100.0f, 35.0f));

    params.push_back (makeFloat (ids::punch,     "Punch",     0.0f, 100.0f, 50.0f));
    params.push_back (makeFloat (ids::threshold, "Threshold", -36.0f, 0.0f, -18.0f));
    params.push_back (makeFloat (ids::release,   "Release",   20.0f, 500.0f, 120.0f));
    params.push_back (makeFloat (ids::parallel,  "Parallel",   0.0f, 100.0f, 70.0f));

    params.push_back (makeFloat (ids::drive,  "Drive",   0.0f, 100.0f, 25.0f));
    params.push_back (makeFloat (ids::satMix, "Sat Mix", 0.0f, 100.0f, 80.0f));
    params.push_back (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { ids::satMode, 1 }, "Sat Type",
        juce::StringArray { "Tape", "Tube", "Iron" }, 0));
    params.push_back (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { ids::clipper, 1 }, "Clipper", true));
    params.push_back (makeFloat (ids::output, "Output", -12.0f, 12.0f, 0.0f));

    params.push_back (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { ids::bypass, 1 }, "Bypass", false));

    return { params.begin(), params.end() };
}

DrumChannelDSP::Settings FartovenProcessor::readSettings() const
{
    auto get = [this] (const char* id) { return apvts.getRawParameterValue (id)->load(); };

    DrumChannelDSP::Settings s;
    s.mode        = static_cast<DrumChannelDSP::Mode> (static_cast<int> (get (ids::mode)));
    s.lowPct      = get (ids::low);
    s.midPct      = get (ids::mid);
    s.highPct     = get (ids::high);
    s.tightPct    = get (ids::tight);
    s.smoothPct   = get (ids::smooth);
    s.punchPct    = get (ids::punch);
    s.thresholdDb = get (ids::threshold);
    s.releaseMs   = get (ids::release);
    s.parallelPct = get (ids::parallel);
    s.drivePct    = get (ids::drive);
    s.satMixPct   = get (ids::satMix);
    s.satType     = static_cast<DrumChannelDSP::SaturationType> (static_cast<int> (get (ids::satMode)));
    s.clipperOn   = get (ids::clipper) > 0.5f;
    s.outputDb    = get (ids::output);
    return s;
}

void FartovenProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    juce::dsp::ProcessSpec spec;
    spec.sampleRate       = sampleRate;
    spec.maximumBlockSize = static_cast<juce::uint32> (samplesPerBlock);
    spec.numChannels      = static_cast<juce::uint32> (getTotalNumOutputChannels());

    channelDSP.prepare (spec);
    setLatencySamples (channelDSP.getLatencySamples());
}

void FartovenProcessor::releaseResources()
{
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

    // Limpia canales de salida sin entrada (p. ej. mono a estéreo).
    for (auto ch = getTotalNumInputChannels(); ch < getTotalNumOutputChannels(); ++ch)
        buffer.clear (ch, 0, buffer.getNumSamples());

    if (apvts.getRawParameterValue (ids::bypass)->load() > 0.5f)
        return;

    channelDSP.setSettings (readSettings());
    channelDSP.process (buffer);

    float peak = 0.0f;
    for (auto ch = 0; ch < buffer.getNumChannels(); ++ch)
        peak = juce::jmax (peak, buffer.getMagnitude (ch, 0, buffer.getNumSamples()));
    outputPeak.store (peak);
}

juce::AudioProcessorEditor* FartovenProcessor::createEditor()
{
    return new FartovenEditor (*this);
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
