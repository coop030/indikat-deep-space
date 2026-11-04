#include "PluginEditor.h"
#include "BinaryData.h"

namespace Palette
{
    const juce::Colour space0  { 0xff04060a };
    const juce::Colour space1  { 0xff0a111d };
    const juce::Colour hair    { 0xff1a2333 };
    const juce::Colour hairHi  { 0xff2b384c };
    const juce::Colour text    { 0xffd2dbe8 };
    const juce::Colour dim     { 0xff66738a };
    const juce::Colour faint   { 0xff38455a };
    const juce::Colour accent  { 0xff7fe6ff };   // ice cyan
    const juce::Colour counter { 0xffa292ff };   // negative side of bipolar knobs
}

namespace
{
    void strokeGlow (juce::Graphics& g, const juce::Path& p, juce::Colour c, float width)
    {
        g.setColour (c.withAlpha (0.06f));
        g.strokePath (p, juce::PathStrokeType (width * 5.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setColour (c.withAlpha (0.14f));
        g.strokePath (p, juce::PathStrokeType (width * 2.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setColour (c);
        g.strokePath (p, juce::PathStrokeType (width, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    void glowDot (juce::Graphics& g, juce::Point<float> c, float r, juce::Colour col)
    {
        g.setColour (col.withMultipliedAlpha (0.08f));
        g.fillEllipse (juce::Rectangle<float> (r * 6.0f, r * 6.0f).withCentre (c));
        g.setColour (col.withMultipliedAlpha (0.2f));
        g.fillEllipse (juce::Rectangle<float> (r * 3.0f, r * 3.0f).withCentre (c));
        g.setColour (col);
        g.fillEllipse (juce::Rectangle<float> (r * 2.0f, r * 2.0f).withCentre (c));
    }
}

//==============================================================================
IndikatFonts::IndikatFonts()
{
    michroma  = juce::Typeface::createSystemTypefaceFor (BinaryData::MichromaRegular_ttf, BinaryData::MichromaRegular_ttfSize);
    questrial = juce::Typeface::createSystemTypefaceFor (BinaryData::QuestrialRegular_ttf, BinaryData::QuestrialRegular_ttfSize);
    shareTech = juce::Typeface::createSystemTypefaceFor (BinaryData::ShareTechMonoRegular_ttf, BinaryData::ShareTechMonoRegular_ttfSize);
}

//==============================================================================
IndikatLookAndFeel::IndikatLookAndFeel()
{
    setColour (juce::Slider::textBoxTextColourId, Palette::text);
    setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxHighlightColourId, Palette::accent.withAlpha (0.3f));
    setColour (juce::Label::textColourId, Palette::dim);
    setColour (juce::Label::textWhenEditingColourId, Palette::text);
    setColour (juce::TextEditor::backgroundColourId, Palette::space1);
    setColour (juce::TextEditor::outlineColourId, Palette::hairHi);
    setColour (juce::TextEditor::focusedOutlineColourId, Palette::accent);
    setColour (juce::CaretComponent::caretColourId, Palette::accent);
    setColour (juce::ComboBox::textColourId, Palette::text);
    setColour (juce::PopupMenu::backgroundColourId, Palette::space1);
    setColour (juce::PopupMenu::textColourId, Palette::text);
    setColour (juce::ResizableWindow::backgroundColourId, Palette::space0);
}

void IndikatLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                                           float startAngle, float endAngle, juce::Slider& s)
{
    if (s.getProperties().contains ("big"))
    {
        drawDecayDial (g, juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h), pos, startAngle, endAngle, s);
        return;
    }

    const float size = (float) juce::jmin (w, h);
    const auto centre = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h).getCentre();
    const float outer = size * 0.5f - 4.0f;
    const float ring = outer * 0.80f;
    const float angle = startAngle + pos * (endAngle - startAngle);
    const bool active = s.isMouseOverOrDragging();

    auto pointOn = [&] (float radius, float a) { return centre.getPointOnCircumference (radius, a); };

    // scale: fine dots outside
    for (int i = 0; i <= 10; ++i)
    {
        const float a = startAngle + (float) i / 10.0f * (endAngle - startAngle);
        const bool major = i == 0 || i == 5 || i == 10;
        const float r = major ? 1.1f : 0.7f;
        g.setColour ((i == 0 || i == 10) ? Palette::dim : Palette::faint);
        g.fillEllipse (juce::Rectangle<float> (r * 2.0f, r * 2.0f).withCentre (pointOn (outer, a)));
    }

    // orbit
    juce::Path track;
    track.addCentredArc (centre.x, centre.y, ring, ring, 0.0f, startAngle, endAngle, true);
    g.setColour (active ? Palette::hairHi : Palette::hair);
    g.strokePath (track, juce::PathStrokeType (1.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // value arc
    const bool bipolar = s.getMinimum() < 0.0 && s.getMaximum() > 0.0;
    const float from = bipolar ? (startAngle + endAngle) * 0.5f : startAngle;
    const auto colour = (bipolar && angle < from) ? Palette::counter : Palette::accent;

    if (std::abs (angle - from) > 0.01f)
    {
        juce::Path arc;
        arc.addCentredArc (centre.x, centre.y, ring, ring, 0.0f, juce::jmin (from, angle), juce::jmax (from, angle), true);
        strokeGlow (g, arc, colour.withMultipliedAlpha (active ? 1.0f : 0.85f), 2.0f);
    }

    // core
    const float core = ring * 0.62f;
    g.setGradientFill (juce::ColourGradient (Palette::space1.brighter (0.15f), centre.x, centre.y - core,
                                             Palette::space0, centre.x, centre.y + core, false));
    g.fillEllipse (juce::Rectangle<float> (core * 2.0f, core * 2.0f).withCentre (centre));
    g.setColour (active ? Palette::hairHi : Palette::hair);
    g.drawEllipse (juce::Rectangle<float> (core * 2.0f, core * 2.0f).withCentre (centre), 1.0f);

    // pointer
    g.setColour (active ? Palette::text : Palette::dim);
    g.drawLine (juce::Line<float> (pointOn (core * 0.35f, angle), pointOn (core * 0.8f, angle)), 1.5f);

    // satellite
    glowDot (g, pointOn (ring, angle), active ? 3.6f : 3.0f, colour);
}

juce::Label* IndikatLookAndFeel::createSliderTextBox (juce::Slider& s)
{
    auto* l = LookAndFeel_V4::createSliderTextBox (s);
    l->setFont (fonts->mono (12.5f));
    l->setColour (juce::Label::textColourId, Palette::text);
    l->setColour (juce::Label::backgroundColourId, juce::Colours::transparentBlack);
    l->setColour (juce::Label::outlineColourId, juce::Colours::transparentBlack);
    return l;
}

void IndikatLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&, bool highlighted, bool down)
{
    if (highlighted || down)
    {
        g.setColour (Palette::accent.withAlpha (down ? 0.12f : 0.05f));
        g.fillEllipse (b.getLocalBounds().toFloat().reduced (2.0f));
    }
}

void IndikatLookAndFeel::drawButtonText (juce::Graphics& g, juce::TextButton& b, bool highlighted, bool)
{
    g.setColour (highlighted ? Palette::accent : Palette::dim);
    g.setFont (fonts->mono (15.0f));
    g.drawText (b.getButtonText(), b.getLocalBounds(), juce::Justification::centred);
}

void IndikatLookAndFeel::drawComboBox (juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox& box)
{
    const bool hot = box.isMouseOver (true) || box.isPopupActive();
    g.setColour (hot ? Palette::accent.withAlpha (0.7f) : Palette::hairHi);
    g.fillRect (0.0f, (float) h - 1.0f, (float) w, 1.0f);

    juce::Path chevron;
    const float cx = (float) w - 7.0f, cy = (float) h * 0.5f;
    chevron.startNewSubPath (cx - 3.5f, cy - 1.5f);
    chevron.lineTo (cx, cy + 2.0f);
    chevron.lineTo (cx + 3.5f, cy - 1.5f);
    g.setColour (hot ? Palette::accent : Palette::dim);
    g.strokePath (chevron, juce::PathStrokeType (1.2f));
}

juce::Font IndikatLookAndFeel::getComboBoxFont (juce::ComboBox&) { return fonts->mono (13.0f); }

void IndikatLookAndFeel::positionComboBoxText (juce::ComboBox& box, juce::Label& label)
{
    label.setBounds (0, 0, box.getWidth() - 16, box.getHeight() - 1);
    label.setFont (getComboBoxFont (box));
    label.setBorderSize ({ 0, 2, 0, 0 });
}

void IndikatLookAndFeel::drawPopupMenuBackground (juce::Graphics& g, int w, int h)
{
    g.fillAll (Palette::space1);
    g.setColour (Palette::hairHi);
    g.drawRect (0, 0, w, h, 1);
}

void IndikatLookAndFeel::drawPopupMenuItem (juce::Graphics& g, const juce::Rectangle<int>& area, bool isSeparator, bool,
                                            bool isHighlighted, bool isTicked, bool, const juce::String& text,
                                            const juce::String&, const juce::Drawable*, const juce::Colour*)
{
    if (isSeparator)
    {
        g.setColour (Palette::hair);
        g.fillRect (area.reduced (8, 0).withHeight (1).withY (area.getCentreY()));
        return;
    }

    auto r = area.reduced (1, 0);
    if (isHighlighted)
    {
        g.setColour (Palette::accent.withAlpha (0.07f));
        g.fillRect (r);
    }
    if (isTicked)
        glowDot (g, { (float) r.getX() + 10.0f, (float) r.getCentreY() }, 2.0f, Palette::accent);

    g.setColour (isHighlighted || isTicked ? Palette::accent : Palette::text);
    g.setFont (getPopupMenuFont());
    g.drawText (text, r.withTrimmedLeft (22), juce::Justification::centredLeft);
}

juce::Font IndikatLookAndFeel::getPopupMenuFont() { return fonts->mono (14.0f); }

juce::Font IndikatLookAndFeel::getLabelFont (juce::Label& l) { return l.getFont(); }

//==============================================================================

//==============================================================================
// Big Decay dial: rings, tick scale, glowing arc and a small starfield in the core
void IndikatLookAndFeel::drawDecayDial (juce::Graphics& g, juce::Rectangle<float> area, float pos,
                                        float startAngle, float endAngle, juce::Slider& s)
{
    const auto centre = area.getCentre();
    const float R = juce::jmin (area.getWidth(), area.getHeight()) * 0.5f - 2.0f;
    const float angle = startAngle + pos * (endAngle - startAngle);
    const float level = (float) s.getProperties().getWithDefault ("level", 0.0);
    const float spin = (float) s.getProperties().getWithDefault ("spin", 0.0);
    const bool active = s.isMouseOverOrDragging();
    auto on = [&] (float r, float a) { return centre.getPointOnCircumference (r, a); };

    g.setColour (Palette::hair);
    g.drawEllipse (juce::Rectangle<float> (R * 2.0f, R * 2.0f).withCentre (centre), 1.0f);
    g.drawEllipse (juce::Rectangle<float> (R * 1.88f, R * 1.88f).withCentre (centre), 1.0f);

    for (int i = 0; i <= 30; ++i)
    {
        const float a = startAngle + (float) i / 30.0f * (endAngle - startAngle);
        const bool major = i % 5 == 0;
        g.setColour (major ? Palette::dim : Palette::faint);
        g.drawLine (juce::Line<float> (on (major ? R * 0.77f : R * 0.80f, a), on (R * 0.86f, a)), 1.0f);
    }

    const float ring = R * 0.70f;
    juce::Path track;
    track.addCentredArc (centre.x, centre.y, ring, ring, 0.0f, startAngle, endAngle, true);
    g.setColour (active ? Palette::hairHi.brighter (0.2f) : Palette::hairHi);
    g.strokePath (track, juce::PathStrokeType (4.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    if (pos > 0.002f)
    {
        juce::Path arc;
        arc.addCentredArc (centre.x, centre.y, ring, ring, 0.0f, startAngle, angle, true);
        strokeGlow (g, arc, Palette::accent, 3.5f);
    }

    // core with stars that brighten with the tail
    const float core = R * 0.58f;
    g.setGradientFill (juce::ColourGradient (Palette::space1.interpolatedWith (juce::Colour (0xff143050), 0.3f + 0.6f * level),
                                             centre.x, centre.y, Palette::space0, centre.x + core, centre.y, true));
    g.fillEllipse (juce::Rectangle<float> (core * 2.0f, core * 2.0f).withCentre (centre));
    juce::Random rng (0xdee95ace);
    for (int i = 0; i < 70; ++i)
    {
        const float r = std::sqrt (rng.nextFloat()) * core * 0.92f;
        const float a = rng.nextFloat() * juce::MathConstants<float>::twoPi + spin * (1.4f - r / core);
        const float b = 0.2f + 0.6f * rng.nextFloat();
        const bool cool = rng.nextFloat() < 0.4f;
        const float sz = rng.nextFloat() < 0.85f ? 1.0f : 1.6f;
        g.setColour ((cool ? Palette::accent : Palette::text).withAlpha (juce::jlimit (0.0f, 1.0f, b * (0.3f + level * 1.4f))));
        const auto p = on (r, a);
        g.fillRect (p.x, p.y, sz, sz);
    }
    g.setColour (active ? Palette::hairHi : Palette::hair);
    g.drawEllipse (juce::Rectangle<float> (core * 2.0f, core * 2.0f).withCentre (centre), 1.0f);

    glowDot (g, on (ring, angle), active ? 4.6f : 4.0f, Palette::accent);

    const auto text = s.getTextFromValue (s.getValue());
    g.setFont (fonts->mono (26.0f));
    g.setColour (Palette::accent.withAlpha (0.25f));
    g.drawText (text, juce::Rectangle<float> (core * 2.0f, 30.0f).withCentre (centre.translated (0.0f, -5.0f)).expanded (1.0f),
                juce::Justification::centred);
    g.setColour (Palette::accent);
    g.drawText (text, juce::Rectangle<float> (core * 2.0f, 30.0f).withCentre (centre.translated (0.0f, -5.0f)),
                juce::Justification::centred);
    g.setFont (fonts->mono (9.0f).withExtraKerningFactor (0.3f));
    g.setColour (Palette::faint);
    g.drawText ("RT60", juce::Rectangle<float> (core * 2.0f, 12.0f).withCentre (centre.translated (0.0f, 16.0f)),
                juce::Justification::centred);
}

//==============================================================================
IndikatPanel::IndikatPanel (DeepSpaceProcessor& p) : proc (p)
{
    setLookAndFeel (&lnf);
    setOpaque (true);

    if (auto svg = juce::XmlDocument::parse (juce::String::fromUTF8 (BinaryData::indikatlogo_svg, BinaryData::indikatlogo_svgSize)))
    {
        logo = juce::Drawable::createFromSVG (*svg);
        if (logo != nullptr)
            logo->replaceColour (juce::Colours::white, Palette::text);
    }

    mix = &addKnob ("mix", "MIX");
    predelay = &addKnob ("predelay", "PREDELAY");
    decay = &addKnob ("decay", "DECAY");
    decay->slider.setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
    decay->slider.getProperties().set ("big", true);
    decay->label.setFont (fonts->mono (12.0f).withExtraKerningFactor (0.34f));
    decay->label.setColour (juce::Label::textColourId, Palette::text);

    groups.push_back ({ "DAMPING", { &addKnob ("highFreq", "HIGH FREQ"), &addKnob ("highShelf", "HIGH SHELF"),
                                     &addKnob ("bassFreq", "BASS FREQ"), &addKnob ("bassMult", "BASS MULT") }, 2, {} });
    groups.push_back ({ "SHAPE", { &addKnob ("size", "SIZE"), &addKnob ("attack", "ATTACK") }, 1, {} });
    groups.push_back ({ "DIFF",  { &addKnob ("early", "EARLY"), &addKnob ("late", "LATE") }, 1, {} });
    groups.push_back ({ "MOD",   { &addKnob ("rate", "RATE"), &addKnob ("depth", "DEPTH") }, 1, {} });
    groups.push_back ({ "EQ",    { &addKnob ("highCut", "HIGH CUT"), &addKnob ("lowCut", "LOW CUT") }, 1, {} });

    addChoice (modeBox, modeAtt, "mode");
    addChoice (colourBox, colourAtt, "colour");

    for (int i = 0; i < proc.getNumPrograms(); ++i)
        presetBox.addItem (proc.getProgramName (i), i + 1);
    presetBox.setSelectedId (proc.getCurrentProgram() + 1, juce::dontSendNotification);
    presetBox.onChange = [this] { proc.setCurrentProgram (presetBox.getSelectedId() - 1); };
    addAndMakeVisible (presetBox);

    auto step = [this] (int delta)
    {
        const int n = proc.getNumPrograms();
        proc.setCurrentProgram ((proc.getCurrentProgram() + delta + n) % n);
        presetBox.setSelectedId (proc.getCurrentProgram() + 1, juce::dontSendNotification);
    };
    prevPreset.onClick = [step] { step (-1); };
    nextPreset.onClick = [step] { step (1); };
    addAndMakeVisible (prevPreset);
    addAndMakeVisible (nextPreset);

    setSize (baseWidth(), baseHeight());
    startTimerHz (30);
}

IndikatPanel::~IndikatPanel()
{
    setLookAndFeel (nullptr);
}

IndikatPanel::Knob& IndikatPanel::addKnob (const juce::String& paramId, const juce::String& name)
{
    auto k = std::make_unique<Knob>();
    k->slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    k->slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 76, 18);
    k->slider.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    k->slider.setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    k->slider.setColour (juce::Slider::textBoxTextColourId, Palette::text);
    k->slider.setRotaryParameters (juce::MathConstants<float>::pi * 1.25f, juce::MathConstants<float>::pi * 2.75f, true);
    k->label.setText (name, juce::dontSendNotification);
    k->label.setJustificationType (juce::Justification::centred);
    k->label.setFont (fonts->label (11.0f).withExtraKerningFactor (0.18f));
    k->label.setColour (juce::Label::textColourId, Palette::dim);
    addAndMakeVisible (k->slider);
    addAndMakeVisible (k->label);
    k->slider.sendLookAndFeelChange();
    k->attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, paramId, k->slider);

    if (auto* param = proc.apvts.getParameter (paramId))
        k->slider.setDoubleClickReturnValue (true, param->convertFrom0to1 (param->getDefaultValue()));

    knobs.push_back (std::move (k));
    return *knobs.back();
}

void IndikatPanel::addChoice (juce::ComboBox& box,
                              std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment>& att,
                              const juce::String& paramId)
{
    if (auto* choice = dynamic_cast<juce::AudioParameterChoice*> (proc.apvts.getParameter (paramId)))
        box.addItemList (choice->choices, 1);
    addAndMakeVisible (box);
    att = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (proc.apvts, paramId, box);
}

//==============================================================================
void IndikatPanel::renderBackground()
{
    const int w = getWidth(), h = getHeight();
    background = juce::Image (juce::Image::RGB, w, h, true);
    juce::Graphics g (background);

    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff0d1828), (float) w * 0.78f, (float) -h * 0.15f,
                                             Palette::space0, (float) w * 0.2f, (float) h * 1.1f, true));
    g.fillAll();

    juce::Random rng (0x1d1ca7);
    for (int i = 0; i < 220; ++i)
    {
        const float x = rng.nextFloat() * (float) w, y = rng.nextFloat() * (float) h;
        const float bright = std::pow (rng.nextFloat(), 3.0f);
        const float r = 0.35f + bright * 0.9f;
        const auto col = rng.nextFloat() < 0.15f ? Palette::accent : juce::Colour (0xffe8eef8);
        g.setColour (col.withAlpha (0.06f + bright * 0.45f));
        g.fillEllipse (x - r, y - r, r * 2.0f, r * 2.0f);
    }

    const juce::Point<float> origin ((float) w * 1.02f, (float) -h * 0.25f);
    for (int i = 0; i < 4; ++i)
    {
        const float r = (float) h * (0.55f + 0.32f * (float) i);
        g.setColour (Palette::accent.withAlpha (0.035f - 0.006f * (float) i));
        g.drawEllipse (juce::Rectangle<float> (r * 2.4f, r * 2.0f).withCentre (origin), 1.0f);
    }
}

void IndikatPanel::paint (juce::Graphics& g)
{
    if (background.isNull() || background.getWidth() != getWidth() || background.getHeight() != getHeight())
        renderBackground();
    g.drawImageAt (background, 0, 0);

    // header
    if (logo != nullptr)
        logo->drawWithin (g, { 28.0f, 24.0f, 22.0f * 4.93f, 22.0f }, juce::RectanglePlacement::xLeft, 1.0f);
    glowDot (g, { 166.0f, 35.0f }, 2.2f, Palette::accent);
    g.setColour (Palette::dim);
    g.setFont (fonts->mono (11.5f).withExtraKerningFactor (0.45f));
    g.drawText ("DEEP SPACE", 177, 27, 220, 16, juce::Justification::centredLeft);
    g.setColour (Palette::faint);
    g.setFont (fonts->mono (10.0f).withExtraKerningFactor (0.3f));
    g.drawText ("REVERB UNIT // 1.0", getWidth() - 28 - 200, 27, 200, 16, juce::Justification::centredRight);

    auto hairline = [&g, this] (int y)
    {
        juce::ColourGradient line (Palette::hairHi.withAlpha (0.0f), 28.0f, 0.0f,
                                   Palette::hairHi.withAlpha (0.0f), (float) getWidth() - 28.0f, 0.0f, false);
        line.addColour (0.12, Palette::hairHi);
        line.addColour (0.88, Palette::hairHi);
        g.setGradientFill (line);
        g.fillRect (28, y, getWidth() - 56, 1);
    };
    hairline (66);
    hairline (360);

    // groups
    for (auto& grp : groups)
    {
        auto r = grp.bounds.toFloat();
        g.setColour (juce::Colours::white.withAlpha (0.012f));
        g.fillRoundedRectangle (r, 6.0f);
        g.setColour (Palette::hair);
        g.drawRoundedRectangle (r.reduced (0.5f), 6.0f, 1.0f);
        g.setFont (fonts->wide (9.0f).withExtraKerningFactor (0.3f));
        g.setColour (Palette::accent.withAlpha (0.9f));
        g.drawText (grp.title, grp.bounds.getX(), grp.bounds.getY() + 10, grp.bounds.getWidth(), 16, juce::Justification::centred);
    }

    // footer labels
    g.setFont (fonts->mono (10.5f).withExtraKerningFactor (0.25f));
    g.setColour (Palette::dim);
    g.drawText ("MODE", modeLabel, juce::Justification::centredLeft);
    g.drawText ("COLOR", colourLabel, juce::Justification::centredLeft);
    g.drawText ("PRESETS", presetLabel, juce::Justification::centredLeft);
}

void IndikatPanel::placeKnob (Knob& k, juce::Rectangle<int> cell)
{
    k.label.setBounds (cell.removeFromTop (16));
    k.slider.setBounds (cell.withSizeKeepingCentre (juce::jmin (cell.getWidth(), 84), cell.getHeight()));
}

void IndikatPanel::resized()
{
    constexpr int top = 82, bottom = 346, gap = 10;
    int x = 28;

    // Mix + Predelay
    placeKnob (*mix,      { x, top + 8, 88, 112 });
    placeKnob (*predelay, { x, top + 140, 88, 112 });
    x += 88 + 14;

    // Decay dial
    decay->label.setBounds (x, top + 6, 214, 18);
    decay->slider.setBounds (juce::Rectangle<int> (x, top + 28, 214, bottom - top - 30).withSizeKeepingCentre (218, 218));
    x += 214 + 14;

    // groups share the rest by column count
    int cols = 0;
    for (auto& grp : groups) cols += grp.columns;
    const int right = getWidth() - 28;
    const int colW = (right - x - gap * ((int) groups.size() - 1)) / cols;
    for (size_t gi = 0; gi < groups.size(); ++gi)
    {
        auto& grp = groups[gi];
        const int w = gi + 1 == groups.size() ? right - x : colW * grp.columns;
        grp.bounds = { x, top, w, bottom - top };
        auto inner = grp.bounds.reduced (6, 0).withTrimmedTop (32).withTrimmedBottom (4);
        const int rows = ((int) grp.knobs.size() + grp.columns - 1) / grp.columns;
        const int rowH = inner.getHeight() / rows;
        for (size_t k = 0; k < grp.knobs.size(); ++k)
        {
            const int c = (int) k % grp.columns, r = (int) k / grp.columns;
            const int cw = inner.getWidth() / grp.columns;
            placeKnob (*grp.knobs[k], juce::Rectangle<int> (inner.getX() + c * cw, inner.getY() + r * rowH, cw, rowH)
                                          .withTrimmedBottom (r + 1 < rows ? 8 : 0).withTrimmedTop (r > 0 ? 4 : 0));
        }
        x += w + gap;
    }

    // footer
    constexpr int fy = 378, fh = 28;
    modeLabel   = { 28, fy, 46, fh };   modeBox.setBounds (76, fy, 140, fh);
    colourLabel = { 246, fy, 54, fh };  colourBox.setBounds (302, fy, 120, fh);
    presetLabel = { 452, fy, 72, fh };  presetBox.setBounds (526, fy, 220, fh);
    prevPreset.setBounds (752, fy, 28, fh);
    nextPreset.setBounds (782, fy, 28, fh);
}

void IndikatPanel::timerCallback()
{
    const float db = juce::Decibels::gainToDecibels (proc.wetLevel.load(), -60.0f);
    levelShown = 0.85f * levelShown + 0.15f * juce::jlimit (0.0f, 1.0f, juce::jmap (db, -54.0f, -12.0f, 0.0f, 1.0f));

    const float rate = proc.apvts.getRawParameterValue ("rate")->load();
    const float dec = proc.apvts.getRawParameterValue ("decay")->load();
    spin += (0.05 + rate * 0.03) / juce::jmax (0.4, std::log10 (dec * 3.0 + 1.0)) / 30.0;

    decay->slider.getProperties().set ("level", levelShown);
    decay->slider.getProperties().set ("spin", spin);
    decay->slider.repaint();

    if (presetBox.getSelectedId() != proc.getCurrentProgram() + 1)
        presetBox.setSelectedId (proc.getCurrentProgram() + 1, juce::dontSendNotification);
}

//==============================================================================
DeepSpaceEditor::DeepSpaceEditor (DeepSpaceProcessor& p)
    : AudioProcessorEditor (&p), panel (p)
{
    addAndMakeVisible (panel);

    const int w = IndikatPanel::baseWidth(), h = IndikatPanel::baseHeight();
    setResizable (true, true);
    setResizeLimits (w * 6 / 10, h * 6 / 10, w * 2, h * 2);
    getConstrainer()->setFixedAspectRatio ((double) w / (double) h);
    setSize (w, h);
}

void DeepSpaceEditor::resized()
{
    const float scale = (float) getWidth() / (float) IndikatPanel::baseWidth();
    panel.setBounds (0, 0, IndikatPanel::baseWidth(), IndikatPanel::baseHeight());
    panel.setTransform (juce::AffineTransform::scale (scale));
}
