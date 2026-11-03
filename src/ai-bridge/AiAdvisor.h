#pragma once
#include "core/Types.h"
namespace lab
{
class AiAdvisor
{
  public:
    virtual ~AiAdvisor() = default;
    // Called off the audio thread. Future providers require explicit preview/consent.
    virtual juce::var propose(const juce::var &measurementSummary) = 0;
};
class NoAiAdvisor final : public AiAdvisor
{
  public:
    juce::var propose(const juce::var &) override
    {
        auto v = object();
        set(v, "action", "no_change");
        set(v, "reason", "AI is disabled; no data was transmitted.");
        return v;
    }
};
} // namespace lab
