// Test del DSP con una "voz" sintetica: armonicos, vibrato, ruido, silencios
// entre notas y un portamento (deslizamiento) entre dos notas ligadas.
//
//   cmake -B build -DFOUREVERHUM_BUILD_PLUGIN=OFF && cmake --build build && ctest --test-dir build

#include "dsp/Analyzer.h"
#include "dsp/Music.h"
#include "dsp/NoteSegmenter.h"
#include "dsp/PitchTracker.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <random>
#include <vector>

using namespace fourever;

namespace
{
constexpr double pi = 3.14159265358979323846;

struct ToneSpec
{
    int    note;        // MIDI
    double durSec;
    double glideSec;    // portamento desde la nota anterior (0 = sin ligadura)
    double gapAfterSec; // silencio despues
};

double midiToHz (double m) { return 440.0 * std::pow (2.0, (m - 69.0) / 12.0); }

std::vector<float> synthVoice (const std::vector<ToneSpec>& tones, double sr,
                               std::vector<double>& onsets,
                               double amp = 0.25, float noiseStd = 0.003f) // 0.003 ~ -50 dBFS
{
    std::vector<float> out;
    std::mt19937 rng (1234);
    std::normal_distribution<float> noise (0.0f, noiseStd);

    double phase = 0.0, t = 0.0, prevNote = tones.front().note;
    const double lead = 0.3; // silencio inicial
    for (int i = 0; i < (int) (lead * sr); ++i)
        out.push_back (noise (rng));
    t = lead;

    for (const auto& tone : tones)
    {
        // En una nota ligada, el inicio musical es la mitad del portamento.
        onsets.push_back (t + tone.glideSec * 0.5);
        const int len = (int) (tone.durSec * sr);
        for (int i = 0; i < len; ++i)
        {
            const double local = i / sr;
            double m = tone.note;
            if (tone.glideSec > 0.0 && local < tone.glideSec)
                m = prevNote + (tone.note - prevNote) * (local / tone.glideSec);
            m += 0.3 * std::sin (2.0 * pi * 5.5 * local); // vibrato +-30 cents

            phase += 2.0 * pi * midiToHz (m) / sr;
            // Envolvente con ataque/release cortos
            const double env = std::min ({ 1.0, local / 0.02, (tone.durSec - local) / 0.03 });
            const double s = std::sin (phase) + 0.5 * std::sin (2 * phase) + 0.25 * std::sin (3 * phase);
            out.push_back ((float) (amp * env * s) + noise (rng));
        }
        prevNote = tone.note;
        t += tone.durSec;

        for (int i = 0; i < (int) (tone.gapAfterSec * sr); ++i)
            out.push_back (noise (rng));
        t += tone.gapAfterSec;
    }
    return out;
}

int failures = 0;

void check (bool cond, const char* what)
{
    std::printf ("  [%s] %s\n", cond ? " OK " : "FAIL", what);
    if (! cond)
        ++failures;
}

void runCase (double sr)
{
    std::printf ("\nSample rate %.0f Hz\n", sr);

    // A3 - C4 - E4 ~> D4 (ligado con portamento) - A3 grave al final
    const std::vector<ToneSpec> melody {
        { 57, 0.40, 0.00, 0.10 },
        { 60, 0.30, 0.00, 0.08 },
        { 64, 0.45, 0.00, 0.00 },
        { 62, 0.40, 0.06, 0.12 },
        { 45, 0.50, 0.00, 0.20 },
    };

    std::vector<double> onsets;
    const auto audio  = synthVoice (melody, sr, onsets);
    const auto frames = trackPitch (audio.data(), audio.size(), sr);
    const auto notes  = segmentNotes (frames);

    for (const auto& nt : notes)
        std::printf ("    nota %3d  %.3f -> %.3f s  vel %.2f\n", nt.midiNote, nt.startSec, nt.endSec, nt.velocity);

    check (notes.size() == melody.size(), "cantidad de notas correcta");
    if (notes.size() != melody.size())
        return;

    bool pitches = true, timing = true;
    for (std::size_t i = 0; i < notes.size(); ++i)
    {
        pitches &= notes[i].midiNote == melody[i].note;
        timing  &= std::abs (notes[i].startSec - onsets[i]) < 0.05;
    }
    check (pitches, "alturas exactas pese al vibrato y el portamento");
    check (timing, "inicio de cada nota con error < 50 ms");
}

void runSilence()
{
    std::printf ("\nSolo ruido de fondo\n");
    std::vector<float> quiet (48000 * 2);
    std::mt19937 rng (7);
    std::normal_distribution<float> noise (0.0f, 0.003f);
    for (auto& x : quiet)
        x = noise (rng);
    const auto notes = segmentNotes (trackPitch (quiet.data(), quiet.size(), 48000.0));
    check (notes.empty(), "no inventa notas en el silencio");
}

const std::vector<ToneSpec> simpleMelody {
    { 60, 0.35, 0.00, 0.12 },
    { 64, 0.35, 0.00, 0.12 },
    { 67, 0.45, 0.00, 0.20 },
};

bool sameNotes (const std::vector<NoteEvent>& got, const std::vector<ToneSpec>& want)
{
    if (got.size() != want.size())
        return false;
    for (std::size_t i = 0; i < got.size(); ++i)
        if (got[i].midiNote != want[i].note)
            return false;
    return true;
}

void runBeatBleed()
{
    // El bajo del beat se filtra por los audifonos al microfono (~ -32 dBFS).
    // Sin filtro aparecen notas fantasma entre las tuyas; con filtro, no.
    std::printf ("\nBeat filtrandose al microfono (filtro de ruido automatico)\n");
    std::vector<double> onsets;
    auto audio = synthVoice (simpleMelody, 48000.0, onsets);
    double ph = 0.0;
    for (auto& x : audio)
    {
        ph += 2.0 * pi * midiToHz (43) / 48000.0;
        x += (float) (0.02 * (std::sin (ph) + 0.6 * std::sin (2 * ph) + 0.3 * std::sin (3 * ph)));
    }

    UserSettings off; off.autoGate = false;
    UserSettings on;  on.autoGate  = true;
    check (! sameNotes (analyze (audio.data(), audio.size(), 48000.0, off), simpleMelody),
           "sin filtro: el bajo genera notas fantasma (caso de control)");
    check (sameNotes (analyze (audio.data(), audio.size(), 48000.0, on), simpleMelody),
           "con filtro: solo las 3 notas tarareadas");
}

void runQuietVoice()
{
    // Voz muy suave (~ -48 dBFS) en una pieza silenciosa: la sensibilidad decide.
    std::printf ("\nVoz muy suave (perilla de sensibilidad)\n");
    std::vector<double> onsets;
    const auto audio = synthVoice (simpleMelody, 48000.0, onsets, 0.004, 0.0003f);

    UserSettings low;  low.sensitivity  = 20.0f;
    UserSettings high; high.sensitivity = 90.0f;
    check (analyze (audio.data(), audio.size(), 48000.0, low).empty(),
           "sensibilidad 20: la ignora (no capta susurros ni ruido)");
    check (sameNotes (analyze (audio.data(), audio.size(), 48000.0, high), simpleMelody),
           "sensibilidad 90: la detecta completa");
}

void runMinNote()
{
    // Una nota de 50 ms (un "golpe" de voz) entre dos notas normales.
    std::printf ("\nNota muy corta (perilla de nota minima)\n");
    const std::vector<ToneSpec> withBlip {
        { 60, 0.35, 0.00, 0.12 },
        { 72, 0.05, 0.00, 0.12 },
        { 64, 0.35, 0.00, 0.20 },
    };
    std::vector<double> onsets;
    const auto audio = synthVoice (withBlip, 48000.0, onsets);

    UserSettings keep; keep.minNoteMs = 30.0f;
    UserSettings drop; drop.minNoteMs = 120.0f;
    check (analyze (audio.data(), audio.size(), 48000.0, keep).size() == 3, "nota minima 30 ms: conserva la nota corta");
    check (analyze (audio.data(), audio.size(), 48000.0, drop).size() == 2, "nota minima 120 ms: la descarta");
}

void runTranspose()
{
    std::printf ("\nOctavas\n");
    std::vector<NoteEvent> notes (3);
    notes[0].midiNote = 48; notes[1].midiNote = 60; notes[2].midiNote = 120;
    const auto up = transposeOctaves (notes, 1);
    const auto down = transposeOctaves (notes, -2);
    check (up[0].midiNote == 60 && up[1].midiNote == 72, "+1 octava suma 12 semitonos");
    check (up[2].midiNote == 120, "una nota que se saldria del rango MIDI no se pierde");
    check (down[0].midiNote == 24 && down[1].midiNote == 36, "-2 octavas resta 24 semitonos");
}

void runSloppySinger()
{
    // Un "cantante" humano a 100 BPM en corcheas (0.3 s): cada nota desafinada
    // hasta +-35 cents y entrando hasta 40 ms antes o despues del pulso.
    std::printf ("\nCantante desafinado y fuera de tiempo -> escala + grid\n");
    const double sr = 48000.0, eighth = 0.3;
    const int    notes[]  { 57, 60, 64, 62, 60, 59, 57, 64 };   // La menor
    const double cents[]  { 0.30, -0.35, 0.25, -0.30, 0.35, -0.20, 0.15, -0.35 };
    const double jitter[] { 0.0, 0.030, -0.025, 0.040, -0.035, 0.020, -0.040, 0.025 };

    std::vector<float> audio ((std::size_t) (0.3 * sr), 0.0f);
    double phase = 0.0, cursor = 0.3;
    for (int i = 0; i < 8; ++i)
    {
        const double start = 0.3 + i * eighth + jitter[i];
        while (cursor < start) { audio.push_back (0.0f); cursor += 1.0 / sr; }
        const double dur = eighth * 0.7;
        for (int k = 0; k < (int) (dur * sr); ++k)
        {
            const double t = k / sr;
            phase += 2.0 * pi * midiToHz (notes[i] + cents[i] + 0.15 * std::sin (2 * pi * 5.0 * t)) / sr;
            const double env = std::min ({ 1.0, t / 0.015, (dur - t) / 0.025 });
            audio.push_back ((float) (0.25 * env * (std::sin (phase) + 0.5 * std::sin (2 * phase))));
            cursor += 1.0 / sr;
        }
    }
    audio.resize (audio.size() + (std::size_t) (0.3 * sr), 0.0f);

    const auto analyzed = analyze (audio.data(), audio.size(), sr, UserSettings{});
    ClipSettings cs;
    cs.bpm  = 100.0;
    cs.grid = Grid::eighth;
    const auto clip = makeClip (analyzed, cs);

    std::printf ("    tonalidad: %s (%s)\n", keyName (clip.key).c_str(), camelot (clip.key).c_str());
    bool pitches = clip.notes.size() == 8, timing = clip.notes.size() == 8;
    for (std::size_t i = 0; i < clip.notes.size() && i < 8; ++i)
    {
        std::printf ("    nota %d en la negra %.3f\n", clip.notes[i].midiNote, clip.notes[i].startBeat);
        pitches &= clip.notes[i].midiNote == notes[i];
        timing  &= std::abs (clip.notes[i].startBeat - 0.5 * (double) i) < 1.0e-6;
    }
    check (keyName (clip.key) == "Am", "detecta La menor");
    check (pitches, "las 8 alturas quedan exactas pese a desafinar hasta 35 cents");
    check (timing, "las 8 notas caen exacto en corcheas pese a +-40 ms de error");
}
} // namespace

int main()
{
    runCase (44100.0);
    runCase (48000.0);
    runCase (96000.0);
    runSilence();
    runBeatBleed();
    runQuietVoice();
    runMinNote();
    runTranspose();
    runSloppySinger();

    std::printf ("\n%s (%d fallos)\n", failures == 0 ? "TODO OK" : "HAY FALLOS", failures);
    return failures == 0 ? 0 : 1;
}
