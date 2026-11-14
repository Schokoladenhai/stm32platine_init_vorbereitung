#include "test/test_led.h"

#include <cstring>

#include "main.h"

namespace test_led {
namespace {

struct Led {
    const char* name;
    GPIO_TypeDef* port;
    uint16_t pin;
};

const Led leds[] = {
    {"red", DEBUG_LED_RED_GPIO_Port, DEBUG_LED_RED_Pin},
    {"green", DEBUG_LED_GREEN_GPIO_Port, DEBUG_LED_GREEN_Pin},
    {"blue", DEBUG_LED_BLUE_GPIO_Port, DEBUG_LED_BLUE_Pin},
};

void cmd_led(int argc, char** argv) {
    if (argc != 3) {
        shell::err("Aufruf: led <red|green|blue> <on|off|toggle>");
        return;
    }
    for (const Led& led : leds) {
        if (std::strcmp(argv[1], led.name) != 0) {
            continue;
        }
        if (std::strcmp(argv[2], "on") == 0) {
            HAL_GPIO_WritePin(led.port, led.pin, GPIO_PIN_SET);
        } else if (std::strcmp(argv[2], "off") == 0) {
            HAL_GPIO_WritePin(led.port, led.pin, GPIO_PIN_RESET);
        } else if (std::strcmp(argv[2], "toggle") == 0) {
            HAL_GPIO_TogglePin(led.port, led.pin);
        } else {
            shell::err("unbekannter Zustand '%s'", argv[2]);
            return;
        }
        shell::ok();
        return;
    }
    shell::err("unbekannte LED '%s'", argv[1]);
}

const shell::Command commands[] = {
    {"led", "led <red|green|blue> <on|off|toggle> (blau blinkt im Test-Build ohnehin)", cmd_led},
};

}  // namespace

shell::CommandTable table() { return SHELL_TABLE(commands); }

}  // namespace test_led
