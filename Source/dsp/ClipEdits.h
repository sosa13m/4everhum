// 4EVERHUM - tararea -> MIDI
// Copyright (C) 2026 sosa13m / colectivo 4EVER
// SPDX-License-Identifier: AGPL-3.0-or-later
// Este programa es software libre: ver el archivo LICENSE.

#pragma once

#include "dsp/Music.h"

#include <vector>

// Edicion manual del clip (Sprint 4): las operaciones del mini piano roll, sin
// UI, para poder testearlas.
namespace fourever
{

// Lleva un tiempo (en negras) a la celda mas cercana del grid. grid <= 0: sin ajuste.
double snapBeat (double beat, double grid);

// Nota movida dBeat negras y dSemis semitonos; el inicio queda en el grid.
ClipNote movedNote (const ClipNote& original, double dBeat, int dSemis, double grid);

// Nota con su final llevado a newEndBeat (en el grid). Nunca mas corta que una celda.
ClipNote resizedNote (const ClipNote& original, double newEndBeat, double grid);

// Inserta manteniendo el orden por inicio. Devuelve el indice donde quedo.
int insertNote (std::vector<ClipNote>& notes, const ClipNote& note);

void sortNotes (std::vector<ClipNote>& notes);

// Indice de la nota que contiene (beat, midi), o -1.
int noteAt (const std::vector<ClipNote>& notes, double beat, int midi);

// Historial de deshacer/rehacer: guarda estados completos (los clips son chicos).
class EditHistory
{
public:
    void clear();
    void push (const std::vector<ClipNote>& stateBeforeChange);   // antes de cada cambio
    bool canUndo() const { return ! undoStack.empty(); }
    bool canRedo() const { return ! redoStack.empty(); }
    std::vector<ClipNote> undo (const std::vector<ClipNote>& current);
    std::vector<ClipNote> redo (const std::vector<ClipNote>& current);

private:
    std::vector<std::vector<ClipNote>> undoStack, redoStack;
    static constexpr std::size_t maxSteps = 100;
};

} // namespace fourever
