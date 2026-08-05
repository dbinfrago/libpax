// see Bluetooth Specification v5.0
// e.g. https://www.mouser.it/pdfdocs/bluetooth-Core-v50.pdf

#include "blescan.h"
#include "libpax.h"

#ifndef BLESCANWINDOW
#define BLESCANWINDOW 80  // [milliseconds]
#endif

#ifndef BLESCANINTERVAL
#define BLESCANINTERVAL 80  // [illiseconds]
#endif

#ifndef TAG
#define TAG __FILE__
#endif

int initialized_ble = 0;
// volatile: set once by the app task, read on every advertising report
// from the HCI event processor task
volatile int ble_rssi_threshold = 0;

// Pre-allocated buffers to avoid malloc overhead in hot path
#define BLE_MAX_PACKET_SIZE 256

// The packet bytes are embedded directly in the queue item (copied by value
// on xQueueSend/xQueueReceive) instead of pointing into a small shared pool.
// A pool sized independently of the queue depth allowed an in-flight, still
// unprocessed entry's backing buffer to be overwritten by a newer packet
// before it was read, corrupting parsed MAC addresses/counts.
typedef struct {
  uint8_t q_data[BLE_MAX_PACKET_SIZE];
  uint16_t q_data_len;
} host_rcv_data_t;

static uint8_t hci_cmd_buf[128];

// Number of in-flight HCI advertising reports the queue can hold before
// the BT controller callback starts dropping them. Raised from 30 to give
// more headroom under high device density, where accurate counting matters
// most.
#define BLE_ADV_QUEUE_SIZE 60

static QueueHandle_t adv_queue;
static TaskHandle_t hci_eventprocessor;

// Count of advertising reports dropped because the queue was full, so the
// queue size can be tuned based on real-world load instead of guessing.
static volatile uint32_t ble_adv_dropped = 0;

uint32_t get_ble_adv_dropped_count(void) { return ble_adv_dropped; }

/*
 * @brief: BT controller callback function, used to notify the upper layer that
 *         controller is ready to receive command
 */
static void controller_rcv_pkt_ready(void) {
  // nothing to do here
}

/*
 * @brief: BT controller callback function to transfer data packet to
 *         the host. Optimized for minimal allocation overhead.
 */
static int host_rcv_pkt(uint8_t *data, uint16_t len) {
  host_rcv_data_t send_data;

  if (len > BLE_MAX_PACKET_SIZE) {
    ESP_LOGE(TAG, "BLE packet too large for queue item (%u > %u)", len,
             (unsigned)BLE_MAX_PACKET_SIZE);
    return ESP_FAIL;
  }

  /* Check second byte for HCI event. If event opcode is 0x0e, the event is
   * HCI Command Complete event. Since we have received "0x0e" event, we can
   * check for byte 4 for command opcode and byte 6 for it's return status.
   * Requires len > 6, checked first since data[6] would otherwise be an
   * out-of-bounds read for shorter packets. */
  if (len > 6 && data[1] == 0x0e && data[6] != 0) {
    ESP_LOGE(TAG, "Event opcode 0x%02x fail with reason: 0x%02x.", 
             data[4], data[6]);
    return ESP_FAIL;
  }

  memcpy(send_data.q_data, data, len);
  send_data.q_data_len = len;
  
  if (xQueueSend(adv_queue, (void *)&send_data, (TickType_t)0) != pdTRUE) {
    ble_adv_dropped = ble_adv_dropped + 1;
    ESP_LOGD(TAG, "Failed to enqueue advertising report. Queue full.");
  }
  
  return ESP_OK;
}

static esp_vhci_host_callback_t vhci_host_cb = {controller_rcv_pkt_ready,
                                                host_rcv_pkt};

static void hci_cmd_send_reset(void) {
  uint16_t sz = make_cmd_reset(hci_cmd_buf);
  esp_vhci_host_send_packet(hci_cmd_buf, sz);
}

static void hci_cmd_send_set_evt_mask(void) {
  /* Set bit 61 in event mask to enable LE Meta events. */
  uint8_t evt_mask[8] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x20};
  uint16_t sz = make_cmd_set_evt_mask(hci_cmd_buf, evt_mask);
  esp_vhci_host_send_packet(hci_cmd_buf, sz);
}

static void hci_cmd_send_ble_scan_params(void) {
  /* Set scan type to 0x01 for active scanning and 0x00 for passive scanning. */
  // see see # Bluetooth Specification v5.0, Vol 6, Part B, sec 4.4.3.1
  uint8_t scan_type = 0x00;  // passive scan, since we don't need / want answers

  /* Scan window and Scan interval are set in terms of number of slots. Each
   * slot is of 625 microseconds. */
  uint16_t scan_interval = BLESCANINTERVAL * 1000 / 625;
  uint16_t scan_window = BLESCANWINDOW * 1000 / 625;
  uint8_t own_addr_type = 0x00; /* Public Device Address (default). */
  uint8_t filter_policy = 0x00; /* Accept all packets excpet directed
                                   advertising packets (default). */
  uint16_t sz =
      make_cmd_ble_set_scan_params(hci_cmd_buf, scan_type, scan_interval,
                                   scan_window, own_addr_type, filter_policy);
  esp_vhci_host_send_packet(hci_cmd_buf, sz);
}

static void hci_cmd_send_ble_scan_start(void) {
  uint8_t scan_enable = 0x01;       /* Scanning enabled. */
  uint8_t filter_duplicates = 0x00; /* Duplicate filtering disabled. */
  uint16_t sz =
      make_cmd_ble_set_scan_enable(hci_cmd_buf, scan_enable, filter_duplicates);
  esp_vhci_host_send_packet(hci_cmd_buf, sz);
  ESP_LOGI(TAG, "BLE Scanning started");
}

// Pre-allocated receiver data and address buffer
static host_rcv_data_t rcv_data_buffer;
static uint8_t addr_buffer[6 * 32];  // Max 32 responses per event

void hci_evt_process(void *pvParameters) {
  uint8_t sub_event, num_responses, hci_event_opcode;
  uint16_t total_data_len;  // sum of up to 32 per-report lengths, must not be uint8_t (overflows)
  uint16_t data_ptr;
  uint16_t q_len;
  int8_t rssi;  // Use signed int8_t for RSSI values

  while (1) {
    uint8_t *queue_data = NULL;
    total_data_len = 0;

    if (xQueueReceive(adv_queue, &rcv_data_buffer, portMAX_DELAY) != pdPASS) {
      ESP_LOGE(TAG, "Queue receive error");
      continue;
    }

    queue_data = rcv_data_buffer.q_data;
    data_ptr = 0;
    q_len = rcv_data_buffer.q_data_len;

    // Bail out before reading the header fields below if the packet is too
    // short to hold them; queue_data is a reused static buffer, so without
    // this check a truncated packet would make us parse stale bytes left
    // over from a previous, larger packet.
    if (q_len < 5) {
      continue;
    }

    // Parsing `data' and copying in various fields
    // see # Bluetooth Specification v5.0, Vol 2, Part E, sec 7.7.65.2
    hci_event_opcode = queue_data[++data_ptr];
    
    if (hci_event_opcode == LE_META_EVENTS) {
      // set `data_ptr' to 4th entry, which will point to sub event
      data_ptr += 2;
      sub_event = queue_data[data_ptr++];
      
      // check if sub event is LE advertising report event
      if (sub_event == HCI_LE_ADV_REPORT) {
        // get number of advertising reports
        num_responses = queue_data[data_ptr++];
        
        // Sanity check
        if (num_responses > 32) {
          ESP_LOGW(TAG, "Invalid num_responses: %u", num_responses);
          continue;
        }

        // skip 2 bytes event type and advertising type for every report
        data_ptr += 2 * num_responses;

        // Each stage below is bounds-checked against q_len before indexing
        // queue_data, so a malformed/truncated report can't read stale data
        // left over from a previous packet in the reused buffer.
        if (data_ptr + 6 * num_responses > q_len) {
          ESP_LOGW(TAG, "Truncated adv report: missing address fields");
          continue;
        }

        // get device address in every advertising report
        // -> note: BD addresses are stored in little endian format!
        // see # Bluetooth Specification v5.0, Vol 2, Part E, sec 5.2
        for (uint8_t i = 0; i < num_responses; i++) {
          for (uint8_t j = 5; j < 6; j--) {  // j--; but since j is unsigned, use j < 6
            addr_buffer[(6 * i) + j] = queue_data[data_ptr++];
          }
        }

        if (data_ptr + num_responses > q_len) {
          ESP_LOGW(TAG, "Truncated adv report: missing data-length fields");
          continue;
        }

        // get length of data for each advertising report
        for (uint8_t i = 0; i < num_responses; i++) {
          total_data_len += queue_data[data_ptr++];
        }

        // skip all data packets
        data_ptr += total_data_len;

        if (data_ptr + num_responses > q_len) {
          ESP_LOGW(TAG, "Truncated adv report: missing rssi fields");
          continue;
        }

        // Count each advertising report within rssi threshold
        for (uint8_t i = 0; i < num_responses; i++) {
          rssi = -(0xFF - queue_data[data_ptr++]);
          
          // Early exit on RSSI threshold (common case)
          if (ble_rssi_threshold != 0 && rssi < ble_rssi_threshold) {
            continue;
          }
          
          mac_add(addr_buffer + 6 * i, MAC_SNIFF_BLE);
        }
      }
    }
  }
}

void start_BLE_scan(uint16_t blescantime, uint16_t blescanwindow,
                    uint16_t blescaninterval) {
#ifdef LIBPAX_BLE
  ESP_LOGI(TAG, "Initializing bluetooth scanner ...");

/* Initialize BT controller to allocate task and other resource. */
#ifdef LIBPAX_ARDUINO
  if (btStart()) {
#endif
#ifdef LIBPAX_ESPIDF
    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_bt_controller_init(&bt_cfg));
    ESP_ERROR_CHECK(esp_bt_controller_enable(ESP_BT_MODE_BLE));
#endif

    /* A queue for storing received HCI packets. */
    ble_adv_dropped = 0;
    adv_queue = xQueueCreate(BLE_ADV_QUEUE_SIZE, sizeof(host_rcv_data_t));
    if (adv_queue == NULL) {
      ESP_LOGE(TAG, "Queue creation failed");
      return;
    }

    /* start HCI event processor task with prio 1 */
    xTaskCreate(&hci_evt_process, "hci_evt_process", 2048, NULL, 1,
                &hci_eventprocessor);

    esp_vhci_host_register_callback(&vhci_host_cb);

    /* start BLE advertising and scanning */
    bool continue_commands = 1;
    int cmd_cnt = 0;

    while (continue_commands) {
      if (continue_commands && esp_vhci_host_check_send_available()) {
        switch (cmd_cnt) {
          // send initialize commands
          case 0:
            hci_cmd_send_reset();
            ++cmd_cnt;
            break;
          case 1:
            hci_cmd_send_set_evt_mask();
            ++cmd_cnt;
            break;

          // setup passive scanning, see BT 5.0 specs Vol 6, Part D, 4.1
          case 2:
            hci_cmd_send_ble_scan_params();
            ++cmd_cnt;
            break;
          case 3:
            hci_cmd_send_ble_scan_start();
            ++cmd_cnt;
            break;
            
          // all commands done
          default:
            continue_commands = 0;
            break;
        }
      }
      vTaskDelay(pdMS_TO_TICKS(100));
    }

    ESP_LOGI(TAG, "Bluetooth scanner started");
    initialized_ble = 1;
#ifdef LIBPAX_ARDUINO
  } else {
    ESP_LOGE(TAG, "Failed on Bluetooth scanner started");
  }
#endif
#endif
}  // start_BLEscan

void stop_BLE_scan(void) {
#ifdef LIBPAX_BLE
  if (initialized_ble) {
    ESP_LOGI(TAG, "Shutting down bluetooth scanner ...");
#ifdef LIBPAX_ARDUINO
    btStop();  // disable bt_controller
#endif
#ifdef LIBPAX_ESPIDF
    ESP_ERROR_CHECK(esp_bt_controller_disable());
    ESP_ERROR_CHECK(esp_bt_controller_deinit());
#endif
    ESP_LOGI(TAG, "Bluetooth scanner stopped");
    initialized_ble = 0;
  }
#endif
}  // stop_BLEscan

void set_BLE_rssi_filter(int set_rssi_threshold) {
  ble_rssi_threshold = set_rssi_threshold;
}
