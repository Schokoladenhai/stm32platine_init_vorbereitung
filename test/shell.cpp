#include "test/shell.h"

#include <cstdarg>
#include <cstdio>
#include <cstring>

#include "FreeRTOS.h"
#include "app/app_config.h"
#include "common/usb_cdc.h"
#include "task.h"

namespace shell {
namespace {

constexpr size_t line_max = 128;
constexpr int args_max = 12;

const CommandTable* registered_tables = nullptr;
size_t registered_count = 0;

StaticTask_t shell_tcb;
StackType_t shell_stack[app_config::shell_stack_words];

void write_str(const char* s) { usb_cdc::write(s, std::strlen(s)); }

void vreply(const char* prefix, const char* fmt, va_list args) {
    char buf[160];
    int n = std::snprintf(buf, sizeof(buf), "%s", prefix);
    n += std::vsnprintf(buf + n, sizeof(buf) - n, fmt, args);
    if (n > static_cast<int>(sizeof(buf)) - 3) {
        n = sizeof(buf) - 3;
    }
    buf[n++] = '\r';
    buf[n++] = '\n';
    usb_cdc::write(buf, n);
}

const Command* find(const char* name) {
    for (size_t t = 0; t < registered_count; ++t) {
        for (size_t i = 0; i < registered_tables[t].count; ++i) {
            if (std::strcmp(registered_tables[t].commands[i].name, name) == 0) {
                return &registered_tables[t].commands[i];
            }
        }
    }
    return nullptr;
}

void execute(char* line) {
    char* argv[args_max];
    int argc = 0;
    for (char* tok = std::strtok(line, " \t"); tok != nullptr && argc < args_max;
         tok = std::strtok(nullptr, " \t")) {
        argv[argc++] = tok;
    }
    if (argc == 0) {
        return;
    }
    const Command* cmd = find(argv[0]);
    if (cmd == nullptr) {
        err("unbekannter Befehl '%s', siehe help", argv[0]);
        return;
    }
    cmd->run(argc, argv);
}

void shell_task(void*) {
    char line[line_max];
    size_t len = 0;
    bool last_was_cr = false;

    for (;;) {
        uint8_t c;
        if (usb_cdc::read(&c, 1, portMAX_DELAY) == 0) {
            continue;
        }
        if (c == '\r' || c == '\n') {
            // \r\n zählt als ein Zeilenende
            if (c == '\n' && last_was_cr) {
                last_was_cr = false;
                continue;
            }
            last_was_cr = (c == '\r');
            write_str("\r\n");
            line[len] = '\0';
            execute(line);
            len = 0;
            write_str("> ");
            continue;
        }
        last_was_cr = false;
        if (c == 0x08 || c == 0x7f) {  // Rücktaste
            if (len > 0) {
                --len;
                write_str("\b \b");
            }
            continue;
        }
        if (c >= 0x20 && c < 0x7f && len < line_max - 1) {
            line[len++] = static_cast<char>(c);
            usb_cdc::write(&c, 1);  // Echo
        }
    }
}

}  // namespace

void start(const CommandTable* tables, size_t table_count) {
    registered_tables = tables;
    registered_count = table_count;
    xTaskCreateStatic(shell_task, "shell", app_config::shell_stack_words, nullptr,
                      app_config::shell_priority, shell_stack, &shell_tcb);
}

void print(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    vreply("", fmt, args);
    va_end(args);
}

void ok() { write_str("OK\r\n"); }

void ok(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    vreply("OK ", fmt, args);
    va_end(args);
}

void err(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    vreply("ERR ", fmt, args);
    va_end(args);
}

const CommandTable* tables() { return registered_tables; }
size_t table_count() { return registered_count; }

}  // namespace shell
