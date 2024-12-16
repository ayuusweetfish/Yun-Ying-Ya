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

  // REG_SET_FIELD(RTC_CNTL_CLK_CONF_REG, RTC_CNTL_FAST_CLK_RTC_SEL, 0);
  rtc_clk_fast_src_set(SOC_RTC_FAST_CLK_SRC_XTAL_D2);
  ESP_LOGI(TAG, "RTC_CNTL_FAST_CLK_RTC_SEL: %u", (unsigned)REG_GET_FIELD(RTC_CNTL_CLK_CONF_REG, RTC_CNTL_FAST_CLK_RTC_SEL));

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
    // ESP_LOGI(TAG, "%08" PRIx32 ", %" PRId32, ulp_c0, ulp_c1);
    char s[17];
    for (int i = 0; i < 16; i++) s[i] = '0' + ((ulp_c0 >> (15 - i)) & 1);
    s[16] = '\0';
    ESP_LOGI(TAG, "read: %s, cycles: %" PRId32, s, ulp_c1);
    // Direct:
    // 16 bits:  159 cycles (0.56 us / bit 🎉)
    // Unrolled `b = (b << 1) | ((REG_READ(RTC_GPIO_IN_REG) >> 19) & 1);`
    // 32 bits:  819 cycles
    // 64 bits: 1652 cycles
/*
00000014 <read>:
  14:   c0002873                rdcycle a6
  18:   67a9                    lui     a5,0xa
  1a:   4247a683                lw      a3,1060(a5) # a424 <RTCCNTL+0x2424>
  1e:   4247a603                lw      a2,1060(a5)
  22:   4247a703                lw      a4,1060(a5)
  26:   82c9                    srl     a3,a3,0x12
  28:   824d                    srl     a2,a2,0x13
  2a:   4247a583                lw      a1,1060(a5)
  2e:   8a05                    and     a2,a2,1
  30:   8a89                    and     a3,a3,2
  32:   8ed1                    or      a3,a3,a2
  34:   834d                    srl     a4,a4,0x13
  36:   4247a603                lw      a2,1060(a5)
  3a:   0686                    sll     a3,a3,0x1
  3c:   8b05                    and     a4,a4,1
  3e:   8f55                    or      a4,a4,a3
  40:   81cd                    srl     a1,a1,0x13
  42:   4247a683                lw      a3,1060(a5)
  46:   0706                    sll     a4,a4,0x1
  48:   8985                    and     a1,a1,1
  4a:   8dd9                    or      a1,a1,a4
  4c:   824d                    srl     a2,a2,0x13
  4e:   4247a703                lw      a4,1060(a5)
  52:   0586                    sll     a1,a1,0x1
  54:   8a05                    and     a2,a2,1
  56:   8e4d                    or      a2,a2,a1
  58:   82cd                    srl     a3,a3,0x13
  5a:   4247a583                lw      a1,1060(a5)
  5e:   0606                    sll     a2,a2,0x1
  60:   8a85                    and     a3,a3,1
  62:   8ed1                    or      a3,a3,a2
  64:   834d                    srl     a4,a4,0x13
  66:   4247a603                lw      a2,1060(a5)
  6a:   0686                    sll     a3,a3,0x1
  6c:   8b05                    and     a4,a4,1
  6e:   8f55                    or      a4,a4,a3
  70:   81cd                    srl     a1,a1,0x13
  72:   4247a683                lw      a3,1060(a5)
  76:   0706                    sll     a4,a4,0x1
  78:   8985                    and     a1,a1,1
  7a:   8dd9                    or      a1,a1,a4
  7c:   824d                    srl     a2,a2,0x13
  7e:   4247a703                lw      a4,1060(a5)
  82:   0586                    sll     a1,a1,0x1
  84:   8a05                    and     a2,a2,1
  86:   8e4d                    or      a2,a2,a1
  88:   82cd                    srl     a3,a3,0x13
  8a:   4247a583                lw      a1,1060(a5)
  8e:   0606                    sll     a2,a2,0x1
  90:   8a85                    and     a3,a3,1
  92:   8ed1                    or      a3,a3,a2
  94:   834d                    srl     a4,a4,0x13
  96:   4247a603                lw      a2,1060(a5)
  9a:   0686                    sll     a3,a3,0x1
  9c:   8b05                    and     a4,a4,1
  9e:   8f55                    or      a4,a4,a3
  a0:   81cd                    srl     a1,a1,0x13
  a2:   4247a683                lw      a3,1060(a5)
  a6:   0706                    sll     a4,a4,0x1
  a8:   8985                    and     a1,a1,1
  aa:   8dd9                    or      a1,a1,a4
  ac:   824d                    srl     a2,a2,0x13
  ae:   4247a703                lw      a4,1060(a5)
  b2:   0586                    sll     a1,a1,0x1
  b4:   8a05                    and     a2,a2,1
  b6:   8e4d                    or      a2,a2,a1
  b8:   82cd                    srl     a3,a3,0x13
  ba:   4247a583                lw      a1,1060(a5)
  be:   0606                    sll     a2,a2,0x1
  c0:   8a85                    and     a3,a3,1
  c2:   8ed1                    or      a3,a3,a2
  c4:   834d                    srl     a4,a4,0x13
  c6:   4247a603                lw      a2,1060(a5)
  ca:   0686                    sll     a3,a3,0x1
  cc:   8b05                    and     a4,a4,1
  ce:   8f55                    or      a4,a4,a3
  d0:   81cd                    srl     a1,a1,0x13
  d2:   4247a683                lw      a3,1060(a5)
  d6:   0706                    sll     a4,a4,0x1
  d8:   8985                    and     a1,a1,1
  da:   8dd9                    or      a1,a1,a4
  dc:   824d                    srl     a2,a2,0x13
  de:   4247a703                lw      a4,1060(a5)
  e2:   0586                    sll     a1,a1,0x1
  e4:   8a05                    and     a2,a2,1
  e6:   8e4d                    or      a2,a2,a1
  e8:   82cd                    srl     a3,a3,0x13
  ea:   4247a583                lw      a1,1060(a5)
  ee:   0606                    sll     a2,a2,0x1
  f0:   8a85                    and     a3,a3,1
  f2:   8ed1                    or      a3,a3,a2
  f4:   834d                    srl     a4,a4,0x13
  f6:   4247a603                lw      a2,1060(a5)
  fa:   0686                    sll     a3,a3,0x1
  fc:   8b05                    and     a4,a4,1
  fe:   8f55                    or      a4,a4,a3
 100:   81cd                    srl     a1,a1,0x13
 102:   4247a683                lw      a3,1060(a5)
 106:   0706                    sll     a4,a4,0x1
 108:   8985                    and     a1,a1,1
 10a:   8dd9                    or      a1,a1,a4
 10c:   824d                    srl     a2,a2,0x13
 10e:   4247a703                lw      a4,1060(a5)
 112:   0586                    sll     a1,a1,0x1
 114:   8a05                    and     a2,a2,1
 116:   8e4d                    or      a2,a2,a1
 118:   82cd                    srl     a3,a3,0x13
 11a:   4247a583                lw      a1,1060(a5)
 11e:   0606                    sll     a2,a2,0x1
 120:   8a85                    and     a3,a3,1
 122:   8ed1                    or      a3,a3,a2
 124:   834d                    srl     a4,a4,0x13
 126:   4247a603                lw      a2,1060(a5)
 12a:   0686                    sll     a3,a3,0x1
 12c:   8b05                    and     a4,a4,1
 12e:   8f55                    or      a4,a4,a3
 130:   81cd                    srl     a1,a1,0x13
 132:   4247a683                lw      a3,1060(a5)
 136:   0706                    sll     a4,a4,0x1
 138:   8985                    and     a1,a1,1
 13a:   8dd9                    or      a1,a1,a4
 13c:   824d                    srl     a2,a2,0x13
 13e:   4247a703                lw      a4,1060(a5)
 142:   0586                    sll     a1,a1,0x1
 144:   8a05                    and     a2,a2,1
 146:   8e4d                    or      a2,a2,a1
 148:   82cd                    srl     a3,a3,0x13
 14a:   4247a583                lw      a1,1060(a5)
 14e:   0606                    sll     a2,a2,0x1
 150:   8a85                    and     a3,a3,1
 152:   8ed1                    or      a3,a3,a2
 154:   834d                    srl     a4,a4,0x13
 156:   4247a603                lw      a2,1060(a5)
 15a:   0686                    sll     a3,a3,0x1
 15c:   8b05                    and     a4,a4,1
 15e:   8f55                    or      a4,a4,a3
 160:   81cd                    srl     a1,a1,0x13
 162:   4247a683                lw      a3,1060(a5)
 166:   0706                    sll     a4,a4,0x1
 168:   8985                    and     a1,a1,1
 16a:   8dd9                    or      a1,a1,a4
 16c:   824d                    srl     a2,a2,0x13
 16e:   4247a703                lw      a4,1060(a5)
 172:   0586                    sll     a1,a1,0x1
 174:   8a05                    and     a2,a2,1
 176:   8e4d                    or      a2,a2,a1
 178:   82cd                    srl     a3,a3,0x13
 17a:   0606                    sll     a2,a2,0x1
 17c:   8a85                    and     a3,a3,1
 17e:   8ed1                    or      a3,a3,a2
 180:   834d                    srl     a4,a4,0x13
 182:   0686                    sll     a3,a3,0x1
 184:   8b05                    and     a4,a4,1
 186:   8f55                    or      a4,a4,a3
 188:   0706                    sll     a4,a4,0x1
 18a:   4247a503                lw      a0,1060(a5)
 18e:   c00027f3                rdcycle a5
 192:   814d                    srl     a0,a0,0x13
 194:   410787b3                sub     a5,a5,a6
 198:   8905                    and     a0,a0,1
 19a:   26f02023                sw      a5,608(zero) # 260 <c1>
 19e:   8d59                    or      a0,a0,a4
 1a0:   8082                    ret
*/
    // Unrolled `REG_READ(RTC_GPIO_IN_REG);`
    // 32 bits: 329 cycles
/*
00000014 <read>:
  14:   c0002773                rdcycle a4
  18:   67a9                    lui     a5,0xa
  1a:   4247a683                lw      a3,1060(a5) # a424 <RTCCNTL+0x2424>
  1e:   4247a683                lw      a3,1060(a5)
  22:   4247a683                lw      a3,1060(a5)
  26:   4247a683                lw      a3,1060(a5)
  2a:   4247a683                lw      a3,1060(a5)
  2e:   4247a683                lw      a3,1060(a5)
  32:   4247a683                lw      a3,1060(a5)
  36:   4247a683                lw      a3,1060(a5)
  3a:   4247a683                lw      a3,1060(a5)
  3e:   4247a683                lw      a3,1060(a5)
  42:   4247a683                lw      a3,1060(a5)
  46:   4247a683                lw      a3,1060(a5)
  4a:   4247a683                lw      a3,1060(a5)
  4e:   4247a683                lw      a3,1060(a5)
  52:   4247a683                lw      a3,1060(a5)
  56:   4247a683                lw      a3,1060(a5)
  5a:   4247a683                lw      a3,1060(a5)
  5e:   4247a683                lw      a3,1060(a5)
  62:   4247a683                lw      a3,1060(a5)
  66:   4247a683                lw      a3,1060(a5)
  6a:   4247a683                lw      a3,1060(a5)
  6e:   4247a683                lw      a3,1060(a5)
  72:   4247a683                lw      a3,1060(a5)
  76:   4247a683                lw      a3,1060(a5)
  7a:   4247a683                lw      a3,1060(a5)
  7e:   4247a683                lw      a3,1060(a5)
  82:   4247a683                lw      a3,1060(a5)
  86:   4247a683                lw      a3,1060(a5)
  8a:   4247a683                lw      a3,1060(a5)
  8e:   42478793                add     a5,a5,1060
  92:   4394                    lw      a3,0(a5)
  94:   4394                    lw      a3,0(a5)
  96:   439c                    lw      a5,0(a5)
  98:   c00027f3                rdcycle a5
  9c:   8f99                    sub     a5,a5,a4
  9e:   16f02223                sw      a5,356(zero) # 164 <c1>
  a2:   4501                    li      a0,0
  a4:   8082                    ret
*/

    uint32_t rtc_clk_period = rtc_clk_cal(RTC_CAL_8MD256, 100);
    uint32_t rtc_fast_hz = 1000000ULL * (1 << RTC_CLK_CAL_FRACT) * 256 / rtc_clk_period;
    ESP_LOGI(TAG, "RC_FAST_CLK = %" PRIu32 " Hz", rtc_fast_hz);

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
