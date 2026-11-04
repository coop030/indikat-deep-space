#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
    juce::NormalisableRange<float> skewed (float lo, float hi, float centre)
    {
        juce::NormalisableRange<float> r (lo, hi);
        r.setSkewForCentre (centre);
        return r;
    }

    struct Preset
    {
        const char* name;
        std::initializer_list<std::pair<const char*, float>> values;
    };

    // mode: 0 Deep Hall, 1 Room, 2 Chamber, 3 Plate, 4 Nebula, 5 Ambience   colour: 0 Vintage, 1 Classic, 2 Modern
    const Preset presets[] = {
        { "Init", {} },
        { "Andromeda Hall", { { "mix", 0.32f }, { "decay", 3.2f }, { "size", 0.85f }, { "attack", 0.55f }, { "highFreq", 5000 },
                              { "highShelf", -6 }, { "bassMult", 1.3f }, { "depth", 0.3f } } },
        { "Small Room", { { "mode", 1 }, { "mix", 0.22f }, { "decay", 0.7f }, { "size", 0.6f }, { "attack", 0.3f }, { "early", 0.7f },
                          { "late", 0.6f }, { "highFreq", 8000 }, { "highShelf", -4 }, { "predelay", 5 }, { "depth", 0.15f } } },
        { "Vocal Plate", { { "mode", 3 }, { "mix", 0.28f }, { "decay", 1.8f }, { "size", 0.75f }, { "attack", 0.2f }, { "early", 1 },
                           { "late", 1 }, { "highFreq", 9000 }, { "highShelf", -3 }, { "predelay", 30 }, { "lowCut", 180 }, { "colour", 2 } } },
        { "Drum Chamber", { { "mode", 2 }, { "mix", 0.25f }, { "decay", 1.2f }, { "size", 0.7f }, { "attack", 0.25f }, { "early", 0.6f },
                            { "late", 0.8f }, { "highFreq", 6000 }, { "highShelf", -8 }, { "predelay", 8 }, { "lowCut", 120 } } },
        { "Night Cathedral", { { "mode", 4 }, { "mix", 0.45f }, { "decay", 9 }, { "size", 0.9f }, { "attack", 0.75f }, { "highFreq", 3500 },
                               { "highShelf", -12 }, { "bassMult", 1.5f }, { "depth", 0.45f }, { "rate", 0.3f } } },
        { "Nebula", { { "mode", 4 }, { "mix", 0.55f }, { "decay", 22 }, { "size", 1 }, { "attack", 0.85f }, { "early", 1 }, { "late", 1 },
                      { "highFreq", 4500 }, { "highShelf", -9 }, { "bassMult", 1.2f }, { "depth", 0.5f }, { "rate", 0.4f }, { "predelay", 60 } } },
        { "Ambience", { { "mode", 5 }, { "mix", 0.3f }, { "decay", 0.45f }, { "size", 0.5f }, { "attack", 0.1f }, { "early", 0.9f },
                        { "late", 0.5f }, { "predelay", 0 }, { "colour", 2 } } },
        { "Vintage Plate", { { "mode", 3 }, { "colour", 0 }, { "mix", 0.3f }, { "decay", 2.4f }, { "size", 0.8f }, { "attack", 0.3f },
                             { "early", 1 }, { "late", 1 }, { "highFreq", 4000 }, { "highShelf", -6 }, { "lowCut", 150 } } },
        { "Dark Matter", { { "mode", 0 }, { "colour", 0 }, { "mix", 0.42f }, { "decay", 6 }, { "size", 0.95f }, { "attack", 0.6f },
                           { "highFreq", 1800 }, { "highShelf", -18 }, { "bassMult", 2 }, { "depth", 0.6f }, { "rate", 0.25f }, { "highCut", 4000 } } },
        { "Starlight", { { "mode", 0 }, { "colour", 2 }, { "mix", 0.38f }, { "decay", 5 }, { "size", 0.8f }, { "attack", 0.4f },
                         { "highFreq", 12000 }, { "highShelf", -2 }, { "bassMult", 0.7f }, { "lowCut", 250 }, { "depth", 0.25f }, { "rate", 1.1f } } },
        { "Slapback Space", { { "mode", 2 }, { "mix", 0.3f }, { "decay", 1.5f }, { "size", 0.5f }, { "attack", 0.15f }, { "early", 0.3f },
                              { "late", 0.4f }, { "predelay", 110 }, { "highFreq", 5000 }, { "highShelf", -6 }, { "lowCut", 200 } } },
    };
    constexpr int numPresets = (int) std::size (presets);
}

//==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout DeepSpaceProcessor::createLayout()
{
    using namespace juce;
    std::vector<std::unique_ptr<RangedAudioParameter>> p;

    auto attr = [] (std::function<String (float)> f)
    {
        return AudioParameterFloatAttributes()
                   .withStringFromValueFunction ([f] (float v, int) { return f (v); })
                   .withValueFromStringFunction ([] (const String& t)
                                                 { auto v = t.getFloatValue();
                                                   return t.containsIgnoreCase ("k") ? v * 1000.0f : t.containsChar ('%') ? v / 100.0f : v; });
    };
    auto hz  = attr ([] (float v) { return v >= 1000.0f ? String (v / 1000.0f, v >= 10000.0f ? 1 : 2) + " kHz" : String (roundToInt (v)) + " Hz"; });
    auto pct = attr ([] (float v) { return String (v * 100.0f, 1) + " %"; });
    auto ms  = attr ([] (float v) { return String (v, v < 100.0f ? 1 : 0) + " ms"; });
    auto sec = attr ([] (float v) { return String (v, v < 10.0f ? 2 : 1) + " s"; });
    auto dB  = attr ([] (float v) { return String (v, 1) + " dB"; });
    auto mul = attr ([] (float v) { return String (v, 2) + " x"; });

    auto add = [&p] (const char* id, const char* name, NormalisableRange<float> r, float def, AudioParameterFloatAttributes a)
    { p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { id, 1 }, name, r, def, a)); };

    add ("mix",       "Mix",        NormalisableRange<float> (0.0f, 1.0f), 0.35f, pct);
    add ("predelay",  "Predelay",   skewed (0.0f, 500.0f, 60.0f), 20.0f, ms);
    add ("decay",     "Decay",      skewed (0.2f, 30.0f, 3.0f), 4.0f, sec);
    add ("highFreq",  "High Freq",  skewed (500.0f, 20000.0f, 4000.0f), 6000.0f, hz);
    add ("highShelf", "High Shelf", NormalisableRange<float> (-24.0f, 0.0f), -6.0f, dB);
    add ("bassFreq",  "Bass Freq",  skewed (50.0f, 1000.0f, 250.0f), 500.0f, hz);
    add ("bassMult",  "Bass Mult",  skewed (0.25f, 4.0f, 1.0f), 1.2f, mul);
    add ("size",      "Size",       NormalisableRange<float> (0.0f, 1.0f), 0.8f, pct);
    add ("attack",    "Attack",     NormalisableRange<float> (0.0f, 1.0f), 0.5f, pct);
    add ("early",     "Early Diff", NormalisableRange<float> (0.0f, 1.0f), 0.8f, pct);
    add ("late",      "Late Diff",  NormalisableRange<float> (0.0f, 1.0f), 0.8f, pct);
    add ("rate",      "Mod Rate",   skewed (0.05f, 8.0f, 0.8f), 0.6f, attr ([] (float v) { return String (v, 2) + " Hz"; }));
    add ("depth",     "Mod Depth",  NormalisableRange<float> (0.0f, 1.0f), 0.3f, pct);
    add ("highCut",   "High Cut",   skewed (500.0f, 20000.0f, 5000.0f), 9000.0f, hz);
    add ("lowCut",    "Low Cut",    skewed (10.0f, 1000.0f, 100.0f), 40.0f, hz);

    StringArray modeNames, colourNames;
    for (auto& m : DeepSpaceReverb::modes) modeNames.add (m.name);
    for (auto& c : DeepSpaceReverb::colours) colourNames.add (c.name);
    p.push_back (std::make_unique<AudioParameterChoice> (ParameterID { "mode", 1 }, "Mode", modeNames, 0));
    p.push_back (std::make_unique<AudioParameterChoice> (ParameterID { "colour", 1 }, "Color", colourNames, 1));

    return { p.begin(), p.end() };
}

DeepSpaceProcessor::DeepSpaceProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "DeepSpace", createLayout())
{
    auto get = [this] (const char* id) { return apvts.getRawParameterValue (id); };
    pMix = get ("mix");           pPredelay = get ("predelay"); pDecay = get ("decay");
    pHighFreq = get ("highFreq"); pHighShelf = get ("highShelf"); pBassFreq = get ("bassFreq"); pBassMult = get ("bassMult");
    pSize = get ("size");         pAttack = get ("attack");     pEarly = get ("early");       pLate = get ("late");
    pRate = get ("rate");         pDepth = get ("depth");       pHighCut = get ("highCut");   pLowCut = get ("lowCut");
    pMode = get ("mode");         pColour = get ("colour");

    setCurrentProgram (1);
}

bool DeepSpaceProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    const auto in = layouts.getMainInputChannelSet();
    return out == juce::AudioChannelSet::stereo()
        && (in == juce::AudioChannelSet::stereo() || in == juce::AudioChannelSet::mono());
}

void DeepSpaceProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    sr = sampleRate;
    reverb.prepare (sr);
    wet.setSize (2, samplesPerBlock);
    for (int ch = 0; ch < 2; ++ch) { highCut[ch].reset(); lowCut[ch].reset(); }
    lastHighCut = lastLowCut = -1.0f;
    dryS.reset (sr, 0.05); wetS.reset (sr, 0.05);
    const float mix = pMix->load();
    dryS.setCurrentAndTargetValue (std::cos (mix * juce::MathConstants<float>::halfPi));
    wetS.setCurrentAndTargetValue (std::sin (mix * juce::MathConstants<float>::halfPi) * 1.3f);
}

void DeepSpaceProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    const int numIn = getTotalNumInputChannels();
    if (n == 0) return;

    if (wet.getNumSamples() < n) wet.setSize (2, n, false, false, true);

    // mono input -> both sides
    if (numIn == 1 && buffer.getNumChannels() > 1)
        buffer.copyFrom (1, 0, buffer, 0, 0, n);

    DeepSpaceReverb::Params rp;
    rp.predelayMs = pPredelay->load(); rp.decay = pDecay->load();
    rp.highFreq = pHighFreq->load();   rp.highShelfDb = pHighShelf->load();
    rp.bassFreq = pBassFreq->load();   rp.bassMult = pBassMult->load();
    rp.size = pSize->load();           rp.attack = pAttack->load();
    rp.early = pEarly->load();         rp.late = pLate->load();
    rp.rate = pRate->load();           rp.depth = pDepth->load();
    rp.mode = (int) pMode->load();     rp.colour = (int) pColour->load();
    reverb.setParams (rp);

    const float colourLp = DeepSpaceReverb::colours[juce::jlimit (0, 2, rp.colour)].lowpass;
    const float hc = juce::jmin (pHighCut->load(), colourLp * 1.6f, (float) sr * 0.45f), lc = pLowCut->load();
    if (std::abs (hc - lastHighCut) > 0.01f)
    {
        auto c = juce::dsp::IIR::Coefficients<float>::makeLowPass (sr, hc);
        for (auto& f : highCut) f.coefficients = c;
        lastHighCut = hc;
    }
    if (std::abs (lc - lastLowCut) > 0.01f)
    {
        auto c = juce::dsp::IIR::Coefficients<float>::makeHighPass (sr, lc);
        for (auto& f : lowCut) f.coefficients = c;
        lastLowCut = lc;
    }

    auto* L = buffer.getWritePointer (0);
    auto* R = buffer.getWritePointer (1);
    auto* wl = wet.getWritePointer (0);
    auto* wr = wet.getWritePointer (1);
    reverb.process (L, R, wl, wr, n);

    const float mix = pMix->load();
    dryS.setTargetValue (std::cos (mix * juce::MathConstants<float>::halfPi));
    wetS.setTargetValue (std::sin (mix * juce::MathConstants<float>::halfPi) * 1.3f);

    double sum = 0.0;
    for (int i = 0; i < n; ++i)
    {
        const float a = highCut[0].processSample (lowCut[0].processSample (wl[i]));
        const float b = highCut[1].processSample (lowCut[1].processSample (wr[i]));
        sum += (double) (a * a + b * b);
        const float d = dryS.getNextValue(), w = wetS.getNextValue();
        L[i] = L[i] * d + a * w;
        R[i] = R[i] * d + b * w;
    }
    wetLevel.store ((float) std::sqrt (sum / (2.0 * n)));
}

//==============================================================================
int DeepSpaceProcessor::getNumPrograms() { return numPresets; }

const juce::String DeepSpaceProcessor::getProgramName (int index)
{
    return juce::isPositiveAndBelow (index, numPresets) ? presets[index].name : "";
}

void DeepSpaceProcessor::setCurrentProgram (int index)
{
    if (! juce::isPositiveAndBelow (index, numPresets))
        return;
    currentProgram = index;

    for (auto* param : getParameters())
        if (auto* p = dynamic_cast<juce::RangedAudioParameter*> (param))
            p->setValueNotifyingHost (p->getDefaultValue());

    for (auto& [id, value] : presets[index].values)
        if (auto* p = apvts.getParameter (id))
            p->setValueNotifyingHost (p->convertTo0to1 (value));
}

void DeepSpaceProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.setProperty ("program", currentProgram, nullptr);
    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void DeepSpaceProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
        {
            auto state = juce::ValueTree::fromXml (*xml);
            currentProgram = state.getProperty ("program", 0);
            apvts.replaceState (state);
        }
}

juce::AudioProcessorEditor* DeepSpaceProcessor::createEditor() { return new DeepSpaceEditor (*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new DeepSpaceProcessor(); }
