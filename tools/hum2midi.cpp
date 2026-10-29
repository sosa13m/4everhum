// hum2midi: convierte una grabacion de tarareo en .mid desde la terminal, con
// el mismo motor y los mismos ajustes que el plugin.
//
//   hum2midi toma.wav
//   hum2midi toma.wav --bpm 140 --octava 1 -o melodia.mid
//   hum2midi toma.wav --sensibilidad 70 --estabilidad 80
//
// Sirve para probar rapido cambios de ajustes sobre grabaciones reales, sin
// abrir el DAW.

#include "dsp/Analyzer.h"
#include "midi/MidiExporter.h"

#include <juce_audio_formats/juce_audio_formats.h>

#include <cstdio>

namespace
{
void printUsage()
{
    std::puts (
        "Uso: hum2midi <entrada.wav|aiff|flac|mp3> [opciones]\n"
        "\n"
        "Opciones:\n"
        "  -o, --salida <archivo.mid>   Archivo de salida (por defecto: junto a la entrada)\n"
        "  --bpm <n>                    Tempo del MIDI (por defecto 120)\n"
        "  --sensibilidad <0-100>       Que tan suave puede ser la voz (por defecto 50)\n"
        "  --nota-minima <30-300>       Duracion minima de nota en ms (por defecto 70)\n"
        "  --estabilidad <0-100>        Cuanto ignora vibrato/desafinacion (por defecto 50)\n"
        "  --octava <-2..2>             Transponer en octavas (por defecto 0)\n"
        "  --sin-filtro                 Desactiva el filtro de ruido automatico\n"
        "  -h, --ayuda                  Muestra esta ayuda");
}

bool readNumber (const juce::StringArray& args, int& i, double& out)
{
    if (i + 1 >= args.size())
        return false;
    const auto text = args[++i];
    if (! text.containsOnly ("+-0123456789.") || text.isEmpty())
        return false;
    out = text.getDoubleValue();
    return true;
}
} // namespace

int main (int argc, char* argv[])
{
    juce::StringArray args;
    for (int i = 1; i < argc; ++i)
        args.add (juce::String::fromUTF8 (argv[i]));

    if (args.isEmpty() || args.contains ("-h") || args.contains ("--ayuda") || args.contains ("--help"))
    {
        printUsage();
        return args.isEmpty() ? 1 : 0;
    }

    juce::File input, output;
    fourever::UserSettings settings;
    double bpm = 120.0, octave = 0.0;

    for (int i = 0; i < args.size(); ++i)
    {
        const auto& a = args[i];
        double v = 0.0;
        bool ok = true;

        if (a == "-o" || a == "--salida")
        {
            ok = i + 1 < args.size();
            if (ok) output = juce::File::getCurrentWorkingDirectory().getChildFile (args[++i]);
        }
        else if (a == "--bpm")           { ok = readNumber (args, i, v) && v > 0.0; bpm = v; }
        else if (a == "--sensibilidad")  { ok = readNumber (args, i, v); settings.sensitivity = (float) v; }
        else if (a == "--nota-minima")   { ok = readNumber (args, i, v); settings.minNoteMs   = (float) v; }
        else if (a == "--estabilidad")   { ok = readNumber (args, i, v); settings.stability   = (float) v; }
        else if (a == "--octava")        { ok = readNumber (args, i, v) && v >= -2.0 && v <= 2.0; octave = v; }
        else if (a == "--sin-filtro")    { settings.autoGate = false; }
        else if (a.startsWith ("-"))     { ok = false; }
        else                             { input = juce::File::getCurrentWorkingDirectory().getChildFile (a); }

        if (! ok)
        {
            std::fprintf (stderr, "Opcion invalida o sin valor: %s\n\n", a.toRawUTF8());
            printUsage();
            return 1;
        }
    }

    if (! input.existsAsFile())
    {
        std::fprintf (stderr, "No existe el archivo: %s\n", input.getFullPathName().toRawUTF8());
        return 1;
    }
    if (output == juce::File{})
        output = input.withFileExtension ("mid");

    // --- Leer audio y mezclar a mono ---------------------------------------
    juce::AudioFormatManager formats;
    formats.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> reader (formats.createReaderFor (input));
    if (reader == nullptr)
    {
        std::fprintf (stderr, "Formato de audio no soportado: %s\n", input.getFileName().toRawUTF8());
        return 1;
    }

    const auto numSamples = (int) reader->lengthInSamples;
    const auto numCh      = (int) reader->numChannels;
    juce::AudioBuffer<float> buffer (numCh, numSamples);
    reader->read (&buffer, 0, numSamples, 0, true, true);

    std::vector<float> mono ((std::size_t) numSamples, 0.0f);
    for (int ch = 0; ch < numCh; ++ch)
        for (int i = 0; i < numSamples; ++i)
            mono[(std::size_t) i] += buffer.getSample (ch, i) / (float) numCh;

    // --- Analizar ----------------------------------------------------------
    std::printf ("Entrada: %s  (%.1f s, %.0f Hz, %d canal%s)\n", input.getFileName().toRawUTF8(),
                 numSamples / reader->sampleRate, reader->sampleRate, numCh, numCh == 1 ? "" : "es");
    std::printf ("Ajustes: sensibilidad %.0f | nota minima %.0f ms | estabilidad %.0f | octava %+d | filtro %s\n",
                 settings.sensitivity, settings.minNoteMs, settings.stability, (int) octave,
                 settings.autoGate ? "si" : "no");

    auto notes = fourever::analyze (mono.data(), mono.size(), reader->sampleRate, settings);
    notes      = fourever::transposeOctaves (std::move (notes), (int) octave);

    if (notes.empty())
    {
        std::puts ("\nNo se detectaron notas. Prueba con --sensibilidad 80 o revisa que la grabacion tenga voz.");
        return 2;
    }

    // --- Mostrar (en tiempos del MIDI resultante: la primera nota cae en 0) --
    const double beatSec = 60.0 / bpm;
    const double t0      = notes.front().startSec;
    std::printf ("\n  #  Nota (FL)  MIDI   Tiempo     Inicio (negras)  Duracion\n");
    for (std::size_t i = 0; i < notes.size(); ++i)
    {
        const auto& n = notes[i];
        std::printf ("%3zu  %-9s  %4d   %6.3f s   %8.2f          %4.0f ms\n", i + 1,
                     juce::MidiMessage::getMidiNoteName (n.midiNote, true, true, 5).toRawUTF8(),
                     n.midiNote, n.startSec, (n.startSec - t0) / beatSec, (n.endSec - n.startSec) * 1000.0);
    }

    fourever::MidiExportSettings exportSettings;
    exportSettings.bpm = bpm;
    if (! fourever::writeMidiFile (output, notes, exportSettings))
    {
        std::fprintf (stderr, "\nNo se pudo escribir %s\n", output.getFullPathName().toRawUTF8());
        return 1;
    }

    std::printf ("\n%zu notas -> %s (%.0f BPM)\n", notes.size(), output.getFullPathName().toRawUTF8(), bpm);
    return 0;
}
