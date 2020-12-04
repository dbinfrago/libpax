#include <libpax_api.h>
#include <stdio.h>

struct count_payload_t count_from_libpax;

void process_count(void) {
  printf("pax: %d; %d; %d;\n", count_from_libpax.pax, count_from_libpax.wifi_count, count_from_libpax.ble_count);
}

void init() {
  struct libpax_config_t configuration; 
  libpax_default_config(&configuration);
  configuration.blecounter = 1;
  configuration.blescantime = 0; // infinit
  configuration.wificounter = 1; 
  configuration.wifi_channel_map = WIFI_CHANNEL_ALL;
  configuration.wifi_channel_switch_interval = 50;
  configuration.wifi_rssi_threshold = -80;
  libpax_update_config(&configuration);

  // internal processing initialization
  libpax_counter_init(process_count, &count_from_libpax, 10*1000, 1); 
  libpax_counter_start();
}

#ifdef LIBPAX_ARDUINO
// arduino code
void setup() {
  init();
}

void loop() {}
#endif

#ifdef LIBPAX_ESPIDF
// espidf code
extern "C" void app_main();
void app_main() {
  init();
}
#endif