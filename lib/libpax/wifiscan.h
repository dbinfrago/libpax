#ifndef _WIFISCAN_H
#define _WIFISCAN_H

#include <esp_wifi.h>
#include <esp_coexist.h>
typedef struct {
  unsigned frame_ctrl : 16;
  unsigned duration_id : 16;
  uint8_t addr1[6];  // receiver address
  uint8_t addr2[6];  // sender address
  uint8_t addr3[6];  // filtering address
  unsigned sequence_ctrl : 16;
  uint8_t addr4[6];  // optional
} wifi_ieee80211_mac_hdr_t;

typedef struct {
  wifi_ieee80211_mac_hdr_t hdr;
  uint8_t payload[0];  // network data ended with 4 bytes csum (CRC32)
} wifi_ieee80211_packet_t;

// extern const wifi_ieee80211_mac_hdr_t *hdr;

void set_country(uint8_t country_code);
void set_channels(uint16_t channels_map);
void set_rssi_filter(int8_t set_rssi_threshold);

void wifi_sniffer_init(uint16_t wifi_channel_switch_interval);
void wifi_sniffer_stop();

extern int run_count;
extern int timeback_delta;

#endif
