#include "AudioPermission.h"
#include <juce_core/juce_core.h>
namespace lab
{
void requestAudioInputPermission(std::function<void(bool)> callback)
{
    juce::RuntimePermissions::request(juce::RuntimePermissions::recordAudio, std::move(callback));
}
} // namespace lab
