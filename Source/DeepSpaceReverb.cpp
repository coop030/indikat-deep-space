#include "DeepSpaceReverb.h"
#include <algorithm>

namespace
{
    constexpr double twoPi = 6.283185307179586;
    constexpr float pi = 3.14159265f;

    constexpr float had (int i, int j)
    {
        int x = i & j, b = 0;
        while (x != 0) { b ^= x & 1; x >>= 1; }
        return b ? -1.0f : 1.0f;
    }

    inline float hash (int i) { const float s = std::sin ((float) i * 12.9898f + 78.233f) * 43758.5453f; return s - std::floor (s); }

    inline float softClip (float x)
    {
        const float a = std::abs (x);
        if (a < 1.0f) return x;
        return std::copysign (1.0f + 0.5f * std::tanh ((a - 1.0f) * 2.0f), x);
    }

    constexpr float earlyMs[4]  { 3.7f, 5.9f, 9.1f, 13.7f };
    constexpr float attackMs[2] { 19.0f, 37.0f };
    constexpr float swellMs[4]  { 0.0f, 23.0f, 57.0f, 109.0f };
    constexpr float sideMul[2]  { 1.0f, 1.083f };
    constexpr int linePerm[16]  { 0, 8, 4, 12, 2, 10, 6, 14, 1, 9, 5, 13, 3, 11, 7, 15 };

    // early-reflection patterns (ms at Size 100 %), left / right
    constexpr float erRoom[2][12] = {
        { 4.3f, 7.9f, 11.2f, 14.9f, 19.1f, 23.7f, 28.9f, 33.1f, 38.3f, 44.9f, 51.1f, 58.3f },
        { 5.1f, 8.7f, 12.5f, 16.3f, 20.9f, 24.1f, 30.7f, 35.3f, 40.9f, 46.1f, 53.9f, 60.1f } };
    constexpr float erChamber[2][12] = {
        { 6.1f, 9.7f, 13.9f, 17.3f, 22.1f, 26.9f, 31.7f, 37.1f, 42.7f, 49.3f, 55.9f, 63.7f },
        { 6.9f, 10.9f, 14.3f, 18.7f, 23.9f, 27.7f, 33.9f, 38.9f, 45.1f, 50.7f, 57.9f, 66.1f } };
    constexpr float erHall[2][12] = {
        { 11.3f, 19.7f, 27.1f, 36.9f, 47.3f, 59.9f, 71.3f, 83.7f, 0, 0, 0, 0 },
        { 13.1f, 21.9f, 30.3f, 39.7f, 51.1f, 62.3f, 75.9f, 88.1f, 0, 0, 0, 0 } };

}

using M = DeepSpaceReverb::Mode;
using A = DeepSpaceReverb::Algo;

//                 name               algo         min   max    er   pat  apMs n  mod                     scale dirty bright atk   gain
const M DeepSpaceReverb::modes[] = {
    { "Concert Hall",    A::hall,   39.0f, 131.0f, 0.18f, 3, 6.1f, 1, DeepSpaceReverb::sine,       1.0f, false, 1.0f, 0.0f,  1.0f },
    { "Bright Hall",     A::hall,   34.0f, 117.0f, 0.22f, 3, 5.3f, 1, DeepSpaceReverb::sine,       1.0f, false, 1.7f, 0.0f,  0.86f },
    { "Plate",           A::hall,    9.0f,  47.0f, 0.0f,  0, 1.9f, 2, DeepSpaceReverb::sine,       0.8f, false, 1.4f, 0.0f,  1.29f },
    { "Room",            A::hall,    9.0f,  41.0f, 0.75f, 1, 2.9f, 2, DeepSpaceReverb::sine,       1.0f, false, 1.1f, 0.0f,  0.93f },
    { "Chamber",         A::hall,   17.0f,  67.0f, 0.55f, 2, 4.1f, 2, DeepSpaceReverb::sine,       0.7f, false, 1.0f, 0.0f,  1.0f },
    { "Random Space",    A::hall,   31.0f, 113.0f, 0.25f, 3, 5.7f, 1, DeepSpaceReverb::randomWalk, 2.4f, false, 1.0f, 0.0f,  1.0f },
    { "Chorus Space",    A::hall,   35.0f, 121.0f, 0.2f,  3, 6.1f, 1, DeepSpaceReverb::chorus,     3.5f, false, 1.0f, 0.0f,  1.0f },
    { "Ambience",        A::hall,    7.0f,  27.0f, 1.0f,  1, 2.3f, 2, DeepSpaceReverb::sine,       1.0f, false, 1.1f, 0.0f,  0.8f },
    { "Sanctuary",       A::hall,   61.0f, 197.0f, 0.12f, 3, 8.9f, 2, DeepSpaceReverb::sine,       0.8f, false, 0.9f, 0.35f, 0.92f },
    { "Dirty Hall",      A::hall,   39.0f, 131.0f, 0.18f, 3, 6.1f, 1, DeepSpaceReverb::randomWalk, 1.4f, true,  0.8f, 0.0f,  0.90f },
    { "Dirty Plate",     A::hall,    9.0f,  47.0f, 0.0f,  0, 1.9f, 2, DeepSpaceReverb::randomWalk, 1.2f, true,  1.0f, 0.0f,  1.23f },
    { "Smooth Plate",    A::hall,   13.0f,  61.0f, 0.0f,  0, 2.7f, 2, DeepSpaceReverb::sine,       1.0f, false, 1.2f, 0.3f,  1.24f },
    { "Smooth Room",     A::hall,   11.0f,  47.0f, 0.45f, 1, 3.3f, 2, DeepSpaceReverb::sine,       1.0f, false, 0.9f, 0.2f,  1.29f },
    { "Smooth Random",   A::hall,   33.0f, 119.0f, 0.15f, 3, 6.7f, 2, DeepSpaceReverb::randomWalk, 1.6f, false, 1.0f, 0.25f, 1.0f },
    { "Nonlin",          A::nonlin,  0.0f,   0.0f, 0.0f,  0, 0.0f, 0, DeepSpaceReverb::sine,       0.0f, false, 1.0f, 0.0f,  0.39f },
    { "Chaotic Hall",    A::hall,   37.0f, 137.0f, 0.18f, 3, 6.1f, 1, DeepSpaceReverb::chaos,      2.0f, false, 1.0f, 0.0f,  1.0f },
    { "Chaotic Chamber", A::hall,   19.0f,  71.0f, 0.5f,  2, 4.1f, 2, DeepSpaceReverb::chaos,      2.0f, false, 1.0f, 0.0f,  1.0f },
};
const int DeepSpaceReverb::numModes = (int) (sizeof (modes) / sizeof (modes[0]));

//                                               bandwidth  tank LP  bits  mod jitter
const DeepSpaceReverb::Colour DeepSpaceReverb::colours[3] = {
    { "1970s", 8500.0f,  6200.0f,  12, 0.6f  },
    { "1980s", 12500.0f, 9800.0f,  0,  0.15f },
    { "Now",   21000.0f, 21000.0f, 0,  0.0f  },
};

//==============================================================================
void DeepSpaceReverb::Biquad::set (double nb0, double nb1, double nb2, double a0, double na1, double na2)
{
    b0 = (float) (nb0 / a0); b1 = (float) (nb1 / a0); b2 = (float) (nb2 / a0);
    a1 = (float) (na1 / a0); a2 = (float) (na2 / a0);
}

void DeepSpaceReverb::Biquad::lowShelf (double fs, double f, double dB)
{
    const double A_ = std::pow (10.0, dB / 40.0), w = twoPi * std::min (f, fs * 0.45) / fs;
    const double c = std::cos (w), al = std::sin (w) / 2.0 * std::sqrt (2.0), sa = 2.0 * std::sqrt (A_) * al;
    set (A_ * ((A_ + 1) - (A_ - 1) * c + sa), 2 * A_ * ((A_ - 1) - (A_ + 1) * c), A_ * ((A_ + 1) - (A_ - 1) * c - sa),
         (A_ + 1) + (A_ - 1) * c + sa, -2 * ((A_ - 1) + (A_ + 1) * c), (A_ + 1) + (A_ - 1) * c - sa);
}

void DeepSpaceReverb::Biquad::highShelf (double fs, double f, double dB)
{
    const double A_ = std::pow (10.0, dB / 40.0), w = twoPi * std::min (f, fs * 0.45) / fs;
    const double c = std::cos (w), al = std::sin (w) / 2.0 * std::sqrt (2.0), sa = 2.0 * std::sqrt (A_) * al;
    set (A_ * ((A_ + 1) + (A_ - 1) * c + sa), -2 * A_ * ((A_ - 1) + (A_ + 1) * c), A_ * ((A_ + 1) + (A_ - 1) * c - sa),
         (A_ + 1) - (A_ - 1) * c + sa, 2 * ((A_ - 1) - (A_ + 1) * c), (A_ + 1) - (A_ - 1) * c - sa);
}

void DeepSpaceReverb::Biquad::lowPass (double fs, double f, double q)
{
    if (f >= fs * 0.45) { set (1, 0, 0, 1, 0, 0); return; }
    const double w = twoPi * f / fs, c = std::cos (w), al = std::sin (w) / (2.0 * q);
    set ((1 - c) / 2, 1 - c, (1 - c) / 2, 1 + al, -2 * c, 1 - al);
}

float DeepSpaceReverb::Mod::next (ModType type, float jitter)
{
    phase += inc;
    if (phase > twoPi) phase -= twoPi;
    t += tInc;
    if (t >= 1.0f) { a = b; b = rnd(); t -= 1.0f; }
    const float walk = a + (b - a) * (0.5f - 0.5f * std::cos (pi * t));
    const float s = (float) std::sin (phase);
    switch (type)
    {
        case randomWalk: return walk;
        case chaos:      return 0.75f * walk + 0.35f * s;
        case sine: case chorus: default:         return s * (1.0f - 0.5f * jitter) + walk * jitter;
    }
}

//==============================================================================
void DeepSpaceReverb::prepare (double sampleRate)
{
    sr = sampleRate;
    smooth = (float) (1.0 - std::exp (-1.0 / (0.03 * sr)));
    const auto ms = [this] (double m) { return (int) std::ceil (m * 0.001 * sr); };

    for (int ch = 0; ch < 2; ++ch)
    {
        predelay[ch].init (ms (510.0));
        er[ch].init (ms (100.0));
        swell[ch].init (ms (130.0));
        nonlinBuf[ch].init (ms (1600.0));
        for (auto& ap : earlyAp[ch])  ap.d.init (ms (20.0));
        for (auto& ap : attackAp[ch]) ap.d.init (ms (60.0));
        for (auto& ap : nlAp[ch])     ap.d.init (ms (20.0));
    }
    for (int i = 0; i < numLines; ++i)
    {
        auto& l = lines[(size_t) i];
        l.delay.init (ms (260.0));
        for (auto& ap : l.ap) ap.d.init (ms (30.0));
        l.sign = (i / 2) % 2 ? -1.0f : 1.0f;
        l.outL = had (3, i) * 0.25f;
        l.outR = had (12, i) * 0.25f;
        l.mod.seed = 0x9e3779b9u * (uint32_t) (i + 1);
        l.mod.phase = hash (i) * twoPi;
        l.mod.a = l.mod.rnd(); l.mod.b = l.mod.rnd();
    }
    lastMode = -1;
    snapNext = true;
    reset();
}

void DeepSpaceReverb::reset()
{
    for (int ch = 0; ch < 2; ++ch)
    {
        predelay[ch].clear(); er[ch].clear(); swell[ch].clear(); nonlinBuf[ch].clear();
        for (auto& ap : earlyAp[ch])  ap.d.clear();
        for (auto& ap : attackAp[ch]) ap.d.clear();
        for (auto& ap : nlAp[ch])     ap.d.clear();
        for (auto& b : band[ch]) b.clear();
        for (auto& b : outBand[ch]) b.clear();
    }
    for (auto& l : lines) { l.delay.clear(); for (auto& ap : l.ap) ap.d.clear(); l.damp.clear(); }
}

//==============================================================================
void DeepSpaceReverb::setParams (const Params& p)
{
    const int mi = std::clamp (p.mode, 0, numModes - 1);
    mode = &modes[mi];
    if (mi != lastMode)
    {
        // a new algorithm starts with an empty tank
        for (auto& l : lines) { l.delay.clear(); for (auto& ap : l.ap) ap.d.clear(); l.damp.clear(); }
        for (int ch = 0; ch < 2; ++ch) nonlinBuf[ch].clear();
        lastMode = mi;
    }

    const auto& co = colours[std::clamp (p.colour, 0, numColours - 1)];
    const float s = (float) sr * 0.001f;
    const float decay = std::max (0.1f, p.decay);
    const float att = std::clamp (p.attack + mode->attackBias, 0.0f, 1.0f);
    const float hfDecay = decay * std::pow (10.0f, p.highShelfDb / 24.0f) * std::min (1.6f, mode->bright);
    const float lfDecay = decay * p.bassMult;
    const float tankLp = std::min (co.tankLowpass * mode->bright, 21000.0f);

    bits = mode->dirty ? (co.bits > 0 ? 10 : 12) : co.bits;
    modJitter = co.modJitter;
    drive = mode->dirty ? 1.8f : 1.0f;

    // per-loop damping from the three decay times
    auto setDamping = [&] (Damping& d, float loopSec, float& gain)
    {
        const float dB   = -60.0f * loopSec / decay;
        const float dBhf = -60.0f * loopSec / std::max (0.05f, hfDecay);
        const float dBlf = -60.0f * loopSec / std::max (0.05f, lfDecay);
        gain = std::pow (10.0f, dB / 20.0f);
        d.high.highShelf (sr, p.highFreq, std::clamp (dBhf - dB, -48.0f, 0.0f));
        d.bass.lowShelf (sr, p.bassFreq, std::clamp (dBlf - dB, -30.0f, -dB - 0.05f));
        d.colour.lowPass (sr, tankLp, 0.6);
    };

    predelayTarget = std::max (1.0f, p.predelayMs * s);
    for (int ch = 0; ch < 2; ++ch)
    {
        band[ch][0].lowPass (sr, co.bandwidth, 0.5412);   band[ch][1].lowPass (sr, co.bandwidth, 1.3066);
        outBand[ch][0].lowPass (sr, co.bandwidth, 0.5412); outBand[ch][1].lowPass (sr, co.bandwidth, 1.3066);
        const float scaleE = 0.6f + 0.4f * p.size;
        for (int k = 0; k < 4; ++k) { earlyAp[ch][k].target = earlyMs[k] * sideMul[ch] * scaleE * s; earlyAp[ch][k].gTarget = p.early * 0.72f; }
        for (int k = 0; k < 2; ++k)
        {
            attackAp[ch][k].target = attackMs[k] * sideMul[ch] * (0.3f + 1.2f * att) * s;
            attackAp[ch][k].gTarget = att * 0.7f;
        }
        nlAp[ch][0].target = 7.3f * sideMul[ch] * s;  nlAp[ch][1].target = 11.9f * sideMul[ch] * s;
        nlAp[ch][0].gTarget = nlAp[ch][1].gTarget = p.late * 0.6f;
    }

    // attack swell: the tank is fed through 4 taps that spread the onset over up to ~110 ms
    const float w[4] { 1.0f - 0.85f * att, 0.45f * att, 0.65f * att, 0.85f * att };
    const float wn = 1.0f / std::sqrt (w[0] * w[0] + w[1] * w[1] + w[2] * w[2] + w[3] * w[3]);
    for (int k = 0; k < 4; ++k) { swellTapT[k] = std::max (1.0f, swellMs[k] * att * s); swellWT[k] = w[k] * wn; }

    // early reflections
    const float (*pat)[12] = mode->erPattern == 1 ? erRoom : mode->erPattern == 2 ? erChamber : mode->erPattern == 3 ? erHall : nullptr;
    erCount = 0;
    if (pat != nullptr)
    {
        const float scaleR = 0.3f + 0.7f * p.size;
        for (int k = 0; k < 12; ++k)
        {
            if (pat[0][k] <= 0.0f) break;
            for (int ch = 0; ch < 2; ++ch) erTaps[ch][k] = std::max (1, (int) (pat[ch][k] * scaleR * s));
            erGains[k] = (1.0f - 0.55f * (float) k / 12.0f) * ((k * 7) % 3 == 0 ? -1.0f : 1.0f) * 0.32f;
            ++erCount;
        }
    }
    erLevelT = mode->erLevel * (1.0f - 0.7f * att);
    erFeedT = mode->erLevel * 0.6f;

    if (mode->algo == Algo::hall)
    {
        const float scale = 0.2f + 0.8f * p.size;
        float avgLoop = 0.0f;
        for (int i = 0; i < numLines; ++i)
        {
            auto& l = lines[(size_t) i];
            const float f = (float) linePerm[i] / 15.0f;
            const float d = mode->minMs * std::pow (mode->maxMs / mode->minMs, f) * (1.0f + 0.023f * std::sin ((float) i * 12.9898f + 1.0f));
            const float lineMs = std::max (3.0f, d * scale);
            l.timeTarget = lineMs * s;

            float apTotal = 0.0f;
            for (int k = 0; k < 2; ++k)
            {
                const float apMs = mode->apMs * (0.4f + 0.6f * scale) * (1.0f + 0.13f * (float) i / 15.0f + 0.41f * (float) k);
                l.ap[k].target = apMs * s;
                l.ap[k].gTarget = k < mode->apCount ? p.late * 0.65f : 0.0f;
                if (k < mode->apCount) apTotal += apMs;
            }
            const float loop = (lineMs + apTotal) * 0.001f;
            avgLoop += loop / (float) numLines;
            setDamping (l.damp, loop, l.gainTarget);

            const float rate = p.rate * (0.7f + 0.6f * hash (i + 31));
            l.mod.inc = twoPi * rate / sr;
            l.mod.tInc = rate * (0.8f + 0.4f * hash (i + 7)) / (float) sr;
            const float depthSec = p.depth * mode->modScale * (mode->mod == chaos ? 0.006f : 0.0018f) / (1.0f + p.rate * 0.4f);
            l.depthTarget = std::min (lineMs * 0.25f, depthSec * 1000.0f) * s;
        }
        const float gp = std::pow (10.0f, -3.0f * avgLoop / decay);
        injectT = 0.35f * std::min (1.0f, std::pow ((1.0f - gp * gp) / 0.5f, 0.8f));
    }
    else // nonlin: gate / reverse envelope over up to 1.5 s
    {
        const float len = std::clamp (decay * 0.45f, 0.06f, 1.5f) * (float) sr;
        float sum = 0.0f;
        for (int k = 0; k < 48; ++k)
        {
            const float x = ((float) k + 0.5f) / 48.0f;
            nlTaps[k] = std::max (1.0f, len * ((float) k + 0.5f + 0.4f * (hash (k) - 0.5f)) / 48.0f);
            float e = (1.0f - att) * (1.0f - 0.3f * x) + att * (0.15f + 1.2f * x);
            e *= std::min (1.0f, (1.0f - x) / 0.06f);
            nlGains[k] = e;
            sum += e * e;
            nlSigns[0][k] = hash (k + 100) > 0.5f ? 1.0f : -1.0f;
            nlSigns[1][k] = hash (k + 200) > 0.5f ? 1.0f : -1.0f;
        }
        const float n = 0.9f / std::sqrt (sum);
        for (auto& g : nlGains) g *= n;
    }

    outGainT = mode->gain;

    if (snapNext)
    {
        snapNext = false;
        predelaySamples = predelayTarget; inject = injectT; erLevel = erLevelT; erFeed = erFeedT; outGain = outGainT;
        for (int k = 0; k < 4; ++k) { swellTap[k] = swellTapT[k]; swellW[k] = swellWT[k]; }
        auto snap = [] (Allpass& ap) { ap.delay = ap.target; ap.g = ap.gTarget; };
        for (int ch = 0; ch < 2; ++ch)
        {
            for (auto& ap : earlyAp[ch]) snap (ap);
            for (auto& ap : attackAp[ch]) snap (ap);
            for (auto& ap : nlAp[ch]) snap (ap);
        }
        for (auto& l : lines) { l.time = l.timeTarget; l.gain = l.gainTarget; l.depth = l.depthTarget; for (auto& ap : l.ap) snap (ap); }
    }
}

//==============================================================================
float DeepSpaceReverb::grain (float x)
{
    if (bits <= 0) return x;
    const float q = (float) (1 << (bits - 1));
    noiseSeed = noiseSeed * 1664525u + 1013904223u;
    const float dither = ((float) (noiseSeed >> 9) / 8388608.0f - 0.5f) * 0.6f;
    return std::round (x * q + dither) / q;
}

void DeepSpaceReverb::processHall (float tl, float tr, float& yl, float& yr)
{
    float y[numLines];
    float sl = 0.0f, sr2 = 0.0f;
    const bool dirty = mode->dirty;

    for (int k = 0; k < numLines; ++k)
    {
        auto& l = lines[(size_t) k];
        l.time += (l.timeTarget - l.time) * smooth;
        l.gain += (l.gainTarget - l.gain) * smooth;
        l.depth += (l.depthTarget - l.depth) * smooth;

        const float m = l.mod.next (mode->mod, modJitter);
        float v = l.delay.read (std::max (2.0f, l.time + l.depth * m));
        for (int a = 0; a < mode->apCount; ++a) v = l.ap[a].process (v, smooth);
        v = l.damp.process (v);
        if (dirty) v = std::tanh (v * 1.6f) * 0.625f;

        sl += l.outL * v;
        sr2 += l.outR * v;
        y[k] = softClip (v * l.gain);
    }

    for (int h = 1; h < numLines; h <<= 1)
        for (int a = 0; a < numLines; a += h << 1)
            for (int b = a; b < a + h; ++b)
            {
                const float x = y[b], z = y[b + h];
                y[b] = x + z; y[b + h] = x - z;
            }

    for (int k = 0; k < numLines; ++k)
    {
        auto& l = lines[(size_t) k];
        l.delay.write (y[k] * 0.25f + (k % 2 ? tr : tl) * l.sign * inject);
    }
    yl = sl; yr = sr2;
}

void DeepSpaceReverb::processNonlin (float tl, float tr, float& yl, float& yr)
{
    nonlinBuf[0].write (tl);
    nonlinBuf[1].write (tr);
    float a = 0.0f, b = 0.0f;
    for (int k = 0; k < 48; ++k)
    {
        const int d = (int) nlTaps[k];
        a += nlGains[k] * nlSigns[0][k] * nonlinBuf[0].tap (d);
        b += nlGains[k] * nlSigns[1][k] * nonlinBuf[1].tap (d);
    }
    for (auto& ap : nlAp[0]) a = ap.process (a, smooth);
    for (auto& ap : nlAp[1]) b = ap.process (b, smooth);
    yl = a; yr = b;
}

void DeepSpaceReverb::process (const float* inL, const float* inR, float* outL, float* outR, int n)
{
    for (int i = 0; i < n; ++i)
    {
        predelaySamples += (predelayTarget - predelaySamples) * smooth;
        inject += (injectT - inject) * smooth;
        erLevel += (erLevelT - erLevel) * smooth;
        erFeed += (erFeedT - erFeed) * smooth;
        outGain += (outGainT - outGain) * smooth;
        for (int k = 0; k < 4; ++k)
        {
            swellTap[k] += (swellTapT[k] - swellTap[k]) * smooth;
            swellW[k] += (swellWT[k] - swellW[k]) * smooth;
        }

        const float in[2] = { inL[i], inR[i] };
        float early[2], tank[2];
        for (int ch = 0; ch < 2; ++ch)
        {
            predelay[ch].write (in[ch]);
            float x = predelay[ch].read (predelaySamples);
            x = band[ch][1].process (band[ch][0].process (x));
            x = grain (x * drive);
            if (drive > 1.0f) x = std::tanh (x) / drive * 1.4f;

            // early reflections feed both the output and the tank
            er[ch].write (x);
            float refl = 0.0f;
            for (int k = 0; k < erCount; ++k) refl += erGains[k] * er[ch].tap (erTaps[ch][k]);
            x += refl * erFeed;

            for (auto& ap : earlyAp[ch]) x = ap.process (x, smooth);
            early[ch] = x;

            if (mode->algo == Algo::nonlin) { tank[ch] = x; continue; }

            for (auto& ap : attackAp[ch]) x = ap.process (x, smooth);
            swell[ch].write (x);
            float sw = 0.0f;
            for (int k = 0; k < 4; ++k) sw += swellW[k] * swell[ch].read (std::max (1.0f, swellTap[k]));
            tank[ch] = sw;
        }

        float yl = 0.0f, yr = 0.0f;
        switch (mode->algo)
        {
            case Algo::hall:   processHall (tank[0], tank[1], yl, yr); break;
            case Algo::nonlin: processNonlin (tank[0], tank[1], yl, yr); break;
        }

        yl += early[0] * erLevel * 0.5f;
        yr += early[1] * erLevel * 0.5f;
        outL[i] = outBand[0][1].process (outBand[0][0].process (yl)) * outGain;
        outR[i] = outBand[1][1].process (outBand[1][0].process (yr)) * outGain;
    }
}
