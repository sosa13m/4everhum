#include "dsp/NoteSegmenter.h"

#include <algorithm>
#include <cmath>

namespace fourever
{
namespace
{
// Centinela "sin voz". No usamos NaN: con optimizaciones agresivas de punto
// flotante los chequeos de NaN pueden desaparecer.
constexpr float noPitch = -1000.0f;
inline bool isUnvoiced (float m) { return m <= noPitch; }

float hzToMidi (float hz) { return 69.0f + 12.0f * std::log2 (hz / 440.0f); }

float median (std::vector<float> v)
{
    if (v.empty())
        return noPitch;
    const auto mid = v.begin() + (std::ptrdiff_t) (v.size() / 2);
    std::nth_element (v.begin(), mid, v.end());
    return *mid;
}

struct Segment
{
    int start = 0; // indice de frame (inclusive)
    int end   = 0; // indice de frame (exclusive)
    int note  = -1;
};
} // namespace

std::vector<NoteEvent> segmentNotes (const std::vector<PitchFrame>& frames,
                                     const SegmenterSettings& s)
{
    std::vector<NoteEvent> notes;
    const int n = (int) frames.size();
    if (n < 2)
        return notes;

    const double dt = frames[1].timeSec - frames[0].timeSec;

    // 1) Pitch continuo en semitonos MIDI (NaN = sin voz).
    std::vector<float> raw ((std::size_t) n, noPitch);
    for (int i = 0; i < n; ++i)
        if (frames[(std::size_t) i].f0Hz > 0.0f && frames[(std::size_t) i].confidence >= s.minConfidence)
            raw[(std::size_t) i] = hzToMidi (frames[(std::size_t) i].f0Hz);

    // 1b) Rellenar microcortes (<= 2 frames) dentro de una nota sostenida, para
    //     que un frame "perdido" no parta una nota larga en dos repetidas.
    for (int i = 1; i < n - 1; ++i)
    {
        if (! isUnvoiced (raw[(std::size_t) i]) || isUnvoiced (raw[(std::size_t) i - 1]))
            continue;
        int j = i;
        while (j < n && j - i < 3 && isUnvoiced (raw[(std::size_t) j]))
            ++j;
        if (j < n && j - i <= 2 && ! isUnvoiced (raw[(std::size_t) j]))
            for (int k = i; k < j; ++k)
                raw[(std::size_t) k] = raw[(std::size_t) i - 1]
                                       + (raw[(std::size_t) j] - raw[(std::size_t) i - 1]) * (float) (k - i + 1) / (float) (j - i + 1);
        i = j - 1;
    }

    // 2) Filtro de mediana solo sobre frames con voz.
    std::vector<float> smooth ((std::size_t) n, noPitch);
    const int half = std::max (0, s.medianFrames / 2);
    for (int i = 0; i < n; ++i)
    {
        if (isUnvoiced (raw[(std::size_t) i]))
            continue;
        std::vector<float> win;
        for (int k = std::max (0, i - half); k <= std::min (n - 1, i + half); ++k)
            if (! isUnvoiced (raw[(std::size_t) k]))
                win.push_back (raw[(std::size_t) k]);
        smooth[(std::size_t) i] = median (std::move (win));
    }

    // 3) Cuantizacion con histeresis: la nota no cambia hasta que el pitch
    //    se aleja lo suficiente (evita el parpadeo entre dos semitonos).
    std::vector<int> q ((std::size_t) n, -1);
    int current = -1;
    for (int i = 0; i < n; ++i)
    {
        const float m = smooth[(std::size_t) i];
        if (isUnvoiced (m))
        {
            current = -1;
        }
        else if (current < 0 || std::abs (m - (float) current) > s.hysteresis)
        {
            current = (int) std::lround (m);
        }
        q[(std::size_t) i] = current;
    }

    // 4) Run-length en segmentos.
    std::vector<Segment> segs;
    for (int i = 0; i < n; ++i)
    {
        if (q[(std::size_t) i] < 0)
            continue;
        if (! segs.empty() && segs.back().end == i && segs.back().note == q[(std::size_t) i])
            segs.back().end = i + 1;
        else
            segs.push_back ({ i, i + 1, q[(std::size_t) i] });
    }

    // 5) Notas demasiado cortas: si estan pegadas a otra nota se absorben en la
    //    vecina mas larga (tipico de un portamento); si estan aisladas, se borran.
    const int minFrames = std::max (1, (int) std::ceil (s.minNoteSec / dt));
    for (;;)
    {
        int shortest = -1;
        for (int i = 0; i < (int) segs.size(); ++i)
            if (segs[(std::size_t) i].end - segs[(std::size_t) i].start < minFrames
                && (shortest < 0 || segs[(std::size_t) i].end - segs[(std::size_t) i].start
                                        < segs[(std::size_t) shortest].end - segs[(std::size_t) shortest].start))
                shortest = i;

        if (shortest < 0)
            break;

        auto& seg = segs[(std::size_t) shortest];
        const bool touchPrev = shortest > 0 && segs[(std::size_t) shortest - 1].end == seg.start;
        const bool touchNext = shortest + 1 < (int) segs.size() && segs[(std::size_t) shortest + 1].start == seg.end;

        if (touchPrev || touchNext)
        {
            const int lenPrev = touchPrev ? segs[(std::size_t) shortest - 1].end - segs[(std::size_t) shortest - 1].start : -1;
            const int lenNext = touchNext ? segs[(std::size_t) shortest + 1].end - segs[(std::size_t) shortest + 1].start : -1;
            if (lenPrev >= lenNext)
                segs[(std::size_t) shortest - 1].end = seg.end;
            else
                segs[(std::size_t) shortest + 1].start = seg.start;
        }
        segs.erase (segs.begin() + shortest);
    }

    // 6) Pitch final de cada nota = mediana del pitch suavizado dentro de ella,
    //    y se fusionan notas contiguas que terminaron en la misma altura.
    for (auto& seg : segs)
    {
        std::vector<float> v;
        for (int i = seg.start; i < seg.end; ++i)
            if (! isUnvoiced (smooth[(std::size_t) i]))
                v.push_back (smooth[(std::size_t) i]);
        if (! v.empty())
            seg.note = (int) std::lround (median (std::move (v)));
    }

    std::vector<Segment> merged;
    for (const auto& seg : segs)
    {
        if (! merged.empty() && merged.back().end == seg.start && merged.back().note == seg.note)
            merged.back().end = seg.end;
        else
            merged.push_back (seg);
    }

    // 6b) En notas ligadas (sin silencio entre medio) la histeresis retrasa el
    //     cambio. Movemos la frontera al punto donde el pitch ya esta mas cerca
    //     de la nota nueva que de la anterior (= mitad del portamento).
    for (std::size_t i = 0; i + 1 < merged.size(); ++i)
    {
        auto& a = merged[i];
        auto& b = merged[i + 1];
        if (a.end != b.start || a.note == b.note)
            continue;

        auto closerToB = [&] (int frame)
        {
            const float m = smooth[(std::size_t) frame];
            return ! isUnvoiced (m) && std::abs (m - (float) b.note) < std::abs (m - (float) a.note);
        };

        int boundary = a.end;
        while (boundary - 1 > a.start && closerToB (boundary - 1))
            --boundary;
        while (boundary < b.end - 1 && ! closerToB (boundary))
            ++boundary;

        a.end   = boundary;
        b.start = boundary;
    }

    // 7) A tiempos y velocity (energia media -> 0.3..1.0).
    for (const auto& seg : merged)
    {
        double db = 0.0;
        for (int i = seg.start; i < seg.end; ++i)
            db += frames[(std::size_t) i].rmsDb;
        db /= (seg.end - seg.start);

        NoteEvent ev;
        ev.startSec = std::max (0.0, frames[(std::size_t) seg.start].timeSec - dt * 0.5);
        ev.endSec   = frames[(std::size_t) seg.end - 1].timeSec + dt * 0.5;
        ev.midiNote = std::clamp (seg.note, 0, 127);
        ev.velocity = (float) std::clamp (0.3 + 0.7 * (db + 45.0) / 39.0, 0.3, 1.0);
        notes.push_back (ev);
    }

    return notes;
}

} // namespace fourever
