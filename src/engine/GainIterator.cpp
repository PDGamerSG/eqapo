#include "engine/GainIterator.h"

#include <algorithm>
#include <cmath>

namespace eqapo {

GainIterator::GainIterator(std::vector<FilterNode> nodes) : nodes_(std::move(nodes)) {}

double GainIterator::gainAt(double freq) const {
    if (nodes_.empty()) return 0.0;
    FilterNode probe{freq, 0.0};
    auto it = std::lower_bound(nodes_.begin(), nodes_.end(), probe);  // first node with freq >= probe
    if (it == nodes_.begin()) return it->dbGain;
    if (it == nodes_.end()) return (it - 1)->dbGain;
    const FilterNode& left = *(it - 1);
    const FilterNode& right = *it;
    if (left.dbGain == right.dbGain) return left.dbGain;  // supports -inf on both sides
    double logLeft = std::log(left.freq);
    double t = (std::log(freq) - logLeft) / (std::log(right.freq) - logLeft);
    return left.dbGain + t * (right.dbGain - left.dbGain);
}

}  // namespace eqapo
