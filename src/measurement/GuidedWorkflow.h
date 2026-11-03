#pragma once
#include "core/SessionStore.h"
namespace lab
{
// Durable user progress, deliberately separate from live audio authorization.
class GuidedWorkflow
{
  public:
    explicit GuidedWorkflow(juce::File project);
    juce::var state;
    juce::File file;
    void create();
    void duplicateSetup();
    void load(const juce::File &);
    void save();
    juce::Array<juce::File> history() const;
    void importLegacySessions();
    bool hasResult(int mode) const;
    juce::File result(int mode) const;
    int nextMeasurement() const;
    bool hasScenario() const;
    juce::var nextScenarioStep() const;
    int scenarioCompleted() const;
    void recordScenarioResult(const juce::var &metadata, const juce::File &folder);
    void recordResult(int mode, const juce::File &);
    static juce::File defaultProject();
    static juce::File defaultProjectForExecutable(const juce::File &);
    juce::File profilesFolder() const;
    juce::Array<juce::File> profiles() const;
    juce::File saveProfile(const juce::var &plan, const juce::String &deviceProfile,
                           const juce::String &goal) const;
    static juce::var loadProfile(const juce::File &);
    static juce::String modeName(int mode);
    static juce::var resultReview(const juce::File &);
    static bool resultUsable(const juce::File &);
    static juce::String resultDescription(const juce::File &);

  private:
    juce::File root;
};
} // namespace lab
