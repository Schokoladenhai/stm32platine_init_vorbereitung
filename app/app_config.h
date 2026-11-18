#pragma once

// Prioritäten, Stackgrößen und Perioden aller Tasks an einer Stelle.
// Stackgrößen in Wörtern (4 Byte).

#include "FreeRTOS.h"
#include "task.h"

namespace app_config {

// Lebenszeichen-LED
inline constexpr UBaseType_t heartbeat_priority = tskIDLE_PRIORITY + 1;
inline constexpr uint32_t heartbeat_stack_words = 128;
inline constexpr uint32_t heartbeat_period_ms = 500;

// Kommando-Shell (nur Test-Build)
inline constexpr UBaseType_t shell_priority = tskIDLE_PRIORITY + 2;
inline constexpr uint32_t shell_stack_words = 512;

// Antrieb
// VESC-ID ("Controller ID" im VESC-Tool). Der Vorgänger hat am Fahrzeug mit 86 getestet,
// LARS nutzt 51. Am Fahrzeug mit "vesc scan" in der Test-Shell prüfen.
inline constexpr uint8_t vesc_id = 86;

}  // namespace app_config
