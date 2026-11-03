#pragma once
#include "Types.h"
namespace lab
{
class SessionStore
{
  public:
    explicit SessionStore(const juce::File &root);
    juce::File beginSession(const juce::String &workflowId = {});
    static juce::String runPath(const juce::var &workflow, const juce::String &id);
    static juce::File runFolder(const juce::File &project, const juce::String &workflowId);
    static juce::Array<juce::File> sessions(const juce::File &project);
    static juce::File projectForSession(const juce::File &session);
    juce::File commit(const juce::File &pending);
    static juce::var load(const juce::File &session);

  private:
    juce::File root;
};
} // namespace lab
