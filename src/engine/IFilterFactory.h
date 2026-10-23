#pragma once
#include <memory>
#include <string>
#include <vector>

#include "engine/IFilter.h"

namespace eqapo {

class FilterEngine;
using FilterList = std::vector<std::unique_ptr<IFilter>>;

// One factory per configuration command. Factories are consulted in a fixed
// order for every `Command: parameters` line; a factory may clear `command`
// to make all later factories skip the line (Device/Stage/If gating).
class IFilterFactory {
public:
    virtual ~IFilterFactory() = default;
    virtual void initialize(FilterEngine* engine) {}
    virtual FilterList startOfConfiguration() { return {}; }
    virtual FilterList startOfFile(const std::string& configPath) { return {}; }
    virtual FilterList createFilter(const std::string& configPath, std::string& command, std::string& parameters) = 0;
    virtual FilterList endOfFile(const std::string& configPath) { return {}; }
    virtual FilterList endOfConfiguration() { return {}; }
};

}  // namespace eqapo
