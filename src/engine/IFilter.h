#pragma once
#include <string>
#include <vector>

#include "engine/ChannelHelper.h"

namespace eqapo {

// One processing stage. Lifetime: created by a factory while parsing, initialised
// once with the sample rate and the selected channels, then process() is called
// from the audio thread only.
class IFilter {
public:
    virtual ~IFilter() = default;

    // Request to receive every channel instead of the current selection (Copy).
    virtual bool getAllChannels() const { return false; }
    // False when output and input must not alias (filter reads inputs it writes).
    virtual bool getInPlace() const { return true; }
    // True when the channel list returned by initialize() becomes the new selection (Channel).
    virtual bool getSelectChannels() const { return false; }
    // Returns the channel names produced by this filter (may add or remove channels).
    virtual Channels initialize(float sampleRate, unsigned maxFrameCount, const Channels& channelNames) = 0;
    // Audio thread. output[c]/input[c] are planar float buffers of frameCount samples.
    virtual void process(float** output, float** input, unsigned frameCount) = 0;
    // Human-readable one-liner for status output.
    virtual std::string describe() const = 0;
};

}  // namespace eqapo
