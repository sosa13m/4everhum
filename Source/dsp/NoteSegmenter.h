#pragma once

#include "dsp/PitchTracker.h"

#include <vector>

namespace fourever
{

struct NoteEvent
{
    double startSec  = 0.0;
    double endSec    = 0.0;
    int    midiNote  = 60;   // 60 = C5 en FL Studio (C4 en la convencion "cientifica")
    float  pitch     = 60.0f; // altura medida con decimales (60.4 = un poco alto de C5)
    float  velocity  = 0.8f; // 0..1
};

struct SegmenterSettings
{
    float  minConfidence = 0.5f;  // frames con menos confianza se tratan como silencio
    int    medianFrames  = 5;     // suavizado: quita saltos sueltos y parte del vibrato
    float  hysteresis    = 0.65f; // semitonos que hay que alejarse para cambiar de nota
    double minNoteSec    = 0.07;  // notas mas cortas se absorben o se descartan
};

// Convierte la curva de pitch en notas discretas (aun sin escala ni grid:
// eso llega en el sprint 3).
std::vector<NoteEvent> segmentNotes (const std::vector<PitchFrame>& frames,
                                     const SegmenterSettings& settings = {});

} // namespace fourever
