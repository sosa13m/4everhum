#pragma once

#include "dsp/NoteSegmenter.h"
#include "dsp/PitchTracker.h"

#include <cstddef>
#include <vector>

namespace fourever
{

// Los ajustes tal como los ve el usuario (perillas del plugin / flags de hum2midi).
// Se traducen a los parametros internos del detector y el segmentador.
struct UserSettings
{
    float sensitivity = 50.0f; // 0..100: mas alto = detecta voz mas suave
    float minNoteMs   = 70.0f; // 30..300: notas mas cortas se descartan
    float stability   = 50.0f; // 0..100: mas alto = ignora mas el vibrato y las desafinaciones
    bool  autoGate    = true;  // ajustar el umbral al ruido de fondo de la toma
};

PitchTrackerSettings toPitchSettings (const UserSettings&);
SegmenterSettings    toSegmenterSettings (const UserSettings&);

// Toma completa -> notas.
std::vector<NoteEvent> analyze (const float* samples, std::size_t numSamples,
                                double sampleRate, const UserSettings& settings);

// Sube o baja las notas en octavas (tararear grave y querer el piano agudo).
std::vector<NoteEvent> transposeOctaves (std::vector<NoteEvent> notes, int octaves);

} // namespace fourever
