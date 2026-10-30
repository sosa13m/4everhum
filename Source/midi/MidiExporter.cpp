// 4EVERHUM - tararea -> MIDI
// Copyright (C) 2026 sosa13m / colectivo 4EVER
// SPDX-License-Identifier: AGPL-3.0-or-later
// Este programa es software libre: ver el archivo LICENSE.

#include "midi/MidiExporter.h"

namespace fourever
{

juce::MidiFile buildMidiFile (const std::vector<ClipNote>& notes, const MidiExportSettings& s)
{
    const double bpm = s.bpm > 0.0 ? s.bpm : 120.0;
    auto toTick = [&] (double beat) { return std::round (beat * s.ppq); };

    juce::MidiMessageSequence seq;
    seq.addEvent (juce::MidiMessage::textMetaEvent (3, s.trackName), 0.0);
    seq.addEvent (juce::MidiMessage::tempoMetaEvent ((int) std::round (60'000'000.0 / bpm)), 0.0);
    seq.addEvent (juce::MidiMessage::timeSignatureMetaEvent (4, 4), 0.0);

    double lastTick = 0.0;
    for (const auto& n : notes)
    {
        const double on  = juce::jmax (0.0, toTick (n.startBeat));
        // 1 tick menos para que el note-off nunca coincida con el note-on de la
        // siguiente nota igual (algunos DAWs las fusionarian).
        const double off = juce::jmax (on + 1.0, toTick (n.startBeat + n.lengthBeat) - 1.0);
        const auto   vel = (juce::uint8) juce::jlimit (1, 127, (int) std::round (n.velocity * 127.0f));

        seq.addEvent (juce::MidiMessage::noteOn  (s.channel, n.midiNote, vel), on);
        seq.addEvent (juce::MidiMessage::noteOff (s.channel, n.midiNote),      off);
        lastTick = juce::jmax (lastTick, off);
    }

    seq.addEvent (juce::MidiMessage::endOfTrack(), lastTick);
    seq.sort();
    seq.updateMatchedPairs();

    juce::MidiFile file;
    file.setTicksPerQuarterNote (s.ppq);
    file.addTrack (seq);
    return file;
}

bool writeMidiFile (const juce::File& file, const std::vector<ClipNote>& notes, const MidiExportSettings& s)
{
    auto midi = buildMidiFile (notes, s);

    file.deleteFile();
    juce::FileOutputStream out (file);
    if (! out.openedOk())
        return false;

    return midi.writeTo (out, 1);
}

juce::String suggestedMidiFileName (double bpm, const juce::String& keyLabel)
{
    return "4EVERHUM_" + (keyLabel.isNotEmpty() ? keyLabel + "_" : juce::String())
         + juce::String (juce::roundToInt (bpm)) + "bpm_"
         + juce::Time::getCurrentTime().formatted ("%Y%m%d-%H%M%S") + ".mid";
}

} // namespace fourever
