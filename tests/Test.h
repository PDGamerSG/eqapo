// Minimal test harness: TEST(name) { CHECK(...); }
#pragma once
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

struct TestCase {
    const char* name;
    void (*fn)();
};
std::vector<TestCase>& testRegistry();
extern int g_testFailures;

struct TestRegistrar {
    TestRegistrar(const char* name, void (*fn)()) { testRegistry().push_back({name, fn}); }
};

#define TEST(name)                                           \
    static void test_##name();                               \
    static TestRegistrar registrar_##name(#name, test_##name); \
    static void test_##name()

#define CHECK(cond)                                                              \
    do {                                                                         \
        if (!(cond)) {                                                           \
            std::printf("  FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);        \
            g_testFailures++;                                                    \
        }                                                                        \
    } while (0)

#define CHECK_EQ(a, b)                                                                     \
    do {                                                                                   \
        auto va_ = (a);                                                                    \
        auto vb_ = (b);                                                                    \
        if (!(va_ == vb_)) {                                                               \
            std::printf("  FAIL %s:%d: %s == %s\n", __FILE__, __LINE__, #a, #b);           \
            std::printf("        lhs: %s\n        rhs: %s\n", testToString(va_).c_str(),   \
                        testToString(vb_).c_str());                                        \
            g_testFailures++;                                                              \
        }                                                                                  \
    } while (0)

#define CHECK_NEAR(a, b, tol)                                                                    \
    do {                                                                                         \
        double va_ = static_cast<double>(a);                                                     \
        double vb_ = static_cast<double>(b);                                                     \
        if (!(std::fabs(va_ - vb_) <= (tol))) {                                                  \
            std::printf("  FAIL %s:%d: %s ~= %s  (%.9g vs %.9g, tol %g)\n", __FILE__, __LINE__,  \
                        #a, #b, va_, vb_, (double)(tol));                                        \
            g_testFailures++;                                                                    \
        }                                                                                        \
    } while (0)

inline std::string testToString(const std::string& s) { return "\"" + s + "\""; }
inline std::string testToString(const char* s) { return std::string("\"") + s + "\""; }
inline std::string testToString(bool b) { return b ? "true" : "false"; }
template <typename T>
std::string testToString(const T& v) {
    if constexpr (std::is_integral_v<T>) return std::to_string(v);
    else if constexpr (std::is_floating_point_v<T>) return std::to_string(v);
    else return "<value>";
}
template <typename T>
std::string testToString(const std::vector<T>& v) {
    std::string s = "[";
    for (size_t i = 0; i < v.size(); i++) {
        if (i) s += ", ";
        s += testToString(v[i]);
    }
    return s + "]";
}
