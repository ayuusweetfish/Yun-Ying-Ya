#include <stdint.h>
#include <stdbool.h>
#include "ulp_riscv.h"
#include "ulp_riscv_utils.h"
#include "ulp_riscv_gpio.h"

// Must be used. Otherwise optimized out.
// https://esp32.com/viewtopic.php?p=81491&sid=ec28339a89796fb78962d628e859bd76#p81491
__attribute__ ((used)) uint32_t wakeup_count = 0;

int main()
{
  ulp_riscv_gpio_init(GPIO_NUM_9);
  while (1) {
    for (int i = 0; i < 5; i++) {
      ulp_riscv_gpio_output_level(GPIO_NUM_9, 1);
      ulp_riscv_delay_cycles(1000 * ULP_RISCV_CYCLES_PER_MS);
      ulp_riscv_gpio_output_level(GPIO_NUM_9, 0);
      ulp_riscv_delay_cycles(1000 * ULP_RISCV_CYCLES_PER_MS);
    }
    wakeup_count++;
    ulp_riscv_wakeup_main_processor();
  }
}
