#include "app/app.h"

#include "FreeRTOS.h"
#include "app/app_config.h"
#include "common/usb_cdc.h"
#include "main.h"
#include "task.h"
#include "usb_device.h"

#if FW_TEST_BUILD
#include "test/test_main.h"
#endif

namespace {

// Lebenszeichen: Debug-Build blinkt grün, Test-Build blau, Release gar nicht
// (FW_HEARTBEAT kommt aus CMakeLists.txt). So sieht man ohne Debugger, dass
// die Firmware läuft und welche drauf ist.
#if FW_HEARTBEAT
#if FW_TEST_BUILD
GPIO_TypeDef* const heartbeat_port = DEBUG_LED_BLUE_GPIO_Port;
constexpr uint16_t heartbeat_pin = DEBUG_LED_BLUE_Pin;
#else
GPIO_TypeDef* const heartbeat_port = DEBUG_LED_GREEN_GPIO_Port;
constexpr uint16_t heartbeat_pin = DEBUG_LED_GREEN_Pin;
#endif

StaticTask_t heartbeat_tcb;
StackType_t heartbeat_stack[app_config::heartbeat_stack_words];

void heartbeat_task(void*) {
    for (;;) {
        HAL_GPIO_TogglePin(heartbeat_port, heartbeat_pin);
        vTaskDelay(pdMS_TO_TICKS(app_config::heartbeat_period_ms));
    }
}
#endif  // FW_HEARTBEAT

void app_init() {
#if FW_TEST_BUILD
    test::start();  // später: Kommando-Shell über USB-CDC
#else
    // später: Tabelle, Busse, Treiber und Tasks (siehe Architekturkonzept)
#endif
#if FW_HEARTBEAT
    xTaskCreateStatic(heartbeat_task, "heartbeat", app_config::heartbeat_stack_words, nullptr,
                      app_config::heartbeat_priority, heartbeat_stack, &heartbeat_tcb);
#endif
}

}  // namespace

extern "C" void app_main(void* /*argument*/) {
    // Puffer für USB-Empfang anlegen, bevor der USB-Stack Daten liefern kann.
    usb_cdc::init();
    // Das hat der CubeMX-Platzhalter in main.c gemacht und muss deshalb hier passieren.
    MX_USB_DEVICE_Init();

    app_init();

    // Die Start-Task wird nicht mehr gebraucht, FreeRTOS gibt ihren Stack frei.
    vTaskDelete(nullptr);
}
