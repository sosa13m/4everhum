// 4EVERHUM - tararea -> MIDI
// Copyright (C) 2026 sosa13m / colectivo 4EVER
// SPDX-License-Identifier: AGPL-3.0-or-later
// Este programa es software libre: ver el archivo LICENSE.

// Tests del motor musical: tonalidad, Camelot, ajuste a escala, grid y clip.

#include "dsp/Music.h"

#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

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

// Melodia simple: cada nota dura `dur` segundos, sin silencios, altura exacta.
std::vector<NoteEvent> melody (const std::vector<int>& midi, double dur = 0.4, double gap = 0.05)
{
    std::vector<NoteEvent> v;
    double t = 0.0;
    for (int m : midi)
    {
        NoteEvent n;
        n.startSec = t;
        n.endSec   = t + dur;
        n.midiNote = m;
        n.pitch    = (float) m;
        v.push_back (n);
        t += dur + gap;
    }
    return v;
}

std::vector<int> transpose (std::vector<int> v, int semis)
{
    for (auto& m : v) m += semis;
    return v;
}

bool near (double a, double b, double tol = 1.0e-6) { return std::abs (a - b) < tol; }

void testKeyDetection()
{
    std::printf ("\nDeteccion de tonalidad\n");

    struct Case { const char* name; std::vector<int> notes; std::string expected; };
    const std::vector<Case> cases {
        { "La menor (A C E D C B A)",         { 57, 60, 64, 62, 60, 59, 57 },             "Am" },
        { "Do mayor (C E G A G F E D C)",     { 60, 64, 67, 69, 67, 65, 64, 62, 60 },     "C"  },
        { "Fa# menor (F# A C# B A G# F#)",    { 66, 69, 73, 71, 69, 68, 66 },             "F#m" },
        { "Mib mayor (Eb G Bb C Bb Ab G F Eb)", { 63, 67, 70, 72, 70, 68, 67, 65, 63 },  "Eb" },
        { "Do menor estilo trap (C Eb G Ab G Eb D C)", { 60, 63, 67, 68, 67, 63, 62, 60 }, "Cm" },
    };
    for (const auto& c : cases)
    {
        const auto est = detectKey (melody (c.notes));
        check (est.valid && keyName (est.key) == c.expected,
               std::string (c.name) + " -> " + keyName (est.key) + " (esperado " + c.expected + ")");
    }

    // La misma melodia en las 12 tonalidades: la tonica tiene que seguirla.
    const std::vector<int> aMinor { 57, 60, 64, 62, 60, 59, 57 };
    bool allRotations = true;
    for (int t = 0; t < 12; ++t)
    {
        const auto est = detectKey (melody (transpose (aMinor, t)));
        allRotations &= est.key.minor && est.key.tonic == (9 + t) % 12;
    }
    check (allRotations, "la melodia en La menor transpuesta a las 12 tonalidades se detecta siempre bien");

    check (! detectKey (melody ({ 60, 62, 60, 62 })).valid, "con solo 2 notas distintas no inventa tonalidad");
    check (! detectKey ({}).valid, "sin notas: tonalidad invalida");
}

void testCamelot()
{
    std::printf ("\nRueda de Camelot (igual que el detector de 4EVERTOOLS)\n");
    struct C { Key k; const char* code; };
    const C cases[] { { { 0, false }, "8B" }, { { 9, true }, "8A" }, { { 7, false }, "9B" }, { { 5, false }, "7B" },
                      { { 11, false }, "1B" }, { { 6, true }, "11A" }, { { 3, false }, "5B" }, { { 7, true }, "6A" },
                      { { 1, true }, "12A" }, { { 1, false }, "3B" } };
    bool ok = true;
    for (const auto& c : cases)
    {
        const bool good = camelot (c.k) == c.code;
        if (! good)
            std::printf ("    %s da %s, esperado %s\n", keyName (c.k).c_str(), camelot (c.k).c_str(), c.code);
        ok &= good;
    }
    check (ok, "C=8B, Am=8A, G=9B, F=7B, B=1B, F#m=11A, Eb=5B, Gm=6A, C#m=12A, Db=3B");
    check (keyNameSpanish ({ 9, true }) == "La menor" && keyNameSpanish ({ 6, false }) == "Fa# mayor",
           "nombres en espanol: La menor, Fa# mayor");
}

void testSnap()
{
    std::printf ("\nAjuste a escala (usa la altura real cantada)\n");
    check (snapToScale (61.4f, 0, Scale::major) == 62, "61.4 en Do mayor -> Re (62): estaba mas cerca de Re");
    check (snapToScale (60.7f, 0, Scale::major) == 60, "60.7 en Do mayor -> Do (60)");
    check (snapToScale (64.3f, 0, Scale::major) == 64, "una nota que ya esta en la escala no se mueve");
    check (snapToScale (58.6f, 9, Scale::pentatonicMinor) == 60, "58.6 en La pentatonica menor -> Do (60)");
    check (snapToScale (63.0f, 9, Scale::minor) == 62 || snapToScale (63.0f, 9, Scale::minor) == 64,
           "Mib exacto en La menor cae en un vecino de la escala");
    check (snapToScale (61.2f, 0, Scale::none) == 61, "sin escala: redondeo simple");
    check (isInScale (68, 0, Scale::harmonicMinor) && isInScale (71, 0, Scale::harmonicMinor) && ! isInScale (70, 0, Scale::harmonicMinor),
           "Do menor armonica: Lab y Si si, Sib no");
}

void testQuantize()
{
    std::printf ("\nCuantizacion (120 BPM, la negra dura 0.5 s)\n");

    // Corcheas tarareadas con imprecision humana: deberian caer en 0, 0.5, 1, 1.5 negras.
    std::vector<NoteEvent> notes;
    const double starts[] { 1.000, 1.262, 1.489, 1.771 };
    for (int i = 0; i < 4; ++i)
    {
        NoteEvent n;
        n.startSec = starts[i];
        n.endSec   = starts[i] + 0.2;
        n.midiNote = 60 + 2 * i;
        n.pitch    = (float) n.midiNote;
        notes.push_back (n);
    }

    ClipSettings s;
    s.bpm   = 120.0;
    s.scale = Scale::none;
    s.grid  = Grid::eighth;

    auto clip = makeClip (notes, s);
    bool grid = clip.notes.size() == 4;
    for (std::size_t i = 0; grid && i < 4; ++i)
        grid &= near (clip.notes[i].startBeat, 0.5 * (double) i);
    check (grid, "1/8 al 100%: corcheas desparejas quedan en 0, 0.5, 1 y 1.5");

    s.strength = 0.5f;
    clip = makeClip (notes, s);
    check (near (clip.notes[1].startBeat, (0.524 + 0.5) / 2.0, 1.0e-3), "fuerza 50%: la nota queda a medio camino del grid");

    s.strength = 0.0f;
    clip = makeClip (notes, s);
    check (near (clip.notes[1].startBeat, 0.524, 1.0e-3) && near (clip.notes[1].lengthBeat, 0.4, 1.0e-3),
           "fuerza 0%: nada se mueve");

    s.strength = 1.0f;
    s.grid     = Grid::off;
    clip = makeClip (notes, s);
    check (near (clip.notes[3].startBeat, 1.542, 1.0e-3), "grid apagado: tiempos tal cual");

    // Tresillos: 3 notas por negra.
    std::vector<NoteEvent> trip;
    for (int i = 0; i < 6; ++i)
    {
        NoteEvent n;
        n.startSec = i * (0.5 / 3.0) + (i % 2 ? 0.015 : -0.012) + 0.1;
        n.endSec   = n.startSec + 0.12;
        n.midiNote = 60;
        n.pitch    = 60.0f;
        trip.push_back (n);
    }
    s.grid = Grid::eighthTriplet;
    clip = makeClip (trip, s);
    bool triplets = clip.notes.size() == 6;
    for (std::size_t i = 0; triplets && i < 6; ++i)
        triplets &= near (clip.notes[i].startBeat, (double) i / 3.0, 1.0e-6);
    check (triplets, "1/8T: seis notas caen exacto en tresillos de corchea");

    // Dos notas que caen en la misma celda: queda la mas larga.
    std::vector<NoteEvent> clash = melody ({ 60, 62, 64 }, 0.3, 0.2);
    clash[1].startSec = clash[0].startSec + 0.04; // un "golpe" pegado a la primera
    clash[1].endSec   = clash[1].startSec + 0.05;
    s.grid = Grid::eighth;
    clip = makeClip (clash, s);
    check (clip.notes.size() == 2 && clip.notes[0].midiNote == 60, "dos notas en la misma celda: queda la mas larga");

    bool noOverlap = true;
    for (std::size_t i = 0; i + 1 < clip.notes.size(); ++i)
        noOverlap &= clip.notes[i].startBeat + clip.notes[i].lengthBeat <= clip.notes[i + 1].startBeat + 1.0e-9;
    check (noOverlap, "ninguna nota pisa a la siguiente");
}

void testAlignment()
{
    std::printf ("\nAlineado al compas del proyecto\n");

    // Se empezo a grabar con FL en la negra 6.5 (compas 2, tiempo 3 y medio).
    // La primera nota entra 0.25 s despues (= media negra a 120) -> negra 7.0.
    std::vector<NoteEvent> notes = melody ({ 57, 60, 64 }, 0.4, 0.1);
    for (auto& n : notes) { n.startSec += 0.25; n.endSec += 0.25; }

    ClipSettings s;
    s.bpm            = 120.0;
    s.hostPpqAtStart = 6.5;
    s.grid           = Grid::sixteenth;

    auto clip = makeClip (notes, s);
    check (clip.alignedToProject, "con posicion del proyecto, el clip queda alineado");
    check (near (clip.originBeat, 4.0), "el clip empieza en el compas 2 (negra 4)");
    check (near (clip.notes[0].startBeat, 3.0), "la primera nota queda en el tiempo 4 del compas, donde se canto");

    s.hostPpqAtStart = -1.0;
    clip = makeClip (notes, s);
    check (! clip.alignedToProject && near (clip.notes[0].startBeat, 0.0),
           "sin posicion del proyecto: la primera nota cae en el tiempo 1");
}

void testKeyChoices()
{
    std::printf ("\nTonalidad y escala elegidas a mano\n");

    // Melodia en La menor cantada con una nota desafinada (Sol# medio bajo: 67.6).
    auto notes = melody ({ 57, 60, 64, 62, 60, 68, 57 });
    notes[5].pitch = 67.6f;

    ClipSettings s;
    s.grid = Grid::off;

    auto clip = makeClip (notes, s);
    check (keyName (clip.key) == "Am" && clip.scaleUsed == Scale::minor, "auto: La menor, escala menor");
    check (clip.notes[5].midiNote == 67, "en La menor natural, el 67.6 cae en Sol (67)");

    s.scale = Scale::harmonicMinor;
    clip = makeClip (notes, s);
    check (clip.notes[5].midiNote == 68, "en La menor armonica, el mismo 67.6 cae en Sol# (68)");

    s.scale   = Scale::autoDetect;
    s.keyRoot = 9;
    clip = makeClip (notes, s);
    check (clip.key.tonic == 9 && clip.key.minor, "tonica La elegida a mano: elige el modo menor porque calza mejor");

    s.keyRoot = -1;
    s.scale   = Scale::none;
    clip = makeClip (notes, s);
    check (clip.notes[5].midiNote == 68, "sin escala: el 67.6 se redondea a 68");

    s.scale   = Scale::autoDetect;
    s.octaves = 1;
    clip = makeClip (notes, s);
    check (clip.notes[0].midiNote == 69, "octava +1 se aplica despues de la escala");

    // Muy pocas notas para adivinar: no se fuerza escala.
    auto two = melody ({ 60, 62, 60, 62 });
    two[1].pitch = 61.6f;
    s.octaves = 0;
    clip = makeClip (two, s);
    check (clip.scaleUsed == Scale::none && clip.notes[1].midiNote == 62, "2 notas distintas: sin escala forzada");
}
} // namespace

int main()
{
    testKeyDetection();
    testCamelot();
    testSnap();
    testQuantize();
    testAlignment();
    testKeyChoices();

    std::printf ("\n%s (%d fallos)\n", failures == 0 ? "TODO OK" : "HAY FALLOS", failures);
    return failures == 0 ? 0 : 1;
}
