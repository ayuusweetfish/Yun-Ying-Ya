#include "main.h"

#include <stdio.h>
#include <inttypes.h>
#include "sdkconfig.h"

#include "freertos/FreeRTOS.h"
#include "esp_chip_info.h"
#include "esp_flash.h"
#include "esp_log.h"
#include "nvs_flash.h"

#define TAG "main"

static void print_chip_info();

void app_main(void)
{
  print_chip_info();

  led_init();

  // NVS
  esp_err_t ret = nvs_flash_init();
  if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    ESP_ERROR_CHECK(nvs_flash_erase());
    ret = nvs_flash_init();
  }
  ESP_ERROR_CHECK(ret);

  // Wi-Fi
if (1) {
  wifi_init_sta();
  led_set_state(1);
  int http_test_result = http_test();
  printf("HTTP test result: %d\n", http_test_result);
}
  led_set_state(2);

  // I2S input
  i2s_init();

  // Audio processing
  audio_init();

  // Streaming POST request handle
  post_handle_t *p = post_create();

  enum {
    STATE_IDLE,
    STATE_SPEECH,
  } state = STATE_IDLE;
  int last_sent = 0;
  while (1) {
    vTaskDelay(100 / portTICK_PERIOD_MS);
    if (state == 0 && audio_wake_state() != 0) {
      printf("Wake-up word detected\n");
      state = STATE_SPEECH;
      last_sent = 0;
      post_open(p);
    }
    if (state == STATE_SPEECH) {
      int n = audio_speech_buffer_size();
      printf("Speech buffer size %d\n", n);
      // Send new samples to the server
      if (n > last_sent) {
        post_write(p, audio_speech_buffer() + last_sent, (n - last_sent) * sizeof(int16_t));
        last_sent = n;
      }
      if (audio_speech_ended()) {
        state = STATE_IDLE;
        const char *s = post_finish(p);
        printf("Result: %s\n", s != NULL ? s : "(null)");
        audio_clear_wake_state();
      }
    }
  }

  esp_restart();
}

void print_chip_info()
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
}
