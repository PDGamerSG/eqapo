#pragma once
#include <vector>

namespace eqapo {

struct FilterNode {
    double freq;
    double dbGain;
    bool operator<(const FilterNode& o) const { return freq < o.freq; }
};

// Piecewise-linear interpolation of node gains on a logarithmic frequency axis;
// constant (first/last node gain) outside the node range.
class GainIterator {
public:
    explicit GainIterator(std::vector<FilterNode> nodes);  // nodes must be sorted by freq
    double gainAt(double freq) const;

private:
    std::vector<FilterNode> nodes_;
};

}  // namespace eqapo
