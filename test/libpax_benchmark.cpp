#include <algorithm>
#include <list>
#include <numeric>
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/timers.h"
#include "unity.h"

#include <libpax_api.h>

#ifndef NO_BENCHMARK

extern "C" void libpax_counter_reset();
extern "C" int add_to_bucket(uint16_t id);
extern "C" void reset_bucket();

double mean(const std::list<double>& data) {
  return std::accumulate(data.begin(), data.end(), 0.0) / data.size();
}

double variance(const std::list<double>& data) {
  double xBar = mean(data);
  double sqSum =
      std::inner_product(data.begin(), data.end(), data.begin(), 0.0);
  return sqSum / data.size() - xBar * xBar;
}

double stDev(const std::list<double>& data) {
  return std::sqrt(variance(data));
}

#define MACS_TO_INSERT 500
#define SUPPORTED_PACKAGAES_PER_SECOND 2000
#define ITERATIONS_PER_TIME_MEASURE 100
#define REDUNANTEN_TESTS 2
#define BURST_SICE 5
#define ROUND_ROBIN_SIMULATION
#define BURST_SIMULATION

int queue_lock = 0;
QueueHandle_t queue;
TaskHandle_t testProcessTask;
void process_queue(void* pvParameters) {
  uint16_t dequeued_element;
  queue_lock = 1;
  while (queue_lock) {
    if (xQueueReceive(queue, &dequeued_element, 10) != pdTRUE) {
      if (queue_lock) {
        ESP_LOGE("queue",
                 "Premature return from xQueueReceive() with no data!");
      }
      continue;
    }
    // libpax_wifi_counter_add_mac(dequeued_element);
  }
  vTaskDelete(testProcessTask);
}

#define QUEUE_SIZE 50
void queue_test_init() {
  queue = xQueueCreate(QUEUE_SIZE, sizeof(uint16_t));

  xTaskCreatePinnedToCore(process_queue,         // task function
                          "test_queue_process",  // name of task
                          2048,                  // stack size of task
                          (void*)1,              // parameter of the task
                          1,                     // priority of the task
                          &testProcessTask,      // task handle
                          1);                    // CPU core
}

int queue_test(uint16_t mac_to_add) {
  if (xQueueSendToBack(queue, (void*)&mac_to_add, 10) != pdPASS) {
    ESP_LOGW("queue", "Queue overflow!");
  }
  return 1;
}

void queue_test_deinit() {
  queue_lock = 0;
  vTaskDelay(pdMS_TO_TICKS(200));
  vQueueDelete(queue);
}

void noop() {}

int seen = 0;
int collision = 0;
int new_insert = 0;

void reset_debug() {
  seen = 0;
  collision = 0;
  new_insert = 0;
}

void print_debug() {
  printf("seen; collision; new_insert;\n");
  printf("%d;%d;%d;\n", seen, collision, new_insert);
}

struct methodes_to_test_t {
  char name[16];
  void (*init)();
  int (*use)(uint16_t);
  void (*deinit)();
  void (*reset)();
};

methodes_to_test_t methodes_to_test[3] = {
    {"queue", queue_test_init, queue_test, queue_test_deinit,
     libpax_counter_reset},
    //{"default", noop, libpax_wifi_counter_add_mac, noop,
    // libpax_wifi_counter_reset},
    {"homebrew", reset_debug, add_to_bucket, print_debug, reset_bucket}};

/*
Benchmark test
*/
TEST_CASE(
    "adding_macs should be fast enough to support 2000 packages per second",
    "[benchmark]") {
  for (int method_index = 0; method_index < 2; method_index++) {
    int16_t redundant_tests = REDUNANTEN_TESTS;
    std::list<double> times;
    int is_active = 0;
#ifdef ROUND_ROBIN_SIMULATION
    is_active = 1;
#endif
    printf("ROUND_ROBIN_SIMULATION; %d;\n", is_active);
    is_active = 0;
#ifdef BURST_SIMULATION
    is_active = 1;
#endif
    printf("BURST_SIMULATION; %d;\n", is_active);
    printf("method_name; macs_inserted; iterations_per_time_measure;\n");
    printf("%s; %d; %d;\n", methodes_to_test[method_index].name, MACS_TO_INSERT,
           ITERATIONS_PER_TIME_MEASURE);
    printf("Scale is in microseconds;\n");
    printf("time_min; time_max; time_mean; time_variance; time_stDev\n");
    while (redundant_tests > 0) {
      int64_t start_time = 0;
      redundant_tests--;
      methodes_to_test[method_index].init();
      times.clear();
      start_time = esp_timer_get_time();
      methodes_to_test[method_index].reset();
      printf("reset time;%lld;\n", esp_timer_get_time() - start_time);

// run twice for collision simulation
#ifdef ROUND_ROBIN_SIMULATION
      for (int run = 1; run <= 2; run++) {
        int64_t time_diff = 0;
        int16_t mac_count_to_insert = MACS_TO_INSERT;
        uint16_t test_mac_addr = (uint16_t)10000;

        start_time = esp_timer_get_time();
        while (mac_count_to_insert > 0) {
          if (mac_count_to_insert % ITERATIONS_PER_TIME_MEASURE == 0) {
            time_diff = esp_timer_get_time() - start_time;
            times.push_back(time_diff / ITERATIONS_PER_TIME_MEASURE);
            start_time = esp_timer_get_time();
          }
          methodes_to_test[method_index].use(test_mac_addr);
          test_mac_addr += (uint16_t)1;
          mac_count_to_insert -= 1;
        }
      }
#endif

#ifdef BURST_SIMULATION
      int64_t time_diff = 0;
      int16_t mac_count_to_insert = MACS_TO_INSERT;
      uint16_t test_mac_addr = (uint16_t)10000;

      start_time = esp_timer_get_time();
      methodes_to_test[method_index].reset();
      printf("reset time;%lld;\n", esp_timer_get_time() - start_time);
      start_time = esp_timer_get_time();
      while (mac_count_to_insert > 0) {
        if (mac_count_to_insert % ITERATIONS_PER_TIME_MEASURE == 0) {
          time_diff = esp_timer_get_time() - start_time;
          times.push_back(time_diff /
                          (ITERATIONS_PER_TIME_MEASURE * BURST_SICE));
          start_time = esp_timer_get_time();
        }
        methodes_to_test[method_index].use(test_mac_addr);
        for (int i = 0; i < BURST_SICE; i++)
          methodes_to_test[method_index].use(test_mac_addr);
        test_mac_addr += (uint16_t)1;
        mac_count_to_insert -= 1;
      }
#endif

      methodes_to_test[method_index].deinit();

      double time_min = *std::min_element(times.begin(), times.end());
      double time_max = *std::max_element(times.begin(), times.end());
      double time_mean = mean(times);
      double time_variance = variance(times);
      double time_stDev = stDev(times);

      printf("%f; %f; %f; %f; %f;\n", time_min, time_max, time_mean,
             time_variance, time_stDev);

      TEST_ASSERT_LESS_THAN(1000 * 1000 / SUPPORTED_PACKAGAES_PER_SECOND,
                            time_max);
    }
  }
}
#endif