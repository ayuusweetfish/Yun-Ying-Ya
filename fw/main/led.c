#include "main.h"

#include "freertos/FreeRTOS.h"
#include "esp_log.h"

#define USE_LED_STRIP 1
#define USE_LED_PWM   1

static const char *TAG = "LED";
#define PIN_ONBOARD_LED GPIO_NUM_48
// Remember to close the solder bridge

#if USE_LED_STRIP
#include "led_strip.h"
static led_strip_handle_t led_strip;
#endif

#if USE_LED_PWM
#include "driver/ledc.h"
#include "driver/gpio.h"
#endif

static inline void output_tint(int r, int g, int b);

void led_init()
{
#if USE_LED_STRIP
  /// LED strip common configuration
  led_strip_config_t strip_config = {
    .strip_gpio_num = PIN_ONBOARD_LED,  // The GPIO that connected to the LED strip's data line
    .max_leds = 1,                 // The number of LEDs in the strip,
    .led_model = LED_MODEL_SK6812, // LED strip model, it determines the bit timing
    .color_component_format = LED_STRIP_COLOR_COMPONENT_FMT_GRB, // The color component format is G-R-B
    .flags = {
      .invert_out = false, // don't invert the output signal
    }
  };

  /// RMT backend specific configuration
  led_strip_rmt_config_t rmt_config = {
    .clk_src = RMT_CLK_SRC_DEFAULT,    // different clock source can lead to different power consumption
    .resolution_hz = 10 * 1000 * 1000, // RMT counter clock frequency: 10MHz
    .mem_block_symbols = 64,           // the memory size of each RMT channel, in words (4 bytes)
    .flags = {
      .with_dma = false,  // DMA feature is available on chips like ESP32-S3/P4
    }
  };

  /// Create the LED strip object
  ESP_ERROR_CHECK(led_strip_new_rmt_device(&strip_config, &rmt_config, &led_strip));

  ESP_LOGI(TAG, "Initialised LED with RMT backend");
#endif

#if USE_LED_PWM
if (1) {
  ESP_ERROR_CHECK(ledc_timer_config(&(ledc_timer_config_t){
    .speed_mode = LEDC_LOW_SPEED_MODE,
    .duty_resolution = LEDC_TIMER_13_BIT,
    .timer_num = LEDC_TIMER_0,
    .freq_hz = 4000,
    .clk_cfg = LEDC_AUTO_CLK,
  }));
  static const struct { ledc_channel_t ch; int pin; } channels[3] = {
    {LEDC_CHANNEL_0, 17},
    {LEDC_CHANNEL_1, 18},
    {LEDC_CHANNEL_2, 21},
  };
  for (int i = 0; i < 3; i++)
    ESP_ERROR_CHECK(ledc_channel_config(&(ledc_channel_config_t){
      .speed_mode = LEDC_LOW_SPEED_MODE,
      .channel = channels[i].ch,
      .timer_sel = LEDC_TIMER_0,
      .intr_type = LEDC_INTR_DISABLE,
      .gpio_num = channels[i].pin,
      .duty = 0,
      .hpoint = 0,
    }));
} else {
  gpio_config(&(gpio_config_t){
    .pin_bit_mask = (1 << 17) | (1 << 18) | (1 << 21),
    .mode = GPIO_MODE_OUTPUT,
    .pull_up_en = GPIO_PULLUP_ENABLE,
  });
}

  ESP_LOGI(TAG, "Initialised LED with PWM controller");
#endif

  output_tint(0, 0, 0);
}

static inline void output_tint(int r, int g, int b)
{
#if USE_LED_STRIP
  led_strip_set_pixel(led_strip, 0, r, g, b);
  led_strip_refresh(led_strip);
#endif

#if USE_LED_PWM
if (1) {
  ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, ((1 << 13) - 1) - (r << 5));
  ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_1, ((1 << 13) - 1) - (g << 5));
  ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_2, ((1 << 13) - 1) - (b << 5));
  ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);
  ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_1);
  ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_2);
} else {
  gpio_set_level(17, !(r >= 5));
  gpio_set_level(18, !(g >= 5));
  gpio_set_level(21, !(b >= 5));
}
#endif
}

void led_set_state(int state)
{
  if (state == 1) output_tint(2, 6, 10);
  if (state == 2) output_tint(2, 2, 2);
  if (state == 3) output_tint(10, 6, 2);
}
