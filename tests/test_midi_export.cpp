// Prueba de punta a punta: voz sintetica -> notas -> .mid en disco -> se relee
// el archivo y se verifica que las notas caen donde deben a 140 BPM.

#include "dsp/Music.h"
#include "dsp/NoteSegmenter.h"
#include "dsp/PitchTracker.h"
#include "midi/MidiExporter.h"

#include <cmath>
#include <cstdio>

namespace
{
std::vector<float> tone (double sr, double midi, double sec)
{
    std::vector<float> v ((std::size_t) (sr * sec));
    double ph = 0.0;
    const double hz = 440.0 * std::pow (2.0, (midi - 69.0) / 12.0);
    for (std::size_t i = 0; i < v.size(); ++i)
    {
        ph += 2.0 * juce::MathConstants<double>::pi * hz / sr;
        const double env = std::min ({ 1.0, i / (0.02 * sr), (v.size() - i) / (0.03 * sr) });
        v[i] = (float) (0.3 * env * (std::sin (ph) + 0.4 * std::sin (2 * ph)));
    }
    return v;
}
} // namespace

int main()
{
    const double sr = 48000.0, bpm = 140.0, beat = 60.0 / bpm;

    // Negras a 140 BPM: Do - Mi - Sol (con 0.5 s de silencio inicial que se recorta)
    std::vector<float> audio ((std::size_t) (0.5 * sr), 0.0f);
    for (int note : { 60, 64, 67 })
    {
        auto t = tone (sr, note, beat * 0.85);
        audio.insert (audio.end(), t.begin(), t.end());
        audio.insert (audio.end(), (std::size_t) (beat * 0.15 * sr), 0.0f);
    }

    const auto notes = fourever::segmentNotes (fourever::trackPitch (audio.data(), audio.size(), sr));

    fourever::ClipSettings clipSettings;
    clipSettings.bpm  = bpm;
    clipSettings.grid = fourever::Grid::sixteenth;
    const auto clip = fourever::makeClip (notes, clipSettings);

    fourever::MidiExportSettings settings;
    settings.bpm = bpm;
    const auto file = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("4everhum_test.mid");
    if (! fourever::writeMidiFile (file, clip.notes, settings))
    {
        std::printf ("FAIL: no se pudo escribir el .mid\n");
        return 1;
    }

    juce::FileInputStream in (file);
    juce::MidiFile midi;
    if (! midi.readFrom (in))
    {
        std::printf ("FAIL: el .mid no se puede leer\n");
        return 1;
    }

    const int ppq = midi.getTimeFormat();
    const auto* track = midi.getTrack (0);
    std::vector<std::pair<int, double>> ons; // (nota, posicion en negras)
    double tempoUs = 0.0;
    for (const auto* ev : *track)
    {
        if (ev->message.isNoteOn())
            ons.emplace_back (ev->message.getNoteNumber(), ev->message.getTimeStamp() / ppq);
        if (ev->message.isTempoMetaEvent())
            tempoUs = ev->message.getTempoSecondsPerQuarterNote() * 1.0e6;
    }

    bool ok = ppq == 960 && std::abs (tempoUs - 60.0e6 / bpm) < 1.0 && ons.size() == 3;
    const int expected[] = { 60, 64, 67 };
    for (std::size_t i = 0; ok && i < ons.size(); ++i)
    {
        std::printf ("  nota %d en la negra %.3f\n", ons[i].first, ons[i].second);
        ok &= ons[i].first == expected[i] && std::abs (ons[i].second - (double) i) < 1.0e-9;
    }

    std::printf ("%s  (ppq %d, tempo %.0f us/negra, %zu notas)\n", ok ? "OK" : "FAIL", ppq, tempoUs, ons.size());
    file.deleteFile();
    return ok ? 0 : 1;
}
