#pragma once

#include "PluginProcessor.h"

// Embedded fonts (OFL)
struct IndikatFonts
{
    IndikatFonts();
    juce::Font wide (float h) const  { return juce::Font (juce::FontOptions (michroma).withHeight (h)); }
    juce::Font label (float h) const { return juce::Font (juce::FontOptions (questrial).withHeight (h)); }
    juce::Font mono (float h) const  { return juce::Font (juce::FontOptions (shareTech).withHeight (h)); }

    juce::Typeface::Ptr michroma, questrial, shareTech;
};

class IndikatLookAndFeel : public juce::LookAndFeel_V4
{
public:
    IndikatLookAndFeel();

    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos,
                           float startAngle, float endAngle, juce::Slider&) override;
    juce::Label* createSliderTextBox (juce::Slider&) override;

    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool highlighted, bool down) override;
    void drawButtonText (juce::Graphics&, juce::TextButton&, bool highlighted, bool down) override;

    void drawComboBox (juce::Graphics&, int w, int h, bool down, int, int, int, int, juce::ComboBox&) override;
    juce::Font getComboBoxFont (juce::ComboBox&) override;
    void positionComboBoxText (juce::ComboBox&, juce::Label&) override;

    void drawPopupMenuBackground (juce::Graphics&, int w, int h) override;
    void drawPopupMenuItem (juce::Graphics&, const juce::Rectangle<int>&, bool isSeparator, bool isActive,
                            bool isHighlighted, bool isTicked, bool hasSubMenu, const juce::String& text,
                            const juce::String& shortcutKeyText, const juce::Drawable* icon,
                            const juce::Colour* textColour) override;
    juce::Font getPopupMenuFont() override;
    juce::Font getLabelFont (juce::Label&) override;

private:
    void drawDecayDial (juce::Graphics&, juce::Rectangle<float>, float pos, float startAngle, float endAngle, juce::Slider&);
    juce::SharedResourcePointer<IndikatFonts> fonts;
};

// The UI at a fixed base size; the editor scales it
class IndikatPanel : public juce::Component, private juce::Timer
{
public:
    explicit IndikatPanel (DeepSpaceProcessor&);
    ~IndikatPanel() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    static int baseWidth()  { return 1040; }
    static int baseHeight() { return 430; }

private:
    struct Knob
    {
        juce::Slider slider;
        juce::Label label;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
    };

    struct Group
    {
        juce::String title;
        std::vector<Knob*> knobs;
        int columns = 1;
        juce::Rectangle<int> bounds;
    };

    Knob& addKnob (const juce::String& paramId, const juce::String& name);
    void addChoice (juce::ComboBox&, std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment>&,
                    const juce::String& paramId);
    void placeKnob (Knob&, juce::Rectangle<int> cell);
    void renderBackground();
    void timerCallback() override;

    DeepSpaceProcessor& proc;
    IndikatLookAndFeel lnf;
    juce::SharedResourcePointer<IndikatFonts> fonts;

    std::vector<std::unique_ptr<Knob>> knobs;
    std::vector<Group> groups;
    Knob* mix = nullptr;
    Knob* predelay = nullptr;
    Knob* decay = nullptr;

    juce::ComboBox modeBox, colourBox, presetBox;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> modeAtt, colourAtt;
    juce::TextButton prevPreset { "<" }, nextPreset { ">" };
    juce::Rectangle<int> modeLabel, colourLabel, presetLabel;

    juce::Image background;
    std::unique_ptr<juce::Drawable> logo;
    float levelShown = 0.0f;
    double spin = 0.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (IndikatPanel)
};

class DeepSpaceEditor : public juce::AudioProcessorEditor
{
public:
    explicit DeepSpaceEditor (DeepSpaceProcessor&);
    void resized() override;

private:
    IndikatPanel panel;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DeepSpaceEditor)
};
