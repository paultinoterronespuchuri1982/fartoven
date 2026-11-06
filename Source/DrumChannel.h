#pragma once

#include <juce_dsp/juce_dsp.h>

/**
 * Voz de batería: ganancia, filtro de paso bajo, pre-ganancia (drive) y saturación.
 *
 * Cada instancia tiene su propio estado y sus propios buffers, así que las voces
 * no comparten nada entre sí. Todos los buffers se reservan en prepare().
 * Las llamadas a setParams() recalculan coeficientes de filtro cuando cambian los
 * valores; eso puede reservar memoria, por lo que se llama solo si hubo cambio.
 */
class DrumChannel
{
public:
    /** Curvas de saturación. Cada una tiene un carácter distinto; ver DrumChannel.cpp. */
    enum class SaturationMode
    {
        Cinta = 0,   // curva suave y simétrica
        Valvula,     // asimétrica, aporta armónicos pares
        Metal        // curva dura, cercana a un recorte
    };

    struct Params
    {
        float gainDb = 0.0f;                          // -24 .. +12 dB
        float drivePct = 0.0f;                        // 0 .. 100 %, pre-ganancia de 1x a 10x
        SaturationMode saturation = SaturationMode::Cinta;
        float filterHz = 20000.0f;                    // 200 .. 20000 Hz, paso bajo

        // Ecualización propia de la voz: tres bandas con ganancia y frecuencia de referencia
        float lowDb = 0.0f, midDb = 0.0f, highDb = 0.0f;   // -12 .. +12 dB
        float lowHz = 150.0f, midHz = 1000.0f, highHz = 5000.0f;
    };

    void prepare (const juce::dsp::ProcessSpec& spec);
    void reset();

    /** Aplica nuevos parámetros. Solo recalcula el filtro si cambió la frecuencia. */
    void setParams (const Params& p);

    /**
     * Procesa `input` y suma el resultado, multiplicado por `weight`, a `output`.
     * `input` y `output` deben tener el mismo número de canales y muestras
     * que el máximo indicado en prepare().
     */
    void processAdd (const juce::AudioBuffer<float>& input, juce::AudioBuffer<float>& output, float weight);

    /** Aplica la curva de saturación a una muestra. Visible para pruebas. */
    static float shape (float x, SaturationMode mode) noexcept;

private:
    static constexpr float limit = 1.0f;

    double sampleRate = 44100.0;
    int maxBlock = 512;
    int channels = 2;
    Params current;
    bool filterDirty = true;

    juce::AudioBuffer<float> work;     // copia de trabajo de la entrada
    using Biquad = juce::dsp::ProcessorDuplicator<juce::dsp::IIR::Filter<float>, juce::dsp::IIR::Coefficients<float>>;
    Biquad lowpass;
    Biquad eqLow, eqMid, eqHigh;       // EQ de tres bandas propia de la voz

    void updateFilters();

    juce::SmoothedValue<float> gainRamp;
    juce::SmoothedValue<float> driveRamp;
};
