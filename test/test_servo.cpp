#include "test/test_servo.h"

#include <cstdlib>
#include <cstring>

#include "FreeRTOS.h"
#include "app/app_config.h"
#include "drivers/servo/servo.hpp"
#include "task.h"

namespace test_servo {
namespace {

drivers::Servo steering({&htim3, TIM_CHANNEL_1, app_config::steering_min_us, app_config::steering_max_us,
                         app_config::steering_center_us, app_config::steering_frequency_hz});

bool ensure_running() {
    return steering.running() || steering.start();
}

void sweep() {
    const auto& c = steering.config();
    constexpr uint16_t step_us = 25;
    constexpr TickType_t step_time = pdMS_TO_TICKS(100);
    for (uint16_t us = c.center_us; us > c.min_us; us -= step_us) {
        steering.set_pulse_us(us);
        vTaskDelay(step_time);
    }
    for (uint16_t us = c.min_us; us < c.max_us; us += step_us) {
        steering.set_pulse_us(us);
        vTaskDelay(step_time);
    }
    for (uint16_t us = c.max_us; us > c.center_us; us -= step_us) {
        steering.set_pulse_us(us);
        vTaskDelay(step_time);
    }
    steering.set_pulse_us(c.center_us);
    shell::ok("%u..%u us abgefahren, jetzt Mitte %u us", c.min_us, c.max_us, c.center_us);
}

void cmd_servo(int argc, char** argv) {
    const char* arg = argc >= 2 ? argv[1] : "";
    if (argc != 2) {
        shell::err("Aufruf: servo <Pulsbreite us> | center | sweep | off | info");
        return;
    }
    if (std::strcmp(arg, "off") == 0) {
        steering.stop();
        shell::ok("Signal aus");
        return;
    }
    if (std::strcmp(arg, "info") == 0) {
        const auto& c = steering.config();
        shell::ok("%s, %u us (Grenzen %u..%u, Mitte %u, %u Hz)", steering.running() ? "an" : "aus",
                  steering.pulse_us(), c.min_us, c.max_us, c.center_us, c.frequency_hz);
        return;
    }
    if (!ensure_running()) {
        shell::err("Start fehlgeschlagen (TIM3 zaehlt nicht mit 1 MHz?)");
        return;
    }
    if (std::strcmp(arg, "center") == 0) {
        steering.set_pulse_us(steering.config().center_us);
        shell::ok("%u us", steering.pulse_us());
    } else if (std::strcmp(arg, "sweep") == 0) {
        sweep();
    } else {
        char* end = nullptr;
        const unsigned long us = std::strtoul(arg, &end, 10);
        if (end == arg || *end != '\0' || us > 3000) {
            shell::err("ungueltige Pulsbreite '%s'", arg);
            return;
        }
        steering.set_pulse_us(static_cast<uint16_t>(us));
        if (steering.pulse_us() != us) {
            shell::ok("%u us (auf Grenze begrenzt)", steering.pulse_us());
        } else {
            shell::ok("%u us", steering.pulse_us());
        }
    }
}

const shell::Command commands[] = {
    {"servo", "servo <us> | center | sweep | off | info (Lenkservo an SERVO1, PA6)", cmd_servo},
};

}  // namespace

shell::CommandTable table() { return SHELL_TABLE(commands); }

}  // namespace test_servo
