#include "main.h"

#include <stdio.h>
#include <inttypes.h>
#include "sdkconfig.h"

#include "freertos/FreeRTOS.h"
#include "esp_chip_info.h"
#include "esp_flash.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "nvs_flash.h"

#define TAG "main"

void app_main(void)
{
  printf("Hello world!\n");

  /* Print chip information */
  esp_chip_info_t chip_info;
  uint32_t flash_size;
  esp_chip_info(&chip_info);
  printf("This is %s chip with %d CPU core(s), %s%s%s%s, ",
         CONFIG_IDF_TARGET,
         chip_info.cores,
         (chip_info.features & CHIP_FEATURE_WIFI_BGN) ? "WiFi/" : "",
         (chip_info.features & CHIP_FEATURE_BT) ? "BT" : "",
         (chip_info.features & CHIP_FEATURE_BLE) ? "BLE" : "",
         (chip_info.features & CHIP_FEATURE_IEEE802154) ? ", 802.15.4 (Zigbee/Thread)" : "");

  unsigned major_rev = chip_info.revision / 100;
  unsigned minor_rev = chip_info.revision % 100;
  printf("silicon revision v%d.%d, ", major_rev, minor_rev);
  if (esp_flash_get_size(NULL, &flash_size) != ESP_OK) {
    printf("Get flash size failed");
    return;
  }

  printf("%" PRIu32 "MB %s flash\n", flash_size / (uint32_t)(1024 * 1024),
         (chip_info.features & CHIP_FEATURE_EMB_FLASH) ? "embedded" : "external");

  printf("Minimum free heap size: %" PRIu32 " bytes\n", esp_get_minimum_free_heap_size());

  uint8_t *psram_buf = heap_caps_malloc(1048576, MALLOC_CAP_SPIRAM);
  psram_buf[1000000] = 0xAA;
  printf("PSRAM buffer %p\n", psram_buf);
  ESP_LOG_BUFFER_HEX("main", psram_buf + 999980, 100);

  i2s_init();

  // Initialize NVS
  esp_err_t ret = nvs_flash_init();
  if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    ESP_ERROR_CHECK(nvs_flash_erase());
    ret = nvs_flash_init();
  }
  ESP_ERROR_CHECK(ret);

  ESP_LOGI(TAG, "ESP_WIFI_MODE_STA");
  wifi_init_sta();

/*
  Hello world!
  This is esp32s3 chip with 2 CPU core(s), WiFi/BLE, silicon revision v0.2, 8MB external flash
  Minimum free heap size: 389836 bytes
*/

  if (esp_reset_reason() == ESP_RST_POWERON) {
    ESP_LOGI(TAG, "Updating time from NVS");
    ESP_ERROR_CHECK(update_time_from_nvs());
  }

  const esp_timer_create_args_t nvs_update_timer_args = {
    .callback = (void *)&fetch_and_store_time_in_nvs,
  };

  esp_timer_handle_t nvs_update_timer;
  ESP_ERROR_CHECK(esp_timer_create(&nvs_update_timer_args, &nvs_update_timer));
  ESP_ERROR_CHECK(esp_timer_start_periodic(nvs_update_timer, 86400000000ULL));

  xTaskCreate(&http_get_task, "http_get_task", 4096, NULL, 5, NULL);

  while (1) {
    printf("Tick\n");
    vTaskDelay(1000 / portTICK_PERIOD_MS);
  }

  for (int i = 10; i >= 0; i--) {
    printf("Restarting in %d seconds...\n", i);
    vTaskDelay(1000 / portTICK_PERIOD_MS);
  }
  printf("Restarting now.\n");
  fflush(stdout);
  esp_restart();
}
