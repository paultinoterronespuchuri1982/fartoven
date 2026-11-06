# FARTOVEN DRUMS

Plugin de procesado para mezcla de baterías, desarrollado con JUCE 8.

Formatos: VST3 y Standalone en Windows. AU en macOS. AAX no está incluido: requiere el SDK de Avid.

## Descripción

FARTOVEN DRUMS procesa el audio de una batería con una cadena pensada para esculpir cada pieza sin menús anidados:

1. Ecualización de tres bandas (Low, Mid, High) con frecuencias ajustadas al modo elegido.
2. Limpieza con dos filtros de corte: Tight (barro en medios-bajos) y Smooth (asperezas en medios).
3. Compresión paralela (Punch) con umbral, release y mezcla seco/húmedo.
4. Saturación con sobremuestreo 2x y tres caracteres (Tape, Tube, Iron).
5. Trim de salida y clipper opcional.

## Características

- Cinco modos: Kick, Snare, Toms, Cymbals y Room. Cada uno cambia las frecuencias de la EQ y de la limpieza.
- Medidor de pico de salida en dBFS.
- Bypass para comparar con la señal original.
- Estado guardado en el proyecto del DAW (APVTS).

## Requisitos

- Windows 10 o superior (64 bits), o macOS 10.15 o superior.
- CMake 3.22 o superior.
- Compilador C++17: Visual Studio 2022 con la carga de trabajo «Desarrollo para el escritorio con C++» en Windows, o Xcode en macOS.
- Git, para que CMake descargue JUCE 8.0.4 automáticamente.

En Windows, si la ruta del proyecto es muy larga, MSVC puede fallar con errores de longitud de ruta. En ese caso compila desde una carpeta con ruta corta, por ejemplo con `subst`, o mueve el proyecto a una carpeta como `C:\dev\fartoven-drums`.

## Compilar

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

Los artefactos quedan en `build/FartovenDrums_artefacts/Release/`:

- `VST3/FARTOVEN DRUMS.vst3`
- `Standalone/FARTOVEN DRUMS.exe`

## Ejecutar las pruebas

```bash
cmake --build build --config Release --target FartovenDrumsTests
build/FartovenDrumsTests_artefacts/Release/FartovenDrumsTests.exe
```

Las pruebas comprueban que el motor no produce NaN ni Inf, que el clipper mantiene la salida dentro de ±1, que cada modo responde a un impulso, que los ajustes aleatorios no rompen el procesado y que la latencia reportada es coherente. No comprueban el sonido por oído.

## Instalación

- **Windows, VST3:** copia `FARTOVEN DRUMS.vst3` a `C:\Program Files\Common Files\VST3\`. Requiere permisos de administrador.
- **macOS, AU:** copia `FARTOVEN DRUMS.component` a `~/Library/Audio/Plug-Ins/Components/`.
- **Standalone:** ejecuta el `.exe` directamente.

## Uso de los controles

| Bloque | Control | Qué hace |
|---|---|---|
| Modo | KICK, SNARE, TOMS, CYMBALS, ROOM | Cambia las frecuencias de referencia de la EQ y la limpieza. |
| Tono | Low, Mid, High | Ganancia de cada banda, de -100 a +100 (hasta ±12 dB). |
| Limpieza | Tight | Corta el barro en medios-bajos (hasta -15 dB). |
| Limpieza | Smooth | Corta asperezas en medios (hasta -12 dB). |
| Cuerpo | Punch | Cantidad de compresión: de ratio 1,5:1 a 8:1. |
| Cuerpo | Thresh | Umbral del compresor, de -36 a 0 dB. |
| Cuerpo | Release | Tiempo de recuperación del compresor, de 20 a 500 ms. |
| Cuerpo | Parallel | Mezcla de la señal comprimida frente a la original (0 % = solo seca). |
| Carácter | Drive | Ganancia antes de la saturación. |
| Carácter | Sat Type | Tape, Tube o Iron. |
| Carácter | Sat Mix | Cantidad de saturación mezclada con la señal limpia. |
| Carácter | Clipper | Activa el limitador suave de salida. |
| Salida | Output | Trim de salida, de -12 a +12 dB. |
| Pie | Bypass | Pasa el audio sin procesar. |

Los valores por defecto son un punto de partida. Ajusta por oído según la grabación.

## Voces por instrumento

Además del bus, FARTOVEN DRUMS tiene cinco voces: **Kick, Snare, Toms, Cymbals y Room**. Cada una tiene sus propios parámetros:

| Parámetro | Rango | Qué hace |
|---|---|---|
| `kickLow`, `snareLow`, … | -12 a +12 dB | Banda de graves de la EQ del instrumento. |
| `kickMid`, `snareMid`, … | -12 a +12 dB | Banda de medios de la EQ del instrumento. |
| `kickHigh`, `snareHigh`, … | -12 a +12 dB | Banda de agudos de la EQ del instrumento. |
| `kickGain`, `snareGain`, … | -12 a +12 dB | Ganancia de la voz. |
| `kickDrive`, … | 0 a 100 % | Pre-ganancia antes de la saturación (de 1x a 10x). |
| `kickSat`, … | Cinta, Válvula, Metal | Curva de saturación de la voz. |
| `kickFilter`, … | 200 a 20 000 Hz | Paso bajo de la voz. A 20 kHz no filtra. |

### Editar cada instrumento

En la parte superior de la ventana hay un selector con **Kick, Snare, Toms, Cymbals y Room**. Elige uno y el bloque **CANAL** muestra su EQ (Low, Mid, High) y su Gain. Cada instrumento conserva su configuración aunque cambies de selección.

El selector solo decide qué instrumento se edita en pantalla: no cambia el procesado. Mover un control de un instrumento no cambia el ecualizador de los demás.

Las bandas de EQ tienen frecuencias de referencia propias para cada instrumento: graves, medios y agudos del bombo no son los mismos que los de los platos.

### Curvas de saturación

- **Cinta:** `y = tanh(1.2·x) / tanh(1.2)`. Curva suave y simétrica: redondea picos y añade calidez.
- **Válvula:** para `x ≥ 0`, `y = 1 - exp(-1.5·x)`; para `x < 0`, `y = -0.8·(1 - exp(1.2·x))`. Asimétrica: el lado positivo satura antes, lo que añade armónicos pares.
- **Metal:** `y = x / (1 + |x|)^0.75`, con recorte a [-1, 1]. Curva dura, con ataque agresivo y armónicos impares.

Todas las salidas quedan en [-1, 1].

### Cómo funciona la mezcla

Cada voz procesa **la misma entrada** con su propio filtro, ganancia y saturación. Sus resultados se suman con un peso de 1/5 para que el nivel no se multiplique. Por eso, con el audio de un solo bus, las voces no separan los instrumentos: solo colorean la mezcla.

Para tratar cada instrumento por separado en el DAW:

1. Envía el bombo, la caja, los toms, los platos y el ambiente a sus propias pistas.
2. Pon una instancia de FARTOVEN DRUMS en cada pista.
3. En cada instancia, usa solo la voz que corresponde (por ejemplo, `kickGain` y `kickSat` en la pista del bombo) y pon el **Mode** adecuado.

Si quieres procesar varios instrumentos en una sola instancia, tendrías que enrutar cada instrumento a una entrada sidechain separada, algo que el plugin no hace todavía.

Importante: el DAW entrega un solo bus. Cada canal procesa una copia del mismo audio y el resultado se suma. Por eso, en una sola instancia, cambiar la EQ de un instrumento sí cambia la suma, pero no altera el procesado de los demás canales.

## Limitaciones conocidas

- No es un synth: procesa el audio de entrada y no genera sonidos a partir de MIDI.
- La interfaz no guarda ajustes distintos por modo: al cambiar de modo se conservan los mismos valores.
- Los coeficientes de los filtros se recalculan al cambiar un parámetro, lo que puede reservar memoria en el hilo de audio. Conviene evitar automatizar esos parámetros de forma continua con bloques muy pequeños.
- AAX no está incluido.
- No hay firma de código ni instalador.

## Licencia

Pendiente de definir. El proyecto no se publica todavía bajo ninguna licencia.

## Créditos

- Motor de audio y cadena de procesado: FARTOVEN.
- Framework: [JUCE](https://juce.com/), descargado durante la compilación.
