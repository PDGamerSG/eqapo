#include "engine/factories/Factories.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <regex>
#include <sstream>

#include "engine/FilterEngine.h"
#include "engine/Log.h"
#include "engine/StringHelper.h"
#include "engine/filters/BiQuadFilter.h"
#include "engine/filters/ChannelFilter.h"
#include "engine/filters/ConvolutionFilter.h"
#include "engine/filters/CopyFilter.h"
#include "engine/filters/DelayFilter.h"
#include "engine/filters/GraphicEQFilter.h"
#include "engine/filters/IIRFilter.h"
#include "engine/filters/PreampFilter.h"

namespace eqapo {

namespace {
template <typename T, typename... Args>
FilterList one(Args&&... args) {
    FilterList list;
    list.push_back(std::make_unique<T>(std::forward<Args>(args)...));
    return list;
}
}  // namespace

// ------------------------------------------------------------------ Device

void DeviceFilterFactory::initialize(FilterEngine* engine) { deviceString_ = engine->deviceString(); }

FilterList DeviceFilterFactory::startOfConfiguration() {
    deviceMatches_ = true;
    return {};
}

FilterList DeviceFilterFactory::createFilter(const std::string&, std::string& command, std::string& parameters) {
    if (command == "Device") {
        bool matches = matchDevice(deviceString_, parameters);
        traceMessage(std::string(matches ? "M" : "Not m") + "atching pattern \"" + StringHelper::trim(parameters) +
                     "\" with device \"" + deviceString_ + "\"");
        deviceMatches_ = matches;
    }
    if (!deviceMatches_) command.clear();  // skip line for further factories
    return {};
}

FilterList DeviceFilterFactory::endOfFile(const std::string&) {
    // in the outer file the device must have matched, otherwise the inner file would not have been included
    deviceMatches_ = true;
    return {};
}

bool DeviceFilterFactory::matchDevice(const std::string& deviceString, const std::string& pattern) {
    std::string value = StringHelper::trim(pattern) + ";";
    std::vector<std::vector<std::string>> fullList;
    std::vector<std::string> currentList;
    std::string currentWord;
    for (char c : value) {
        if (c == ' ' || c == ';') {
            if (!currentWord.empty()) {
                currentList.push_back(currentWord);
                currentWord.clear();
            }
            if (c == ';' && !currentList.empty()) {
                fullList.push_back(currentList);
                currentList.clear();
            }
        } else {
            currentWord += c;
        }
    }

    static const std::regex guidRegex("\\{[0-9a-fA-F]{8}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{12}\\}");
    std::string deviceStringNoGuid = std::regex_replace(deviceString, guidRegex, "");
    std::string lowerFull = StringHelper::toLowerCase(deviceString);
    std::string lowerNoGuid = StringHelper::toLowerCase(deviceStringNoGuid);

    bool matches = false;
    for (const auto& words : fullList) {
        matches = true;
        if (words.size() == 1 && StringHelper::toLowerCase(words[0]) == "all") break;
        for (const std::string& w : words) {
            std::string word = StringHelper::toLowerCase(w);
            const std::string& matchString = word.find('{') == std::string::npos ? lowerNoGuid : lowerFull;
            if (matchString.find(word) == std::string::npos) {
                matches = false;
                break;
            }
        }
        if (matches) break;
    }
    return matches;
}

// ------------------------------------------------------------------- Stage

FilterList StageFilterFactory::startOfConfiguration() {
    stageMatches_ = true;  // post-mix is the only stage and it is the default selection
    while (!stack_.empty()) stack_.pop();
    return {};
}

FilterList StageFilterFactory::startOfFile(const std::string&) {
    stack_.push(stageMatches_);
    return {};
}

FilterList StageFilterFactory::createFilter(const std::string&, std::string& command, std::string& parameters) {
    if (command == "Stage") {
        stageMatches_ = false;
        std::string stageString = StringHelper::toLowerCase(StringHelper::trim(parameters));
        for (const std::string& part : StringHelper::split(stageString, ' ')) {
            if (part == "post-mix") stageMatches_ = true;
            else if (part == "pre-mix" || part == "capture") continue;  // never active on macOS
            else logMessage("Unknown stage \"" + part + "\"! Only pre-mix, post-mix and capture are supported.");
        }
        traceMessage(stageMatches_ ? "Matching stage \"post-mix\"" : "Not matching stage set \"" + stageString + "\"");
    }
    if (!stageMatches_) command.clear();
    return {};
}

FilterList StageFilterFactory::endOfFile(const std::string&) {
    if (!stack_.empty()) {
        stageMatches_ = stack_.top();
        stack_.pop();
    }
    return {};
}

// ----------------------------------------------------------------- Include

static const int kRecursionLimit = 100;

FilterList IncludeFilterFactory::startOfConfiguration() {
    recursionDepth_ = -1;
    return {};
}

FilterList IncludeFilterFactory::startOfFile(const std::string&) {
    recursionDepth_++;
    return {};
}

std::string IncludeFilterFactory::resolvePath(const std::string& configPath, const std::string& rawValue) {
    std::string value = rawValue;
    size_t first = 0;
    while (first < value.size() && StringHelper::isSpace(value[first])) first++;
    value = value.substr(first);
    // Equalizer APO keeps trailing spaces; Windows ignores them in paths, POSIX does not.
    value = StringHelper::trim(value);
    for (char& c : value)
        if (c == '\\') c = '/';
    if (!value.empty() && value[0] == '/') return value;
    std::string dir = configPath;
    size_t slash = dir.find_last_of('/');
    dir = slash == std::string::npos ? "" : dir.substr(0, slash + 1);
    return dir + value;
}

FilterList IncludeFilterFactory::createFilter(const std::string& configPath, std::string& command, std::string& parameters) {
    if (command == "Include") {
        std::string includePath = resolvePath(configPath, parameters);
        if (recursionDepth_ >= kRecursionLimit)
            logMessage(format("Skipping include of %s as recursion limit of %d has been reached", includePath.c_str(),
                              kRecursionLimit));
        else engine_->loadConfigFile(includePath);
        command.clear();
    }
    return {};
}

FilterList IncludeFilterFactory::endOfFile(const std::string&) {
    recursionDepth_--;
    return {};
}

// ----------------------------------------------------------------- Channel

FilterList ChannelFilterFactory::createFilter(const std::string&, std::string& command, std::string& parameters) {
    if (command != "Channel") return {};
    std::vector<std::string> words;
    std::string current;
    for (char c : parameters + " ") {
        char u = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
        if (u == ' ') {
            if (!current.empty()) {
                words.push_back(current);
                current.clear();
            }
        } else {
            current += u;
        }
    }
    return one<ChannelFilter>(words);
}

// --------------------------------------------------------------------- IIR

FilterList IIRFilterFactory::createFilter(const std::string&, std::string& command, std::string& parameters) {
    if (!StringHelper::startsWith(command, "Filter")) return {};
    static const std::regex regexType("^\\s*ON\\s+([A-Za-z]+)");
    static const std::regex regexOrder("\\s*Order\\s+([0-9]+)");
    static const std::regex regexCoefficients("\\s+Coefficients((?: [-+0-9.eE]+)+)");
    std::smatch match;
    if (!std::regex_search(parameters, match, regexType) || match.str(1) != "IIR") return {};
    if (!std::regex_search(parameters, match, regexOrder)) return {};
    unsigned order = static_cast<unsigned>(std::strtoul(match.str(1).c_str(), nullptr, 10));
    if (order < 1) {
        logMessage(format("Order %u not supported, must at least be 1", order));
        return {};
    }
    if (!std::regex_search(parameters, match, regexCoefficients)) return {};
    std::vector<std::string> coefficientStrings = StringHelper::split(match.str(1), ' ');
    if (coefficientStrings.size() != (order + 1) * 2) {
        logMessage(format("Invalid number of coefficients. Expected %u coefficients instead of %zu", (order + 1) * 2,
                          coefficientStrings.size()));
        return {};
    }
    std::vector<double> coefficients;
    for (const std::string& s : coefficientStrings) coefficients.push_back(std::strtod(s.c_str(), nullptr));
    traceMessage(format("Adding IIR filter of order %u", order));
    return one<IIRFilter>(coefficients);
}

// ------------------------------------------------------------------ BiQuad

BiQuadFilterFactory::BiQuadFilterFactory() {
    nameToType_ = {{"PK", BiQuad::PEAKING},    {"PEQ", BiQuad::PEAKING},    {"Modal", BiQuad::PEAKING},
                   {"LP", BiQuad::LOW_PASS},   {"HP", BiQuad::HIGH_PASS},   {"LPQ", BiQuad::LOW_PASS},
                   {"HPQ", BiQuad::HIGH_PASS}, {"BP", BiQuad::BAND_PASS},   {"LS", BiQuad::LOW_SHELF},
                   {"HS", BiQuad::HIGH_SHELF}, {"LSC", BiQuad::LOW_SHELF},  {"HSC", BiQuad::HIGH_SHELF},
                   {"NO", BiQuad::NOTCH},      {"AP", BiQuad::ALL_PASS}};
}

double BiQuadFilterFactory::getFreq(const std::string& freqString) {
    // remove thousands separator for locales utilizing non-breaking space (UTF-8 C2 A0)
    std::string s = StringHelper::replaceAll(freqString, "\xC2\xA0", "");
    double result = 0;
    if (!StringHelper::parseDouble(s, result)) return -1.0;
    if (s.size() >= 5 && s.find_first_of("eE") == std::string::npos && s[s.size() - 4] == '.') {
        // Interpret as thousands separator because of Room EQ Wizard ("1.000" => 1000)
        result *= 1000.0;
    }
    return result;
}

FilterList BiQuadFilterFactory::createFilter(const std::string&, std::string& command, std::string& parameters) {
    if (!StringHelper::startsWith(command, "Filter")) return {};
    static const std::regex regexType("^\\s*ON\\s+([A-Za-z]+)");
    static const std::regex regexFreq("\\s+Fc\\s*([-+0-9.eE\xC2\xA0]+)\\s*H\\s*z");
    static const std::regex regexGain("\\s+Gain\\s*([-+0-9.eE]+)\\s*dB");
    static const std::regex regexQ("\\s+Q\\s*([-+0-9.eE]+)");
    static const std::regex regexBW("\\s+BW\\s+Oct\\s*([-+0-9.eE]+)");
    static const std::regex regexSlope("^\\s*([-+0-9.eE]+)\\s*dB");

    // Conversion to period as decimal mark, if needed
    parameters = StringHelper::replaceCharacters(parameters, ",", ".");

    std::smatch match;
    if (!std::regex_search(parameters, match, regexType)) return {};
    std::string typeString = match.str(1);
    auto typeIt = nameToType_.find(typeString);
    if (typeIt == nameToType_.end()) {
        if (typeString != "None" && typeString != "IIR") logMessage("Invalid filter type " + typeString);
        return {};
    }
    BiQuad::Type type = typeIt->second;
    std::string rest = match.suffix().str();

    double freq = 0, gain = 0, bandwidthOrQOrS = 0;
    bool isBandwidthOrS = false, isCornerFreq = false, error = false;

    if (std::regex_search(rest, match, regexFreq)) {
        freq = getFreq(match.str(1));
    } else {
        logMessage("No frequency given in filter string " + typeString + rest);
        error = true;
    }

    if (std::regex_search(rest, match, regexGain)) {
        if (type == BiQuad::LOW_PASS || type == BiQuad::HIGH_PASS || type == BiQuad::NOTCH || type == BiQuad::ALL_PASS)
            traceMessage("Ignoring gain for filter of type " + typeString);
        else gain = std::strtod(match.str(1).c_str(), nullptr);
    } else if (type == BiQuad::PEAKING || type == BiQuad::LOW_SHELF || type == BiQuad::HIGH_SHELF) {
        logMessage("No gain given in filter string " + typeString + rest);
        error = true;
    }

    if (std::regex_search(rest, match, regexQ)) bandwidthOrQOrS = std::strtod(match.str(1).c_str(), nullptr);

    if (std::regex_search(rest, match, regexBW)) {
        if (type == BiQuad::LOW_SHELF || type == BiQuad::HIGH_SHELF)
            traceMessage("Ignoring bandwidth for filter of type " + typeString);
        else {
            bandwidthOrQOrS = std::strtod(match.str(1).c_str(), nullptr);
            isBandwidthOrS = true;
        }
    }

    if (std::regex_search(rest, match, regexSlope)) {
        if (!(type == BiQuad::LOW_SHELF || type == BiQuad::HIGH_SHELF))
            traceMessage("Ignoring slope for filter of type " + typeString);
        else {
            bandwidthOrQOrS = std::strtod(match.str(1).c_str(), nullptr);
            isBandwidthOrS = true;
        }
    }

    if (bandwidthOrQOrS == 0) {
        if (type == BiQuad::PEAKING || type == BiQuad::ALL_PASS) {
            logMessage("No Q or bandwidth given in filter string " + typeString + rest);
            error = true;
        } else if (type == BiQuad::LOW_PASS || type == BiQuad::HIGH_PASS || type == BiQuad::BAND_PASS) {
            bandwidthOrQOrS = M_SQRT1_2;
        } else if (type == BiQuad::LOW_SHELF || type == BiQuad::HIGH_SHELF) {
            bandwidthOrQOrS = 0.9;  // found out by experimentation with RoomEQWizard
            isBandwidthOrS = true;
        } else if (type == BiQuad::NOTCH) {
            bandwidthOrQOrS = 30.0;  // found out by experimentation with RoomEQWizard
        }
    } else if (type == BiQuad::LOW_SHELF || type == BiQuad::HIGH_SHELF) {
        if (isBandwidthOrS) bandwidthOrQOrS /= 12.0;  // maximum S is 1 for 12 dB
        if (typeString.back() != 'C') isCornerFreq = true;
    }

    if (error) return {};
    traceMessage(format("Adding %s filter with frequency %g Hz, gain %g dB, %s %g", typeString.c_str(), freq, gain,
                        isBandwidthOrS ? "BW/S" : "Q", bandwidthOrQOrS));
    return one<BiQuadFilter>(type, gain, freq, bandwidthOrQOrS, isBandwidthOrS, isCornerFreq);
}

// ------------------------------------------------------------------ Preamp

FilterList PreampFilterFactory::createFilter(const std::string&, std::string& command, std::string& parameters) {
    if (command != "Preamp") return {};
    std::string value = StringHelper::replaceCharacters(parameters, ",", ".");
    double db = 0;
    if (!StringHelper::parseDouble(StringHelper::trim(value), db)) return {};
    traceMessage(format("Adjusting preamp by %g dB", db));
    return one<PreampFilter>(db);
}

// ------------------------------------------------------------------- Delay

FilterList DelayFilterFactory::createFilter(const std::string&, std::string& command, std::string& parameters) {
    if (command != "Delay") return {};
    std::string value = StringHelper::replaceCharacters(parameters, ",", ".");
    std::istringstream stream(value);
    double delay = -1;
    std::string unit;
    stream >> delay >> unit;
    if (!(delay >= 0)) return {};
    std::string lower = StringHelper::toLowerCase(unit);
    if (lower == "ms") {
        traceMessage(format("Delaying by %g ms", delay));
        return one<DelayFilter>(delay, true);
    }
    if (lower == "samples") {
        traceMessage(format("Delaying by %g samples", delay));
        return one<DelayFilter>(delay, false);
    }
    return {};
}

// -------------------------------------------------------------------- Copy

FilterList CopyFilterFactory::createFilter(const std::string&, std::string& command, std::string& parameters) {
    if (command != "Copy") return {};
    std::vector<CopyAssignment> assignments;
    for (const std::string& assignmentString : StringHelper::split(parameters, ' ')) {
        CopyAssignment assignment;
        std::vector<std::string> parts = StringHelper::split(assignmentString, '=');
        if (parts.size() == 2) {
            assignment.targetChannel = parts[0];
            for (const std::string& summandString : StringHelper::split(parts[1], '+')) {
                std::vector<std::string> factors = StringHelper::split(summandString, '*');
                std::string factor, channel;
                if (factors.size() == 2) {
                    factor = factors[0];
                    channel = factors[1];
                } else if (factors.size() == 1) {
                    if (factors[0] == "0" || factors[0].find('.') != std::string::npos) factor = factors[0];
                    else channel = factors[0];
                }
                CopyAssignment::Summand summand;
                if (factor.empty()) {
                    summand.factor = 1.0;
                    summand.isDecibel = false;
                } else {
                    summand.factor = std::strtod(factor.c_str(), nullptr);
                    summand.isDecibel =
                        factor.size() > 2 && StringHelper::toLowerCase(factor.substr(factor.size() - 2)) == "db";
                }
                summand.channel = channel;
                assignment.sourceSum.push_back(summand);
            }
        }
        if (!assignment.targetChannel.empty() && !assignment.sourceSum.empty()) assignments.push_back(assignment);
    }
    return one<CopyFilter>(assignments);
}

// ------------------------------------------------------------- Convolution

FilterList ConvolutionFilterFactory::createFilter(const std::string& configPath, std::string& command,
                                                  std::string& parameters) {
    if (command != "Convolution") return {};
    return one<ConvolutionFilter>(IncludeFilterFactory::resolvePath(configPath, parameters));
}

// --------------------------------------------------------------- GraphicEQ

FilterList GraphicEQFilterFactory::createFilter(const std::string&, std::string& command, std::string& parameters) {
    if (command != "GraphicEQ") return {};
    std::string value = parameters;
    if (value.find('.') == std::string::npos) value = StringHelper::replaceCharacters(value, ",", ".");
    static const std::regex regexNumber("[-+0-9.eE]+");
    std::vector<FilterNode> nodes;
    auto it = std::sregex_iterator(value.begin(), value.end(), regexNumber);
    auto end = std::sregex_iterator();
    while (it != end) {
        std::string freqString = it->str();
        ++it;
        if (it == end) break;
        std::string gainString = it->str();
        ++it;
        nodes.push_back({std::strtod(freqString.c_str(), nullptr), std::strtod(gainString.c_str(), nullptr)});
    }
    std::stable_sort(nodes.begin(), nodes.end());
    traceMessage(format("Graphic equalizer with %zu nodes", nodes.size()));
    return one<GraphicEQFilter>(nodes, 16384u);
}

}  // namespace eqapo
