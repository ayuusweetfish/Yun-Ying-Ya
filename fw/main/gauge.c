#include "main.h"

#include "freertos/FreeRTOS.h"
#include "esp_log.h"
#include "driver/i2c_master.h"

static const uint8_t MAX17048_ADDR = 0b0110110;
static inline bool max17048_init();
static inline uint16_t max17048_read_reg(uint8_t reg);
static inline void max17048_write_reg(uint8_t reg, uint16_t value);

static i2c_master_bus_handle_t bus_handle = NULL;
static i2c_master_dev_handle_t device_handle = NULL;

bool gauge_init()
{
  if (bus_handle == NULL) {
    ESP_ERROR_CHECK(i2c_new_master_bus(&(i2c_master_bus_config_t){
      .i2c_port = I2C_NUM_0,
      .sda_io_num = PIN_I2C_SDA,
      .scl_io_num = PIN_I2C_SCL,
      .clk_source = I2C_CLK_SRC_DEFAULT,
      .glitch_ignore_cnt = 7,
    }, &bus_handle));

    ESP_ERROR_CHECK(i2c_master_bus_add_device(bus_handle, &(i2c_device_config_t){
      .dev_addr_length = I2C_ADDR_BIT_LEN_7,
      .device_address = MAX17048_ADDR,
      .scl_speed_hz = 100000,
    }, &device_handle));
  }

  return max17048_init();
}

static inline bool max17048_init()
{
  vTaskDelay(10 / portTICK_PERIOD_MS);

  esp_err_t result = i2c_master_probe(bus_handle, MAX17048_ADDR, 1000);
  if (result == ESP_ERR_NOT_FOUND) return false;

  // MODE.EnSleep = 1
  max17048_write_reg(0x06, 0x2000);
  return true;
}
static inline uint16_t max17048_read_reg(uint8_t reg)
{
  uint8_t rx_buf[2];
  ESP_ERROR_CHECK(i2c_master_transmit_receive(device_handle, &reg, 1, rx_buf, 2, 1000));
  return ((uint16_t)rx_buf[0] << 8) | rx_buf[1];
}
static inline void max17048_write_reg(uint8_t reg, uint16_t value)
{
  uint8_t buf[3] = { reg, value >> 8, value & 0xff };
  ESP_ERROR_CHECK(i2c_master_transmit(device_handle, buf, 3, 1000));
}

struct gauge_state gauge_query()
{
  struct gauge_state s;
  // VCELL
  s.voltage = (uint32_t)max17048_read_reg(0x02) * 625 / 8;
  // SOC
  s.fuel = (uint32_t)max17048_read_reg(0x04) * 10000 / 256;
  // CRATE
  s.discharge = (uint32_t)max17048_read_reg(0x16) * 2080;
  return s;
}

void gauge_sleep()
{
  // CONFIG.SLEEP = 1
  max17048_write_reg(0x0C, 0x979C);
}

void gauge_wake()
{
  // CONFIG.SLEEP = 0
  max17048_write_reg(0x0C, 0x971C);
}
