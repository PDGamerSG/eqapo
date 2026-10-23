#include "audio/AudioFile.h"

#import <AudioToolbox/AudioToolbox.h>
#import <CoreFoundation/CoreFoundation.h>

#include <cstring>

namespace eqapo {

namespace {
std::string osStatusString(OSStatus status) {
    char code[5] = {0};
    UInt32 be = CFSwapInt32HostToBig(static_cast<UInt32>(status));
    std::memcpy(code, &be, 4);
    bool printable = true;
    for (char c : code)
        if (c < 32 || c > 126) printable = false;
    if (printable && code[0]) return std::string("'") + code + "'";
    return std::to_string(status);
}

CFURLRef urlFor(const std::string& path) {
    CFStringRef str = CFStringCreateWithCString(nullptr, path.c_str(), kCFStringEncodingUTF8);
    CFURLRef url = CFURLCreateWithFileSystemPath(nullptr, str, kCFURLPOSIXPathStyle, false);
    CFRelease(str);
    return url;
}
}  // namespace

AudioData readAudioFile(const std::string& path, std::string& error) {
    AudioData data;
    CFURLRef url = urlFor(path);
    ExtAudioFileRef file = nullptr;
    OSStatus status = ExtAudioFileOpenURL(url, &file);
    CFRelease(url);
    if (status != noErr) {
        error = "ExtAudioFileOpenURL failed: " + osStatusString(status);
        return data;
    }

    AudioStreamBasicDescription fileFormat{};
    UInt32 size = sizeof(fileFormat);
    status = ExtAudioFileGetProperty(file, kExtAudioFileProperty_FileDataFormat, &size, &fileFormat);
    if (status != noErr) {
        error = "cannot read file format: " + osStatusString(status);
        ExtAudioFileDispose(file);
        return data;
    }
    SInt64 frameCount = 0;
    size = sizeof(frameCount);
    ExtAudioFileGetProperty(file, kExtAudioFileProperty_FileLengthFrames, &size, &frameCount);

    const UInt32 channels = fileFormat.mChannelsPerFrame;
    AudioStreamBasicDescription clientFormat{};
    clientFormat.mSampleRate = fileFormat.mSampleRate;
    clientFormat.mFormatID = kAudioFormatLinearPCM;
    clientFormat.mFormatFlags = kAudioFormatFlagIsFloat | kAudioFormatFlagIsPacked | kAudioFormatFlagIsNonInterleaved;
    clientFormat.mBitsPerChannel = 32;
    clientFormat.mChannelsPerFrame = channels;
    clientFormat.mFramesPerPacket = 1;
    clientFormat.mBytesPerFrame = 4;
    clientFormat.mBytesPerPacket = 4;
    status = ExtAudioFileSetProperty(file, kExtAudioFileProperty_ClientDataFormat, sizeof(clientFormat), &clientFormat);
    if (status != noErr) {
        error = "cannot set client format: " + osStatusString(status);
        ExtAudioFileDispose(file);
        return data;
    }

    data.sampleRate = fileFormat.mSampleRate;
    data.channels.assign(channels, std::vector<float>());
    const UInt32 chunk = 16384;
    std::vector<std::vector<float>> buffers(channels, std::vector<float>(chunk));
    std::vector<char> ablStorage(sizeof(AudioBufferList) + sizeof(AudioBuffer) * channels);
    auto* abl = reinterpret_cast<AudioBufferList*>(ablStorage.data());
    while (true) {
        abl->mNumberBuffers = channels;
        for (UInt32 c = 0; c < channels; c++) {
            abl->mBuffers[c].mNumberChannels = 1;
            abl->mBuffers[c].mDataByteSize = chunk * sizeof(float);
            abl->mBuffers[c].mData = buffers[c].data();
        }
        UInt32 frames = chunk;
        status = ExtAudioFileRead(file, &frames, abl);
        if (status != noErr) {
            error = "ExtAudioFileRead failed: " + osStatusString(status);
            break;
        }
        if (frames == 0) break;
        for (UInt32 c = 0; c < channels; c++)
            data.channels[c].insert(data.channels[c].end(), buffers[c].begin(), buffers[c].begin() + frames);
    }
    ExtAudioFileDispose(file);
    if (data.frames() == 0 && error.empty()) error = "file contains no audio";
    if (!error.empty()) data.channels.clear();
    return data;
}

bool writeWavFloat(const std::string& path, const AudioData& data, std::string& error) {
    if (data.channels.empty()) {
        error = "no channels";
        return false;
    }
    const UInt32 channels = static_cast<UInt32>(data.channels.size());
    AudioStreamBasicDescription format{};
    format.mSampleRate = data.sampleRate;
    format.mFormatID = kAudioFormatLinearPCM;
    format.mFormatFlags = kAudioFormatFlagIsFloat | kAudioFormatFlagIsPacked;
    format.mBitsPerChannel = 32;
    format.mChannelsPerFrame = channels;
    format.mFramesPerPacket = 1;
    format.mBytesPerFrame = 4 * channels;
    format.mBytesPerPacket = 4 * channels;

    unlink(path.c_str());
    CFURLRef url = urlFor(path);
    ExtAudioFileRef file = nullptr;
    OSStatus status = ExtAudioFileCreateWithURL(url, kAudioFileWAVEType, &format, nullptr, kAudioFileFlags_EraseFile, &file);
    CFRelease(url);
    if (status != noErr) {
        error = "ExtAudioFileCreateWithURL failed: " + osStatusString(status);
        return false;
    }
    size_t frames = data.frames();
    std::vector<float> interleaved(frames * channels);
    for (UInt32 c = 0; c < channels; c++)
        for (size_t i = 0; i < frames; i++) interleaved[i * channels + c] = data.channels[c][i];
    AudioBufferList abl;
    abl.mNumberBuffers = 1;
    abl.mBuffers[0].mNumberChannels = channels;
    abl.mBuffers[0].mDataByteSize = static_cast<UInt32>(interleaved.size() * sizeof(float));
    abl.mBuffers[0].mData = interleaved.data();
    status = ExtAudioFileWrite(file, static_cast<UInt32>(frames), &abl);
    ExtAudioFileDispose(file);
    if (status != noErr) {
        error = "ExtAudioFileWrite failed: " + osStatusString(status);
        return false;
    }
    return true;
}

}  // namespace eqapo
