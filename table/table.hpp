#pragma once

// Datentabelle: die einzige Verbindung zwischen den Tasks.
//
// Sensor-Tasks schreiben Messwerte hinein, Aktor-Tasks lesen Stellwerte heraus, die
// Kommunikations-Task verbindet die Tabelle mit dem Zentral-PC. Kein Task ruft einen anderen
// direkt auf.
//
//   // Schreiber (genau eine Task pro Eintrag)
//   table::set<table::Id::drive_cmd>({2000.0f, -0.2f});  // erpm, steering
//
//   // Leser (beliebig viele Tasks)
//   const auto e = table::get<table::Id::drive_cmd>();   // Kopie, kein Zeiger in die Tabelle
//   if (e.fresh(100)) { ... e.value.erpm ... }   // gültig und höchstens 100 ms alt
//
// Die Einträge stehen in table_entries.def, ihre Typen in table_types.hpp.
//
// Regeln:
// - Ein Schreiber pro Eintrag. Die erste Task, die einen Eintrag schreibt, wird sein Besitzer;
//   schreibt danach eine andere Task hinein, hält die Firmware an (configASSERT).
// - Nur aus Tasks aufrufen, nie aus Interrupts. ISRs geben ihre Daten per Queue oder
//   Notification an ihre Task, die schreibt dann (FreeRTOS prüft das im kritischen Abschnitt).
// - Lesen und Schreiben kopieren den ganzen Eintrag in einem kurzen kritischen Abschnitt
//   (Interrupts kurz gesperrt). Bei höchstens 64 Byte dauert das unter 1 µs. Das ist billiger
//   als ein Mutex und kann nicht verklemmen. Ein Leser sieht deshalb nie einen halb
//   geschriebenen Wert.
//
// Aufwecken: Mit notify_on_set() meldet sich eine Task für einen Eintrag an. Jedes set() setzt
// dann in ihrer Task-Notification das Bit notify_bit(id). Die Task wartet z. B. mit
//   xTaskNotifyWait(0, ~0u, &bits, timeout)
// und reagiert sofort auf neue Werte statt zyklisch nachzusehen.

#include <cstddef>
#include <cstdint>
#include <type_traits>

#include "FreeRTOS.h"
#include "table/table_types.hpp"
#include "task.h"

namespace table {

// Größte erlaubte Eintragsgröße in Byte (begrenzt die Zeit mit gesperrten Interrupts).
inline constexpr size_t max_entry_size = 64;
// So viele Tasks können sich pro Eintrag mit notify_on_set() anmelden.
inline constexpr size_t max_listeners = 3;

enum class Id : uint8_t {
#define TABLE_ENTRY(name, type, desc) name,
#include "table/table_entries.def"
#undef TABLE_ENTRY
};

inline constexpr size_t count = 0
#define TABLE_ENTRY(name, type, desc) +1
#include "table/table_entries.def"
#undef TABLE_ENTRY
    ;
static_assert(count <= 32, "höchstens 32 Einträge (ein Notification-Bit pro Eintrag)");

// Typ eines Eintrags: TypeOf<Id::drive_cmd>::type == DriveCmd
template <Id id>
struct TypeOf;
#define TABLE_ENTRY(name, entry_type, desc)                                                  \
    template <>                                                                              \
    struct TypeOf<Id::name> {                                                                \
        using type = entry_type;                                                             \
    };                                                                                       \
    static_assert(std::is_trivially_copyable_v<entry_type>,                                  \
                  #entry_type ": nur einfache Werte (kopierbar per memcpy)");                \
    static_assert(sizeof(entry_type) <= max_entry_size, #entry_type ": Eintrag zu groß");
#include "table/table_entries.def"
#undef TABLE_ENTRY

// Kopie eines Eintrags mit Zeitstempel und Gültig-Flag.
template <typename T>
struct Entry {
    T value{};
    TickType_t stamp = 0;  // Tick beim Schreiben (bzw. der übergebenen Messzeit)
    bool valid = false;    // false: noch nie geschrieben oder vom Schreiber für ungültig erklärt

    // Alter des Werts in ms. Bei ungültigen Einträgen bedeutungslos, vorher valid prüfen.
    uint32_t age_ms(TickType_t now = xTaskGetTickCount()) const {
        return static_cast<uint32_t>((now - stamp) * portTICK_PERIOD_MS);
    }
    // Gültig und nicht älter als max_age_ms.
    bool fresh(uint32_t max_age_ms, TickType_t now = xTaskGetTickCount()) const {
        return valid && age_ms(now) <= max_age_ms;
    }
};

// Bit, das set() in der Task-Notification der angemeldeten Tasks setzt.
constexpr uint32_t notify_bit(Id id) { return 1u << static_cast<uint8_t>(id); }

// Beschreibung eines Eintrags (für Test-Shell und Diagnose).
struct Info {
    const char* name;
    const char* description;
    size_t size;
};
const Info& info(Id id);

// Besitzer (Schreiber) eines Eintrags, nullptr solange noch niemand geschrieben hat.
TaskHandle_t writer(Id id);

// Meldet task für den Eintrag an: jedes set() setzt dann notify_bit(id) in ihrer Notification.
// false, wenn schon max_listeners Tasks angemeldet sind. Doppelte Anmeldung ist harmlos.
bool notify_on_set(Id id, TaskHandle_t task = xTaskGetCurrentTaskHandle());

namespace detail {
void write(Id id, const void* src, size_t size, TickType_t stamp);
void read(Id id, void* dst, size_t size, TickType_t& stamp, bool& valid);
void invalidate(Id id);
}  // namespace detail

// Schreibt einen Wert, Zeitstempel = jetzt.
template <Id id>
void set(const typename TypeOf<id>::type& value) {
    detail::write(id, &value, sizeof(value), xTaskGetTickCount());
}

// Schreibt einen Wert mit eigenem Zeitstempel, z. B. dem Zeitpunkt der Messung statt dem
// Zeitpunkt des Eintragens.
template <Id id>
void set(const typename TypeOf<id>::type& value, TickType_t stamp) {
    detail::write(id, &value, sizeof(value), stamp);
}

// Erklärt den Eintrag für ungültig (z. B. Sensor ausgefallen). Nur der Schreiber darf das.
template <Id id>
void invalidate() {
    detail::invalidate(id);
}

// Liest eine Kopie des Eintrags.
template <Id id>
Entry<typename TypeOf<id>::type> get() {
    Entry<typename TypeOf<id>::type> e;
    detail::read(id, &e.value, sizeof(e.value), e.stamp, e.valid);
    return e;
}

}  // namespace table
