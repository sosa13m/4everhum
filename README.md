# 4EVERHUM — tararea → MIDI

Plugin de audio (VST3 / AU / Standalone) de la familia **4EVERTOOLS**. Grabas tu
voz tarareando, el plugin detecta las notas y te entrega un `.mid` que
**arrastras directo al piano roll** de cualquier instrumento (Purity, Nexus,
Serum…) en FL Studio, Ableton, Reaper, etc.

**Estado: Sprint 2**
- Sprint 1: grabación, detección de notas (YIN), exportación MIDI con el tempo del
  proyecto y arrastre al DAW.
- Sprint 2: perillas de ajuste con re-análisis instantáneo (sin volver a grabar),
  filtro de ruido automático, transposición por octavas, exportar la toma en
  `.wav` y la herramienta de terminal `hum2midi`.

Aún sin escala ni cuantización al grid (Sprint 3).

---

## Descargar sin compilar

Cada `git push` dispara **GitHub Actions**, que compila todo para Windows. En la
pestaña **Actions** del repo, entra a la última ejecución y descarga el artifact
**4EVERHUM-windows** (VST3, Standalone y `hum2midi.exe`).

## Requisitos para compilar (Windows)

- **Visual Studio 2022 Build Tools** con la carga "Desarrollo para el escritorio con C++"
- **CMake ≥ 3.22** y **Git** en el PATH

JUCE 8 se descarga solo la primera vez que configuras (tarda 1–2 min).

## Compilar

```powershell
cmake -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release --target FourEverHum_VST3 FourEverHum_Standalone hum2midi
ctest --test-dir build -C Release --output-on-failure
```

Resultado:

- `build\FourEverHum_artefacts\Release\VST3\4EVERHUM.vst3`
- `build\FourEverHum_artefacts\Release\Standalone\4EVERHUM.exe` (para probar sin DAW)
- `build\hum2midi_artefacts\Release\hum2midi.exe`

## Instalar el VST3

En una PowerShell **como administrador**:

```powershell
Copy-Item -Recurse -Force build\FourEverHum_artefacts\Release\VST3\4EVERHUM.vst3 "C:\Program Files\Common Files\VST3\"
```

## Usarlo en FL Studio

1. **Options → Manage plugins → Find plugins** para que FL detecte 4EVERHUM.
2. En el **Mixer**, elige un track libre y en su entrada (arriba a la derecha) selecciona tu micrófono.
3. Agrega **4EVERHUM** en un slot de efectos de ese track.
4. Pulsa **● Grabar**, tararea la melodía y pulsa **■ Detener**. Puedes grabar con el proyecto sonando: el plugin toma el tempo de FL.
5. Abre el **piano roll del canal de Purity** (o el instrumento que quieras) y **arrastra la melodía** desde la vista de notas de 4EVERHUM hasta el piano roll.

Consejos:

- Usa audífonos para que el beat no se cuele en el micrófono.
- Tararea con sílabas claras ("da", "na"); separar las notas ayuda a detectar repeticiones.
- **Guardar .mid…** sirve si prefieres guardarlo en una carpeta.

El plugin **no altera el audio**: la señal del micrófono pasa tal cual.

## Las perillas

Afinan el resultado **sobre la última toma, sin volver a grabar**: mueves una
perilla y la melodía se recalcula al instante.

| Control | Si pasa esto… | …haz esto |
|---|---|---|
| **Sensibilidad** | Se pierden notas que sí cantaste | Súbela |
|  | Aparecen notas que no cantaste | Bájala |
| **Nota mínima** | Salen notas cortitas sueltas | Súbela |
|  | Se pierden notas rápidas | Bájala |
| **Estabilidad** | Una nota larga se parte en varias | Súbela |
|  | Dos notas cercanas salen como una | Bájala |
| **Octava** | Tarareas grave y quieres la melodía en otro registro | Elige ±1 o ±2 |
| **Filtro de ruido** | El beat se filtra al micrófono y aparecen notas graves fantasma | Déjalo activado |

Las perillas se guardan con el proyecto del DAW y se pueden automatizar. La toma
de audio no se guarda con el proyecto (para no inflarlo); si quieres conservarla,
usa **Guardar toma .wav…**.

## hum2midi (terminal)

El mismo motor del plugin, para convertir grabaciones sin abrir el DAW. Sirve
para probar ajustes rápido sobre tus tomas reales (guardadas con **Guardar toma
.wav…** o grabadas con el celular).

```powershell
hum2midi toma.wav
hum2midi toma.wav --bpm 140 --octava 1 -o melodia.mid
hum2midi toma.wav --sensibilidad 70 --estabilidad 80
hum2midi --ayuda
```

Acepta WAV, AIFF, FLAC y MP3. Imprime cada nota con su nombre en la convención
de FL (Do central = C5), su posición en negras y su duración.

## Probar el DSP sin compilar JUCE

```bash
cmake -B build-tests -DFOUREVERHUM_BUILD_PLUGIN=OFF
cmake --build build-tests
ctest --test-dir build-tests --output-on-failure
```

Los tests generan una "voz" sintética con vibrato, ruido y portamento, y
verifican alturas exactas y tiempos de inicio (error < 50 ms) a 44.1, 48 y 96 kHz.
También cubren cada perilla: el filtro de ruido con un bajo filtrándose al
micrófono, la sensibilidad con voz muy suave, la nota mínima y las octavas.

## Estructura

```
Source/
  dsp/PitchTracker.*     Deteccion de pitch YIN (decimado a ~16 kHz, sin JUCE)
  dsp/NoteSegmenter.*    Curva de pitch -> notas (mediana, histeresis, portamentos)
  dsp/Analyzer.*         Perillas -> parametros internos; analisis completo de una toma
  midi/MidiExporter.*    Notas -> .mid (960 PPQ, tempo del proyecto)
  ui/NotePreview.*       Mini piano roll + arrastre al DAW
  ui/HumLookAndFeel.*    Botones sticker, perillas, checkbox y tooltips
  ui/Theme.*             Paleta SOTANO 4EVER, tipografias, grano y cinta
Resources/fonts/         Anton, Special Elite y Space Mono (con sus licencias)
Resources/images/        Logo 4ever
  PluginProcessor.*      Grabacion lock-free + analisis en hilo de fondo
  PluginEditor.*         Interfaz
tools/hum2midi.cpp       Conversor WAV -> MIDI por terminal
tests/                   Tests del DSP y del export MIDI
.github/workflows/       Build automatico del VST3 para Windows
```

## Diseño

La interfaz usa la identidad del tema **SÓTANO 4EVER** de 4EVERTOOLS: pared de
concreto bajo una ampolleta, el cartel negro con borde rojo de la tienda, una
hoja fotocopiada pegada con cinta como zona de trabajo y botones tipo sticker
con sombra dura. Misma paleta (`#d93025`, `#ece6d8`, `#121010`) y mismas
tipografías que la web: **Anton** (titulares), **Special Elite** (etiquetas a
máquina) y **Space Mono** (números). Las fuentes van embebidas en el plugin, así
que se ven igual en cualquier PC.

Fuentes: Anton y Space Mono bajo SIL Open Font License 1.1; Special Elite bajo
Apache License 2.0. Las licencias están en `Resources/fonts/`.

## Licencia de JUCE

JUCE 8 es gratis para uso personal y para productos bajo cierto umbral de
ingresos (plan Starter), o bajo AGPLv3. Antes de vender el plugin revisa
https://juce.com/get-juce/ para ver qué plan te corresponde.

## Próximos sprints

- **Sprint 3:** detección de tonalidad, ajuste a escala y cuantización al grid.
- **Sprint 4:** edición de notas en el mini piano roll antes de arrastrar.
- **Sprint 5:** modo alta calidad con red neuronal (Basic Pitch / CREPE).
