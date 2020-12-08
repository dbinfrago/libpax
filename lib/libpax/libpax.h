#ifndef _LIBPAX_H
#define _LIBPAX_H

#include "libpax_api.h"

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

#define CONFIG_MAJOR_VERSION 0
#define CONFIG_MINOR_VERSION 1

/*
 Memory payload structure for persiting configurations
*/ 
struct libpax_config_storage_t {
    uint8_t major_version;
    uint8_t minor_version;
    // ensure we start 32bit aligned with the actual config
    uint8_t reserved_start[2];
    struct libpax_config_t config;
    // Added for structure alignment
    uint8_t pad;
     // reserved for future use
    uint8_t reserved_end[36];             
    uint8_t checksum[4]; 
};


enum snifftype_t { MAC_SNIFF_WIFI, MAC_SNIFF_BLE, MAC_SNIFF_BLE_ENS };
// #pragma once
void wifi_sniffer_loop(void* pvParameters);

void libpax_wifi_counter_start();
void libpax_wifi_counter_stop();
void libpax_wifi_counter_init();
int libpax_wifi_counter_count();

int libpax_ble_counter_count();

void libpax_counter_reset();

void reset_bucket();
int mac_add(uint8_t *paddr, snifftype_t sniff_type);
int add_to_bucket(uint16_t id);

extern void IRAM_ATTR libpax_wifi_counter_add_mac_IRAM(uint32_t mac_input);

void wifiDefaultConfig();
#endif
