#include "dsp/PitchTracker.h"

#include <algorithm>
#include <cmath>

namespace fourever
{
namespace
{
constexpr double pi = 3.14159265358979323846;

// Biquad pasa-bajos (RBJ cookbook), forma directa II transpuesta.
struct Biquad
{
    double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0;
    double z1 = 0, z2 = 0;

    static Biquad lowpass (double sampleRate, double cutoff, double q)
    {
        const double w0    = 2.0 * pi * cutoff / sampleRate;
        const double cosw  = std::cos (w0);
        const double alpha = std::sin (w0) / (2.0 * q);
        const double a0    = 1.0 + alpha;

        Biquad f;
        f.b0 = (1.0 - cosw) * 0.5 / a0;
        f.b1 = (1.0 - cosw) / a0;
        f.b2 = f.b0;
        f.a1 = -2.0 * cosw / a0;
        f.a2 = (1.0 - alpha) / a0;
        return f;
    }

    double process (double x)
    {
        const double y = b0 * x + z1;
        z1 = b1 * x - a1 * y + z2;
        z2 = b2 * x - a2 * y;
        return y;
    }
};

// Filtra (Butterworth de 4o orden) y decima por un factor entero.
std::vector<float> decimate (const float* in, std::size_t n, double sampleRate,
                             int factor, double cutoff)
{
    if (factor <= 1)
        return { in, in + n };

    // 4o orden Butterworth = dos biquads con estos Q.
    auto s1 = Biquad::lowpass (sampleRate, cutoff, 0.54119610);
    auto s2 = Biquad::lowpass (sampleRate, cutoff, 1.30656296);

    std::vector<float> out;
    out.reserve (n / (std::size_t) factor + 1);

    for (std::size_t i = 0; i < n; ++i)
    {
        const double y = s2.process (s1.process (in[i]));
        if (i % (std::size_t) factor == 0)
            out.push_back ((float) y);
    }
    return out;
}
} // namespace

float effectiveSilenceDb (const std::vector<float>& frameRmsDb, const PitchTrackerSettings& s)
{
    if (! s.autoGate || frameRmsDb.size() < 10)
        return s.silenceDb;

    // El percentil 10 de la energia es una buena estimacion del ruido de fondo
    // (microfono, ventilador, beat filtrandose por los audifonos...).
    std::vector<float> sorted (frameRmsDb);
    const auto p10 = sorted.begin() + (std::ptrdiff_t) (sorted.size() / 10);
    std::nth_element (sorted.begin(), p10, sorted.end());

    const float adaptive = std::min (*p10 + s.autoGateMarginDb, s.autoGateCeilingDb);
    return std::max (s.silenceDb, adaptive);
}

std::vector<PitchFrame> trackPitch (const float* samples, std::size_t numSamples,
                                    double sampleRate, const PitchTrackerSettings& s)
{
    std::vector<PitchFrame> frames;
    if (samples == nullptr || numSamples == 0 || sampleRate <= 0.0)
        return frames;

    // 1) Decimar: la fundamental de la voz esta bajo ~1 kHz, analizar a 48 kHz
    //    es desperdiciar CPU. 48k -> 16k, 44.1k -> 22.05k, 96k -> 16k.
    const int    factor = std::max (1, (int) std::floor (sampleRate / s.targetRate));
    const double rate   = sampleRate / factor;
    const double cutoff = std::min (4000.0, 0.4 * rate);
    const auto   x      = decimate (samples, numSamples, sampleRate, factor, cutoff);

    // 2) Tamanos de ventana: integramos sobre un periodo de la nota mas grave.
    const int tauMin = std::max (2, (int) std::floor (rate / s.maxF0Hz));
    const int tauMax = (int) std::ceil (rate / s.minF0Hz);
    const int integ  = tauMax;
    const int frameLen = integ + tauMax + 2;
    const int hop      = std::max (1, (int) std::lround (s.hopSec * rate));

    if ((int) x.size() < frameLen)
        return frames;

    std::vector<double> d ((std::size_t) tauMax + 1);
    std::vector<double> cmndf ((std::size_t) tauMax + 1);

    // Primera pasada: energia de cada frame, para estimar el ruido de fondo.
    std::vector<float> rmsDb;
    for (int start = 0; start + frameLen <= (int) x.size(); start += hop)
    {
        double energy = 0.0;
        for (int j = 0; j < integ; ++j)
            energy += (double) x[(std::size_t) (start + j)] * x[(std::size_t) (start + j)];
        const double rms = std::sqrt (energy / integ);
        rmsDb.push_back ((float) (20.0 * std::log10 (std::max (rms, 1.0e-9))));
    }
    const float gateDb = effectiveSilenceDb (rmsDb, s);

    // Segunda pasada: YIN en los frames que superan el umbral.
    std::size_t frameIndex = 0;
    for (int start = 0; start + frameLen <= (int) x.size(); start += hop, ++frameIndex)
    {
        const float* w = x.data() + start;

        PitchFrame frame;
        frame.timeSec = (start + frameLen * 0.5) / rate;
        frame.rmsDb   = rmsDb[frameIndex];

        if (frame.rmsDb < gateDb)
        {
            frames.push_back (frame);
            continue;
        }

        // 3) Funcion diferencia d(tau).
        d[0] = 0.0;
        for (int tau = 1; tau <= tauMax; ++tau)
        {
            double sum = 0.0;
            for (int j = 0; j < integ; ++j)
            {
                const double diff = (double) w[j] - (double) w[j + tau];
                sum += diff * diff;
            }
            d[(std::size_t) tau] = sum;
        }

        // 4) Diferencia normalizada acumulada (CMNDF).
        cmndf[0] = 1.0;
        double running = 0.0;
        for (int tau = 1; tau <= tauMax; ++tau)
        {
            running += d[(std::size_t) tau];
            cmndf[(std::size_t) tau] = running > 0.0 ? d[(std::size_t) tau] * tau / running : 1.0;
        }

        // 5) Primer minimo bajo el umbral absoluto.
        int bestTau = -1;
        for (int tau = tauMin; tau < tauMax; ++tau)
        {
            if (cmndf[(std::size_t) tau] < s.yinThreshold)
            {
                while (tau + 1 < tauMax && cmndf[(std::size_t) tau + 1] < cmndf[(std::size_t) tau])
                    ++tau;
                bestTau = tau;
                break;
            }
        }

        if (bestTau > 0)
        {
            // 6) Interpolacion parabolica para precision sub-muestra.
            double refined = bestTau;
            if (bestTau > 1 && bestTau < tauMax)
            {
                const double a = cmndf[(std::size_t) bestTau - 1];
                const double b = cmndf[(std::size_t) bestTau];
                const double c = cmndf[(std::size_t) bestTau + 1];
                const double denom = a - 2.0 * b + c;
                if (std::abs (denom) > 1.0e-12)
                    refined += 0.5 * (a - c) / denom;
            }

            frame.f0Hz       = (float) (rate / refined);
            frame.confidence = (float) std::clamp (1.0 - cmndf[(std::size_t) bestTau], 0.0, 1.0);
        }

        frames.push_back (frame);
    }

    return frames;
}

} // namespace fourever
