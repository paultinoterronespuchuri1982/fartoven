# FARTOVEN DRUMS

Plugin de audio para procesamiento de baterías, desarrollado con JUCE 8. Incluye un control de ganancia (-12 a +12 dB) y una saturación suave (drive, 0–100) aplicada con `std::tanh`.

Formatos: VST3, AU y Standalone.

## Requisitos

- Windows 10 o superior, o macOS 10.15 o superior
- CMake 3.22 o superior
- Compilador C++17 (Visual Studio 2019/2022 en Windows, Xcode en macOS)
- Git (CMake descarga JUCE 8.0.4 automáticamente con FetchContent)

## Compilar

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

Los artefactos se generan en `build/FartovenDrums_artefacts/Release/`.

## Licencia

Pendiente de definir.
