// 4EVERHUM - tararea -> MIDI
// Copyright (C) 2026 sosa13m / colectivo 4EVER
// SPDX-License-Identifier: AGPL-3.0-or-later
// Este programa es software libre: ver el archivo LICENSE.

#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
const juce::StringArray octaveChoices { "-2", "-1", "0", "+1", "+2" };
constexpr int octaveZeroIndex = 2;

// Parametros que cambian el analisis (requieren re-analizar la toma)...
const juce::StringArray analysisIds { paramIds::sensitivity, paramIds::minNote,
                                      paramIds::stability, paramIds::autoGate, paramIds::precision };
// ...y los que solo cambian el clip final (instantaneos).
const juce::StringArray clipIds { paramIds::octave, paramIds::keyRoot, paramIds::scale,
                                  paramIds::quantize, paramIds::strength };
} // namespace

const juce::StringArray& FourEverHumProcessor::keyChoices()
{
    static const juce::StringArray c { "Auto", "C", "C#", "D", "Eb", "E", "F", "F#", "G", "Ab", "A", "Bb", "B" };
    return c;
}

const juce::StringArray& FourEverHumProcessor::scaleChoices()
{
    // Mismo orden que fourever::Scale.
    static const juce::StringArray c { "Auto", "Mayor", "Menor", juce::String::fromUTF8 ("Menor armónica"),
                                       juce::String::fromUTF8 ("Dórica"), "Frigia",
                                       juce::String::fromUTF8 ("Pentatónica mayor"),
                                       juce::String::fromUTF8 ("Pentatónica menor"), "Sin escala" };
    return c;
}

const juce::StringArray& FourEverHumProcessor::quantizeChoices()
{
    // Mismo orden que fourever::Grid.
    static const juce::StringArray c { "Apagado", "1/4", "1/8", "1/16", "1/32", "1/8T", "1/16T" };
    return c;
}

juce::AudioProcessorValueTreeState::ParameterLayout FourEverHumProcessor::createLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout layout;

    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { paramIds::sensitivity, 1 }, "Sensibilidad",
        NormalisableRange<float> (0.0f, 100.0f, 1.0f), 50.0f,
        AudioParameterFloatAttributes().withLabel ("%")));

    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { paramIds::minNote, 1 }, String::fromUTF8 ("Nota mínima"),
        NormalisableRange<float> (30.0f, 300.0f, 5.0f), 70.0f,
        AudioParameterFloatAttributes().withLabel ("ms")));

    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { paramIds::stability, 1 }, "Estabilidad",
        NormalisableRange<float> (0.0f, 100.0f, 1.0f), 50.0f,
        AudioParameterFloatAttributes().withLabel ("%")));

    layout.add (std::make_unique<AudioParameterChoice> (
        ParameterID { paramIds::octave, 1 }, "Octava", octaveChoices, octaveZeroIndex));

    layout.add (std::make_unique<AudioParameterBool> (
        ParameterID { paramIds::autoGate, 1 }, "Filtro de ruido", true));

    layout.add (std::make_unique<AudioParameterBool> (
        ParameterID { paramIds::precision, 1 }, String::fromUTF8 ("Alta precisión"), true));

    layout.add (std::make_unique<AudioParameterChoice> (
        ParameterID { paramIds::keyRoot, 1 }, "Tonalidad", keyChoices(), 0));

    layout.add (std::make_unique<AudioParameterChoice> (
        ParameterID { paramIds::scale, 1 }, "Escala", scaleChoices(), 0));

    layout.add (std::make_unique<AudioParameterChoice> (
        ParameterID { paramIds::quantize, 1 }, "Cuantizar", quantizeChoices(), 3)); // 1/16

    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { paramIds::strength, 1 }, "Fuerza",
        NormalisableRange<float> (0.0f, 100.0f, 1.0f), 100.0f,
        AudioParameterFloatAttributes().withLabel ("%")));

    return layout;
}

FourEverHumProcessor::FourEverHumProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      params (*this, nullptr, "PARAMS", createLayout())
{
    sensitivityParam = params.getRawParameterValue (paramIds::sensitivity);
    minNoteParam     = params.getRawParameterValue (paramIds::minNote);
    stabilityParam   = params.getRawParameterValue (paramIds::stability);
    octaveParam      = params.getRawParameterValue (paramIds::octave);
    autoGateParam    = params.getRawParameterValue (paramIds::autoGate);
    precisionParam   = params.getRawParameterValue (paramIds::precision);
    keyRootParam     = params.getRawParameterValue (paramIds::keyRoot);
    scaleParam       = params.getRawParameterValue (paramIds::scale);
    quantizeParam    = params.getRawParameterValue (paramIds::quantize);
    strengthParam    = params.getRawParameterValue (paramIds::strength);

    for (const auto* ids : { &analysisIds, &clipIds })
        for (const auto& id : *ids)
            params.addParameterListener (id, this);

    startTimerHz (20);
}

FourEverHumProcessor::~FourEverHumProcessor()
{
    stopTimer();
    for (const auto* ids : { &analysisIds, &clipIds })
        for (const auto& id : *ids)
            params.removeParameterListener (id, this);
    analysisPool.removeAllJobs (true, 5000);
}

bool FourEverHumProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
        return false;
    return layouts.getMainInputChannelSet() == out;
}

void FourEverHumProcessor::prepareToPlay (double sampleRate, int)
{
    const auto needed = (std::size_t) (sampleRate * maxRecordSeconds);

    if (! juce::exactlyEqual (sampleRate, currentSampleRate) || recordBuffer.size() != needed)
    {
        // Cambio de sample rate: lo que se estaba grabando ya no sirve.
        // (La ultima toma terminada se conserva: guarda su propio sample rate.)
        recording.store (false);
        if (state.load() == State::recording)
            state.store (State::idle);

        recordBuffer.assign (needed, 0.0f);
        writePos.store (0);
        currentSampleRate = sampleRate;
    }
}

void FourEverHumProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const int numIn      = getTotalNumInputChannels();
    const int numSamples = buffer.getNumSamples();

    for (int ch = numIn; ch < getTotalNumOutputChannels(); ++ch)
        buffer.clear (ch, 0, numSamples);

    // Tempo del DAW y, en el primer bloque de una toma, donde estaba el proyecto.
    const bool captureStart = recording.load (std::memory_order_acquire) && needStartPosition.load();
    if (auto* ph = getPlayHead())
    {
        if (auto pos = ph->getPosition())
        {
            if (auto bpm = pos->getBpm())
                hostBpm.store (*bpm);

            if (captureStart)
            {
                const auto ppq = pos->getPpqPosition();
                takeStartPpq.store (pos->getIsPlaying() && ppq.hasValue() ? *ppq : -1.0);
                if (auto sig = pos->getTimeSignature())
                    takeBeatsPerBar.store (juce::jmax (1, sig->numerator * 4 / juce::jmax (1, sig->denominator)));
            }
        }
    }
    if (captureStart)
        needStartPosition.store (false);

    // Medidor de entrada.
    float peak = 0.0f;
    for (int ch = 0; ch < numIn; ++ch)
        peak = juce::jmax (peak, buffer.getMagnitude (ch, 0, numSamples));
    inputLevel.store (juce::jmax (peak, inputLevel.load() * 0.85f));

    if (! recording.load (std::memory_order_acquire) || numIn == 0)
        return; // el audio pasa tal cual: el plugin no altera la senal

    const int pos      = writePos.load (std::memory_order_relaxed);
    const int capacity = (int) recordBuffer.size();
    const int toWrite  = juce::jmin (numSamples, capacity - pos);
    const float gain   = 1.0f / (float) numIn;

    for (int i = 0; i < toWrite; ++i)
    {
        float mono = 0.0f;
        for (int ch = 0; ch < numIn; ++ch)
            mono += buffer.getSample (ch, i);
        recordBuffer[(std::size_t) (pos + i)] = mono * gain;
    }

    writePos.store (pos + toWrite, std::memory_order_release);

    if (pos + toWrite >= capacity)
        recording.store (false); // buffer lleno; el editor detecta esto y analiza
}

fourever::UserSettings FourEverHumProcessor::currentSettings() const
{
    fourever::UserSettings u;
    u.sensitivity = sensitivityParam->load();
    u.minNoteMs   = minNoteParam->load();
    u.stability   = stabilityParam->load();
    u.autoGate    = autoGateParam->load() > 0.5f;
    u.highPrecision = precisionParam->load() > 0.5f;
    return u;
}

int FourEverHumProcessor::octaveShift() const
{
    return juce::roundToInt (octaveParam->load()) - octaveZeroIndex;
}

void FourEverHumProcessor::startRecording()
{
    if (state.load() == State::analyzing)
        return;

    writePos.store (0, std::memory_order_relaxed);
    takeStartPpq.store (-1.0);
    takeBeatsPerBar.store (4);
    needStartPosition.store (true);
    state.store (State::recording);
    recording.store (true, std::memory_order_release);
}

void FourEverHumProcessor::stopRecording()
{
    recording.store (false, std::memory_order_release);
    if (state.load() != State::recording)
        return;

    // Copia de lo grabado: el analisis trabaja sobre su propia copia y el
    // audio thread queda libre para una nueva toma.
    auto take = std::make_shared<Take>();
    const int n = writePos.load (std::memory_order_acquire);
    take->samples.assign (recordBuffer.begin(), recordBuffer.begin() + n);
    take->sampleRate = currentSampleRate;
    take->bpm        = hostBpm.load();
    take->hostPpq    = needStartPosition.load() ? -1.0 : takeStartPpq.load();
    take->beatsPerBar = takeBeatsPerBar.load();

    {
        const juce::ScopedLock sl (resultLock);
        lastTake = take;
        editedNotes.clear(); // toma nueva: la edicion manual anterior ya no aplica
        edited = false;
    }
    launchAnalysis (take);
}

void FourEverHumProcessor::launchAnalysis (std::shared_ptr<const Take> take)
{
    settingsDirty.store (false);
    state.store (State::analyzing);

    const auto settings = currentSettings();

    analysisPool.addJob ([this, take, settings]
    {
        auto result = fourever::analyze (take->samples.data(), take->samples.size(),
                                         take->sampleRate, settings);
        {
            const juce::ScopedLock sl (resultLock);
            notes = std::move (result);
        }
        resultBpm.store (take->bpm);
        resultHostPpq.store (take->hostPpq);
        resultBeatsPerBar.store (take->beatsPerBar);
        ++resultVersion;
        state.store (State::ready);
    });
}

void FourEverHumProcessor::parameterChanged (const juce::String& id, float)
{
    // Puede llamarse desde el audio thread (automatizacion): solo atomics.
    if (clipIds.contains (id))
    {
        ++resultVersion; // escala, grid y octava no requieren re-analizar
        return;
    }
    lastSettingsChangeMs.store (juce::Time::getMillisecondCounter());
    settingsDirty.store (true);
}

void FourEverHumProcessor::timerCallback()
{
    if (! settingsDirty.load())
        return;

    const auto s = state.load();
    if (s == State::recording || s == State::analyzing)
        return; // al terminar se usaran los ajustes nuevos

    if (juce::Time::getMillisecondCounter() - lastSettingsChangeMs.load() < 150)
        return;

    if (auto take = getLastTake())
        launchAnalysis (take);
    else
        settingsDirty.store (false); // no hay toma que re-analizar
}

double FourEverHumProcessor::getRecordedSeconds() const
{
    return currentSampleRate > 0.0 ? writePos.load() / currentSampleRate : 0.0;
}

fourever::ClipSettings FourEverHumProcessor::currentClipSettings() const
{
    fourever::ClipSettings c;
    c.bpm            = resultBpm.load();
    c.hostPpqAtStart = resultHostPpq.load();
    c.beatsPerBar    = resultBeatsPerBar.load();
    c.keyRoot        = juce::roundToInt (keyRootParam->load()) - 1; // 0 = Auto -> -1
    c.scale          = (fourever::Scale) juce::jlimit (0, scaleChoices().size() - 1, juce::roundToInt (scaleParam->load()));
    c.grid           = (fourever::Grid) juce::jlimit (0, quantizeChoices().size() - 1, juce::roundToInt (quantizeParam->load()));
    c.strength       = strengthParam->load() / 100.0f;
    c.octaves        = octaveShift();
    return c;
}

fourever::Clip FourEverHumProcessor::getClip() const
{
    std::vector<fourever::NoteEvent> copy;
    std::vector<fourever::ClipNote> manual;
    bool isEdited = false;
    {
        const juce::ScopedLock sl (resultLock);
        copy     = notes;
        manual   = editedNotes;
        isEdited = edited;
    }

    auto clip = fourever::makeClip (copy, currentClipSettings());
    if (isEdited)
    {
        clip.notes  = std::move (manual);
        clip.edited = true;
    }
    return clip;
}

void FourEverHumProcessor::setEditedNotes (std::vector<fourever::ClipNote> newNotes)
{
    {
        const juce::ScopedLock sl (resultLock);
        editedNotes = std::move (newNotes);
        edited      = true;
    }
    ++resultVersion;
}

void FourEverHumProcessor::clearEditedNotes()
{
    {
        const juce::ScopedLock sl (resultLock);
        editedNotes.clear();
        edited = false;
    }
    ++resultVersion;
}

bool FourEverHumProcessor::hasEditedNotes() const
{
    const juce::ScopedLock sl (resultLock);
    return edited;
}

std::shared_ptr<const FourEverHumProcessor::Take> FourEverHumProcessor::getLastTake() const
{
    const juce::ScopedLock sl (resultLock);
    return lastTake;
}

// Con el proyecto del DAW se guardan las perillas y las notas detectadas (no el
// audio de la toma, para no inflar el proyecto).
void FourEverHumProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    juce::ValueTree tree ("4EVERHUM");
    tree.setProperty ("version", 2, nullptr);
    tree.setProperty ("bpm", resultBpm.load(), nullptr);
    tree.setProperty ("hostPpq", resultHostPpq.load(), nullptr);
    tree.setProperty ("beatsPerBar", resultBeatsPerBar.load(), nullptr);
    tree.appendChild (params.copyState(), nullptr);

    std::vector<fourever::NoteEvent> raw;
    std::vector<fourever::ClipNote> manual;
    bool isEdited = false;
    {
        const juce::ScopedLock sl (resultLock);
        raw      = notes;
        manual   = editedNotes;
        isEdited = edited;
    }

    if (isEdited)
    {
        juce::ValueTree ed ("EDITED");
        for (const auto& n : manual)
        {
            juce::ValueTree c ("CNOTE");
            c.setProperty ("start", n.startBeat, nullptr);
            c.setProperty ("len",   n.lengthBeat, nullptr);
            c.setProperty ("pitch", n.midiNote, nullptr);
            c.setProperty ("vel",   n.velocity, nullptr);
            ed.appendChild (c, nullptr);
        }
        tree.appendChild (ed, nullptr);
    }

    for (const auto& n : raw)
    {
        juce::ValueTree nt ("NOTE");
        nt.setProperty ("start", n.startSec, nullptr);
        nt.setProperty ("end",   n.endSec,   nullptr);
        nt.setProperty ("pitch", n.midiNote, nullptr);
        nt.setProperty ("pitchExact", n.pitch, nullptr);
        nt.setProperty ("vel",   n.velocity, nullptr);
        tree.appendChild (nt, nullptr);
    }

    juce::MemoryOutputStream out (destData, false);
    tree.writeToStream (out);
}

void FourEverHumProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    const auto tree = juce::ValueTree::readFromData (data, (size_t) sizeInBytes);
    if (! tree.hasType ("4EVERHUM"))
        return;

    resultBpm.store ((double) tree.getProperty ("bpm", 120.0));
    resultHostPpq.store ((double) tree.getProperty ("hostPpq", -1.0));
    resultBeatsPerBar.store ((int) tree.getProperty ("beatsPerBar", 4));

    const auto paramState = tree.getChildWithName (params.state.getType());
    if (paramState.isValid())
        params.replaceState (paramState);

    std::vector<fourever::NoteEvent> loaded;
    for (const auto& nt : tree)
    {
        if (! nt.hasType ("NOTE"))
            continue;
        fourever::NoteEvent n;
        n.startSec = nt.getProperty ("start");
        n.endSec   = nt.getProperty ("end");
        n.midiNote = nt.getProperty ("pitch");
        n.pitch    = (float) (double) nt.getProperty ("pitchExact", (double) n.midiNote);
        n.velocity = (float) (double) nt.getProperty ("vel");
        loaded.push_back (n);
    }

    std::vector<fourever::ClipNote> manual;
    const auto ed = tree.getChildWithName ("EDITED");
    for (const auto& c : ed)
    {
        fourever::ClipNote n;
        n.startBeat  = c.getProperty ("start");
        n.lengthBeat = c.getProperty ("len");
        n.midiNote   = c.getProperty ("pitch");
        n.velocity   = (float) (double) c.getProperty ("vel", 0.8);
        manual.push_back (n);
    }

    {
        const juce::ScopedLock sl (resultLock);
        notes       = std::move (loaded);
        editedNotes = std::move (manual);
        edited      = ed.isValid();
        lastTake    = nullptr;
    }
    ++resultVersion;

    const auto s = state.load();
    if (s == State::idle || s == State::ready)
        state.store (getClip().notes.empty() ? State::idle : State::ready);
}

juce::AudioProcessorEditor* FourEverHumProcessor::createEditor()
{
    return new FourEverHumEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new FourEverHumProcessor();
}
