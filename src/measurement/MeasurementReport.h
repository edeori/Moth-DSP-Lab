#pragma once
#include "core/SessionStore.h"
namespace lab
{
void writeHardwareReport(const juce::File &folder);
juce::Array<juce::File> collectionResults(const juce::File &project, const juce::var &workflow);
juce::File exportMeasurementHandoff(const juce::File &project, const juce::var &workflow);
} // namespace lab
