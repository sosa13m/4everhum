// 4EVERHUM - tararea -> MIDI
// Copyright (C) 2026 sosa13m / colectivo 4EVER
// SPDX-License-Identifier: AGPL-3.0-or-later
// Este programa es software libre: ver el archivo LICENSE.

#include "dsp/ClipEdits.h"

#include <algorithm>
#include <cmath>

namespace fourever
{

double snapBeat (double beat, double grid)
{
    return grid > 0.0 ? std::round (beat / grid) * grid : beat;
}

ClipNote movedNote (const ClipNote& original, double dBeat, int dSemis, double grid)
{
    ClipNote n = original;
    n.startBeat = std::max (0.0, snapBeat (original.startBeat + dBeat, grid));
    n.midiNote  = std::clamp (original.midiNote + dSemis, 0, 127);
    return n;
}

ClipNote resizedNote (const ClipNote& original, double newEndBeat, double grid)
{
    ClipNote n = original;
    const double minLength = grid > 0.0 ? grid : 1.0 / 32.0;
    const double end = snapBeat (newEndBeat, grid);
    n.lengthBeat = std::max (minLength, end - original.startBeat);
    return n;
}

void sortNotes (std::vector<ClipNote>& notes)
{
    std::stable_sort (notes.begin(), notes.end(),
                      [] (const ClipNote& a, const ClipNote& b) { return a.startBeat < b.startBeat; });
}

int insertNote (std::vector<ClipNote>& notes, const ClipNote& note)
{
    const auto it = std::upper_bound (notes.begin(), notes.end(), note,
                                      [] (const ClipNote& a, const ClipNote& b) { return a.startBeat < b.startBeat; });
    // El indice se calcula ANTES de insertar: insert() puede reubicar el vector e
    // invalidar cualquier iterador tomado antes (y el orden de evaluacion de los
    // argumentos de una funcion no esta definido en C++).
    const auto index = std::distance (notes.begin(), it);
    notes.insert (it, note);
    return (int) index;
}

int noteAt (const std::vector<ClipNote>& notes, double beat, int midi)
{
    // Si hay notas superpuestas gana la de mas arriba en la lista (la ultima dibujada).
    for (int i = (int) notes.size() - 1; i >= 0; --i)
    {
        const auto& n = notes[(std::size_t) i];
        if (n.midiNote == midi && beat >= n.startBeat && beat < n.startBeat + n.lengthBeat)
            return i;
    }
    return -1;
}

void EditHistory::clear()
{
    undoStack.clear();
    redoStack.clear();
}

void EditHistory::push (const std::vector<ClipNote>& stateBeforeChange)
{
    undoStack.push_back (stateBeforeChange);
    if (undoStack.size() > maxSteps)
        undoStack.erase (undoStack.begin());
    redoStack.clear(); // un cambio nuevo invalida lo que se podia rehacer
}

std::vector<ClipNote> EditHistory::undo (const std::vector<ClipNote>& current)
{
    if (undoStack.empty())
        return current;
    redoStack.push_back (current);
    auto previous = std::move (undoStack.back());
    undoStack.pop_back();
    return previous;
}

std::vector<ClipNote> EditHistory::redo (const std::vector<ClipNote>& current)
{
    if (redoStack.empty())
        return current;
    undoStack.push_back (current);
    auto next = std::move (redoStack.back());
    redoStack.pop_back();
    return next;
}

} // namespace fourever
