#ifndef _GLOBALS_H
#define _GLOBALS_H

#ifdef LIBPAX_ESPIDF // ESPIDF
#include "esp_log.h"
#include <freertos/FreeRTOS.h>
#include <freertos/timers.h>
#include <freertos/timers.h>
#else // Arduino IDE
#include <Arduino.h>
#endif

extern uint16_t macs_wifi;
extern uint16_t macs_ble;
extern uint8_t channel;  // wifi channel rotation counter
extern TimerHandle_t WifiChanTimer;

#endif
