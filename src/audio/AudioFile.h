#pragma once
#include <string>
#include <vector>

namespace eqapo {

struct AudioData {
    double sampleRate = 0;
    std::vector<std::vector<float>> channels;  // planar
    size_t frames() const { return channels.empty() ? 0 : channels[0].size(); }
};

// macOS reads any file AudioToolbox can decode. Linux accepts 32-bit float PCM WAV.
AudioData readAudioFile(const std::string& path, std::string& error);
// Writes a 32-bit float WAV.
bool writeWavFloat(const std::string& path, const AudioData& data, std::string& error);

}  // namespace eqapo
