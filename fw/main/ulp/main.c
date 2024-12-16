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

void wait_second()
{
  for (int i = 0; i < 102400; i++) {
  /*
    while (ulp_riscv_gpio_get_level(GPIO_NUM_9) == 1) { }
    while (ulp_riscv_gpio_get_level(GPIO_NUM_9) == 0) { }
  */
    while ((REG_READ(RTC_GPIO_IN_REG) & (1 << 19)) == 0) { }
    while ((REG_READ(RTC_GPIO_IN_REG) & (1 << 19)) != 0) { }
  }
}
void wait_1m()
{
  for (int i = 0; i < 1000000; i++) {
  /*
    int r = ulp_riscv_gpio_get_level(GPIO_NUM_9);
    if (r == 0) c0++; else c1++;
  */
    REG_READ(RTC_GPIO_IN_REG);
  }
}

void blast()
{
  while (1) {
    REG_WRITE(RTC_GPIO_OUT_W1TS_REG, 1 << 19);
    REG_WRITE(RTC_GPIO_OUT_W1TC_REG, 1 << 19);
  }
}

int main()
{
  wakeup_signal = wakeup_count = c0 = c1 = 0;
  ulp_riscv_gpio_init(GPIO_NUM_9);
  // ulp_riscv_gpio_init(GPIO_NUM_8);
  // blast();
  while (1) {
    // wait_second();
    wait_1m();
    wakeup_count++;
    wakeup_signal = 1;
    ulp_riscv_wakeup_main_processor();
  }
}
