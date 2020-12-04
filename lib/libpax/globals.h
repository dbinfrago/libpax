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

#include "paxcounter.conf"

// Struct holding devices's runtime configuration
typedef struct {
  // uint8_t loradr;      // 0-15, lora datarate
  // uint8_t txpower;     // 2-15, lora tx power
  // uint8_t adrmode;     // 0=disabled, 1=enabled
  // uint8_t screensaver; // 0=disabled, 1=enabled
  // uint8_t screenon;    // 0=disabled, 1=enabled
  uint8_t
      countermode;    // 0=cyclic unconfirmed, 1=cumulative, 2=cyclic confirmed
  int16_t rssilimit;  // threshold for rssilimiter, negative value!
  // uint8_t sendcycle;   // payload send cycle [seconds/2]
  uint8_t wifichancycle;  // wifi channel switch cycle [seconds/100]
  uint8_t blescantime;    // BLE scan cycle duration [seconds]
  uint8_t blescan;        // 0=disabled, 1=enabled
  uint8_t wifiscan;       // 0=disabled, 1=enabled
  uint8_t wifiant;        // 0=internal, 1=external (for LoPy/LoPy4)
  // uint8_t vendorfilter;  // 0=disabled, 1=enabled
  // uint8_t rgblum;        // RGB Led luminosity (0..100%)
  // uint8_t monitormode;   // 0=disabled, 1=enabled
  // uint8_t runmode;       // 0=normal, 1=update
  // uint8_t payloadmask;   // bitswitches for payload data
  char version[10];  // Firmware version
  uint8_t wifi_channel_min;
  uint8_t wifi_channel_max;
  uint8_t wifi_channel_switch_interval;
  char wifi_my_country;
} configData_t;
extern configData_t cfg_pax;  // current device configuration
extern TimerHandle_t WifiChanTimer;
extern uint8_t volatile channel;  // wifi channel rotation counter

#endif
