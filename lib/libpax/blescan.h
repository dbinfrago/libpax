#ifndef _BLESCAN_H
#define _BLESCAN_H

#include "globals.h"

#include <stdio.h>
#include <string.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>
#include <nvs_flash.h>
#include <esp_bt.h>
#include <esp_log.h>
#include <bt_hci_common.h>

void start_BLE_scan(uint16_t blescantime, uint16_t blescanwindow, uint16_t blescaninterval);
void stop_BLE_scan(void);
void set_BLE_rssi_filter(int set_rssi_threshold);

#endif
