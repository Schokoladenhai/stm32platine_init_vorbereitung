#pragma once

// Treiber für einen Modellbau-Servo an einem PWM-Kanal der Platine (TIM3 CH1/CH2).
//
// Ein Werkzeug ohne eigene Task: set_pulse_us() schreibt nur das Compare-Register, der Timer
// erzeugt das Signal selbst. Gehört einer Task (später task_drivetrain).
//
// Der Timer muss mit 1 MHz zählen (TIM3 in CubeMX: Prescaler 83 bei 84 MHz Timertakt).
// Die Periode setzt start() selbst (50 Hz = 20000 µs), weil sie in der .ioc noch auf 65535 steht.
//
// Versorgung: Der Lenkservo zieht Spitzen bis 3,6 A und hängt an einem eigenen UBEC.
// Von der Platine kommen nur Signal und Masse.

#include <cstdint>

#include "tim.h"

namespace drivers {

class Servo {
public:
    struct Config {
        TIM_HandleTypeDef* htim;
        uint32_t channel;        // TIM_CHANNEL_1 oder TIM_CHANNEL_2
        uint16_t min_us;         // Anschlag, wird nie unterschritten
        uint16_t max_us;         // Anschlag, wird nie überschritten
        uint16_t center_us;      // Geradeaus, wird bei start() gesetzt
        uint16_t frequency_hz;   // 50 für Standard-Servos, digitale oft bis 333
    };

    explicit Servo(const Config& config) : config_(config) {}

    // Setzt Periode, Mittelstellung und startet das PWM-Signal. false bei falschem Timertakt.
    bool start();

    // Pulsbreite in µs, wird auf min_us..max_us begrenzt. Wirkt ab der nächsten PWM-Periode.
    void set_pulse_us(uint16_t pulse_us);

    // Signal abschalten (Servo hält nicht mehr, lässt sich von Hand drehen).
    void stop();

    bool running() const { return running_; }
    uint16_t pulse_us() const { return pulse_us_; }
    const Config& config() const { return config_; }

private:
    Config config_;
    bool running_ = false;
    uint16_t pulse_us_ = 0;
};

}  // namespace drivers
