#include "main.h"

#include <stdio.h>
#include <inttypes.h>
#include "sdkconfig.h"

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
#include "soc/rtc.h"
#include "soc/rtc_cntl_reg.h"

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
if (0) {
  wifi_init_sta();
  led_set_state(LED_STATE_CONN_CHECK, 500);
  int http_test_result = http_test();
  printf("HTTP test result: %d\n", http_test_result);
}
  led_set_state(LED_STATE_IDLE, 500);

  // XXX: This does not work when allocated on the stack, for unknown reasons?
  static SemaphoreHandle_t sem_ulp;
  sem_ulp = xSemaphoreCreateBinary();
  assert(sem_ulp != NULL);

  esp_err_t exit_sleep_cb(int64_t sleep_time_us, void *arg)
  {
    if (ulp_wakeup_signal != 0) {
      ulp_wakeup_signal = 0;

      xSemaphoreGiveFromISR(sem_ulp, NULL);
      // Omitting the second argument and the manual yield does not affect
      // behaviour. The only caveat is that higher-priority tasks may not
      // be correctly preempting and instead wait for the next tick.
      // As this is called in a critical section, we minimise unsafe moves.
    }
    return ESP_OK;
  }

  esp_pm_light_sleep_register_cbs(&(esp_pm_sleep_cbs_register_config_t){
    .exit_cb = exit_sleep_cb,
  });

  ESP_LOGI(TAG, "RTC_CNTL_FAST_CLK_RTC_SEL: %u", (unsigned)REG_GET_FIELD(RTC_CNTL_CLK_CONF_REG, RTC_CNTL_FAST_CLK_RTC_SEL));
  rtc_clk_fast_src_set(SOC_RTC_FAST_CLK_SRC_XTAL_D2);

/*
  rtc_gpio_init(GPIO_NUM_8);
  rtc_gpio_set_direction(GPIO_NUM_8, RTC_GPIO_MODE_INPUT_OUTPUT);
  rtc_gpio_set_direction_in_sleep(GPIO_NUM_8, RTC_GPIO_MODE_INPUT_OUTPUT);
  rtc_gpio_pullup_en(GPIO_NUM_8);
  rtc_gpio_pulldown_dis(GPIO_NUM_8);
*/

  rtc_gpio_init(GPIO_NUM_9);
  rtc_gpio_set_direction(GPIO_NUM_9, RTC_GPIO_MODE_INPUT_ONLY);
  rtc_gpio_set_direction_in_sleep(GPIO_NUM_9, RTC_GPIO_MODE_INPUT_ONLY);
  rtc_gpio_pullup_dis(GPIO_NUM_9);
  rtc_gpio_pulldown_dis(GPIO_NUM_9);
  extern const uint8_t bin_start[] asm("_binary_ulp_duck_bin_start");
  extern const uint8_t bin_end[]   asm("_binary_ulp_duck_bin_end");
  ESP_ERROR_CHECK(ulp_riscv_load_binary(bin_start, bin_end - bin_start));
  ESP_ERROR_CHECK(ulp_riscv_run());
  ESP_LOGI(TAG, "RTC_CNTL_FAST_CLK_RTC_SEL: %u", (unsigned)REG_GET_FIELD(RTC_CNTL_CLK_CONF_REG, RTC_CNTL_FAST_CLK_RTC_SEL));

  esp_sleep_enable_ulp_wakeup();

  while (1) {
    xSemaphoreTake(sem_ulp, portMAX_DELAY);
    ESP_LOGI(TAG, "Wake up: %" PRId32, ulp_wakeup_count);
    char s[17];
    for (int i = 0; i < 16; i++) s[i] = '0' + ((ulp_c0 >> (15 - i)) & 1);
    s[16] = '\0';
    ESP_LOGI(TAG, "read: %s, cycles: %" PRIu32 ", total: %10" PRIu32, s, ulp_c1, ulp_c2);
    // Unrolled `REG_READ(RTC_GPIO_IN_REG)`:
    // 16 bits:  159 cycles (0.56 us / bit 🎉)
    // Unrolled `b = (b << 1) | ((REG_READ(RTC_GPIO_IN_REG) >> 19) & 1);`
    // 32 bits:  819 cycles
    // 64 bits: 1652 cycles

    led_set_state(LED_STATE_CONN_CHECK, 500);
  /*
    int http_test_result = http_test();
    printf("HTTP test result: %d\n", http_test_result);
  */
    vTaskDelay(500 / portTICK_PERIOD_MS);
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
