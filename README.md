# 4EVERHUM — tararea → MIDI

Plugin de audio (VST3 / Standalone) de la familia **4EVERTOOLS**. Grabas tu voz
tarareando, el plugin detecta la melodía —con su tonalidad, en escala y en el
grid— y la **arrastras directo al piano roll** de cualquier instrumento (Purity,
Nexus, Serum…) en FL Studio, Ableton, Reaper, etc.

**Versión 1.0.0** · los cambios están en [CHANGELOG.md](CHANGELOG.md).

---

## Instalar (Windows)

1. Descarga el zip de la última versión en **Releases** del repo
   (`4EVERHUM-v1.0.0-windows.zip`) y descomprímelo completo.
2. Doble clic en **`instalar.bat`** y acepta el permiso de administrador.
   - Copia el plugin a `C:\Program Files\Common Files\VST3\` (donde lo busca FL).
   - Instala la app Standalone (menú Inicio → **4EVERHUM**) y `hum2midi` en tu PATH.
   - Si Windows dice "Windows protegió su PC": *Más información → Ejecutar de todas
     formas* (el instalador no está firmado).
3. En FL Studio: **Options → Manage plugins → Find plugins**.

Para desinstalar: `desinstalar.bat` (queda en `%LOCALAPPDATA%\4EVERHUM`).

## Usar en FL Studio

1. En el **Mixer**, elige un track libre y pon tu micrófono como entrada.
2. Agrega **4EVERHUM** en un slot de efectos de ese track.
3. **GRABAR** → tararea → **DETENER**. Usa audífonos para que el beat no se cuele
   en el micrófono. Si grabas con el proyecto sonando, las notas quedan en su
   compás real.
4. Abre el piano roll del canal de tu instrumento y arrastra la melodía desde el
   botón negro **ARRASTRAR AL PIANO ROLL**.

El plugin **no altera el audio**: la señal del micrófono pasa tal cual.

## Los controles

Todo se recalcula al instante sobre la última toma: no hay que volver a grabar.

**DETECCIÓN**

| Control | Si pasa esto… | …haz esto |
|---|---|---|
| **Sensibilidad** | Se pierden notas que sí cantaste / aparecen notas que no | Súbela / bájala |
| **Nota mínima** | Salen notas cortitas sueltas / se pierden notas rápidas | Súbela / bájala |
| **Estabilidad** | Una nota larga se parte en varias / dos notas salen como una | Súbela / bájala |
| **Filtro de ruido** | El beat se filtra al micrófono y aparecen notas graves fantasma | Déjalo activado |
| **Alta precisión** | Voz grave o rasposa con saltos de octava | Déjalo activado (pYIN) |

**MÚSICA**

- **Tonalidad**: en *Auto* el plugin la detecta y la estampa en el sello
  (`LA MENOR · Am · 8A`, con el código Camelot del detector de 4EVERTOOLS). Si ya
  sabes en qué tonalidad está el beat, elígela.
- **Escala**: cada nota cae en la nota de la escala más cercana a lo que de verdad
  cantaste. *Auto* usa mayor o menor según la tonalidad; también hay menor
  armónica, dórica, frigia, pentatónicas y *Sin escala*.
- **Cuantizar** y **Fuerza**: de 1/4 a 1/32 y tresillos (1/8T, 1/16T). Con fuerza
  100% quedan exactas; con menos conservan algo de tu groove.
- **Octava**: sube o baja toda la melodía hasta 2 octavas.

Los controles se guardan con el proyecto del DAW y se pueden automatizar.

## Editar la melodía

En la hoja (el piano roll):

- **clic**: elegir una nota · **arrastrar**: moverla (tiempo en el grid, altura en
  semitonos; con **Shift** se mueve libre)
- **arrastrar el borde derecho**: alargarla o acortarla
- **doble clic** en un espacio vacío: agregar una nota (ya en la escala)
- **clic derecho** o **Supr**: borrar · **flechas**: mover la nota elegida
- **Deshacer / Rehacer** (o Ctrl+Z / Ctrl+Y)
- **Automático**: descarta tus ediciones y vuelve a la melodía detectada

Mientras la melodía está editada a mano, las perillas no la cambian. Las
ediciones se guardan con el proyecto.

## Temas

Arriba a la derecha eliges el tema, igual que en 4EVERTOOLS. Se recuerda para la
próxima vez.

| Tema | Mundo | Tipografías |
|---|---|---|
| **SÓTANO 4EVER** | disquería en un sótano | Anton · Special Elite · Space Mono |
| **LINEA 4 EVER** | metro con graffiti | Sedgwick Ave Display · Bungee · Inter |
| **4EVERTALE** | RPG de 16 bits | Pixelify Sans · Press Start 2P |
| **LIBRETA NEGRA** | blackbook en blanco y negro | Permanent Marker · Caveat |
| **4EVERPALOOZA** | festival, verde tóxico y rosado | Big Shoulders Display · Archivo |

La ventana se puede agrandar desde la esquina: crece el piano roll.

## hum2midi (terminal)

El mismo motor del plugin, para convertir grabaciones sin abrir el DAW (por
ejemplo una toma guardada con **Guardar toma .wav…** o grabada con el celular).

```powershell
hum2midi toma.wav
hum2midi toma.wav --bpm 140 --octava 1 -o melodia.mid
hum2midi toma.wav --tonalidad Am --cuantizar 1/8
hum2midi toma.wav --escala frigia --cuantizar 1/16T --fuerza 70
hum2midi --ayuda
```

Acepta WAV, AIFF, FLAC y MP3. Imprime la tonalidad (con Camelot y alternativas)
y cada nota como queda en el `.mid` (nombres de FL: Do central = C5).

---

## Para desarrolladores

### Compilar (Windows)

Requisitos: **Visual Studio 2022 Build Tools** ("Desarrollo para el escritorio con
C++"), **CMake ≥ 3.22** y **Git**. JUCE 8 se descarga solo la primera vez.

```powershell
cmake -B build -A x64
cmake --build build --config Release --target FourEverHum_VST3 FourEverHum_Standalone hum2midi
ctest --test-dir build -C Release --output-on-failure
```

Sin instalar nada: cada `git push` compila en **GitHub Actions** (artifact
*4EVERHUM-windows*), y cada tag `v*` publica una **Release** con el zip listo:

```powershell
git tag v1.0.0
git push origin v1.0.0
```

### Tests

```bash
cmake -B build-tests -DFOUREVERHUM_BUILD_PLUGIN=OFF
cmake --build build-tests
ctest --test-dir build-tests --output-on-failure
```

- `dsp_tests`: detección con voz sintética (vibrato, ruido, portamento) a 44.1, 48
  y 96 kHz; cada perilla; un cantante desafinado 35 cents y desfasado 40 ms que
  tiene que salir exacto en escala y en corcheas.
- `music_tests`: tonalidad en las 12 transposiciones, Camelot, escalas, grid con
  fuerza y tresillos, alineado al compás.
- `edit_tests`: mover, alargar, agregar, borrar, deshacer y rehacer.
- `bench_pitch`: YIN vs pYIN con voces difíciles y altura real conocida (falla si
  pYIN empeora algún caso). Voz rasposa: YIN 2/10 notas, pYIN 8/10.
- `midi_export_test`: voz → `.mid` en disco → se relee y se verifica.

### Estructura

```
Source/
  dsp/PitchTracker.*     YIN y pYIN (candidatos + HMM/Viterbi), decimado a ~16 kHz
  dsp/NoteSegmenter.*    Curva de altura -> notas (mediana, histeresis, portamentos)
  dsp/Analyzer.*         Perillas -> parametros internos; analisis de una toma
  dsp/Music.*            Tonalidad, Camelot, escalas, grid y el clip final en negras
  dsp/ClipEdits.*        Edicion manual del clip y deshacer/rehacer
  midi/MidiExporter.*    Clip -> .mid (960 PPQ, tempo del proyecto)
  ui/NotePreview.*       Piano roll editable + asa de arrastre al DAW
  ui/HumLookAndFeel.*    Botones, perillas, combos, casillas y tooltips
  ui/Theme.*             Los 5 temas: paletas, tipografias, logos, texturas
  PluginProcessor.*      Grabacion lock-free, analisis en hilo de fondo, estado
  PluginEditor.*         Interfaz
Resources/fonts/         Tipografias de los temas (con sus licencias)
Resources/images/        Logos de los temas
tools/hum2midi.cpp       Conversor audio -> MIDI por terminal
tests/                   Tests y banco de pruebas
packaging/               Instalador, desinstalador y LEEME del zip
.github/workflows/       build.yml (cada push) y release.yml (cada tag v*)
```

### Licencias

- **JUCE 8**: gratis para uso personal y productos bajo cierto umbral de ingresos
  (plan Starter), o bajo AGPLv3. Antes de vender el plugin revisa
  https://juce.com/get-juce/.
- **Tipografías**: SIL Open Font License 1.1 (Anton, Space Mono, Sedgwick Ave
  Display, Bungee, Inter, Pixelify Sans, Press Start 2P, Caveat, Archivo, Big
  Shoulders Display) y Apache License 2.0 (Special Elite, Permanent Marker). Las
  variables se instanciaron al peso que usa 4EVERTOOLS y se recortaron a
  caracteres latinos; Press Start 2P va sin modificar por su *Reserved Font Name*.
  Licencias en `Resources/fonts/`.

### Ideas a futuro

- Modelo neuronal (Basic Pitch / CREPE) como tercer motor de detección.
- Versión para macOS (AU/VST3).
- Salida MIDI en vivo mientras tarareas.
