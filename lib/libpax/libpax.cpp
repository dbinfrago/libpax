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

typedef uint32_t bitmap_t;
enum { BITS_PER_WORD = sizeof(bitmap_t) * CHAR_BIT };
#define WORD_OFFSET(b) ((b) / BITS_PER_WORD)
#define BIT_OFFSET(b) ((b) % BITS_PER_WORD)

// The bitmap requires 2**16 = 65536 entries,
// while using 32 bit integers, we need 65536 / 32 = 2048 integers
// Separate maps per sniff type: a shared map would let a WiFi id and an
// unrelated BLE id collide with each other, doubling the effective
// collision rate whenever both radios are active at once.
// Each map is only allocated when its sniffer is actually built in, so a
// WiFi-only or BLE-only build doesn't waste 8 KiB of DRAM on the other map.
#if defined(LIBPAX_WIFI)
DRAM_ATTR bitmap_t seen_ids_map_wifi[2048];
#endif
#if defined(LIBPAX_BLE)
DRAM_ATTR bitmap_t seen_ids_map_ble[2048];
#endif

volatile uint16_t macs_wifi = 0;
volatile uint16_t macs_ble = 0;

volatile uint8_t channel = 0;  // channel rotation counter

/** remember given id in the bitmap for the given sniff type
 * returns 1 if id is new, 0 if already seen this is since last reset
 * Hot-path critical function - highly optimized
 */
static inline IRAM_ATTR int add_to_bucket(uint16_t id, snifftype_t sniff_type) {
  bitmap_t *map;
#if defined(LIBPAX_WIFI) && defined(LIBPAX_BLE)
  map = (sniff_type == MAC_SNIFF_BLE) ? seen_ids_map_ble : seen_ids_map_wifi;
#elif defined(LIBPAX_BLE)
  map = seen_ids_map_ble;
#elif defined(LIBPAX_WIFI)
  map = seen_ids_map_wifi;
#else
  return 0;  // neither sniffer built in, nothing to track
#endif
  uint16_t word_idx = WORD_OFFSET(id);
  uint32_t bit_mask = ((bitmap_t)1 << BIT_OFFSET(id));

  if (map[word_idx] & bit_mask) {
    return 0;  // already seen
  }

  map[word_idx] |= bit_mask;
  return 1;  // new
}

void reset_bucket() {
#if defined(LIBPAX_WIFI)
  memset(seen_ids_map_wifi, 0, sizeof(seen_ids_map_wifi));
#endif
#if defined(LIBPAX_BLE)
  memset(seen_ids_map_ble, 0, sizeof(seen_ids_map_ble));
#endif
}

int libpax_wifi_counter_count() { return macs_wifi; }

int libpax_ble_counter_count() { return macs_ble; }

/** Hot-path function called from ISR context for every WiFi/BLE packet
 * Optimized for minimum cycles per call
 */
IRAM_ATTR int mac_add(uint8_t *paddr, snifftype_t sniff_type) {
  // Check locally administered bit first (cheapest check)
  if (!(paddr[0] & 0x02)) return 0;  // 0x02 = locally administered bit
  
  // Use last 2 bytes of MAC address as ID (little-endian)
  uint16_t id = (paddr[5] << 8) | paddr[4];
  
  int added = add_to_bucket(id, sniff_type);
  
  // Only update counters on new MAC (most calls return here)
  if (!added) return 0;
  
  // Update appropriate counter based on type
  if (sniff_type == MAC_SNIFF_BLE) {
    macs_ble++;
  } else if (sniff_type == MAC_SNIFF_WIFI) {
    macs_wifi++;
  }
  
  return 1;
}
