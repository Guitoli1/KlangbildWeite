#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"
#include "VintageUI.h"

// Look im Stil eines 19-Zoll-Studiogeräts: blaugrauer Hammerschlaglack, weißer Siebdruck, schwarze Zeigerknöpfe
class RackLookAndFeel : public VintageLookAndFeel
{
public:
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos, float startAngle, float endAngle, juce::Slider&) override;
};

class WeiteAudioProcessorEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit WeiteAudioProcessorEditor (WeiteAudioProcessor&);
    ~WeiteAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;

    struct Knob { juce::Slider slider; juce::String name; std::unique_ptr<SliderAttachment> attachment; };

    juce::Slider& addKnob (const juce::String& paramID, const juce::String& name, const juce::String& steps);
    juce::ToggleButton& addToggle (const juce::String& paramID, const juce::String& onText, const juce::String& offText, juce::Colour lamp);
    void timerCallback() override;
    void renderPanel();

    // Leuchtschirm (Goniometer): Mitte senkrecht, Seite waagerecht, grünes Nachleuchten
    struct Scope : public juce::Component
    {
        explicit Scope (WeiteAudioProcessor& p) : proc (p) {}
        WeiteAudioProcessor& proc;
        juce::Image phosphor;
        int readPos = 0;
        float scale = 2.0f;
        void advance();
        void paint (juce::Graphics&) override;
        void resized() override { phosphor = juce::Image (juce::Image::ARGB, juce::jmax (1, getWidth()), juce::jmax (1, getHeight()), true); }
    };

    // Korrelationsmesser als Zeigerinstrument (−1 … +1)
    struct CorrMeter : public juce::Component
    {
        explicit CorrMeter (VintageLookAndFeel& l) : lnf (l) {}
        VintageLookAndFeel& lnf;
        float value = 1.0f, needle = 1.0f, vel = 0.0f;
        void paint (juce::Graphics&) override;
    };

    WeiteAudioProcessor& proc;
    RackLookAndFeel lnf;

    std::vector<std::unique_ptr<Knob>> knobs;
    std::vector<std::unique_ptr<juce::ToggleButton>> toggles;
    std::vector<std::unique_ptr<ButtonAttachment>> buttonAttachments;
    juce::ComboBox presetBox;
    Scope scope { proc };
    CorrMeter corr { lnf };

    static constexpr int rackW = 1280, rackH = 520;
    juce::Rectangle<int> nameplate, tagArea, scopeBezel, corrBezel;
    std::array<juce::Rectangle<int>, 5> sections;
    std::vector<std::pair<juce::Rectangle<int>, juce::String>> toggleLabels;
    juce::Image panelImage;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (WeiteAudioProcessorEditor)
};
