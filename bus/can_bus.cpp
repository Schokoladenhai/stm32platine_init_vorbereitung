#include "bus/can_bus.hpp"

#include "can.h"
#include "semphr.h"
#include "task.h"

// Die Callbacks werden zur Laufzeit registriert (HAL_CAN_RegisterCallback).
// In CubeMX: Project Manager > Advanced Settings > Register Callback > CAN = ENABLE.
#if (USE_HAL_CAN_REGISTER_CALLBACKS != 1)
#error "can_bus braucht USE_HAL_CAN_REGISTER_CALLBACKS = 1 (CubeMX: Register Callback CAN)"
#endif

namespace can_bus {
namespace {

// Die bxCAN-Filterbänke 0..13 gehören zu CAN1, 14..27 zu CAN2 (auf der Platine nicht benutzt).
constexpr uint32_t can2_start_bank = 14;

struct Subscription {
    Filter filter;
    QueueHandle_t queue;
};

CAN_HandleTypeDef& hcan = hcan1;
bool initialized = false;

// Anmeldungen. Die ISR liest nur die ersten `subscription_count` Einträge. Ein neuer Eintrag
// wird erst vollständig geschrieben und danach gezählt, deshalb braucht die ISR keine Sperre.
Subscription subscriptions[max_subscriptions];
volatile size_t subscription_count = 0;

// Schützt subscribe() gegen gleichzeitige Aufrufe (Vergabe der Filterbänke).
StaticSemaphore_t config_mutex_buffer;
SemaphoreHandle_t config_mutex = nullptr;

// Weckt wartende Sender, sobald eine Mailbox frei wird. Der Wert ist nur ein Hinweis:
// send() prüft nach dem Aufwachen selbst, ob wirklich eine Mailbox frei ist.
StaticSemaphore_t tx_space_buffer;
SemaphoreHandle_t tx_space = nullptr;

// Zähler. Werden nur in der ISR bzw. in Critical Sections geändert.
Stats counters = {};

bool matches(const Filter& f, const Frame& frame) {
    return f.extended == frame.extended && (frame.id & f.mask) == (f.id & f.mask);
}

// ---- Interrupt-Seite ---------------------------------------------------------

void read_fifo(CAN_HandleTypeDef* h, uint32_t fifo) {
    BaseType_t woken = pdFALSE;
    CAN_RxHeaderTypeDef header;
    Frame frame;

    while (HAL_CAN_GetRxFifoFillLevel(h, fifo) > 0) {
        if (HAL_CAN_GetRxMessage(h, fifo, &header, frame.data) != HAL_OK) {
            break;
        }
        frame.extended = header.IDE == CAN_ID_EXT;
        frame.id = frame.extended ? header.ExtId : header.StdId;
        frame.remote = header.RTR == CAN_RTR_REMOTE;
        frame.len = static_cast<uint8_t>(header.DLC > 8 ? 8 : header.DLC);
        ++counters.rx_frames;

        bool delivered = false;
        const size_t n = subscription_count;
        for (size_t i = 0; i < n; ++i) {
            if (!matches(subscriptions[i].filter, frame)) {
                continue;
            }
            delivered = true;
            if (xQueueSendFromISR(subscriptions[i].queue, &frame, &woken) != pdTRUE) {
                ++counters.rx_dropped;
            }
        }
        if (!delivered) {
            ++counters.rx_unmatched;
        }
    }
    portYIELD_FROM_ISR(woken);
}

void on_fifo0(CAN_HandleTypeDef* h) { read_fifo(h, CAN_RX_FIFO0); }
void on_fifo1(CAN_HandleTypeDef* h) { read_fifo(h, CAN_RX_FIFO1); }

void signal_tx_space() {
    BaseType_t woken = pdFALSE;
    xSemaphoreGiveFromISR(tx_space, &woken);  // schon "voll" ist egal, es ist nur ein Hinweis
    portYIELD_FROM_ISR(woken);
}

void on_tx_complete(CAN_HandleTypeDef*) {
    ++counters.tx_frames;
    signal_tx_space();
}

void on_tx_abort(CAN_HandleTypeDef*) {
    ++counters.tx_failed;
    signal_tx_space();
}

void on_error(CAN_HandleTypeDef* h) {
    const uint32_t code = HAL_CAN_GetError(h);
    ++counters.bus_errors;
    counters.last_error = code;

    // Eine Mailbox, die mit Fehler oder verlorener Arbitrierung endet, meldet die HAL nur hier.
    constexpr uint32_t tx_errors = HAL_CAN_ERROR_TX_ALST0 | HAL_CAN_ERROR_TX_TERR0 |
                                   HAL_CAN_ERROR_TX_ALST1 | HAL_CAN_ERROR_TX_TERR1 |
                                   HAL_CAN_ERROR_TX_ALST2 | HAL_CAN_ERROR_TX_TERR2;
    if (code & tx_errors) {
        ++counters.tx_failed;
        signal_tx_space();
    }
    if (code & (HAL_CAN_ERROR_RX_FOV0 | HAL_CAN_ERROR_RX_FOV1)) {
        ++counters.rx_overruns;
    }
    if (code & HAL_CAN_ERROR_BOF) {
        ++counters.bus_off;
    }
    // Die HAL sammelt Fehlerbits sonst auf, dann würden sie beim nächsten Mal doppelt zählen.
    HAL_CAN_ResetError(h);
}

// ---- Hilfen für Filter -------------------------------------------------------

// Bringt ID bzw. Maske in das Format der 32-Bit-Filterregister (STID/EXID, IDE, RTR).
uint32_t filter_register(uint32_t id, bool extended) {
    if (extended) {
        return ((id & 0x1FFFFFFFu) << 3) | CAN_ID_EXT;  // CAN_ID_EXT = IDE-Bit (Bit 2)
    }
    return (id & 0x7FFu) << 21;
}

}  // namespace

bool init() {
    if (initialized) {
        return true;
    }
    if (hcan.Instance != CAN1 || hcan.State != HAL_CAN_STATE_READY) {
        return false;  // MX_CAN1_Init fehlt oder CAN läuft schon
    }

    config_mutex = xSemaphoreCreateMutexStatic(&config_mutex_buffer);
    tx_space = xSemaphoreCreateCountingStatic(3, 0, &tx_space_buffer);

    // Nach Bus-Off selbst wieder anmelden (in CubeMX steht AutoBusOff auf DISABLE, dann bliebe
    // der Bus bis zum Reset tot). Das Bit lässt sich nur im Init-Modus setzen, in dem die HAL
    // den CAN nach HAL_CAN_Init bis HAL_CAN_Start lässt.
    SET_BIT(hcan.Instance->MCR, CAN_MCR_ABOM);

    bool ok = HAL_CAN_RegisterCallback(&hcan, HAL_CAN_RX_FIFO0_MSG_PENDING_CB_ID, on_fifo0) == HAL_OK &&
              HAL_CAN_RegisterCallback(&hcan, HAL_CAN_RX_FIFO1_MSG_PENDING_CB_ID, on_fifo1) == HAL_OK &&
              HAL_CAN_RegisterCallback(&hcan, HAL_CAN_TX_MAILBOX0_COMPLETE_CB_ID, on_tx_complete) == HAL_OK &&
              HAL_CAN_RegisterCallback(&hcan, HAL_CAN_TX_MAILBOX1_COMPLETE_CB_ID, on_tx_complete) == HAL_OK &&
              HAL_CAN_RegisterCallback(&hcan, HAL_CAN_TX_MAILBOX2_COMPLETE_CB_ID, on_tx_complete) == HAL_OK &&
              HAL_CAN_RegisterCallback(&hcan, HAL_CAN_TX_MAILBOX0_ABORT_CB_ID, on_tx_abort) == HAL_OK &&
              HAL_CAN_RegisterCallback(&hcan, HAL_CAN_TX_MAILBOX1_ABORT_CB_ID, on_tx_abort) == HAL_OK &&
              HAL_CAN_RegisterCallback(&hcan, HAL_CAN_TX_MAILBOX2_ABORT_CB_ID, on_tx_abort) == HAL_OK &&
              HAL_CAN_RegisterCallback(&hcan, HAL_CAN_ERROR_CB_ID, on_error) == HAL_OK;
    if (!ok) {
        return false;
    }

    // Ohne aktive Filterbank empfängt der bxCAN nichts. Das ist gewollt: Erst subscribe()
    // schaltet passende IDs frei.
    if (HAL_CAN_Start(&hcan) != HAL_OK) {
        return false;
    }
    constexpr uint32_t notifications = CAN_IT_RX_FIFO0_MSG_PENDING | CAN_IT_RX_FIFO1_MSG_PENDING |
                                       CAN_IT_RX_FIFO0_OVERRUN | CAN_IT_RX_FIFO1_OVERRUN |
                                       CAN_IT_TX_MAILBOX_EMPTY | CAN_IT_ERROR_WARNING |
                                       CAN_IT_ERROR_PASSIVE | CAN_IT_BUSOFF | CAN_IT_ERROR;
    // Bewusst ohne CAN_IT_LAST_ERROR_CODE: Ohne Gegenstelle am Bus gäbe das einen Interrupt
    // pro Sendeversuch.
    if (HAL_CAN_ActivateNotification(&hcan, notifications) != HAL_OK) {
        return false;
    }
    initialized = true;
    return true;
}

bool send(const Frame& frame, TickType_t timeout) {
    if (!initialized || frame.len > 8 || (!frame.extended && frame.id > 0x7FFu) ||
        frame.id > 0x1FFFFFFFu) {
        return false;
    }

    CAN_TxHeaderTypeDef header = {};
    header.IDE = frame.extended ? CAN_ID_EXT : CAN_ID_STD;
    header.ExtId = frame.extended ? frame.id : 0;
    header.StdId = frame.extended ? 0 : frame.id;
    header.RTR = frame.remote ? CAN_RTR_REMOTE : CAN_RTR_DATA;
    header.DLC = frame.len;
    header.TransmitGlobalTime = DISABLE;

    const TickType_t start = xTaskGetTickCount();
    for (;;) {
        // Freie Mailbox suchen und belegen darf nicht von einem anderen Sender unterbrochen
        // werden, sonst wählen beide dieselbe. Dauert nur wenige Registerzugriffe.
        bool queued = false;
        taskENTER_CRITICAL();
        if (HAL_CAN_GetTxMailboxesFreeLevel(&hcan) > 0) {
            uint32_t mailbox;
            queued = HAL_CAN_AddTxMessage(&hcan, &header, frame.data, &mailbox) == HAL_OK;
        }
        taskEXIT_CRITICAL();
        if (queued) {
            return true;
        }

        const TickType_t waited = xTaskGetTickCount() - start;
        if (waited >= timeout || xSemaphoreTake(tx_space, timeout - waited) != pdTRUE) {
            break;
        }
    }

    // Alle Mailboxen hängen. Verwerfen, damit keine veralteten Frames mehr rausgehen.
    // Die Abort-Callbacks wecken dann ggf. andere wartende Sender.
    // Nur wenn immer noch alle belegt sind, sonst hat gerade ein anderer Sender Platz bekommen.
    taskENTER_CRITICAL();
    ++counters.tx_timeouts;
    if (HAL_CAN_GetTxMailboxesFreeLevel(&hcan) == 0) {
        HAL_CAN_AbortTxRequest(&hcan, CAN_TX_MAILBOX0 | CAN_TX_MAILBOX1 | CAN_TX_MAILBOX2);
    }
    taskEXIT_CRITICAL();
    return false;
}

bool subscribe(const Filter& filter, QueueHandle_t queue) {
    if (!initialized || queue == nullptr) {
        return false;
    }
    xSemaphoreTake(config_mutex, portMAX_DELAY);

    const size_t bank = subscription_count;
    if (bank >= max_subscriptions) {
        xSemaphoreGive(config_mutex);
        return false;
    }

    const uint32_t id = filter_register(filter.id, filter.extended);
    // IDE-Bit immer vergleichen (Standard und Extended nicht mischen), RTR nicht.
    const uint32_t mask = filter_register(filter.mask, filter.extended) | CAN_ID_EXT;

    CAN_FilterTypeDef config = {};
    config.FilterBank = bank;
    config.FilterMode = CAN_FILTERMODE_IDMASK;
    config.FilterScale = CAN_FILTERSCALE_32BIT;
    config.FilterIdHigh = id >> 16;
    config.FilterIdLow = id & 0xFFFFu;
    config.FilterMaskIdHigh = mask >> 16;
    config.FilterMaskIdLow = mask & 0xFFFFu;
    config.FilterFIFOAssignment = CAN_FILTER_FIFO0;
    config.FilterActivation = CAN_FILTER_ENABLE;
    config.SlaveStartFilterBank = can2_start_bank;

    // Erst den Abonnenten eintragen, dann den Hardware-Filter öffnen. Die Filterbank lässt
    // sich im laufenden Betrieb setzen, der Empfang pausiert dabei nur kurz.
    subscriptions[bank] = Subscription{filter, queue};
    taskENTER_CRITICAL();
    subscription_count = bank + 1;
    taskEXIT_CRITICAL();

    const bool ok = HAL_CAN_ConfigFilter(&hcan, &config) == HAL_OK;
    if (!ok) {
        taskENTER_CRITICAL();
        subscription_count = bank;
        taskEXIT_CRITICAL();
    }
    xSemaphoreGive(config_mutex);
    return ok;
}

Stats stats() {
    taskENTER_CRITICAL();
    Stats s = counters;
    s.subscriptions = subscription_count;
    taskEXIT_CRITICAL();

    const uint32_t esr = hcan.Instance ? hcan.Instance->ESR : 0;
    s.tx_error_count = static_cast<uint8_t>((esr & CAN_ESR_TEC) >> CAN_ESR_TEC_Pos);
    s.rx_error_count = static_cast<uint8_t>((esr & CAN_ESR_REC) >> CAN_ESR_REC_Pos);
    s.error_passive = (esr & CAN_ESR_EPVF) != 0;
    s.is_bus_off = (esr & CAN_ESR_BOFF) != 0;
    return s;
}

}  // namespace can_bus
