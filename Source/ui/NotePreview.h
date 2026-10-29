#pragma once

#include "dsp/NoteSegmenter.h"

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include <functional>
#include <vector>

// Mini piano roll de solo lectura. Es tambien la zona de arrastre: al
// arrastrarlo hacia fuera se genera un .mid y se suelta en el DAW.
class NotePreview final : public juce::Component
{
public:
    NotePreview();

    void setNotes (std::vector<fourever::NoteEvent> newNotes, double bpm);
    bool hasNotes() const { return ! notes.empty(); }

    // Lo asigna el editor: escribe el .mid y devuelve el archivo.
    std::function<juce::File()> createMidiFileForDrag;

    void paint (juce::Graphics&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override { dragStarted = false; }

private:
    std::vector<fourever::NoteEvent> notes;
    double bpm = 120.0;
    bool dragStarted = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NotePreview)
};
