#include <stdint.h>
#include <stdbool.h>
#include "ulp_riscv.h"
#include "ulp_riscv_utils.h"
#include "ulp_riscv_gpio.h"

// Must be used. Otherwise optimized out.
// https://esp32.com/viewtopic.php?p=81491&sid=ec28339a89796fb78962d628e859bd76#p81491
uint32_t wakeup_count = 0;
uint32_t wakeup_signal = 0;

uint32_t c0 = 0, c1 = 0;

#pragma GCC push_options
#pragma GCC optimize("O3")
uint32_t read()
{
  uint32_t b = 0;
  uint32_t t = ULP_RISCV_GET_CCOUNT();
/*
  #pragma GCC unroll 32
  for (int i = 0; i < 32; i++)
    b = (b << 1) | ((REG_READ(RTC_GPIO_IN_REG) >> 19) & 1);
*/

  uint32_t b0, b1, b2, b3, b4, b5, b6, b7, b8, b9, b10, b11, b12, b13, b14, b15;
// for i in range(16): print('b%-2d = REG_READ(RTC_GPIO_IN_REG);' % i)
  b0  = REG_READ(RTC_GPIO_IN_REG);
  b1  = REG_READ(RTC_GPIO_IN_REG);
  b2  = REG_READ(RTC_GPIO_IN_REG);
  b3  = REG_READ(RTC_GPIO_IN_REG);
  b4  = REG_READ(RTC_GPIO_IN_REG);
  b5  = REG_READ(RTC_GPIO_IN_REG);
  b6  = REG_READ(RTC_GPIO_IN_REG);
  b7  = REG_READ(RTC_GPIO_IN_REG);
  b8  = REG_READ(RTC_GPIO_IN_REG);
  b9  = REG_READ(RTC_GPIO_IN_REG);
  b10 = REG_READ(RTC_GPIO_IN_REG);
  b11 = REG_READ(RTC_GPIO_IN_REG);
  b12 = REG_READ(RTC_GPIO_IN_REG);
  b13 = REG_READ(RTC_GPIO_IN_REG);
  b14 = REG_READ(RTC_GPIO_IN_REG);
  b15 = REG_READ(RTC_GPIO_IN_REG);

  c1 = ULP_RISCV_GET_CCOUNT() - t;

// for i in range(16): print('(((b%-2d >> 19) & 1) << %2d) |' % (i, 15 - i))
  b = (
    (((b0  >> 19) & 1) << 15) |
    (((b1  >> 19) & 1) << 14) |
    (((b2  >> 19) & 1) << 13) |
    (((b3  >> 19) & 1) << 12) |
    (((b4  >> 19) & 1) << 11) |
    (((b5  >> 19) & 1) << 10) |
    (((b6  >> 19) & 1) <<  9) |
    (((b7  >> 19) & 1) <<  8) |
    (((b8  >> 19) & 1) <<  7) |
    (((b9  >> 19) & 1) <<  6) |
    (((b10 >> 19) & 1) <<  5) |
    (((b11 >> 19) & 1) <<  4) |
    (((b12 >> 19) & 1) <<  3) |
    (((b13 >> 19) & 1) <<  2) |
    (((b14 >> 19) & 1) <<  1) |
    (((b15 >> 19) & 1) <<  0) |
  0);
  return b;

/*
  uint32_t b0, b1, b2, b3, b4, b5, b6, b7;
  b0 = REG_READ(RTC_GPIO_IN_REG) & (1 << 19);
  b1 = REG_READ(RTC_GPIO_IN_REG) & (1 << 19);
  b2 = REG_READ(RTC_GPIO_IN_REG) & (1 << 19);
  b3 = REG_READ(RTC_GPIO_IN_REG) & (1 << 19);
  b4 = REG_READ(RTC_GPIO_IN_REG) & (1 << 19);
  b5 = REG_READ(RTC_GPIO_IN_REG) & (1 << 19);
  b6 = REG_READ(RTC_GPIO_IN_REG) & (1 << 19);
  b7 = REG_READ(RTC_GPIO_IN_REG) & (1 << 19);
  return
    (((uint32_t)(!!b0)) << 0) |
    (((uint32_t)(!!b1)) << 1) |
    (((uint32_t)(!!b2)) << 2) |
    (((uint32_t)(!!b3)) << 3) |
    (((uint32_t)(!!b4)) << 4) |
    (((uint32_t)(!!b5)) << 5) |
    (((uint32_t)(!!b6)) << 6) |
    (((uint32_t)(!!b7)) << 7);
*/
}
#pragma GCC pop_options

int main()
{
  wakeup_signal = wakeup_count = c0 = c1 = 0;
  ulp_riscv_gpio_init(GPIO_NUM_9);
  // ulp_riscv_gpio_init(GPIO_NUM_8);
  while (1) {
    ulp_riscv_delay_cycles(1000 * 20000);
    c0 = read();
    wakeup_count++;
    wakeup_signal = 1;
    ulp_riscv_wakeup_main_processor();
  }
}
