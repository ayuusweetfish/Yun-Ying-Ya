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
uint32_t debug[32];

uint32_t audio_buf[256];

#pragma GCC push_options
#pragma GCC optimize("O3")
uint32_t read()
{
  uint32_t b[26];

  // Wait for WS falling edge
  // while ((REG_READ(RTC_GPIO_IN_REG) & (1 << (10 + PIN_I2S_WS_PROBE))) == 0) { }
  while ((REG_READ(RTC_GPIO_IN_REG) & (1 << (10 + PIN_I2S_WS_PROBE))) != 0) { }
  uint32_t t = ULP_RISCV_GET_CCOUNT();

#if 1
  uint32_t b0, b1, b2, b3, b4, b5, b6, b7, b8, b9, b10, b11,
           b12, b13, b14, b15, b16, b17, b18, b19, b20, b21, b22, b23,
           b24, b25, b26, b27, b28, b29, b30, b31;
  uint32_t addr;
// ', '.join('b%d' % i for i in range(20))
  __asm__ volatile (
    "lui %[addr], 0xa\n"  // Address: 0xa424 (main CPU 0x60008424)
    "lw %[b0], 0x424(%[addr])\n"
    "lw %[b1], 0x424(%[addr])\n"
    "lw %[b2], 0x424(%[addr])\n"
    "lw %[b3], 0x424(%[addr])\n"
    "lw %[b4], 0x424(%[addr])\n"
    "lw %[b5], 0x424(%[addr])\n"
    "lw %[b6], 0x424(%[addr])\n"
    "lw %[b7], 0x424(%[addr])\n"
    "lw %[b8], 0x424(%[addr])\n"
    "lw %[b9], 0x424(%[addr])\n"
    "lw %[b10], 0x424(%[addr])\n"
    "lw %[b11], 0x424(%[addr])\n"
    "lw %[b12], 0x424(%[addr])\n"
    "lw %[b13], 0x424(%[addr])\n"
    "lw %[b14], 0x424(%[addr])\n"
    "lw %[b15], 0x424(%[addr])\n"
    "lw %[b16], 0x424(%[addr])\n"
    "lw %[b17], 0x424(%[addr])\n"
    "lw %[b18], 0x424(%[addr])\n"
    "lw %[b19], 0x424(%[addr])\n"
    "lw %[b20], 0x424(%[addr])\n"
    "lw %[b21], 0x424(%[addr])\n"
    "lw %[b22], 0x424(%[addr])\n"
    "lw %[b23], 0x424(%[addr])\n"
    "lw %[b24], 0x424(%[addr])\n"
    "lw %[b25], 0x424(%[addr])\n"
// ''.join('"lw %%[b%d], 0x424(%%[addr])\\n"\n' % i for i in range(20))
    : [addr] "=&r" (addr)
     ,[b0] "=&r" (b0)
     ,[b1] "=&r" (b1)
     ,[b2] "=&r" (b2)
     ,[b3] "=&r" (b3)
     ,[b4] "=&r" (b4)
     ,[b5] "=&r" (b5)
     ,[b6] "=&r" (b6)
     ,[b7] "=&r" (b7)
     ,[b8] "=&r" (b8)
     ,[b9] "=&r" (b9)
     ,[b10] "=&r" (b10)
     ,[b11] "=&r" (b11)
     ,[b12] "=&r" (b12)
     ,[b13] "=&r" (b13)
     ,[b14] "=&r" (b14)
     ,[b15] "=&r" (b15)
     ,[b16] "=&r" (b16)
     ,[b17] "=&r" (b17)
     ,[b18] "=&r" (b18)
     ,[b19] "=&r" (b19)
     ,[b20] "=&r" (b20)
     ,[b21] "=&r" (b21)
     ,[b22] "=&r" (b22)
     ,[b23] "=&r" (b23)
     ,[b24] "=&r" (b24)
     ,[b25] "=&r" (b25)
// ''.join(' ,[b%d] "=&r" (b%d)\n' % (i, i) for i in range(20))
  );

  b[0] = b0;
  b[1] = b1;
  b[2] = b2;
  b[3] = b3;
  b[4] = b4;
  b[5] = b5;
  b[6] = b6;
  b[7] = b7;
  b[8] = b8;
  b[9] = b9;
  b[10] = b10;
  b[11] = b11;
  b[12] = b12;
  b[13] = b13;
  b[14] = b14;
  b[15] = b15;
  b[16] = b16;
  b[17] = b17;
  b[18] = b18;
  b[19] = b19;
  b[20] = b20;
  b[21] = b21;
  b[22] = b22;
  b[23] = b23;
  b[24] = b24;
  b[25] = b25;
// ''.join('b[%d] = b%d;\n' % (i, i) for i in range(20))

#else
  #pragma GCC unroll 16
  for (int i = 0; i < 16; i++)
    b[i] = REG_READ(RTC_GPIO_IN_REG);

#endif

  uint32_t x = 0;
  uint32_t n = 0;
  #pragma GCC unroll 26
  for (int i = 0; i < 26; i++) {
    if (
      ((b[i] >> (10 + PIN_I2S_BCK_PROBE)) & 1) &&
      (i == 0 || !((b[i - 1] >> (10 + PIN_I2S_BCK_PROBE)) & 1))
    ) {
      x = (x << 1) | ((b[i] >> (10 + PIN_I2S_DIN)) & 1);
      n++;
    }
  }
  x <<= (16 - n);

  c1 = ULP_RISCV_GET_CCOUNT() - t;

  return x;
}
#pragma GCC pop_options

int main()
{
  wakeup_signal = wakeup_count = c0 = c1 = c2 = debug[0] = 0;
  ulp_riscv_gpio_init(PIN_I2S_BCK_PROBE);
  ulp_riscv_gpio_init(PIN_I2S_WS_PROBE);
  ulp_riscv_gpio_init(PIN_I2S_DIN);
  uint32_t t = ULP_RISCV_GET_CCOUNT();
  while (0) {
    t += 4000 * 20000;
    while (ULP_RISCV_GET_CCOUNT() - t >= 0x80000000) { }
    c0 = read();
    wakeup_count++;
    wakeup_signal = 1;
    ulp_riscv_wakeup_main_processor();
  }

  // Wait for WS rising edge
  while ((REG_READ(RTC_GPIO_IN_REG) & (1 << (10 + PIN_I2S_WS_PROBE))) != 0) { }
  while ((REG_READ(RTC_GPIO_IN_REG) & (1 << (10 + PIN_I2S_WS_PROBE))) == 0) { }
  int block = 0;
  int successive = 0;
  while (1) {
    uint32_t rms = 0;
    for (int i = 0; i < 64; i++) {
      uint32_t sample = read();
      audio_buf[i] = sample;
      int32_t s16 = (int32_t)(int16_t)sample;
      rms += s16 * s16;
    }
    c0 = audio_buf[0];
    c2 = rms;
    if (rms >= 64 * 10000) {
      if (++successive >= 4) {
        if (successive >= 6) successive = 6;
        wakeup_count++;
        wakeup_signal = 1;
        ulp_riscv_wakeup_main_processor();
      }
    } else {
      successive -= 2;
      if (successive < 0) successive = 0;
    }
    block = (block + 128) % 256;
  }
}
