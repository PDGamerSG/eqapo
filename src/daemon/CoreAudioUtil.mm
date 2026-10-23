#include "daemon/CoreAudioUtil.h"

#import <Foundation/Foundation.h>

#include <cstring>

namespace eqapo::ca {

std::string statusString(OSStatus status) {
    char code[5] = {0};
    UInt32 be = CFSwapInt32HostToBig(static_cast<UInt32>(status));
    std::memcpy(code, &be, 4);
    bool printable = true;
    for (int i = 0; i < 4; i++)
        if (code[i] < 32 || code[i] > 126) printable = false;
    if (printable) return std::string("'") + code + "'";
    return std::to_string(status);
}

namespace {
template <typename T>
bool getProperty(AudioObjectID object, AudioObjectPropertySelector selector, AudioObjectPropertyScope scope, T& out) {
    AudioObjectPropertyAddress addr{selector, scope, kAudioObjectPropertyElementMain};
    UInt32 size = sizeof(T);
    return AudioObjectGetPropertyData(object, &addr, 0, nullptr, &size, &out) == noErr;
}

std::string getStringProperty(AudioObjectID object, AudioObjectPropertySelector selector) {
    CFStringRef str = nullptr;
    if (!getProperty(object, selector, kAudioObjectPropertyScopeGlobal, str) || !str) return "";
    std::string result = [(__bridge NSString*)str UTF8String] ?: "";
    CFRelease(str);
    return result;
}
}  // namespace

AudioObjectID defaultOutputDevice() {
    AudioObjectID id = kAudioObjectUnknown;
    getProperty(kAudioObjectSystemObject, kAudioHardwarePropertyDefaultOutputDevice, kAudioObjectPropertyScopeGlobal, id);
    return id;
}

std::vector<AudioObjectID> allDevices() {
    AudioObjectPropertyAddress addr{kAudioHardwarePropertyDevices, kAudioObjectPropertyScopeGlobal,
                                    kAudioObjectPropertyElementMain};
    UInt32 size = 0;
    if (AudioObjectGetPropertyDataSize(kAudioObjectSystemObject, &addr, 0, nullptr, &size) != noErr) return {};
    std::vector<AudioObjectID> ids(size / sizeof(AudioObjectID));
    if (AudioObjectGetPropertyData(kAudioObjectSystemObject, &addr, 0, nullptr, &size, ids.data()) != noErr) return {};
    ids.resize(size / sizeof(AudioObjectID));
    return ids;
}

std::string deviceUID(AudioObjectID device) { return getStringProperty(device, kAudioDevicePropertyDeviceUID); }
std::string deviceName(AudioObjectID device) { return getStringProperty(device, kAudioObjectPropertyName); }
std::string deviceManufacturer(AudioObjectID device) { return getStringProperty(device, kAudioObjectPropertyManufacturer); }

double nominalSampleRate(AudioObjectID device) {
    Float64 rate = 0;
    getProperty(device, kAudioDevicePropertyNominalSampleRate, kAudioObjectPropertyScopeGlobal, rate);
    return rate;
}

bool setNominalSampleRate(AudioObjectID device, double rate) {
    AudioObjectPropertyAddress addr{kAudioDevicePropertyNominalSampleRate, kAudioObjectPropertyScopeGlobal,
                                    kAudioObjectPropertyElementMain};
    Float64 r = rate;
    return AudioObjectSetPropertyData(device, &addr, 0, nullptr, sizeof(r), &r) == noErr;
}

UInt32 bufferFrameSize(AudioObjectID device) {
    UInt32 frames = 0;
    getProperty(device, kAudioDevicePropertyBufferFrameSize, kAudioObjectPropertyScopeGlobal, frames);
    return frames;
}

bool setBufferFrameSize(AudioObjectID device, UInt32 frames) {
    AudioObjectPropertyAddress addr{kAudioDevicePropertyBufferFrameSize, kAudioObjectPropertyScopeGlobal,
                                    kAudioObjectPropertyElementMain};
    return AudioObjectSetPropertyData(device, &addr, 0, nullptr, sizeof(frames), &frames) == noErr;
}

std::vector<unsigned> streamChannelLayout(AudioObjectID device, AudioObjectPropertyScope scope) {
    AudioObjectPropertyAddress addr{kAudioDevicePropertyStreamConfiguration, scope, kAudioObjectPropertyElementMain};
    UInt32 size = 0;
    if (AudioObjectGetPropertyDataSize(device, &addr, 0, nullptr, &size) != noErr || size == 0) return {};
    std::vector<char> storage(size);
    auto* abl = reinterpret_cast<AudioBufferList*>(storage.data());
    if (AudioObjectGetPropertyData(device, &addr, 0, nullptr, &size, abl) != noErr) return {};
    std::vector<unsigned> layout;
    for (UInt32 i = 0; i < abl->mNumberBuffers; i++) layout.push_back(abl->mBuffers[i].mNumberChannels);
    return layout;
}

unsigned channelCount(AudioObjectID device, AudioObjectPropertyScope scope) {
    unsigned total = 0;
    for (unsigned n : streamChannelLayout(device, scope)) total += n;
    return total;
}

bool isAlive(AudioObjectID device) {
    UInt32 alive = 0;
    if (!getProperty(device, kAudioDevicePropertyDeviceIsAlive, kAudioObjectPropertyScopeGlobal, alive)) return false;
    return alive != 0;
}

bool isFloat32(AudioObjectID device, AudioObjectPropertyScope scope, std::string& description) {
    AudioObjectPropertyAddress addr{kAudioDevicePropertyStreams, scope, kAudioObjectPropertyElementMain};
    UInt32 size = 0;
    if (AudioObjectGetPropertyDataSize(device, &addr, 0, nullptr, &size) != noErr) return false;
    std::vector<AudioObjectID> streams(size / sizeof(AudioObjectID));
    if (AudioObjectGetPropertyData(device, &addr, 0, nullptr, &size, streams.data()) != noErr) return false;
    description.clear();
    bool ok = !streams.empty();
    for (AudioObjectID stream : streams) {
        AudioStreamBasicDescription fmt{};
        if (!getProperty(stream, kAudioStreamPropertyVirtualFormat, kAudioObjectPropertyScopeGlobal, fmt)) {
            ok = false;
            continue;
        }
        bool isFloat = fmt.mFormatID == kAudioFormatLinearPCM && (fmt.mFormatFlags & kAudioFormatFlagIsFloat) &&
                       fmt.mBitsPerChannel == 32;
        char buf[128];
        std::snprintf(buf, sizeof(buf), "%s%.0f Hz %u ch %s%u-bit%s", description.empty() ? "" : "; ", fmt.mSampleRate,
                      fmt.mChannelsPerFrame, isFloat ? "float" : "int", fmt.mBitsPerChannel,
                      (fmt.mFormatFlags & kAudioFormatFlagIsNonInterleaved) ? " planar" : "");
        description += buf;
        if (!isFloat) ok = false;
    }
    return ok;
}

UInt32 transportType(AudioObjectID device) {
    UInt32 t = 0;
    getProperty(device, kAudioDevicePropertyTransportType, kAudioObjectPropertyScopeGlobal, t);
    return t;
}

std::string transportTypeName(UInt32 type) {
    switch (type) {
        case kAudioDeviceTransportTypeBuiltIn: return "built-in";
        case kAudioDeviceTransportTypeAggregate: return "aggregate";
        case kAudioDeviceTransportTypeVirtual: return "virtual";
        case kAudioDeviceTransportTypePCI: return "PCI";
        case kAudioDeviceTransportTypeUSB: return "USB";
        case kAudioDeviceTransportTypeFireWire: return "FireWire";
        case kAudioDeviceTransportTypeBluetooth: return "Bluetooth";
        case kAudioDeviceTransportTypeBluetoothLE: return "Bluetooth LE";
        case kAudioDeviceTransportTypeHDMI: return "HDMI";
        case kAudioDeviceTransportTypeDisplayPort: return "DisplayPort";
        case kAudioDeviceTransportTypeAirPlay: return "AirPlay";
        case kAudioDeviceTransportTypeAVB: return "AVB";
        case kAudioDeviceTransportTypeThunderbolt: return "Thunderbolt";
        case kAudioDeviceTransportTypeContinuityCaptureWired: return "Continuity (wired)";
        case kAudioDeviceTransportTypeContinuityCaptureWireless: return "Continuity (wireless)";
        default: return "unknown";
    }
}

AudioObjectID processObjectForPID(pid_t pid) {
    AudioObjectPropertyAddress addr{kAudioHardwarePropertyTranslatePIDToProcessObject, kAudioObjectPropertyScopeGlobal,
                                    kAudioObjectPropertyElementMain};
    AudioObjectID object = kAudioObjectUnknown;
    UInt32 size = sizeof(object);
    pid_t p = pid;
    if (AudioObjectGetPropertyData(kAudioObjectSystemObject, &addr, sizeof(p), &p, &size, &object) != noErr) return 0;
    return object;
}

AudioObjectID deviceForUID(const std::string& uid) {
    AudioObjectPropertyAddress addr{kAudioHardwarePropertyTranslateUIDToDevice, kAudioObjectPropertyScopeGlobal,
                                    kAudioObjectPropertyElementMain};
    CFStringRef str = CFStringCreateWithCString(nullptr, uid.c_str(), kCFStringEncodingUTF8);
    AudioObjectID device = kAudioObjectUnknown;
    UInt32 size = sizeof(device);
    AudioObjectGetPropertyData(kAudioObjectSystemObject, &addr, sizeof(str), &str, &size, &device);
    CFRelease(str);
    return device;
}

// ------------------------------------------------------------ PropertyListener

PropertyListener::PropertyListener(AudioObjectID object, AudioObjectPropertyAddress address, dispatch_queue_t queue,
                                   std::function<void()> callback)
    : object_(object), address_(address), queue_(queue) {
    auto cb = std::make_shared<std::function<void()>>(std::move(callback));
    AudioObjectPropertyListenerBlock block = ^(UInt32, const AudioObjectPropertyAddress*) {
        (*cb)();
    };
    block_ = (__bridge_retained void*)[block copy];
    OSStatus st = AudioObjectAddPropertyListenerBlock(object_, &address_, queue_, (__bridge AudioObjectPropertyListenerBlock)block_);
    if (st != noErr) {
        if (block_) CFRelease(block_);
        block_ = nullptr;
        queue_ = nullptr;
        object_ = 0;
    }
}

PropertyListener::~PropertyListener() { reset(); }

PropertyListener::PropertyListener(PropertyListener&& other) noexcept
    : object_(other.object_), address_(other.address_), queue_(other.queue_), block_(other.block_) {
    other.object_ = 0;
    other.queue_ = nullptr;
    other.block_ = nullptr;
}

PropertyListener& PropertyListener::operator=(PropertyListener&& other) noexcept {
    if (this != &other) {
        reset();
        object_ = other.object_;
        address_ = other.address_;
        queue_ = other.queue_;
        block_ = other.block_;
        other.object_ = 0;
        other.queue_ = nullptr;
        other.block_ = nullptr;
    }
    return *this;
}

void PropertyListener::reset() {
    if (block_) {
        AudioObjectRemovePropertyListenerBlock(object_, &address_, queue_, (__bridge AudioObjectPropertyListenerBlock)block_);
        CFRelease(block_);
        block_ = nullptr;
    }
    queue_ = nullptr;
    object_ = 0;
}

}  // namespace eqapo::ca
