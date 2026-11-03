#pragma once
#include "AiAdvisor.h"
#include "core/SessionStore.h"
#include <atomic>
namespace lab
{
inline constexpr int plannerTimeoutMs = 360000;
// The planner can request only capabilities implemented by the deterministic executor.
juce::var scenarioSchema();
// Numeric evidence only; the same bounded payload is previewed and transmitted.
juce::var compactPlannerContext(const juce::var &);
void validateScenario(const juce::var &);
juce::String describeScenario(const juce::var &);
juce::String scenarioStepInstructions(const juce::var &step, const juce::var &previous = {});
juce::var plannerContext(const juce::String &profile, const juce::String &goal, const juce::var &evidence);
juce::var runCodexPlanner(const juce::var &context, const std::atomic_bool &cancelled,
                          const juce::File &executable = {}, int timeoutMs = plannerTimeoutMs);
class CodexAdvisor final : public AiAdvisor
{
  public:
    explicit CodexAdvisor(const std::atomic_bool &stop) : cancelled(stop) {}
    juce::var propose(const juce::var &summary) override
    {
        return runCodexPlanner(summary, cancelled);
    }

  private:
    const std::atomic_bool &cancelled;
};
} // namespace lab
