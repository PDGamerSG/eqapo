#include "engine/filters/ChannelFilter.h"

#include "engine/Log.h"
#include "engine/StringHelper.h"

namespace eqapo {

ChannelFilter::ChannelFilter(std::vector<std::string> words) : words_(std::move(words)) {}

Channels ChannelFilter::initialize(float, unsigned, const Channels& channelNames) {
    std::vector<bool> selected(channelNames.size(), false);
    for (const std::string& word : words_) {
        if (word == "ALL") {
            selected.assign(channelNames.size(), true);
            continue;
        }
        int index = ChannelHelper::getChannelIndex(word, channelNames);
        if (index >= 0 && index < static_cast<int>(channelNames.size())) selected[static_cast<size_t>(index)] = true;
    }
    Channels result;
    for (size_t i = 0; i < channelNames.size(); i++)
        if (selected[i]) result.push_back(channelNames[i]);
    traceMessage("Selecting channel(s) " + StringHelper::join(result, ", "));
    return result;
}

std::string ChannelFilter::describe() const { return "Channel " + StringHelper::join(words_, " "); }

}  // namespace eqapo
