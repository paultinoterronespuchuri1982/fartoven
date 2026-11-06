// Pruebas del motor DrumChannelDSP. Se ejecutan con el target FartovenDrumsTests.
// No comprueban el sonido por oído: verifican propiedades que deben cumplirse siempre
// (sin NaN, sin valores fuera de rango, cada modo responde, latencia coherente).

#include <juce_core/juce_core.h>
#include <juce_dsp/juce_dsp.h>

#include "../Source/DrumChannelDSP.h"

namespace
{
    constexpr int blockSize = 512;

    juce::dsp::ProcessSpec makeSpec (double sampleRate, int channels)
    {
        juce::dsp::ProcessSpec spec;
        spec.sampleRate = sampleRate;
        spec.maximumBlockSize = (juce::uint32) blockSize;
        spec.numChannels = (juce::uint32) channels;
        return spec;
    }

    bool allFinite (const juce::AudioBuffer<float>& buffer)
    {
        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
            for (int i = 0; i < buffer.getNumSamples(); ++i)
                if (! std::isfinite (buffer.getSample (ch, i)))
                    return false;
        return true;
    }

    float maxAbs (const juce::AudioBuffer<float>& buffer)
    {
        float m = 0.0f;
        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
            m = juce::jmax (m, buffer.getMagnitude (ch, 0, buffer.getNumSamples()));
        return m;
    }
}

class DrumChannelDSPTests : public juce::UnitTest
{
public:
    DrumChannelDSPTests() : juce::UnitTest ("DrumChannelDSP", "DSP") {}

    void runTest() override
    {
        beginTest ("El silencio produce salida finita y silenciosa");
        {
            DrumChannelDSP dsp;
            dsp.prepare (makeSpec (48000.0, 2));

            juce::AudioBuffer<float> buffer (2, blockSize);
            buffer.clear();
            dsp.process (buffer);

            expect (allFinite (buffer), "El silencio produjo NaN o Inf");
            expect (maxAbs (buffer) < 1.0e-6f, "El silencio no debe producir señal");
        }

        beginTest ("El clipper mantiene la salida en rango ante un impulso fuerte");
        {
            DrumChannelDSP dsp;
            dsp.prepare (makeSpec (48000.0, 2));

            DrumChannelDSP::Settings s;
            s.clipperOn = true;
            s.outputDb = 12.0f;   // ganancia de salida máxima
            s.drivePct = 100.0f;  // empuje máximo
            dsp.setSettings (s);

            juce::AudioBuffer<float> buffer (2, blockSize);
            buffer.clear();
            for (int ch = 0; ch < 2; ++ch)
                buffer.setSample (ch, 0, 16.0f);

            // Varios bloques para que las rampas y el sobremuestreo se estabilicen
            for (int b = 0; b < 4; ++b)
                dsp.process (buffer);

            expect (allFinite (buffer), "La saturación fuerte produjo NaN o Inf");
            expect (maxAbs (buffer) <= 1.0001f, "El clipper dejó pasar picos por encima de 1.0");
        }

        beginTest ("Cada modo responde a un impulso");
        {
            for (int m = 0; m < 5; ++m)
            {
                DrumChannelDSP dsp;
                dsp.prepare (makeSpec (48000.0, 2));

                DrumChannelDSP::Settings s;
                s.mode = (DrumChannelDSP::Mode) m;
                dsp.setSettings (s);

                juce::AudioBuffer<float> buffer (2, blockSize);
                buffer.clear();
                for (int ch = 0; ch < 2; ++ch)
                    buffer.setSample (ch, 0, 0.5f);

                dsp.process (buffer);

                expect (allFinite (buffer), "Modo " + juce::String (m) + " produjo NaN o Inf");
                expect (maxAbs (buffer) > 1.0e-4f, "Modo " + juce::String (m) + " no produjo salida");
            }
        }

        beginTest ("Ajustes aleatorios y ruido no producen NaN ni Inf");
        {
            juce::Random rng (1234);

            for (double sr : { 44100.0, 48000.0, 96000.0 })
            {
                DrumChannelDSP dsp;
                dsp.prepare (makeSpec (sr, 2));

                for (int round = 0; round < 20; ++round)
                {
                    DrumChannelDSP::Settings s;
                    s.mode = (DrumChannelDSP::Mode) rng.nextInt (5);
                    s.lowPct = rng.nextFloat() * 200.0f - 100.0f;
                    s.midPct = rng.nextFloat() * 200.0f - 100.0f;
                    s.highPct = rng.nextFloat() * 200.0f - 100.0f;
                    s.tightPct = rng.nextFloat() * 100.0f;
                    s.smoothPct = rng.nextFloat() * 100.0f;
                    s.punchPct = rng.nextFloat() * 100.0f;
                    s.thresholdDb = -36.0f + rng.nextFloat() * 36.0f;
                    s.releaseMs = 20.0f + rng.nextFloat() * 480.0f;
                    s.parallelPct = rng.nextFloat() * 100.0f;
                    s.drivePct = rng.nextFloat() * 100.0f;
                    s.satMixPct = rng.nextFloat() * 100.0f;
                    s.satType = (DrumChannelDSP::SaturationType) rng.nextInt (3);
                    s.clipperOn = rng.nextBool();
                    s.outputDb = rng.nextFloat() * 24.0f - 12.0f;
                    dsp.setSettings (s);

                    juce::AudioBuffer<float> buffer (2, blockSize);
                    for (int ch = 0; ch < 2; ++ch)
                        for (int i = 0; i < blockSize; ++i)
                            buffer.setSample (ch, i, rng.nextFloat() * 2.0f - 1.0f);

                    dsp.process (buffer);

                    expect (allFinite (buffer), "NaN o Inf con ajustes aleatorios a " + juce::String (sr) + " Hz");
                }
            }
        }

        beginTest ("Mono y estéreo funcionan");
        {
            DrumChannelDSP dsp;
            dsp.prepare (makeSpec (48000.0, 1));

            juce::AudioBuffer<float> buffer (1, blockSize);
            buffer.clear();
            buffer.setSample (0, 0, 0.5f);
            dsp.process (buffer);

            expect (allFinite (buffer), "Mono produjo NaN o Inf");
        }

        beginTest ("La latencia reportada es positiva por el sobremuestreo");
        {
            DrumChannelDSP dsp;
            dsp.prepare (makeSpec (48000.0, 2));
            expect (dsp.getLatencySamples() > 0, "El sobremuestreo debe introducir latencia");
        }
    }
};

static DrumChannelDSPTests drumChannelDSPTests;
