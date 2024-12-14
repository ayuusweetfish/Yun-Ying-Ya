#include "main.h"

#include <stdio.h>
#include <inttypes.h>
#include "sdkconfig.h"

#include <stdatomic.h>
#include "freertos/FreeRTOS.h"
#include "esp_chip_info.h"
#include "esp_clk_tree.h"
#include "esp_flash.h"
#include "esp_log.h"
#include "esp_pm.h"
#include "esp_sleep.h"
#include "nvs_flash.h"
#include "ulp_riscv.h"
#include "driver/rtc_io.h"

#include "ulp_duck.h"

#define TAG "main"

static void print_chip_info();

void app_main(void)
{
  print_chip_info();

  led_init();
  led_set_state(LED_STATE_STARTUP, 500);

  // `esp_pm/include/esp_pm.h`: Type is no longer implementation-specific
  ESP_ERROR_CHECK(esp_pm_configure(&(esp_pm_config_t){
    .max_freq_mhz = 160,
    .min_freq_mhz =  10,
    .light_sleep_enable = true,
  }));

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
  led_set_state(LED_STATE_CONN_CHECK, 500);
  int http_test_result = http_test();
  printf("HTTP test result: %d\n", http_test_result);
}
  led_set_state(LED_STATE_IDLE, 500);

  SemaphoreHandle_t sem_ulp;
  sem_ulp = xSemaphoreCreateBinary();
  assert(sem_ulp != NULL);

  static atomic_flag ulp_wake_flag = ATOMIC_FLAG_INIT;
  atomic_flag_test_and_set(&ulp_wake_flag);

  esp_err_t enter_cb(int64_t sleep_time_us, void *arg)
  {
    return ESP_OK;
  }

  esp_err_t exit_cb(int64_t sleep_time_us, void *arg)
  {
    // Sometimes misses!
    if (esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_ULP) {
    /*
      BaseType_t higher_prio_woken = pdFALSE;
      xSemaphoreGive(sem_ulp, &higher_prio_woken);
      if (higher_prio_woken != pdFALSE)
        portYIELD_FROM_ISR(higher_prio_woken);
    */
      atomic_flag_clear(&ulp_wake_flag);
    }
    return ESP_OK;
  }

  esp_pm_light_sleep_register_cbs(&(esp_pm_sleep_cbs_register_config_t){
    .enter_cb = enter_cb,
    .exit_cb = exit_cb,
  });

  rtc_gpio_init(GPIO_NUM_9);
  rtc_gpio_set_direction(GPIO_NUM_9, RTC_GPIO_MODE_OUTPUT_ONLY);
  rtc_gpio_set_direction_in_sleep(GPIO_NUM_9, RTC_GPIO_MODE_OUTPUT_ONLY);
  rtc_gpio_pullup_dis(GPIO_NUM_9);
  rtc_gpio_pulldown_dis(GPIO_NUM_9);
  extern const uint8_t bin_start[] asm("_binary_ulp_duck_bin_start");
  extern const uint8_t bin_end[]   asm("_binary_ulp_duck_bin_end");
  ESP_ERROR_CHECK(ulp_riscv_load_binary(bin_start, bin_end - bin_start));
  ESP_ERROR_CHECK(ulp_riscv_run());

  esp_sleep_enable_ulp_wakeup();

  while (1) {
    while (atomic_flag_test_and_set(&ulp_wake_flag))
      vTaskDelay(100 / portTICK_PERIOD_MS);
    // xSemaphoreTake(sem_ulp, portMAX_DELAY);
    printf("Wake up: %" PRId32 "\n", ulp_wakeup_count);
    led_set_state(LED_STATE_CONN_CHECK, 500);
    int http_test_result = http_test();
    printf("HTTP test result: %d\n", http_test_result);
    led_set_state(LED_STATE_IDLE, 500);
  }

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
      led_set_state(LED_STATE_SPEECH, 500);
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
        if (s != NULL && led_set_program(s)) {
          led_set_state(LED_STATE_RUN, 500);
          vTaskDelay(35000 / portTICK_PERIOD_MS);
          led_set_state(LED_STATE_IDLE, 2000);
        } else {
          led_set_state(LED_STATE_ERROR, 500);
          vTaskDelay(2000 / portTICK_PERIOD_MS);
          led_set_state(LED_STATE_IDLE, 500);
        }
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

  uint32_t cpu_freq;
  ESP_ERROR_CHECK(esp_clk_tree_src_get_freq_hz(SOC_MOD_CLK_CPU, 0, &cpu_freq));
  printf("CPU frequency: %" PRIu32 " Hz\n", cpu_freq);
}
