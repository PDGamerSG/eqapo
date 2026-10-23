#include "engine/filters/CopyFilter.h"

#include <algorithm>
#include <cmath>
#include <cstring>

#include "engine/Log.h"

namespace eqapo {

CopyFilter::CopyFilter(std::vector<CopyAssignment> assignments) : assignments_(std::move(assignments)) {}

Channels CopyFilter::initialize(float, unsigned, const Channels& channelNames) {
    internal_.clear();
    Channels outChannelNames;
    for (const CopyAssignment& a : assignments_) {
        InternalAssignment ia;
        std::string channelName = a.targetChannel;
        int channelIndex = ChannelHelper::getChannelIndex(a.targetChannel, channelNames, true);
        if (channelIndex != -1) channelName = channelNames[static_cast<size_t>(channelIndex)];
        auto it = std::find(outChannelNames.begin(), outChannelNames.end(), channelName);
        ia.targetChannel = static_cast<int>(it - outChannelNames.begin());
        if (it == outChannelNames.end()) outChannelNames.push_back(channelName);

        for (const CopyAssignment::Summand& s : a.sourceSum) {
            InternalSummand is;
            is.channel = s.channel.empty() ? -1 : ChannelHelper::getChannelIndex(s.channel, channelNames);
            is.factor = s.isDecibel ? static_cast<float>(std::pow(10.0, s.factor / 20.0)) : static_cast<float>(s.factor);
            ia.sourceSum.push_back(is);
        }
        internal_.push_back(std::move(ia));
    }
    return outChannelNames;
}

void CopyFilter::process(float** output, float** input, unsigned frameCount) {
    for (const InternalAssignment& ia : internal_) {
        if (ia.targetChannel < 0 || ia.sourceSum.empty()) continue;
        float* out = output[ia.targetChannel];
        {
            const InternalSummand& is = ia.sourceSum[0];
            if (is.channel == -1) {
                for (unsigned f = 0; f < frameCount; f++) out[f] = is.factor;
            } else if (is.factor == 1.0f) {
                std::memcpy(out, input[is.channel], frameCount * sizeof(float));
            } else {
                const float* in = input[is.channel];
                for (unsigned f = 0; f < frameCount; f++) out[f] = is.factor * in[f];
            }
        }
        for (size_t j = 1; j < ia.sourceSum.size(); j++) {
            const InternalSummand& is = ia.sourceSum[j];
            if (is.channel == -1) {
                for (unsigned f = 0; f < frameCount; f++) out[f] += is.factor;
            } else if (is.factor == 1.0f) {
                const float* in = input[is.channel];
                for (unsigned f = 0; f < frameCount; f++) out[f] += in[f];
            } else {
                const float* in = input[is.channel];
                for (unsigned f = 0; f < frameCount; f++) out[f] += is.factor * in[f];
            }
        }
    }
}

std::string CopyFilter::describe() const {
    std::string s = "Copy";
    for (const CopyAssignment& a : assignments_) {
        s += " " + a.targetChannel + "=";
        for (size_t i = 0; i < a.sourceSum.size(); i++) {
            const auto& sm = a.sourceSum[i];
            if (i) s += "+";
            if (sm.channel.empty()) s += format("%g", sm.factor);
            else if (sm.factor == 1.0 && !sm.isDecibel) s += sm.channel;
            else s += format("%g%s*%s", sm.factor, sm.isDecibel ? "dB" : "", sm.channel.c_str());
        }
    }
    return s;
}

}  // namespace eqapo
