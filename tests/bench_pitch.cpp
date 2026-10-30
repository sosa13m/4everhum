// 4EVERHUM - tararea -> MIDI
// Copyright (C) 2026 sosa13m / colectivo 4EVER
// SPDX-License-Identifier: AGPL-3.0-or-later
// Este programa es software libre: ver el archivo LICENSE.

// Banco de pruebas YIN vs pYIN con voces sinteticas dificiles y altura real conocida.
//   bench_pitch            -> imprime la tabla de comparacion
// Tambien sirve de test: falla si pYIN no mejora a YIN en los casos dificiles.

#include "dsp/Analyzer.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <random>
#include <string>
#include <vector>

using namespace fourever;

namespace
{
constexpr double pi = 3.14159265358979323846;
constexpr double sr = 44100.0;

struct Seg { double midi; double dur; double gapAfter; };

struct Voice
{
    std::vector<double> harmonics { 1.0, 0.5, 0.25 }; // amplitud de cada armonico
    double noise      = 0.003;  // ruido blanco (desv. estandar)
    double fry        = 0.0;    // 0..1: un periodo de cada dos mas debil (subarmonico)
    double vibrato    = 0.25;   // semitonos
    double jitter     = 0.0;    // variacion aleatoria del periodo (fraccion)
};

struct Take
{
    std::vector<float> audio;
    std::vector<double> truthMidi; // por muestra; <0 = silencio
    std::vector<Seg> segs;
};

Take synth (const std::vector<Seg>& segs, const Voice& v, unsigned seed)
{
    Take tk;
    tk.segs = segs;
    std::mt19937 rng (seed);
    std::normal_distribution<double> n01 (0.0, 1.0);
    auto silence = [&] (double sec)
    {
        for (int i = 0; i < (int) (sec * sr); ++i)
        {
            tk.audio.push_back ((float) (v.noise * n01 (rng)));
            tk.truthMidi.push_back (-1.0);
        }
    };
    silence (0.3);
    double phase = 0.0;
    for (const auto& s : segs)
    {
        const int len = (int) (s.dur * sr);
        double jit = 0.0;
        for (int i = 0; i < len; ++i)
        {
            const double t = i / sr;
            const double m = s.midi + v.vibrato * std::sin (2 * pi * 5.5 * t);
            if (i % 200 == 0) jit = v.jitter * n01 (rng);
            const double f = 440.0 * std::pow (2.0, (m - 69.0) / 12.0) * (1.0 + jit);
            phase += 2 * pi * f / sr;
            double x = 0.0;
            for (std::size_t h = 0; h < v.harmonics.size(); ++h)
                x += v.harmonics[h] * std::sin ((double) (h + 1) * phase);
            // Fry: cada segundo periodo atenuado -> aparece un subarmonico en f0/2.
            if (v.fry > 0.0 && std::fmod (phase, 4 * pi) > 2 * pi)
                x *= (1.0 - v.fry);
            const double env = std::min ({ 1.0, t / 0.02, (s.dur - t) / 0.03 });
            tk.audio.push_back ((float) (0.22 * env * x + v.noise * n01 (rng)));
            tk.truthMidi.push_back (env > 0.3 ? m : -1.0);
        }
        silence (s.gapAfter);
    }
    silence (0.3);
    return tk;
}

struct Score
{
    double rawAccuracy = 0, octaveErrors = 0, voicingRecall = 0, falseAlarm = 0;
    int notesRight = 0, notesTotal = 0, notesFound = 0;
};

Score evaluate (const Take& tk, bool pyin)
{
    UserSettings u;
    u.highPrecision = pyin;
    const auto frames = trackPitch (tk.audio.data(), tk.audio.size(), sr, toPitchSettings (u));
    const auto notes  = segmentNotes (frames, toSegmenterSettings (u));

    Score sc;
    int voicedTruth = 0, unvoicedTruth = 0, hit = 0, octave = 0, recalled = 0, fa = 0;
    for (const auto& f : frames)
    {
        const auto idx = (std::size_t) std::clamp ((int) (f.timeSec * sr), 0, (int) tk.truthMidi.size() - 1);
        const double truth = tk.truthMidi[idx];
        const bool est = f.f0Hz > 0.0f && f.confidence >= 0.5f;
        if (truth < 0) { ++unvoicedTruth; if (est) ++fa; continue; }
        ++voicedTruth;
        if (! est) continue;
        ++recalled;
        const double m = 69.0 + 12.0 * std::log2 (f.f0Hz / 440.0);
        const double cents = std::abs (m - truth) * 100.0;
        if (cents < 50.0) ++hit;
        else if (std::abs (std::abs (m - truth) - 12.0) < 1.0) ++octave;
    }
    sc.rawAccuracy   = voicedTruth ? 100.0 * hit / voicedTruth : 0;
    sc.octaveErrors  = voicedTruth ? 100.0 * octave / voicedTruth : 0;
    sc.voicingRecall = voicedTruth ? 100.0 * recalled / voicedTruth : 0;
    sc.falseAlarm    = unvoicedTruth ? 100.0 * fa / unvoicedTruth : 0;

    sc.notesTotal = (int) tk.segs.size();
    sc.notesFound = (int) notes.size();
    // Nota correcta: hay una nota detectada que empieza a menos de 60 ms y tiene la altura correcta.
    double t = 0.3;
    for (const auto& s : tk.segs)
    {
        for (const auto& n : notes)
            if (std::abs (n.startSec - t) < 0.06 && n.midiNote == (int) std::lround (s.midi)) { ++sc.notesRight; break; }
        t += s.dur + s.gapAfter;
    }
    return sc;
}

std::vector<Seg> melody (double transpose = 0.0, double dur = 0.32, double gap = 0.08)
{
    const double base[] { 57, 60, 64, 62, 60, 59, 57, 52, 55, 57 };
    std::vector<Seg> v;
    for (double m : base) v.push_back ({ m + transpose, dur, gap });
    return v;
}
} // namespace

int main()
{
    struct Case { std::string name; Take take; bool hard; };
    std::vector<Case> cases;

    Voice clean;
    cases.push_back ({ "Voz limpia",                         synth (melody(), clean, 1), false });

    Voice weakF0; weakF0.harmonics = { 0.12, 1.0, 0.6, 0.35 };
    cases.push_back ({ "Fundamental debil (2o armonico fuerte)", synth (melody(), weakF0, 2), true });

    Voice breathy; breathy.noise = 0.06;
    cases.push_back ({ "Voz aireada (ruido fuerte)",          synth (melody(), breathy, 3), true });

    Voice fry; fry.fry = 0.45; fry.harmonics = { 1.0, 0.7, 0.4 };
    cases.push_back ({ "Voz rasposa (subarmonico)",           synth (melody (-12.0), fry, 4), true });

    Voice rough; rough.jitter = 0.012; rough.noise = 0.03; rough.vibrato = 0.45;
    cases.push_back ({ "Voz inestable (jitter + vibrato)",    synth (melody(), rough, 5), true });

    Voice whistle; whistle.harmonics = { 1.0 }; whistle.vibrato = 0.15;
    cases.push_back ({ "Silbido agudo",                       synth (melody (19.0), whistle, 6), false });

    std::printf ("\n%-42s %-6s %8s %8s %8s %8s %10s\n", "Caso", "Metodo", "Altura", "Octava", "Voz", "Falsos", "Notas");
    int failures = 0;
    double sumYin = 0, sumPyin = 0;
    for (const auto& c : cases)
    {
        const auto y = evaluate (c.take, false);
        const auto p = evaluate (c.take, true);
        for (const auto& [label, sc] : { std::pair<const char*, Score> { "YIN", y }, { "pYIN", p } })
            std::printf ("%-42s %-6s %7.1f%% %7.1f%% %7.1f%% %7.1f%% %5d/%-4d\n", label[0] == 'Y' ? c.name.c_str() : "",
                         label, sc.rawAccuracy, sc.octaveErrors, sc.voicingRecall, sc.falseAlarm, sc.notesRight, sc.notesTotal);
        sumYin += y.notesRight;
        sumPyin += p.notesRight;
        // pYIN no debe ser peor que YIN en ningun caso (en notas correctas).
        if (p.notesRight < y.notesRight) { std::printf ("  ^ pYIN peor que YIN aqui\n"); ++failures; }
    }
    std::printf ("\nNotas correctas en total: YIN %.0f, pYIN %.0f\n", sumYin, sumPyin);
    if (sumPyin <= sumYin) { std::printf ("pYIN no mejora a YIN en el total\n"); ++failures; }
    std::printf ("%s (%d fallos)\n", failures == 0 ? "TODO OK" : "HAY FALLOS", failures);
    return failures == 0 ? 0 : 1;
}
