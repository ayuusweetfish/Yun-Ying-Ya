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
#include "driver/gpio.h"
#include "soc/gpio_reg.h"
#include "soc/rtc.h"

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
    .max_freq_mhz =  80,
    .min_freq_mhz =  10,
    .light_sleep_enable = true,
  }));

  // Battery fuel gauge
if (0) {
  while (!gauge_init()) {
    ESP_LOGI(TAG, "Battery fuel gauge not found. Retrying.");
    vTaskDelay(1000 / portTICK_PERIOD_MS);
  }
  while (1) {
    gauge_wake();
    vTaskDelay(1000 / portTICK_PERIOD_MS);
    struct gauge_state s = gauge_query();
    ESP_LOGI(TAG, "VCELL = %" PRIu32 " uV, fuel = %" PRIu32 ", discharge = %" PRIu32,
      s.voltage, s.fuel, s.discharge);
    gauge_sleep();
    vTaskDelay(1000 / portTICK_PERIOD_MS);
  }
}
  bool gauge_valid = gauge_init();
  if (gauge_valid) {
    gauge_wake();
    struct gauge_state s = gauge_query();
    ESP_LOGI(TAG, "VCELL = %" PRIu32 " uV, fuel = %" PRIu32 ", discharge = %" PRIu32,
      s.voltage, s.fuel, s.discharge);
    gauge_sleep();
  } else {
    ESP_LOGI(TAG, "Battery fuel gauge not found. Ignoring.");
  }

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

  // I2S input
  i2s_init();

  // Audio processing
  audio_init();

  rtc_clk_fast_src_set(SOC_RTC_FAST_CLK_SRC_XTAL_D2);

  rtc_gpio_init(PIN_I2S_BCK_PROBE);
  rtc_gpio_set_direction(PIN_I2S_BCK_PROBE, RTC_GPIO_MODE_INPUT_ONLY);
  rtc_gpio_set_direction_in_sleep(PIN_I2S_BCK_PROBE, RTC_GPIO_MODE_INPUT_ONLY);

  rtc_gpio_init(PIN_I2S_WS_PROBE);
  rtc_gpio_set_direction(PIN_I2S_WS_PROBE, RTC_GPIO_MODE_INPUT_ONLY);
  rtc_gpio_set_direction_in_sleep(PIN_I2S_WS_PROBE, RTC_GPIO_MODE_INPUT_ONLY);

  rtc_gpio_init(PIN_I2S_DIN);
  rtc_gpio_set_direction(PIN_I2S_DIN, RTC_GPIO_MODE_INPUT_ONLY);
  rtc_gpio_set_direction_in_sleep(PIN_I2S_DIN, RTC_GPIO_MODE_INPUT_ONLY);
  rtc_gpio_pulldown_en(PIN_I2S_DIN);

if (0) {
  gpio_config(&(gpio_config_t){
    .pin_bit_mask = (1 << PIN_I2S_BCK_PROBE) | (1 << PIN_I2S_WS_PROBE) | (1 << PIN_I2S_DIN),
    .mode = GPIO_MODE_INPUT,
    .pull_down_en = GPIO_PULLDOWN_ENABLE,
  });
  while (1) {
    const int N = 128;
    uint32_t in[N];
    // Wait for WS rising edge
    while ((REG_READ(GPIO_IN_REG) & (1 << PIN_I2S_WS_PROBE)) != 0) { }
    while ((REG_READ(GPIO_IN_REG) & (1 << PIN_I2S_WS_PROBE)) == 0) { }
    // Count 28 BCK pulses
    for (int i = 0; i < 28; i++) {
      while ((REG_READ(GPIO_IN_REG) & (1 << PIN_I2S_BCK_PROBE)) == 0) { }
      while ((REG_READ(GPIO_IN_REG) & (1 << PIN_I2S_BCK_PROBE)) != 0) { }
    }
    for (int i = 0; i < N; i++) in[i] = REG_READ(GPIO_IN_REG);
    char s[N + 1]; s[N] = '\0';
    for (int i = 0; i < N; i++) s[i] = '0' + ((in[i] >> PIN_I2S_BCK_PROBE) & 1);
    ESP_LOGI(TAG, "BCK: %s", s);
    for (int i = 0; i < N; i++) s[i] = '0' + ((in[i] >> PIN_I2S_WS_PROBE) & 1);
    ESP_LOGI(TAG, " WS: %s", s);
    for (int i = 0; i < N; i++) s[i] = '0' + ((in[i] >> PIN_I2S_DIN) & 1);
    ESP_LOGI(TAG, "DIN: %s", s);
    ESP_LOGI(TAG, "");
    for (int i = 0; i < 10; i++) vTaskDelay(1); // Prevent auto light sleep for a while
    vTaskDelay(1000 / portTICK_PERIOD_MS);
  }
}

  extern const uint8_t bin_start[] asm("_binary_ulp_duck_bin_start");
  extern const uint8_t bin_end[]   asm("_binary_ulp_duck_bin_end");
  ESP_ERROR_CHECK(ulp_riscv_load_binary(bin_start, bin_end - bin_start));
  ESP_ERROR_CHECK(ulp_riscv_run());

  esp_sleep_enable_ulp_wakeup();

  while (0) {
    static bool n_first = false; if (!n_first) { n_first = true; audio_pause(); }
    xSemaphoreTake(sem_ulp, portMAX_DELAY);
    ESP_LOGI(TAG, "Wake up: %" PRIu32 " %04" PRIx32 " %" PRIu32 " power=%10" PRIu32 " background=%10" PRIu32, ulp_wakeup_count, ulp_c0, ulp_c1, ulp_c2, ulp_c3);
    // ~800 cycles for 24 bits

    led_set_state(LED_STATE_CONN_CHECK, 200);
  /*
    int http_test_result = http_test();
    printf("HTTP test result: %d\n", http_test_result);
  */
    audio_resume();
    vTaskDelay(2000 / portTICK_PERIOD_MS);

    led_set_state(LED_STATE_IDLE, 200);
    audio_pause();
    vTaskDelay(200 / portTICK_PERIOD_MS);

    xQueueReset((QueueHandle_t)sem_ulp);
  }

  // Streaming POST request handle
  post_handle_t *p = post_create();

  enum {
    STATE_LISTEN,
    STATE_SPEECH,
  } state = STATE_LISTEN;
  int last_sent = 0;
  while (1) {
    vTaskDelay(100 / portTICK_PERIOD_MS);
    if (state == STATE_LISTEN) {
      if (audio_wake_state() != 0) {
        printf("Wake-up word detected\n");
        state = STATE_SPEECH;
        led_set_state(LED_STATE_SPEECH, 500);
        last_sent = 0;
        post_open(p);
      } else if (audio_can_sleep()) {
        ESP_LOGI(TAG, "Can sleep now!");
        audio_pause();
        ulp_wakeup_signal = 0;
        xQueueReset((QueueHandle_t)sem_ulp);
        xSemaphoreTake(sem_ulp, portMAX_DELAY);
        audio_clear_can_sleep();
        audio_push((const int32_t *)&ulp_audio_buf, 1024, (ulp_cur_buf_ptr - 768 + 1024) % 1024, 768);
        ESP_LOGI(TAG, "Resuming now!");
        audio_resume();
        continue;
      }
    }
    if (state == STATE_SPEECH) {
      bool is_ended = audio_speech_ended();
      int n = audio_speech_buffer_size();
      printf("Speech buffer size %d\n", n);
      // Send new samples to the server
      if (n > last_sent) {
        post_write(p, audio_speech_buffer() + last_sent, (n - last_sent) * sizeof(int16_t));
        last_sent = n;
      }
      if (is_ended) {
        state = STATE_LISTEN;
        const char *s = post_finish(p);
        printf("Result: %s\n", s != NULL ? s : "(null)");
        if (s != NULL && led_set_program(s)) {
          led_set_state(LED_STATE_RUN, 500);
          vTaskDelay(led_get_program_duration() / portTICK_PERIOD_MS);
          led_set_state(LED_STATE_IDLE, 2000);
        } else {
          led_set_state(LED_STATE_ERROR, 500);
          vTaskDelay(2000 / portTICK_PERIOD_MS);
          led_set_state(LED_STATE_IDLE, 500);
        }
        audio_resume();
        vTaskDelay(100 / portTICK_PERIOD_MS);
        audio_clear_wake_state();
        ESP_LOGI(TAG, "Finished running, resuming audio listen!");
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
