#include "test/test_vesc.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "app/app_config.h"
#include "bus/can_bus.hpp"
#include "drivers/vesc/vesc.hpp"
#include "task.h"

namespace test_vesc {
namespace {

// Die Shell-Task besitzt hier den Treiber, so wie später task_drivetrain.
drivers::Vesc vesc(app_config::vesc_id);

// Alle 29-Bit-Frames landen hier, damit "vesc scan" auch VESCs mit anderer ID findet.
constexpr UBaseType_t rx_queue_length = 64;
StaticQueue_t rx_queue_buffer;
uint8_t rx_queue_storage[rx_queue_length * sizeof(can_bus::Frame)];
QueueHandle_t rx_queue = nullptr;

uint32_t seen_ids[8] = {};  // Bitfeld 0..255: VESC-IDs, von denen Status kam

// Wie oft Befehle wiederholt werden (VESC-Timeout ist meist 1000 ms) und wie lange höchstens.
constexpr uint32_t repeat_ms = 20;
constexpr uint32_t max_duration_ms = 10000;

bool subscribed = false;

bool ensure_subscribed() {
    if (rx_queue == nullptr) {
        rx_queue = xQueueCreateStatic(rx_queue_length, sizeof(can_bus::Frame), rx_queue_storage,
                                      &rx_queue_buffer);
    }
    if (!subscribed) {
        subscribed = can_bus::subscribe({0, 0, true}, rx_queue);
    }
    return subscribed;
}

// Holt alle wartenden Frames ab und gibt sie dem Treiber.
void drain() {
    can_bus::Frame f;
    while (rx_queue != nullptr && xQueueReceive(rx_queue, &f, 0) == pdTRUE) {
        if (vesc::packet_of(f.id) == vesc::PacketId::status_1) {
            const uint8_t id = vesc::vesc_id_of(f.id);
            seen_ids[id / 32] |= 1u << (id % 32);
        }
        vesc.handle(f);
    }
}

// Festkomma-Ausgabe, weil printf in newlib-nano keine float kann.
const char* fixed(char* buf, size_t size, float v, int decimals) {
    int scale = 1;
    for (int i = 0; i < decimals; ++i) scale *= 10;
    const bool neg = v < 0;
    const long scaled = static_cast<long>((neg ? -v : v) * scale + 0.5f);
    if (decimals == 0) {
        std::snprintf(buf, size, "%s%ld", neg ? "-" : "", scaled);
    } else {
        std::snprintf(buf, size, "%s%ld.%0*ld", neg ? "-" : "", scaled / scale, decimals, scaled % scale);
    }
    return buf;
}

bool parse_float(const char* text, float& value) {
    char* end = nullptr;
    value = std::strtof(text, &end);
    return end != text && *end == '\0';
}

bool parse_ms(int argc, char** argv, int index, uint32_t& ms) {
    ms = 0;
    if (argc <= index) return true;
    char* end = nullptr;
    const unsigned long v = std::strtoul(argv[index], &end, 10);
    if (end == argv[index] || *end != '\0' || v > max_duration_ms) return false;
    ms = static_cast<uint32_t>(v);
    return true;
}

enum class Cmd { erpm, duty, current, brake, servo };

// Sendet den Befehl einmal oder wiederholt ihn `ms` lang, danach Strom 0 (Motor frei).
void run(Cmd cmd, float value, uint32_t ms) {
    auto send = [&]() {
        switch (cmd) {
            case Cmd::erpm: return vesc.set_erpm(static_cast<int32_t>(value));
            case Cmd::duty: return vesc.set_duty(value);
            case Cmd::current: return vesc.set_current(value);
            case Cmd::brake: return vesc.set_brake_current(value);
            // Werte über 2 gelten als Pulsbreite in µs, sonst als Position 0..1
            case Cmd::servo: return value > 2.0f ? vesc.set_servo_pulse_us(value) : vesc.set_servo(value);
        }
        return false;
    };
    uint32_t sent = 0;
    uint32_t failed = 0;
    const TickType_t start = xTaskGetTickCount();
    TickType_t wake = start;
    do {
        (send() ? sent : failed)++;
        drain();
        vTaskDelayUntil(&wake, pdMS_TO_TICKS(repeat_ms));
    } while (ms > 0 && (xTaskGetTickCount() - start) < pdMS_TO_TICKS(ms));
    if (ms > 0 && cmd != Cmd::servo) {
        vesc.set_current(0);
    }
    if (sent == 0) {
        shell::err("nichts gesendet (quittiert jemand am Bus? 'can stat')");
        return;
    }
    shell::ok("%lu gesendet, %lu fehlgeschlagen", static_cast<unsigned long>(sent),
              static_cast<unsigned long>(failed));
}

void cmd_value(Cmd cmd, int argc, char** argv, const char* usage) {
    float value;
    uint32_t ms;
    if (argc < 3 || argc > 4 || !parse_float(argv[2], value) || !parse_ms(argc, argv, 3, ms)) {
        shell::err("Aufruf: %s", usage);
        return;
    }
    run(cmd, value, ms);
}

void status() {
    drain();
    const auto& s = vesc.status();
    const TickType_t now = xTaskGetTickCount();
    char a[16], b[16];
    if (s.frames == 0) {
        shell::err("noch nichts von VESC %u empfangen ('vesc scan' sucht andere IDs)", vesc.id());
        return;
    }
    shell::print("erpm        %ld", static_cast<long>(s.erpm));
    shell::print("strom       %s A, duty %s", fixed(a, sizeof a, s.current_a, 1), fixed(b, sizeof b, s.duty, 3));
    shell::print("akku        %s V, %s A", fixed(a, sizeof a, s.voltage_in_v, 1),
                 fixed(b, sizeof b, s.current_in_a, 1));
    shell::print("temperatur  FET %s C, Motor %s C", fixed(a, sizeof a, s.temp_fet_c, 1),
                 fixed(b, sizeof b, s.temp_motor_c, 1));
    shell::print("verbrauch   %s Ah, %s Wh, tacho %ld", fixed(a, sizeof a, s.amp_hours, 3),
                 fixed(b, sizeof b, s.watt_hours, 2), static_cast<long>(s.tacho));
    shell::print("pakete      status1..5 empfangen: %c%c%c%c%c", (s.seen_packets & 1) ? '1' : '-',
                 (s.seen_packets & 2) ? '2' : '-', (s.seen_packets & 4) ? '3' : '-',
                 (s.seen_packets & 8) ? '4' : '-', (s.seen_packets & 16) ? '5' : '-');
    shell::ok("VESC %u, %lu Frames, letzter vor %lu ms", vesc.id(), static_cast<unsigned long>(s.frames),
              static_cast<unsigned long>((now - s.last_frame) * portTICK_PERIOD_MS));
}

void scan(int argc, char** argv) {
    uint32_t ms;
    if (!parse_ms(argc, argv, 2, ms)) {
        shell::err("Aufruf: vesc scan [ms]");
        return;
    }
    vTaskDelay(pdMS_TO_TICKS(ms ? ms : 500));
    drain();
    unsigned n = 0;
    for (unsigned id = 0; id < 256; ++id) {
        if (seen_ids[id / 32] & (1u << (id % 32))) {
            shell::print("VESC-ID %u (0x%02X)", id, id);
            ++n;
        }
    }
    shell::ok("%u VESC gefunden (sendet nur, wenn im VESC-Tool 'CAN Status Message Mode' an ist)", n);
}

void cmd_vesc(int argc, char** argv) {
    if (!ensure_subscribed()) {
        shell::err("CAN-Anmeldung fehlgeschlagen ('can stat')");
        return;
    }
    const char* sub = argc >= 2 ? argv[1] : "";
    if (std::strcmp(sub, "id") == 0) {
        if (argc == 3) {
            const unsigned long id = std::strtoul(argv[2], nullptr, 0);
            if (id > 255) {
                shell::err("ID 0..255");
                return;
            }
            vesc = drivers::Vesc(static_cast<uint8_t>(id));
        }
        shell::ok("VESC-ID %u", vesc.id());
    } else if (std::strcmp(sub, "scan") == 0) {
        scan(argc, argv);
    } else if (std::strcmp(sub, "status") == 0) {
        status();
    } else if (std::strcmp(sub, "erpm") == 0) {
        cmd_value(Cmd::erpm, argc, argv, "vesc erpm <erpm> [ms]");
    } else if (std::strcmp(sub, "duty") == 0) {
        cmd_value(Cmd::duty, argc, argv, "vesc duty <-1..1> [ms]");
    } else if (std::strcmp(sub, "current") == 0) {
        cmd_value(Cmd::current, argc, argv, "vesc current <A> [ms]");
    } else if (std::strcmp(sub, "brake") == 0) {
        cmd_value(Cmd::brake, argc, argv, "vesc brake <A> [ms]");
    } else if (std::strcmp(sub, "servo") == 0) {
        cmd_value(Cmd::servo, argc, argv, "vesc servo <0..1 | Pulsbreite in us> [ms]");
    } else if (std::strcmp(sub, "stop") == 0) {
        if (vesc.set_current(0)) {
            shell::ok();
        } else {
            shell::err("senden fehlgeschlagen");
        }
    } else {
        shell::err("Aufruf: vesc <id|scan|status|erpm|duty|current|brake|servo|stop> ...");
    }
}

const shell::Command commands[] = {
    {"vesc",
     "vesc id [n] | scan [ms] | status | erpm|duty|current|brake|servo <wert> [ms, max 10000] | stop", cmd_vesc},
};

}  // namespace

shell::CommandTable table() { return SHELL_TABLE(commands); }

}  // namespace test_vesc
