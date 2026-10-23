#include "Test.h"

#include <cstring>

std::vector<TestCase>& testRegistry() {
    static std::vector<TestCase> registry;
    return registry;
}
int g_testFailures = 0;

int main(int argc, char** argv) {
    const char* filter = argc > 1 ? argv[1] : nullptr;
    int run = 0, failedTests = 0;
    for (const TestCase& tc : testRegistry()) {
        if (filter && !std::strstr(tc.name, filter)) continue;
        int before = g_testFailures;
        std::printf("[ RUN  ] %s\n", tc.name);
        tc.fn();
        run++;
        if (g_testFailures != before) {
            failedTests++;
            std::printf("[ FAIL ] %s\n", tc.name);
        } else {
            std::printf("[  OK  ] %s\n", tc.name);
        }
    }
    std::printf("\n%d tests run, %d failed (%d failed checks)\n", run, failedTests, g_testFailures);
    return failedTests == 0 ? 0 : 1;
}
