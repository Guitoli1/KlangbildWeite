#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "ScaledEditor.h"

namespace
{
using APF = juce::AudioParameterFloat;
using Attr = juce::AudioParameterFloatAttributes;

juce::NormalisableRange<float> logRange (float lo, float hi)
{
    return { lo, hi,
             [] (float a, float b, float n) { return a * std::pow (b / a, n); },
             [] (float a, float b, float v) { return std::log (v / a) / std::log (b / a); },
             [] (float a, float b, float v) { return juce::jlimit (a, b, v); } };
}

Attr unit (const char* u, int decimals, bool sign = false)
{
    return Attr().withStringFromValueFunction ([=] (float v, int)
                 {
                     auto s = juce::String (std::abs (v) < 0.05f && decimals > 0 ? 0.0f : v, decimals).replaceCharacter ('.', ',');
                     if (sign && v > 0.049f) s = "+" + s;
                     return (s + " " + u).trim();
                 })
                 .withValueFromStringFunction ([] (const juce::String& s) { return s.replaceCharacter (',', '.').getFloatValue(); });
}
}

juce::AudioProcessorValueTreeState::ParameterLayout WeiteAudioProcessor::createLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout l;
    l.add (std::make_unique<APF> (juce::ParameterID { "width", 1 }, "Breite", juce::NormalisableRange<float> (0.0f, 200.0f, 0.1f), 100.0f, unit ("%", 0)));
    l.add (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { "monobass", 1 }, "Bass mono", false));
    l.add (std::make_unique<APF> (juce::ParameterID { "monofreq", 1 }, "Bass mono bis", logRange (40.0f, 320.0f), 120.0f, unit ("Hz", 0)));
    l.add (std::make_unique<APF> (juce::ParameterID { "balance", 1 }, "Balance", juce::NormalisableRange<float> (-100.0f, 100.0f, 0.1f), 0.0f, unit ("", 0, true)));
    l.add (std::make_unique<APF> (juce::ParameterID { "output", 1 }, "Ausgang", juce::NormalisableRange<float> (-12.0f, 12.0f, 0.1f), 0.0f, unit ("dB", 1, true)));
    l.add (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { "monitor", 1 }, juce::String (juce::CharPointer_UTF8 ("Abh\xc3\xb6ren")),
                                                         juce::StringArray { "Stereo", "Mono", "Seite" }, 0));
    l.add (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { "on", 1 }, "Weite an", true));
    return l;
}

const std::vector<WeiteAudioProcessor::Preset>& WeiteAudioProcessor::presets()
{
    static const std::vector<Preset> p = {
        { "Neutral",                        { { "width", 100 }, { "monobass", 0 }, { "monofreq", 120 }, { "balance", 0 } } },
        { "Bass mono (120 Hz)",             { { "width", 100 }, { "monobass", 1 }, { "monofreq", 120 }, { "balance", 0 } } },
        { "Etwas breiter",                  { { "width", 125 }, { "monobass", 1 }, { "monofreq", 100 }, { "balance", 0 } } },
        { "Breit",                          { { "width", 150 }, { "monobass", 1 }, { "monofreq", 150 }, { "balance", 0 } } },
        { "Schmaler",                       { { "width", 70 },  { "monobass", 0 }, { "monofreq", 120 }, { "balance", 0 } } },
        { "Mono (fast)",                    { { "width", 20 },  { "monobass", 1 }, { "monofreq", 200 }, { "balance", 0 } } },
    };
    return p;
}

void WeiteAudioProcessor::setCurrentProgram (int index)
{
    if (! juce::isPositiveAndBelow (index, (int) presets().size()))
        return;
    currentProgram = index;
    for (auto& [id, value] : presets()[(size_t) index].values)
        if (auto* param = apvts.getParameter (id))
            param->setValueNotifyingHost (param->convertTo0to1 (value));
}

const juce::String WeiteAudioProcessor::getProgramName (int index)
{
    return juce::isPositiveAndBelow (index, (int) presets().size())
               ? juce::String (juce::CharPointer_UTF8 (presets()[(size_t) index].name)) : juce::String();
}

WeiteAudioProcessor::WeiteAudioProcessor()
    : AudioProcessor (BusesProperties().withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "KlangbildWeite", createLayout())
{
    for (auto& v : scopeL) v.store (0.0f);
    for (auto& v : scopeR) v.store (0.0f);
}

bool WeiteAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    return layouts.getMainInputChannelSet() == juce::AudioChannelSet::stereo()
        && layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

void WeiteAudioProcessor::prepareToPlay (double sampleRate, int)
{
    sr = sampleRate;
    lastMonoFreq = -1;
    for (auto& s : hpState) s.reset();
    for (auto& s : midHpState) s.reset();
    for (auto& s : midLpState) s.reset();
    auto v = [this] (const char* id) { return apvts.getRawParameterValue (id)->load(); };
    smWidth.reset (sr, 0.03);    smWidth.setCurrentAndTargetValue (v ("width") * 0.01f);
    smMonoBass.reset (sr, 0.03); smMonoBass.setCurrentAndTargetValue (v ("monobass") > 0.5f ? 1.0f : 0.0f);
    smGainL.reset (sr, 0.03);    smGainL.setCurrentAndTargetValue (1.0f);
    smGainR.reset (sr, 0.03);    smGainR.setCurrentAndTargetValue (1.0f);
    smOn.reset (sr, 0.02);       smOn.setCurrentAndTargetValue (v ("on") > 0.5f ? 1.0f : 0.0f);
    avgCoef = 1.0 - std::exp (-1.0 / (0.3 * sr));   // Korrelation über ~300 ms
    eLL = eRR = eLR = 0;
    decimStep = juce::jmax (1, (int) std::lround (sr / 24000.0));
}

void WeiteAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    if (buffer.getNumChannels() < 2)
        return;

    auto v = [this] (const char* id) { return apvts.getRawParameterValue (id)->load(); };
    const float mf = v ("monofreq");
    if (! juce::exactlyEqual (mf, lastMonoFreq))
    {
        lastMonoFreq = mf;
        sideHp = eq::makeHighPass (sr, mf, 0.70710678);   // zweimal hintereinander = Linkwitz-Riley 4. Ordnung
        midLp  = eq::makeLowPass  (sr, mf, 0.70710678);
    }
    smWidth.setTargetValue (v ("width") * 0.01f);
    smMonoBass.setTargetValue (v ("monobass") > 0.5f ? 1.0f : 0.0f);
    // Balance: Gegenseite absenken (bei ±100 ganz aus), Mitte bleibt bei 0 dB
    const float bal = v ("balance") * 0.01f;
    const float out = juce::Decibels::decibelsToGain (v ("output"));
    smGainL.setTargetValue (out * (bal > 0.0f ? 1.0f - bal : 1.0f));
    smGainR.setTargetValue (out * (bal < 0.0f ? 1.0f + bal : 1.0f));
    smOn.setTargetValue (v ("on") > 0.5f ? 1.0f : 0.0f);
    const int monitor = (int) v ("monitor");

    float* L = buffer.getWritePointer (0);
    float* R = buffer.getWritePointer (1);
    int sp = scopePos.load();

    for (int i = 0; i < n; ++i)
    {
        const float l = L[i], r = R[i];
        float m = 0.5f * (l + r);
        float s = 0.5f * (l - r) * smWidth.getNextValue();
        // Bass mono: tiefe Anteile aus der Seite herausfiltern. Die Mitte läuft durch den passenden Allpass
        // (Tiefpass + Hochpass der Weiche), damit Mitte und Seite oberhalb der Grenze phasengleich bleiben.
        const float sHp = hpState[1].process (sideHp, hpState[0].process (sideHp, s));
        const float mAp = midLpState[1].process (midLp, midLpState[0].process (midLp, m))
                        + midHpState[1].process (sideHp, midHpState[0].process (sideHp, m));
        const float mb = smMonoBass.getNextValue();
        s = s + (sHp - s) * mb;
        m = m + (mAp - m) * mb;

        float yl = (m + s) * smGainL.getNextValue();
        float yr = (m - s) * smGainR.getNextValue();
        const float on = smOn.getNextValue();
        yl = l + (yl - l) * on;
        yr = r + (yr - r) * on;

        // Anzeigen messen das bearbeitete Stereobild (vor dem Abhör-Schalter)
        eLL += ((double) yl * yl - eLL) * avgCoef;
        eRR += ((double) yr * yr - eRR) * avgCoef;
        eLR += ((double) yl * yr - eLR) * avgCoef;
        if (++decim >= decimStep)
        {
            decim = 0;
            scopeL[(size_t) sp].store (yl); scopeR[(size_t) sp].store (yr);
            sp = (sp + 1) % scopeLen;
        }

        // Abhören
        if (monitor == 1)      { const float mm = 0.5f * (yl + yr); yl = yr = mm; }
        else if (monitor == 2) { const float ss = 0.5f * (yl - yr); yl = yr = ss; }
        L[i] = yl; R[i] = yr;
    }
    scopePos.store (sp);

    const double den = std::sqrt (eLL * eRR);
    correlation.store (den > 1.0e-10 ? (float) juce::jlimit (-1.0, 1.0, eLR / den) : 1.0f);
    const double tot = eLL + eRR;
    balanceMeter.store (tot > 1.0e-10 ? (float) ((eRR - eLL) / tot) : 0.0f);
}

void WeiteAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.setProperty ("program", currentProgram, nullptr);
    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void WeiteAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    auto xml = getXmlFromBinary (data, sizeInBytes);
    if (xml == nullptr || ! xml->hasTagName (apvts.state.getType()))
        return;
    auto tree = juce::ValueTree::fromXml (*xml);
    currentProgram = juce::jlimit (0, getNumPrograms() - 1, (int) tree.getProperty ("program", 0));
    apvts.replaceState (tree);
    for (auto* p : getParameters())
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (p))
            rp->setValueNotifyingHost (rp->convertTo0to1 (rp->convertFrom0to1 (rp->getValue())));
}

juce::AudioProcessorEditor* WeiteAudioProcessor::createEditor() { return new kb::ScaledEditor (*this, new WeiteAudioProcessorEditor (*this)); }

#ifndef KB_EMBEDDED
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new WeiteAudioProcessor(); }
#endif
