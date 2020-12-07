#include <libpax_api.h>
#include "libpax.h"
#include "wifiscan.h"
#include "blescan.h"

#include <esp_event.h>      // needed for Wifi event handler
#include <esp_spi_flash.h>  // needed for reading ESP32 chip attributes
#include <esp_log.h>
#include "freertos/task.h"  // needed for tasks
#include "freertos/timers.h"  // TimerHandle_t
#include <string.h>

struct libpax_config_t current_config;
int config_set = 0;


void (*report_callback)(void);
struct count_payload_t* pCurrent_count;
int counter_mode;

void fill_counter(struct count_payload_t* count) {
    count->wifi_count = libpax_wifi_counter_count();
    count->ble_count = libpax_ble_counter_count();
    count->pax = pCurrent_count->wifi_count + pCurrent_count->ble_count;
}

void libpax_counter_reset() {
  macs_wifi = 0;
  macs_ble = 0;
  reset_bucket();
}

void report(TimerHandle_t xTimer) {
    fill_counter(pCurrent_count);
    report_callback();
    libpax_counter_reset();
}

void libpax_serialize_config(char* store_addr, struct libpax_config_t* configuration) {
    struct libpax_config_storage_t storage_buffer;
    storage_buffer.major_version = CONFIG_MAJOR_VERSION;
    storage_buffer.minor_version = CONFIG_MINOR_VERSION;
    memcpy(&(storage_buffer.config), configuration, sizeof(struct libpax_config_t));
    memset(storage_buffer.checksum, 0, sizeof(storage_buffer.checksum));
    memcpy(store_addr, &storage_buffer, sizeof(struct libpax_config_storage_t));
}

int libpax_deserialize_config(char* source, struct libpax_config_t* configuration) {
    struct libpax_config_storage_t storage_buffer;
    memcpy(&storage_buffer, source, sizeof(struct libpax_config_storage_t));
    if (storage_buffer.major_version != CONFIG_MAJOR_VERSION) {
        ESP_LOGE("configuration", "Restoring incompatible config with different MAJOR version: %d.%d instead of %d.%d",
            storage_buffer.major_version, storage_buffer.minor_version, CONFIG_MAJOR_VERSION, CONFIG_MINOR_VERSION);
        return -1;
    }
    if (storage_buffer.minor_version != CONFIG_MINOR_VERSION) {
        ESP_LOGW("configuration", "Restoring config with different MINOR version: %d.%d instead of %d.%d",
            storage_buffer.major_version, storage_buffer.minor_version, CONFIG_MAJOR_VERSION, CONFIG_MINOR_VERSION);
    }
    memcpy(configuration, &(storage_buffer.config), sizeof(struct libpax_config_t));
    return 0;
}

void libpax_default_config(struct libpax_config_t* configuration) {
    memset(configuration, 0, sizeof(struct libpax_config_t));
    configuration->blecounter = 0;
    configuration->wificounter = 1;
    configuration->wifi_my_country = 1;
    configuration->wifi_channel_map = 0b100010100100100;
    configuration->wifi_channel_switch_interval = 50;
    configuration->wifi_rssi_threshold = -80;
    configuration->blescaninterval = 0;
    configuration->blescantime = 0;
    configuration->blescanwindow = 0;
}

void libpax_get_current_config(struct libpax_config_t* configuration) {
    memcpy(configuration, &current_config, sizeof(struct libpax_config_t));
}

int libpax_update_config(struct libpax_config_t* configuration) {
    int result = 0;

#ifndef LIBPAX_WIFI
    if(configuration->wificounter) {
        ESP_LOGE("configuration", "Configuration requests Wi-Fi but was disabled at compile time.");
        result &= LIBPAX_ERROR_WIFI_NOT_AVAILABLE;
    }
#endif

#ifndef LIBPAX_BLE
    if(configuration->blecounter) {
        ESP_LOGE("configuration", "Configuration requests BLE but was disabled at compile time.");
        result &= LIBPAX_ERROR_BLE_NOT_AVAILABLE;
    }
#endif

    if(result == 0) {
        memcpy(&current_config, configuration, sizeof(struct libpax_config_t));
        config_set = 1;
    }
    return result;
}

TimerHandle_t PaxReportTimer = NULL;
int libpax_counter_init(void (*init_callback)(void), struct count_payload_t* init_current_count, uint16_t init_pax_report_interval, int init_counter_mode) {
    if (PaxReportTimer != NULL && xTimerIsTimerActive(PaxReportTimer)) {
        ESP_LOGW("initialization", "lib already active. Ignoring new init.");
        return -1;
    }

    report_callback = init_callback;
    pCurrent_count = init_current_count;
    counter_mode = init_counter_mode;

    libpax_counter_reset();

    PaxReportTimer = xTimerCreate("PaxReportTimer", pdMS_TO_TICKS(init_pax_report_interval),
                                pdTRUE, (void*)0, report);
    xTimerStart(PaxReportTimer, 0);
    return 0;
}

int libpax_counter_start() {
    if(config_set == 0) {
        ESP_LOGE("configuration", "Configuration was not yet set.");
        return -1;
    }
    if(current_config.wificounter) {
        set_country(current_config.wifi_my_country);
        set_channels(current_config.wifi_channel_map);
        set_rssi_filter(current_config.wifi_rssi_threshold);
        wifi_sniffer_init(current_config.wifi_channel_switch_interval);
    }
    if(current_config.blecounter) {
        start_BLEscan(current_config.blescantime);
    }
    return 0;
}

int libpax_counter_stop() {
    wifi_sniffer_stop();
    stop_BLEscan();
    xTimerStop(PaxReportTimer, 0);
    return 0;
}

int libpax_counter_count(struct count_payload_t* count) {
    fill_counter(count);
    return 0;
}
