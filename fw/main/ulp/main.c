#include <stdint.h>
#include <stdbool.h>
#include "ulp_riscv.h"
#include "ulp_riscv_utils.h"
#include "ulp_riscv_gpio.h"

#include "../pins.h"

// Must be used. Otherwise optimized out.
// https://esp32.com/viewtopic.php?p=81491&sid=ec28339a89796fb78962d628e859bd76#p81491
uint32_t wakeup_count = 0;
uint32_t wakeup_signal = 0;

uint32_t c0 = 0, c1 = 0, c2 = 0;
uint32_t debug[16];

#pragma GCC push_options
#pragma GCC optimize("O3")
uint32_t read()
{
  uint32_t b[16];

  // Wait for WS falling edge
  while ((REG_READ(RTC_GPIO_IN_REG) & (1 << (10 + PIN_I2S_WS_PROBE))) == 0) { }
  while (((b[0] = REG_READ(RTC_GPIO_IN_REG)) & (1 << (10 + PIN_I2S_WS_PROBE))) != 0) { }
  // for (int i = 0; i < 65; i++) REG_READ(RTC_GPIO_IN_REG);  // Adjusted time

  // uint32_t t = ULP_RISCV_GET_CCOUNT();

  #pragma GCC unroll 16
  for (int i = 1; i < 16; i++)
    b[i] = REG_READ(RTC_GPIO_IN_REG);

  // c1 = ULP_RISCV_GET_CCOUNT() - t;

  uint32_t x = 0;
  for (int i = 0; i < 16; i++)
    x |= (b[i]) << (15 - i);

  for (int i = 0; i < 16; i++)
    debug[i] = b[i] >> 10;

  return x;
}
#pragma GCC pop_options

int main()
{
  wakeup_signal = wakeup_count = c0 = c1 = c2 = 0;
  ulp_riscv_gpio_init(PIN_I2S_BCK_PROBE);
  ulp_riscv_gpio_init(PIN_I2S_WS_PROBE);
  ulp_riscv_gpio_init(PIN_I2S_DIN);
  uint32_t t = ULP_RISCV_GET_CCOUNT();
  while (1) {
    t += 1000 * 20000;
    while (ULP_RISCV_GET_CCOUNT() - t >= 0x80000000) { }
    c0 = read();
    wakeup_count++;
    wakeup_signal = 1;
    ulp_riscv_wakeup_main_processor();
  }
}
