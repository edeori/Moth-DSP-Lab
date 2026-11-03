#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_cryptography/juce_cryptography.h>
#include <juce_dsp/juce_dsp.h>
#include <stdexcept>

namespace lab
{
inline constexpr const char *version = "0.1.0";
inline void require(bool ok, const juce::String &message)
{
    if (!ok)
        throw std::runtime_error(message.toStdString());
}
inline juce::var object()
{
    return juce::var(new juce::DynamicObject());
}
inline void set(juce::var &v, const juce::Identifier &key, const juce::var &value)
{
    v.getDynamicObject()->setProperty(key, value);
}
inline juce::String encodeState(const juce::MemoryBlock &b)
{
    return juce::Base64::toBase64(b.getData(), b.getSize());
}
inline bool decodeState(juce::MemoryBlock &b, const juce::String &text)
{
    b.reset();
    juce::MemoryOutputStream stream(b, false);
    return juce::Base64::convertFromBase64(stream, text);
}
struct RenderSettings
{
    double sampleRate = 48000;
    int blockSize = 512;
    int channels = 2;
    double tailSeconds = 2;
    void validate() const;
    juce::var toJson() const;
    static RenderSettings fromJson(const juce::var &);
};
struct AudioData
{
    juce::AudioBuffer<float> samples;
    double sampleRate = 48000;
};
struct MeasurementArtifact
{
    AudioData audio;
    AudioData rawAudio;
    int reportedLatency = 0;
    double reportedTail = 0;
    juce::var metadata;
};
struct TargetSpec
{
    juce::File plugin;
    juce::String descriptionXml;
    juce::MemoryBlock state;
    juce::var toJson() const;
    static TargetSpec fromJson(const juce::var &);
};
class AudioTarget
{
  public:
    virtual ~AudioTarget() = default;
    virtual MeasurementArtifact renderOffline(const AudioData &, const RenderSettings &) = 0;
    // Hardware targets implement a future asynchronous measureRealtime contract.
};
juce::String utcNow();
AudioData readWav(const juce::File &);
void writeWav(const juce::File &, const AudioData &);
juce::var readJson(const juce::File &);
void writeJson(const juce::File &, const juce::var &);
} // namespace lab
