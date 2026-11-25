#include "audio/AudioFile.h"

#include <cstdint>
#include <cstring>
#include <fstream>

namespace eqapo {

namespace {
uint32_t readU32(const unsigned char* p) {
    return static_cast<uint32_t>(p[0]) | (static_cast<uint32_t>(p[1]) << 8) |
           (static_cast<uint32_t>(p[2]) << 16) | (static_cast<uint32_t>(p[3]) << 24);
}

uint16_t readU16(const unsigned char* p) { return static_cast<uint16_t>(p[0] | (p[1] << 8)); }

void writeU32(std::ofstream& file, uint32_t value) {
    unsigned char bytes[4] = {static_cast<unsigned char>(value), static_cast<unsigned char>(value >> 8),
                              static_cast<unsigned char>(value >> 16), static_cast<unsigned char>(value >> 24)};
    file.write(reinterpret_cast<const char*>(bytes), sizeof(bytes));
}

void writeU16(std::ofstream& file, uint16_t value) {
    unsigned char bytes[2] = {static_cast<unsigned char>(value), static_cast<unsigned char>(value >> 8)};
    file.write(reinterpret_cast<const char*>(bytes), sizeof(bytes));
}
}  // namespace

AudioData readAudioFile(const std::string& path, std::string& error) {
    AudioData data;
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        error = "cannot open audio file: " + path;
        return data;
    }

    unsigned char header[12]{};
    file.read(reinterpret_cast<char*>(header), sizeof(header));
    if (file.gcount() != sizeof(header) || std::memcmp(header, "RIFF", 4) != 0 ||
        std::memcmp(header + 8, "WAVE", 4) != 0) {
        error = "unsupported audio file (Linux build accepts PCM WAV only): " + path;
        return data;
    }

    uint16_t format = 0, channels = 0, bits = 0;
    uint32_t sampleRate = 0, dataBytes = 0;
    bool haveFormat = false, haveData = false;
    while (file && !(haveFormat && haveData)) {
        unsigned char chunkHeader[8]{};
        file.read(reinterpret_cast<char*>(chunkHeader), sizeof(chunkHeader));
        if (file.gcount() != sizeof(chunkHeader)) break;
        uint32_t size = readU32(chunkHeader + 4);
        if (std::memcmp(chunkHeader, "fmt ", 4) == 0) {
            if (size < 16) {
                error = "invalid WAV format chunk";
                return data;
            }
            unsigned char fmt[16]{};
            file.read(reinterpret_cast<char*>(fmt), sizeof(fmt));
            format = readU16(fmt);
            channels = readU16(fmt + 2);
            sampleRate = readU32(fmt + 4);
            bits = readU16(fmt + 14);
            if (size > 16) file.seekg(size - 16, std::ios::cur);
            haveFormat = true;
        } else if (std::memcmp(chunkHeader, "data", 4) == 0) {
            dataBytes = size;
            std::streampos start = file.tellg();
            if (!haveFormat) {
                file.seekg(size, std::ios::cur);
            } else {
                haveData = true;
                break;
            }
            (void)start;
        } else {
            file.seekg(size, std::ios::cur);
        }
        if (size & 1) file.seekg(1, std::ios::cur);
    }
    if (!haveFormat || !haveData || format != 3 || channels == 0 || bits != 32 || sampleRate == 0) {
        error = "unsupported WAV format (expected IEEE float PCM, 32-bit)";
        return data;
    }
    const size_t bytesPerFrame = static_cast<size_t>(channels) * sizeof(float);
    if (dataBytes == 0 || dataBytes % bytesPerFrame != 0) {
        error = "invalid WAV data chunk";
        return data;
    }
    const size_t frames = dataBytes / bytesPerFrame;
    std::vector<float> interleaved(frames * channels);
    file.read(reinterpret_cast<char*>(interleaved.data()), static_cast<std::streamsize>(dataBytes));
    if (file.gcount() != static_cast<std::streamsize>(dataBytes)) {
        error = "truncated WAV data";
        return AudioData{};
    }
    data.sampleRate = sampleRate;
    data.channels.assign(channels, std::vector<float>(frames));
    for (size_t frame = 0; frame < frames; frame++)
        for (uint16_t channel = 0; channel < channels; channel++)
            data.channels[channel][frame] = interleaved[frame * channels + channel];
    return data;
}

bool writeWavFloat(const std::string& path, const AudioData& data, std::string& error) {
    if (data.channels.empty() || data.sampleRate <= 0) {
        error = "no audio data";
        return false;
    }
    const uint16_t channels = static_cast<uint16_t>(data.channels.size());
    const size_t frames = data.frames();
    for (const auto& channel : data.channels) {
        if (channel.size() != frames) {
            error = "channel lengths do not match";
            return false;
        }
    }
    const uint32_t dataBytes = static_cast<uint32_t>(frames * channels * sizeof(float));
    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    if (!file) {
        error = "cannot write audio file: " + path;
        return false;
    }
    file.write("RIFF", 4);
    writeU32(file, 36 + dataBytes);
    file.write("WAVEfmt ", 8);
    writeU32(file, 16);
    writeU16(file, 3);
    writeU16(file, channels);
    writeU32(file, static_cast<uint32_t>(data.sampleRate));
    writeU32(file, static_cast<uint32_t>(data.sampleRate) * channels * sizeof(float));
    writeU16(file, static_cast<uint16_t>(channels * sizeof(float)));
    writeU16(file, 32);
    file.write("data", 4);
    writeU32(file, dataBytes);
    for (size_t frame = 0; frame < frames; frame++)
        for (uint16_t channel = 0; channel < channels; channel++) file.write(reinterpret_cast<const char*>(&data.channels[channel][frame]), sizeof(float));
    if (!file.good()) {
        error = "failed while writing WAV data";
        return false;
    }
    return true;
}

}  // namespace eqapo
