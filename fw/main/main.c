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

while (0) {
  vTaskDelay(pdMS_TO_TICKS(1000));
  led_set_state(LED_STATE_IDLE, 500);
  vTaskDelay(pdMS_TO_TICKS(1000));
  led_set_state(LED_STATE_STARTUP, 500);
}

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
if (0) {
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

if (1) {
  // Audio processing
  audio_init();

  // Audio is paused on startup
  audio_resume();
  audio_pause();
}

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

  vTaskDelay(pdMS_TO_TICKS(100));

  ulp_check_power = 1;

if (0) {
  ulp_check_power = 0;
  vTaskDelay(1000 / portTICK_PERIOD_MS);
  printf("Starting!\n");
  uint32_t last_push = *(volatile uint32_t *)&ulp_cur_buf_ptr;
  i2s_enable();

  int32_t *b = heap_caps_malloc(32000 * sizeof(int32_t), MALLOC_CAP_SPIRAM);
  size_t ptr_b = 0;

  int16_t *a = heap_caps_malloc(16000 * sizeof(int16_t), MALLOC_CAP_SPIRAM);
  uint32_t ptr = 0;

  while (1) {
    vTaskDelay(pdMS_TO_TICKS(20));
    uint32_t cur_push = *(volatile uint32_t *)&ulp_cur_buf_ptr;

    for (uint32_t i = last_push; i != cur_push; i = (i + 64) % 2048) {
      if (ptr_b < 32000) i2s_read(b, &ptr_b, ptr_b + 64 < 32000 ? ptr_b + 64 : 32000);
      for (uint32_t j = 0; j < 64; j++)
        a[ptr + j] = ((int16_t *)&ulp_audio_buf)[i + j];
      uint32_t power = 0;
      int16_t last_s16 = (int16_t)a[ptr];
      for (uint32_t j = 1; j < 64; j++) {
        int16_t s16 = (int16_t)a[ptr + j];
        int32_t s_diff = last_s16 - s16;
        power += (uint32_t)(s_diff * s_diff) / 64;
        last_s16 = s16;
      }
      printf("%8u power = %10u\n", (unsigned)ptr, (unsigned)power);
      ptr += 64;
      if (ptr == 32000) {
        printf("End! cycles = %u\n", (unsigned)ulp_c1);
        // Dump
        // ESP_LOG_BUFFER_HEX(TAG, a, sizeof a);
        putchar('\n');
        printf("== by ULP ==\n");
        for (int i = 0; i < 32000; i += 320) {
          for (int j = 0; j < 320; j++) printf("%04" PRIx16, a[i + j]);
          putchar('\n');
          vTaskDelay(1);
        }
        putchar('\n');
        printf("== by I2S ==\n");
        for (int i = 0; i < 32000; i += 320) {
          for (int j = 0; j < 320; j++) printf("%04" PRIx16, (int16_t)(b[i + j] >> 16));
          putchar('\n');
          vTaskDelay(1);
        }
        putchar('\n');
        for (int i = 0; i < 10; i++) vTaskDelay(1); // Prevent auto light sleep for a while
        vTaskDelay(2000 / portTICK_PERIOD_MS);
        printf("Starting!\n");
        last_push = *(volatile uint32_t *)&ulp_cur_buf_ptr;
        break;
      }
    }

    last_push = cur_push;
  }
}

  esp_sleep_enable_ulp_wakeup();

  while (0) {
    static bool n_first = false; if (!n_first) { n_first = true; audio_pause(); }
    xSemaphoreTake(sem_ulp, portMAX_DELAY);
    ESP_LOGI(TAG, "Wake up: %04" PRIx32 " %" PRIu32 " power=%10" PRIu32 " background=%10" PRIu32, ulp_c0, ulp_c1, ulp_c2, ulp_c3);
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

  ulp_check_power = 1;
  // ulp_check_power = 0;

  vTaskDelay(pdMS_TO_TICKS(100));
  for (int i = 0; i < 21; i++)
    printf("debuga %2u %3u\n", (unsigned)i, (unsigned)((&ulp_debuga)[i]));

  while (0) {
    bool waken = xSemaphoreTake(sem_ulp, 0);
    uint16_t sample = ulp_c1;
    sample = ((const int16_t *)&ulp_audio_buf)[(ulp_cur_buf_ptr - 1 + 2048) % 2048];
    ESP_LOGI(TAG, "Wake up: power=%10" PRIu32 " sample=%04" PRIx16 " count=%04" PRIu32 " cycles=%4" PRIu32 " %c", ulp_c2, sample, ulp_c0, ulp_c3, waken ? '*' : ' ');
    if (0) {
      static const int N = 10;
      uint32_t in[N];
      for (int i = 0; i < N; i++) in[i] = ((volatile uint32_t *)&ulp_debug)[i] >> 10;
      char s[N + 1]; s[N] = '\0';
      for (int i = 0; i < N; i++) s[i] = '0' + ((in[i] >> PIN_I2S_BCK_PROBE) & 1);
      ESP_LOGI(TAG, "BCK: %s", s);
      for (int i = 0; i < N; i++) s[i] = '0' + ((in[i] >> PIN_I2S_WS_PROBE) & 1);
      ESP_LOGI(TAG, " WS: %s", s);
      for (int i = 0; i < N; i++) s[i] = '0' + ((in[i] >> PIN_I2S_DIN) & 1);
      ESP_LOGI(TAG, "DIN: %s", s);
      ESP_LOGI(TAG, "");
    }
    led_set_state(waken ? LED_STATE_SIGNAL : LED_STATE_IDLE, 50);
    vTaskDelay(pdMS_TO_TICKS(100));
  }

  enum {
    STATE_LISTEN,
    STATE_SPEECH,
  } state = STATE_LISTEN;
  int last_sent = 0;
  uint32_t last_push = 0;
  void buf_push(uint32_t last_push, uint32_t cur_push)
  {
    if (last_push == cur_push) return;
    const int16_t *in_buf = (const int16_t *)&ulp_audio_buf;
    static int32_t buf[2048];
    uint32_t p = 0;
    for (uint32_t i = last_push; i != cur_push; i = (i + 1) % 2048)
      buf[p++] = in_buf[i];
    audio_push(buf, p);
  }
  while (1) {
    vTaskDelay(10 / portTICK_PERIOD_MS);
    if (state == STATE_LISTEN) {
      // Push data in
      uint32_t cur_push = *(volatile uint32_t *)&ulp_cur_buf_ptr;
      buf_push(last_push, cur_push);
      last_push = cur_push;
      if (audio_wake_state() != 0) {
        printf("Wake-up word detected\n");
        state = STATE_SPEECH;
        led_set_state(LED_STATE_SPEECH, 500);
        last_sent = 0;
        // post_open(p);
        audio_resume();
      } else if (audio_can_sleep()) {
        ESP_LOGI(TAG, "Can sleep now!");
        led_set_state(LED_STATE_IDLE, 50);
        audio_pause();
        ulp_wakeup_signal = 0;
        ulp_check_power = 1;
        xQueueReset((QueueHandle_t)sem_ulp);
        xSemaphoreTake(sem_ulp, portMAX_DELAY);
        ESP_LOGI(TAG, "Resuming now!");
        led_set_state(LED_STATE_SIGNAL, 50);
        last_push = *(volatile uint32_t *)&ulp_cur_buf_ptr;
        buf_push((last_push - 1536 + 2048) % 2048, last_push);
        audio_clear_can_sleep();
        ulp_check_power = 0;
        // audio_resume();
        continue;
      }
    }
    if (state == STATE_SPEECH) {
      bool is_ended = audio_speech_ended();
      int n = audio_speech_buffer_size();
      printf("Speech buffer size %d\n", n);
      // Send new samples to the server
      if (n > last_sent) {
        // post_write(p, audio_speech_buffer() + last_sent, (n - last_sent) * sizeof(int16_t));
        last_sent = n;
      }
      if (is_ended) {
        audio_pause();
        state = STATE_LISTEN;
        const char *s = NULL; // post_finish(p);
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
        ulp_check_power = 1;
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
