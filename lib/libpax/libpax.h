#ifndef _LIBPAX_H
#define _LIBPAX_H

// #define LIBPAX_ESPIDF 1

#ifdef LIBPAX_ESPIDF
// #include "esp_partition.h"
#else
#include <Arduino.h>
#endif

// #include <iostream>

// #include <string.h>
// #include "freertos/FreeRTOS.h"
#include "globals.h"
// #include "esp_int_wdt.h"
// #include "esp_task_wdt.h"
#include "wifiscan.h"

// #define PARTITION_APP_TEST 1
//#define PARTITION_APP_CP 1
// #define PARTITION_API_TEST 1
// #define SPI_MAIN 1
// #define WATCHDOG_TEST 1


#define MAC_SNIFF_WIFI 0
#define MAC_SNIFF_BLE 1

// #pragma once
void wifi_sniffer_loop(void* pvParameters);

void libpax_wifi_counter_start();
void libpax_wifi_counter_stop();
void libpax_wifi_counter_init();
int libpax_wifi_counter_count();

int libpax_ble_counter_count();

void libpax_counter_reset();

void reset_bucket();
int mac_add(uint8_t *paddr, bool sniff_type);
int add_to_bucket(uint16_t id);

extern void IRAM_ATTR libpax_wifi_counter_add_mac_IRAM(uint32_t mac_input);

void wifiDefaultConfig();
#endif
