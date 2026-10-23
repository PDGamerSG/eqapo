#include <cmath>
#include <fstream>
#include <string>

#include "Test.h"
#include "TestUtil.h"
#include "engine/FilterEngine.h"
#include "engine/expr/Expression.h"
#include "engine/filters/BiQuad.h"

using namespace eqapo;

namespace {
double num(ExpressionEvaluator& e, const std::string& s) { return e.evaluate(s).toNumber(); }
std::string str(ExpressionEvaluator& e, const std::string& s) { return e.evaluate(s).toString(); }
bool throws(ExpressionEvaluator& e, const std::string& s) {
    try {
        e.evaluate(s);
    } catch (const ExpressionError&) {
        return true;
    }
    return false;
}
std::string writeTemp(const std::string& name, const std::string& content) {
    std::string path = "/tmp/eqapo_expr_" + name;
    std::ofstream(path) << content;
    return path;
}
}  // namespace

TEST(expr_arithmetic_precedence) {
    ExpressionEvaluator e;
    CHECK_NEAR(num(e, "1 + 2 * 3"), 7, 1e-12);
    CHECK_NEAR(num(e, "(1 + 2) * 3"), 9, 1e-12);
    CHECK_NEAR(num(e, "2 ^ 3 ^ 2"), 512, 1e-12);  // right associative
    CHECK_NEAR(num(e, "-2 ^ 2"), -4, 1e-12);      // power binds tighter than unary minus
    CHECK_NEAR(num(e, "10 / 4"), 2.5, 1e-12);
    CHECK_NEAR(num(e, "7 - 2 - 1"), 4, 1e-12);
    CHECK_NEAR(num(e, "1.5e3 + .5"), 1500.5, 1e-12);
    CHECK_NEAR(num(e, "sqrt(16) + abs(-3) + max(1, 5, 2) + min(4, 2) + sum(1, 2, 3)"), 4 + 3 + 5 + 2 + 6, 1e-12);
    CHECK_NEAR(num(e, "20*log10(0.5)"), -6.0206, 1e-4);
    CHECK_NEAR(num(e, "sin(pi/2) + cos(0) + exp(0) + ln(e)"), 4, 1e-12);
    CHECK_NEAR(num(e, "6 & 3"), 2, 1e-12);
    CHECK_NEAR(num(e, "6 | 1"), 7, 1e-12);
    CHECK_NEAR(num(e, "1 << 4"), 16, 1e-12);
    CHECK_NEAR(num(e, "(int) 3.9 + (float) 2"), 5, 1e-12);
}

TEST(expr_strings_bools_comparisons_ternary) {
    ExpressionEvaluator e;
    CHECK_EQ(str(e, "\"a\" + \"b\" + 1"), std::string("ab1"));
    CHECK_EQ(str(e, "toupper(\"abc\") + tolower(\"DEF\")"), std::string("ABCdef"));
    CHECK_NEAR(num(e, "strlen(\"hello\")"), 5, 1e-12);
    CHECK_NEAR(num(e, "str2dbl(\"2.5\") * 2"), 5, 1e-12);
    CHECK_EQ(str(e, "1 < 2"), std::string("true"));
    CHECK_EQ(str(e, "1 >= 2"), std::string("false"));
    CHECK_EQ(str(e, "\"x\" == \"x\" and 2 != 3"), std::string("true"));
    CHECK_EQ(str(e, "true or false"), std::string("true"));
    CHECK_EQ(str(e, "true xor true"), std::string("false"));
    CHECK_EQ(str(e, "!true"), std::string("false"));
    CHECK_EQ(str(e, "3 > 2 ? \"big\" : \"small\""), std::string("big"));
    CHECK_EQ(str(e, "sin(0)^2 + cos(0)^2 == 1"), std::string("true"));
    CHECK_EQ(str(e, "\"escaped \\\"quote\\\" and \\\\ backslash\""), std::string("escaped \"quote\" and \\ backslash"));
    CHECK_EQ(str(e, "20*log10(0.5)").substr(0, 7), std::string("-6.0205"));
    CHECK_EQ(str(e, "48000 / 100"), std::string("480"));
    CHECK_EQ(str(e, "3 * 2"), std::string("6"));
}

TEST(expr_variables_arrays_regex) {
    ExpressionEvaluator e;
    CHECK_NEAR(num(e, "a = 2; b = a * 3; b + 1"), 7, 1e-12);
    CHECK(e.hasVariable("a"));
    CHECK_NEAR(num(e, "a += 5; a"), 7, 1e-12);
    CHECK_NEAR(num(e, "a /= 2"), 3.5, 1e-12);
    e.clearVariables();
    CHECK(throws(e, "a + 1"));
    CHECK(throws(e, "pi = 3"));
    CHECK(throws(e, "unknownFunction(1)"));
    CHECK(throws(e, "1 +"));
    CHECK(throws(e, "readRegString(\"k\", \"v\")"));
    CHECK_NEAR(num(e, "sizeof({1, 2, 3})"), 3, 1e-12);
    CHECK_NEAR(num(e, "{10, 20, 30}[1]"), 20, 1e-12);
    CHECK_NEAR(num(e, "x = regexSearch(\"High Definition .*\", \"High Definition Audio Device\"); sizeof(x)"), 1, 1e-12);
    CHECK_NEAR(num(e, "sizeof(regexSearch(\"nope\", \"text\"))"), 0, 1e-12);
    CHECK_EQ(str(e, "regexReplace(\"a+\", \"baaad\", \"-\")"), std::string("b-d"));
    CHECK_EQ(str(e, "regexSearch(\"(\\\\d+) Hz\", \"at 440 Hz\")[1]"), std::string("440"));
}

TEST(engine_if_elseif_else_and_eval) {
    std::string cfg =
        "Eval: a = 0; b = pi\n"
        "If: sampleRate == 44100\n"
        "  Preamp: -1 dB\n"
        "ElseIf: sampleRate == 48000\n"
        "  Preamp: -2 dB\n"
        "  If: inputChannelCount == 1\n"
        "    Preamp: -10 dB\n"
        "  ElseIf: inputChannelCount == 2\n"
        "    Preamp: -20 dB\n"
        "  Else:\n"
        "    Preamp: -30 dB\n"
        "  EndIf:\n"
        "Else:\n"
        "  Preamp: -3 dB\n"
        "EndIf:\n"
        "If: a > b\n"
        "  Preamp: -99 dB\n"
        "EndIf:\n"
        "If: stage == \"post-mix\" and deviceName == \"Test Device\"\n"
        "  Preamp: -4 dB\n"
        "EndIf:\n";
    std::string path = writeTemp("if.txt", cfg);
    FilterEngine engine;
    engine.setDeviceInfo("Test Device", "uid");
    engine.initialize(48000, 2, 2, 64);
    engine.loadConfig(path);
    CHECK_EQ(engine.describeLoaded(), (std::vector<std::string>{"Preamp -2 dB", "Preamp -20 dB", "Preamp -4 dB"}));

    FilterEngine engine44;
    engine44.initialize(44100, 1, 1, 64);
    engine44.loadConfig(path);
    CHECK_EQ(engine44.describeLoaded(), (std::vector<std::string>{"Preamp -1 dB"}));
}

TEST(engine_inline_expressions_and_iir_example) {
    std::string cfg =
        "Eval: linGain = 0.5\n"
        "Filter: ON PK Fc `500*2` Hz Gain `20*log10(linGain)` dB Q 10.0\n"
        "Preamp: `-3` dB\n";
    std::string path = writeTemp("inline.txt", cfg);
    FilterEngine engine;
    engine.initialize(48000, 2, 2, 64);
    engine.loadConfig(path);
    std::vector<std::string> d = engine.describeLoaded();
    CHECK_EQ(d.size(), size_t{2});
    if (d.size() == 2) {
        CHECK_EQ(d[0].substr(0, 26), std::string("PK Fc 1000 Hz Gain -6.0206"));
        CHECK_EQ(d[1], std::string("Preamp -3 dB"));
    }

    // Equalizer APO's iir_lowpass.txt example must equal `Filter: ON LP Fc 3000 Hz`
    std::string iir =
        "Eval: freq = 3000\n"
        "Eval: Q = 1/sqrt(2)\n"
        "Eval: omega = 2 * pi * freq / sampleRate\n"
        "Eval: sn = sin(omega)\n"
        "Eval: cs = cos(omega)\n"
        "Eval: alpha = sin(omega) / (2 * Q)\n"
        "Eval: a0 = (1 - cs) / 2\n"
        "Eval: a1 = 1 - cs\n"
        "Eval: a2 = (1 - cs) / 2\n"
        "Eval: b0 = 1 + alpha\n"
        "Eval: b1 = -2 * cs\n"
        "Eval: b2 = 1 - alpha\n"
        "Filter: ON IIR Order 2 Coefficients `a0` `a1` `a2` `b0` `b1` `b2`\n";
    std::string iirPath = writeTemp("iir.txt", iir);
    std::string lpPath = writeTemp("lp.txt", "Filter: ON LP Fc 3000 Hz\n");
    FilterEngine e1, e2;
    e1.initialize(48000, 1, 1, 64);
    e2.initialize(48000, 1, 1, 64);
    e1.loadConfig(iirPath);
    e2.loadConfig(lpPath);
    CHECK_EQ(e1.describeLoaded(), (std::vector<std::string>{"IIR order 2"}));
    std::vector<float> in(256), o1(256), o2(256);
    for (size_t i = 0; i < in.size(); i++) in[i] = static_cast<float>(std::sin(0.2 * i));
    for (size_t pos = 0; pos < in.size(); pos += 64) {
        float* ip = in.data() + pos;
        float* a = o1.data() + pos;
        float* b = o2.data() + pos;
        e1.process(&a, &ip, 64);
        e2.process(&b, &ip, 64);
    }
    for (size_t i = 0; i < in.size(); i++) CHECK_NEAR(o1[i], o2[i], 1e-6);
}

TEST(engine_if_errors_are_logged_not_fatal) {
    std::string cfg = "If: nonsense +\nPreamp: -1 dB\nEndIf:\nElse:\nEndIf:\nPreamp: -2 dB\nIf: 1 == 1\nPreamp: -3 dB\n";
    std::string path = writeTemp("iferr.txt", cfg);
    FilterEngine engine;
    engine.initialize(48000, 2, 2, 64);
    engine.loadConfig(path);
    // failed If evaluates false (skips -1 dB); stray Else/EndIf are ignored; unclosed If is reported
    CHECK_EQ(engine.describeLoaded(), (std::vector<std::string>{"Preamp -2 dB", "Preamp -3 dB"}));
}
