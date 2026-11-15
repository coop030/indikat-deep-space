// Renders every preset with a short test signal (clicks + a noise burst), then lets the tail ring out.
// Checks for NaN/Inf and overload, and that the tail actually decays (no runaway feedback).
// Output: one WAV per preset in the given folder.
#include "../Source/PluginProcessor.h"
#include "../Source/PluginEditor.h"

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;
    const juce::File outDir (argc > 1 ? juce::String (argv[1]) : juce::File::getCurrentWorkingDirectory().getFullPathName());
    outDir.createDirectory();

    constexpr double sr = 48000.0;
    constexpr int block = 256;
    bool allOk = true;

    DeepSpaceProcessor proc;
    proc.setPlayConfigDetails (2, 2, sr, block);

    // --process <in.wav> <out.wav> <preset number> [param value ...]: run a file through a preset (for demos)
    if (argc > 4 && juce::String (argv[1]) == "--process")
    {
        juce::WavAudioFormat wav;
        std::unique_ptr<juce::AudioFormatReader> reader (wav.createReaderFor (new juce::FileInputStream (juce::File (argv[2])), true));
        if (reader == nullptr) return 2;
        const int len = (int) reader->lengthInSamples;
        juce::AudioBuffer<float> audio (2, len);
        reader->read (&audio, 0, len, 0, true, true);
        proc.setPlayConfigDetails (2, 2, reader->sampleRate, block);
        proc.setCurrentProgram (juce::String (argv[4]).getIntValue() - 1);
        for (int a = 5; a + 1 < argc; a += 2)
            if (auto* p = proc.apvts.getParameter (argv[a])) p->setValueNotifyingHost (p->convertTo0to1 (juce::String (argv[a + 1]).getFloatValue()));
        proc.prepareToPlay (reader->sampleRate, block);
        for (int pos = 0; pos < len; pos += block)
        {
            const int n = juce::jmin (block, len - pos);
            juce::AudioBuffer<float> buf (audio.getArrayOfWritePointers(), 2, pos, n);
            juce::MidiBuffer midi;
            proc.processBlock (buf, midi);
        }
        juce::File out (argv[3]);
        out.deleteFile();
        if (auto stream = std::unique_ptr<juce::FileOutputStream> (out.createOutputStream()))
            if (auto writer = std::unique_ptr<juce::AudioFormatWriter> (wav.createWriterFor (stream.get(), reader->sampleRate, 2, 24, {}, 0)))
            {
                stream.release();
                writer->writeFromAudioSampleBuffer (audio, 0, len);
            }
        return 0;
    }

    // --ir <dir>: 100 % wet impulse response of every mode (default settings, Decay 2.5 s) as WAV
    if (argc > 2 && juce::String (argv[1]) == "--ir")
    {
        const juce::File dir (argv[2]);
        dir.createDirectory();
        auto* modeParam = dynamic_cast<juce::AudioParameterChoice*> (proc.apvts.getParameter ("mode"));
        auto set = [&proc] (const char* id, float v) { auto* p = proc.apvts.getParameter (id); p->setValueNotifyingHost (p->convertTo0to1 (v)); };
        for (int m = 0; m < modeParam->choices.size(); ++m)
        {
            proc.setCurrentProgram (0);
            set ("mode", (float) m); set ("mix", 1.0f); set ("decay", 2.5f); set ("predelay", 0.0f);
            for (int a = 3; a + 1 < argc; a += 2) set (argv[a], juce::String (argv[a + 1]).getFloatValue());
            proc.prepareToPlay (sr, block);
            const int total = (int) (sr * 4.0);
            juce::AudioBuffer<float> out (2, total);
            for (int pos = 0; pos < total; pos += block)
            {
                const int n = juce::jmin (block, total - pos);
                juce::AudioBuffer<float> buf (2, n);
                buf.clear();
                if (pos == 0) { buf.setSample (0, 0, 1.0f); buf.setSample (1, 0, 1.0f); }
                juce::MidiBuffer midi;
                proc.processBlock (buf, midi);
                for (int ch = 0; ch < 2; ++ch) out.copyFrom (ch, pos, buf, ch, 0, n);
            }
            auto file = dir.getChildFile ("ir " + juce::String (m).paddedLeft ('0', 2) + " " + modeParam->choices[m] + ".wav");
            file.deleteFile();
            juce::WavAudioFormat wav;
            if (auto stream = std::unique_ptr<juce::FileOutputStream> (file.createOutputStream()))
                if (auto writer = std::unique_ptr<juce::AudioFormatWriter> (wav.createWriterFor (stream.get(), sr, 2, 24, {}, 0)))
                {
                    stream.release();
                    writer->writeFromAudioSampleBuffer (out, 0, total);
                }
        }
        return 0;
    }

    // --snapshot <file.png>: render the plugin window to an image and exit
    if (argc > 2 && juce::String (argv[1]) == "--snapshot")
    {
        std::unique_ptr<juce::AudioProcessorEditor> ed (proc.createEditor());
        auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 2.0f);
        juce::File f (argv[2]);
        f.deleteFile();
        juce::FileOutputStream os (f);
        juce::PNGImageFormat().writeImageToStream (img, os);
        return 0;
    }

    for (int p = 0; p < proc.getNumPrograms(); ++p)
    {
        proc.setCurrentProgram (p);
        proc.prepareToPlay (sr, block);
        const float decay = proc.apvts.getRawParameterValue ("decay")->load();

        // 2 s of input, then up to 2x the decay time of silence (capped)
        const int inputLen = (int) (sr * 2.0);
        const int total = inputLen + (int) (sr * juce::jlimit (3.0, 40.0, decay * 2.0 + 1.0));
        juce::AudioBuffer<float> out (2, total);
        juce::Random rng (1234);

        float peak = 0.0f; bool bad = false;
        for (int pos = 0; pos < total; pos += block)
        {
            const int n = juce::jmin (block, total - pos);
            juce::AudioBuffer<float> buf (2, n);
            buf.clear();
            for (int i = 0; i < n; ++i)
            {
                const int t = pos + i;
                float x = 0.0f;
                if (t < inputLen && t % 24000 < 40) x = 0.8f * (1.0f - (float) (t % 24000) / 40.0f);     // clicks every 0.5 s
                if (t >= 12000 && t < 21600) x += (rng.nextFloat() * 2.0f - 1.0f) * 0.25f;                 // 200 ms noise burst
                buf.setSample (0, i, x);
                buf.setSample (1, i, x * 0.9f);
            }
            juce::MidiBuffer midi;
            proc.processBlock (buf, midi);
            for (int ch = 0; ch < 2; ++ch)
            {
                out.copyFrom (ch, pos, buf, ch, 0, n);
                for (int i = 0; i < n; ++i)
                {
                    const float v = buf.getSample (ch, i);
                    if (! std::isfinite (v)) bad = true;
                    peak = juce::jmax (peak, std::abs (v));
                }
            }
        }

        // tail level: RMS of the last 250 ms vs. peak RMS while the input plays
        auto rmsAt = [&out] (int start, int len)
        {
            double s = 0.0;
            for (int ch = 0; ch < 2; ++ch)
                for (int i = start; i < start + len; ++i) s += (double) out.getSample (ch, i) * out.getSample (ch, i);
            return std::sqrt (s / (2.0 * len));
        };
        const int win = (int) (sr * 0.25);
        double loud = 0.0;
        for (int st = 0; st + win <= inputLen; st += win / 2) loud = juce::jmax (loud, rmsAt (st, win));
        const double tail = rmsAt (total - win, win);
        const double dropDb = 20.0 * std::log10 ((tail + 1e-12) / (loud + 1e-12));

        const bool decays = dropDb < -40.0;
        const bool ok = ! bad && peak > 0.01f && peak < 2.0f && decays;
        allOk &= ok;
        std::printf ("%-2d %-18s decay %5.2f s  peak %6.2f dBFS  tail drop %6.1f dB  %s\n", p + 1,
                     proc.getProgramName (p).toRawUTF8(), decay, juce::Decibels::gainToDecibels (peak), dropDb,
                     ok ? "OK" : "PROBLEM");

        out.applyGain (juce::Decibels::decibelsToGain (-1.0f) / juce::jmax (peak, 1.0e-6f));
        auto file = outDir.getChildFile (juce::String (p + 1).paddedLeft ('0', 2) + " " + proc.getProgramName (p) + ".wav");
        file.deleteFile();
        juce::WavAudioFormat wav;
        if (auto stream = std::unique_ptr<juce::FileOutputStream> (file.createOutputStream()))
            if (auto writer = std::unique_ptr<juce::AudioFormatWriter> (wav.createWriterFor (stream.get(), sr, 2, 24, {}, 0)))
            {
                stream.release();
                writer->writeFromAudioSampleBuffer (out, 0, out.getNumSamples());
            }
    }
    return allOk ? 0 : 1;
}
