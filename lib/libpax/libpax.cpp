#include "globals.h"

#include "libpax.h"
#include <libpax_api.h>
#include "esp_log.h"
#include <string.h>

uint16_t seen_ids [LIBPAX_MAX_SIZE];
int seen_ids_count = 0;

uint16_t volatile macs_wifi = 0;
uint16_t volatile macs_ble = 0;

uint8_t volatile channel = 0;  // channel rotation counter

configData_t cfg_pax;


/** remember given id
 * returns 1 if id is new, 0 if already seen this is since last reset
 */
int add_to_bucket(uint16_t id) {
  if(seen_ids_count == LIBPAX_MAX_SIZE) {
    return 0;
  }

  int pos = id % LIBPAX_MAX_SIZE;
  int start_pos = pos;
  while(1) {
    if(seen_ids[pos] == id) {
      return 0;
    }
    else if(seen_ids[pos] == 0) {
      seen_ids[pos] = id;
      seen_ids_count++;
      return 1;
    } else {
      pos = (pos + 1) % LIBPAX_MAX_SIZE;
      if(pos == start_pos) {
        // This should only be impossible to reach. Reaching our start_pos
        // means we searched didn't find an empty spoint in the bucket.
        // But there does exist a fast-pass for this therefor this 
        // should not be reachable and just exists as a fallback to 
        // avoid beeing stuck in an endless loop.
        return 0;
      }
    }
  }
}


void reset_bucket() {
  memset(seen_ids, 0, sizeof(seen_ids));
  seen_ids_count = 0;
}


void wifiDefaultConfig() {
  cfg_pax.countermode =
      COUNTERMODE;        // 0=cyclic, 1=cumulative, 2=cyclic confirmed
  cfg_pax.rssilimit = 0;  // threshold for rssilimiter, negative value!
  cfg_pax.wifichancycle =
      WIFI_CHANNEL_SWITCH_INTERVAL;  // wifi channel switch cycle [seconds/100]
  cfg_pax.blescantime =
      BLESCANINTERVAL /
      10;  // BT channel scan cycle [seconds/100], default 1 (= 10ms)
  cfg_pax.blescan = BLECOUNTER;    // 0=disabled, 1=enabled
  cfg_pax.wifiscan = WIFICOUNTER;  // 0=disabled, 1=enabled
  cfg_pax.wifiant = 0;             // 0=internal, 1=external (for LoPy/LoPy4)
}

int libpax_wifi_counter_count() {
  return macs_wifi;
}

int libpax_ble_counter_count() {
  return macs_ble;
}


uint64_t macConvert(uint8_t *paddr) {
  uint64_t *mac;
  mac = (uint64_t *)paddr;
  return (__builtin_bswap64(*mac) >> 16);
}

int mac_add(uint8_t *paddr, snifftype_t sniff_type) {
  // mac addresses are 6 bytes long, we only use the last two bytes
  uint16_t id = *(paddr + 4);

  
  int added = add_to_bucket(id);

  // Count only if MAC was not yet seen
  if (added) {
    if(sniff_type == MAC_SNIFF_BLE) {
      macs_ble++;
    } else if(sniff_type == MAC_SNIFF_WIFI)  {
      macs_wifi++;
    }
  };  // added

  return added;  // function returns bool if a new and unique Wifi or BLE mac
                 // was counted (true) or not (false)
}
