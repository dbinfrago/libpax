#ifndef _BLESCAN_H
#define _BLESCAN_H

#include "globals.h"

// Bluetooth specific includes
#include <esp_bt.h>
#include <esp_bt_main.h>
#include <esp_gap_ble_api.h>
#include <esp_blufi_api.h> // needed for BLE_ADDR types, do not remove
#include <esp_coexist.h>

void start_BLE_scan(uint16_t blescantime, uint16_t blescanwindow, uint16_t blescaninterval);
void stop_BLE_scan(void);

#endif