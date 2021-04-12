#include <list>
#include <algorithm>
#include "esp_log.h"
#include "unity.h"
#include "freertos/FreeRTOS.h"
#include "freertos/timers.h"
#include "freertos/queue.h"

#include <libpax.h>

#include "libpax_benchmark.h"

#ifndef NO_BENCHMARK // only supported with arduino because of C++ code use

double mean(const std::list<double>& data) {
        return std::accumulate(data.begin(), data.end(), 0.0) / data.size();
}

double variance(const std::list<double>& data) {
        double xBar = mean(data);
        double sqSum = std::inner_product(data.begin(), data.end(), data.begin(), 0.0);
        return sqSum / data.size() - xBar * xBar;
}

double stDev(const std::list<double>& data) {
     return std::sqrt(variance(data));       
}

#define MACS_TO_INSERT 500
#define SUPPORTED_PACKAGAES_PER_SECOND 2000
#define ITERATIONS_PER_TIME_MEASURE 100
#define REDUNANTEN_TESTS 100
#define BURST_SICE 2
#define ROUND_ROBIN_SIMULATION
#define BURST_SIMULATION

int queue_lock = 0;
QueueHandle_t queue;
TaskHandle_t testProcessTask;
void process_queue(void *pvParameters){
  uint16_t dequeued_element;
  queue_lock = 1;
  while(true) {
    if (xQueueReceive(queue, &dequeued_element, 200) != pdTRUE) {
      if (queue_lock) {
        ESP_LOGE("queue", "Premature return from xQueueReceive() with no data!");
      }
      continue;
    }
    add_to_bucket(dequeued_element);
  }
}
 
#define QUEUE_SIZE 50
void queue_test_init() {
    queue = xQueueCreate(QUEUE_SIZE, sizeof(uint16_t));

    xTaskCreatePinnedToCore(process_queue,     // task function
                            "test_queue_process",   // name of task
                            2048,            // stack size of task
                            (void *)1,       // parameter of the task
                            1,               // priority of the task
                            &testProcessTask, // task handle
                            1);              // CPU core
}

int queue_test(uint16_t mac_to_add) {
    if(queue_lock) {
      if (xQueueSendToBack(queue, (void *)&mac_to_add, 10) !=
          pdPASS) {
            ESP_LOGW("queue", "Queue overflow!");
      }
    } else {
      ESP_LOGW("queue", "Write on not avalible queue!");
    }
    return 0;
}

void queue_test_deinit() {
    queue_lock = 0;
    vTaskDelay(pdMS_TO_TICKS(100));
    vTaskDelete(testProcessTask);
    vQueueDelete(queue);
}

void noop() {

}

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

int add_to_bucket_debug(uint16_t mac_part) {
  int ret = add_to_bucket(mac_part);
  if(ret) {
    new_insert += 1;
  } else {
    collision += 1;
  }
  seen += 1;
  return ret;
}

struct methodes_to_test_t {
  char name[16];
  void (*init)();
  int (*use)(uint16_t);
  void (*deinit)();
  void (*reset)();
};

methodes_to_test_t methodes_to_test[3] = {
  {
    "queue",
    queue_test_init,
    queue_test,
    queue_test_deinit,
    libpax_counter_reset
  },
  // {
  //   "default",
  //   noop,
  //   libpax_wifi_counter_add_mac,
  //   noop,
  //   libpax_wifi_counter_reset
  // },
  {
    "homebrew",
    reset_debug,
    add_to_bucket_debug,
    print_debug,
    reset_bucket
  }
};


/*
Benchmark test
*/
void test_benchmark() {
  double time_min;
  double time_max;
  double time_mean;
  double time_variance;
  double time_stDev;
  int64_t total_time_start;
  uint32_t mac_count_to_insert;
  for(int method_index = 0; method_index < 2; method_index++)
  {
    int16_t redundant_tests = REDUNANTEN_TESTS;
    std::list<double> times;
    int is_active = 0; 
    #ifdef ROUND_ROBIN_SIMULATION
      is_active  = 1;
    #endif
    printf("ROUND_ROBIN_SIMULATION; %d;\n", is_active);
    is_active  = 0;
    #ifdef BURST_SIMULATION
      is_active  = 1;
    #endif
    printf("BURST_SIMULATION; %d;\n", is_active);
    printf("method_name; macs_inserted; iterations_per_time_measure;\n");
    printf("%s; %d; %d;\n", methodes_to_test[method_index].name, MACS_TO_INSERT, ITERATIONS_PER_TIME_MEASURE);
    printf("Scale is in microseconds;\n");
    std::list<double> aprox_opertion_times_robin;
    std::list<double> aprox_opertion_times_burst;
    while(redundant_tests > 0) {
      int64_t start_time = 0;
      redundant_tests--;
      
      // run twice for collision simulation
      #ifdef ROUND_ROBIN_SIMULATION
      printf("ROUND_ROBIN SIMULATION:\n");
      times.clear();
      total_time_start = esp_timer_get_time();
      methodes_to_test[method_index].init();

      for(int run = 1; run <= 2; run++) {  
        double time_diff = 0;
        mac_count_to_insert = MACS_TO_INSERT * BURST_SICE;
        uint16_t test_mac_addr = (uint16_t)10000;

        start_time = esp_timer_get_time();
        while(mac_count_to_insert > 0) {
          if(mac_count_to_insert % ITERATIONS_PER_TIME_MEASURE == 0) {
            time_diff = esp_timer_get_time() - start_time;
            times.push_back(time_diff);
            start_time = esp_timer_get_time();
          }
          methodes_to_test[method_index].use(test_mac_addr);
          test_mac_addr += (uint16_t)1;
          mac_count_to_insert -= 1;
        }
      }
      printf("Algorithm time spent (for %d operations): %f;\n",
        MACS_TO_INSERT * BURST_SICE * 2, std::accumulate(times.begin(), times.end(), 0.0f));
      printf("Algorithm time aprx. per single operation: %f;\n",
        (std::accumulate(times.begin(), times.end(), 0.0f) / (MACS_TO_INSERT * BURST_SICE * 2)));
      aprox_opertion_times_robin.push_back((std::accumulate(times.begin(), times.end(), 0.0f) / (MACS_TO_INSERT * BURST_SICE * 2)));
      printf("Total time spent: %lld\n", esp_timer_get_time() - total_time_start);
      start_time = esp_timer_get_time();
      methodes_to_test[method_index].reset();
      printf("reset time: %lld;\n", esp_timer_get_time() - start_time);

      methodes_to_test[method_index].deinit();
      time_min = *std::min_element(times.begin(), times.end());
      time_max = *std::max_element(times.begin(), times.end());
      time_mean = mean(times);
      time_variance = variance(times);
      time_stDev = stDev(times);
      printf("time_min; time_max; time_mean; time_variance; time_stDev\n");
      printf("%f; %f; %f; %f; %f;\n", time_min, time_max, time_mean, time_variance, time_stDev);
      #endif

      #ifdef BURST_SIMULATION
      printf("BURST SIMULATION:\n");
      times.clear();
      long total_time_start = esp_timer_get_time();
      methodes_to_test[method_index].init();
      int64_t time_diff = 0;
      mac_count_to_insert = MACS_TO_INSERT;
      uint16_t test_mac_addr = (uint16_t)10000;

      start_time = esp_timer_get_time();
      while(mac_count_to_insert > 0) {
        if(mac_count_to_insert % ITERATIONS_PER_TIME_MEASURE == 0) {
          time_diff = esp_timer_get_time() - start_time;
          times.push_back(time_diff/BURST_SICE);
          start_time = esp_timer_get_time();
        }
        methodes_to_test[method_index].use(test_mac_addr);
        for(int i = 1; i < BURST_SICE * 2; i++) {  
          methodes_to_test[method_index].use(test_mac_addr);
        }
        test_mac_addr += (uint16_t)1;
        mac_count_to_insert -= 1;
      }
      printf("Algorithm time spent (for %d operations): %f;\n",
        MACS_TO_INSERT * BURST_SICE * 2, std::accumulate(times.begin(), times.end(), 0.0f));
      printf("Algorithm time aprx. per single operation: %f;\n",
        (std::accumulate(times.begin(), times.end(), 0.0f) / (MACS_TO_INSERT * BURST_SICE * 2)));
      aprox_opertion_times_burst.push_back((std::accumulate(times.begin(), times.end(), 0.0f) / (MACS_TO_INSERT * BURST_SICE * 2)));
      printf("Total time spent: %lld\n", esp_timer_get_time() - total_time_start);
      start_time = esp_timer_get_time();
      methodes_to_test[method_index].reset();
      printf("reset time: %lld;\n", esp_timer_get_time() - start_time);
      methodes_to_test[method_index].deinit();

      time_min = *std::min_element(times.begin(), times.end());
      time_max = *std::max_element(times.begin(), times.end());
      time_mean = mean(times);
      time_variance = variance(times);
      time_stDev = stDev(times);
      printf("time_min; time_max; time_mean; time_variance; time_stDev\n");
      printf("%f; %f; %f; %f; %f;\n", time_min, time_max, time_mean, time_variance, time_stDev);
      #endif      
    }
    printf("BURST OVERALL VARIANCE: %f\n", variance(aprox_opertion_times_burst));
    printf("ROUND ROBIN OVERALL VARIANCE: %f\n", variance(aprox_opertion_times_robin));
    // give time for print to apear
    vTaskDelay(pdMS_TO_TICKS(1000));
  }
}
#endif