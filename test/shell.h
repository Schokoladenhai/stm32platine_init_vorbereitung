#pragma once

#include <cstddef>

// Zeilenbasierte Kommando-Shell über USB-CDC (nur im Test-Build).
// Jede Antwort endet mit genau einer Zeile "OK ..." oder "ERR ...",
// davor dürfen beliebig viele Datenzeilen stehen. So lässt sie sich
// von Hand und per Skript bedienen.
namespace shell {

struct Command {
    const char* name;  // erstes Wort der Zeile
    const char* help;  // eine Zeile für "help"
    void (*run)(int argc, char** argv);  // argv[0] ist der Befehlsname
};

struct CommandTable {
    const Command* commands;
    size_t count;
};

// Startet die Shell-Task mit den übergebenen Befehlstabellen.
void start(const CommandTable* tables, size_t table_count);

// Ausgabe für Befehle
void print(const char* fmt, ...) __attribute__((format(printf, 1, 2)));  // Datenzeile
void ok();
void ok(const char* fmt, ...) __attribute__((format(printf, 1, 2)));
void err(const char* fmt, ...) __attribute__((format(printf, 1, 2)));

// Festkomma-Text für float, weil printf in newlib-nano keine float kann.
// Schreibt in buf und gibt buf zurück, z. B. fixed(b, sizeof b, -1.25f, 2) -> "-1.25".
const char* fixed(char* buf, size_t size, float v, int decimals);

// Alle registrierten Tabellen (für "help")
const CommandTable* tables();
size_t table_count();

}  // namespace shell

// Hilfsmakro: Tabelle aus einem Array bauen
#define SHELL_TABLE(array) \
    shell::CommandTable { array, sizeof(array) / sizeof((array)[0]) }
