#ifndef _LIBPAX_H
#define _LIBPAX_H

#include "libpax_api.h"
#include <string.h>
#include "globals.h"
#include "wifiscan.h"

#define CONFIG_MAJOR_VERSION 1
#define CONFIG_MINOR_VERSION 1

// Memory payload structure for persiting configurations
struct libpax_config_storage_t {
    uint8_t major_version;
    uint8_t minor_version;
    // ensure we start 32bit aligned with the actual config
    uint8_t reserved_start[2];
    struct libpax_config_t config;
    // Added for structure alignment
    uint8_t pad[2];
    // reserved for future use
    uint8_t reserved_end[21];             
    uint8_t checksum[4]; 
};

typedef enum { MAC_SNIFF_WIFI, MAC_SNIFF_BLE, MAC_SNIFF_BLE_ENS } snifftype_t;

int libpax_wifi_counter_count();
int libpax_ble_counter_count();
void libpax_counter_reset();
void reset_bucket();
int mac_add(uint8_t *paddr, snifftype_t sniff_type);
int add_to_bucket(uint16_t id);

#endif
