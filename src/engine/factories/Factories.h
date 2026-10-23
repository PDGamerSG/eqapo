// All configuration-command factories. Each mirrors the corresponding
// Equalizer APO factory's parsing rules.
#pragma once
#include <map>
#include <stack>

#include "engine/IFilterFactory.h"
#include "engine/filters/BiQuad.h"

namespace eqapo {

// `Device: pattern; pattern` — gates following lines on the current device.
class DeviceFilterFactory : public IFilterFactory {
public:
    void initialize(FilterEngine* engine) override;
    FilterList startOfConfiguration() override;
    FilterList createFilter(const std::string& configPath, std::string& command, std::string& parameters) override;
    FilterList endOfFile(const std::string& configPath) override;
    static bool matchDevice(const std::string& deviceString, const std::string& pattern);

private:
    std::string deviceString_;
    bool deviceMatches_ = true;
};

// `Stage: pre-mix post-mix capture` — only `post-mix` exists on macOS.
class StageFilterFactory : public IFilterFactory {
public:
    FilterList startOfConfiguration() override;
    FilterList startOfFile(const std::string& configPath) override;
    FilterList createFilter(const std::string& configPath, std::string& command, std::string& parameters) override;
    FilterList endOfFile(const std::string& configPath) override;

private:
    bool stageMatches_ = true;
    std::stack<bool> stack_;
};

// `Include: file`
class IncludeFilterFactory : public IFilterFactory {
public:
    void initialize(FilterEngine* engine) override { engine_ = engine; }
    FilterList startOfConfiguration() override;
    FilterList startOfFile(const std::string& configPath) override;
    FilterList createFilter(const std::string& configPath, std::string& command, std::string& parameters) override;
    FilterList endOfFile(const std::string& configPath) override;
    static std::string resolvePath(const std::string& configPath, const std::string& value);

private:
    FilterEngine* engine_ = nullptr;
    int recursionDepth_ = -1;
};

class ChannelFilterFactory : public IFilterFactory {
public:
    FilterList createFilter(const std::string& configPath, std::string& command, std::string& parameters) override;
};

class IIRFilterFactory : public IFilterFactory {
public:
    FilterList createFilter(const std::string& configPath, std::string& command, std::string& parameters) override;
};

class BiQuadFilterFactory : public IFilterFactory {
public:
    BiQuadFilterFactory();
    FilterList createFilter(const std::string& configPath, std::string& command, std::string& parameters) override;
    // Parses a frequency string the way Equalizer APO does (thousands separator rule).
    static double getFreq(const std::string& freqString);

private:
    std::map<std::string, BiQuad::Type> nameToType_;
};

class PreampFilterFactory : public IFilterFactory {
public:
    FilterList createFilter(const std::string& configPath, std::string& command, std::string& parameters) override;
};

class DelayFilterFactory : public IFilterFactory {
public:
    FilterList createFilter(const std::string& configPath, std::string& command, std::string& parameters) override;
};

class CopyFilterFactory : public IFilterFactory {
public:
    FilterList createFilter(const std::string& configPath, std::string& command, std::string& parameters) override;
};

class ConvolutionFilterFactory : public IFilterFactory {
public:
    FilterList createFilter(const std::string& configPath, std::string& command, std::string& parameters) override;
};

class GraphicEQFilterFactory : public IFilterFactory {
public:
    FilterList createFilter(const std::string& configPath, std::string& command, std::string& parameters) override;
};

}  // namespace eqapo
