#pragma once

#define BOARD_REV 2

#if BOARD_REV == 1
  #define PIN_R 17
  #define PIN_G 18
  #define PIN_B 21
  #define PIN_SCK 8
#else
  #define PIN_R 47
  #define PIN_G 33
  #define PIN_B 48
  #define PIN_SCK 1
#endif

#if BOARD_REV == 1
  #define PIN_I2S_BCK 14
  #define PIN_I2S_WS  12
  #define PIN_I2S_DIN 13
  #define PIN_I2S_BCK_PROBE 9
#else
  #define PIN_I2S_BCK  1
  #define PIN_I2S_WS   4
  #define PIN_I2S_DIN  3
  #define PIN_I2S_BCK_PROBE 2
#endif
