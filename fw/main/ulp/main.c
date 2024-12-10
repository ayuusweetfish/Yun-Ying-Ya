#include <stdint.h>
#include <stdbool.h>
#include "ulp_riscv.h"
#include "ulp_riscv_utils.h"
#include "ulp_riscv_gpio.h"

int main()
{
  ulp_riscv_gpio_init(GPIO_NUM_9);
  while (1) {
    ulp_riscv_gpio_output_level(GPIO_NUM_9, 1);
    ulp_riscv_delay_cycles(1000 * ULP_RISCV_CYCLES_PER_MS);
    ulp_riscv_gpio_output_level(GPIO_NUM_9, 0);
    ulp_riscv_delay_cycles(1000 * ULP_RISCV_CYCLES_PER_MS);
  }
}
