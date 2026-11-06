#pragma once

#include <juce_dsp/juce_dsp.h>
#include <tuple>
#include <vector>

/**
 * Motor de procesado de FARTOVEN DRUMS, aplicado sobre el audio de entrada.
 *
 * Cadena: EQ por pieza -> limpieza (Tight/Smooth) -> compresión paralela (Punch)
 *         -> saturación con sobremuestreo 2x -> trim de salida -> clipper.
 *
 * Reglas de tiempo real:
 *  - process() no reserva memoria: todos los buffers se crean en prepare().
 *  - Las rampas de ganancia se calculan una vez por muestra y se reutilizan
 *    en todos los canales, para que L y R no se desfasen durante una rampa.
 *  - Los coeficientes de los filtros solo se recalculan si cambian los ajustes.
 *    Ese recálculo sí puede reservar memoria, así que conviene que el host no
 *    mueva parámetros de forma continua durante el procesado de bloques pequeños.
 */
class DrumChannelDSP
{
public:
    enum class Mode { Kick = 0, Snare, Toms, Cymbals, Room };
    enum class SaturationType { Tape = 0, Tube, Iron };

    /** Ajustes en unidades de usuario (porcentajes, dB, milisegundos). */
    struct Settings
    {
        Mode mode = Mode::Kick;
        float lowPct = 0.0f;          // -100..100
        float midPct = 0.0f;          // -100..100
        float highPct = 0.0f;         // -100..100
        float tightPct = 40.0f;       // 0..100
        float smoothPct = 35.0f;      // 0..100
        float punchPct = 50.0f;       // 0..100
        float thresholdDb = -18.0f;   // -36..0
        float releaseMs = 120.0f;     // 20..500
        float parallelPct = 70.0f;    // 0..100
        float drivePct = 25.0f;       // 0..100
        float satMixPct = 80.0f;      // 0..100
        SaturationType satType = SaturationType::Tape;
        bool clipperOn = true;
        float outputDb = 0.0f;        // -12..12
    };

    /** Latencia que introduce el sobremuestreo, en muestras. Se reporta al host. */
    int getLatencySamples() const noexcept { return oversampling != nullptr ? (int) oversampling->getLatencyInSamples() : 0; }

    void prepare (const juce::dsp::ProcessSpec& spec)
    {
        sampleRate = spec.sampleRate;
        numChannels = (int) spec.numChannels;
        maxBlockSize = (int) spec.maximumBlockSize;

        scratch.setSize (numChannels, maxBlockSize, false, false, true);

        for (auto* ramp : { &dryRamp, &wetRamp, &satRamp, &trimRamp })
            ramp->resize ((size_t) maxBlockSize, 0.0f);

        lowShelf.prepare (spec);
        midPeak.prepare (spec);
        highShelf.prepare (spec);
        tightFilter.prepare (spec);
        smoothFilter.prepare (spec);
        compressor.prepare (spec);

        // El parámetro es un exponente: 1 significa 2^1 = 2x.
        oversampling = std::make_unique<juce::dsp::Oversampling<float>> (
            spec.numChannels, 1, juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true);
        oversampling->initProcessing ((size_t) maxBlockSize);

        constexpr double rampSeconds = 0.025;
        dryGain.reset (sampleRate, rampSeconds);
        wetGain.reset (sampleRate, rampSeconds);
        satGain.reset (sampleRate, rampSeconds);
        trimGain.reset (sampleRate, rampSeconds);

        // Valores iniciales sin rampa: el primer bloque ya usa coeficientes válidos.
        dryGain.setCurrentAndTargetValue (1.0f - current.parallelPct * 0.01f);
        wetGain.setCurrentAndTargetValue (current.parallelPct * 0.01f);
        satGain.setCurrentAndTargetValue (current.satMixPct * 0.01f);
        trimGain.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (current.outputDb));

        reset();
        applyAll();
    }

    void reset() noexcept
    {
        lowShelf.reset();
        midPeak.reset();
        highShelf.reset();
        tightFilter.reset();
        smoothFilter.reset();
        compressor.reset();

        if (oversampling != nullptr)
            oversampling->reset();

        scratch.clear();
    }

    /** Aplica nuevos ajustes. Solo recalcula filtros si algo ha cambiado. */
    void setSettings (const Settings& newSettings)
    {
        if (sameSettings (newSettings, current))
            return;

        current = newSettings;
        applyAll();
    }

    /** Procesa el bloque in-place. */
    void process (juce::AudioBuffer<float>& buffer)
    {
        const int n = buffer.getNumSamples();
        const int nc = juce::jmin (buffer.getNumChannels(), numChannels);

        if (nc == 0 || n == 0 || n > maxBlockSize)
            return;

        juce::dsp::AudioBlock<float> block (buffer);

        // 1. EQ y limpieza
        juce::dsp::ProcessContextReplacing<float> mainCtx (block);
        lowShelf.process (mainCtx);
        midPeak.process (mainCtx);
        highShelf.process (mainCtx);
        tightFilter.process (mainCtx);
        smoothFilter.process (mainCtx);

        // 2. Compresión paralela sobre una copia preasignada
        for (int ch = 0; ch < nc; ++ch)
            scratch.copyFrom (ch, 0, buffer, ch, 0, n);

        juce::dsp::AudioBlock<float> wetBlock (scratch.getArrayOfWritePointers(), (size_t) nc, (size_t) n);
        juce::dsp::ProcessContextReplacing<float> wetCtx (wetBlock);
        compressor.process (wetCtx);

        // Una rampa por muestra, compartida por todos los canales
        fillRamp (dryGain, dryRamp, n);
        fillRamp (wetGain, wetRamp, n);

        for (int ch = 0; ch < nc; ++ch)
        {
            float* d = block.getChannelPointer ((size_t) ch);
            const float* w = scratch.getReadPointer (ch);

            for (int i = 0; i < n; ++i)
                d[i] = d[i] * dryRamp[(size_t) i] + w[i] * wetRamp[(size_t) i];
        }

        // 3. Saturación con sobremuestreo. La mezcla se hace dentro del dominio
        //    sobremuestreado, así la señal seca y la saturada tienen la misma latencia.
        const float drive = 1.0f + (current.drivePct * 0.01f) * driveMaxGain;
        fillRamp (satGain, satRamp, n);

        auto up = oversampling->processSamplesUp (block);
        const size_t upSamples = up.getNumSamples();

        for (size_t ch = 0; ch < up.getNumChannels(); ++ch)
        {
            float* s = up.getChannelPointer (ch);

            for (size_t i = 0; i < upSamples; ++i)
            {
                const float x = s[i];
                const float shaped = shape (x * drive);
                const float mix = satRamp[i / kOversampleRatio];
                s[i] = x * (1.0f - mix) + shaped * mix;
            }
        }

        oversampling->processSamplesDown (block);

        // 4. Trim de salida y clipper
        fillRamp (trimGain, trimRamp, n);

        for (int ch = 0; ch < nc; ++ch)
        {
            float* d = block.getChannelPointer ((size_t) ch);

            for (int i = 0; i < n; ++i)
            {
                float x = d[i] * trimRamp[(size_t) i];
                if (current.clipperOn)
                    x = softClip (x);
                d[i] = x;
            }
        }
    }

private:
    // --- Constantes de diseño, con su unidad ---
    static constexpr float eqRangeDb = 12.0f;        // EQ a +-12 dB con el máximo
    static constexpr float tightMaxCutDb = 15.0f;    // Tight corta hasta 15 dB
    static constexpr float smoothMaxCutDb = 12.0f;   // Smooth corta hasta 12 dB
    static constexpr float compAttackMs = 14.0f;     // Deja pasar el transitorio inicial
    static constexpr float compRatioMin = 1.5f;
    static constexpr float compRatioSpan = 6.5f;     // Ratio final entre 1.5:1 y 8:1
    static constexpr float driveMaxGain = 4.5f;      // Drive 100% = pre-ganancia 5.5x
    static constexpr float clipThreshold = 0.95f;    // Por debajo, el clipper no toca la señal
    static constexpr float defaultParallel = 0.7f;
    static constexpr size_t kOversampleRatio = 2;

    /** Índice de filtro por pieza: frecuencias en Hz. */
    struct ModeFrequencies { float low, mid, high, tight, smooth; };

    static ModeFrequencies frequenciesFor (Mode m) noexcept
    {
        switch (m)
        {
            case Mode::Kick:    return { 75.0f, 380.0f, 5500.0f, 210.0f, 420.0f };
            case Mode::Snare:   return { 140.0f, 1100.0f, 5500.0f, 260.0f, 780.0f };
            case Mode::Toms:    return { 100.0f, 1100.0f, 5500.0f, 340.0f, 780.0f };
            case Mode::Cymbals: return { 100.0f, 3500.0f, 9500.0f, 260.0f, 780.0f };
            case Mode::Room:    return { 100.0f, 1100.0f, 5500.0f, 260.0f, 780.0f };
        }
        return { 100.0f, 1100.0f, 5500.0f, 260.0f, 780.0f };
    }

    /** Traslada `current` a filtros, compresor y rampas. Puede reservar memoria (coeficientes). */
    void applyAll()
    {
        updateFilters();

        compressor.setThreshold (current.thresholdDb);
        compressor.setRatio (compRatioMin + (current.punchPct * 0.01f) * compRatioSpan);
        compressor.setAttack (compAttackMs);
        compressor.setRelease (juce::jlimit (20.0f, 500.0f, current.releaseMs));

        const float wet = current.parallelPct * 0.01f;
        dryGain.setTargetValue (1.0f - wet);
        wetGain.setTargetValue (wet);

        satGain.setTargetValue (current.satMixPct * 0.01f);
        trimGain.setTargetValue (juce::Decibels::decibelsToGain (current.outputDb));
    }

    void updateFilters()
    {
        const auto f = frequenciesFor (current.mode);
        using C = juce::dsp::IIR::Coefficients<float>;

        const float lowDb  = current.lowPct  * eqRangeDb * 0.01f;
        const float midDb  = current.midPct  * eqRangeDb * 0.01f;
        const float highDb = current.highPct * eqRangeDb * 0.01f;

        *lowShelf.state  = *C::makeLowShelf   (sampleRate, f.low,   0.707f, juce::Decibels::decibelsToGain (lowDb));
        *midPeak.state   = *C::makePeakFilter (sampleRate, f.mid,   1.1f,   juce::Decibels::decibelsToGain (midDb));
        *highShelf.state = *C::makeHighShelf  (sampleRate, f.high,  0.707f, juce::Decibels::decibelsToGain (highDb));

        *tightFilter.state  = *C::makePeakFilter (sampleRate, f.tight,  2.2f, juce::Decibels::decibelsToGain (-current.tightPct * tightMaxCutDb * 0.01f));
        *smoothFilter.state = *C::makePeakFilter (sampleRate, f.smooth, 1.8f, juce::Decibels::decibelsToGain (-current.smoothPct * smoothMaxCutDb * 0.01f));
    }

    /** Saturación según el carácter elegido. Entrada y salida en rango aproximado [-1, 1]. */
    float shape (float x) const noexcept
    {
        switch (current.satType)
        {
            case SaturationType::Tape:
                return std::tanh (x);
            case SaturationType::Tube:
                // Asimetría: lado positivo y negativo con curvas distintas genera armónicos pares
                return x >= 0.0f ? std::tanh (x) : 1.25f * std::tanh (x * 0.75f);
            case SaturationType::Iron:
                return (2.0f / juce::MathConstants<float>::pi) * std::atan (x * 1.4f);
        }
        return x;
    }

    /** Soft-clip: lineal hasta clipThreshold y asintótico hasta 1.0. */
    static float softClip (float x) noexcept
    {
        constexpr float headroom = 1.0f - clipThreshold;
        if (x > clipThreshold)
            return clipThreshold + headroom * std::tanh ((x - clipThreshold) / headroom);
        if (x < -clipThreshold)
            return -clipThreshold - headroom * std::tanh ((-x - clipThreshold) / headroom);
        return x;
    }

    static void fillRamp (juce::SmoothedValue<float>& source, std::vector<float>& dst, int n) noexcept
    {
        for (int i = 0; i < n; ++i)
            dst[(size_t) i] = source.getNextValue();
    }

    static bool sameSettings (const Settings& a, const Settings& b) noexcept
    {
        return std::tie (a.mode, a.lowPct, a.midPct, a.highPct, a.tightPct, a.smoothPct, a.punchPct,
                         a.thresholdDb, a.releaseMs, a.parallelPct, a.drivePct, a.satMixPct,
                         a.satType, a.clipperOn, a.outputDb)
            == std::tie (b.mode, b.lowPct, b.midPct, b.highPct, b.tightPct, b.smoothPct, b.punchPct,
                         b.thresholdDb, b.releaseMs, b.parallelPct, b.drivePct, b.satMixPct,
                         b.satType, b.clipperOn, b.outputDb);
    }

    double sampleRate = 44100.0;
    int numChannels = 2;
    int maxBlockSize = 512;
    Settings current;

    juce::AudioBuffer<float> scratch;
    std::vector<float> dryRamp, wetRamp, satRamp, trimRamp;

    using Biquad = juce::dsp::ProcessorDuplicator<juce::dsp::IIR::Filter<float>, juce::dsp::IIR::Coefficients<float>>;
    Biquad lowShelf, midPeak, highShelf, tightFilter, smoothFilter;
    juce::dsp::Compressor<float> compressor;
    std::unique_ptr<juce::dsp::Oversampling<float>> oversampling;

    juce::SmoothedValue<float> dryGain, wetGain, satGain, trimGain;
};
