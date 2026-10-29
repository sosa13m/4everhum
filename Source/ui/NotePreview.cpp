#include "ui/NotePreview.h"
#include "ui/Theme.h"

NotePreview::NotePreview()
{
    setRepaintsOnMouseActivity (true);
}

void NotePreview::setClip (fourever::Clip newClip, int newBeatsPerBar)
{
    clip        = std::move (newClip);
    beatsPerBar = juce::jmax (1, newBeatsPerBar);
    setMouseCursor (clip.notes.empty() ? juce::MouseCursor::NormalCursor
                                       : juce::MouseCursor::DraggingHandCursor);
    repaint();
}

// Se dibuja sobre la hoja fotocopiada del editor (fondo transparente).
void NotePreview::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat().reduced (1.0f);
    const bool hot    = hasNotes() && isMouseOverOrDragging();

    // Recuadro: punteado en reposo, rojo solido cuando lo vas a arrastrar.
    if (hot)
    {
        g.setColour (theme::red.withAlpha (0.06f));
        g.fillRect (bounds);
        g.setColour (theme::red);
        g.drawRect (bounds, 2.0f);
    }
    else
    {
        juce::Path frame;
        frame.addRectangle (bounds);
        juce::Path dashed;
        const float dashes[] = { 7.0f, 5.0f };
        juce::PathStrokeType (1.6f).createDashedStroke (dashed, frame, dashes, 2);
        g.setColour (theme::ink.withAlpha (0.55f));
        g.fillPath (dashed);
    }

    if (clip.notes.empty())
    {
        auto area = getLocalBounds().reduced (20);
        auto block = area.withSizeKeepingCentre (area.getWidth(), 86);
        g.setColour (theme::ink);
        g.setFont (theme::anton (44.0f));
        g.drawText (theme::ui ("TARAREA ALGO."), block.removeFromTop (52), juce::Justification::centred);
        g.setColour (theme::inkSoft);
        g.setFont (theme::typed (16.0f));
        g.drawText (theme::ui ("Pulsa GRABAR, canta la melodía y pulsa DETENER."),
                    block, juce::Justification::centredTop);
        return;
    }

    auto area = bounds.reduced (14.0f, 12.0f);
    area.removeFromBottom (20.0f); // espacio para la indicacion de arrastre

    // Rango visible: el clip completo (lo mismo que se arrastra), en compases enteros.
    const auto& notes = clip.notes;
    int lo = 127, hi = 0;
    for (const auto& n : notes)
    {
        lo = juce::jmin (lo, n.midiNote);
        hi = juce::jmax (hi, n.midiNote);
    }
    const double bar = (double) beatsPerBar;
    const double t0  = 0.0;
    const double t1  = juce::jmax (bar, std::ceil (clip.lengthBeats() / bar - 1.0e-9) * bar);
    lo -= 2; hi += 2;
    if (hi - lo < 12) { const int pad = (12 - (hi - lo)) / 2; lo -= pad; hi = lo + 12; }

    const float rowH = area.getHeight() / (float) (hi - lo + 1);
    auto xOf = [&] (double beat) { return area.getX() + (float) ((beat - t0) / (t1 - t0)) * area.getWidth(); };
    auto yOf = [&] (int note)    { return area.getBottom() - (float) (note - lo + 1) * rowH; };

    // Filas de teclas negras, como en un piano roll (un tono de papel mas oscuro).
    for (int nt = lo; nt <= hi; ++nt)
    {
        const int pc = ((nt % 12) + 12) % 12;
        if (pc == 1 || pc == 3 || pc == 6 || pc == 8 || pc == 10)
        {
            g.setColour (theme::ink.withAlpha (0.06f));
            g.fillRect (area.getX(), yOf (nt), area.getWidth(), rowH);
        }
    }

    // Grid: negras y compases. Si el clip esta alineado al proyecto, los numeros
    // de compas son los del proyecto de FL.
    const int firstBar = clip.alignedToProject ? juce::roundToInt (clip.originBeat / bar) + 1 : 1;
    g.setFont (theme::mono (10.0f));
    for (int beat = 0; beat <= juce::roundToInt (t1); ++beat)
    {
        const float x = xOf ((double) beat);
        const bool isBar = beat % beatsPerBar == 0;
        g.setColour (theme::ink.withAlpha (isBar ? 0.38f : 0.13f));
        g.drawVerticalLine (juce::roundToInt (x), area.getY(), area.getBottom());
        if (isBar && beat < juce::roundToInt (t1))
        {
            g.setColour (theme::inkSoft);
            g.drawText (juce::String (firstBar + beat / beatsPerBar), juce::Rectangle<float> (x + 3.0f, area.getY() - 1.0f, 30.0f, 12.0f),
                        juce::Justification::topLeft);
        }
    }

    // Notas: bloques rojos de tinta con sombra dura.
    g.setFont (theme::mono (11.0f, true));
    for (const auto& n : notes)
    {
        juce::Rectangle<float> r (xOf (n.startBeat), yOf (n.midiNote),
                                  juce::jmax (4.0f, xOf (n.startBeat + n.lengthBeat) - xOf (n.startBeat)), rowH);
        r = r.reduced (0.0f, juce::jmin (1.0f, rowH * 0.1f));

        g.setColour (theme::ink.withAlpha (0.3f));
        g.fillRect (r.translated (2.0f, 2.0f));
        g.setColour (theme::red.withMultipliedBrightness (0.8f + 0.2f * n.velocity));
        g.fillRect (r);
        g.setColour (theme::ink);
        g.drawRect (r, 1.4f);

        if (r.getWidth() > 30.0f && rowH > 8.5f)
        {
            g.setFont (theme::mono (juce::jmin (11.0f, rowH - 1.0f), true));
            // FL Studio llama C5 al Do central (MIDI 60).
            g.setColour (juce::Colours::white);
            g.drawText (juce::MidiMessage::getMidiNoteName (n.midiNote, true, true, 5),
                        r.reduced (5.0f, 0.0f), juce::Justification::centredLeft, false);
        }
    }

    g.setColour (hot ? theme::redDark : theme::inkSoft);
    g.setFont (theme::typed (15.0f));
    g.drawText (theme::ui ("arrastra la melodía al piano roll  →"),
                getLocalBounds().reduced (14, 8), juce::Justification::bottomRight);
}

void NotePreview::mouseDrag (const juce::MouseEvent& e)
{
    if (dragStarted || clip.notes.empty() || e.getDistanceFromDragStart() < 6 || ! createMidiFileForDrag)
        return;

    dragStarted = true;
    const auto file = createMidiFileForDrag();
    if (! file.existsAsFile())
        return;

    // Arrastre nativo del sistema operativo: FL Studio, Ableton, Reaper,
    // el explorador de archivos... cualquiera que acepte un .mid.
    juce::DragAndDropContainer::performExternalDragDropOfFiles ({ file.getFullPathName() }, false, this);
}
