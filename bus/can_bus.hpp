#pragma once

// Threadsafe Zugriff auf CAN1 für alle Tasks.
//
// Nur dieses Modul fasst hcan1 an. Treiber und Tasks rufen nie HAL_CAN_* direkt auf.
//
// Senden:    can_bus::send() aus beliebig vielen Tasks gleichzeitig. Ist keine der drei
//            Hardware-Mailboxen frei, wartet send() höchstens `timeout` darauf.
// Empfangen: Ein Task meldet mit can_bus::subscribe() einen ID-Filter und seine Queue an.
//            Die CAN-ISR liest jeden Frame aus der Hardware-FIFO und kopiert ihn in die Queue
//            jedes passenden Abonnenten. Der Task holt die Frames dort ab und wertet sie aus.
//
// Grundlage ist thread_safe_can.c aus LARS (CAuDri/LARS-Hardware-Interface, 80d4b5e),
// vereinfacht auf CAN1 und ohne Dispatcher-Task.

#include <cstddef>
#include <cstdint>

#include "FreeRTOS.h"
#include "queue.h"

namespace can_bus {

// Ein CAN-Frame, so wie er in den Queues der Abonnenten landet.
struct Frame {
    uint32_t id = 0;         // 11 Bit (Standard) oder 29 Bit (Extended)
    bool extended = false;   // true: 29-Bit-ID
    bool remote = false;     // true: Remote-Frame (ohne Daten)
    uint8_t len = 0;         // 0..8
    uint8_t data[8] = {};
};

// Empfangsfilter: Ein Frame passt, wenn (frame.id & mask) == (id & mask)
// und frame.extended == extended. mask = 0 nimmt alle IDs dieser Art.
struct Filter {
    uint32_t id = 0;
    uint32_t mask = 0;
    bool extended = false;
};

// Maximale Zahl der Anmeldungen. CAN1 hat 14 Hardware-Filterbänke, eine pro Anmeldung.
inline constexpr size_t max_subscriptions = 14;

// Startet CAN1 (nach MX_CAN1_Init, aus einer Task, einmal beim Start).
bool init();

// Sendet einen Frame. Wartet höchstens `timeout`, bis eine Mailbox frei ist.
// false: nicht initialisiert, ungültiger Frame oder Timeout. Bei Timeout hängen alle drei
// Mailboxen (z. B. niemand am Bus quittiert), dann werden sie verworfen, damit später
// keine veralteten Befehle mehr rausgehen.
// Nicht aus einer ISR aufrufen.
bool send(const Frame& frame, TickType_t timeout = pdMS_TO_TICKS(5));

// Meldet einen Empfangsfilter an. Passende Frames kopiert die ISR in `queue`
// (Elementgröße sizeof(Frame)). Ist die Queue voll, wird der Frame verworfen und gezählt.
// Eine Queue darf mehrere Filter haben. Anmeldungen bleiben bis zum Reset bestehen.
// Nicht aus einer ISR aufrufen.
bool subscribe(const Filter& filter, QueueHandle_t queue);

// Zähler für Diagnose (Test-Shell, später Statuswerte in der Tabelle).
struct Stats {
    uint32_t tx_frames;      // erfolgreich gesendet (Mailbox meldet TXOK)
    uint32_t tx_timeouts;    // send() ohne freie Mailbox abgebrochen
    uint32_t tx_failed;      // Mailbox mit Fehler oder Arbitrierungsverlust beendet bzw. verworfen
    uint32_t rx_frames;      // aus der Hardware-FIFO gelesen
    uint32_t rx_unmatched;   // kein Abonnent passte (sollte dank Hardware-Filter 0 sein)
    uint32_t rx_dropped;     // Queue eines Abonnenten war voll
    uint32_t rx_overruns;    // Hardware-FIFO lief über (ISR kam nicht nach)
    uint32_t bus_errors;     // Fehler-Interrupts insgesamt
    uint32_t bus_off;        // Wechsel in Bus-Off
    uint32_t last_error;     // letzter HAL-Fehlercode (HAL_CAN_ERROR_*)
    uint8_t tx_error_count;  // TEC aus dem Fehlerregister
    uint8_t rx_error_count;  // REC aus dem Fehlerregister
    bool error_passive;
    bool is_bus_off;
    size_t subscriptions;
};
Stats stats();

}  // namespace can_bus
