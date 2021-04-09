/*
LICENSE

Copyright  2020      Deutsche Bahn Station&Service AG

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.

*/
#include "globals.h"

#include "libpax.h"
#include <libpax_api.h>
#include "esp_log.h"
#include <string.h>

typedef uint32_t bitmap_t;
enum { BITS_PER_WORD = sizeof(bitmap_t) * CHAR_BIT };
#define WORD_OFFSET(b) ((b) / BITS_PER_WORD)
#define BIT_OFFSET(b) ((b) % BITS_PER_WORD)
#define LIBPAX_MAX_SIZE 0xFFFF  // full enumeration of uint16_t
#define LIBPAX_MAP_SIZE (LIBPAX_MAX_SIZE / BITS_PER_WORD)

bitmap_t seen_ids_map[LIBPAX_MAP_SIZE];
int seen_ids_count = 0;

uint16_t volatile macs_wifi = 0;
uint16_t volatile macs_ble = 0;

uint8_t volatile channel = 0;  // channel rotation counter

void set_id(bitmap_t *bitmap, uint16_t id) {
  bitmap[WORD_OFFSET(id)] |= ((bitmap_t)1 << BIT_OFFSET(id));
}

int get_id(bitmap_t *bitmap, uint16_t id) {
  bitmap_t bit = bitmap[WORD_OFFSET(id)] & ((bitmap_t)1 << BIT_OFFSET(id));
  return bit != 0;
}

/** remember given id
 * returns 1 if id is new, 0 if already seen this is since last reset
 */
int add_to_bucket(uint16_t id) {
  if (get_id(seen_ids_map, id)) {
    return 0;  // already seen
  } else {
    set_id(seen_ids_map, id);
    seen_ids_count++;
    return 1;  // new
  }
}

void reset_bucket() {
  memset(seen_ids_map, 0, sizeof(seen_ids_map));
  seen_ids_count = 0;
}

int libpax_wifi_counter_count() {
  return macs_wifi;
}

int libpax_ble_counter_count() {
  return macs_ble;
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
