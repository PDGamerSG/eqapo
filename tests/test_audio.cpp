#include <cstdio>
#include <string>

#include "Test.h"
#include "audio/AudioFile.h"

using namespace eqapo;

TEST(audio_wav_float_roundtrip_preserves_planar_samples) {
    const std::string path = "/tmp/eqapo-audio-roundtrip.wav";
    AudioData input;
    input.sampleRate = 48000;
    input.channels = {{0.0f, 0.25f, -0.5f, 1.0f}, {0.5f, -0.25f, 0.0f, -1.0f}};

    std::string error;
    CHECK(writeWavFloat(path, input, error));
    CHECK(error.empty());

    AudioData output = readAudioFile(path, error);
    CHECK(error.empty());
    CHECK_NEAR(output.sampleRate, input.sampleRate, 1e-6);
    CHECK_EQ(output.channels.size(), input.channels.size());
    CHECK_EQ(output.frames(), input.frames());
    for (size_t channel = 0; channel < input.channels.size(); channel++)
        for (size_t frame = 0; frame < input.frames(); frame++)
            CHECK_NEAR(output.channels[channel][frame], input.channels[channel][frame], 1e-6);

    std::remove(path.c_str());
}
