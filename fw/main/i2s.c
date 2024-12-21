#include "main.h"

#include "freertos/FreeRTOS.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "driver/i2s_std.h"

static const char *TAG = "I2S";

static i2s_chan_handle_t rx_chan;

void i2s_init()
{
  i2s_chan_config_t rx_chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(
    I2S_NUM_AUTO,
    PIN_I2S_IS_MASTER ? I2S_ROLE_MASTER : I2S_ROLE_SLAVE);
  ESP_ERROR_CHECK(i2s_new_channel(&rx_chan_cfg, NULL, &rx_chan));

  i2s_std_config_t rx_std_cfg = {
    // ESP-SR requires 16 kHz sample rate
    .clk_cfg  = I2S_STD_CLK_DEFAULT_CONFIG(16000),
    // INMP441 outputs 32-bit data
    .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_32BIT, I2S_SLOT_MODE_MONO),
    // Set pins according to hardware setup
    .gpio_cfg = {
      .mclk = I2S_GPIO_UNUSED,
      .dout = I2S_GPIO_UNUSED,
      .bclk = PIN_I2S_IS_MASTER ? PIN_I2S_BCK : PIN_I2S_BCK_PROBE,
      .ws   = PIN_I2S_IS_MASTER ? PIN_I2S_WS  : PIN_I2S_WS_PROBE,
      .din  = PIN_I2S_DIN,

      .invert_flags = {
        .mclk_inv = false,
        .bclk_inv = false,
        .ws_inv   = false,
      },
    },
  };
  // Set according to INMP441's L/R pin (low - left, high - right)
  rx_std_cfg.slot_cfg.slot_mask = (PIN_I2S_LR ? I2S_STD_SLOT_RIGHT : I2S_STD_SLOT_LEFT);
  ESP_ERROR_CHECK(i2s_channel_init_std_mode(rx_chan, &rx_std_cfg));

#if 0
  gpio_set_pull_mode(GPIO_NUM_17, GPIO_PULLDOWN_ONLY);
#else
  gpio_set_pull_mode(GPIO_NUM_13, GPIO_PULLDOWN_ONLY);
  gpio_config(&(gpio_config_t){
    .pin_bit_mask = (1 << 15),
    .mode = GPIO_MODE_OUTPUT,
  });
  gpio_set_level(15, 1);
#endif

  ESP_LOGI(TAG, "I2S input initialised");
}

esp_err_t i2s_enable()
{
  return i2s_channel_enable(rx_chan);
}

esp_err_t i2s_read(int32_t *buf, size_t *n, size_t max_n)
{
  size_t read_bytes;
  esp_err_t result = i2s_channel_read(rx_chan, buf + *n, (max_n - *n) * sizeof(int32_t), &read_bytes, 1000);
  if (result != ESP_OK) return result;
  *n += read_bytes / sizeof(int32_t);
  return ESP_OK;
}
