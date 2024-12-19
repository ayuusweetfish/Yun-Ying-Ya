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

#pragma GCC push_options
#pragma GCC optimize("O3")
uint32_t read()
{
  uint32_t t = ULP_RISCV_GET_CCOUNT();

  uint32_t b[16];
  #pragma GCC unroll 16
  for (int i = 0; i < 16; i++)
    b[i] = (REG_READ(RTC_GPIO_IN_REG) >> (10 + PIN_I2S_BCK_PROBE)) & 1;

  c1 = ULP_RISCV_GET_CCOUNT() - t;

  uint32_t x = 0;
  for (int i = 0; i < 16; i++)
    x |= (b[i]) << (15 - i);

  return x;
}
#pragma GCC pop_options

int main()
{
  wakeup_signal = wakeup_count = c0 = c1 = c2 = 0;
  ulp_riscv_gpio_init(PIN_I2S_BCK_PROBE);
  uint32_t t = ULP_RISCV_GET_CCOUNT();
  while (1) {
    t += 1000 * 20000;
    while (ULP_RISCV_GET_CCOUNT() - t >= 0x80000000) { }
    c0 = read();
    wakeup_count++;
    wakeup_signal = 1;
    uint32_t t1, t2;
    asm volatile (
      "rdcycle %0\n"
      // (empty): 5
      // nop: 11
      // nop; nop: 13
      // addi %2, %2, 1: 11
      // addi %2, %2, 1; addi %2, %2, 1: 13
      "addi %2, %2, 1\n"
      "addi %2, %2, 1\n"
      "rdcycle %1\n"
      : "=r" (t1), "=r" (t2), "+r" (wakeup_count)
    );
    c2 = t2 - t1;
    ulp_riscv_wakeup_main_processor();
  }
}
