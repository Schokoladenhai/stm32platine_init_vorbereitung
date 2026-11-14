#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Einstieg der Anwendung. CubeMX legt die Start-Task "app" an und ruft diese
 * Funktion auf (im .ioc als "As weak" eingestellt, unsere Definition ersetzt
 * den Platzhalter in main.c).
 */
void app_main(void* argument);

#ifdef __cplusplus
}
#endif
