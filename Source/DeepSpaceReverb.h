#pragma once

#include <array>
#include <cmath>
#include <vector>

// INDIKAT Deep Space reverb engine (wet signal only, stereo).
//
// Predelay -> early diffusion (4 allpasses per side) -> attack smear (2 long allpasses)
//          -> 8-line feedback delay network, Hadamard mix. Each line:
//             modulated delay -> late allpass -> high shelf -> bass shelf -> colour low-pass -> feedback gain
// The feedback gain of every line is set from the RT60 (Decay), and the bass shelf stretches the decay
// below Bass Freq by Bass Mult. The tank input is scaled with the loop gain so long tails don't get louder.
class DeepSpaceReverb
{
public:
    struct Params
    {
        float predelayMs = 20.0f, decay = 4.0f;
        float highFreq = 6000.0f, highShelfDb = -6.0f, bassFreq = 500.0f, bassMult = 1.2f;
        float size = 0.8f, attack = 0.5f, early = 0.8f, late = 0.8f;
        float rate = 0.6f, depth = 0.3f;
        int mode = 0, colour = 1;
    };

    struct Mode { const char* name; float ratios[8]; float length; float earlyLevel; };
    static const Mode modes[6];
    static constexpr int numModes = 6;

    struct Colour { const char* name; float lowpass; float modScale; };
    static const Colour colours[3];
    static constexpr int numColours = 3;

    void prepare (double sampleRate);
    void reset();
    void setParams (const Params&);   // once per block
    void process (const float* inL, const float* inR, float* outL, float* outR, int numSamples);

    static constexpr int numLines = 8;

private:
    //==========================================================================
    struct Delay
    {
        std::vector<float> buf;
        int mask = 0, pos = 0;

        void init (int maxSamples)
        {
            int n = 1;
            while (n < maxSamples + 4) n <<= 1;
            buf.assign ((size_t) n, 0.0f);
            mask = n - 1;
            pos = 0;
        }
        void clear() { std::fill (buf.begin(), buf.end(), 0.0f); }
        void write (float x) { buf[(size_t) pos] = x; pos = (pos + 1) & mask; }

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

        float process (float x, float smooth)
        {
            delay += (target - delay) * smooth;
            g += (gTarget - g) * smooth;
            const float z = d.read (delay);
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
        void lowPass (double sr, double f);
        void highPass (double sr, double f);
        void set (double b0, double b1, double b2, double a0, double a1, double a2);
    };

    struct Line
    {
        Delay delay;
        Allpass late;
        Biquad high, bass, colour;
        float time = 1000.0f, timeTarget = 1000.0f;   // samples
        float gain = 0.0f, gainTarget = 0.0f;
        float modDepth = 0.0f, modDepthTarget = 0.0f;
        double phase = 0.0, phaseInc = 0.0;
        float sign = 1.0f;
    };

    double sr = 48000.0;
    float smooth = 0.001f;          // per-sample one-pole coefficient (~25 ms)
    Delay predelay[2];
    float predelaySamples = 0.0f, predelayTarget = 0.0f;
    Allpass earlyAp[2][4], attackAp[2][2];
    std::array<Line, numLines> lines;
    float inject = 0.5f, injectTarget = 0.5f;
    float erLevel = 0.0f, erTarget = 0.0f;

    Params last;
    bool hasLast = false;
};
