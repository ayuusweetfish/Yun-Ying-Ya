#pragma once

#define BOARD_REV 1

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
  #define PIN_I2S_BCK 9
  #define PIN_I2S_WS  8
  #define PIN_I2S_DIN 13
  #define PIN_I2S_BCK_PROBE 14
  #define PIN_I2S_WS_PROBE  12
  #define PIN_I2S_LR 1
  #define PIN_I2S_IS_MASTER 0
  #define PIN_MIC_EN  15
#else
  #define PIN_I2S_BCK  1
  #define PIN_I2S_WS   5
  #define PIN_I2S_DIN  3
  #define PIN_I2S_BCK_PROBE 2
  #define PIN_I2S_WS_PROBE  4
  #define PIN_I2S_LR 0
  #define PIN_I2S_IS_MASTER 0
  #undef  PIN_MIC_EN
#endif

#if BOARD_REV == 1
  #define PIN_I2C_SCL 36
  #define PIN_I2C_SDA 35
#else
  #define PIN_I2C_SCL 20
  #define PIN_I2C_SDA 19
#endif
