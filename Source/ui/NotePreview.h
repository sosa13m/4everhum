#pragma once

#include "dsp/Music.h"

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

    void setClip (fourever::Clip newClip, int beatsPerBar);
    bool hasNotes() const { return ! clip.notes.empty(); }

    // Lo asigna el editor: escribe el .mid y devuelve el archivo.
    std::function<juce::File()> createMidiFileForDrag;

    void paint (juce::Graphics&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override { dragStarted = false; }

private:
    fourever::Clip clip;
    int beatsPerBar = 4;
    bool dragStarted = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NotePreview)
};
