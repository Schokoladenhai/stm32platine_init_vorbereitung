#pragma once

// Treiber für einen VESC-Motorregler am CAN-Bus.
//
// Ein Werkzeug ohne eigene Task und ohne Wissen über die Datentabelle:
// - Befehle (set_erpm, set_duty, ...) bauen einen Frame und senden ihn über can_bus.
// - Empfangene Frames gibt die besitzende Task mit handle() herein, der Treiber merkt sich den
//   letzten Status. Die Task meldet dafür filter() bei can_bus an und leert ihre Queue selbst.
//
// Der VESC stoppt den Motor von selbst, wenn länger keine Befehle kommen (VESC-Tool:
// App Settings > General > Timeout, Standard 1000 ms). Befehle müssen also zyklisch kommen.
//
// Nicht threadsafe: Eine Instanz gehört genau einer Task.

#include <cstdint>

#include "FreeRTOS.h"
#include "bus/can_bus.hpp"
#include "drivers/vesc/vesc_protocol.hpp"

namespace drivers {

class Vesc {
public:
    struct Status {
        // status_1
        int32_t erpm = 0;         // elektrische U/min (mechanisch = ERPM / Polpaarzahl)
        float current_a = 0;      // Motorstrom
        float duty = 0;           // -1..1
        // status_2 / status_3
        float amp_hours = 0;
        float amp_hours_charged = 0;
        float watt_hours = 0;
        float watt_hours_charged = 0;
        // status_4
        float temp_fet_c = 0;
        float temp_motor_c = 0;
        float current_in_a = 0;   // Strom aus dem Akku
        float pid_pos_deg = 0;
        // status_5
        int32_t tacho = 0;
        float voltage_in_v = 0;   // Akkuspannung

        uint32_t frames = 0;           // ausgewertete Frames insgesamt
        TickType_t last_frame = 0;     // Tick des letzten Frames
        uint8_t seen_packets = 0;      // Bit n gesetzt: status_(n+1) schon empfangen
    };

    explicit Vesc(uint8_t vesc_id) : id_(vesc_id) {}

    uint8_t id() const { return id_; }

    // Filter für can_bus::subscribe: alle 29-Bit-Frames dieses VESC.
    can_bus::Filter filter() const { return {id_, 0xFF, true}; }

    // Befehle. false, wenn der Frame nicht gesendet werden konnte.
    bool set_erpm(int32_t erpm);
    bool set_duty(float duty);                 // -1..1, wird begrenzt
    bool set_current(float amps);              // Motorstrom, Vorzeichen = Richtung
    bool set_brake_current(float amps);        // Bremsstrom, immer positiv

    // Servo-Ausgang des VESC (Lenkung). position 0..1 wird begrenzt; der VESC macht daraus
    // standardmäßig 1000..2000 µs Pulsbreite. Der VESC-Servoausgang muss im VESC-Tool
    // freigegeben sein (App Settings > General > Enable Servo Output).
    bool set_servo(float position);
    bool set_servo_pulse_us(float pulse_us) { return set_servo((pulse_us - 1000.0f) / 1000.0f); }

    // Wertet einen empfangenen Frame aus. false, wenn er nicht von diesem VESC stammt
    // oder der Pakettyp unbekannt ist.
    bool handle(const can_bus::Frame& frame);

    const Status& status() const { return status_; }

    // true, wenn innerhalb von max_age ein Frame von diesem VESC kam.
    bool online(TickType_t now, TickType_t max_age) const;

private:
    bool send(vesc::PacketId packet, int32_t value);

    uint8_t id_;
    Status status_;
};

}  // namespace drivers
