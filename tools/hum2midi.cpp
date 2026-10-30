// hum2midi: convierte una grabacion de tarareo en .mid desde la terminal, con
// el mismo motor y los mismos ajustes que el plugin.
//
//   hum2midi toma.wav
//   hum2midi toma.wav --bpm 140 --octava 1 -o melodia.mid
//   hum2midi toma.wav --sensibilidad 70 --estabilidad 80
//   hum2midi toma.wav --tonalidad Am --cuantizar 1/8
//
// Sirve para probar rapido cambios de ajustes sobre grabaciones reales, sin
// abrir el DAW.

#include "dsp/Analyzer.h"
#include "dsp/Music.h"
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
        "  --rapido                     Usa YIN en vez de pYIN (menos preciso, mas rapido)\n"
        "  --tonalidad <auto|C|C#|Db|...|B>[m]  Tonalidad (por defecto auto; 'Am' = La menor)\n"
        "  --escala <nombre>            auto, mayor, menor, menor-armonica, dorica, frigia,\n"
        "                               penta-mayor, penta-menor o ninguna (por defecto auto)\n"
        "  --cuantizar <grid>           apagado, 1/4, 1/8, 1/16, 1/32, 1/8T o 1/16T (por defecto 1/16)\n"
        "  --fuerza <0-100>             Fuerza de la cuantizacion (por defecto 100)\n"
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

// "C", "c#", "Db", "Am", "F#m" -> tonica 0..11 (y si termina en m, menor). -1 si no se entiende.
int parseKey (juce::String text, bool& minor)
{
    text = text.trim();
    minor = text.length() > 1 && text.endsWithIgnoreCase ("m");
    if (minor)
        text = text.dropLastCharacters (1);

    static const char* names[] { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    static const char* flats[] { "C", "Db", "D", "Eb", "E", "F", "Gb", "G", "Ab", "A", "Bb", "B" };
    for (int i = 0; i < 12; ++i)
        if (text.equalsIgnoreCase (names[i]) || text.equalsIgnoreCase (flats[i]))
            return i;
    return -1;
}

bool parseScale (const juce::String& text, fourever::Scale& out)
{
    using S = fourever::Scale;
    const std::pair<const char*, S> table[] {
        { "auto", S::autoDetect }, { "mayor", S::major }, { "menor", S::minor },
        { "menor-armonica", S::harmonicMinor }, { "dorica", S::dorian }, { "frigia", S::phrygian },
        { "penta-mayor", S::pentatonicMajor }, { "penta-menor", S::pentatonicMinor }, { "ninguna", S::none } };
    for (const auto& [name, value] : table)
        if (text.equalsIgnoreCase (name)) { out = value; return true; }
    return false;
}

bool parseGrid (const juce::String& text, fourever::Grid& out)
{
    using G = fourever::Grid;
    const std::pair<const char*, G> table[] {
        { "apagado", G::off }, { "1/4", G::quarter }, { "1/8", G::eighth }, { "1/16", G::sixteenth },
        { "1/32", G::thirtySecond }, { "1/8T", G::eighthTriplet }, { "1/16T", G::sixteenthTriplet } };
    for (const auto& [name, value] : table)
        if (text.equalsIgnoreCase (name)) { out = value; return true; }
    return false;
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
    fourever::ClipSettings clipSettings;
    double bpm = 120.0, octave = 0.0;
    bool scaleGiven = false, keyIsMinor = false;

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
        else if (a == "--rapido")        { settings.highPrecision = false; }
        else if (a == "--fuerza")        { ok = readNumber (args, i, v) && v >= 0.0 && v <= 100.0; clipSettings.strength = (float) (v / 100.0); }
        else if (a == "--tonalidad")
        {
            ok = i + 1 < args.size();
            if (ok && ! args[i + 1].equalsIgnoreCase ("auto"))
            {
                clipSettings.keyRoot = parseKey (args[i + 1], keyIsMinor);
                ok = clipSettings.keyRoot >= 0;
            }
            ++i;
        }
        else if (a == "--escala")        { ok = i + 1 < args.size() && parseScale (args[++i], clipSettings.scale); scaleGiven = true; }
        else if (a == "--cuantizar")     { ok = i + 1 < args.size() && parseGrid (args[++i], clipSettings.grid); }
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

    // "--tonalidad Am" sin --escala: escala menor.
    if (keyIsMinor && ! scaleGiven)
        clipSettings.scale = fourever::Scale::minor;

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
    std::printf ("Ajustes: sensibilidad %.0f | nota minima %.0f ms | estabilidad %.0f | octava %+d | filtro %s | %s\n",
                 settings.sensitivity, settings.minNoteMs, settings.stability, (int) octave,
                 settings.autoGate ? "si" : "no", settings.highPrecision ? "pYIN" : "YIN");

    const auto notes = fourever::analyze (mono.data(), mono.size(), reader->sampleRate, settings);

    if (notes.empty())
    {
        std::puts ("\nNo se detectaron notas. Prueba con --sensibilidad 80 o revisa que la grabacion tenga voz.");
        return 2;
    }

    clipSettings.bpm     = bpm;
    clipSettings.octaves = (int) octave;
    const auto clip = fourever::makeClip (notes, clipSettings);

    // --- Tonalidad -----------------------------------------------------------
    if (clip.scaleUsed == fourever::Scale::none)
        std::printf ("Tonalidad: sin escala%s\n", clip.detected.valid ? "" : " (muy pocas notas para detectarla)");
    else
        std::printf ("Tonalidad: %s (%s, Camelot %s)%s\n", fourever::keyNameSpanish (clip.key).c_str(),
                     fourever::keyName (clip.key).c_str(), fourever::camelot (clip.key).c_str(),
                     clipSettings.keyRoot >= 0 ? " - elegida" : " - detectada");
    if (clip.detected.valid && clipSettings.keyRoot < 0)
        std::printf ("           otras posibles: %s, %s\n", fourever::keyName (clip.detected.alternatives[1]).c_str(),
                     fourever::keyName (clip.detected.alternatives[2]).c_str());

    // --- Notas tal como quedan en el MIDI ----------------------------------------
    std::printf ("\n  #  Nota (FL)  MIDI   Inicio (negras)  Largo (negras)\n");
    for (std::size_t i = 0; i < clip.notes.size(); ++i)
    {
        const auto& n = clip.notes[i];
        std::printf ("%3zu  %-9s  %4d   %8.3f         %8.3f\n", i + 1,
                     juce::MidiMessage::getMidiNoteName (n.midiNote, true, true, 5).toRawUTF8(),
                     n.midiNote, n.startBeat, n.lengthBeat);
    }

    fourever::MidiExportSettings exportSettings;
    exportSettings.bpm = bpm;
    if (! fourever::writeMidiFile (output, clip.notes, exportSettings))
    {
        std::fprintf (stderr, "\nNo se pudo escribir %s\n", output.getFullPathName().toRawUTF8());
        return 1;
    }

    std::printf ("\n%zu notas -> %s (%.0f BPM)\n", clip.notes.size(), output.getFullPathName().toRawUTF8(), bpm);
    return 0;
}
