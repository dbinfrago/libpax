#ifndef _GLOBALS_H
#define _GLOBALS_H

#ifdef LIBPAX_ESPIDF
#include "esp_log.h"
/*espidf*/
#include "freertos/FreeRTOS.h"
#include "freertos/timers.h"

#include <freertos/timers.h>

#else
#include <Arduino.h>
#endif

extern uint16_t volatile macs_wifi;
extern uint16_t volatile macs_ble;

extern TimerHandle_t WifiChanTimer;
extern uint8_t volatile channel;  // wifi channel rotation counter

#endif
