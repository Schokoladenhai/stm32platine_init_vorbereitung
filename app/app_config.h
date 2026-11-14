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

}  // namespace app_config
