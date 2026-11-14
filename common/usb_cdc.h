#pragma once

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Wird aus dem USB-Interrupt in cubemx/USB_DEVICE/App/usbd_cdc_if.c aufgerufen
 * (eine Zeile im User-Code von CDC_Receive_FS bzw. CDC_Receive_HS).
 * port: 0 = USB FS, 1 = USB HS.
 */
void usb_cdc_rx_isr(int port, const uint8_t* data, uint32_t len);

#ifdef __cplusplus
}

#include "FreeRTOS.h"

/**
 * Virtuelle serielle Schnittstelle über USB (CDC) für Tasks.
 * Empfang: Der Interrupt legt Bytes in einen Stream-Buffer, genau eine Task liest.
 * Senden: Antworten gehen an den Port, von dem zuletzt Daten kamen.
 */
namespace usb_cdc {

// Vor dem Start des USB-Stacks aufrufen.
void init();

// Liest bis zu len Bytes. Gibt die Anzahl zurück (0 bei Timeout).
size_t read(uint8_t* data, size_t len, TickType_t timeout);

// Sendet len Bytes. false, wenn USB nicht verbunden ist oder der Timeout abläuft.
bool write(const void* data, size_t len, TickType_t timeout = pdMS_TO_TICKS(50));

}  // namespace usb_cdc
#endif
