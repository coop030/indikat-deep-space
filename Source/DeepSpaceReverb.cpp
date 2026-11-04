#include "DeepSpaceReverb.h"
#include <algorithm>

namespace
{
    constexpr double twoPi = 6.283185307179586;

    // Hadamard sign: (-1)^popcount(i & j)
    constexpr float had (int i, int j)
    {
        int x = i & j, b = 0;
        while (x != 0) { b ^= x & 1; x >>= 1; }
        return b ? -1.0f : 1.0f;
    }

    inline float softClip (float x)
    {
        const float a = std::abs (x);
        if (a < 1.0f) return x;
        return std::copysign (1.0f + 0.5f * std::tanh ((a - 1.0f) * 2.0f), x);
    }

    constexpr float earlyMs[4]  { 4.3f, 6.9f, 10.7f, 16.3f };
    constexpr float attackMs[2] { 31.0f, 53.0f };
    constexpr float sideMul[2]  { 1.0f, 1.071f };
}

const DeepSpaceReverb::Mode DeepSpaceReverb::modes[6] = {
    { "Deep Hall", { 1.0f, 0.871f, 0.757f, 0.677f, 0.583f, 0.523f, 0.449f, 0.389f }, 0.19f,  0.55f },
    { "Room",      { 1.0f, 0.917f, 0.813f, 0.743f, 0.661f, 0.601f, 0.547f, 0.487f }, 0.055f, 0.8f  },
    { "Chamber",   { 1.0f, 0.893f, 0.797f, 0.709f, 0.631f, 0.563f, 0.501f, 0.447f }, 0.09f,  0.7f  },
    { "Plate",     { 1.0f, 0.947f, 0.883f, 0.829f, 0.769f, 0.719f, 0.661f, 0.613f }, 0.07f,  0.15f },
    { "Nebula",    { 1.0f, 0.953f, 0.887f, 0.829f, 0.761f, 0.709f, 0.643f, 0.587f }, 0.36f,  0.3f  },
    { "Ambience",  { 1.0f, 0.79f,  0.62f,  0.48f,  0.41f,  0.33f,  0.27f,  0.22f  }, 0.035f, 1.0f  },
};

const DeepSpaceReverb::Colour DeepSpaceReverb::colours[3] = {
    { "Vintage", 5200.0f,  1.6f  },
    { "Classic", 10500.0f, 1.15f },
    { "Modern",  21000.0f, 1.0f  },
};

//==============================================================================
void DeepSpaceReverb::Biquad::set (double nb0, double nb1, double nb2, double a0, double na1, double na2)
{
    b0 = (float) (nb0 / a0); b1 = (float) (nb1 / a0); b2 = (float) (nb2 / a0);
    a1 = (float) (na1 / a0); a2 = (float) (na2 / a0);
}

void DeepSpaceReverb::Biquad::lowShelf (double fs, double f, double dB)
{
    const double A = std::pow (10.0, dB / 40.0), w = twoPi * std::min (f, fs * 0.45) / fs;
    const double c = std::cos (w), al = std::sin (w) / 2.0 * std::sqrt (2.0), sa = 2.0 * std::sqrt (A) * al;
    set (A * ((A + 1) - (A - 1) * c + sa), 2 * A * ((A - 1) - (A + 1) * c), A * ((A + 1) - (A - 1) * c - sa),
         (A + 1) + (A - 1) * c + sa, -2 * ((A - 1) + (A + 1) * c), (A + 1) + (A - 1) * c - sa);
}

void DeepSpaceReverb::Biquad::highShelf (double fs, double f, double dB)
{
    const double A = std::pow (10.0, dB / 40.0), w = twoPi * std::min (f, fs * 0.45) / fs;
    const double c = std::cos (w), al = std::sin (w) / 2.0 * std::sqrt (2.0), sa = 2.0 * std::sqrt (A) * al;
    set (A * ((A + 1) + (A - 1) * c + sa), -2 * A * ((A - 1) + (A + 1) * c), A * ((A + 1) + (A - 1) * c - sa),
         (A + 1) - (A - 1) * c + sa, 2 * ((A - 1) - (A + 1) * c), (A + 1) - (A - 1) * c - sa);
}

void DeepSpaceReverb::Biquad::lowPass (double fs, double f)
{
    if (f >= fs * 0.45) { set (1, 0, 0, 1, 0, 0); return; }   // bypass
    const double w = twoPi * f / fs, c = std::cos (w), al = std::sin (w) / (2.0 * 0.7071);
    set ((1 - c) / 2, 1 - c, (1 - c) / 2, 1 + al, -2 * c, 1 - al);
}

void DeepSpaceReverb::Biquad::highPass (double fs, double f)
{
    const double w = twoPi * std::min (f, fs * 0.45) / fs, c = std::cos (w), al = std::sin (w) / (2.0 * 0.7071);
    set ((1 + c) / 2, -(1 + c), (1 + c) / 2, 1 + al, -2 * c, 1 - al);
}

//==============================================================================
void DeepSpaceReverb::prepare (double sampleRate)
{
    sr = sampleRate;
    smooth = (float) (1.0 - std::exp (-1.0 / (0.025 * sr)));
    const auto ms = [this] (double m) { return (int) std::ceil (m * 0.001 * sr); };

    for (int ch = 0; ch < 2; ++ch)
    {
        predelay[ch].init (ms (510.0));
        for (auto& ap : earlyAp[ch])  ap.d.init (ms (20.0));
        for (auto& ap : attackAp[ch]) ap.d.init (ms (110.0));
    }
    for (int i = 0; i < numLines; ++i)
    {
        auto& l = lines[(size_t) i];
        l.delay.init (ms (420.0));
        l.late.d.init (ms (20.0));
        l.sign = (i % 2) ? -1.0f : 1.0f;
        l.phase = (double) i * 0.785;
    }
    hasLast = false;
    reset();
}

void DeepSpaceReverb::reset()
{
    for (int ch = 0; ch < 2; ++ch)
    {
        predelay[ch].clear();
        for (auto& ap : earlyAp[ch])  ap.d.clear();
        for (auto& ap : attackAp[ch]) ap.d.clear();
    }
    for (auto& l : lines)
    {
        l.delay.clear(); l.late.d.clear();
        l.high.clear(); l.bass.clear(); l.colour.clear();
    }
}

void DeepSpaceReverb::setParams (const Params& p)
{
    const auto& md = modes[std::clamp (p.mode, 0, numModes - 1)];
    const auto& co = colours[std::clamp (p.colour, 0, numColours - 1)];
    const float s = (float) sr * 0.001f;   // samples per ms
    const float decay = std::max (0.1f, p.decay);

    predelayTarget = std::max (1.0f, p.predelayMs * s);

    const float ge = p.early * 0.72f, ga = p.attack * 0.75f;
    for (int ch = 0; ch < 2; ++ch)
    {
        for (int k = 0; k < 4; ++k) { earlyAp[ch][k].target = earlyMs[k] * sideMul[ch] * s; earlyAp[ch][k].gTarget = ge; }
        for (int k = 0; k < 2; ++k)
        {
            attackAp[ch][k].target = attackMs[k] * sideMul[ch] * (0.4f + p.attack * 1.4f) * s;
            attackAp[ch][k].gTarget = ga;
        }
    }
    erTarget = md.earlyLevel * (1.0f - p.attack * 0.85f) * 0.55f;

    const float base = 0.012f + p.size * md.length;   // seconds
    const float lateScale = 0.6f + 0.8f * p.size;
    float avgLoop = 0.0f;
    const auto diff = [] (float a, float b) { return std::abs (a - b) > 1.0e-6f * (1.0f + std::abs (b)); };
    const bool coeffsChanged = ! hasLast || diff (p.decay, last.decay) || diff (p.highFreq, last.highFreq)
                            || diff (p.highShelfDb, last.highShelfDb) || diff (p.bassFreq, last.bassFreq)
                            || diff (p.bassMult, last.bassMult) || diff (p.size, last.size) || p.mode != last.mode
                            || p.colour != last.colour;

    for (int i = 0; i < numLines; ++i)
    {
        auto& l = lines[(size_t) i];
        const float t = base * md.ratios[i];
        const float lateMs = (3.1f + 1.17f * (float) i) * lateScale;
        const float loop = t + lateMs * 0.001f;
        avgLoop += loop / (float) numLines;

        l.timeTarget = std::max (2.0f, t * (float) sr);
        l.late.target = lateMs * s;
        l.late.gTarget = p.late * 0.6f;

        // RT60: -60 dB after `decay` seconds; bass decays bassMult times longer
        const float dB  = -60.0f * loop / decay;
        const float dBb = -60.0f * loop / (decay * p.bassMult);
        l.gainTarget = std::pow (10.0f, dB / 20.0f);

        if (coeffsChanged)
        {
            l.bass.lowShelf (sr, p.bassFreq, std::clamp (dBb - dB, -30.0f, -dB - 0.05f));
            l.high.highShelf (sr, p.highFreq, p.highShelfDb * std::min (1.0f, loop / 0.06f + 0.25f));
            l.colour.lowPass (sr, co.lowpass);
        }

        const float depthSec = std::min (t * 0.25f, p.depth * co.modScale * 0.0035f / (1.0f + p.rate * 0.6f));
        l.modDepthTarget = depthSec * (float) sr;
        l.phaseInc = twoPi * p.rate * (0.62 + 0.11 * i) / sr;
    }

    // keep the tail level steady across Decay
    const float gp = std::pow (10.0f, -3.0f * avgLoop / decay);
    injectTarget = 0.5f * std::min (1.0f, std::pow ((1.0f - gp * gp) / 0.5f, 0.8f));

    last = p;
    hasLast = true;
}

void DeepSpaceReverb::process (const float* inL, const float* inR, float* outL, float* outR, int n)
{
    constexpr float norm = 0.35355339f;   // 1 / sqrt(8)
    float fb[numLines];

    for (int i = 0; i < n; ++i)
    {
        predelaySamples += (predelayTarget - predelaySamples) * smooth;
        inject += (injectTarget - inject) * smooth;
        erLevel += (erTarget - erLevel) * smooth;

        float side[2] = { inL[i], inR[i] }, er[2], tank[2];
        for (int ch = 0; ch < 2; ++ch)
        {
            predelay[ch].write (side[ch]);
            float x = predelay[ch].read (predelaySamples);
            for (auto& ap : earlyAp[ch]) x = ap.process (x, smooth);
            er[ch] = x;
            for (auto& ap : attackAp[ch]) x = ap.process (x, smooth);
            tank[ch] = x * inject;
        }

        float wl = 0.0f, wr = 0.0f;
        for (int k = 0; k < numLines; ++k)
        {
            auto& l = lines[(size_t) k];
            l.time += (l.timeTarget - l.time) * smooth;
            l.gain += (l.gainTarget - l.gain) * smooth;
            l.modDepth += (l.modDepthTarget - l.modDepth) * smooth;
            l.phase += l.phaseInc;
            if (l.phase > twoPi) l.phase -= twoPi;

            const float d = std::max (2.0f, l.time + l.modDepth * (float) std::sin (l.phase));
            float y = l.delay.read (d);
            y = l.late.process (y, smooth);
            y = l.high.process (y);
            y = l.bass.process (y);
            y = l.colour.process (y);

            wl += had (1, k) * y;
            wr += had (2, k) * y;
            fb[k] = softClip (y * l.gain);
        }

        // fast Walsh-Hadamard transform (orthonormal mix)
        for (int h = 1; h < numLines; h <<= 1)
            for (int a = 0; a < numLines; a += h << 1)
                for (int b = a; b < a + h; ++b)
                {
                    const float x = fb[b], y = fb[b + h];
                    fb[b] = x + y; fb[b + h] = x - y;
                }

        for (int k = 0; k < numLines; ++k)
        {
            auto& l = lines[(size_t) k];
            l.delay.write (fb[k] * norm + tank[k < 4 ? 0 : 1] * l.sign);
        }

        outL[i] = wl * 0.42f + er[0] * erLevel;
        outR[i] = wr * 0.42f + er[1] * erLevel;
    }
}
