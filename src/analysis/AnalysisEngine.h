#pragma once
#include "core/Types.h"
namespace lab
{
struct AnalysisResult
{
    double referenceRmsDb = -160, candidateRmsDb = -160, gainDifferenceDb = 0;
    double referencePeak = 0, candidatePeak = 0, residualRmsDb = -160;
    int candidateDelaySamples = 0;
    double correlation = 0;
    bool alignmentReliable = false;
    AudioData residual;
    std::vector<float> frequencies, referenceSpectrumDb, candidateSpectrumDb;
    juce::var toJson() const;
};
class AnalysisEngine
{
  public:
    AnalysisResult compare(const AudioData &, const AudioData &, int maxLag = 4096) const;
};
} // namespace lab
