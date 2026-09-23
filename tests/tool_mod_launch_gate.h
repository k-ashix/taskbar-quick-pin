// Pure "what role does THIS windhawk.exe process play for the tool mod?" decision.
//
// Shared (by mirroring, like the other tests/*.h) between the Windhawk tool-mod
// launcher boilerplate pasted into taskbar-quick-pin.wh.cpp (Wh_ModInit) and its
// unit tests (tool_mod_launch_gate_test.cpp), with no Win32 deps.
//
// Background (spec 3/4): as a TOOL MOD the mod is loaded into windhawk.exe. The
// official launcher's Wh_ModInit inspects the process command line and decides:
//   * a Windhawk service process (-service / -service-start / -service-stop)
//     must NOT run the mod at all;
//   * the dedicated tool process for THIS mod (-tool-mod <this id>) runs the
//     mod body (WhTool_ModInit);
//   * a -tool-mod process for a DIFFERENT mod is a no-op here;
//   * otherwise this is the launcher instance, which spawns the dedicated
//     `windhawk.exe -tool-mod <id>` process (Wh_ModAfterInit).
// This single-instance mechanism REPLACES the old Explorer "which explorer.exe
// owns Shell_TrayWnd" ownership gate (which was deleted). It models the exact
// command-line branching of the pasted boilerplate.

#ifndef TASKBAR_QUICK_PIN_TOOL_MOD_LAUNCH_GATE_H
#define TASKBAR_QUICK_PIN_TOOL_MOD_LAUNCH_GATE_H

#include <string>
#include <vector>

enum ToolModRole {
    TMR_EXCLUDED = 0,   // a -service* process: do not run the mod
    TMR_CURRENT_TOOL,   // -tool-mod <this id>: run WhTool_ModInit in this process
    TMR_OTHER_TOOL,     // -tool-mod <other id>: no-op in this process
    TMR_LAUNCHER        // plain instance: spawn the dedicated tool process
};

// argv    : full command-line tokens; argv[0] is the process path (args start at 1,
//           mirroring the boilerplate's CommandLineToArgvW loop bounds).
// modId   : this mod's WH_MOD_ID.
//
// Contract (branch priority mirrors the boilerplate):
//   1. any -service / -service-start / -service-stop token -> EXCLUDED.
//   2. a "-tool-mod" token that HAS a following token:
//        following token == modId -> CURRENT_TOOL, else OTHER_TOOL.
//      (a trailing "-tool-mod" with no id after it is ignored -- like argc-1.)
//   3. otherwise -> LAUNCHER.
static inline ToolModRole DecideToolModRole(const std::vector<std::string>& argv,
                                            const std::string& modId) {
    for (size_t i = 1; i < argv.size(); ++i) {
        if (argv[i] == "-service" || argv[i] == "-service-start" ||
            argv[i] == "-service-stop") {
            return TMR_EXCLUDED;
        }
    }
    for (size_t i = 1; i + 1 < argv.size(); ++i) {
        if (argv[i] == "-tool-mod") {
            return (argv[i + 1] == modId) ? TMR_CURRENT_TOOL : TMR_OTHER_TOOL;
        }
    }
    return TMR_LAUNCHER;
}

#endif  // TASKBAR_QUICK_PIN_TOOL_MOD_LAUNCH_GATE_H
