#pragma once

#include <Arduino.h>

#ifndef STM32_MCU_SERIES
// from gpio.h

/** GPIO register map type */
typedef struct gpio_reg_map {
    __IO uint32_t CRL;      /**< Port configuration register low */
    __IO uint32_t CRH;      /**< Port configuration register high */
    __IO uint32_t IDR;      /**< Port input data register */
    __IO uint32_t ODR;      /**< Port output data register */
    __IO uint32_t BSRR;     /**< Port bit set/reset register */
    __IO uint32_t BRR;      /**< Port bit reset register */
    __IO uint32_t LCKR;     /**< Port configuration lock register */
} gpio_reg_map;

#define GPIOA_BASE                      ((struct gpio_reg_map*)0x40010800)
#define GPIOB_BASE                      ((struct gpio_reg_map*)0x40010C00)
#define GPIOC_BASE                      ((struct gpio_reg_map*)0x40011000)
#define GPIOD_BASE                      ((struct gpio_reg_map*)0x40011400)

#define GPIO_CR_MODE                    0x3
#define GPIO_CR_MODE_INPUT              0x0
#define GPIO_CR_MODE_OUTPUT_10MHZ       0x1
#define GPIO_CR_MODE_OUTPUT_2MHZ        0x2
#define GPIO_CR_MODE_OUTPUT_50MHZ       0x3

#endif


class Apa102Port
{
public:
  Apa102Port(gpio_reg_map& regMap);
  void configureStrip(int strip, uint8_t dataPinOffset, uint8_t clockPinOffset);
  bool writeStrip(int strip, uint8_t* data, uint16_t len);
  bool isStripReady(int strip) const { return _dataLen[strip] == 0 && isStripConfigured(strip); }

  bool update();
  bool needsUpdating() const { return _clockMask != 0; }
  
private:
  void configurePin(int pin);
  bool isStripConfigured(int strip) const { return _dataPin[strip] != kStripNotConfigured; }
  bool setStripData(int strip, uint8_t* data, uint16_t len);
  void writeStrips4();
  void writeStrips8();

  static constexpr int kMaxStripsPerPort = 8;
  static constexpr int kStripNotConfigured = 32;

  gpio_reg_map& _regMap;
  uint8_t _dataPin[kMaxStripsPerPort] = {};
  uint8_t _clockPin[kMaxStripsPerPort] = {};
  uint32_t _dataMask[kMaxStripsPerPort] = {};
  uint8_t _dataRotate[kMaxStripsPerPort] = {};
  uint16_t _clockMask = 0;
  uint8_t* _data[kMaxStripsPerPort];
  uint16_t _dataLen[kMaxStripsPerPort] = {};
  bool _moreThan4Strips = false;
  static uint8_t _zeroData;
};
