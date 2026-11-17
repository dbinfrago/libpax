#ifndef _GLOBALS_H
#define _GLOBALS_H

#include <atomic>

#ifdef LIBPAX_ESPIDF // ESPIDF
#include "esp_log.h"
#include <freertos/FreeRTOS.h>
#include <freertos/timers.h>
#else // Arduino IDE
#include <Arduino.h>
#endif

// volatile: written from the WiFi/BLE RX context, read from the report
// timer task, so the compiler must not cache a stale value across calls
extern std::atomic<uint16_t> macs_wifi;
extern std::atomic<uint16_t> macs_ble;
extern volatile uint8_t channel;  // wifi channel rotation counter, written by the timer task
extern TimerHandle_t WifiChanTimer;

#endif
