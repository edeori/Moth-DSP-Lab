#include "AudioPermission.h"
#import <AVFoundation/AVFoundation.h>
namespace lab
{
void requestAudioInputPermission(std::function<void(bool)> callback)
{
    if (@available(macOS 10.14, *))
    {
        auto status = [AVCaptureDevice authorizationStatusForMediaType:AVMediaTypeAudio];
        if (status == AVAuthorizationStatusNotDetermined)
        {
            [AVCaptureDevice requestAccessForMediaType:AVMediaTypeAudio
                                     completionHandler:^(BOOL granted) {
                                       callback(granted);
                                     }];
        }
        else
            callback(status == AVAuthorizationStatusAuthorized);
    }
    else
        callback(true);
}
} // namespace lab
