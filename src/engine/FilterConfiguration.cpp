#include "engine/FilterConfiguration.h"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace eqapo {

FilterConfiguration::FilterConfiguration(std::vector<FilterInfo> infos, unsigned realChannelCount,
                                         unsigned outputChannelCount, unsigned allChannelCount, unsigned maxFrameCount)
    : realChannelCount_(realChannelCount),
      outputChannelCount_(outputChannelCount),
      allChannelCount_(allChannelCount),
      infos_(std::move(infos)) {
    // 16-byte aligned channel rows; two sets (ping/pong for out-of-place filters)
    size_t row = (static_cast<size_t>(maxFrameCount) + 3) & ~size_t{3};
    storage_.assign(static_cast<size_t>(allChannelCount) * row * 2, 0.0f);
    allSamples_.resize(allChannelCount);
    allSamples2_.resize(allChannelCount);
    for (unsigned c = 0; c < allChannelCount; c++) {
        allSamples_[c] = storage_.data() + c * row;
        allSamples2_[c] = storage_.data() + (allChannelCount + c) * row;
    }
    size_t maxPtrs = std::max<size_t>(allChannelCount, 1);
    for (const FilterInfo& fi : infos_) {
        maxPtrs = std::max(maxPtrs, fi.inChannels.size());
        maxPtrs = std::max(maxPtrs, fi.outChannels.size());
    }
    currentSamples_.assign(maxPtrs, nullptr);
    currentSamples2_.assign(maxPtrs, nullptr);
}

void FilterConfiguration::read(float** input, unsigned frameCount) {
    for (unsigned c = 0; c < realChannelCount_ && c < allChannelCount_; c++)
        std::memcpy(allSamples_[c], input[c], frameCount * sizeof(float));
}

void FilterConfiguration::process(unsigned frameCount) {
    for (unsigned c = realChannelCount_; c < allChannelCount_; c++) std::memset(allSamples_[c], 0, frameCount * sizeof(float));

    // mono input with >= stereo output: upmix like the Windows audio system would do without an APO
    if (realChannelCount_ == 1 && outputChannelCount_ >= 2 && allChannelCount_ >= 2)
        std::memcpy(allSamples_[1], allSamples_[0], frameCount * sizeof(float));

    for (FilterInfo& fi : infos_) {
        for (size_t j = 0; j < fi.inChannels.size(); j++) currentSamples_[j] = allSamples_[fi.inChannels[j]];
        if (fi.inPlace) {
            for (size_t j = 0; j < fi.outChannels.size(); j++) currentSamples2_[j] = allSamples_[fi.outChannels[j]];
        } else {
            for (size_t j = 0; j < fi.outChannels.size(); j++) currentSamples2_[j] = allSamples2_[fi.outChannels[j]];
        }
        fi.filter->process(currentSamples2_.data(), currentSamples_.data(), frameCount);
        if (!fi.inPlace) {
            for (size_t j = 0; j < fi.outChannels.size(); j++)
                std::swap(allSamples_[fi.outChannels[j]], allSamples2_[fi.outChannels[j]]);
        }
    }
}

unsigned FilterConfiguration::doTransition(const FilterConfiguration& next, unsigned frameCount,
                                           unsigned transitionCounter, unsigned transitionLength) {
    const unsigned channels = std::min({outputChannelCount_, allChannelCount_, next.allChannelCount_});
    for (unsigned f = 0; f < frameCount; f++) {
        float factor = 1.0f;
        if (transitionCounter < transitionLength)
            factor = 0.5f * (1.0f - std::cos(static_cast<float>(transitionCounter) * static_cast<float>(M_PI) /
                                             static_cast<float>(transitionLength)));
        for (unsigned c = 0; c < channels; c++)
            allSamples_[c][f] = allSamples_[c][f] * (1.0f - factor) + next.allSamples_[c][f] * factor;
        transitionCounter++;
    }
    return transitionCounter;
}

void FilterConfiguration::write(float** output, unsigned frameCount) const {
    for (unsigned c = 0; c < outputChannelCount_; c++) {
        if (c < allChannelCount_) std::memcpy(output[c], allSamples_[c], frameCount * sizeof(float));
        else std::memset(output[c], 0, frameCount * sizeof(float));
    }
}

std::vector<std::string> FilterConfiguration::describe() const {
    std::vector<std::string> lines;
    for (const FilterInfo& fi : infos_) lines.push_back(fi.filter->describe());
    return lines;
}

}  // namespace eqapo
