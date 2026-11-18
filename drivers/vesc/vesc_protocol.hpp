#pragma once

// CAN-Protokoll des VESC (Firmware 6.x), soweit der Treiber es braucht.
//
// Quelle: VESC-Firmware comm_can.c / datatypes.h (github.com/vedderb/bldc) und
// "VESC6 CAN Commands and Telemetry" (vesc-project.com).
//
// Der VESC nutzt nur 29-Bit-IDs:
//   Bits 0..7   VESC-ID (im VESC-Tool einstellbar, "Controller ID")
//   Bits 8..15  Pakettyp (PacketId)
// Alle Zahlen in den Daten sind big-endian (höchstwertiges Byte zuerst).
//
// Hinweis: LARS (80d4b5e) schreibt die Werte little-endian und setzt im Filter das RTR- statt
// des IDE-Bits. Beides ist hier korrigiert. Der Vorgänger-Code sendet big-endian und lief.

#include <cstdint>

namespace vesc {

enum class PacketId : uint8_t {
    set_duty = 0,               // int32 Duty * 100000
    set_current = 1,            // int32 Strom in mA
    set_current_brake = 2,      // int32 Bremsstrom in mA
    set_rpm = 3,                // int32 ERPM (elektrische Umdrehungen pro Minute)
    set_pos = 4,                // int32 Position in Grad * 1000000
    process_short_buffer = 8,   // kurzer UART-Befehl über CAN getunnelt (z. B. Servo-Position)
    status_1 = 9,               // int32 ERPM, int16 Strom*10, int16 Duty*1000
    status_2 = 14,              // int32 Ah*10000, int32 Ah geladen*10000
    status_3 = 15,              // int32 Wh*10000, int32 Wh geladen*10000
    status_4 = 16,              // int16 T_FET*10, int16 T_Motor*10, int16 I_in*10, int16 PID-Pos*50
    status_5 = 27,              // int32 Tacho, int16 U_in*10, int16 reserviert
    status_6 = 58,              // int16 ADC1..3*1000, int16 PPM*1000
};

// Befehlsnummern des UART-Protokolls, die über process_short_buffer getunnelt werden.
enum class CommId : uint8_t {
    set_servo_pos = 12,  // int16 Position * 1000, Position 0..1 (VESC: 0 = 1 ms, 1 = 2 ms Puls)
};

constexpr uint32_t can_id(uint8_t vesc_id, PacketId packet) {
    return (static_cast<uint32_t>(packet) << 8) | vesc_id;
}
constexpr uint8_t vesc_id_of(uint32_t can_id) { return static_cast<uint8_t>(can_id & 0xFF); }
constexpr PacketId packet_of(uint32_t can_id) { return static_cast<PacketId>((can_id >> 8) & 0xFF); }

inline void put_i32(uint8_t* p, int32_t v) {
    const uint32_t u = static_cast<uint32_t>(v);
    p[0] = static_cast<uint8_t>(u >> 24);
    p[1] = static_cast<uint8_t>(u >> 16);
    p[2] = static_cast<uint8_t>(u >> 8);
    p[3] = static_cast<uint8_t>(u);
}
inline void put_i16(uint8_t* p, int16_t v) {
    const uint16_t u = static_cast<uint16_t>(v);
    p[0] = static_cast<uint8_t>(u >> 8);
    p[1] = static_cast<uint8_t>(u);
}
inline int32_t get_i32(const uint8_t* p) {
    return static_cast<int32_t>((static_cast<uint32_t>(p[0]) << 24) | (static_cast<uint32_t>(p[1]) << 16) |
                                (static_cast<uint32_t>(p[2]) << 8) | p[3]);
}
inline int16_t get_i16(const uint8_t* p) {
    return static_cast<int16_t>((static_cast<uint16_t>(p[0]) << 8) | p[1]);
}

}  // namespace vesc
