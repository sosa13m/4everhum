#pragma once

#include "dsp/Analyzer.h"
#include "dsp/Music.h"

#include <juce_audio_processors/juce_audio_processors.h>

#include <atomic>
#include <memory>
#include <vector>

namespace paramIds
{
inline constexpr auto sensitivity = "sensitivity";
inline constexpr auto minNote     = "minNote";
inline constexpr auto stability   = "stability";
inline constexpr auto octave      = "octave";
inline constexpr auto autoGate    = "autoGate";
inline constexpr auto keyRoot     = "keyRoot";
inline constexpr auto scale       = "scale";
inline constexpr auto quantize    = "quantize";
inline constexpr auto strength    = "strength";
} // namespace paramIds

// 4EVERHUM: efecto de audio que se pone en el track del mixer donde entra el
// microfono. Graba el tarareo, lo analiza en un hilo de fondo y deja las notas
// listas para arrastrarlas como .mid al piano roll.
class FourEverHumProcessor final : public juce::AudioProcessor,
                                   private juce::AudioProcessorValueTreeState::Listener,
                                   private juce::Timer
{
public:
    enum class State { idle, recording, analyzing, ready };

    // Una toma grabada (mono). Inmutable: se comparte entre hilos sin copiar.
    struct Take
    {
        std::vector<float> samples;
        double sampleRate = 44100.0;
        double bpm        = 120.0; // tempo del proyecto al grabar
        double hostPpq    = -1.0;  // negra del proyecto donde empezo la toma (<0 = FL detenido)
        int    beatsPerBar = 4;
    };

    FourEverHumProcessor();
    ~FourEverHumProcessor() override;

    // --- AudioProcessor -----------------------------------------------------
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override  { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    // --- API para el editor (hilo de mensajes) ------------------------------
    void startRecording();
    void stopRecording(); // lanza el analisis automaticamente

    State  getState() const              { return state.load(); }
    bool   isCapturing() const           { return recording.load(); } // false si el buffer se lleno
    double getRecordedSeconds() const;
    double getMaxRecordSeconds() const   { return maxRecordSeconds; }
    float  getInputLevel() const         { return inputLevel.load(); }
    double getHostBpm() const            { return hostBpm.load(); }
    double getResultBpm() const          { return resultBpm.load(); } // tempo al momento de grabar
    int    getResultBeatsPerBar() const  { return resultBeatsPerBar.load(); }

    // Cambia cuando hay notas nuevas o cuando cambia la octava.
    int    getResultVersion() const      { return resultVersion.load(); }

    // La melodia final, tal como se ve y se arrastra: en negras, con escala,
    // grid y octava aplicados.
    fourever::Clip getClip() const;

    // Ultima toma (nullptr si no hay: p. ej. tras reabrir el proyecto).
    std::shared_ptr<const Take> getLastTake() const;

    fourever::UserSettings currentSettings() const;
    fourever::ClipSettings currentClipSettings() const;

    // Opciones de los parametros de eleccion (las usa el editor para sus combos).
    static const juce::StringArray& keyChoices();
    static const juce::StringArray& scaleChoices();
    static const juce::StringArray& quantizeChoices();

    juce::AudioProcessorValueTreeState params;

private:
    static constexpr double maxRecordSeconds = 120.0;
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    void parameterChanged (const juce::String& id, float newValue) override;
    void timerCallback() override;
    void launchAnalysis (std::shared_ptr<const Take> take);
    int  octaveShift() const;

    // Buffer de grabacion preasignado: el audio thread solo escribe, sin
    // reservar memoria ni bloquear.
    std::vector<float>   recordBuffer;
    std::atomic<int>     writePos { 0 };
    std::atomic<bool>    recording { false };
    std::atomic<State>   state { State::idle };
    std::atomic<float>   inputLevel { 0.0f };
    std::atomic<double>  hostBpm { 120.0 };
    double               currentSampleRate = 44100.0;

    std::atomic<float>* sensitivityParam = nullptr;
    std::atomic<float>* minNoteParam     = nullptr;
    std::atomic<float>* stabilityParam   = nullptr;
    std::atomic<float>* octaveParam      = nullptr;
    std::atomic<float>* autoGateParam    = nullptr;
    std::atomic<float>* keyRootParam     = nullptr;
    std::atomic<float>* scaleParam       = nullptr;
    std::atomic<float>* quantizeParam    = nullptr;
    std::atomic<float>* strengthParam    = nullptr;

    // Posicion del proyecto al empezar a grabar (la captura el audio thread en
    // el primer bloque de la toma).
    std::atomic<bool>   needStartPosition { false };
    std::atomic<double> takeStartPpq { -1.0 };
    std::atomic<int>    takeBeatsPerBar { 4 };

    // Re-analisis con "debounce": mientras giras una perilla no se analiza en
    // cada paso, sino un instante despues de soltarla.
    std::atomic<bool>    settingsDirty { false };
    std::atomic<juce::uint32> lastSettingsChangeMs { 0 };

    juce::ThreadPool     analysisPool { juce::ThreadPoolOptions{}.withThreadName ("4EVERHUM analysis")
                                                                 .withNumberOfThreads (1) };
    mutable juce::CriticalSection resultLock;
    std::vector<fourever::NoteEvent> notes;   // sin transponer
    std::shared_ptr<const Take>      lastTake;
    std::atomic<int>     resultVersion { 0 };
    std::atomic<double>  resultBpm { 120.0 };
    std::atomic<double>  resultHostPpq { -1.0 };
    std::atomic<int>     resultBeatsPerBar { 4 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FourEverHumProcessor)
};
