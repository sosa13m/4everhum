// 4EVERHUM - tararea -> MIDI
// Copyright (C) 2026 sosa13m / colectivo 4EVER
// SPDX-License-Identifier: AGPL-3.0-or-later
// Este programa es software libre: ver el archivo LICENSE.

#include "dsp/Music.h"

#include <algorithm>
#include <cmath>
#include <numeric>

namespace fourever
{
namespace
{
// Perfiles de Krumhansl-Kessler (1982).
constexpr std::array<double, 12> majorProfile { 6.35, 2.23, 3.48, 2.33, 4.38, 4.09, 2.52, 5.19, 2.39, 3.66, 2.29, 2.88 };
constexpr std::array<double, 12> minorProfile { 6.33, 2.68, 3.52, 5.38, 2.60, 3.53, 2.54, 4.75, 3.98, 2.69, 3.34, 3.17 };

const char* const tonicNames[12]   { "C", "C#", "D", "Eb", "E", "F", "F#", "G", "Ab", "A", "Bb", "B" };
const char* const minorNames[12]   { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "Bb", "B" };
const char* const spanishNames[12] { "Do", "Do#", "Re", "Mib", "Mi", "Fa", "Fa#", "Sol", "Lab", "La", "Sib", "Si" };
const char* const spanishMinor[12] { "Do", "Do#", "Re", "Re#", "Mi", "Fa", "Fa#", "Sol", "Sol#", "La", "Sib", "Si" };

int pitchClass (int midi) { return ((midi % 12) + 12) % 12; }

double correlation (const std::array<double, 12>& a, const std::array<double, 12>& profile, int rotation)
{
    const double meanA = std::accumulate (a.begin(), a.end(), 0.0) / 12.0;
    const double meanP = std::accumulate (profile.begin(), profile.end(), 0.0) / 12.0;
    double num = 0.0, da = 0.0, dp = 0.0;
    for (int i = 0; i < 12; ++i)
    {
        const double x = a[(std::size_t) ((i + rotation) % 12)] - meanA;
        const double y = profile[(std::size_t) i] - meanP;
        num += x * y; da += x * x; dp += y * y;
    }
    return (da > 0.0 && dp > 0.0) ? num / std::sqrt (da * dp) : 0.0;
}

double correlationFor (const std::array<double, 12>& hist, Key k)
{
    return correlation (hist, k.minor ? minorProfile : majorProfile, k.tonic);
}

std::array<double, 12> histogram (const std::vector<NoteEvent>& notes)
{
    std::array<double, 12> h {};
    for (std::size_t i = 0; i < notes.size(); ++i)
    {
        const auto& n = notes[i];
        // Duracion con techo (una nota larguisima no decide sola) y un pequeno
        // extra para la ultima nota: las melodias suelen terminar en la tonica.
        double w = std::min (n.endSec - n.startSec, 1.0) + 0.1;
        if (i + 1 == notes.size())
            w *= 1.5;
        h[(std::size_t) pitchClass (n.midiNote)] += w;
    }
    return h;
}

Scale resolveScale (Scale requested, Key key)
{
    if (requested == Scale::autoDetect)
        return key.minor ? Scale::minor : Scale::major;
    return requested;
}
} // namespace

// --- Tonalidad -----------------------------------------------------------------

KeyEstimate detectKey (const std::vector<NoteEvent>& notes)
{
    KeyEstimate est;
    if (notes.empty())
        return est;

    const auto hist = histogram (notes);

    struct Scored { Key key; double r; };
    std::vector<Scored> all;
    for (int t = 0; t < 12; ++t)
        for (bool minor : { false, true })
            all.push_back ({ { t, minor }, correlationFor (hist, { t, minor }) });

    std::sort (all.begin(), all.end(), [] (const Scored& a, const Scored& b) { return a.r > b.r; });

    est.key        = all[0].key;
    est.score      = all[0].r;
    est.confidence = std::clamp ((all[0].r - all[1].r) * 4.0, 0.0, 1.0);
    for (std::size_t i = 0; i < est.alternatives.size(); ++i)
        est.alternatives[i] = all[i].key;

    // Con menos de 3 clases de altura distintas no hay de donde sacar una tonalidad.
    const auto distinct = std::count_if (hist.begin(), hist.end(), [] (double v) { return v > 0.0; });
    est.valid = distinct >= 3;
    return est;
}

std::string keyName (Key k)
{
    return k.minor ? std::string (minorNames[k.tonic]) + "m" : std::string (tonicNames[k.tonic]);
}

std::string keyNameSpanish (Key k)
{
    return k.minor ? std::string (spanishMinor[k.tonic]) + " menor"
                   : std::string (spanishNames[k.tonic]) + " mayor";
}

std::string camelot (Key k)
{
    // Rueda de Camelot: C mayor = 8B, La menor = 8A; cada quinta suma 1.
    const int relativeMajor = k.minor ? (k.tonic + 3) % 12 : k.tonic;
    const int number = ((relativeMajor * 7) % 12 + 7) % 12 + 1;
    return std::to_string (number) + (k.minor ? "A" : "B");
}

// --- Escalas -------------------------------------------------------------------

std::vector<int> scaleIntervals (Scale s)
{
    switch (s)
    {
        case Scale::major:           return { 0, 2, 4, 5, 7, 9, 11 };
        case Scale::minor:           return { 0, 2, 3, 5, 7, 8, 10 };
        case Scale::harmonicMinor:   return { 0, 2, 3, 5, 7, 8, 11 };
        case Scale::dorian:          return { 0, 2, 3, 5, 7, 9, 10 };
        case Scale::phrygian:        return { 0, 1, 3, 5, 7, 8, 10 };
        case Scale::pentatonicMajor: return { 0, 2, 4, 7, 9 };
        case Scale::pentatonicMinor: return { 0, 3, 5, 7, 10 };
        case Scale::autoDetect:
        case Scale::none:            break;
    }
    return {};
}

bool isInScale (int midiNote, int tonic, Scale s)
{
    const auto iv = scaleIntervals (s);
    if (iv.empty())
        return true;
    const int degree = pitchClass (midiNote - tonic);
    return std::find (iv.begin(), iv.end(), degree) != iv.end();
}

int snapToScale (float pitch, int tonic, Scale s)
{
    const int nearest = (int) std::lround (pitch);
    if (scaleIntervals (s).empty())
        return std::clamp (nearest, 0, 127);

    // La nota de la escala mas cercana a lo que de verdad cantaste: si cantaste
    // 61.4 en Do mayor, gana Re (62, a 0.6) y no Do (60, a 1.4).
    int best = nearest;
    float bestDist = 1.0e9f;
    for (int m = nearest - 3; m <= nearest + 3; ++m)
    {
        if (m < 0 || m > 127 || ! isInScale (m, tonic, s))
            continue;
        const float d = std::abs ((float) m - pitch);
        if (d < bestDist - 1.0e-4f)
        {
            best = m;
            bestDist = d;
        }
    }
    return best;
}

// --- Grid ----------------------------------------------------------------------

double gridBeats (Grid g)
{
    switch (g)
    {
        case Grid::quarter:          return 1.0;
        case Grid::eighth:           return 0.5;
        case Grid::sixteenth:        return 0.25;
        case Grid::thirtySecond:     return 0.125;
        case Grid::eighthTriplet:    return 1.0 / 3.0;
        case Grid::sixteenthTriplet: return 1.0 / 6.0;
        case Grid::off:              break;
    }
    return 0.0;
}

// --- Clip ----------------------------------------------------------------------

double Clip::lengthBeats() const
{
    double end = 0.0;
    for (const auto& n : notes)
        end = std::max (end, n.startBeat + n.lengthBeat);
    return end;
}

Clip makeClip (const std::vector<NoteEvent>& notes, const ClipSettings& s)
{
    Clip clip;
    clip.gridBeats = gridBeats (s.grid);
    clip.detected  = detectKey (notes);
    if (notes.empty())
        return clip;

    // 1) Tonalidad: la elegida o la detectada. Si eligen solo la tonica, el modo
    //    (mayor/menor) se decide con el que mejor calce para esa tonica.
    if (s.keyRoot >= 0)
    {
        const auto hist = histogram (notes);
        const Key maj { s.keyRoot % 12, false }, min { s.keyRoot % 12, true };
        clip.key = correlationFor (hist, min) > correlationFor (hist, maj) ? min : maj;
    }
    else
    {
        clip.key = clip.detected.key;
    }
    clip.scaleUsed = resolveScale (s.scale, clip.key);
    if (s.keyRoot < 0 && ! clip.detected.valid && s.scale == Scale::autoDetect)
        clip.scaleUsed = Scale::none; // muy pocas notas para adivinar: no forzar nada

    // 2) Segundos -> negras. Si sabemos donde estaba el proyecto al grabar, las
    //    notas quedan en su lugar real dentro del compas.
    const double beatsPerSec = (s.bpm > 0.0 ? s.bpm : 120.0) / 60.0;
    clip.alignedToProject = s.hostPpqAtStart >= 0.0;
    const double base = clip.alignedToProject ? s.hostPpqAtStart : -notes.front().startSec * beatsPerSec;

    const double g        = gridBeats (s.grid);
    const double strength = std::clamp ((double) s.strength, 0.0, 1.0);
    auto quantize = [&] (double beat)
    {
        if (g <= 0.0)
            return beat;
        const double target = std::round (beat / g) * g;
        return beat + strength * (target - beat);
    };

    for (const auto& n : notes)
    {
        ClipNote c;
        const double start = base + n.startSec * beatsPerSec;
        const double end   = base + n.endSec * beatsPerSec;
        c.startBeat  = quantize (start);
        c.lengthBeat = quantize (end) - c.startBeat;
        if (g > 0.0 && strength > 0.0)
            c.lengthBeat = std::max (c.lengthBeat, g * strength); // con fuerza 100%: minimo una celda
        c.lengthBeat = std::max (c.lengthBeat, 1.0 / 32.0);

        // 3) Altura: a la escala (con la altura real medida) y luego la octava.
        int m = snapToScale (n.pitch, clip.key.tonic, clip.scaleUsed);
        m += 12 * s.octaves;
        while (m > 127) m -= 12;
        while (m < 0)   m += 12;
        c.midiNote = m;
        c.velocity = n.velocity;
        clip.notes.push_back (c);
    }

    // 4) Limpieza tras cuantizar: la melodia es monofonica, asi que si dos notas
    //    caen en el mismo inicio queda la mas larga, y ninguna pisa a la siguiente.
    std::stable_sort (clip.notes.begin(), clip.notes.end(),
                      [] (const ClipNote& a, const ClipNote& b) { return a.startBeat < b.startBeat; });
    std::vector<ClipNote> cleaned;
    for (const auto& c : clip.notes)
    {
        if (! cleaned.empty() && std::abs (c.startBeat - cleaned.back().startBeat) < 1.0e-6)
        {
            if (c.lengthBeat > cleaned.back().lengthBeat)
                cleaned.back() = c;
            continue;
        }
        if (! cleaned.empty())
        {
            auto& prev = cleaned.back();
            prev.lengthBeat = std::min (prev.lengthBeat, c.startBeat - prev.startBeat);
        }
        cleaned.push_back (c);
    }
    clip.notes = std::move (cleaned);

    // 5) Origen del clip: alineado -> el inicio del compas de la primera nota
    //    (asi al soltarlo en un compas cada nota cae donde la cantaste);
    //    sin alinear -> la primera nota en el tiempo 0.
    const double first = clip.notes.front().startBeat;
    const int bar = std::max (1, s.beatsPerBar);
    clip.originBeat = clip.alignedToProject ? std::floor (first / bar + 1.0e-9) * bar : first;
    for (auto& c : clip.notes)
        c.startBeat = std::max (0.0, c.startBeat - clip.originBeat);

    return clip;
}

} // namespace fourever
