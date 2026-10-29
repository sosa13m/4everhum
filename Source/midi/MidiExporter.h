#pragma once

#include "dsp/NoteSegmenter.h"

#include <juce_audio_basics/juce_audio_basics.h>

#include <vector>

namespace fourever
{

struct MidiExportSettings
{
    double      bpm                = 120.0;
    int         ppq                = 960;  // resolucion estandar, FL la importa sin perdida
    int         channel            = 1;
    bool        trimLeadingSilence = true; // la primera nota cae en el tiempo 1
    juce::String trackName         = "4EVERHUM";
};

juce::MidiFile buildMidiFile (const std::vector<NoteEvent>& notes, const MidiExportSettings& settings);

bool writeMidiFile (const juce::File& file, const std::vector<NoteEvent>& notes, const MidiExportSettings& settings);

// Nombre sugerido: "4EVERHUM_140bpm_20261029-171502.mid"
juce::String suggestedMidiFileName (double bpm);

} // namespace fourever
