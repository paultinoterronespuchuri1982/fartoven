#include <juce_core/juce_core.h>
#include <iostream>

namespace
{
    /** JUCE no imprime nada en consola por defecto: este logger envía los mensajes a stdout. */
    class StdoutLogger : public juce::Logger
    {
    public:
        void logMessage (const juce::String& message) override
        {
            std::cout << message << std::endl;
        }
    };
}

// Ejecuta todas las UnitTest registradas. Devuelve 0 si todo pasa, 1 si algo falla.
int main()
{
    StdoutLogger logger;
    juce::Logger::setCurrentLogger (&logger);

    juce::UnitTestRunner runner;
    runner.runAllTests();

    int failures = 0;
    int passes = 0;
    for (int i = 0; i < runner.getNumResults(); ++i)
    {
        if (auto* result = runner.getResult (i))
        {
            failures += result->failures;
            passes += result->passes;
        }
    }

    std::cout << "Resumen: " << passes << " comprobaciones correctas, " << failures << " fallidas." << std::endl;
    juce::Logger::setCurrentLogger (nullptr);
    return failures == 0 ? 0 : 1;
}
