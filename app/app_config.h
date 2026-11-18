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
// VESC-ID ("Controller ID" im VESC-Tool). Am Fahrzeug per "vesc scan" ermittelt (2026-11-18).
// Zum Vergleich: Vorgänger-Code 86, LARS 51.
inline constexpr uint8_t vesc_id = 20;

// Lenkservo am Anschluss SERVO1 (TIM3 CH1, PA6). Grenzen und Mitte aus der Messung des
// Vorgängers (SA 4.4): Mitte 1520 µs, Bereich 1095..1795 µs. Am Fahrzeug prüfen.
inline constexpr uint16_t steering_min_us = 1095;
inline constexpr uint16_t steering_max_us = 1795;
inline constexpr uint16_t steering_center_us = 1520;
inline constexpr uint16_t steering_frequency_hz = 50;

}  // namespace app_config
