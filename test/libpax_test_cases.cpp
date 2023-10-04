#include <libpax.h>
#include <libpax_api.h>
#include <limits.h>
#include <string.h>
#include "unity.h"

/*
  Tests if both of the last two bytes of the MAC are used in the counting
  algorithm.
*/
void test_mac_add_bytes() {
  libpax_counter_reset();
  uint8_t test_mac_addr[6] = {0x0b, 0x01, 0x0, 0x0, 0x0, 0x0};
  test_mac_addr[4] = 0x01;
  test_mac_addr[5] = 0x01;
  mac_add(test_mac_addr, MAC_SNIFF_WIFI);
  test_mac_addr[4] = 0x02;
  test_mac_addr[5] = 0x01;
  mac_add(test_mac_addr, MAC_SNIFF_WIFI);
  TEST_ASSERT_EQUAL(2, libpax_wifi_counter_count());

  libpax_counter_reset();
  TEST_ASSERT_EQUAL(0, libpax_wifi_counter_count());
  test_mac_addr[4] = 0x01;
  test_mac_addr[5] = 0x01;
  mac_add(test_mac_addr, MAC_SNIFF_WIFI);
  test_mac_addr[4] = 0x01;
  test_mac_addr[5] = 0x02;
  mac_add(test_mac_addr, MAC_SNIFF_WIFI);
  TEST_ASSERT_EQUAL(2, libpax_wifi_counter_count());
}

/* test the function  libpax_counter_add_mac
1. add 100 diffrent mac addresses, the count should increase
2. add 100 same mac addresses, the count should not increase
 */

void test_collision_add() {
  libpax_counter_reset();
  uint8_t test_mac_addr[6];
  test_mac_addr[0] = 0x0b;
  test_mac_addr[1] = 0x10;

  uint16_t *test_mac_addr_p = (uint16_t *)(test_mac_addr + 4);
  *test_mac_addr_p = 1;
  for (int i = 0; i < 1000; i++) {
    int count_start = libpax_wifi_counter_count();
    mac_add(test_mac_addr, MAC_SNIFF_WIFI);
    TEST_ASSERT_EQUAL(1, libpax_wifi_counter_count() - count_start);
    *test_mac_addr_p += 1;
  }

  ESP_LOGI("testing", "Collision tests starts ###");
  *test_mac_addr_p = 1;
  for (int i = 0; i < 1000; i++) {
    int count_start = libpax_wifi_counter_count();
    mac_add(test_mac_addr, MAC_SNIFF_WIFI);
    TEST_ASSERT_EQUAL(libpax_wifi_counter_count(), count_start);
    *test_mac_addr_p += 1;
  }
}

/* test the function  libpax_counter_reset()
1. when count >= 0, reset should worked
2. when count >0 (add 100 diffrent mac addresses),reset should worked
 */
void test_counter_reset() {
  libpax_counter_reset();
  TEST_ASSERT_EQUAL(0, libpax_wifi_counter_count());

  uint8_t test_mac_addr[6] = {0x0b, 0x01, 1, 1, 1, 1};
  mac_add(test_mac_addr, MAC_SNIFF_WIFI);
  TEST_ASSERT_EQUAL(1, libpax_wifi_counter_count());

  libpax_counter_reset();
  TEST_ASSERT_EQUAL(0, libpax_wifi_counter_count());
}

/*
configuration test
*/
void test_config_store() {
  // ensure that the public api advertises the same size for the serialized
  // config size
  TEST_ASSERT_EQUAL(sizeof(struct libpax_config_storage_t), LIBPAX_CONFIG_SIZE);

  struct libpax_config_t configuration;
  struct libpax_config_t current_config;
  libpax_default_config(&configuration);
  libpax_update_config(&configuration);
  libpax_get_current_config(&current_config);
  TEST_ASSERT_EQUAL(0, memcmp(&configuration, &current_config,
                              sizeof(struct libpax_config_t)));
  current_config.wifi_channel_map = 0b101;
  TEST_ASSERT_NOT_EQUAL(0, memcmp(&configuration, &current_config,
                                  sizeof(struct libpax_config_t)));
  struct libpax_config_t read_configuration;
  char configuration_memory[LIBPAX_CONFIG_SIZE];
  
  // Test if memory for two serialize runs is the same
  char configuration_memory_second_run[LIBPAX_CONFIG_SIZE];
  libpax_serialize_config(configuration_memory, &current_config);
  libpax_serialize_config(configuration_memory_second_run, &current_config);
  TEST_ASSERT_EQUAL(0, memcmp(configuration_memory, configuration_memory_second_run, LIBPAX_CONFIG_SIZE));

  // Test if memory in spare area is zeroed
  uint8_t spare_cmp[23];
  memset(spare_cmp, 0, sizeof(spare_cmp));
  TEST_ASSERT_EQUAL(0, memcmp(((libpax_config_storage_t*)configuration_memory)->pad, spare_cmp, sizeof(spare_cmp)));

  TEST_ASSERT_EQUAL(
      0, libpax_deserialize_config(configuration_memory, &read_configuration));
  TEST_ASSERT_EQUAL(0, memcmp(&current_config, &read_configuration,
                              sizeof(struct libpax_config_t)));
}

struct count_payload_t count_from_libpax;
int time_called_back = 0;

void process_count(void) {
  time_called_back++;
  printf("pax: %lu; %lu; %lu;\n", count_from_libpax.pax,
         count_from_libpax.wifi_count, count_from_libpax.ble_count);
}

void test_callback() {
  time_called_back = 0;

  TickType_t xLastWakeTime = xTaskGetTickCount();
  int err_code = libpax_counter_start();  // note: this is a blocking call
  printf("after start: Current free heap: %d\n",
         heap_caps_get_free_size(MALLOC_CAP_8BIT));
  TEST_ASSERT_EQUAL(0, err_code);
  printf("libpax should be running\n");

  vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(6010));
  TEST_ASSERT_EQUAL(6, time_called_back);
}

void test_stop() {
  time_called_back = 0;

  int err_code = libpax_counter_stop();
  printf("after stop: Current free heap: %d\n",
         heap_caps_get_free_size(MALLOC_CAP_8BIT));
  TEST_ASSERT_EQUAL(0, err_code);
  printf("libpax should be stopped\n");
  vTaskDelay(pdMS_TO_TICKS(1000));
  TEST_ASSERT_EQUAL(0, time_called_back);
}

/*
integration test
*/
void test_integration() {
  struct libpax_config_t configuration;
  libpax_default_config(&configuration);

  // only wificounter is active
  configuration.blecounter = 1;
  configuration.blescantime = 0;  // infinit
  configuration.wificounter = 1;
  configuration.wifi_channel_map = WIFI_CHANNEL_ALL;
  configuration.wifi_channel_switch_interval = 50;
  configuration.wifi_rssi_threshold = 0;
  libpax_update_config(&configuration);

  // internal processing initialization
  int err_code = libpax_counter_init(process_count, &count_from_libpax, 1, 1);
  TEST_ASSERT_EQUAL(0, err_code);
  test_callback();
  test_stop();

  struct libpax_config_t gotConfiguration;

  libpax_get_current_config(&gotConfiguration);
  gotConfiguration.blecounter = 0;
  gotConfiguration.wificounter = 0;
  libpax_update_config(&gotConfiguration);
  err_code = libpax_counter_init(process_count, &count_from_libpax, 1, 1);
  TEST_ASSERT_EQUAL(0, err_code);
  test_callback();
  test_stop();

  libpax_get_current_config(&gotConfiguration);
  gotConfiguration.blecounter = 0;
  gotConfiguration.wificounter = 1;
  libpax_update_config(&gotConfiguration);
  err_code = libpax_counter_init(process_count, &count_from_libpax, 1, 1);
  TEST_ASSERT_EQUAL(0, err_code);
  test_callback();
  test_stop();

  libpax_get_current_config(&gotConfiguration);
  gotConfiguration.blecounter = 1;
  gotConfiguration.wificounter = 0;
  libpax_update_config(&gotConfiguration);
  err_code = libpax_counter_init(process_count, &count_from_libpax, 1, 1);
  TEST_ASSERT_EQUAL(0, err_code);
  test_callback();
  test_stop();

  libpax_get_current_config(&gotConfiguration);
  gotConfiguration.blecounter = 0;
  gotConfiguration.wificounter = 1;
  libpax_update_config(&gotConfiguration);
  err_code = libpax_counter_init(process_count, &count_from_libpax, 1, 1);
  TEST_ASSERT_EQUAL(0, err_code);
  test_callback();
  test_stop();

  libpax_get_current_config(&gotConfiguration);
  gotConfiguration.blecounter = 1;
  gotConfiguration.wificounter = 1;
  libpax_update_config(&gotConfiguration);
  err_code = libpax_counter_init(process_count, &count_from_libpax, 1, 1);
  TEST_ASSERT_EQUAL(0, err_code);
  test_callback();
  test_stop();
}

void test_dual_start_with_ble() {
    struct libpax_config_t configuration;
    libpax_default_config(&configuration);
    // only blecounter is active
    configuration.blecounter = 1;
    configuration.wificounter = 0;
    libpax_update_config(&configuration);
    int err_code = libpax_counter_init(process_count, &count_from_libpax, 1, 1);
    TEST_ASSERT_EQUAL(0, err_code);
    err_code = libpax_counter_start(); 
    TEST_ASSERT_EQUAL(0, err_code);
    // Should not crash the firmware
    err_code = libpax_counter_start();
    TEST_ASSERT_EQUAL(-1, err_code);
    err_code = libpax_counter_stop();
    TEST_ASSERT_EQUAL(0, err_code);
  }

void test_no_unusual_reset() {
    const soc_reset_reason_t reason = esp_rom_get_reset_reason(0);
    TEST_ASSERT_MESSAGE(reason != RESET_REASON_CPU0_SW, "Should not be software reset (lib crash?)");
}

int run_tests() {
  UNITY_BEGIN();

  RUN_TEST(test_no_unusual_reset);

  const soc_reset_reason_t reason = esp_rom_get_reset_reason(0);

  if (reason == RESET_REASON_CPU0_SW) {
    return UNITY_END();
  } 

  RUN_TEST(test_mac_add_bytes);
  RUN_TEST(test_collision_add);
  RUN_TEST(test_counter_reset);
  RUN_TEST(test_config_store);
  RUN_TEST(test_dual_start_with_ble);
  RUN_TEST(test_integration);

  return UNITY_END();
}

#ifdef LIBPAX_ARDUINO
void setup() { run_tests(); }
void loop() {}
#endif
#ifdef LIBPAX_ESPIDF
extern "C" void app_main();
void app_main() { run_tests(); }
#endif
