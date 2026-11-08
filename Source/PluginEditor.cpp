#include "PluginEditor.h"

using namespace vintage;

namespace
{
juce::String u8 (const char* t) { return juce::String (juce::CharPointer_UTF8 (t)); }

const juce::Colour lacquer   { 0xff3f4247 };   // Graphit (Mastering)
const juce::Colour silk      { 0xfff2eee4 };
const juce::Colour silkDim   { 0xc8f2eee4 };
const juce::Colour phosphorC { 0xff7dff9a };
}

//==============================================================================
void RackLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                                        float startAngle, float endAngle, juce::Slider& slider)
{
    const auto b = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h);
    const auto c = b.getCentre();
    const float R = juce::jmin (b.getWidth(), b.getHeight()) * 0.5f;
    const float knobR = juce::jmax (16.0f, R - 27.0f);
    const float angle = startAngle + pos * (endAngle - startAngle);

    // Skala im Siebdruck
    const auto steps = juce::StringArray::fromTokens (slider.getProperties()["stepLabels"].toString(), "|", "");
    g.setColour (silk);
    if (steps.size() > 1)
    {
        for (int i = 0; i < steps.size(); ++i)
        {
            const float a = startAngle + (float) i / (float) (steps.size() - 1) * (endAngle - startAngle);
            const auto p1 = c.getPointOnCircumference (knobR + 4.0f, a), p2 = c.getPointOnCircumference (knobR + 11.0f, a);
            g.drawLine ({ p1, p2 }, 1.6f);
            if (steps[i].isNotEmpty())
            {
                g.setFont (labelFont (steps[i].length() > 3 ? 10.5f : 12.0f, 0.03f));
                const auto tp = c.getPointOnCircumference (knobR + (steps[i].length() > 3 ? 22.0f : 19.0f), a);
                g.drawText (steps[i], juce::Rectangle<float> (56.0f, 16.0f).withCentre (tp), juce::Justification::centred);
            }
        }
    }
    else
    {
        for (int i = 0; i <= 20; ++i)
        {
            const float a = startAngle + (float) i / 20.0f * (endAngle - startAngle);
            const bool major = i % 2 == 0;
            g.drawLine ({ c.getPointOnCircumference (knobR + 4.0f, a), c.getPointOnCircumference (knobR + (major ? 11.0f : 7.0f), a) }, major ? 1.6f : 1.0f);
            if (major)
            {
                g.setFont (labelFont (12.0f, 0.03f));
                g.drawText (juce::String (i / 2), juce::Rectangle<float> (24.0f, 14.0f).withCentre (c.getPointOnCircumference (knobR + 19.0f, a)), juce::Justification::centred);
            }
        }
    }

    // Schatten
    g.setColour (juce::Colours::black.withAlpha (0.45f));
    g.fillEllipse (juce::Rectangle<float> (knobR * 2.0f, knobR * 2.0f).withCentre (c.translated (2.5f, 4.0f)));
    // Rock (geriffelt)
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff3a3836), c.x, c.y - knobR, juce::Colour (0xff0c0b0a), c.x, c.y + knobR, false));
    g.fillEllipse (juce::Rectangle<float> (knobR * 2.0f, knobR * 2.0f).withCentre (c));
    g.setColour (juce::Colours::black.withAlpha (0.6f));
    for (int k = 0; k < 48; ++k)
    {
        const float a = (float) k / 48.0f * juce::MathConstants<float>::twoPi;
        g.drawLine ({ c.getPointOnCircumference (knobR * 0.86f, a), c.getPointOnCircumference (knobR - 0.5f, a) }, 1.2f);
    }
    // Kappe
    const float capR = knobR * 0.74f;
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff4a4744), c.x - capR * 0.5f, c.y - capR, juce::Colour (0xff161514), c.x + capR * 0.4f, c.y + capR, false));
    g.fillEllipse (juce::Rectangle<float> (capR * 2.0f, capR * 2.0f).withCentre (c));
    g.setColour (juce::Colours::white.withAlpha (0.10f));
    g.fillEllipse (juce::Rectangle<float> (capR * 1.1f, capR * 0.55f).withCentre (c.translated (-capR * 0.2f, -capR * 0.5f)));
    g.setColour (juce::Colours::black.withAlpha (0.7f));
    g.drawEllipse (juce::Rectangle<float> (capR * 2.0f, capR * 2.0f).withCentre (c), 1.0f);
    // weiße Zeigerlinie über Kappe und Rock
    g.setColour (juce::Colour (0xfff5f1e6));
    g.drawLine ({ c.getPointOnCircumference (capR * 0.25f, angle), c.getPointOnCircumference (knobR - 1.5f, angle) }, 3.0f);
}

//==============================================================================
void WeiteAudioProcessorEditor::Scope::advance()
{
    // Nachleuchten: altes Bild langsam abdunkeln, neue Punkte hell einzeichnen
    juce::Graphics g (phosphor);
    g.setColour (juce::Colours::black.withAlpha (0.22f));
    g.fillAll();
    const int end = proc.scopePos.load();
    int count = (end - readPos + WeiteAudioProcessor::scopeLen) % WeiteAudioProcessor::scopeLen;
    if (count == 0) return;
    const float cx = getWidth() * 0.5f, cy = getHeight() * 0.5f, rad = getWidth() * 0.46f;

    // automatische Empfindlichkeit, damit auch leise Signale gut sichtbar sind
    float mx = 1.0e-4f;
    for (int i = 0, p = readPos; i < count; ++i, p = (p + 1) % WeiteAudioProcessor::scopeLen)
        mx = juce::jmax (mx, std::abs (proc.scopeL[(size_t) p].load()), std::abs (proc.scopeR[(size_t) p].load()));
    const float target = juce::jlimit (1.0f, 40.0f, 0.75f / mx);
    scale += (target - scale) * (target < scale ? 0.5f : 0.03f);

    g.setColour (phosphorC.withAlpha (0.8f));
    for (int i = 0; i < count; ++i)
    {
        const float l = proc.scopeL[(size_t) readPos].load() * scale, r = proc.scopeR[(size_t) readPos].load() * scale;
        readPos = (readPos + 1) % WeiteAudioProcessor::scopeLen;
        const float x = cx + (r - l) * 0.7071f * rad, y = cy - (l + r) * 0.7071f * rad;
        g.fillRect (x - 1.0f, y - 1.0f, 2.0f, 2.0f);
    }
}

void WeiteAudioProcessorEditor::Scope::paint (juce::Graphics& g)
{
    const auto b = getLocalBounds().toFloat();
    juce::Path screen; screen.addRoundedRectangle (b, 26.0f);
    g.saveState();
    g.reduceClipRegion (screen);
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff10261a), b.getCentreX(), b.getCentreY(), juce::Colour (0xff040a06), b.getX(), b.getY(), true));
    g.fillAll();
    // Raster
    const float cx = b.getCentreX(), cy = b.getCentreY(), rad = b.getWidth() * 0.46f;
    g.setColour (phosphorC.withAlpha (0.16f));
    for (int k = -4; k <= 4; ++k)
    {
        g.drawHorizontalLine ((int) (cy + k * rad / 4.0f), b.getX(), b.getRight());
        g.drawVerticalLine ((int) (cx + k * rad / 4.0f), b.getY(), b.getBottom());
    }
    g.setColour (phosphorC.withAlpha (0.3f));
    g.drawLine (cx - rad * 0.7071f, cy - rad * 0.7071f, cx + rad * 0.7071f, cy + rad * 0.7071f, 1.0f);
    g.drawLine (cx + rad * 0.7071f, cy - rad * 0.7071f, cx - rad * 0.7071f, cy + rad * 0.7071f, 1.0f);
    g.setFont (juce::FontOptions (11.0f, juce::Font::bold));
    g.drawText ("L", juce::Rectangle<float> (cx - rad * 0.74f - 10.0f, cy - rad * 0.74f - 10.0f, 20.0f, 20.0f), juce::Justification::centred);
    g.drawText ("R", juce::Rectangle<float> (cx + rad * 0.74f - 10.0f, cy - rad * 0.74f - 10.0f, 20.0f, 20.0f), juce::Justification::centred);
    g.drawText ("M", juce::Rectangle<float> (cx - 10.0f, b.getY() + 4.0f, 20.0f, 16.0f), juce::Justification::centred);
    g.drawText ("S", juce::Rectangle<float> (b.getRight() - 22.0f, cy - 18.0f, 16.0f, 16.0f), juce::Justification::centred);

    g.drawImageAt (phosphor, 0, 0);
    // Glas-Reflex
    g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.10f), b.getX(), b.getY(), juce::Colours::transparentWhite, b.getCentreX(), b.getCentreY(), false));
    g.fillEllipse (b.getX() - b.getWidth() * 0.3f, b.getY() - b.getHeight() * 0.4f, b.getWidth() * 1.1f, b.getHeight() * 0.8f);
    g.restoreState();
    g.setColour (juce::Colours::black);
    g.drawRoundedRectangle (b.reduced (0.5f), 26.0f, 2.0f);
}

void WeiteAudioProcessorEditor::CorrMeter::paint (juce::Graphics& g)
{
    const auto b = getLocalBounds().toFloat();
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xfff6ecd2), b.getCentreX(), b.getBottom(), juce::Colour (0xffd9c08c), b.getX(), b.getY(), true));
    g.fillRoundedRectangle (b, 4.0f);

    const juce::Point<float> pivot (b.getCentreX(), b.getBottom() + b.getHeight() * 0.25f);
    const float rad = b.getHeight() * 0.95f;
    const float span = 0.85f;   // Bogen ±0,85 rad
    auto angleFor = [&] (float v) { return v * span; };   // −1 links, +1 rechts

    // rote Zone −1 … 0
    juce::Path red;
    red.addCentredArc (pivot.x, pivot.y, rad - 4.0f, rad - 4.0f, 0.0f, angleFor (-1.0f), angleFor (0.0f), true);
    g.setColour (juce::Colour (0xffc62828));
    g.strokePath (red, juce::PathStrokeType (5.0f));
    juce::Path arc;
    arc.addCentredArc (pivot.x, pivot.y, rad, rad, 0.0f, angleFor (-1.0f), angleFor (1.0f), true);
    g.setColour (juce::Colour (0xff3b2a1e));
    g.strokePath (arc, juce::PathStrokeType (1.4f));

    g.setFont (lnf.labelFont (12.0f, 0.05f));
    const char* labels[] = { "\xe2\x88\x92" "1", "\xe2\x88\x92" "0,5", "0", "+0,5", "+1" };
    for (int i = 0; i <= 8; ++i)
    {
        const float v = -1.0f + i * 0.25f;
        const float a = angleFor (v);
        const bool major = i % 2 == 0;
        g.drawLine ({ pivot.getPointOnCircumference (rad, a), pivot.getPointOnCircumference (rad + (major ? 9.0f : 5.0f), a) }, major ? 1.5f : 1.0f);
        if (major)
            g.drawText (u8 (labels[i / 2]), juce::Rectangle<float> (44.0f, 14.0f).withCentre (pivot.getPointOnCircumference (rad + 20.0f, a)), juce::Justification::centred);
    }
    g.setFont (lnf.labelFont (11.0f, 0.2f));
    g.setColour (juce::Colour (0xff3b2a1e).withAlpha (0.8f));
    g.drawText ("KORRELATION", b.withTrimmedBottom (14.0f).withTrimmedTop (b.getHeight() * 0.52f), juce::Justification::centredTop);
    g.setFont (lnf.labelFont (9.0f, 0.15f));
    g.drawText ("GEGENPHASIG", b.reduced (8.0f, 6.0f), juce::Justification::bottomLeft);
    g.drawText ("MONO", b.reduced (8.0f, 6.0f), juce::Justification::bottomRight);

    g.saveState();
    g.reduceClipRegion (b.toNearestInt());
    const auto tip = pivot.getPointOnCircumference (rad + 6.0f, angleFor (needle));
    g.setColour (juce::Colours::black.withAlpha (0.2f));
    g.drawLine ({ pivot.translated (2.0f, 2.0f), tip.translated (2.0f, 2.0f) }, 1.6f);
    g.setColour (juce::Colour (0xff1a1410));
    g.drawLine ({ pivot, tip }, 1.6f);
    g.restoreState();
    g.setColour (juce::Colours::black.withAlpha (0.6f));
    g.drawRoundedRectangle (b, 4.0f, 1.0f);
}

//==============================================================================
WeiteAudioProcessorEditor::WeiteAudioProcessorEditor (WeiteAudioProcessor& p)
    : AudioProcessorEditor (&p), proc (p)
{
    setLookAndFeel (&lnf);
    addKnob ("monitor",  u8 ("ABH\xc3\x96REN"), "STEREO|MONO|SEITE");
    addKnob ("width",    "BREITE (%)", "0|50|100|150|200");
    addKnob ("balance",  "BALANCE", "L||0||R");
    addKnob ("monofreq", "BASS MONO BIS (HZ)", "40|80|160|320");
    addKnob ("output",   "AUSGANG (DB)", u8 ("\xe2\x88\x92" "12|\xe2\x88\x92" "6|0|+6|+12"));
    addToggle ("monobass", "EIN", "AUS", lampAmber);
    addToggle ("on", "EIN", "AUS", lampRed);

    for (int i = 0; i < proc.getNumPrograms(); ++i)
        presetBox.addItem (proc.getProgramName (i), i + 1);
    presetBox.setSelectedId (proc.getCurrentProgram() + 1, juce::dontSendNotification);
    presetBox.onChange = [this] { proc.setCurrentProgram (presetBox.getSelectedId() - 1); };
    addAndMakeVisible (presetBox);

    for (auto* c : std::initializer_list<juce::Component*> { &scope, &corr })
    {
        c->setInterceptsMouseClicks (false, false);
        addAndMakeVisible (c);
    }
    setSize (rackW, rackH);
    startTimerHz (30);
}

WeiteAudioProcessorEditor::~WeiteAudioProcessorEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

juce::Slider& WeiteAudioProcessorEditor::addKnob (const juce::String& paramID, const juce::String& name, const juce::String& steps)
{
    auto k = std::make_unique<Knob>();
    k->name = name;
    k->slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    k->slider.setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
    k->slider.setRotaryParameters (juce::MathConstants<float>::pi * 1.25f, juce::MathConstants<float>::pi * 2.75f, true);
    k->slider.setMouseDragSensitivity (paramID == "monitor" ? 120 : 240);
    k->slider.getProperties().set ("stepLabels", steps);
    addAndMakeVisible (k->slider);
    k->attachment = std::make_unique<SliderAttachment> (proc.apvts, paramID, k->slider);
    if (auto* rp = proc.apvts.getParameter (paramID))
        k->slider.setDoubleClickReturnValue (true, rp->convertFrom0to1 (rp->getDefaultValue()));
    knobs.push_back (std::move (k));
    return knobs.back()->slider;
}

juce::ToggleButton& WeiteAudioProcessorEditor::addToggle (const juce::String& paramID, const juce::String& onText, const juce::String& offText, juce::Colour lamp)
{
    auto b = std::make_unique<juce::ToggleButton>();
    b->setButtonText ("x");
    b->getProperties().set ("onText", onText);
    b->getProperties().set ("offText", offText);
    b->getProperties().set ("lamp", (int) lamp.getARGB());
    b->setColour (juce::ToggleButton::textColourId, silk);
    addAndMakeVisible (*b);
    buttonAttachments.push_back (std::make_unique<ButtonAttachment> (proc.apvts, paramID, *b));
    toggles.push_back (std::move (b));
    return *toggles.back();
}

void WeiteAudioProcessorEditor::timerCallback()
{
    scope.advance();
    scope.repaint();
    // Zeiger mit etwas Trägheit
    const float target = proc.correlation.load();
    corr.vel += ((target - corr.needle) * 0.35f - corr.vel) * 0.5f;
    corr.needle += corr.vel;
    corr.repaint();

    if (presetBox.getSelectedId() != proc.getCurrentProgram() + 1)
        presetBox.setSelectedId (proc.getCurrentProgram() + 1, juce::dontSendNotification);
}

//==============================================================================
void WeiteAudioProcessorEditor::renderPanel()
{
    panelImage = juce::Image (juce::Image::ARGB, rackW, rackH, true);
    juce::Graphics g (panelImage);
    g.setGradientFill (juce::ColourGradient (lacquer.brighter (0.12f), 0, 0, lacquer.darker (0.18f), 0, (float) rackH, false));
    g.fillRect (0, 0, rackW, rackH);
    juce::Random rnd (13);
    for (int i = 0; i < 12000; ++i)
    {
        const float r = 1.5f + rnd.nextFloat() * 4.0f;
        const float x = rnd.nextFloat() * rackW, y = rnd.nextFloat() * rackH;
        g.setColour ((rnd.nextBool() ? juce::Colours::white : juce::Colours::black).withAlpha (0.012f + rnd.nextFloat() * 0.02f));
        g.fillEllipse (x - r, y - r, 2 * r, 2 * r);
    }
    {
        juce::Image::BitmapData data (panelImage, juce::Image::BitmapData::readWrite);
        for (int y = 0; y < rackH; ++y)
            for (int x = 0; x < rackW; ++x)
            {
                auto c = data.getPixelColour (x, y);
                const int n = rnd.nextInt (9) - 4;
                data.setPixelColour (x, y, juce::Colour ((juce::uint8) juce::jlimit (0, 255, c.getRed() + n),
                                                         (juce::uint8) juce::jlimit (0, 255, c.getGreen() + n),
                                                         (juce::uint8) juce::jlimit (0, 255, c.getBlue() + n)));
            }
    }
    g.setColour (juce::Colours::white.withAlpha (0.25f)); g.drawHorizontalLine (0, 0.0f, (float) rackW);
    g.setColour (juce::Colours::black.withAlpha (0.45f)); g.drawHorizontalLine (rackH - 1, 0.0f, (float) rackW);

    // Rack-Ohren mit Langlöchern
    for (float ex : { 0.0f, (float) rackW - 56.0f })
    {
        g.setColour (juce::Colours::black.withAlpha (0.25f));
        g.drawVerticalLine ((int) (ex < 1.0f ? 56.0f : ex), 0.0f, (float) rackH);
        g.setColour (juce::Colours::white.withAlpha (0.12f));
        g.drawVerticalLine ((int) (ex < 1.0f ? 57.0f : ex + 1.0f), 0.0f, (float) rackH);
        for (float sy : { 44.0f, (float) rackH * 0.5f, (float) rackH - 44.0f })
        {
            const auto slot = juce::Rectangle<float> (ex + 14.0f, sy - 8.0f, 28.0f, 16.0f);
            g.setColour (juce::Colour (0xff141210));
            g.fillRoundedRectangle (slot, 8.0f);
            g.setColour (juce::Colours::white.withAlpha (0.18f));
            g.drawRoundedRectangle (slot.translated (0.0f, 1.0f), 8.0f, 1.0f);
            VintageLookAndFeel::drawScrew (g, slot.getCentre().translated (ex < 1.0f ? 3.0f : -3.0f, 0.0f), 6.5f, sy * 0.01f + ex * 0.003f);
        }
    }

    const juce::String titles[] = { "LEUCHTSCHIRM", "KORRELATION", "STEREO", "BASS MONO", "SUMME" };
    for (size_t i = 0; i < sections.size(); ++i)
    {
        const auto r = sections[i].toFloat();
        g.setColour (silkDim);
        juce::Path box; box.addRoundedRectangle (r, 5.0f);
        g.strokePath (box, juce::PathStrokeType (1.2f));
        const auto f = lnf.labelFont (13.0f, 0.3f);
        const float tw = (float) juce::GlyphArrangement::getStringWidthInt (f, titles[i]) + 20.0f;
        const auto tr = juce::Rectangle<float> (tw, 16.0f).withCentre ({ r.getCentreX(), r.getY() });
        g.setColour (lacquer);
        g.fillRect (tr);
        g.setColour (silk);
        g.setFont (f);
        g.drawText (titles[i], tr, juce::Justification::centred);
    }

    // Rahmen um Schirm und Messinstrument
    for (auto bz : { scopeBezel, corrBezel })
    {
        const auto r = bz.toFloat();
        g.setColour (juce::Colours::black.withAlpha (0.4f));
        g.fillRoundedRectangle (r.translated (2.0f, 3.0f), 12.0f);
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff2c2a28), 0, r.getY(), juce::Colour (0xff121110), 0, r.getBottom(), false));
        g.fillRoundedRectangle (r, 12.0f);
    }

    for (auto& k : knobs)
    {
        const auto sb = k->slider.getBounds().toFloat();
        g.setColour (silk);
        g.setFont (lnf.labelFont (12.0f, 0.16f));
        g.drawText (k->name, juce::Rectangle<float> (sb.getX() - 30.0f, sb.getBottom() - 8.0f, sb.getWidth() + 60.0f, 16.0f), juce::Justification::centred);
    }
    for (auto& [r, t] : toggleLabels)
    {
        g.setColour (silk);
        g.setFont (lnf.labelFont (11.0f, 0.22f));
        g.drawText (t, r.toFloat(), juce::Justification::centredLeft);
    }
    g.setColour (silkDim);
    g.setFont (lnf.labelFont (10.0f, 0.18f));
    g.drawText (u8 ("MITTE SENKRECHT \xc2\xb7 BREITE WAAGERECHT"), juce::Rectangle<float> ((float) scopeBezel.getX(), (float) scopeBezel.getBottom() + 8.0f, (float) scopeBezel.getWidth(), 14.0f), juce::Justification::centred);

    // Typenschild
    const auto np = nameplate.toFloat();
    g.setColour (juce::Colours::black.withAlpha (0.4f));
    g.fillRoundedRectangle (np.translated (1.5f, 2.5f), 4.0f);
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff2a2522), 0, np.getY(), juce::Colour (0xff121010), 0, np.getBottom(), false));
    g.fillRoundedRectangle (np, 4.0f);
    g.setColour (gold.withAlpha (0.7f));
    g.drawRoundedRectangle (np.reduced (3.0f), 3.0f, 1.0f);
    g.setFont (lnf.scriptFont (34.0f));
    g.setColour (logo);
    g.drawText ("Klangbild", np.withTrimmedLeft (12.0f).withHeight (44.0f).translated (0.0f, 2.0f), juce::Justification::centredLeft);
    g.setColour (gold);
    g.setFont (lnf.labelFont (17.0f, 0.45f));
    g.drawText ("WEITE", np.withTrimmedLeft (16.0f).withTrimmedTop (44.0f).withHeight (20.0f), juce::Justification::centredLeft);
    g.setColour (logo.withAlpha (0.6f));
    g.setFont (lnf.labelFont (9.0f, 0.2f));
    g.drawText ("STEREO-WERKZEUG", np.withTrimmedLeft (16.0f).withTrimmedTop (66.0f).withHeight (14.0f), juce::Justification::centredLeft);

    g.setColour (silk);
    g.setFont (lnf.labelFont (11.0f, 0.22f));
    g.drawText ("PROGRAMM", presetBox.getBounds().toFloat().withY ((float) presetBox.getY() - 16.0f).withHeight (14.0f), juce::Justification::centredLeft);
    g.setColour (silkDim);
    g.setFont (lnf.labelFont (10.0f, 0.3f));
    g.drawText (u8 ("TYP KB-2S  \xc2\xb7  NR. 0614"), tagArea.toFloat(), juce::Justification::centredRight);
}

void WeiteAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.drawImageAt (panelImage, 0, 0);
}

void WeiteAudioProcessorEditor::resized()
{
    const int top = 30, bottom = rackH - 34;
    sections[0] = { 72, top, 320, bottom - top };
    sections[1] = { 404, top, 290, bottom - top };
    sections[2] = { 706, top, 160, bottom - top };
    sections[3] = { 878, top, 160, bottom - top };
    const int colX = 1050, colW = rackW - 56 - 16 - colX;
    sections[4] = { colX, top + 176, colW, bottom - top - 176 };

    scopeBezel = juce::Rectangle<int> (292, 292).withCentre ({ sections[0].getCentreX(), top + 186 });
    scope.setBounds (scopeBezel.reduced (10));
    corrBezel = juce::Rectangle<int> (266, 160).withCentre ({ sections[1].getCentreX(), top + 112 });
    corr.setBounds (corrBezel.reduced (9));

    knobs[0]->slider.setBounds (sections[1].getCentreX() - 75, top + 236, 150, 150);   // Abhören
    knobs[1]->slider.setBounds (sections[2].getCentreX() - 75, top + 30, 150, 150);    // Breite
    knobs[2]->slider.setBounds (sections[2].getCentreX() - 75, top + 236, 150, 150);   // Balance
    knobs[3]->slider.setBounds (sections[3].getCentreX() - 75, top + 30, 150, 150);    // Bass mono bis
    toggles[0]->setBounds (sections[3].getX() + 20, top + 270, 130, 40);
    toggleLabels.clear();
    toggleLabels.push_back ({ { sections[3].getX() + 24, top + 252, 120, 14 }, "BASS MONO" });

    nameplate = { colX, top - 8, colW, 88 };
    presetBox.setBounds (colX, top + 112, colW, 28);
    knobs[4]->slider.setBounds (colX + (colW - 140) / 2, top + 196, 140, 140);
    toggles[1]->setBounds (colX + 14, top + 376, 130, 40);
    toggleLabels.push_back ({ { colX + 18, top + 360, 120, 14 }, "WEITE" });
    tagArea = { colX - 60, rackH - 28, colW + 60, 16 };

    renderPanel();
}
