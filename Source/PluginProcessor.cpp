#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
const juce::StringArray octaveChoices { "-2", "-1", "0", "+1", "+2" };
constexpr int octaveZeroIndex = 2;

const juce::StringArray settingIds { paramIds::sensitivity, paramIds::minNote,
                                     paramIds::stability, paramIds::octave, paramIds::autoGate };
} // namespace

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

    for (const auto& id : settingIds)
        params.addParameterListener (id, this);

    startTimerHz (20);
}

FourEverHumProcessor::~FourEverHumProcessor()
{
    stopTimer();
    for (const auto& id : settingIds)
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

    // Tempo del DAW (para exportar el MIDI alineado al proyecto).
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
            if (auto bpm = pos->getBpm())
                hostBpm.store (*bpm);

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

    {
        const juce::ScopedLock sl (resultLock);
        lastTake = take;
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
        ++resultVersion;
        state.store (State::ready);
    });
}

void FourEverHumProcessor::parameterChanged (const juce::String& id, float)
{
    // Puede llamarse desde el audio thread (automatizacion): solo atomics.
    if (id == paramIds::octave)
    {
        ++resultVersion; // la octava no requiere re-analizar
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

std::vector<fourever::NoteEvent> FourEverHumProcessor::getNotes() const
{
    std::vector<fourever::NoteEvent> copy;
    {
        const juce::ScopedLock sl (resultLock);
        copy = notes;
    }
    return fourever::transposeOctaves (std::move (copy), octaveShift());
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
    tree.appendChild (params.copyState(), nullptr);

    std::vector<fourever::NoteEvent> raw;
    {
        const juce::ScopedLock sl (resultLock);
        raw = notes;
    }

    for (const auto& n : raw)
    {
        juce::ValueTree nt ("NOTE");
        nt.setProperty ("start", n.startSec, nullptr);
        nt.setProperty ("end",   n.endSec,   nullptr);
        nt.setProperty ("pitch", n.midiNote, nullptr);
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
        n.velocity = (float) (double) nt.getProperty ("vel");
        loaded.push_back (n);
    }

    {
        const juce::ScopedLock sl (resultLock);
        notes    = std::move (loaded);
        lastTake = nullptr;
    }
    ++resultVersion;

    const auto s = state.load();
    if (s == State::idle || s == State::ready)
        state.store (getNotes().empty() ? State::idle : State::ready);
}

juce::AudioProcessorEditor* FourEverHumProcessor::createEditor()
{
    return new FourEverHumEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new FourEverHumProcessor();
}
