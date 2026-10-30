#pragma once

#include "dsp/NoteSegmenter.h"

#include <array>
#include <string>
#include <vector>

// Teoria musical aplicada a la toma: tonalidad, escalas, grid y el "clip" final
// (las notas en negras, tal como se arrastran al piano roll). Sin JUCE.
namespace fourever
{

// --- Tonalidad -----------------------------------------------------------------

struct Key
{
    int  tonic = 0;      // 0 = C ... 11 = B
    bool minor = false;
};

struct KeyEstimate
{
    Key    key;
    double score      = 0.0; // correlacion con el perfil de la tonalidad (-1..1)
    double confidence = 0.0; // 0..1: que tan por encima quedo de la segunda opcion
    std::array<Key, 3> alternatives {}; // las 3 mejores, la primera = key
    bool   valid      = false;          // false si no hay notas suficientes
};

// Krumhansl-Schmuckler: histograma de clases de altura (ponderado por duracion)
// correlacionado con los perfiles de las 24 tonalidades.
KeyEstimate detectKey (const std::vector<NoteEvent>& notes);

std::string keyName (Key k);          // "Am", "F#", "Eb"
std::string keyNameSpanish (Key k);   // "La menor", "Fa# mayor"
std::string camelot (Key k);          // "8A", "2B"

// --- Escalas -------------------------------------------------------------------

enum class Scale
{
    autoDetect,    // mayor o menor segun la tonalidad detectada
    major,
    minor,
    harmonicMinor,
    dorian,
    phrygian,
    pentatonicMajor,
    pentatonicMinor,
    none           // sin ajuste: deja la altura medida tal cual
};

// Intervalos (semitonos desde la tonica). Vacio para Scale::none / autoDetect.
std::vector<int> scaleIntervals (Scale s);
bool isInScale (int midiNote, int tonic, Scale s);

// Nota de la escala mas cercana a una altura medida con decimales.
int snapToScale (float pitch, int tonic, Scale s);

// --- Grid ----------------------------------------------------------------------

enum class Grid { off, quarter, eighth, sixteenth, thirtySecond, eighthTriplet, sixteenthTriplet };

double gridBeats (Grid g); // 1/16 = 0.25 negras, 1/8T = 1/3, etc. (0 = apagado)

// --- Clip: las notas finales, en negras ------------------------------------------

struct ClipNote
{
    double startBeat  = 0.0;
    double lengthBeat = 1.0;
    int    midiNote   = 60;
    float  velocity   = 0.8f;
};

struct ClipSettings
{
    double bpm             = 120.0;
    double hostPpqAtStart  = -1.0; // posicion del proyecto al empezar a grabar (<0 = desconocida)
    int    beatsPerBar     = 4;
    int    keyRoot         = -1;   // -1 = automatica
    Scale  scale           = Scale::autoDetect;
    Grid   grid            = Grid::sixteenth;
    float  strength        = 1.0f; // 0..1, como el "quantize strength" de FL
    int    octaves         = 0;
};

struct Clip
{
    std::vector<ClipNote> notes;
    KeyEstimate detected;          // lo que salio del analisis
    Key         key;               // la que se uso (detectada o elegida)
    Scale       scaleUsed  = Scale::none;
    bool        alignedToProject = false; // notas en su posicion real dentro del compas
    double      originBeat = 0.0;  // en que negra del proyecto empieza el clip
    double      gridBeats  = 0.0;  // celda del grid usado (0 = apagado); la usa el editor
    bool        edited     = false; // true si las notas fueron editadas a mano
    double      lengthBeats() const;
};

// Toma analizada (segundos) -> clip listo para mostrar y arrastrar.
Clip makeClip (const std::vector<NoteEvent>& notes, const ClipSettings& settings);

} // namespace fourever
