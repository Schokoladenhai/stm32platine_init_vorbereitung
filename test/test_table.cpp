#include "test/test_table.h"

#include <cstdlib>
#include <cstring>

#include "main.h"  // CMSIS: DWT-Zykluszähler für "table bench"
#include "table/table.hpp"

namespace test_table {
namespace {

using shell::fixed;

// ---- Inhalt eines Eintrags ausgeben, eine Funktion pro Typ ----------------

void show(const table::DriveCmd& v) {
    char a[16], b[16];
    shell::print("    erpm %s, steering %s", fixed(a, sizeof a, v.erpm, 0), fixed(b, sizeof b, v.steering, 3));
}

void show(const table::DriveState& v) {
    char a[16], b[16], c[16];
    shell::print("    erpm %ld, strom %s A, duty %s", static_cast<long>(v.erpm), fixed(a, sizeof a, v.current_a, 1),
                 fixed(b, sizeof b, v.duty, 3));
    shell::print("    akku %s V / %s A, FET %s C", fixed(a, sizeof a, v.voltage_in_v, 1),
                 fixed(b, sizeof b, v.current_in_a, 1), fixed(c, sizeof c, v.temp_fet_c, 1));
    shell::print("    motor %s C, tacho %ld, steering %s, vesc %s", fixed(a, sizeof a, v.temp_motor_c, 1),
                 static_cast<long>(v.tacho), fixed(b, sizeof b, v.steering, 3), v.vesc_online ? "online" : "offline");
}

template <table::Id id>
void show_entry(TickType_t now) {
    const auto e = table::get<id>();
    const table::Info& info = table::info(id);
    const TaskHandle_t w = table::writer(id);
    if (!e.valid) {
        shell::print("%-12s ungueltig     Schreiber %-10s %s", info.name, w ? pcTaskGetName(w) : "-",
                     info.description);
        return;
    }
    shell::print("%-12s vor %6lu ms Schreiber %-10s %s", info.name, static_cast<unsigned long>(e.age_ms(now)),
                 w ? pcTaskGetName(w) : "-", info.description);
    show(e.value);
}

void list() {
    const TickType_t now = xTaskGetTickCount();
#define TABLE_ENTRY(name, type, desc) show_entry<table::Id::name>(now);
#include "table/table_entries.def"
#undef TABLE_ENTRY
    shell::ok("%u Eintraege", static_cast<unsigned>(table::count));
}

bool parse_float(const char* text, float& value) {
    char* end = nullptr;
    value = std::strtof(text, &end);
    return end != text && *end == '\0';
}

// Schreibt drive_cmd. Danach ist die Shell-Task Besitzer dieses Eintrags: Im Test-Build gibt
// es keinen anderen Schreiber, im normalen Build gibt es diese Shell nicht.
void set_drive_cmd(int argc, char** argv) {
    table::DriveCmd cmd;
    if (argc != 5 || !parse_float(argv[3], cmd.erpm) || !parse_float(argv[4], cmd.steering)) {
        shell::err("Aufruf: table set drive_cmd <erpm> <steering -1..1>");
        return;
    }
    table::set<table::Id::drive_cmd>(cmd);
    shell::ok();
}

// Misst, wie lange get/set auf dem größten Eintrag dauern (= wie lange die Interrupts gesperrt sind,
// plus Aufruf-Overhead). Schreibt drive_cmd mit dem aktuellen Wert zurück, ändert also nichts.
void bench() {
    constexpr int runs = 1000;
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CYCCNT = 0;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;

    uint32_t t0 = DWT->CYCCNT;
    for (int i = 0; i < runs; ++i) {
        volatile auto e = table::get<table::Id::drive_state>();
        (void)e;
    }
    const uint32_t get_cycles = (DWT->CYCCNT - t0) / runs;

    const auto cmd = table::get<table::Id::drive_cmd>().value;
    t0 = DWT->CYCCNT;
    for (int i = 0; i < runs; ++i) {
        table::set<table::Id::drive_cmd>(cmd);
    }
    const uint32_t set_cycles = (DWT->CYCCNT - t0) / runs;

    const uint32_t mhz = SystemCoreClock / 1000000u;
    shell::print("get drive_state (%u Byte): %lu Takte = %lu ns", static_cast<unsigned>(sizeof(table::DriveState)),
                 static_cast<unsigned long>(get_cycles), static_cast<unsigned long>(get_cycles * 1000u / mhz));
    shell::print("set drive_cmd   (%u Byte): %lu Takte = %lu ns", static_cast<unsigned>(sizeof(table::DriveCmd)),
                 static_cast<unsigned long>(set_cycles), static_cast<unsigned long>(set_cycles * 1000u / mhz));
    shell::ok("Mittel ueber %d Aufrufe bei %lu MHz", runs, static_cast<unsigned long>(mhz));
}

void cmd_table(int argc, char** argv) {
    const char* sub = argc >= 2 ? argv[1] : "list";
    if (std::strcmp(sub, "list") == 0) {
        list();
    } else if (std::strcmp(sub, "set") == 0 && argc >= 3 && std::strcmp(argv[2], "drive_cmd") == 0) {
        set_drive_cmd(argc, argv);
    } else if (std::strcmp(sub, "bench") == 0) {
        bench();
    } else {
        shell::err("Aufruf: table [list] | set drive_cmd <erpm> <steering> | bench");
    }
}

const shell::Command commands[] = {
    {"table", "table [list] | set drive_cmd <erpm> <steering> | bench", cmd_table},
};

}  // namespace

shell::CommandTable table() { return SHELL_TABLE(commands); }

}  // namespace test_table
