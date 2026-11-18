#include "test/test_main.h"

#include <cstring>

#include "FreeRTOS.h"
#include "task.h"
#include "test/shell.h"
#include "test/test_can.h"
#include "test/test_led.h"
#include "test/test_table.h"
#include "test/test_vesc.h"

namespace {

// ---- Allgemeine Befehle --------------------------------------------------

void cmd_help(int, char**) {
    for (size_t t = 0; t < shell::table_count(); ++t) {
        const shell::CommandTable& table = shell::tables()[t];
        for (size_t i = 0; i < table.count; ++i) {
            shell::print("%-10s %s", table.commands[i].name, table.commands[i].help);
        }
    }
    shell::ok();
}

void cmd_version(int, char**) { shell::ok("%s %s test", FW_VERSION, FW_BUILD_TYPE); }

// Beendet nur die Sitzung in term.sh, auf der Platine passiert nichts.
void cmd_exit(int, char**) { shell::ok(); }

char task_state(eTaskState s) {
    switch (s) {
        case eRunning: return 'R';
        case eReady: return 'r';
        case eBlocked: return 'B';
        case eSuspended: return 'S';
        case eDeleted: return 'D';
        default: return '?';
    }
}

void cmd_tasks(int, char**) {
    constexpr UBaseType_t max_tasks = 16;
    TaskStatus_t status[max_tasks];
    const UBaseType_t n = uxTaskGetSystemState(status, max_tasks, nullptr);
    if (n == 0) {
        shell::err("mehr als %u Tasks", static_cast<unsigned>(max_tasks));
        return;
    }
    shell::print("%-12s %s %4s %s", "Name", "Z", "Prio", "Stack frei (Woerter)");
    for (UBaseType_t i = 0; i < n; ++i) {
        shell::print("%-12s %c %4u %u", status[i].pcTaskName, task_state(status[i].eCurrentState),
                     static_cast<unsigned>(status[i].uxCurrentPriority),
                     static_cast<unsigned>(status[i].usStackHighWaterMark));
    }
    shell::ok("%u Tasks, Heap frei %u Byte", static_cast<unsigned>(n),
              static_cast<unsigned>(xPortGetFreeHeapSize()));
}

const shell::Command general_commands[] = {
    {"help", "alle Befehle anzeigen", cmd_help},
    {"version", "Firmware-Version und Build-Art", cmd_version},
    {"tasks", "FreeRTOS-Tasks mit Zustand, Prioritaet, freiem Stack", cmd_tasks},
    {"exit", "Terminal (term.sh) beenden", cmd_exit},
};

// ---- Registrierung: eine Zeile pro Treiber-Testdatei ---------------------

const shell::CommandTable all_tables[] = {
    SHELL_TABLE(general_commands),
    test_led::table(),
    test_can::table(),
    test_vesc::table(),
    test_table::table(),
};

}  // namespace

namespace test {

void start() { shell::start(all_tables, sizeof(all_tables) / sizeof(all_tables[0])); }

}  // namespace test
