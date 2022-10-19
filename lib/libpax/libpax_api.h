#ifndef _LIBPAX_API_H
#define _LIBPAX_API_H

#include <stdint.h>

// Build-time options
// #define LIBPAX_WIFI // enables WiFi sniffing features in build 
// #define LIBPAX_BLE  // enables BLE sniffing features in build

#define WIFI_CHANNEL_ALL    0b1111111111111
#define WIFI_CHANNEL_1      0b0000000000001
#define WIFI_CHANNEL_2      0b0000000000010
#define WIFI_CHANNEL_3      0b0000000000100
#define WIFI_CHANNEL_4      0b0000000001000
#define WIFI_CHANNEL_5      0b0000000010000
#define WIFI_CHANNEL_6      0b0000000100000
#define WIFI_CHANNEL_7      0b0000001000000
#define WIFI_CHANNEL_8      0b0000010000000
#define WIFI_CHANNEL_9      0b0000100000000
#define WIFI_CHANNEL_10     0b0001000000000
#define WIFI_CHANNEL_11     0b0010000000000
#define WIFI_CHANNEL_12     0b0100000000000
#define WIFI_CHANNEL_13     0b1000000000000

#define LIBPAX_ERROR_WIFI_NOT_AVAILABLE 0b00000001
#define LIBPAX_ERROR_BLE_NOT_AVAILABLE  0b00000010

// configuration given to lib for sniffing parameters
struct libpax_config_t {
    uint16_t wifi_channel_map;              // bit map which channel to cycle through
                                            // <-  13 ..........1 ->
                                            //    0b1010000001001 would be Channel: 1, 4, 11, 13
    uint8_t wificounter;                    // set to 0 if you do not want to install the WiFi sniffer
    char wifi_my_country[3];                // set country code for WiFi RF settings, e.g. "01", "DE", etc.
    uint16_t wifi_channel_switch_interval;  // [seconds/100] -> 0,5 sec.
    int wifi_rssi_threshold;                // Filter for how strong the wifi signal should be to be counted
    int ble_rssi_threshold;                 // Filter for how strong the bluetooth signal should be to be counted
    uint8_t blecounter;                     // set to 0 if you do not want to install the BLE sniffer
    uint32_t blescantime;                   // [seconds] scan duration, 0 means infinite [default]
    uint16_t blescanwindow;                 // [milliseconds] scan window, see below, 3 ... 10240, default 80ms
    uint16_t blescaninterval;               // [illiseconds] scan interval, see below, 3 ... 10240, default 80ms = 100% duty cycle
};


// payload updated periodically or on demand
struct count_payload_t {
    uint32_t pax;        // pax estimatenion. Currently implemented as sum of wifi and ble
    uint32_t wifi_count; // detected wifi_count in interval
    uint32_t ble_count;  // detected ble_count in interval
};

/**
 *   Must be called before use of the lib. Initialze a callback the payload paxcount is written back too.
 *   @param[in] callback Callback which is called every pax_report_interval_sec to inform on current pax
 *   @param[out] current_count memory for pax count. Updated directly before callback is called
 *   @param[in] pax_report_interval_sec defines interval in s between a pax count callback.
 *   @param[in] countermode avalible modes TBD
*/
int libpax_counter_init(void (*callback)(void), struct count_payload_t* current_count, uint16_t pax_report_interval_sec, int countermode);

/**
 *   Starts hardware wifi layer and counting of pax
 */
int libpax_counter_start();

/**
 *   Stops sniffing process after which no new macs will be received and the wifi allocation is shutdown
 */
int libpax_counter_stop();

/**
 *  Optional external counter query outside of given time interval
 *  param[out] count
 */
int libpax_counter_count(struct count_payload_t* count);

/*
 * Size in bytes of a serialized config
 */
#define LIBPAX_CONFIG_SIZE 64

/*
 * Writes given configuration into memory at store_addr
 *   @param [out] store_addr addr to write configuration payload into (should be allocated to sizeof(libpax_config_storage_t))
 *   @param configuration configuration used for writing into memory at store_addr
*/
void libpax_serialize_config(char* store_addr, struct libpax_config_t* configuration);

/*
 Writes given configuration into memory at store_addr
 *   @param source addr from which configuration payload is read (only minor version changes are allowed)
 *   @param [out] configuration configuration which was stored in memory at restore_addr
*/
int libpax_deserialize_config(char* source, struct libpax_config_t* configuration);

/*
 Sets scanning configuration
 *   @param configuration configuration used for scanning behaviour
*/
int libpax_update_config(struct libpax_config_t* configuration);

/*
 Returns current set configuration as out parameter
 *   @param [out] configuration configuration used for scanning behaviour
*/
void libpax_get_current_config(struct libpax_config_t* configuration);

/*
 Writes default configuration into out parameter
 *   @param [out] configuration configuration used for scanning behaviour
*/
void libpax_default_config(struct libpax_config_t* configuration);
#endif
