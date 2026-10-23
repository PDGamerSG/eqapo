// Tiny expression language for `If:`/`Eval:` lines and `inline` expressions.
// Mirrors the subset of muparserx that Equalizer APO configurations use:
// numbers, strings, booleans, arrays; + - * / ^ ; comparisons; and/or/xor/!;
// bit ops & | << >>; ternary ?: ; assignment = += -= *= /= ; (int)/(float) casts;
// functions abs sin cos tan sinh cosh tanh ln log log10 exp sqrt min max sum
// str2dbl strlen tolower toupper sizeof regexSearch regexReplace.
#pragma once
#include <map>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

namespace eqapo {

struct Value {
    enum Type { Number, Bool, String, Array };
    Type type = Number;
    double number = 0;
    bool isInteger = true;  // numbers parsed/computed as integers print without decimals
    bool boolean = false;
    std::string string;
    std::vector<Value> array;

    static Value fromNumber(double d, bool integer = false);
    static Value fromBool(bool b);
    static Value fromString(std::string s);
    static Value fromArray(std::vector<Value> a);

    std::string toString() const;
    bool toBool() const;
    double toNumber() const;  // throws for strings/arrays
    char typeChar() const;    // 'f' 'i' 'b' 's' 'm' as muparserx names them
};

class ExpressionError : public std::runtime_error {
public:
    explicit ExpressionError(const std::string& m) : std::runtime_error(m) {}
};

class ExpressionEvaluator {
public:
    ExpressionEvaluator();
    void clearVariables();
    void clearConstants();
    void defineConstant(const std::string& name, Value value);
    bool hasVariable(const std::string& name) const { return variables_.count(name) != 0; }
    // Evaluates one or more ';'-separated expressions; returns the last value.
    Value evaluate(const std::string& expression);

    struct Node;

private:
    Value eval(const Node& node);
    Value callFunction(const std::string& name, std::vector<Value>& args);
    std::map<std::string, Value> constants_;
    std::map<std::string, Value> variables_;
};

}  // namespace eqapo
