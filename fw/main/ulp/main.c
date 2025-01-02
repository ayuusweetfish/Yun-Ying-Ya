#include <stdint.h>
#include <stdbool.h>
#include "ulp_riscv.h"
#include "ulp_riscv_utils.h"
#include "ulp_riscv_gpio.h"

#include "../pins.h"

// Must be used. Otherwise optimized out.
// https://esp32.com/viewtopic.php?p=81491&sid=ec28339a89796fb78962d628e859bd76#p81491
uint32_t wakeup_signal = 0;
volatile uint32_t check_power = 0;

uint32_t c0 = 0, c1 = 0, c2 = 0, c3 = 0;

uint16_t audio_buf[2048];
uint32_t cur_buf_ptr;

uint32_t debug[10];
// uint32_t debuga[32];

#define N_EDGES 64

uint32_t next_edge = 0;
uint32_t last_sample = 0;

#pragma GCC push_options
#pragma GCC optimize("O3")
static inline uint32_t read()
{
  return 0;
}

static inline uint32_t read_less()
{
  uint32_t b0, b1, b2, b3, b4, b5, b6, b7, b8, b9, b10, b11,
           b12, b13, b14, b15, b16, b17, b18, b19, b20, b21, b22, b23,
           b24, b25, b26, b27, b28, b29, b30, b31;

  uint32_t addr;
  __asm__ volatile (
    "lui %[addr], 0xa\n"  // Address: 0xa424 (main CPU 0x60008424)
    : [addr] "=r" (addr)
  );

/*
  // Wait for WS falling edge

  uint32_t scratch;
  __asm__ volatile (
    "1:"
    "lw %[b0], 0x424(%[addr])\n"
    "sll %[scratch], %[b0], %[shift_amt]\n"
    "bltz %[scratch], 1b\n"
    : [b0] "=&r" (b0)
     ,[scratch] "=&r" (scratch)
    : [addr] "r" (addr),
      [shift_amt] "i" (31 - (10 + PIN_I2S_AUX_PROBE))
  );
*/

  uint32_t cycles_start, cycles_end;
  uint32_t scratch;
  __asm__ volatile (
    ".p2align 2\n"
    ".option norvc\n"
    "1:"
    "rdcycle %[cycles_start]\n"
    "sub %[scratch], %[cycles_start], %[next_edge]\n"
    "blt %[scratch], zero, 1b\n"
    : [scratch] "=&r" (scratch),
      [cycles_start] "=&r" (cycles_start)
    : [next_edge] "r" (next_edge)
  );

  __asm__ volatile (
    ".p2align 2\n"
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
  /*
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
  */
    "rdcycle %[cycles_end]\n"
    ".option rvc\n"
// ''.join('"lw %%[b%d], 0x424(%%[addr])\\n"\n' % i for i in range(32))
    : [b0] "=&r" (b0)
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
    /*
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
    */
     ,[cycles_end] "=&r" (cycles_end)
    : [addr] "r" (addr)
// ''.join(' ,[b%d] "=&r" (b%d)\n' % (i, i) for i in range(32))
  );

  uint32_t x = 0;

if (cycles_end - cycles_start < 19 + 9 * 16 /* number of bits read */) {
  // This will give very rare glitches due to bus contention (0~5 per second),
  // but it should be very acceptable
  uint32_t n = 0;
uint32_t set = 0;
if (!((b0 >> (10 + PIN_I2S_WS_PROBE)) & 1) && !((b0 >> (10 + PIN_I2S_BCK_PROBE)) & 1)) { set++; }
if (set >= 2 && ((b1 >> (10 + PIN_I2S_BCK_PROBE)) & 1) && !((b0 >> (10 + PIN_I2S_BCK_PROBE)) & 1)) { x = (x << 1) | ((b1 >> (10 + PIN_I2S_DIN)) & 1); n++; }
if (!((b1 >> (10 + PIN_I2S_WS_PROBE)) & 1) && !((b1 >> (10 + PIN_I2S_BCK_PROBE)) & 1) && ((b0 >> (10 + PIN_I2S_BCK_PROBE)) & 1)) { set++; }
if (set >= 2 && ((b2 >> (10 + PIN_I2S_BCK_PROBE)) & 1) && !((b1 >> (10 + PIN_I2S_BCK_PROBE)) & 1)) { x = (x << 1) | ((b2 >> (10 + PIN_I2S_DIN)) & 1); n++; }
if (!((b2 >> (10 + PIN_I2S_WS_PROBE)) & 1) && !((b2 >> (10 + PIN_I2S_BCK_PROBE)) & 1) && ((b1 >> (10 + PIN_I2S_BCK_PROBE)) & 1)) { set++; }
if (set >= 2 && ((b3 >> (10 + PIN_I2S_BCK_PROBE)) & 1) && !((b2 >> (10 + PIN_I2S_BCK_PROBE)) & 1)) { x = (x << 1) | ((b3 >> (10 + PIN_I2S_DIN)) & 1); n++; }
if (!((b3 >> (10 + PIN_I2S_WS_PROBE)) & 1) && !((b3 >> (10 + PIN_I2S_BCK_PROBE)) & 1) && ((b2 >> (10 + PIN_I2S_BCK_PROBE)) & 1)) { set++; }
if (set >= 2 && ((b4 >> (10 + PIN_I2S_BCK_PROBE)) & 1) && !((b3 >> (10 + PIN_I2S_BCK_PROBE)) & 1)) { x = (x << 1) | ((b4 >> (10 + PIN_I2S_DIN)) & 1); n++; }
if (!((b4 >> (10 + PIN_I2S_WS_PROBE)) & 1) && !((b4 >> (10 + PIN_I2S_BCK_PROBE)) & 1) && ((b3 >> (10 + PIN_I2S_BCK_PROBE)) & 1)) { set++; }
if (set >= 2 && ((b5 >> (10 + PIN_I2S_BCK_PROBE)) & 1) && !((b4 >> (10 + PIN_I2S_BCK_PROBE)) & 1)) { x = (x << 1) | ((b5 >> (10 + PIN_I2S_DIN)) & 1); n++; }
if (((b6 >> (10 + PIN_I2S_BCK_PROBE)) & 1) && !((b5 >> (10 + PIN_I2S_BCK_PROBE)) & 1)) { x = (x << 1) | ((b6 >> (10 + PIN_I2S_DIN)) & 1); n++; }
if (((b7 >> (10 + PIN_I2S_BCK_PROBE)) & 1) && !((b6 >> (10 + PIN_I2S_BCK_PROBE)) & 1)) { x = (x << 1) | ((b7 >> (10 + PIN_I2S_DIN)) & 1); n++; }
if (((b8 >> (10 + PIN_I2S_BCK_PROBE)) & 1) && !((b7 >> (10 + PIN_I2S_BCK_PROBE)) & 1)) { x = (x << 1) | ((b8 >> (10 + PIN_I2S_DIN)) & 1); n++; }
if (((b9 >> (10 + PIN_I2S_BCK_PROBE)) & 1) && !((b8 >> (10 + PIN_I2S_BCK_PROBE)) & 1)) { x = (x << 1) | ((b9 >> (10 + PIN_I2S_DIN)) & 1); n++; }
if (((b10 >> (10 + PIN_I2S_BCK_PROBE)) & 1) && !((b9 >> (10 + PIN_I2S_BCK_PROBE)) & 1)) { x = (x << 1) | ((b10 >> (10 + PIN_I2S_DIN)) & 1); n++; }
if (((b11 >> (10 + PIN_I2S_BCK_PROBE)) & 1) && !((b10 >> (10 + PIN_I2S_BCK_PROBE)) & 1)) { x = (x << 1) | ((b11 >> (10 + PIN_I2S_DIN)) & 1); n++; }
if (((b12 >> (10 + PIN_I2S_BCK_PROBE)) & 1) && !((b11 >> (10 + PIN_I2S_BCK_PROBE)) & 1)) { x = (x << 1) | ((b12 >> (10 + PIN_I2S_DIN)) & 1); n++; }
if (((b13 >> (10 + PIN_I2S_BCK_PROBE)) & 1) && !((b12 >> (10 + PIN_I2S_BCK_PROBE)) & 1)) { x = (x << 1) | ((b13 >> (10 + PIN_I2S_DIN)) & 1); n++; }
if (((b14 >> (10 + PIN_I2S_BCK_PROBE)) & 1) && !((b13 >> (10 + PIN_I2S_BCK_PROBE)) & 1)) { x = (x << 1) | ((b14 >> (10 + PIN_I2S_DIN)) & 1); n++; }
if (((b15 >> (10 + PIN_I2S_BCK_PROBE)) & 1) && !((b14 >> (10 + PIN_I2S_BCK_PROBE)) & 1)) { x = (x << 1) | ((b15 >> (10 + PIN_I2S_DIN)) & 1); n++; }
/*
if (((b16 >> (10 + PIN_I2S_BCK_PROBE)) & 1) && !((b15 >> (10 + PIN_I2S_BCK_PROBE)) & 1)) { x = (x << 1) | ((b16 >> (10 + PIN_I2S_DIN)) & 1); n++; }
if (((b17 >> (10 + PIN_I2S_BCK_PROBE)) & 1) && !((b16 >> (10 + PIN_I2S_BCK_PROBE)) & 1)) { x = (x << 1) | ((b17 >> (10 + PIN_I2S_DIN)) & 1); n++; }
if (((b18 >> (10 + PIN_I2S_BCK_PROBE)) & 1) && !((b17 >> (10 + PIN_I2S_BCK_PROBE)) & 1)) { x = (x << 1) | ((b18 >> (10 + PIN_I2S_DIN)) & 1); n++; }
if (((b19 >> (10 + PIN_I2S_BCK_PROBE)) & 1) && !((b18 >> (10 + PIN_I2S_BCK_PROBE)) & 1)) { x = (x << 1) | ((b19 >> (10 + PIN_I2S_DIN)) & 1); n++; }
if (((b20 >> (10 + PIN_I2S_BCK_PROBE)) & 1) && !((b19 >> (10 + PIN_I2S_BCK_PROBE)) & 1)) { x = (x << 1) | ((b20 >> (10 + PIN_I2S_DIN)) & 1); n++; }
if (((b21 >> (10 + PIN_I2S_BCK_PROBE)) & 1) && !((b20 >> (10 + PIN_I2S_BCK_PROBE)) & 1)) { x = (x << 1) | ((b21 >> (10 + PIN_I2S_DIN)) & 1); n++; }
if (((b22 >> (10 + PIN_I2S_BCK_PROBE)) & 1) && !((b21 >> (10 + PIN_I2S_BCK_PROBE)) & 1)) { x = (x << 1) | ((b22 >> (10 + PIN_I2S_DIN)) & 1); n++; }
if (((b23 >> (10 + PIN_I2S_BCK_PROBE)) & 1) && !((b22 >> (10 + PIN_I2S_BCK_PROBE)) & 1)) { x = (x << 1) | ((b23 >> (10 + PIN_I2S_DIN)) & 1); n++; }
if (((b24 >> (10 + PIN_I2S_BCK_PROBE)) & 1) && !((b23 >> (10 + PIN_I2S_BCK_PROBE)) & 1)) { x = (x << 1) | ((b24 >> (10 + PIN_I2S_DIN)) & 1); n++; }
if (((b25 >> (10 + PIN_I2S_BCK_PROBE)) & 1) && !((b24 >> (10 + PIN_I2S_BCK_PROBE)) & 1)) { x = (x << 1) | ((b25 >> (10 + PIN_I2S_DIN)) & 1); n++; }
if (((b26 >> (10 + PIN_I2S_BCK_PROBE)) & 1) && !((b25 >> (10 + PIN_I2S_BCK_PROBE)) & 1)) { x = (x << 1) | ((b26 >> (10 + PIN_I2S_DIN)) & 1); n++; }
if (((b27 >> (10 + PIN_I2S_BCK_PROBE)) & 1) && !((b26 >> (10 + PIN_I2S_BCK_PROBE)) & 1)) { x = (x << 1) | ((b27 >> (10 + PIN_I2S_DIN)) & 1); n++; }
if (((b28 >> (10 + PIN_I2S_BCK_PROBE)) & 1) && !((b27 >> (10 + PIN_I2S_BCK_PROBE)) & 1)) { x = (x << 1) | ((b28 >> (10 + PIN_I2S_DIN)) & 1); n++; }
if (((b29 >> (10 + PIN_I2S_BCK_PROBE)) & 1) && !((b28 >> (10 + PIN_I2S_BCK_PROBE)) & 1)) { x = (x << 1) | ((b29 >> (10 + PIN_I2S_DIN)) & 1); n++; }
if (((b30 >> (10 + PIN_I2S_BCK_PROBE)) & 1) && !((b29 >> (10 + PIN_I2S_BCK_PROBE)) & 1)) { x = (x << 1) | ((b30 >> (10 + PIN_I2S_DIN)) & 1); n++; }
if (((b31 >> (10 + PIN_I2S_BCK_PROBE)) & 1) && !((b30 >> (10 + PIN_I2S_BCK_PROBE)) & 1)) { x = (x << 1) | ((b31 >> (10 + PIN_I2S_DIN)) & 1); n++; }
*/
  // ''.join('if (((b%d >> (10 + PIN_I2S_BCK_PROBE)) & 1) && !((b%d >> (10 + PIN_I2S_BCK_PROBE)) & 1)) { x = (x << 1) | ((b%d >> (10 + PIN_I2S_DIN)) & 1); n++; }\n' % (i, i - 1, i) for i in range(32))
  // ''.join('if ((b%d >> (10 + PIN_I2S_BCK_PROBE)) & 1) { x = (x << 1) | ((b%d >> (10 + PIN_I2S_DIN)) & 1); n++; }\n' % (i, i) for i in range(32))

  x <<= (16 - n);
  x &= 0xffff;
  last_sample = x;
} else {
  x = last_sample;
}

if (1 || x != 0x8000) {
  debug[0] = b0;
  debug[1] = b1;
  debug[2] = b2;
  debug[3] = b3;
  debug[4] = b4;
  debug[5] = b5;
  debug[6] = b6;
  debug[7] = b7;
  debug[8] = b8;
  debug[9] = b9;
/*
  debug[10] = b10;
  debug[11] = b11;
  debug[12] = b12;
  debug[13] = b13;
  debug[14] = b14;
  debug[15] = b15;
  debug[16] = b16;
  debug[17] = b17;
  debug[18] = b18;
  debug[19] = b19;
  debug[20] = b20;
  debug[21] = b21;
  debug[22] = b22;
  debug[23] = b23;
  debug[24] = b24;
  debug[25] = b25;
*/
  // ''.join('debug[%d] = b%d;\n' % (i, i) for i in range(26))
  // c0 = cycle_start;
  // c1 = next_edge;
  c1 = x;
  c0++;
  c3 = cycles_end - cycles_start;
}

  // c1 = ULP_RISCV_GET_CCOUNT() - t;
  next_edge += 1252;

  return x;
}

static uint32_t check_edges()
{
  // RISC-V CPU clock is 20 MHz
  // WS toggles at 16 kHz = 1250 clocks per toggle

  uint32_t b0, b1, b2, b3, b4, b5, b6, b7, b8, b9, b10, b11,
           b12, b13, b14, b15, b16, b17, b18, b19, b20, b21, b22, b23,
           b24, b25, b26, b27, b28, b29, b30, b31;

  uint32_t addr;
  __asm__ volatile (
    "lui %[addr], 0xa\n"  // Address: 0xa424 (main CPU 0x60008424)
    : [addr] "=r" (addr)
  );

  uint32_t edge_count;
  uint32_t t;
  uint32_t seed = 1;

  while (edge_count < N_EDGES) {
    seed = seed * 1103515245 + 12345;
    ulp_riscv_delay_cycles((seed >> 11) & 4095);

    uint32_t cycles_start, cycles_end;
    __asm__ volatile (
      ".p2align 2\n"
      ".option norvc\n"
      "rdcycle %[cycles_start]\n"
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
      "rdcycle %[cycles_end]\n"
      ".option rvc\n"
      : [cycles_start] "=&r" (cycles_start)
       ,[cycles_end] "=&r" (cycles_end)
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
      : [addr] "r" (addr)
    );

  #define RECORD_EDGE(_n) { \
    uint32_t ti = (cycles_start + 9 * (_n)) % 1252; \
    if (edge_count == 0) t = ti; \
    else if (t > ti) t = ti; /* TODO: Handle wraprounds */ \
    edge_count++; continue; \
  }
    if (!((b1 >> (10 + PIN_I2S_WS_PROBE)) & 1) && ((b0 >> (10 + PIN_I2S_WS_PROBE)) & 1)) RECORD_EDGE(1)
    if (!((b2 >> (10 + PIN_I2S_WS_PROBE)) & 1) && ((b1 >> (10 + PIN_I2S_WS_PROBE)) & 1)) RECORD_EDGE(2)
    if (!((b3 >> (10 + PIN_I2S_WS_PROBE)) & 1) && ((b2 >> (10 + PIN_I2S_WS_PROBE)) & 1)) RECORD_EDGE(3)
    if (!((b4 >> (10 + PIN_I2S_WS_PROBE)) & 1) && ((b3 >> (10 + PIN_I2S_WS_PROBE)) & 1)) RECORD_EDGE(4)
    if (!((b5 >> (10 + PIN_I2S_WS_PROBE)) & 1) && ((b4 >> (10 + PIN_I2S_WS_PROBE)) & 1)) RECORD_EDGE(5)
    if (!((b6 >> (10 + PIN_I2S_WS_PROBE)) & 1) && ((b5 >> (10 + PIN_I2S_WS_PROBE)) & 1)) RECORD_EDGE(6)
    if (!((b7 >> (10 + PIN_I2S_WS_PROBE)) & 1) && ((b6 >> (10 + PIN_I2S_WS_PROBE)) & 1)) RECORD_EDGE(7)
    if (!((b8 >> (10 + PIN_I2S_WS_PROBE)) & 1) && ((b7 >> (10 + PIN_I2S_WS_PROBE)) & 1)) RECORD_EDGE(8)
    if (!((b9 >> (10 + PIN_I2S_WS_PROBE)) & 1) && ((b8 >> (10 + PIN_I2S_WS_PROBE)) & 1)) RECORD_EDGE(9)
    if (!((b10 >> (10 + PIN_I2S_WS_PROBE)) & 1) && ((b9 >> (10 + PIN_I2S_WS_PROBE)) & 1)) RECORD_EDGE(10)
    if (!((b11 >> (10 + PIN_I2S_WS_PROBE)) & 1) && ((b10 >> (10 + PIN_I2S_WS_PROBE)) & 1)) RECORD_EDGE(11)
    if (!((b12 >> (10 + PIN_I2S_WS_PROBE)) & 1) && ((b11 >> (10 + PIN_I2S_WS_PROBE)) & 1)) RECORD_EDGE(12)
    if (!((b13 >> (10 + PIN_I2S_WS_PROBE)) & 1) && ((b12 >> (10 + PIN_I2S_WS_PROBE)) & 1)) RECORD_EDGE(13)
    if (!((b14 >> (10 + PIN_I2S_WS_PROBE)) & 1) && ((b13 >> (10 + PIN_I2S_WS_PROBE)) & 1)) RECORD_EDGE(14)
    if (!((b15 >> (10 + PIN_I2S_WS_PROBE)) & 1) && ((b14 >> (10 + PIN_I2S_WS_PROBE)) & 1)) RECORD_EDGE(15)
  }

  return t;
}
#pragma GCC pop_options

int main()
{
  c0 = c1 = c2 = c3 = debug[0] = 0;
  ulp_riscv_gpio_init(PIN_I2S_BCK_PROBE);
  ulp_riscv_gpio_init(PIN_I2S_WS_PROBE);
  ulp_riscv_gpio_init(PIN_I2S_DIN);
  ulp_riscv_gpio_init(PIN_I2S_AUX_PROBE);

  uint32_t offs = check_edges();

  uint32_t t = ULP_RISCV_GET_CCOUNT();
  next_edge = t - t % 1252 + 1252 * 4000 + offs - 18;

  uint32_t block = 0;
  int successive = 0;
  while (1) {
    if (check_power) {
      uint32_t power = 0;
      uint32_t sample = read_less();
      audio_buf[block + 0] = sample;
      int16_t last_s16 = (int16_t)sample;
      for (int i = 1; i < 64; i++) {
        uint32_t sample = read_less();
        audio_buf[block + i] = sample;
        int32_t s16 = (int16_t)sample;
        int32_t s_diff = last_s16 - s16;
        power += (uint32_t)(s_diff * s_diff) / 64;
        last_s16 = s16;
      }
      cur_buf_ptr = block = (block + 64) % 2048;
      c2 = power;
      if (power >= 20000) {
        if (++successive >= 4) {
          successive = 4;
          wakeup_signal = 1;
          ulp_riscv_wakeup_main_processor();
        }
      } else {
        successive -= 2;
        if (successive < 0) successive = 0;
      }
    } else {
      uint32_t power = 0;
      for (int i = 0; i < 64; i++) {
        uint32_t sample = read();
        audio_buf[block + i] = sample;
      }
      cur_buf_ptr = block = (block + 64) % 2048;
    }
  }
}
