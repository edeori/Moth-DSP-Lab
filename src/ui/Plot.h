#pragma once
#include "core/SessionStore.h"
namespace lab
{
class Plot final : public juce::Component
{
  public:
    void load(const juce::File &session);
    void clear()
    {
        a = AudioData{};
        b = AudioData{};
        residual = AudioData{};
        summary = juce::var{};
        repaint();
    }
    void setDifference(bool value)
    {
        difference = value;
        repaint();
    }
    void paint(juce::Graphics &) override;

  private:
    AudioData a, b, residual;
    bool difference = false, hardwareBaseline = false;
    juce::var summary;
};
} // namespace lab
