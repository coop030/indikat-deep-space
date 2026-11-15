#pragma once

#include <array>
#include <cmath>
#include <cstdint>
#include <vector>

// INDIKAT Deep Space reverb engine (wet signal only, stereo).
//
// Two algorithm families, chosen by Mode:
//  - Hall:   16-line feedback delay network (Hadamard mix), per line: modulated delay, late allpasses,
//            high shelf + bass shelf set from the RT60, colour low-pass. Optional early reflections.
//            Halls, rooms, chambers and plates are voicings of it (line lengths, diffusion, reflections).
//  - Nonlin: gated / reverse envelope built from a dense diffused tap pattern.
// In front of every algorithm: predelay -> colour bandwidth -> early diffusion -> attack swell.
// Colour (1970s / 1980s / Now) sets bandwidth, converter grain and modulation character.
class DeepSpaceReverb
{
public:
    struct Params
    {
        float predelayMs = 20.0f, decay = 2.5f;
        float highFreq = 6000.0f, highShelfDb = -6.0f, bassFreq = 500.0f, bassMult = 1.2f;
        float size = 0.8f, attack = 0.5f, early = 0.8f, late = 0.8f;
        float rate = 0.6f, depth = 0.3f;
        int mode = 0, colour = 2;
    };

    enum class Algo { hall, nonlin };
    enum ModType { sine, randomWalk, chorus, chaos };

    struct Mode
    {
        const char* name;
        Algo algo;
        float minMs, maxMs;      // hall: line lengths at Size 100 %
        float erLevel;           // early reflections in the output
        int erPattern;           // 0 none, 1 room, 2 chamber, 3 hall
        float apMs; int apCount; // late allpasses per line
        ModType mod; float modScale;
        bool dirty;
        float bright;            // > 1 = brighter tank
        float attackBias;        // added to Attack
        float gain;              // output trim
    };
    static const Mode modes[];
    static const int numModes;

    struct Colour { const char* name; float bandwidth; float tankLowpass; int bits; float modJitter; };
    static const Colour colours[3];
    static constexpr int numColours = 3;

    void prepare (double sampleRate);
    void reset();
    void setParams (const Params&);   // once per block
    void process (const float* inL, const float* inR, float* outL, float* outR, int numSamples);

    static constexpr int numLines = 16;

private:
    //==========================================================================
    struct Delay
    {
        std::vector<float> buf;
        int mask = 0, pos = 0;

        void init (int maxSamples)
        {
            int n = 1;
            while (n < maxSamples + 8) n <<= 1;
            buf.assign ((size_t) n, 0.0f);
            mask = n - 1;
            pos = 0;
        }
        void clear() { std::fill (buf.begin(), buf.end(), 0.0f); }
        void write (float x) { buf[(size_t) pos] = x; pos = (pos + 1) & mask; }
        float tap (int d) const { return buf[(size_t) ((pos - d) & mask)]; }

        // d = delay in samples (>= 1), cubic Hermite interpolation
        float read (float d) const
        {
            const int i = (int) d;
            const float f = d - (float) i;
            const int b = pos - i;
            const float xm1 = buf[(size_t) ((b + 1) & mask)];
            const float x0  = buf[(size_t) (b & mask)];
            const float x1  = buf[(size_t) ((b - 1) & mask)];
            const float x2  = buf[(size_t) ((b - 2) & mask)];
            const float c1 = 0.5f * (x1 - xm1);
            const float c2 = xm1 - 2.5f * x0 + 2.0f * x1 - 0.5f * x2;
            const float c3 = 0.5f * (x2 - xm1) + 1.5f * (x0 - x1);
            return ((c3 * f + c2) * f + c1) * f + x0;
        }
    };

    struct Allpass
    {
        Delay d;
        float delay = 1.0f, target = 1.0f, g = 0.0f, gTarget = 0.0f;

        float process (float x, float smooth, float mod = 0.0f)
        {
            delay += (target - delay) * smooth;
            g += (gTarget - g) * smooth;
            const float z = d.read (std::max (1.0f, delay + mod));
            const float s = x + g * z;
            d.write (s);
            return z - g * s;
        }
    };

    struct Biquad
    {
        float b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;
        float process (float x)
        {
            const float y = b0 * x + z1;
            z1 = b1 * x - a1 * y + z2;
            z2 = b2 * x - a2 * y;
            return y;
        }
        void clear() { z1 = z2 = 0.0f; }
        void lowShelf (double sr, double f, double dB);
        void highShelf (double sr, double f, double dB);
        void lowPass (double sr, double f, double q = 0.7071);
        void set (double b0, double b1, double b2, double a0, double a1, double a2);
    };

    struct Damping
    {
        Biquad high, bass, colour;
        float process (float x) { return colour.process (bass.process (high.process (x))); }
        void clear() { high.clear(); bass.clear(); colour.clear(); }
    };

    // smooth LFO: sine, interpolated random walk, or both
    struct Mod
    {
        double phase = 0.0, inc = 0.0;
        float a = 0.0f, b = 0.0f, t = 1.0f, tInc = 0.001f;
        uint32_t seed = 1;
        float rnd() { seed = seed * 1664525u + 1013904223u; return (float) (seed >> 8) / 8388608.0f - 1.0f; }
        float next (ModType type, float jitter);
    };

    struct Line
    {
        Delay delay;
        Allpass ap[2];
        Damping damp;
        Mod mod;
        float time = 1000.0f, timeTarget = 1000.0f;
        float gain = 0.0f, gainTarget = 0.0f;
        float depth = 0.0f, depthTarget = 0.0f;
        float sign = 1.0f, outL = 0.0f, outR = 0.0f;
    };

    void processHall (float tl, float tr, float& yl, float& yr);
    void processNonlin (float tl, float tr, float& yl, float& yr);
    float grain (float x);

    double sr = 48000.0;
    float smooth = 0.001f;
    const Mode* mode = &modes[0];
    int lastMode = -1;

    Delay predelay[2];
    float predelaySamples = 0.0f, predelayTarget = 0.0f;
    Biquad band[2][2], outBand[2][2];
    Allpass earlyAp[2][4];
    Allpass attackAp[2][2];
    Delay swell[2];
    float swellTap[4] {}, swellTapT[4] {}, swellW[4] {}, swellWT[4] {};
    Delay er[2];
    float erLevel = 0.0f, erLevelT = 0.0f, erFeed = 0.0f, erFeedT = 0.0f;
    int erTaps[2][12] {}; float erGains[12] {}; int erCount = 0;

    std::array<Line, numLines> lines;
    float inject = 0.25f, injectT = 0.25f;

    Delay nonlinBuf[2];
    float nlTaps[48] {}, nlGains[48] {}, nlSigns[2][48] {};
    Allpass nlAp[2][2];

    float outGain = 1.0f, outGainT = 1.0f;
    float drive = 1.0f;
    int bits = 0;
    float modJitter = 0.0f;
    uint32_t noiseSeed = 12345;
    bool snapNext = true;   // jump to the targets on the first block after prepare()
};
