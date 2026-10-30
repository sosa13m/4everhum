#pragma once

#include "dsp/ClipEdits.h"
#include "dsp/Music.h"

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include <functional>
#include <vector>

// Mini piano roll editable (Sprint 4).
//   clic: elegir nota · arrastrar: mover (tiempo en el grid, altura en semitonos)
//   arrastrar el borde derecho: alargar · doble clic en vacio: agregar
//   clic derecho o Supr: borrar · Ctrl+Z / Ctrl+Y: deshacer / rehacer
// El clip se envia al DAW desde el asa "ARRASTRAR AL PIANO ROLL".
class NotePreview final : public juce::Component,
                          public juce::SettableTooltipClient
{
public:
    NotePreview();
    ~NotePreview() override;

    void setClip (fourever::Clip newClip, int beatsPerBar);
    bool hasNotes() const { return ! clip.notes.empty(); }

    std::function<juce::File()> createMidiFileForDrag;                    // escribe el .mid a arrastrar
    std::function<void (std::vector<fourever::ClipNote>)> onNotesEdited;  // la edicion se confirmo
    std::function<void()> onRestoreAutomatic;                             // volver a lo detectado

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    bool keyPressed (const juce::KeyPress&) override;

private:
    // Geometria del piano roll: tiempo (negras) <-> x, nota MIDI <-> y.
    struct Layout
    {
        juce::Rectangle<float> area;
        double beats = 4.0;
        int    lo = 48, hi = 72;
        float  rowH = 10.0f;

        float  xOf (double beat) const { return area.getX() + (float) (beat / beats) * area.getWidth(); }
        float  yOf (int note) const    { return area.getBottom() - (float) (note - lo + 1) * rowH; }
        double beatAt (float x) const  { return (double) ((x - area.getX()) / area.getWidth()) * beats; }
        int    noteAt (float y) const  { return lo + (int) std::floor ((area.getBottom() - y) / rowH); }
        juce::Rectangle<float> rectOf (const fourever::ClipNote&) const;
    };

    enum class Gesture { none, move, resize };

    class DragHandle;

    Layout computeLayout() const;
    const Layout& layout() const { return gesture != Gesture::none ? frozenLayout : liveLayout; }
    double editGrid (const juce::ModifierKeys&) const;
    void commit (std::vector<fourever::ClipNote> newNotes, const std::vector<fourever::ClipNote>& before);
    void deleteSelected();
    void undo();
    void redo();
    void updateButtons();
    void startExternalDrag();

    fourever::Clip clip;
    int beatsPerBar = 4;
    bool hasPendingClip = false;
    fourever::Clip pendingClip;   // llego un clip nuevo en medio de un arrastre
    int pendingBeatsPerBar = 4;

    Layout liveLayout, frozenLayout;
    Gesture gesture = Gesture::none;
    int selected = -1;
    fourever::ClipNote dragOrigin;
    std::vector<fourever::ClipNote> beforeGesture;
    bool gestureChanged = false;

    fourever::EditHistory history;

    juce::TextButton undoButton, redoButton, restoreButton;
    std::unique_ptr<DragHandle> dragHandle;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NotePreview)
};
