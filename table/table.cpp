#include "table/table.hpp"

#include <cstring>

namespace table {
namespace {

// Speicher für alle Werte, ein Feld pro Eintrag mit seinem echten Typ (richtige Ausrichtung).
struct Values {
#define TABLE_ENTRY(name, type, desc) type name{};
#include "table/table_entries.def"
#undef TABLE_ENTRY
};
Values values;

// Verwaltung pro Eintrag
struct Slot {
    void* data;
    size_t size;
    TickType_t stamp;
    bool valid;
    TaskHandle_t writer;
    TaskHandle_t listeners[max_listeners];
};

Slot slots[count] = {
#define TABLE_ENTRY(name, type, desc) {&values.name, sizeof(type), 0, false, nullptr, {}},
#include "table/table_entries.def"
#undef TABLE_ENTRY
};

const Info infos[count] = {
#define TABLE_ENTRY(name, type, desc) {#name, desc, sizeof(type)},
#include "table/table_entries.def"
#undef TABLE_ENTRY
};

Slot& slot(Id id) {
    const auto i = static_cast<size_t>(id);
    configASSERT(i < count);
    return slots[i];
}

// Im kritischen Abschnitt aufrufen. Die erste schreibende Task wird Besitzer, jede andere
// hält die Firmware an: zwei Schreiber für einen Eintrag sind ein Designfehler.
void check_writer(Slot& s) {
    const TaskHandle_t self = xTaskGetCurrentTaskHandle();
    if (s.writer == nullptr) {
        s.writer = self;
    }
    configASSERT(s.writer == self);
}

void notify_listeners(Id id, const Slot& s) {
    // Kopie der Liste, damit xTaskNotify außerhalb des kritischen Abschnitts läuft.
    TaskHandle_t listeners[max_listeners];
    taskENTER_CRITICAL();
    std::memcpy(listeners, s.listeners, sizeof(listeners));
    taskEXIT_CRITICAL();
    for (TaskHandle_t t : listeners) {
        if (t != nullptr) {
            xTaskNotify(t, notify_bit(id), eSetBits);
        }
    }
}

}  // namespace

namespace detail {

void write(Id id, const void* src, size_t size, TickType_t stamp) {
    Slot& s = slot(id);
    configASSERT(size == s.size);
    taskENTER_CRITICAL();
    check_writer(s);
    std::memcpy(s.data, src, size);
    s.stamp = stamp;
    s.valid = true;
    taskEXIT_CRITICAL();
    notify_listeners(id, s);
}

void read(Id id, void* dst, size_t size, TickType_t& stamp, bool& valid) {
    Slot& s = slot(id);
    configASSERT(size == s.size);
    taskENTER_CRITICAL();
    std::memcpy(dst, s.data, size);
    stamp = s.stamp;
    valid = s.valid;
    taskEXIT_CRITICAL();
}

void invalidate(Id id) {
    Slot& s = slot(id);
    taskENTER_CRITICAL();
    check_writer(s);
    s.valid = false;
    taskEXIT_CRITICAL();
    notify_listeners(id, s);
}

}  // namespace detail

const Info& info(Id id) {
    const auto i = static_cast<size_t>(id);
    configASSERT(i < count);
    return infos[i];
}

TaskHandle_t writer(Id id) {
    Slot& s = slot(id);
    taskENTER_CRITICAL();
    const TaskHandle_t w = s.writer;
    taskEXIT_CRITICAL();
    return w;
}

bool notify_on_set(Id id, TaskHandle_t task) {
    Slot& s = slot(id);
    bool ok = false;
    taskENTER_CRITICAL();
    for (TaskHandle_t& t : s.listeners) {
        if (t == task) {
            ok = true;
            break;
        }
    }
    if (!ok) {
        for (TaskHandle_t& t : s.listeners) {
            if (t == nullptr) {
                t = task;
                ok = true;
                break;
            }
        }
    }
    taskEXIT_CRITICAL();
    return ok;
}

}  // namespace table
