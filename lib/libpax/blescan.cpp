#include "blescan.h"
#include "libpax.h"

#include <bt_hci_common.h>
#include <esp_bt.h>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/task.h>
#include <nvs_flash.h>
#include <stdio.h>
#include <string.h>

// local Tag for logging
static const char TAG[] = "bluetooth";
int initialized_ble = 0;
int ble_rssi_threshold = 0;

typedef struct {
  char scan_local_name[32];
  uint8_t name_len;
} ble_scan_local_name_t;

typedef struct {
  uint8_t *q_data;
  uint16_t q_data_len;
} host_rcv_data_t;

static uint8_t hci_cmd_buf[128];

static uint16_t scanned_count = 0;
static QueueHandle_t adv_queue;
static TaskHandle_t hci_eventprocessor;

static void periodic_timer_callback(void *arg) {
  ESP_LOGI(TAG, "Number of received advertising reports: %d", scanned_count);
}

/*
 * @brief: BT controller callback function, used to notify the upper layer that
 *         controller is ready to receive command
 */
static void controller_rcv_pkt_ready(void) {
  ESP_LOGI(TAG, "controller rcv pkt ready");
}

/*
 * @brief: BT controller callback function to transfer data packet to
 *         the host
 */
static int host_rcv_pkt(uint8_t *data, uint16_t len) {
  host_rcv_data_t send_data;
  uint8_t *data_pkt;
  /* Check second byte for HCI event. If event opcode is 0x0e, the event is
   * HCI Command Complete event. Sice we have recieved "0x0e" event, we can
   * check for byte 4 for command opcode and byte 6 for it's return status. */
  if (data[1] == 0x0e) {
    if (data[6] == 0) {
      ESP_LOGI(TAG, "Event opcode 0x%02x success.", data[4]);
    } else {
      ESP_LOGE(TAG, "Event opcode 0x%02x fail with reason: 0x%02x.", data[4],
               data[6]);
      return ESP_FAIL;
    }
  }

  data_pkt = (uint8_t *)malloc(sizeof(uint8_t) * len);
  if (data_pkt == NULL) {
    ESP_LOGE(TAG, "Malloc data_pkt failed!");
    return ESP_FAIL;
  }
  memcpy(data_pkt, data, len);
  send_data.q_data = data_pkt;
  send_data.q_data_len = len;
  if (xQueueSend(adv_queue, (void *)&send_data, (TickType_t)0) != pdTRUE) {
    ESP_LOGD(TAG, "Failed to enqueue advertising report. Queue full.");
    /* If data sent successfully, then free the pointer in `xQueueReceive'
     * after processing it. Or else if enqueue in not successful, free it
     * here. */
    free(data_pkt);
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
  uint8_t scan_type = 0x01;

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
  ESP_LOGI(TAG, "BLE Scanning started..");
}

static void hci_cmd_send_ble_adv_start(void) {
  uint16_t sz = make_cmd_ble_set_adv_enable(hci_cmd_buf, 1);
  esp_vhci_host_send_packet(hci_cmd_buf, sz);
  ESP_LOGI(TAG, "BLE Advertising started..");
}

static void hci_cmd_send_ble_set_adv_param(void) {
  /* Minimum and maximum Advertising interval are set in terms of slots. Each
   * slot is of 625 microseconds. */
  uint16_t adv_intv_min = 0x100;
  uint16_t adv_intv_max = 0x100;

  /* Connectable undirected advertising (ADV_IND). */
  uint8_t adv_type = 0;

  /* Own address is public address. */
  uint8_t own_addr_type = 0;

  /* Public Device Address */
  uint8_t peer_addr_type = 0;
  uint8_t peer_addr[6] = {0x80, 0x81, 0x82, 0x83, 0x84, 0x85};

  /* Channel 37, 38 and 39 for advertising. */
  uint8_t adv_chn_map = 0x07;

  /* Process scan and connection requests from all devices (i.e., the White List
   * is not in use). */
  uint8_t adv_filter_policy = 0;

  uint16_t sz = make_cmd_ble_set_adv_param(
      hci_cmd_buf, adv_intv_min, adv_intv_max, adv_type, own_addr_type,
      peer_addr_type, peer_addr, adv_chn_map, adv_filter_policy);
  esp_vhci_host_send_packet(hci_cmd_buf, sz);
}

static void hci_cmd_send_ble_set_adv_data(void) {
  char const *adv_name = "LIBPAX";
  uint8_t name_len = (uint8_t)strlen(adv_name);
  uint8_t adv_data[31] = {0x02, 0x01, 0x06, 0x0, 0x09};
  uint8_t adv_data_len;

  adv_data[3] = name_len + 1;
  for (int i = 0; i < name_len; i++) {
    adv_data[5 + i] = (uint8_t)adv_name[i];
  }
  adv_data_len = 5 + name_len;

  uint16_t sz =
      make_cmd_ble_set_adv_data(hci_cmd_buf, adv_data_len, (uint8_t *)adv_data);
  esp_vhci_host_send_packet(hci_cmd_buf, sz);
  ESP_LOGI(TAG, "Starting BLE advertising with name \"%s\"", adv_name);
}

static esp_err_t get_local_name(uint8_t *data_msg, uint8_t data_len,
                                ble_scan_local_name_t *scanned_packet) {
  uint8_t curr_ptr = 0, curr_len, curr_type;
  while (curr_ptr < data_len) {
    curr_len = data_msg[curr_ptr++];
    curr_type = data_msg[curr_ptr++];
    if (curr_len == 0) {
      return ESP_FAIL;
    }

    /* Search for current data type and see if it contains name as data (0x08 or
     * 0x09). */
    if (curr_type == 0x08 || curr_type == 0x09) {
      for (uint8_t i = 0; i < curr_len - 1; i += 1) {
        scanned_packet->scan_local_name[i] = data_msg[curr_ptr + i];
      }
      scanned_packet->name_len = curr_len - 1;
      return ESP_OK;
    } else {
      /* Search for next data. Current length includes 1 octate for AD Type (2nd
       * octate). */
      curr_ptr += curr_len - 1;
    }
  }
  return ESP_FAIL;
}

void hci_evt_process(void *pvParameters) {
  host_rcv_data_t *rcv_data =
      (host_rcv_data_t *)malloc(sizeof(host_rcv_data_t));
  if (rcv_data == NULL) {
    ESP_LOGE(TAG, "Malloc rcv_data failed!");
    return;
  }
  esp_err_t ret;

  while (1) {
    uint8_t sub_event, num_responses, total_data_len, data_msg_ptr,
        hci_event_opcode;
    uint8_t *queue_data = NULL, *event_type = NULL, *addr_type = NULL,
            *addr = NULL, *data_len = NULL, *data_msg = NULL;
    short int *rssi = NULL;
    uint16_t data_ptr;
    ble_scan_local_name_t *scanned_name = NULL;
    total_data_len = 0;
    data_msg_ptr = 0;
    if (xQueueReceive(adv_queue, rcv_data, portMAX_DELAY) != pdPASS) {
      ESP_LOGE(TAG, "Queue receive error");
    } else {
      /* `data_ptr' keeps track of current position in the received data. */
      data_ptr = 0;
      queue_data = rcv_data->q_data;

      /* Parsing `data' and copying in various fields. */
      hci_event_opcode = queue_data[++data_ptr];
      if (hci_event_opcode == LE_META_EVENTS) {
        /* Set `data_ptr' to 4th entry, which will point to sub event. */
        data_ptr += 2;
        sub_event = queue_data[data_ptr++];
        /* Check if sub event is LE advertising report event. */
        if (sub_event == HCI_LE_ADV_REPORT) {
          scanned_count += 1;

          /* Get number of advertising reports. */
          num_responses = queue_data[data_ptr++];
          event_type = (uint8_t *)malloc(sizeof(uint8_t) * num_responses);
          if (event_type == NULL) {
            ESP_LOGE(TAG, "Malloc event_type failed!");
            goto reset;
          }
          for (uint8_t i = 0; i < num_responses; i += 1) {
            event_type[i] = queue_data[data_ptr++];
          }

          /* Get advertising type for every report. */
          addr_type = (uint8_t *)malloc(sizeof(uint8_t) * num_responses);
          if (addr_type == NULL) {
            ESP_LOGE(TAG, "Malloc addr_type failed!");
            goto reset;
          }
          for (uint8_t i = 0; i < num_responses; i += 1) {
            addr_type[i] = queue_data[data_ptr++];
          }

          /* Get BD address in every advertising report and store in
           * single array of length `6 * num_responses' as each address
           * will take 6 spaces. */
          addr = (uint8_t *)malloc(sizeof(uint8_t) * 6 * num_responses);
          if (addr == NULL) {
            ESP_LOGE(TAG, "Malloc addr failed!");
            goto reset;
          }
          for (int i = 0; i < num_responses; i += 1) {
            for (int j = 0; j < 6; j += 1) {
              addr[(6 * i) + j] = queue_data[data_ptr++];
            }
          }

          /* Get length of data for each advertising report. */
          data_len = (uint8_t *)malloc(sizeof(uint8_t) * num_responses);
          if (data_len == NULL) {
            ESP_LOGE(TAG, "Malloc data_len failed!");
            goto reset;
          }
          for (uint8_t i = 0; i < num_responses; i += 1) {
            data_len[i] = queue_data[data_ptr];
            total_data_len += queue_data[data_ptr++];
          }

          if (total_data_len != 0) {
            /* Get all data packets. */
            data_msg = (uint8_t *)malloc(sizeof(uint8_t) * total_data_len);
            if (data_msg == NULL) {
              ESP_LOGE(TAG, "Malloc data_msg failed!");
              goto reset;
            }
            for (uint8_t i = 0; i < num_responses; i += 1) {
              for (uint8_t j = 0; j < data_len[i]; j += 1) {
                data_msg[data_msg_ptr++] = queue_data[data_ptr++];
              }
            }
          }

          /* Counts of advertisements done. This count is set in advertising
           * data every time before advertising. */
          rssi = (short int *)malloc(sizeof(short int) * num_responses);
          if (data_len == NULL) {
            ESP_LOGE(TAG, "Malloc rssi failed!");
            goto reset;
          }
          for (uint8_t i = 0; i < num_responses; i += 1) {
            rssi[i] = -(0xFF - queue_data[data_ptr++]);
          }

          /* Extracting advertiser's name. */
          data_msg_ptr = 0;
          scanned_name = (ble_scan_local_name_t *)malloc(
              num_responses * sizeof(ble_scan_local_name_t));
          if (data_len == NULL) {
            ESP_LOGE(TAG, "Malloc scanned_name failed!");
            goto reset;
          }
          for (uint8_t i = 0; i < num_responses; i += 1) {
            ret = get_local_name(&data_msg[data_msg_ptr], data_len[i],
                                 scanned_name);

            /* count MAC if adv report has a valid name. */
            if (ret == ESP_OK) {
              data_msg_ptr += data_len[i];
              if ((ble_rssi_threshold) &&
                  (rssi[i] < ble_rssi_threshold)) {  // rssi is negative value
                mac_add((uint8_t *)(addr + 6 * i), MAC_SNIFF_BLE);
              }
            }
          }

          /* Freeing all spaces allocated. */
        reset:
          free(scanned_name);
          free(rssi);
          free(data_msg);
          free(data_len);
          free(addr);
          free(addr_type);
          free(event_type);
        }
      }
      free(queue_data);
    }
    memset(rcv_data, 0, sizeof(host_rcv_data_t));
  }
}

void start_BLE_scan(uint16_t blescantime, uint16_t blescanwindow,
                    uint16_t blescaninterval) {
#ifdef LIBPAX_BLE
  ESP_LOGI(TAG, "Initializing bluetooth scanner ...");

// Initialize BT controller to allocate task and other resource.
#ifdef LIBPAX_ARDUINO
  if (btStart()) {
#endif
#ifdef LIBPAX_ESPIDF
    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_bt_controller_init(&bt_cfg));
    ESP_ERROR_CHECK(esp_bt_controller_enable(ESP_BT_MODE_BLE));
#endif

    bool continue_commands = 1;
    int cmd_cnt = 0;

    const esp_timer_create_args_t periodic_timer_args = {
      callback : &periodic_timer_callback,
      arg : NULL,
      dispatch_method : ESP_TIMER_TASK,
      name : "periodic"
    };

    /* Create timer for logging scanned devices. */
    esp_timer_handle_t periodic_timer;
    ESP_ERROR_CHECK(esp_timer_create(&periodic_timer_args, &periodic_timer));

    /* Start periodic timer for BLESCANTIME sec. */
    ESP_ERROR_CHECK(
        esp_timer_start_periodic(periodic_timer, BLESCANTIME * 1000000));

    /* Initialize NVS — it is used to store PHY calibration data */
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES ||
        ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
      ESP_ERROR_CHECK(nvs_flash_erase());
      ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    /* A queue for storing received HCI packets. */
    adv_queue = xQueueCreate(15, sizeof(host_rcv_data_t));
    if (adv_queue == NULL) {
      ESP_LOGE(TAG, "Queue creation failed\n");
      return;
    }
    
    xTaskCreatePinnedToCore(&hci_evt_process, "hci_evt_process", 2048, NULL, 6,
                            &hci_eventprocessor, 0);

    esp_vhci_host_register_callback(&vhci_host_cb);
    while (continue_commands) {
      if (continue_commands && esp_vhci_host_check_send_available()) {
        switch (cmd_cnt) {
          case 0:
            hci_cmd_send_reset();
            ++cmd_cnt;
            break;
          case 1:
            hci_cmd_send_set_evt_mask();
            ++cmd_cnt;
            break;

          /* Send advertising commands. */
          case 2:
            hci_cmd_send_ble_set_adv_param();
            ++cmd_cnt;
            break;
          case 3:
            hci_cmd_send_ble_set_adv_data();
            ++cmd_cnt;
            break;
          case 4:
            hci_cmd_send_ble_adv_start();
            ++cmd_cnt;
            break;

          /* Send scan commands. */
          case 5:
            hci_cmd_send_ble_scan_params();
            ++cmd_cnt;
            break;
          case 6:
            hci_cmd_send_ble_scan_start();
            ++cmd_cnt;
            break;
          default:
            continue_commands = 0;
            break;
        }
        ESP_LOGI(TAG, "BLE Advertise, cmd_sent: %d", cmd_cnt);
      }
      vTaskDelay(1000 / portTICK_RATE_MS);
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
  vTaskDelete(hci_eventprocessor);
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
    ESP_ERROR_CHECK(esp_coex_preference_set(ESP_COEX_PREFER_WIFI));
    ESP_LOGI(TAG, "Bluetooth scanner stopped");
    initialized_ble = 0;
  }
#endif
}  // stop_BLEscan

void set_BLE_rssi_filter(int set_rssi_threshold) {
  ble_rssi_threshold = set_rssi_threshold;
}
