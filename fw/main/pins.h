#pragma once

#define BOARD_REV 2

#if BOARD_REV == 1
  #define PIN_R 17
  #define PIN_G 18
  #define PIN_B 21
#else
  #define PIN_R 47
  #define PIN_G 33
  #define PIN_B 48
#endif

#if BOARD_REV == 1
  #define PIN_I2S_BCK 14
  #define PIN_I2S_WS  12
  #define PIN_I2S_DIN 13
  #define PIN_I2S_BCK_PROBE 9
  #define PIN_I2S_WS_PROBE  8
  #define PIN_I2S_LR 1
  #define PIN_I2S_IS_MASTER 1
#else
  #define PIN_I2S_BCK  1
  #define PIN_I2S_WS   5
  #define PIN_I2S_DIN  3
  #define PIN_I2S_BCK_PROBE 2
  #define PIN_I2S_WS_PROBE  4
  #define PIN_I2S_LR 0
  #define PIN_I2S_IS_MASTER 0
#endif
