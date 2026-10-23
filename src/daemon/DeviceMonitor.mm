#include "daemon/DeviceMonitor.h"

namespace eqapo {

DeviceMonitor::DeviceMonitor(dispatch_queue_t queue) : queue_(queue) {}

void DeviceMonitor::start(std::function<void(const std::string&)> onChange) {
    onChange_ = std::move(onChange);
    lastDefault_ = ca::defaultOutputDevice();
    AudioObjectPropertyAddress defaultAddr{kAudioHardwarePropertyDefaultOutputDevice, kAudioObjectPropertyScopeGlobal,
                                           kAudioObjectPropertyElementMain};
    systemListeners_.emplace_back(kAudioObjectSystemObject, defaultAddr, queue_, [this] {
        AudioObjectID now = ca::defaultOutputDevice();
        if (now != lastDefault_) {
            lastDefault_ = now;
            onChange_("default output device changed");
        }
    });
    AudioObjectPropertyAddress devicesAddr{kAudioHardwarePropertyDevices, kAudioObjectPropertyScopeGlobal,
                                           kAudioObjectPropertyElementMain};
    systemListeners_.emplace_back(kAudioObjectSystemObject, devicesAddr, queue_, [this] {
        // Our own private aggregate also triggers this; only react to real changes.
        AudioObjectID now = ca::defaultOutputDevice();
        if (now != lastDefault_) {
            lastDefault_ = now;
            onChange_("device list changed, new default output device");
        } else if (watched_ && !ca::isAlive(watched_)) {
            onChange_("output device is gone");
        }
    });
}

void DeviceMonitor::watchDevice(AudioObjectID device) {
    deviceListeners_.clear();
    watched_ = device;
    if (!device) return;
    AudioObjectPropertyAddress rateAddr{kAudioDevicePropertyNominalSampleRate, kAudioObjectPropertyScopeGlobal,
                                        kAudioObjectPropertyElementMain};
    deviceListeners_.emplace_back(device, rateAddr, queue_, [this] { onChange_("sample rate changed"); });
    AudioObjectPropertyAddress streamAddr{kAudioDevicePropertyStreamConfiguration, kAudioObjectPropertyScopeOutput,
                                          kAudioObjectPropertyElementMain};
    deviceListeners_.emplace_back(device, streamAddr, queue_, [this] { onChange_("stream configuration changed"); });
    AudioObjectPropertyAddress aliveAddr{kAudioDevicePropertyDeviceIsAlive, kAudioObjectPropertyScopeGlobal,
                                         kAudioObjectPropertyElementMain};
    deviceListeners_.emplace_back(device, aliveAddr, queue_, [this] {
        if (!ca::isAlive(watched_)) onChange_("device is no longer alive");
    });
}

void DeviceMonitor::stop() {
    deviceListeners_.clear();
    systemListeners_.clear();
    watched_ = 0;
}

}  // namespace eqapo
