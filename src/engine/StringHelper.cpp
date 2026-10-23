#include "engine/StringHelper.h"

#include <cctype>
#include <cstdlib>

namespace eqapo {

bool StringHelper::isSpace(char c) {
    unsigned char u = static_cast<unsigned char>(c);
    return u == ' ' || u == '\t' || u == '\r' || u == '\n' || u == '\f' || u == '\v';
}

std::string StringHelper::trim(const std::string& s) {
    size_t first = 0, last = s.size();
    while (first < last && isSpace(s[first])) first++;
    while (last > first && isSpace(s[last - 1])) last--;
    return s.substr(first, last - first);
}

std::vector<std::string> StringHelper::split(const std::string& s, char splitChar, bool skipEmpty) {
    std::vector<std::string> result;
    size_t prev = 0;
    while (true) {
        size_t pos = s.find(splitChar, prev);
        std::string part = s.substr(prev, pos == std::string::npos ? std::string::npos : pos - prev);
        if (!part.empty() || !skipEmpty) result.push_back(part);
        if (pos == std::string::npos) break;
        prev = pos + 1;
    }
    return result;
}

std::string StringHelper::toLowerCase(const std::string& s) {
    std::string r = s;
    for (char& c : r) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return r;
}

std::string StringHelper::toUpperCase(const std::string& s) {
    std::string r = s;
    for (char& c : r) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    return r;
}

std::string StringHelper::replaceCharacters(const std::string& s, const std::string& chars,
                                            const std::string& replacement) {
    std::string result;
    result.reserve(s.size());
    for (char c : s) {
        if (chars.find(c) == std::string::npos) result += c;
        else result += replacement;
    }
    return result;
}

std::string StringHelper::replaceAll(const std::string& s, const std::string& from, const std::string& to) {
    if (from.empty()) return s;
    std::string result;
    size_t pos = 0;
    while (true) {
        size_t found = s.find(from, pos);
        if (found == std::string::npos) {
            result += s.substr(pos);
            break;
        }
        result += s.substr(pos, found - pos);
        result += to;
        pos = found + from.size();
    }
    return result;
}

bool StringHelper::startsWith(const std::string& s, const std::string& prefix) {
    return s.size() >= prefix.size() && s.compare(0, prefix.size(), prefix) == 0;
}

bool StringHelper::endsWith(const std::string& s, const std::string& suffix) {
    return s.size() >= suffix.size() && s.compare(s.size() - suffix.size(), suffix.size(), suffix) == 0;
}

bool StringHelper::parseDouble(const std::string& s, double& out) {
    const char* begin = s.c_str();
    char* end = nullptr;
    double v = std::strtod(begin, &end);
    if (end == begin) return false;
    out = v;
    return true;
}

std::string StringHelper::join(const std::vector<std::string>& parts, const std::string& sep) {
    std::string r;
    for (size_t i = 0; i < parts.size(); i++) {
        if (i) r += sep;
        r += parts[i];
    }
    return r;
}

}  // namespace eqapo
