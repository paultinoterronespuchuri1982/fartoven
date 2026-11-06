#include "DrumChannel.h"

void DrumChannel::prepare (const juce::dsp::ProcessSpec& spec)
{
    sampleRate = spec.sampleRate;
    maxBlock = (int) spec.maximumBlockSize;
    channels = (int) spec.numChannels;

    // Todos los buffers se crean aquí, nunca en processAdd().
    work.setSize (channels, maxBlock, false, false, true);

    lowpass.prepare (spec);
    eqLow.prepare (spec);
    eqMid.prepare (spec);
    eqHigh.prepare (spec);

    constexpr double rampSeconds = 0.02;
    gainRamp.reset (sampleRate, rampSeconds);
    driveRamp.reset (sampleRate, rampSeconds);

    current = Params {};
    gainRamp.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (current.gainDb));
    driveRamp.setCurrentAndTargetValue (1.0f + current.drivePct * 0.09f);

    filterDirty = true;
    setParams (current);
    reset();
}

void DrumChannel::reset()
{
    lowpass.reset();
    eqLow.reset();
    eqMid.reset();
    eqHigh.reset();
    work.clear();
}

void DrumChannel::updateFilters()
{
    using C = juce::dsp::IIR::Coefficients<float>;

    const float cutoff = juce::jlimit (200.0f, 20000.0f, current.filterHz);
    *lowpass.state = *C::makeLowPass (sampleRate, cutoff, 0.707f);

    *eqLow.state  = *C::makeLowShelf (sampleRate, current.lowHz, 0.707f, juce::Decibels::decibelsToGain (current.lowDb));
    *eqMid.state  = *C::makePeakFilter (sampleRate, current.midHz, 1.0f, juce::Decibels::decibelsToGain (current.midDb));
    *eqHigh.state = *C::makeHighShelf (sampleRate, current.highHz, 0.707f, juce::Decibels::decibelsToGain (current.highDb));
}

void DrumChannel::setParams (const Params& p)
{
    // Solo recalcula filtros si algo de ellos cambió
    const bool changed = filterDirty
        || p.filterHz != current.filterHz
        || p.lowDb != current.lowDb || p.lowHz != current.lowHz
        || p.midDb != current.midDb || p.midHz != current.midHz
        || p.highDb != current.highDb || p.highHz != current.highHz;

    current = p;
    if (changed)
    {
        updateFilters();
        filterDirty = false;
    }

    gainRamp.setTargetValue (juce::Decibels::decibelsToGain (p.gainDb));
    // Drive 0..100 % -> pre-ganancia 1x..10x
    driveRamp.setTargetValue (1.0f + p.drivePct * 0.09f);
}

void DrumChannel::processAdd (const juce::AudioBuffer<float>& input, juce::AudioBuffer<float>& output, float weight)
{
    const int n = juce::jmin (input.getNumSamples(), maxBlock);
    const int nc = juce::jmin (juce::jmin (input.getNumChannels(), channels), output.getNumChannels());
    if (n <= 0 || nc <= 0)
        return;

    // 1. Copia de trabajo
    for (int ch = 0; ch < nc; ++ch)
        work.copyFrom (ch, 0, input, ch, 0, n);

    // 2. Filtro paso bajo y EQ de tres bandas de la voz
    juce::dsp::AudioBlock<float> block (work.getArrayOfWritePointers(), (size_t) nc, (size_t) n);
    juce::dsp::ProcessContextReplacing<float> ctx (block);
    lowpass.process (ctx);
    eqLow.process (ctx);
    eqMid.process (ctx);
    eqHigh.process (ctx);

    // 3. Ganancia y drive, con una rampa por muestra compartida por todos los canales
    for (int i = 0; i < n; ++i)
    {
        const float gain = gainRamp.getNextValue();
        const float drive = driveRamp.getNextValue();

        for (int ch = 0; ch < nc; ++ch)
        {
            const float x = work.getSample (ch, i) * gain * drive;
            const float y = shape (x, current.saturation);
            output.addSample (ch, i, y * weight);
        }
    }
}

float DrumChannel::shape (float x, SaturationMode mode) noexcept
{
    float y = x;

    switch (mode)
    {
        case SaturationMode::Cinta:
        {
            // Compresión suave y simétrica. tanh(1.2x) / tanh(1.2) hace que la curva
            // pase por (1, 1): el nivel de referencia no cambia, y el resto se comprime.
            // Efecto esperado: redondea picos y añade calidez sin asperezas.
            constexpr float k = 1.2f;
            static const float norm = std::tanh (k);
            y = std::tanh (k * x) / norm;
            break;
        }

        case SaturationMode::Valvula:
        {
            // Asimétrica: el lado positivo satura hacia 1 y el negativo hacia -0.8.
            // La diferencia entre ambos lados genera armónicos pares (sabor de válvula).
            // No se normaliza por muestra: eso anularía la amplitud de cada golpe.
            if (x >= 0.0f)
                y = 1.0f - std::exp (-1.5f * x);
            else
                y = -0.8f * (1.0f - std::exp (1.2f * x));
            break;
        }

        case SaturationMode::Metal:
        {
            // Curva dura, cercana a un recorte. x / (1 + |x|)^0.75 crece despacio,
            // y el clamp posterior deja la salida en [-1, 1].
            // Efecto esperado: ataque agresivo y armónicos impares.
            y = x / std::pow (1.0f + std::abs (x), 0.75f);
            break;
        }
    }

    return juce::jlimit (-limit, limit, y);
}
