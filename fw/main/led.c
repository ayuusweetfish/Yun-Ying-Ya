#include "main.h"

#include "freertos/FreeRTOS.h"
#include "esp_log.h"
#include "driver/gpio.h"

#include "led_strip.h"

static const char *TAG = "LED";
#define PIN_ONBOARD_LED GPIO_NUM_48
// Remember to close the solder bridge

static led_strip_handle_t led_strip;

void led_init()
{
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
  led_strip_set_pixel(led_strip, 0, 10, 6, 2);
  led_strip_refresh(led_strip);

  // XXX: Testing. Write to GPIO
  gpio_config(&(gpio_config_t){
    .pin_bit_mask = (1 << 17) | (1 << 18) | (1 << 21),
    .mode = GPIO_MODE_OUTPUT,
    .pull_up_en = GPIO_PULLUP_ENABLE,
  });
  gpio_set_level(17, 0);
  gpio_set_level(18, 0);
  gpio_set_level(21, 0);
}

void led_set_state(int state)
{
  if (state == 1) led_strip_set_pixel(led_strip, 0, 2, 6, 10);
  if (state == 2) led_strip_set_pixel(led_strip, 0, 2, 2, 2);
  if (state == 3) led_strip_set_pixel(led_strip, 0, 10, 6, 2);
  led_strip_refresh(led_strip);

  gpio_set_level(17, state == 1 ? 0 : 1);
  gpio_set_level(18, state == 2 ? 0 : 1);
  gpio_set_level(21, state == 3 ? 0 : 1);
}
