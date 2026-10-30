// 4EVERHUM - tararea -> MIDI
// Copyright (C) 2026 sosa13m / colectivo 4EVER
// SPDX-License-Identifier: AGPL-3.0-or-later
// Este programa es software libre: ver el archivo LICENSE.

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
// --- pYIN ----------------------------------------------------------------------

struct Candidate
{
    double f0;
    double prob;
};

// Prior Beta(2, 18) sobre los umbrales 0.01..1.00 (media 0.1), como en el paper.
const std::vector<double>& thresholdPrior()
{
    static const std::vector<double> prior = []
    {
        std::vector<double> p (100);
        double sum = 0.0;
        for (int k = 0; k < 100; ++k)
        {
            const double t = 0.01 * (k + 1);
            p[(std::size_t) k] = t * std::pow (1.0 - t, 17.0); // t^(a-1) (1-t)^(b-1)
            sum += p[(std::size_t) k];
        }
        for (auto& v : p) v /= sum;
        return p;
    }();
    return prior;
}

// Candidatos de altura de un frame: cada umbral del prior "vota" por el primer
// valle de la CMNDF que queda bajo el. Suma de probabilidades = prob. de voz.
std::vector<Candidate> pyinCandidates (const std::vector<double>& cmndf, int tauMin, int tauMax, double rate)
{
    std::vector<int> troughs;
    for (int tau = std::max (tauMin, 1); tau < tauMax; ++tau)
        if (cmndf[(std::size_t) tau] < cmndf[(std::size_t) tau - 1] && cmndf[(std::size_t) tau] <= cmndf[(std::size_t) tau + 1])
            troughs.push_back (tau);
    if (troughs.empty())
        return {};

    const int globalMin = *std::min_element (troughs.begin(), troughs.end(), [&] (int a, int b)
                                             { return cmndf[(std::size_t) a] < cmndf[(std::size_t) b]; });

    std::vector<double> mass (troughs.size(), 0.0);
    const auto& prior = thresholdPrior();
    for (int k = 0; k < 100; ++k)
    {
        const double thr = 0.01 * (k + 1);
        bool found = false;
        for (std::size_t i = 0; i < troughs.size(); ++i)
            if (cmndf[(std::size_t) troughs[i]] < thr) { mass[i] += prior[(std::size_t) k]; found = true; break; }
        if (! found) // ningun valle bajo este umbral: voto debil al minimo global
            for (std::size_t i = 0; i < troughs.size(); ++i)
                if (troughs[i] == globalMin) mass[i] += prior[(std::size_t) k] * 0.01;
    }

    std::vector<Candidate> out;
    for (std::size_t i = 0; i < troughs.size(); ++i)
    {
        if (mass[i] <= 0.0)
            continue;
        const int tau = troughs[i];
        double refined = tau;
        const double a = cmndf[(std::size_t) tau - 1], b = cmndf[(std::size_t) tau], c = cmndf[(std::size_t) tau + 1];
        const double denom = a - 2.0 * b + c;
        if (std::abs (denom) > 1.0e-12)
            refined += 0.5 * (a - c) / denom;
        out.push_back ({ rate / refined, mass[i] });
    }
    return out;
}

// Viterbi sobre estados (voz/sin voz) x (altura en bins de 20 cents).
void pyinDecode (std::vector<PitchFrame>& frames, const std::vector<std::vector<Candidate>>& cands,
                 float minF0, float maxF0, double hopSec)
{
    const int T = (int) frames.size();
    if (T == 0)
        return;

    constexpr double centsPerBin = 20.0;
    const int nBins = (int) std::ceil (1200.0 * std::log2 ((double) maxF0 / minF0) / centsPerBin) + 1;
    auto binOf = [&] (double f0) { return std::clamp ((int) std::lround (1200.0 * std::log2 (f0 / minF0) / centsPerBin), 0, nBins - 1); };
    auto freqOf = [&] (int bin) { return (double) minF0 * std::pow (2.0, bin * centsPerBin / 1200.0); };

    // Salto maximo de altura entre frames: ~36 octavas/s (valor de librosa).
    const int maxStep = std::max (1, (int) std::lround (35.92 * 1200.0 * hopSec / centsPerBin));
    std::vector<double> stepLog ((std::size_t) maxStep + 1);
    double norm = 0.0;
    for (int d = -maxStep; d <= maxStep; ++d)
        norm += (double) (maxStep + 1 - std::abs (d));
    for (int d = 0; d <= maxStep; ++d)
        stepLog[(std::size_t) d] = std::log ((double) (maxStep + 1 - d) / norm);

    const double switchProb = 0.01;
    const double logStay = std::log (1.0 - switchProb), logSwitch = std::log (switchProb);
    constexpr double eps = 1.0e-12;
    const int S = 2 * nBins; // [0, nBins) con voz, [nBins, 2 nBins) sin voz

    std::vector<double> obs ((std::size_t) S), prev ((std::size_t) S), cur ((std::size_t) S);
    std::vector<int> back ((std::size_t) T * (std::size_t) S);

    auto fillObs = [&] (int t)
    {
        double voiced = 0.0;
        std::fill (obs.begin(), obs.begin() + nBins, 0.0);
        for (const auto& c : cands[(std::size_t) t])
        {
            obs[(std::size_t) binOf (c.f0)] += c.prob;
            voiced += c.prob;
        }
        voiced = std::min (voiced, 1.0);
        const double unvoiced = (1.0 - voiced) / nBins;
        for (int b = 0; b < nBins; ++b)
        {
            obs[(std::size_t) b]         = std::log (obs[(std::size_t) b] + eps);
            obs[(std::size_t) (nBins + b)] = std::log (unvoiced + eps);
        }
    };

    fillObs (0);
    for (int st = 0; st < S; ++st)
        prev[(std::size_t) st] = obs[(std::size_t) st] - std::log ((double) S);

    for (int t = 1; t < T; ++t)
    {
        fillObs (t);
        for (int to = 0; to < S; ++to)
        {
            const int toBin = to % nBins;
            const bool toVoiced = to < nBins;
            double best = -1.0e300;
            int arg = 0;
            for (int fromVoicing = 0; fromVoicing < 2; ++fromVoicing)
            {
                const double sw = ((fromVoicing == 0) == toVoiced) ? logStay : logSwitch;
                const int lo = std::max (0, toBin - maxStep), hi = std::min (nBins - 1, toBin + maxStep);
                for (int fb = lo; fb <= hi; ++fb)
                {
                    const int from = fromVoicing * nBins + fb;
                    const double v = prev[(std::size_t) from] + stepLog[(std::size_t) std::abs (fb - toBin)] + sw;
                    if (v > best) { best = v; arg = from; }
                }
            }
            cur[(std::size_t) to] = best + obs[(std::size_t) to];
            back[(std::size_t) t * (std::size_t) S + (std::size_t) to] = arg;
        }
        std::swap (prev, cur);
    }

    int state = (int) std::distance (prev.begin(), std::max_element (prev.begin(), prev.end()));
    for (int t = T - 1; t >= 0; --t)
    {
        auto& f = frames[(std::size_t) t];
        if (state < nBins)
        {
            // Con voz: el candidato real mas cercano al bin elegido (precision fina);
            // si no hay ninguno cerca, el centro del bin.
            double f0 = freqOf (state), bestDist = 1.5;
            for (const auto& c : cands[(std::size_t) t])
            {
                const double dist = std::abs (1200.0 * std::log2 (c.f0 / freqOf (state))) / centsPerBin;
                if (dist <= bestDist) { bestDist = dist; f0 = c.f0; }
            }
            f.f0Hz = (float) f0;
            f.confidence = 1.0f;
        }
        else
        {
            f.f0Hz = 0.0f;
            f.confidence = 0.0f;
        }
        if (t > 0)
            state = back[(std::size_t) t * (std::size_t) S + (std::size_t) state];
    }
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
    const bool usePyin = s.method == PitchMethod::pyin;
    std::vector<std::vector<Candidate>> candidates;
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
            if (usePyin)
                candidates.emplace_back();
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

        if (usePyin)
        {
            candidates.push_back (pyinCandidates (cmndf, tauMin, tauMax, rate));
            frames.push_back (frame);
            continue;
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

    if (usePyin)
        pyinDecode (frames, candidates, s.minF0Hz, s.maxF0Hz, hop / rate);

    return frames;
}

} // namespace fourever
