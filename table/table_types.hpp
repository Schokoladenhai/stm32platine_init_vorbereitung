#pragma once

// Datentypen der Tabelleneinträge. Ein Typ pro Eintrag in table_entries.def.
//
// Regeln (werden in table.hpp per static_assert geprüft):
// - Nur einfache Werte (trivially copyable): keine Zeiger, keine Klassen mit Konstruktor-Logik.
// - Höchstens table::max_entry_size Byte, damit das Kopieren nur kurz die Interrupts sperrt.
// - Einheiten im Feldnamen (_a, _v, _c ...), Gleitkomma als float (der M4 hat nur eine
//   Single-Precision-FPU, double wäre langsam).

#include <cstdint>

namespace table {

// Fahrbefehl (Soll). Schreiber: später die Kommunikations-Task bzw. die RC-Arbitrierung.
struct DriveCmd {
    // Motordrehzahl in elektrischen U/min (ERPM), Vorzeichen = Richtung.
    // Vorläufig, bis Polpaarzahl, Übersetzung und Raddurchmesser feststehen; dann m/s.
    float erpm = 0;
    // Lenkung normiert: -1 = voll links, 0 = Mitte, +1 = voll rechts.
    // task_drivetrain rechnet das mit Mitte und Grenzen auf den Servowert um.
    float steering = 0;
};

// Zustand des Antriebs (Ist). Schreiber: task_drivetrain.
struct DriveState {
    // VESC-Telemetrie (aus den Status-Frames)
    int32_t erpm = 0;
    float current_a = 0;      // Motorstrom
    float duty = 0;           // -1..1
    float voltage_in_v = 0;   // Akkuspannung
    float current_in_a = 0;   // Strom aus dem Akku
    float temp_fet_c = 0;
    float temp_motor_c = 0;
    int32_t tacho = 0;
    // Zuletzt an den VESC-Servoausgang gesendete Lenkung (-1..1), es gibt keine Rückmeldung.
    float steering = 0;
    // true, wenn kürzlich Status-Frames vom VESC kamen
    bool vesc_online = false;
};

}  // namespace table
