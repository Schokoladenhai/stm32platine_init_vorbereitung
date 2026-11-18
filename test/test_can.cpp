#include "test/test_can.h"

#include <cstdlib>
#include <cstring>

#include "bus/can_bus.hpp"

namespace test_can {
namespace {

// Queue für "can listen". Alle Filter der Shell landen hier, "can rx" leert sie.
constexpr UBaseType_t rx_queue_length = 32;
StaticQueue_t rx_queue_buffer;
uint8_t rx_queue_storage[rx_queue_length * sizeof(can_bus::Frame)];
QueueHandle_t rx_queue = nullptr;

bool parse_hex(const char* text, uint32_t max, uint32_t& value) {
    char* end = nullptr;
    const unsigned long v = std::strtoul(text, &end, 16);
    if (end == text || *end != '\0' || v > max) {
        return false;
    }
    value = static_cast<uint32_t>(v);
    return true;
}

void print_frame(const can_bus::Frame& f) {
    char data[3 * 8 + 1] = {};
    for (uint8_t i = 0; i < f.len; ++i) {
        static const char hex[] = "0123456789ABCDEF";
        data[3 * i] = hex[f.data[i] >> 4];
        data[3 * i + 1] = hex[f.data[i] & 0xF];
        data[3 * i + 2] = ' ';
    }
    if (f.extended) {
        shell::print("%08lX x [%u]%s %s", static_cast<unsigned long>(f.id), f.len, f.remote ? " R" : "", data);
    } else {
        shell::print("%03lX   [%u]%s %s", static_cast<unsigned long>(f.id), f.len, f.remote ? " R" : "", data);
    }
}

// can send|sendx <id> [b0 .. b7]
void send(int argc, char** argv, bool force_extended) {
    if (argc < 3 || argc > 11) {
        shell::err("Aufruf: can send <id> [b0 .. b7] (hex)");
        return;
    }
    can_bus::Frame f;
    if (!parse_hex(argv[2], 0x1FFFFFFF, f.id)) {
        shell::err("ungueltige ID '%s'", argv[2]);
        return;
    }
    f.extended = force_extended || f.id > 0x7FF;
    f.len = static_cast<uint8_t>(argc - 3);
    for (int i = 3; i < argc; ++i) {
        uint32_t byte;
        if (!parse_hex(argv[i], 0xFF, byte)) {
            shell::err("ungueltiges Byte '%s'", argv[i]);
            return;
        }
        f.data[i - 3] = static_cast<uint8_t>(byte);
    }
    if (!can_bus::send(f, pdMS_TO_TICKS(100))) {
        shell::err("senden fehlgeschlagen (keine Mailbox frei, quittiert jemand am Bus?)");
        return;
    }
    shell::ok();
}

// can listen <id> <maske> [x]
void listen(int argc, char** argv) {
    if (argc < 4 || argc > 5 || (argc == 5 && std::strcmp(argv[4], "x") != 0)) {
        shell::err("Aufruf: can listen <id> <maske> [x]   (hex, x = 29-Bit-IDs, Maske 0 = alle)");
        return;
    }
    can_bus::Filter filter;
    filter.extended = argc == 5;
    const uint32_t max = filter.extended ? 0x1FFFFFFF : 0x7FF;
    if (!parse_hex(argv[2], max, filter.id) || !parse_hex(argv[3], max, filter.mask)) {
        shell::err("ungueltige ID oder Maske");
        return;
    }
    if (rx_queue == nullptr) {
        rx_queue = xQueueCreateStatic(rx_queue_length, sizeof(can_bus::Frame), rx_queue_storage,
                                      &rx_queue_buffer);
    }
    if (!can_bus::subscribe(filter, rx_queue)) {
        shell::err("anmelden fehlgeschlagen (alle %u Filter belegt?)",
                   static_cast<unsigned>(can_bus::max_subscriptions));
        return;
    }
    shell::ok("Filter aktiv, Frames mit 'can rx' abholen");
}

// can rx: alle bisher empfangenen Frames ausgeben
void rx() {
    unsigned n = 0;
    can_bus::Frame f;
    while (rx_queue != nullptr && xQueueReceive(rx_queue, &f, 0) == pdTRUE) {
        print_frame(f);
        ++n;
    }
    shell::ok("%u Frames", n);
}

void stat() {
    const can_bus::Stats s = can_bus::stats();
    shell::print("tx gesendet   %lu", static_cast<unsigned long>(s.tx_frames));
    shell::print("tx timeout    %lu", static_cast<unsigned long>(s.tx_timeouts));
    shell::print("tx fehler     %lu", static_cast<unsigned long>(s.tx_failed));
    shell::print("rx empfangen  %lu", static_cast<unsigned long>(s.rx_frames));
    shell::print("rx verworfen  %lu (Queue voll)", static_cast<unsigned long>(s.rx_dropped));
    shell::print("rx ohne Ziel  %lu", static_cast<unsigned long>(s.rx_unmatched));
    shell::print("rx ueberlauf  %lu (Hardware-FIFO)", static_cast<unsigned long>(s.rx_overruns));
    shell::print("busfehler     %lu, letzter Code 0x%08lX", static_cast<unsigned long>(s.bus_errors),
                 static_cast<unsigned long>(s.last_error));
    shell::print("bus-off       %lu mal, jetzt %s", static_cast<unsigned long>(s.bus_off),
                 s.is_bus_off ? "ja" : "nein");
    shell::print("TEC/REC       %u/%u%s", s.tx_error_count, s.rx_error_count,
                 s.error_passive ? " (error passive)" : "");
    shell::ok("%u Filter angemeldet", static_cast<unsigned>(s.subscriptions));
}

void cmd_can(int argc, char** argv) {
    const char* sub = argc >= 2 ? argv[1] : "";
    if (std::strcmp(sub, "send") == 0) {
        send(argc, argv, false);
    } else if (std::strcmp(sub, "sendx") == 0) {
        send(argc, argv, true);
    } else if (std::strcmp(sub, "listen") == 0) {
        listen(argc, argv);
    } else if (std::strcmp(sub, "rx") == 0) {
        rx();
    } else if (std::strcmp(sub, "stat") == 0) {
        stat();
    } else {
        shell::err("Aufruf: can <send|sendx|listen|rx|stat> ...");
    }
}

const shell::Command commands[] = {
    {"can", "can send <id> [bytes] | sendx (29 Bit) | listen <id> <maske> [x] | rx | stat", cmd_can},
};

}  // namespace

shell::CommandTable table() { return SHELL_TABLE(commands); }

}  // namespace test_can
