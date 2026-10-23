#pragma once
#include <string>
#include <vector>

namespace eqapo {

// Small string utilities with the exact semantics the config language needs.
struct StringHelper {
    static std::string trim(const std::string& s);
    static std::vector<std::string> split(const std::string& s, char splitChar, bool skipEmpty = true);
    static std::string toLowerCase(const std::string& s);
    static std::string toUpperCase(const std::string& s);
    // Replaces every character of `chars` found in `s` with `replacement`.
    static std::string replaceCharacters(const std::string& s, const std::string& chars,
                                         const std::string& replacement);
    static std::string replaceAll(const std::string& s, const std::string& from, const std::string& to);
    static bool startsWith(const std::string& s, const std::string& prefix);
    static bool endsWith(const std::string& s, const std::string& suffix);
    // strtod on the longest valid prefix; false if nothing parsed.
    static bool parseDouble(const std::string& s, double& out);
    static bool isSpace(char c);
    static std::string join(const std::vector<std::string>& parts, const std::string& sep);
};

}  // namespace eqapo
