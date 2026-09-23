// Unit tests for the tool-mod launch-role decision (spec 3/4): the single-instance
// mechanism that REPLACES the deleted Explorer ownership gate. Mirrors the pasted
// official launcher boilerplate's command-line branching.

#include "tool_mod_launch_gate.h"
#include <cstdio>

static int g_failures = 0;

#define CHECK(cond)                                                            \
    do {                                                                       \
        if (!(cond)) {                                                         \
            std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond);        \
            ++g_failures;                                                      \
        }                                                                      \
    } while (0)

static const std::string ID = "taskbar-quick-pin";

static void test_plain_instance_is_launcher() {
    std::vector<std::string> argv = { "C:\\Windhawk\\windhawk.exe" };
    CHECK(DecideToolModRole(argv, ID) == TMR_LAUNCHER);
}

static void test_current_tool_process() {
    std::vector<std::string> argv = { "windhawk.exe", "-tool-mod", "taskbar-quick-pin" };
    CHECK(DecideToolModRole(argv, ID) == TMR_CURRENT_TOOL);
}

static void test_other_mod_tool_process_is_noop() {
    std::vector<std::string> argv = { "windhawk.exe", "-tool-mod", "some-other-mod" };
    CHECK(DecideToolModRole(argv, ID) == TMR_OTHER_TOOL);
}

static void test_service_process_is_excluded() {
    std::vector<std::string> argv = { "windhawk.exe", "-service" };
    CHECK(DecideToolModRole(argv, ID) == TMR_EXCLUDED);
    std::vector<std::string> argv2 = { "windhawk.exe", "-service-start" };
    CHECK(DecideToolModRole(argv2, ID) == TMR_EXCLUDED);
    std::vector<std::string> argv3 = { "windhawk.exe", "-service-stop" };
    CHECK(DecideToolModRole(argv3, ID) == TMR_EXCLUDED);
}

static void test_service_takes_priority_over_tool_mod() {
    std::vector<std::string> argv = { "windhawk.exe", "-service", "-tool-mod", "taskbar-quick-pin" };
    CHECK(DecideToolModRole(argv, ID) == TMR_EXCLUDED);
}

static void test_dangling_tool_mod_with_no_id_is_launcher() {
    // "-tool-mod" as the final token has no id after it (mirrors argc-1 bound).
    std::vector<std::string> argv = { "windhawk.exe", "-tool-mod" };
    CHECK(DecideToolModRole(argv, ID) == TMR_LAUNCHER);
}

int main() {
    test_plain_instance_is_launcher();
    test_current_tool_process();
    test_other_mod_tool_process_is_noop();
    test_service_process_is_excluded();
    test_service_takes_priority_over_tool_mod();
    test_dangling_tool_mod_with_no_id_is_launcher();

    if (g_failures == 0) {
        std::printf("tool_mod_launch_gate: ALL PASS\n");
        return 0;
    }
    std::printf("tool_mod_launch_gate: %d FAILURE(S)\n", g_failures);
    return 1;
}
