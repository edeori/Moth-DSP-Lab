#pragma once
#include <juce_gui_extra/juce_gui_extra.h>
namespace lab
{
struct LabTheme final : juce::LookAndFeel_V4
{
    static juce::Colour background()
    {
        return juce::Colour(0xff10151e);
    }
    static juce::Colour panel()
    {
        return juce::Colour(0xff19212e);
    }
    static juce::Colour accent()
    {
        return juce::Colour(0xff68dfbf);
    }
    static juce::Colour muted()
    {
        return juce::Colour(0xffa8b7ca);
    }
    LabTheme()
    {
        setColour(juce::Label::textColourId, juce::Colour(0xffeff4fb));
        setColour(juce::TextButton::buttonColourId, juce::Colour(0xff263347));
        setColour(juce::TextButton::textColourOffId, juce::Colour(0xffeff4fb));
        setColour(juce::ComboBox::backgroundColourId, juce::Colour(0xff202b3b));
        setColour(juce::ComboBox::textColourId, juce::Colour(0xffeff4fb));
        setColour(juce::ComboBox::outlineColourId, juce::Colour(0xff41526a));
        setColour(juce::PopupMenu::backgroundColourId, panel());
        setColour(juce::PopupMenu::textColourId, juce::Colour(0xffeff4fb));
        setColour(juce::PopupMenu::highlightedBackgroundColourId, juce::Colour(0xff304a51));
        setColour(juce::TextEditor::backgroundColourId, juce::Colour(0xff131c29));
        setColour(juce::TextEditor::textColourId, juce::Colour(0xffeff4fb));
        setColour(juce::TextEditor::outlineColourId, juce::Colour(0xff3a4a60));
        setColour(juce::TextEditor::focusedOutlineColourId, accent());
        setColour(juce::TextEditor::highlightColourId, juce::Colour(0xff336555));
        setColour(juce::ToggleButton::textColourId, juce::Colour(0xffeff4fb));
        setColour(juce::ToggleButton::tickColourId, accent());
        setColour(juce::ToggleButton::tickDisabledColourId, muted());
        setColour(juce::ProgressBar::backgroundColourId, juce::Colour(0xff263347));
        setColour(juce::ProgressBar::foregroundColourId, accent());
    }
    juce::Font getTextButtonFont(juce::TextButton &, int) override
    {
        return juce::FontOptions(14.f);
    }
    void drawButtonBackground(juce::Graphics &g, juce::Button &button, const juce::Colour &colour, bool over,
                              bool down) override
    {
        auto r = button.getLocalBounds().toFloat().reduced(1);
        auto fill = colour;
        if (down)
            fill = fill.darker(.15f);
        else if (over)
            fill = fill.brighter(.12f);
        if (!button.isEnabled())
            fill = juce::Colour(0xff202c3c);
        g.setColour(fill);
        g.fillRoundedRectangle(r, 7);
        g.setColour(button.hasKeyboardFocus(true)
                        ? accent()
                        : juce::Colour(0xff485870).withAlpha(button.isEnabled() ? .65f : .2f));
        g.drawRoundedRectangle(r, 7, button.hasKeyboardFocus(true) ? 2.f : 1.f);
    }
    void drawButtonText(juce::Graphics &g, juce::TextButton &button, bool, bool) override
    {
        g.setFont(getTextButtonFont(button, button.getHeight()));
        g.setColour(button.isEnabled() ? button.findColour(juce::TextButton::textColourOffId) : muted());
        g.drawFittedText(button.getButtonText(), button.getLocalBounds().reduced(8, 2),
                         juce::Justification::centred, 2);
    }
    static void primary(juce::TextButton &button)
    {
        button.setColour(juce::TextButton::buttonColourId, accent());
        button.setColour(juce::TextButton::textColourOffId, juce::Colour(0xff10261e));
    }
};
} // namespace lab
