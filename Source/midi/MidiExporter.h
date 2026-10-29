#pragma once

#include "dsp/Music.h"

#include <juce_audio_basics/juce_audio_basics.h>

#include <vector>

namespace fourever
{

struct MidiExportSettings
{
    double       bpm       = 120.0;
    int          ppq       = 960;  // resolucion estandar, FL la importa sin perdida
    int          channel   = 1;
    juce::String trackName = "4EVERHUM";
};

// El clip ya viene en negras (cuantizado, en escala y alineado): aqui solo se
// convierte a ticks.
juce::MidiFile buildMidiFile (const std::vector<ClipNote>& notes, const MidiExportSettings& settings);

bool writeMidiFile (const juce::File& file, const std::vector<ClipNote>& notes, const MidiExportSettings& settings);

// Nombre sugerido: "4EVERHUM_Am_140bpm_20261029-171502.mid" (sin tonalidad si no hay)
juce::String suggestedMidiFileName (double bpm, const juce::String& keyLabel = {});

} // namespace fourever
