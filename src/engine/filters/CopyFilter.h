#pragma once
#include <string>
#include <vector>

#include "engine/IFilter.h"

namespace eqapo {

struct CopyAssignment {
    std::string targetChannel;
    struct Summand {
        double factor = 1.0;
        bool isDecibel = false;
        std::string channel;  // empty => constant value `factor`
    };
    std::vector<Summand> sourceSum;
};

// `Copy: T=f*S+... T2=...` — replaces target channels by weighted sums of source channels.
// Targets that do not exist yet are created as virtual channels.
class CopyFilter : public IFilter {
public:
    explicit CopyFilter(std::vector<CopyAssignment> assignments);
    bool getAllChannels() const override { return true; }
    bool getInPlace() const override { return false; }
    Channels initialize(float sampleRate, unsigned maxFrameCount, const Channels& channelNames) override;
    void process(float** output, float** input, unsigned frameCount) override;
    std::string describe() const override;
    const std::vector<CopyAssignment>& assignments() const { return assignments_; }

private:
    struct InternalSummand {
        float factor;
        int channel;  // -1 => constant
    };
    struct InternalAssignment {
        int targetChannel;
        std::vector<InternalSummand> sourceSum;
    };
    std::vector<CopyAssignment> assignments_;
    std::vector<InternalAssignment> internal_;
};

}  // namespace eqapo
