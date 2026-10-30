// 4EVERHUM - tararea -> MIDI
// Copyright (C) 2026 sosa13m / colectivo 4EVER
// SPDX-License-Identifier: AGPL-3.0-or-later
// Este programa es software libre: ver el archivo LICENSE.

#include "ui/NotePreview.h"
#include "ui/HumLookAndFeel.h"
#include "ui/Theme.h"

// --- Asa de arrastre al DAW -------------------------------------------------------

class NotePreview::DragHandle final : public juce::Component,
                                      public juce::SettableTooltipClient
{
public:
    explicit DragHandle (NotePreview& o) : owner (o)
    {
        setMouseCursor (juce::MouseCursor::DraggingHandCursor);
        setRepaintsOnMouseActivity (true);
        setTooltip (theme::ui ("Arrastra desde aquí hasta el piano roll de tu instrumento en FL "
                               "(o a cualquier DAW o carpeta). Se lleva la melodía tal como la ves."));
    }

    void paint (juce::Graphics& g) override
    {
        auto face = getLocalBounds().toFloat().reduced (1.0f);
        face.removeFromRight (HumLookAndFeel::shadowSpace);
        face.removeFromBottom (HumLookAndFeel::shadowSpace);

        const bool on = isEnabled();
        if (on)
            theme::drawHardShadow (g, face, { 3.0f, 3.0f }, theme::c().red);
        if (on && isMouseButtonDown())
            face.translate (2.0f, 2.0f);

        g.setColour (on ? (isMouseOver() ? theme::c().ink.brighter (0.15f) : theme::c().ink) : theme::c().paper2);
        g.fillRect (face);
        g.setColour (on ? theme::c().ink : theme::c().ink.withAlpha (0.3f));
        g.drawRect (face, 2.0f);

        // Flecha hacia afuera (hacia el DAW).
        auto arrowBox = face.removeFromRight (30.0f).reduced (8.0f, 9.0f);
        juce::Path arrow;
        arrow.startNewSubPath (arrowBox.getX(), arrowBox.getCentreY());
        arrow.lineTo (arrowBox.getRight(), arrowBox.getCentreY());
        arrow.startNewSubPath (arrowBox.getRight() - 6.0f, arrowBox.getCentreY() - 5.0f);
        arrow.lineTo (arrowBox.getRight(), arrowBox.getCentreY());
        arrow.lineTo (arrowBox.getRight() - 6.0f, arrowBox.getCentreY() + 5.0f);
        const auto& t = theme::c();
        const bool accentShows = std::abs (t.red.getPerceivedBrightness() - t.ink.getPerceivedBrightness()) > 0.2f;
        g.setColour (on ? (accentShows ? t.red : t.paper) : t.inkSoft);
        g.strokePath (arrow, juce::PathStrokeType (2.4f, juce::PathStrokeType::mitered, juce::PathStrokeType::square));

        g.setColour (on ? theme::c().paper : theme::c().inkSoft);
        g.setFont (theme::anton (16.0f));
        g.drawText ("ARRASTRAR AL PIANO ROLL", face.withTrimmedLeft (10.0f), juce::Justification::centredLeft);
    }

    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (! dragging && isEnabled() && e.getDistanceFromDragStart() > 4)
        {
            dragging = true;
            owner.startExternalDrag();
        }
    }

    void mouseUp (const juce::MouseEvent&) override { dragging = false; repaint(); }

private:
    NotePreview& owner;
    bool dragging = false;
};

// --- Piano roll ----------------------------------------------------------------------

NotePreview::NotePreview()
{
    setWantsKeyboardFocus (true);
    setRepaintsOnMouseActivity (true);
    setTooltip (theme::ui ("Clic: elegir nota · arrastrar: mover · borde derecho: alargar · "
                           "doble clic: agregar · clic derecho o Supr: borrar · Ctrl+Z: deshacer"));

    auto setupButton = [this] (juce::TextButton& b, const char* text, const char* tip)
    {
        b.getProperties().set (HumLookAndFeel::styleKey, "segment");
        b.setButtonText (theme::ui (text));
        b.setTooltip (theme::ui (tip));
        b.setWantsKeyboardFocus (false);
        addAndMakeVisible (b);
    };
    setupButton (undoButton, "DESHACER", "Deshace el último cambio (Ctrl+Z).");
    setupButton (redoButton, "REHACER", "Rehace lo que deshiciste (Ctrl+Y).");
    setupButton (restoreButton, "AUTOMÁTICO", "Descarta tus ediciones y vuelve a la melodía detectada; "
                                              "las perillas vuelven a mandar.");
    undoButton.onClick    = [this] { undo(); };
    redoButton.onClick    = [this] { redo(); };
    restoreButton.onClick = [this] { history.clear(); selected = -1; if (onRestoreAutomatic) onRestoreAutomatic(); };

    dragHandle = std::make_unique<DragHandle> (*this);
    addAndMakeVisible (*dragHandle);

    updateButtons();
}

NotePreview::~NotePreview() = default;

void NotePreview::setClip (fourever::Clip newClip, int newBeatsPerBar)
{
    if (gesture != Gesture::none)
    {
        // No cambiar el clip en medio de un arrastre: se aplica al soltar.
        pendingClip = std::move (newClip);
        pendingBeatsPerBar = newBeatsPerBar;
        hasPendingClip = true;
        return;
    }

    clip        = std::move (newClip);
    beatsPerBar = juce::jmax (1, newBeatsPerBar);
    if (! clip.edited)
        history.clear(); // volvio a lo automatico (o es una toma nueva)
    if (selected >= (int) clip.notes.size())
        selected = -1;

    liveLayout = computeLayout();
    updateButtons();
    repaint();
}

juce::Rectangle<float> NotePreview::Layout::rectOf (const fourever::ClipNote& n) const
{
    const float x0 = xOf (n.startBeat);
    return { x0, yOf (n.midiNote), juce::jmax (4.0f, xOf (n.startBeat + n.lengthBeat) - x0), rowH };
}

NotePreview::Layout NotePreview::computeLayout() const
{
    Layout L;
    auto area = getLocalBounds().toFloat().reduced (14.0f, 12.0f);
    area.removeFromBottom (40.0f); // barra de herramientas + asa de arrastre
    area.removeFromTop (4.0f);
    L.area = area;

    const double bar = (double) beatsPerBar;
    // Siempre queda algo de espacio despues de la ultima nota para poder agregar.
    L.beats = juce::jmax (bar, std::ceil ((clip.lengthBeats() + 0.5) / bar - 1.0e-9) * bar);

    int lo = 127, hi = 0;
    for (const auto& n : clip.notes)
    {
        lo = juce::jmin (lo, n.midiNote);
        hi = juce::jmax (hi, n.midiNote);
    }
    if (clip.notes.empty()) { lo = 57; hi = 69; }
    lo -= 2; hi += 2;
    if (hi - lo < 12) { const int pad = (12 - (hi - lo)) / 2; lo -= pad; hi = lo + 12; }
    L.lo = juce::jmax (0, lo);
    L.hi = juce::jmin (127, hi);
    L.rowH = area.getHeight() / (float) (L.hi - L.lo + 1);
    return L;
}

void NotePreview::resized()
{
    auto bar = getLocalBounds().reduced (14, 8).removeFromBottom (38);
    dragHandle->setBounds (bar.removeFromRight (262));
    undoButton.setBounds (bar.removeFromLeft (104));
    bar.removeFromLeft (4);
    redoButton.setBounds (bar.removeFromLeft (100));
    bar.removeFromLeft (4);
    restoreButton.setBounds (bar.removeFromLeft (130));

    liveLayout = computeLayout();
}

void NotePreview::updateButtons()
{
    const bool notes = hasNotes();
    undoButton.setVisible (notes);
    redoButton.setVisible (notes);
    undoButton.setEnabled (history.canUndo());
    redoButton.setEnabled (history.canRedo());
    restoreButton.setVisible (clip.edited);
    dragHandle->setEnabled (notes);
    dragHandle->repaint();
}

// --- Pintura ---------------------------------------------------------------------------

void NotePreview::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat().reduced (1.0f);

    juce::Path frame;
    frame.addRectangle (bounds);
    juce::Path dashed;
    const float dashes[] = { 7.0f, 5.0f };
    juce::PathStrokeType (1.6f).createDashedStroke (dashed, frame, dashes, 2);
    g.setColour (theme::c().cardInk.withAlpha (0.55f));
    g.fillPath (dashed);

    if (clip.notes.empty())
    {
        auto area = getLocalBounds().reduced (20).withTrimmedBottom (40);
        auto block = area.withSizeKeepingCentre (area.getWidth(), 86);
        g.setColour (theme::c().cardInk);
        g.setFont (theme::title (44.0f));
        g.drawText (theme::ui ("TARAREA ALGO."), block.removeFromTop (52), juce::Justification::centred);
        g.setColour (theme::c().cardInkSoft);
        g.setFont (theme::typed (16.0f));
        g.drawText (theme::ui ("Pulsa GRABAR, canta la melodía y pulsa DETENER."),
                    block, juce::Justification::centredTop);
        return;
    }

    const auto& L = layout();
    const auto& area = L.area;

    // Filas de teclas negras, como en un piano roll (un tono de papel mas oscuro).
    for (int nt = L.lo; nt <= L.hi; ++nt)
    {
        const int pc = ((nt % 12) + 12) % 12;
        if (pc == 1 || pc == 3 || pc == 6 || pc == 8 || pc == 10)
        {
            g.setColour (theme::c().cardInk.withAlpha (0.06f));
            g.fillRect (area.getX(), L.yOf (nt), area.getWidth(), L.rowH);
        }
    }

    // Grid: negras y compases. Si el clip esta alineado al proyecto, los numeros
    // de compas son los del proyecto de FL.
    const int firstBar = clip.alignedToProject ? juce::roundToInt (clip.originBeat / beatsPerBar) + 1 : 1;
    g.setFont (theme::mono (10.0f));
    for (int beat = 0; beat <= juce::roundToInt (L.beats); ++beat)
    {
        const float x = L.xOf ((double) beat);
        const bool isBar = beat % beatsPerBar == 0;
        g.setColour (theme::c().cardInk.withAlpha (isBar ? 0.38f : 0.13f));
        g.drawVerticalLine (juce::roundToInt (x), area.getY(), area.getBottom());
        if (isBar && beat < juce::roundToInt (L.beats))
        {
            g.setColour (theme::c().cardInkSoft);
            g.drawText (juce::String (firstBar + beat / beatsPerBar), juce::Rectangle<float> (x + 3.0f, area.getY() - 1.0f, 30.0f, 12.0f),
                        juce::Justification::topLeft);
        }
    }

    // Notas: bloques rojos de tinta con sombra dura. La elegida va en tinta.
    for (int i = 0; i < (int) clip.notes.size(); ++i)
    {
        const auto& n = clip.notes[(std::size_t) i];
        auto r = L.rectOf (n).reduced (0.0f, juce::jmin (1.0f, L.rowH * 0.1f));
        const bool sel = i == selected;

        g.setColour (sel ? theme::c().red.withAlpha (0.55f) : theme::c().ink.withAlpha (0.3f));
        g.fillRect (r.translated (2.0f, 2.0f));
        g.setColour (sel ? theme::c().cardInk : theme::c().red.withMultipliedBrightness (0.8f + 0.2f * n.velocity));
        g.fillRect (r);
        g.setColour (sel ? theme::c().red : theme::c().ink);
        g.drawRect (r, sel ? 2.0f : 1.4f);

        // Borde derecho: se puede tomar para alargar.
        if (sel && r.getWidth() > 14.0f)
        {
            g.setColour (theme::c().card1.withAlpha (0.8f));
            g.fillRect (r.getRight() - 4.0f, r.getY() + 2.0f, 1.5f, r.getHeight() - 4.0f);
        }

        if (r.getWidth() > 30.0f && L.rowH > 8.5f)
        {
            // FL Studio llama C5 al Do central (MIDI 60).
            g.setFont (theme::mono (juce::jmin (11.0f, L.rowH - 1.0f), true));
            g.setColour (sel ? theme::c().card1 : theme::c().onAccent);
            g.drawText (juce::MidiMessage::getMidiNoteName (n.midiNote, true, true, 5),
                        r.reduced (5.0f, 0.0f), juce::Justification::centredLeft, false);
        }
    }

    // Ayuda corta, entre los botones y el asa.
    g.setColour (theme::c().cardInkSoft);
    g.setFont (theme::typed (13.0f));
    auto help = getLocalBounds().reduced (14, 8).removeFromBottom (38).withTrimmedLeft (348).withTrimmedRight (272);
    g.drawFittedText (theme::ui ("doble clic agrega · clic derecho borra"), help, juce::Justification::centred, 2, 0.85f);
}

// --- Edicion ---------------------------------------------------------------------------

double NotePreview::editGrid (const juce::ModifierKeys& mods) const
{
    if (mods.isShiftDown())
        return 0.0;                                      // Shift: sin ajuste al grid
    return clip.gridBeats > 0.0 ? clip.gridBeats : 0.125; // grid apagado: 1/32 fino
}

void NotePreview::mouseMove (const juce::MouseEvent& e)
{
    const auto& L = liveLayout;
    for (int i = (int) clip.notes.size() - 1; i >= 0; --i)
    {
        const auto r = L.rectOf (clip.notes[(std::size_t) i]);
        if (r.contains (e.position))
        {
            setMouseCursor (e.position.x > r.getRight() - 7.0f && r.getWidth() > 14.0f
                                ? juce::MouseCursor::LeftRightResizeCursor
                                : juce::MouseCursor::DraggingHandCursor);
            return;
        }
    }
    setMouseCursor (juce::MouseCursor::NormalCursor);
}

void NotePreview::mouseDown (const juce::MouseEvent& e)
{
    grabKeyboardFocus();
    if (clip.notes.empty())
        return;

    frozenLayout = liveLayout;
    int hit = -1;
    juce::Rectangle<float> hitRect;
    for (int i = (int) clip.notes.size() - 1; i >= 0; --i)
    {
        const auto r = liveLayout.rectOf (clip.notes[(std::size_t) i]);
        if (r.contains (e.position)) { hit = i; hitRect = r; break; }
    }

    selected = hit;
    if (hit >= 0 && e.mods.isPopupMenu())
    {
        deleteSelected();
        return;
    }

    if (hit >= 0)
    {
        beforeGesture  = clip.notes;
        dragOrigin     = clip.notes[(std::size_t) hit];
        gestureChanged = false;
        gesture = (e.position.x > hitRect.getRight() - 7.0f && hitRect.getWidth() > 14.0f) ? Gesture::resize : Gesture::move;
    }
    repaint();
}

void NotePreview::mouseDrag (const juce::MouseEvent& e)
{
    if (gesture == Gesture::none || selected < 0)
        return;

    const auto& L = frozenLayout;
    const double dBeat = (double) e.getDistanceFromDragStartX() / L.area.getWidth() * L.beats;
    const int    dSemi = juce::roundToInt (-(float) e.getDistanceFromDragStartY() / L.rowH);
    const double grid  = editGrid (e.mods);

    const auto updated = gesture == Gesture::move
                       ? fourever::movedNote (dragOrigin, dBeat, dSemi, grid)
                       : fourever::resizedNote (dragOrigin, dragOrigin.startBeat + dragOrigin.lengthBeat + dBeat, grid);

    auto& current = clip.notes[(std::size_t) selected];
    if (! juce::exactlyEqual (updated.startBeat, current.startBeat) || ! juce::exactlyEqual (updated.lengthBeat, current.lengthBeat)
        || updated.midiNote != current.midiNote)
    {
        current = updated;
        gestureChanged = true;
        repaint();
    }
}

void NotePreview::mouseUp (const juce::MouseEvent&)
{
    if (gesture == Gesture::none)
        return;

    gesture = Gesture::none;
    if (gestureChanged && selected >= 0)
    {
        // Reordenar por tiempo sin perder la nota elegida.
        const auto moved = clip.notes[(std::size_t) selected];
        auto notes = clip.notes;
        notes.erase (notes.begin() + selected);
        selected = fourever::insertNote (notes, moved);
        hasPendingClip = false; // la edicion manda sobre cualquier recalculo pendiente
        commit (std::move (notes), beforeGesture);
    }
    else if (hasPendingClip)
    {
        hasPendingClip = false;
        setClip (std::move (pendingClip), pendingBeatsPerBar);
    }
    liveLayout = computeLayout();
    repaint();
}

void NotePreview::mouseDoubleClick (const juce::MouseEvent& e)
{
    if (clip.notes.empty() || e.mods.isPopupMenu())
        return;

    const auto& L = liveLayout;
    if (! L.area.contains (e.position))
        return;
    for (const auto& n : clip.notes)
        if (L.rectOf (n).contains (e.position))
            return; // doble clic sobre una nota: nada

    // Nota nueva en la celda del grid bajo el mouse, ajustada a la escala del clip.
    const double grid = editGrid (e.mods);
    const double cell = grid > 0.0 ? grid : 0.125;
    fourever::ClipNote n;
    n.startBeat  = juce::jmax (0.0, std::floor (L.beatAt (e.position.x) / cell) * cell);
    n.lengthBeat = std::ceil (0.5 / cell - 1.0e-9) * cell;
    n.midiNote   = fourever::snapToScale ((float) L.noteAt (e.position.y), clip.key.tonic, clip.scaleUsed);
    n.velocity   = 0.8f;

    const auto before = clip.notes;
    auto notes = clip.notes;
    selected = fourever::insertNote (notes, n);
    commit (std::move (notes), before);
}

bool NotePreview::keyPressed (const juce::KeyPress& key)
{
    const auto mods = key.getModifiers();
    const int code  = key.getKeyCode();

    if (code == juce::KeyPress::deleteKey || code == juce::KeyPress::backspaceKey)
    {
        deleteSelected();
        return true;
    }
    if (mods.isCommandDown() && (code == 'Z' || code == 'z'))
    {
        mods.isShiftDown() ? redo() : undo();
        return true;
    }
    if (mods.isCommandDown() && (code == 'Y' || code == 'y'))
    {
        redo();
        return true;
    }

    // Flechas: mover la nota elegida (arriba/abajo un semitono, Shift = octava;
    // izquierda/derecha una celda del grid).
    if (selected >= 0 && selected < (int) clip.notes.size())
    {
        int dSemi = 0;
        double dBeat = 0.0;
        const double cell = clip.gridBeats > 0.0 ? clip.gridBeats : 0.125;
        if (code == juce::KeyPress::upKey)    dSemi = mods.isShiftDown() ? 12 : 1;
        if (code == juce::KeyPress::downKey)  dSemi = mods.isShiftDown() ? -12 : -1;
        if (code == juce::KeyPress::leftKey)  dBeat = -cell;
        if (code == juce::KeyPress::rightKey) dBeat = cell;

        if (dSemi != 0 || ! juce::exactlyEqual (dBeat, 0.0))
        {
            const auto before = clip.notes;
            auto notes = clip.notes;
            const auto moved = fourever::movedNote (notes[(std::size_t) selected], dBeat, dSemi, 0.0);
            notes.erase (notes.begin() + selected);
            selected = fourever::insertNote (notes, moved);
            commit (std::move (notes), before);
            return true;
        }
    }
    return false;
}

void NotePreview::commit (std::vector<fourever::ClipNote> newNotes, const std::vector<fourever::ClipNote>& before)
{
    history.push (before);
    clip.notes  = std::move (newNotes);
    clip.edited = true;
    liveLayout  = computeLayout();
    updateButtons();
    repaint();
    if (onNotesEdited)
        onNotesEdited (clip.notes);
}

void NotePreview::deleteSelected()
{
    if (selected < 0 || selected >= (int) clip.notes.size())
        return;
    const auto before = clip.notes;
    auto notes = clip.notes;
    notes.erase (notes.begin() + selected);
    selected = -1;
    commit (std::move (notes), before);
}

void NotePreview::undo()
{
    if (! history.canUndo())
        return;
    clip.notes = history.undo (clip.notes);
    selected   = -1;
    liveLayout = computeLayout();
    updateButtons();
    repaint();
    if (onNotesEdited)
        onNotesEdited (clip.notes);
}

void NotePreview::redo()
{
    if (! history.canRedo())
        return;
    clip.notes = history.redo (clip.notes);
    selected   = -1;
    liveLayout = computeLayout();
    updateButtons();
    repaint();
    if (onNotesEdited)
        onNotesEdited (clip.notes);
}

void NotePreview::startExternalDrag()
{
    if (clip.notes.empty() || ! createMidiFileForDrag)
        return;

    const auto file = createMidiFileForDrag();
    if (! file.existsAsFile())
        return;

    // Arrastre nativo del sistema operativo: FL Studio, Ableton, Reaper,
    // el explorador de archivos... cualquiera que acepte un .mid.
    juce::DragAndDropContainer::performExternalDragDropOfFiles ({ file.getFullPathName() }, false, dragHandle.get());
}
