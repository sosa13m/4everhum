// Tests de la edicion manual del clip (Sprint 4).

#include "dsp/ClipEdits.h"

#include <cmath>
#include <cstdio>
#include <string>

using namespace fourever;

namespace
{
int failures = 0;

void check (bool cond, const std::string& what)
{
    std::printf ("  [%s] %s\n", cond ? " OK " : "FAIL", what.c_str());
    if (! cond)
        ++failures;
}

bool near (double a, double b) { return std::abs (a - b) < 1.0e-9; }

ClipNote note (double start, double len, int midi)
{
    ClipNote n;
    n.startBeat = start;
    n.lengthBeat = len;
    n.midiNote = midi;
    return n;
}
} // namespace

int main()
{
    std::printf ("\nMover y alargar notas\n");
    const auto a = note (1.0, 0.5, 60);

    auto m = movedNote (a, 0.37, 2, 0.25);
    check (near (m.startBeat, 1.25) && m.midiNote == 62 && near (m.lengthBeat, 0.5),
           "mover +0.37 negras con grid 1/16: cae en 1.25; +2 semitonos; largo intacto");
    check (near (movedNote (a, -5.0, 0, 0.25).startBeat, 0.0), "no se puede mover antes del tiempo 0");
    check (movedNote (a, 0.0, 200, 0.25).midiNote == 127, "la altura queda dentro del rango MIDI");
    check (near (movedNote (a, 0.37, 0, 0.0).startBeat, 1.37), "sin grid: movimiento libre");

    check (near (resizedNote (a, 2.1, 0.25).lengthBeat, 1.0), "alargar hasta 2.1 con 1/16: termina en 2.0 (largo 1)");
    check (near (resizedNote (a, 0.2, 0.25).lengthBeat, 0.25), "nunca queda mas corta que una celda del grid");
    check (near (snapBeat (1.0 / 3.0 + 0.04, 1.0 / 3.0), 1.0 / 3.0), "snap a tresillos");

    std::printf ("\nAgregar, buscar y borrar\n");
    std::vector<ClipNote> notes { note (0.0, 0.5, 60), note (1.0, 0.5, 62), note (2.0, 0.5, 64) };
    const int idx = insertNote (notes, note (1.5, 0.5, 65));
    check (idx == 2 && notes.size() == 4 && notes[2].midiNote == 65, "una nota nueva se inserta en su lugar por tiempo");
    check (noteAt (notes, 1.2, 62) == 1, "clic dentro de una nota la encuentra");
    check (noteAt (notes, 1.2, 63) == -1 && noteAt (notes, 0.6, 60) == -1, "clic fuera de las notas no encuentra nada");
    notes.erase (notes.begin() + 1);
    check (notes.size() == 3 && notes[1].midiNote == 65, "borrar quita solo esa nota");

    std::printf ("\nDeshacer / rehacer\n");
    EditHistory h;
    std::vector<ClipNote> state { note (0.0, 1.0, 60) };
    h.push (state);
    state[0] = movedNote (state[0], 1.0, 0, 0.25);      // cambio 1
    h.push (state);
    state.push_back (note (3.0, 1.0, 67));               // cambio 2

    state = h.undo (state);
    check (state.size() == 1 && near (state[0].startBeat, 1.0), "deshacer quita la nota agregada");
    state = h.undo (state);
    check (state.size() == 1 && near (state[0].startBeat, 0.0), "deshacer otra vez devuelve la nota a su lugar");
    check (! h.canUndo(), "no hay mas que deshacer");
    state = h.redo (state);
    check (near (state[0].startBeat, 1.0), "rehacer vuelve a mover la nota");
    h.push (state);                                      // cambio nuevo
    check (! h.canRedo(), "un cambio nuevo borra lo que se podia rehacer");

    EditHistory big;
    std::vector<ClipNote> s { note (0.0, 1.0, 60) };
    for (int i = 0; i < 150; ++i)
        big.push (s);
    int undos = 0;
    while (big.canUndo()) { s = big.undo (s); ++undos; }
    check (undos == 100, "el historial guarda los ultimos 100 pasos");

    std::printf ("\n%s (%d fallos)\n", failures == 0 ? "TODO OK" : "HAY FALLOS", failures);
    return failures == 0 ? 0 : 1;
}
