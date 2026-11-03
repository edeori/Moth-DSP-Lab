#pragma once
#include <functional>
namespace lab
{
void requestAudioInputPermission(std::function<void(bool)> callback);
}
