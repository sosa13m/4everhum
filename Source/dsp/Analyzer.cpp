// 4EVERHUM - tararea -> MIDI
// Copyright (C) 2026 sosa13m / colectivo 4EVER
// SPDX-License-Identifier: AGPL-3.0-or-later
// Este programa es software libre: ver el archivo LICENSE.

#include "dsp/Analyzer.h"

#include <algorithm>
#include <cmath>

namespace fourever
{

PitchTrackerSettings toPitchSettings (const UserSettings& u)
{
    const float s = std::clamp (u.sensitivity, 0.0f, 100.0f);

    PitchTrackerSettings p;
    p.silenceDb    = -30.0f - 0.35f * s;   //   0 -> -30 dB, 50 -> -47.5 dB, 100 -> -65 dB
    p.yinThreshold = 0.10f + 0.001f * s;   //   0 -> 0.10,   50 -> 0.15,     100 -> 0.20
    p.autoGate     = u.autoGate;
    p.method       = u.highPrecision ? PitchMethod::pyin : PitchMethod::yin;
    return p;
}

SegmenterSettings toSegmenterSettings (const UserSettings& u)
{
    const float s  = std::clamp (u.sensitivity, 0.0f, 100.0f);
    const float st = std::clamp (u.stability, 0.0f, 100.0f);

    SegmenterSettings g;
    g.minConfidence = 0.65f - 0.003f * s;                                // 0.65 .. 0.35
    g.hysteresis    = 0.50f + 0.004f * st;                               // 0.50 .. 0.90 semitonos
    g.medianFrames  = 3 + 2 * (int) std::lround (st / 33.4f);            // 3, 5, 7 o 9 frames
    g.minNoteSec    = std::clamp (u.minNoteMs, 30.0f, 300.0f) / 1000.0;
    return g;
}

std::vector<NoteEvent> analyze (const float* samples, std::size_t numSamples,
                                double sampleRate, const UserSettings& settings)
{
    const auto frames = trackPitch (samples, numSamples, sampleRate, toPitchSettings (settings));
    return segmentNotes (frames, toSegmenterSettings (settings));
}

std::vector<NoteEvent> transposeOctaves (std::vector<NoteEvent> notes, int octaves)
{
    for (auto& n : notes)
    {
        // Si la nota se saldria del rango MIDI se queda en la octava mas cercana valida.
        int m = n.midiNote + 12 * octaves;
        while (m > 127) m -= 12;
        while (m < 0)   m += 12;
        n.pitch += (float) (m - n.midiNote);
        n.midiNote = m;
    }
    return notes;
}

} // namespace fourever
