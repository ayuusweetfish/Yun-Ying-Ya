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

#define ULP_AUDIO_BUF_SIZE 2048
uint16_t audio_buf[ULP_AUDIO_BUF_SIZE];
uint32_t cur_buf_ptr;

uint32_t debug[10];
uint32_t debuga[32];

#define N_EDGES 64

uint32_t next_edge = 0;
uint32_t last_sample = 0;

extern uint32_t sled[32];
#define SLED_ADDRESS 0x64 // Workaround for unsupported relocation scheme
// riscv32-esp-elf-objdump -t build/esp-idf/main/ulp_duck/ulp_duck.elf

#pragma GCC push_options
#pragma GCC optimize("O3")
static inline uint32_t read()
{
  uint32_t b0, b1, b2, b3, b4, b5, b6, b7, b8, b9, b10, b11,
           b12, b13, b14, b15, b16, b17, b18, b19, b20, b21, b22, b23,
           b24, b25, b26, b27, b28, b29, b30, b31;

  uint32_t addr;
  asm (
    "lui %[addr], 0xa\n"  // Address: 0xa424 (main CPU 0x60008424)
    : [addr] "=r" (addr)
  );

  uint32_t cycles_start, cycles_end;
  uint32_t scratch;
  asm volatile (
    "li a0, 0\n"

    ".p2align 2\n"
    ".option norvc\n"
    // Obtain current cycles until >= next_edge
    "1:"
    "rdcycle %[cycles_start]\n"
    "sub %[scratch], %[cycles_start], %[next_edge]\n"
    "bltz %[scratch], 1b\n"

    // Cycle-accurate delay compensation subroutine
    // destination = *(sled + 4 * scratch)
    "andi %[scratch], %[scratch], 31\n" // Mask spurious large values during startup
    "slli %[scratch], %[scratch], 2\n"
    "lw %[scratch], %[sled_base](%[scratch])\n"
    "jalr ra, 0(%[scratch])\n"

    : [scratch] "=&r" (scratch),
      [cycles_start] "=&r" (cycles_start)
    : [next_edge] "r" (next_edge),
      [sled_base] "i" (SLED_ADDRESS)
    : "a0", "ra"
  );

  asm volatile (
    ".p2align 2\n"
    ".option norvc\n"
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
  /*
    "lw %[b23], 0x424(%[addr])\n"
    "lw %[b24], 0x424(%[addr])\n"
    "lw %[b25], 0x424(%[addr])\n"
  */
    "rdcycle %[cycles_end]\n"
    ".option rvc\n"
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
     ,[b16] "=&r" (b16)
     ,[b17] "=&r" (b17)
     ,[b18] "=&r" (b18)
     ,[b19] "=&r" (b19)
     ,[b20] "=&r" (b20)
     ,[b21] "=&r" (b21)
     ,[b22] "=&r" (b22)
    /*
     ,[b23] "=&r" (b23)
     ,[b24] "=&r" (b24)
     ,[b25] "=&r" (b25)
    */
     ,[cycles_end] "=&r" (cycles_end)
    : [addr] "r" (addr)
  );

  uint32_t x = 0;

  // Most: (60~70) + 9 * N
  // Seems a bit more (~5?) with even numbers
  if (cycles_end - cycles_start <= 73 + 9 * 23 /* number of bits read */) {
    // This will give very rare glitches due to bus contention (0~5 per second),
    // but it should be very acceptable
    uint32_t n = 0;
    L0: if ((b0 >> (10 + PIN_I2S_BCK_PROBE)) & 1) { x = (x << 1) | ((b0 >> (10 + PIN_I2S_DIN)) & 1); n++; goto L2; }
    L1: if ((b1 >> (10 + PIN_I2S_BCK_PROBE)) & 1) { x = (x << 1) | ((b1 >> (10 + PIN_I2S_DIN)) & 1); n++; goto L3; }
    L2: if ((b2 >> (10 + PIN_I2S_BCK_PROBE)) & 1) { x = (x << 1) | ((b2 >> (10 + PIN_I2S_DIN)) & 1); n++; goto L4; }
    L3: if ((b3 >> (10 + PIN_I2S_BCK_PROBE)) & 1) { x = (x << 1) | ((b3 >> (10 + PIN_I2S_DIN)) & 1); n++; goto L5; }
    L4: if ((b4 >> (10 + PIN_I2S_BCK_PROBE)) & 1) { x = (x << 1) | ((b4 >> (10 + PIN_I2S_DIN)) & 1); n++; goto L6; }
    L5: if ((b5 >> (10 + PIN_I2S_BCK_PROBE)) & 1) { x = (x << 1) | ((b5 >> (10 + PIN_I2S_DIN)) & 1); n++; goto L7; }
    L6: if ((b6 >> (10 + PIN_I2S_BCK_PROBE)) & 1) { x = (x << 1) | ((b6 >> (10 + PIN_I2S_DIN)) & 1); n++; goto L8; }
    L7: if ((b7 >> (10 + PIN_I2S_BCK_PROBE)) & 1) { x = (x << 1) | ((b7 >> (10 + PIN_I2S_DIN)) & 1); n++; goto L9; }
    L8: if ((b8 >> (10 + PIN_I2S_BCK_PROBE)) & 1) { x = (x << 1) | ((b8 >> (10 + PIN_I2S_DIN)) & 1); n++; goto L10; }
    L9: if ((b9 >> (10 + PIN_I2S_BCK_PROBE)) & 1) { x = (x << 1) | ((b9 >> (10 + PIN_I2S_DIN)) & 1); n++; goto L11; }
    L10: if ((b10 >> (10 + PIN_I2S_BCK_PROBE)) & 1) { x = (x << 1) | ((b10 >> (10 + PIN_I2S_DIN)) & 1); n++; goto L12; }
    L11: if ((b11 >> (10 + PIN_I2S_BCK_PROBE)) & 1) { x = (x << 1) | ((b11 >> (10 + PIN_I2S_DIN)) & 1); n++; goto L13; }
    L12: if ((b12 >> (10 + PIN_I2S_BCK_PROBE)) & 1) { x = (x << 1) | ((b12 >> (10 + PIN_I2S_DIN)) & 1); n++; goto L14; }
    L13: if ((b13 >> (10 + PIN_I2S_BCK_PROBE)) & 1) { x = (x << 1) | ((b13 >> (10 + PIN_I2S_DIN)) & 1); n++; goto L15; }
    L14: if ((b14 >> (10 + PIN_I2S_BCK_PROBE)) & 1) { x = (x << 1) | ((b14 >> (10 + PIN_I2S_DIN)) & 1); n++; goto L16; }
    L15: if ((b15 >> (10 + PIN_I2S_BCK_PROBE)) & 1) { x = (x << 1) | ((b15 >> (10 + PIN_I2S_DIN)) & 1); n++; goto L17; }
    L16: if ((b16 >> (10 + PIN_I2S_BCK_PROBE)) & 1) { x = (x << 1) | ((b16 >> (10 + PIN_I2S_DIN)) & 1); n++; goto L18; }
    L17: if ((b17 >> (10 + PIN_I2S_BCK_PROBE)) & 1) { x = (x << 1) | ((b17 >> (10 + PIN_I2S_DIN)) & 1); n++; goto L19; }
    L18: if ((b18 >> (10 + PIN_I2S_BCK_PROBE)) & 1) { x = (x << 1) | ((b18 >> (10 + PIN_I2S_DIN)) & 1); n++; goto L20; }
    L19: if ((b19 >> (10 + PIN_I2S_BCK_PROBE)) & 1) { x = (x << 1) | ((b19 >> (10 + PIN_I2S_DIN)) & 1); n++; goto L21; }
    L20: if ((b20 >> (10 + PIN_I2S_BCK_PROBE)) & 1) { x = (x << 1) | ((b20 >> (10 + PIN_I2S_DIN)) & 1); n++; goto L22; }
    L21: if ((b21 >> (10 + PIN_I2S_BCK_PROBE)) & 1) { x = (x << 1) | ((b21 >> (10 + PIN_I2S_DIN)) & 1); n++; goto L23; }
    L22: if ((b22 >> (10 + PIN_I2S_BCK_PROBE)) & 1) { x = (x << 1) | ((b22 >> (10 + PIN_I2S_DIN)) & 1); n++; goto L24; }
    L23: // if ((b23 >> (10 + PIN_I2S_BCK_PROBE)) & 1) { x = (x << 1) | ((b23 >> (10 + PIN_I2S_DIN)) & 1); n++; goto L25; }
    L24: // if ((b24 >> (10 + PIN_I2S_BCK_PROBE)) & 1) { x = (x << 1) | ((b24 >> (10 + PIN_I2S_DIN)) & 1); n++; goto L26; }
    L25: // if (((b25 >> (10 + PIN_I2S_BCK_PROBE)) & 1)) { x = (x << 1) | ((b25 >> (10 + PIN_I2S_DIN)) & 1); n++; goto L27; }
    L26: L27:

    // Sign-extend suffix / round to zero
    x <<= (16 - n);
    if (x & (1 << 15)) x |= ((1 << (16 - n)) - 1);
    x &= 0xffff;
    last_sample = x;
  } else {
    x = last_sample;
  }

  next_edge += 1250;
  // Unnecessarily overprotective
  // if (next_edge < ULP_RISCV_GET_CCOUNT()) next_edge += 1250 * 4;

  return x;
}

static inline uint32_t read_less()
{
  uint32_t b0, b1, b2, b3, b4, b5, b6, b7, b8, b9, b10, b11,
           b12, b13, b14, b15, b16, b17, b18, b19, b20, b21, b22, b23,
           b24, b25, b26, b27, b28, b29, b30, b31;

  uint32_t addr;
  asm (
    "lui %[addr], 0xa\n"  // Address: 0xa424 (main CPU 0x60008424)
    : [addr] "=r" (addr)
  );

  uint32_t cycles_start, cycles_end;
  uint32_t scratch;
  asm volatile (
    "li a0, 0\n"

    ".p2align 2\n"
    ".option norvc\n"
    // Obtain current cycles until >= next_edge
    "1:"
    "rdcycle %[cycles_start]\n"
    "sub %[scratch], %[cycles_start], %[next_edge]\n"
    "bltz %[scratch], 1b\n"

    // Cycle-accurate delay compensation subroutine
    // destination = *(sled + 4 * scratch)
    "andi %[scratch], %[scratch], 31\n" // Mask spurious large values during startup
    "slli %[scratch], %[scratch], 2\n"
    "lw %[scratch], %[sled_base](%[scratch])\n"
    "jalr ra, 0(%[scratch])\n"

    : [scratch] "=&r" (scratch),
      [cycles_start] "=&r" (cycles_start)
    : [next_edge] "r" (next_edge),
      [sled_base] "i" (SLED_ADDRESS)
    : "a0", "ra"
  );

  asm volatile (
    ".p2align 2\n"
    ".option norvc\n"
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
  /*
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
     ,[b16] "=&r" (b16)
     ,[b17] "=&r" (b17)
     ,[b18] "=&r" (b18)
    /*
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
  );

  uint32_t x = 0;

  // Most: (60~70) + 9 * N
  if (cycles_end - cycles_start <= 73 + 9 * 19 /* number of bits read */) {
    // This will give very rare glitches due to bus contention (0~5 per second),
    // but it should be very acceptable
    uint32_t n = 0;
    L0: if ((b0 >> (10 + PIN_I2S_BCK_PROBE)) & 1) { x = (x << 1) | ((b0 >> (10 + PIN_I2S_DIN)) & 1); n++; goto L2; }
    L1: if ((b1 >> (10 + PIN_I2S_BCK_PROBE)) & 1) { x = (x << 1) | ((b1 >> (10 + PIN_I2S_DIN)) & 1); n++; goto L3; }
    L2: if ((b2 >> (10 + PIN_I2S_BCK_PROBE)) & 1) { x = (x << 1) | ((b2 >> (10 + PIN_I2S_DIN)) & 1); n++; goto L4; }
    L3: if ((b3 >> (10 + PIN_I2S_BCK_PROBE)) & 1) { x = (x << 1) | ((b3 >> (10 + PIN_I2S_DIN)) & 1); n++; goto L5; }
    L4: if ((b4 >> (10 + PIN_I2S_BCK_PROBE)) & 1) { x = (x << 1) | ((b4 >> (10 + PIN_I2S_DIN)) & 1); n++; goto L6; }
    L5: if ((b5 >> (10 + PIN_I2S_BCK_PROBE)) & 1) { x = (x << 1) | ((b5 >> (10 + PIN_I2S_DIN)) & 1); n++; goto L7; }
    L6: if ((b6 >> (10 + PIN_I2S_BCK_PROBE)) & 1) { x = (x << 1) | ((b6 >> (10 + PIN_I2S_DIN)) & 1); n++; goto L8; }
    L7: if ((b7 >> (10 + PIN_I2S_BCK_PROBE)) & 1) { x = (x << 1) | ((b7 >> (10 + PIN_I2S_DIN)) & 1); n++; goto L9; }
    L8: if ((b8 >> (10 + PIN_I2S_BCK_PROBE)) & 1) { x = (x << 1) | ((b8 >> (10 + PIN_I2S_DIN)) & 1); n++; goto L10; }
    L9: if ((b9 >> (10 + PIN_I2S_BCK_PROBE)) & 1) { x = (x << 1) | ((b9 >> (10 + PIN_I2S_DIN)) & 1); n++; goto L11; }
    L10: if ((b10 >> (10 + PIN_I2S_BCK_PROBE)) & 1) { x = (x << 1) | ((b10 >> (10 + PIN_I2S_DIN)) & 1); n++; goto L12; }
    L11: if ((b11 >> (10 + PIN_I2S_BCK_PROBE)) & 1) { x = (x << 1) | ((b11 >> (10 + PIN_I2S_DIN)) & 1); n++; goto L13; }
    L12: if ((b12 >> (10 + PIN_I2S_BCK_PROBE)) & 1) { x = (x << 1) | ((b12 >> (10 + PIN_I2S_DIN)) & 1); n++; goto L14; }
    L13: if ((b13 >> (10 + PIN_I2S_BCK_PROBE)) & 1) { x = (x << 1) | ((b13 >> (10 + PIN_I2S_DIN)) & 1); n++; goto L15; }
    L14: if ((b14 >> (10 + PIN_I2S_BCK_PROBE)) & 1) { x = (x << 1) | ((b14 >> (10 + PIN_I2S_DIN)) & 1); n++; goto L16; }
    L15: if ((b15 >> (10 + PIN_I2S_BCK_PROBE)) & 1) { x = (x << 1) | ((b15 >> (10 + PIN_I2S_DIN)) & 1); n++; goto L17; }
    L16: if ((b16 >> (10 + PIN_I2S_BCK_PROBE)) & 1) { x = (x << 1) | ((b16 >> (10 + PIN_I2S_DIN)) & 1); n++; goto L18; }
    L17: if ((b17 >> (10 + PIN_I2S_BCK_PROBE)) & 1) { x = (x << 1) | ((b17 >> (10 + PIN_I2S_DIN)) & 1); n++; goto L19; }
    L18: if ((b18 >> (10 + PIN_I2S_BCK_PROBE)) & 1) { x = (x << 1) | ((b18 >> (10 + PIN_I2S_DIN)) & 1); n++; goto L20; }
    L19: // if ((b19 >> (10 + PIN_I2S_BCK_PROBE)) & 1) { x = (x << 1) | ((b19 >> (10 + PIN_I2S_DIN)) & 1); n++; goto L21; }
    L20: // if ((b20 >> (10 + PIN_I2S_BCK_PROBE)) & 1) { x = (x << 1) | ((b20 >> (10 + PIN_I2S_DIN)) & 1); n++; goto L22; }
    L21: // if ((b21 >> (10 + PIN_I2S_BCK_PROBE)) & 1) { x = (x << 1) | ((b21 >> (10 + PIN_I2S_DIN)) & 1); n++; goto L23; }
    L22: // if ((b22 >> (10 + PIN_I2S_BCK_PROBE)) & 1) { x = (x << 1) | ((b22 >> (10 + PIN_I2S_DIN)) & 1); n++; goto L24; }
    L23: // if ((b23 >> (10 + PIN_I2S_BCK_PROBE)) & 1) { x = (x << 1) | ((b23 >> (10 + PIN_I2S_DIN)) & 1); n++; goto L25; }
    L24: // if ((b24 >> (10 + PIN_I2S_BCK_PROBE)) & 1) { x = (x << 1) | ((b24 >> (10 + PIN_I2S_DIN)) & 1); n++; goto L26; }
    L25: // if (((b25 >> (10 + PIN_I2S_BCK_PROBE)) & 1)) { x = (x << 1) | ((b25 >> (10 + PIN_I2S_DIN)) & 1); n++; goto L27; }
    L26: L27:

    // Sign-extend suffix / round to zero
    x <<= (16 - n);
    if (x & (1 << 15)) x |= ((1 << (16 - n)) - 1);
    x &= 0xffff;
    last_sample = x;
  } else {
    x = last_sample;
  }

  next_edge += 1250;

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
  asm (
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
    asm volatile (
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
    uint32_t ti = (cycles_start + 9 * (_n)) % 1250; \
    if (edge_count == 0) t = ti; \
    else if ((t - ti + 1250) % 1250 < 1250 / 2) t = ti; /* TODO: Handle wraprounds */ \
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

  // Assertion
  if (SLED_ADDRESS != (uint32_t)&sled[0]) while (1) { }

  for (int i = 0; i < 21; i++) {
    uint32_t cyc0, cyc1;
    asm volatile (
      ".p2align 2\n"
      ".option norvc\n"
      "mv a0, zero\n"
      "rdcycle %[cyc0]\n"
      "jalr ra, 0(%[dest])\n"
      "rdcycle %[cyc1]\n"
      ".option rvc\n"
      : [cyc0] "=&r" (cyc0), [cyc1] "=&r" (cyc1)
      : [dest] "r" (sled[i])
      : "a0", "ra"
    );
    debuga[i] = cyc1 - cyc0;
  }

  uint32_t offs = check_edges();

  uint32_t t = ULP_RISCV_GET_CCOUNT();
  next_edge = t - t % 1250 + 1250 * 4000 + offs - 36;

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
      cur_buf_ptr = block = (block + 64) % ULP_AUDIO_BUF_SIZE;
      c2 = power;
      if (power >= 1000) {
        if (++successive >= 4) {
          successive = 4;
          wakeup_signal = 1;
          ulp_riscv_wakeup_main_processor();
        }
      } else if (power < 100) {
        successive = 0;
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
      cur_buf_ptr = block = (block + 64) % ULP_AUDIO_BUF_SIZE;
    }
  }
}
