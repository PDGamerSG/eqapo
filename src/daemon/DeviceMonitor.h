#pragma once
#include <CoreAudio/CoreAudio.h>
#include <dispatch/dispatch.h>

#include <functional>
#include <string>
#include <vector>

#include "daemon/CoreAudioUtil.h"

namespace eqapo {

// Watches the default output device and the properties of the device in use.
// All callbacks are delivered on `queue`.
class DeviceMonitor {
public:
    explicit DeviceMonitor(dispatch_queue_t queue);
    // Called with a reason string whenever the pipeline should be rebuilt.
    void start(std::function<void(const std::string&)> onChange);
    // Follow property changes of `device` (sample rate, stream layout, alive).
    void watchDevice(AudioObjectID device);
    void stop();

private:
    dispatch_queue_t queue_;
    std::function<void(const std::string&)> onChange_;
    std::vector<ca::PropertyListener> systemListeners_;
    std::vector<ca::PropertyListener> deviceListeners_;
    AudioObjectID watched_ = 0;
    AudioObjectID lastDefault_ = 0;
};

}  // namespace eqapo
