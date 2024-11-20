#include "main.h"

#include "freertos/FreeRTOS.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "driver/i2s_std.h"

static const char *TAG = "I2S";

static i2s_chan_handle_t rx_chan;

void i2s_example_read_task(void *_unused)
{
  static int32_t r_buf[1024];
  assert(r_buf);
  size_t r_bytes = 0;

  ESP_ERROR_CHECK(i2s_channel_enable(rx_chan));

  while (1) {
    if (i2s_channel_read(rx_chan, r_buf, sizeof r_buf / sizeof(int32_t), &r_bytes, 1000) == ESP_OK) {
      ESP_LOGI(TAG, "I2S read %zd bytes", r_bytes);
      ESP_LOGI(TAG, "%10"PRId32" %10"PRId32" %10"PRId32" %10"PRId32" %10"PRId32" %10"PRId32" %10"PRId32" %10"PRId32"",
        r_buf[0], r_buf[1], r_buf[2], r_buf[3], r_buf[4], r_buf[5], r_buf[6], r_buf[7]);
    } else {
      ESP_LOGI(TAG, "I2S read failed");
    }
    vTaskDelay(pdMS_TO_TICKS(200));
  }
  free(r_buf);
  vTaskDelete(NULL);
}

void i2s_init()
{
  i2s_chan_config_t rx_chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_AUTO, I2S_ROLE_MASTER);
  ESP_ERROR_CHECK(i2s_new_channel(&rx_chan_cfg, NULL, &rx_chan));

  i2s_std_config_t rx_std_cfg = {
    .clk_cfg  = I2S_STD_CLK_DEFAULT_CONFIG(44100),
    .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_32BIT, I2S_SLOT_MODE_MONO),
    .gpio_cfg = {
      .mclk = I2S_GPIO_UNUSED,
      .bclk = GPIO_NUM_15,
      .ws   = GPIO_NUM_16,
      .dout = I2S_GPIO_UNUSED,
      .din  = GPIO_NUM_17,
      .invert_flags = {
        .mclk_inv = false,
        .bclk_inv = false,
        .ws_inv   = false,
      },
    },
  };
  /* Default is only receiving left slot in mono mode,
   * update to right here to show how to change the default configuration */
  rx_std_cfg.slot_cfg.slot_mask = I2S_STD_SLOT_RIGHT;
  ESP_ERROR_CHECK(i2s_channel_init_std_mode(rx_chan, &rx_std_cfg));

  gpio_set_pull_mode(GPIO_NUM_17, GPIO_PULLDOWN_ONLY);

  xTaskCreate(i2s_example_read_task, "i2s_example_read_task", 4096, NULL, 5, NULL);
}
