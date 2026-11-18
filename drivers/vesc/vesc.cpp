#include "drivers/vesc/vesc.hpp"

#include "task.h"

namespace drivers {

using vesc::get_i16;
using vesc::get_i32;
using vesc::PacketId;

bool Vesc::send(PacketId packet, int32_t value) {
    can_bus::Frame f;
    f.id = vesc::can_id(id_, packet);
    f.extended = true;
    f.len = 4;
    vesc::put_i32(f.data, value);
    return can_bus::send(f);
}

bool Vesc::set_erpm(int32_t erpm) { return send(PacketId::set_rpm, erpm); }

bool Vesc::set_duty(float duty) {
    if (duty > 1.0f) duty = 1.0f;
    if (duty < -1.0f) duty = -1.0f;
    return send(PacketId::set_duty, static_cast<int32_t>(duty * 100000.0f));
}

bool Vesc::set_current(float amps) {
    return send(PacketId::set_current, static_cast<int32_t>(amps * 1000.0f));
}

bool Vesc::set_brake_current(float amps) {
    if (amps < 0) amps = -amps;
    return send(PacketId::set_current_brake, static_cast<int32_t>(amps * 1000.0f));
}

bool Vesc::set_servo(float position) {
    if (position < 0.0f) position = 0.0f;
    if (position > 1.0f) position = 1.0f;

    // Byte für Byte wie im Vorgänger-Code (can.c, CAN_SendVESC_SetServo), dort am Fahrzeug getestet:
    // [Absender-ID 0][Sendemodus 0][COMM_SET_SERVO_POS][int16 Position*1000]
    // Sendemodus 0 = Antwort per CAN an den Absender; dieser Befehl antwortet aber nicht.
    can_bus::Frame f;
    f.id = vesc::can_id(id_, PacketId::process_short_buffer);
    f.extended = true;
    f.len = 5;
    f.data[0] = 0;
    f.data[1] = 0;
    f.data[2] = static_cast<uint8_t>(vesc::CommId::set_servo_pos);
    vesc::put_i16(f.data + 3, static_cast<int16_t>(position * 1000.0f + 0.5f));
    return can_bus::send(f);
}

bool Vesc::handle(const can_bus::Frame& f) {
    if (!f.extended || vesc::vesc_id_of(f.id) != id_) {
        return false;
    }
    const uint8_t* d = f.data;
    int seen_bit = -1;

    switch (vesc::packet_of(f.id)) {
        case PacketId::status_1:
            if (f.len < 8) return false;
            status_.erpm = get_i32(d);
            status_.current_a = get_i16(d + 4) / 10.0f;
            status_.duty = get_i16(d + 6) / 1000.0f;
            seen_bit = 0;
            break;
        case PacketId::status_2:
            if (f.len < 8) return false;
            status_.amp_hours = get_i32(d) / 10000.0f;
            status_.amp_hours_charged = get_i32(d + 4) / 10000.0f;
            seen_bit = 1;
            break;
        case PacketId::status_3:
            if (f.len < 8) return false;
            status_.watt_hours = get_i32(d) / 10000.0f;
            status_.watt_hours_charged = get_i32(d + 4) / 10000.0f;
            seen_bit = 2;
            break;
        case PacketId::status_4:
            if (f.len < 8) return false;
            status_.temp_fet_c = get_i16(d) / 10.0f;
            status_.temp_motor_c = get_i16(d + 2) / 10.0f;
            status_.current_in_a = get_i16(d + 4) / 10.0f;
            status_.pid_pos_deg = get_i16(d + 6) / 50.0f;
            seen_bit = 3;
            break;
        case PacketId::status_5:
            if (f.len < 6) return false;
            status_.tacho = get_i32(d);
            status_.voltage_in_v = get_i16(d + 4) / 10.0f;
            seen_bit = 4;
            break;
        default:
            return false;  // z. B. Antworten auf getunnelte Befehle, status_6
    }

    status_.seen_packets |= static_cast<uint8_t>(1u << seen_bit);
    status_.last_frame = xTaskGetTickCount();
    ++status_.frames;
    return true;
}

bool Vesc::online(TickType_t now, TickType_t max_age) const {
    return status_.frames > 0 && (now - status_.last_frame) <= max_age;
}

}  // namespace drivers
