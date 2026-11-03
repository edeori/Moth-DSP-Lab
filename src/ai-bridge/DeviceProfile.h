#pragma once
#include "core/SessionStore.h"
namespace lab
{
juce::var panelRecognitionSchema();
juce::var parseControlInventory(const juce::String &text);
juce::File importPanelPhoto(const juce::File &project, const juce::File &source);
juce::var devicePlannerContext(const juce::var &context, const juce::var &device);
void validateDevicePlan(const juce::var &plan, const juce::var &device);
bool genericDevice(const juce::var &state);
} // namespace lab
