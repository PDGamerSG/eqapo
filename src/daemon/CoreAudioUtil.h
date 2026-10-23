// Thin C++ wrappers over the Core Audio HAL C API.
#pragma once
#include <CoreAudio/CoreAudio.h>

#include <functional>
#include <string>
#include <vector>

namespace eqapo::ca {

std::string statusString(OSStatus status);

AudioObjectID defaultOutputDevice();
std::vector<AudioObjectID> allDevices();
std::string deviceUID(AudioObjectID device);
std::string deviceName(AudioObjectID device);
std::string deviceManufacturer(AudioObjectID device);
double nominalSampleRate(AudioObjectID device);
bool setNominalSampleRate(AudioObjectID device, double rate);
UInt32 bufferFrameSize(AudioObjectID device);
bool setBufferFrameSize(AudioObjectID device, UInt32 frames);
// Total channels across all streams of the given scope (input or output).
unsigned channelCount(AudioObjectID device, AudioObjectPropertyScope scope);
// Channel layout of the streams: one entry per stream buffer with its channel count.
std::vector<unsigned> streamChannelLayout(AudioObjectID device, AudioObjectPropertyScope scope);
bool isAlive(AudioObjectID device);
bool isFloat32(AudioObjectID device, AudioObjectPropertyScope scope, std::string& description);
UInt32 transportType(AudioObjectID device);
std::string transportTypeName(UInt32 type);
AudioObjectID processObjectForPID(pid_t pid);
AudioObjectID deviceForUID(const std::string& uid);

// Property listener registered with a block on `queue`; removed on destruction.
class PropertyListener {
public:
    PropertyListener() = default;
    PropertyListener(AudioObjectID object, AudioObjectPropertyAddress address, dispatch_queue_t queue,
                     std::function<void()> callback);
    ~PropertyListener();
    PropertyListener(const PropertyListener&) = delete;
    PropertyListener& operator=(const PropertyListener&) = delete;
    PropertyListener(PropertyListener&& other) noexcept;
    PropertyListener& operator=(PropertyListener&& other) noexcept;
    void reset();

private:
    AudioObjectID object_ = 0;
    AudioObjectPropertyAddress address_{};
    dispatch_queue_t queue_ = nullptr;
    void* block_ = nullptr;  // AudioObjectPropertyListenerBlock (retained)
};

}  // namespace eqapo::ca
