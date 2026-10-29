#include "midi/MidiExporter.h"

namespace fourever
{

juce::MidiFile buildMidiFile (const std::vector<NoteEvent>& notes, const MidiExportSettings& s)
{
    const double bpm          = s.bpm > 0.0 ? s.bpm : 120.0;
    const double ticksPerSec  = bpm / 60.0 * s.ppq;
    const double offsetSec    = (s.trimLeadingSilence && ! notes.empty()) ? notes.front().startSec : 0.0;
    auto toTick = [&] (double sec) { return std::round ((sec - offsetSec) * ticksPerSec); };

    juce::MidiMessageSequence seq;
    seq.addEvent (juce::MidiMessage::textMetaEvent (3, s.trackName), 0.0);
    seq.addEvent (juce::MidiMessage::tempoMetaEvent ((int) std::round (60'000'000.0 / bpm)), 0.0);
    seq.addEvent (juce::MidiMessage::timeSignatureMetaEvent (4, 4), 0.0);

    double lastTick = 0.0;
    for (const auto& n : notes)
    {
        const double on  = juce::jmax (0.0, toTick (n.startSec));
        // 1 tick menos para que el note-off nunca coincida con el note-on de la
        // siguiente nota igual (algunos DAWs las fusionarian).
        const double off = juce::jmax (on + 1.0, toTick (n.endSec) - 1.0);
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

bool writeMidiFile (const juce::File& file, const std::vector<NoteEvent>& notes, const MidiExportSettings& s)
{
    auto midi = buildMidiFile (notes, s);

    file.deleteFile();
    juce::FileOutputStream out (file);
    if (! out.openedOk())
        return false;

    return midi.writeTo (out, 1);
}

juce::String suggestedMidiFileName (double bpm)
{
    return "4EVERHUM_" + juce::String (juce::roundToInt (bpm)) + "bpm_"
         + juce::Time::getCurrentTime().formatted ("%Y%m%d-%H%M%S") + ".mid";
}

} // namespace fourever
