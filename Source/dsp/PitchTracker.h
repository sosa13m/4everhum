#pragma once

#include <cstddef>
#include <vector>

namespace fourever
{

// Un frame de analisis (~10 ms) de la voz grabada.
struct PitchFrame
{
    double timeSec    = 0.0;  // centro del frame, relativo al inicio de la grabacion
    float  f0Hz       = 0.0f; // frecuencia fundamental; 0 = sin voz / silencio
    float  confidence = 0.0f; // 0..1 (1 - minimo de la CMNDF de YIN)
    float  rmsDb      = -120.0f;
};

enum class PitchMethod
{
    yin,  // YIN clasico: rapido, decide frame a frame
    pyin  // pYIN (Mauch & Dixon 2014): varios candidatos por frame + HMM/Viterbi;
          // sigue la melodia en el tiempo y evita saltos de octava
};

struct PitchTrackerSettings
{
    PitchMethod method  = PitchMethod::yin;
    double targetRate   = 16000.0; // la voz se decima a ~16 kHz antes de analizar
    float  minF0Hz      = 65.0f;   // ~C2, voces graves
    float  maxF0Hz      = 1100.0f; // ~C6, silbidos/falsete
    float  yinThreshold = 0.15f;   // umbral absoluto de YIN (mas bajo = mas estricto)
    float  silenceDb    = -45.0f;  // por debajo de esto se considera silencio
    bool   autoGate     = true;    // subir el umbral si el ruido de fondo es alto
    float  autoGateMarginDb  = 10.0f;  // umbral = ruido de fondo + margen...
    float  autoGateCeilingDb = -30.0f; // ...pero nunca mas alto que esto
    double hopSec       = 0.010;   // un frame cada 10 ms
};

// Umbral de silencio efectivo para una toma: el de los ajustes o, con autoGate,
// el ruido de fondo medido (percentil 10 de la energia) + margen.
float effectiveSilenceDb (const std::vector<float>& frameRmsDb, const PitchTrackerSettings& settings);

// Deteccion de pitch monofonica con YIN (de Cheveigne & Kawahara, 2002) o pYIN.
// Pensado para correr offline en un hilo de fondo, nunca en el audio thread.
std::vector<PitchFrame> trackPitch (const float* samples,
                                    std::size_t numSamples,
                                    double sampleRate,
                                    const PitchTrackerSettings& settings = {});

} // namespace fourever
