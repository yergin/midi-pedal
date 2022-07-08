#include "Apa102Port.h"

#define ROR(result, value, shift) \
  __asm__ __volatile__ ( \
    "ROR %[res], %[val], %[s]" \
    : [res] "=r" (result) \
    : [val] "r" (value), [s] "r" (shift) \
  );


uint8_t Apa102Port::_zeroData = 0;

Apa102Port::Apa102Port(gpio_reg_map& regMap)
  : _regMap(regMap)
{
  for (int strip = 0; strip < kMaxStripsPerPort; ++strip) {
    _data[strip] = &_zeroData;
    _dataPin[strip] = kStripNotConfigured;
    _clockPin[strip] = kStripNotConfigured;
  }
}

void Apa102Port::configureStrip(int strip, uint8_t dataPinOffset, uint8_t clockPinOffset)
{
  if (strip < 0 || strip >= kMaxStripsPerPort) {
    return;
  }

  _dataPin[strip] = dataPinOffset;
  _clockPin[strip] = clockPinOffset;
  _dataRotate[strip] = (32 - dataPinOffset + 7) % 32;
  configurePin(dataPinOffset);
  configurePin(clockPinOffset);
  setStripData(strip, 0, 0);
  _moreThan4Strips = _moreThan4Strips || strip >= 4;
}

void Apa102Port::configurePin(int pin)
{
  _regMap.BRR = 1 << pin;
  if (pin < 8) {
    _regMap.CRL &= ~(0xF << pin * 4);
    _regMap.CRL |= GPIO_CR_MODE_OUTPUT_50MHZ << pin * 4;
  }
  else {
    _regMap.CRH &= ~(0xF << (pin - 8) * 4);
    _regMap.CRH |= GPIO_CR_MODE_OUTPUT_50MHZ << (pin - 8) * 4;
  }
}

bool Apa102Port::writeStrip(int strip, uint8_t* data, uint16_t len)
{
  if (!isStripReady(strip)) {
    return false;
  }

  if (!setStripData(strip, data, len)) {
    return false;
  }

  return true;
}

bool Apa102Port::setStripData(int strip, uint8_t* data, uint16_t len)
{
  if (strip < 0 || strip >= kMaxStripsPerPort) {
    return false;
  }

  if (!isStripReady(strip)) {
    return false;
  }
  
  if (data && len) {
    _clockMask |= 1 << _clockPin[strip];
    _dataMask[strip] = 0x00010001 << _dataPin[strip];
    _data[strip] = data;
    _dataLen[strip] = len;
  }
  else {
    _dataMask[strip] = 0;
    _data[strip] = &_zeroData;
    _dataLen[strip] = 0;
    _clockMask &= ~(1 << _clockPin[strip]);
  }

  return true;
}

void Apa102Port::writeStrips8()
{
  uint32_t mask0 = _dataMask[0];
  uint32_t mask1 = _dataMask[1];
  uint32_t mask2 = _dataMask[2];
  uint32_t mask3 = _dataMask[3];
  uint32_t mask4 = _dataMask[4];
  uint32_t mask5 = _dataMask[5];
  uint32_t mask6 = _dataMask[6];
  uint32_t mask7 = _dataMask[7];

  uint32_t bsrr0 = *_data[0]; ROR(bsrr0, bsrr0 | bsrr0 << 16 ^ 0xFF0000, _dataRotate[0]);
  uint32_t bsrr1 = *_data[1]; ROR(bsrr1, bsrr1 | bsrr1 << 16 ^ 0xFF0000, _dataRotate[1]);
  uint32_t bsrr2 = *_data[2]; ROR(bsrr2, bsrr2 | bsrr2 << 16 ^ 0xFF0000, _dataRotate[2]);
  uint32_t bsrr3 = *_data[3]; ROR(bsrr3, bsrr3 | bsrr3 << 16 ^ 0xFF0000, _dataRotate[3]);
  uint32_t bsrr4 = *_data[4]; ROR(bsrr4, bsrr4 | bsrr4 << 16 ^ 0xFF0000, _dataRotate[4]);
  uint32_t bsrr5 = *_data[5]; ROR(bsrr5, bsrr5 | bsrr5 << 16 ^ 0xFF0000, _dataRotate[5]);
  uint32_t bsrr6 = *_data[6]; ROR(bsrr6, bsrr6 | bsrr6 << 16 ^ 0xFF0000, _dataRotate[6]);
  uint32_t bsrr7 = *_data[7]; ROR(bsrr7, bsrr7 | bsrr7 << 16 ^ 0xFF0000, _dataRotate[7]);
  
  _regMap.BRR = _clockMask; // clock low

  for (int b = 7; b >= 0; b--) {
    _regMap.BSRR = bsrr0 & mask0;
    _regMap.BSRR = bsrr1 & mask1;
    _regMap.BSRR = bsrr2 & mask2;
    _regMap.BSRR = bsrr3 & mask3;
    _regMap.BSRR = bsrr4 & mask4;
    _regMap.BSRR = bsrr5 & mask5;
    _regMap.BSRR = bsrr6 & mask6;
    _regMap.BSRR = bsrr7 & mask7;
    
    _regMap.BSRR = _clockMask; // clock high

    asm("ROR %[bsrr], %[bsrr], #31" : [bsrr] "+r" (bsrr0));
    asm("ROR %[bsrr], %[bsrr], #31" : [bsrr] "+r" (bsrr1));
    asm("ROR %[bsrr], %[bsrr], #31" : [bsrr] "+r" (bsrr2));
    asm("ROR %[bsrr], %[bsrr], #31" : [bsrr] "+r" (bsrr3));
    asm("ROR %[bsrr], %[bsrr], #31" : [bsrr] "+r" (bsrr4));
    asm("ROR %[bsrr], %[bsrr], #31" : [bsrr] "+r" (bsrr5));
    asm("ROR %[bsrr], %[bsrr], #31" : [bsrr] "+r" (bsrr6));
    asm("ROR %[bsrr], %[bsrr], #31" : [bsrr] "+r" (bsrr7));
    
    _regMap.BRR = _clockMask; // clock low
  }
}

void Apa102Port::writeStrips4()
{
  uint32_t mask0 = _dataMask[0];
  uint32_t mask1 = _dataMask[1];
  uint32_t mask2 = _dataMask[2];
  uint32_t mask3 = _dataMask[3];

  register uint32_t bsrr0 = *_data[0]; ROR(bsrr0, bsrr0 | bsrr0 << 16 ^ 0xFF0000, _dataRotate[0]);
  register uint32_t bsrr1 = *_data[1]; ROR(bsrr1, bsrr1 | bsrr1 << 16 ^ 0xFF0000, _dataRotate[1]);
  register uint32_t bsrr2 = *_data[2]; ROR(bsrr2, bsrr2 | bsrr2 << 16 ^ 0xFF0000, _dataRotate[2]);
  register uint32_t bsrr3 = *_data[3]; ROR(bsrr3, bsrr3 | bsrr3 << 16 ^ 0xFF0000, _dataRotate[3]);
  
  _regMap.BRR = _clockMask; // clock low

  _regMap.BSRR = bsrr0 & mask0;
  _regMap.BSRR = bsrr1 & mask1;
  _regMap.BSRR = bsrr2 & mask2;
  _regMap.BSRR = bsrr3 & mask3;

  _regMap.BSRR = _clockMask; // clock high
  _regMap.BRR = _clockMask; // clock low

  asm("AND %[bsrr], %[mask], %[in], ROR #31" : [bsrr] "=r" (_regMap.BSRR) : [mask] "r" (mask0), [in] "r" (bsrr0));
  asm("AND %[bsrr], %[mask], %[in], ROR #31" : [bsrr] "=r" (_regMap.BSRR) : [mask] "r" (mask1), [in] "r" (bsrr1));
  asm("AND %[bsrr], %[mask], %[in], ROR #31" : [bsrr] "=r" (_regMap.BSRR) : [mask] "r" (mask2), [in] "r" (bsrr2));
  asm("AND %[bsrr], %[mask], %[in], ROR #31" : [bsrr] "=r" (_regMap.BSRR) : [mask] "r" (mask3), [in] "r" (bsrr3));

  _regMap.BSRR = _clockMask; // clock high
  _regMap.BRR = _clockMask; // clock low

  asm("AND %[bsrr], %[mask], %[in], ROR #30" : [bsrr] "=r" (_regMap.BSRR) : [mask] "r" (mask0), [in] "r" (bsrr0));
  asm("AND %[bsrr], %[mask], %[in], ROR #30" : [bsrr] "=r" (_regMap.BSRR) : [mask] "r" (mask1), [in] "r" (bsrr1));
  asm("AND %[bsrr], %[mask], %[in], ROR #30" : [bsrr] "=r" (_regMap.BSRR) : [mask] "r" (mask2), [in] "r" (bsrr2));
  asm("AND %[bsrr], %[mask], %[in], ROR #30" : [bsrr] "=r" (_regMap.BSRR) : [mask] "r" (mask3), [in] "r" (bsrr3));

  _regMap.BSRR = _clockMask; // clock high
  _regMap.BRR = _clockMask; // clock low

  asm("AND %[bsrr], %[mask], %[in], ROR #29" : [bsrr] "=r" (_regMap.BSRR) : [mask] "r" (mask0), [in] "r" (bsrr0));
  asm("AND %[bsrr], %[mask], %[in], ROR #29" : [bsrr] "=r" (_regMap.BSRR) : [mask] "r" (mask1), [in] "r" (bsrr1));
  asm("AND %[bsrr], %[mask], %[in], ROR #29" : [bsrr] "=r" (_regMap.BSRR) : [mask] "r" (mask2), [in] "r" (bsrr2));
  asm("AND %[bsrr], %[mask], %[in], ROR #29" : [bsrr] "=r" (_regMap.BSRR) : [mask] "r" (mask3), [in] "r" (bsrr3));

  _regMap.BSRR = _clockMask; // clock high
  _regMap.BRR = _clockMask; // clock low

  asm("AND %[bsrr], %[mask], %[in], ROR #28" : [bsrr] "=r" (_regMap.BSRR) : [mask] "r" (mask0), [in] "r" (bsrr0));
  asm("AND %[bsrr], %[mask], %[in], ROR #28" : [bsrr] "=r" (_regMap.BSRR) : [mask] "r" (mask1), [in] "r" (bsrr1));
  asm("AND %[bsrr], %[mask], %[in], ROR #28" : [bsrr] "=r" (_regMap.BSRR) : [mask] "r" (mask2), [in] "r" (bsrr2));
  asm("AND %[bsrr], %[mask], %[in], ROR #28" : [bsrr] "=r" (_regMap.BSRR) : [mask] "r" (mask3), [in] "r" (bsrr3));

  _regMap.BSRR = _clockMask; // clock high
  _regMap.BRR = _clockMask; // clock low

  asm("AND %[bsrr], %[mask], %[in], ROR #27" : [bsrr] "=r" (_regMap.BSRR) : [mask] "r" (mask0), [in] "r" (bsrr0));
  asm("AND %[bsrr], %[mask], %[in], ROR #27" : [bsrr] "=r" (_regMap.BSRR) : [mask] "r" (mask1), [in] "r" (bsrr1));
  asm("AND %[bsrr], %[mask], %[in], ROR #27" : [bsrr] "=r" (_regMap.BSRR) : [mask] "r" (mask2), [in] "r" (bsrr2));
  asm("AND %[bsrr], %[mask], %[in], ROR #27" : [bsrr] "=r" (_regMap.BSRR) : [mask] "r" (mask3), [in] "r" (bsrr3));

  _regMap.BSRR = _clockMask; // clock high
  _regMap.BRR = _clockMask; // clock low

  asm("AND %[bsrr], %[mask], %[in], ROR #26" : [bsrr] "=r" (_regMap.BSRR) : [mask] "r" (mask0), [in] "r" (bsrr0));
  asm("AND %[bsrr], %[mask], %[in], ROR #26" : [bsrr] "=r" (_regMap.BSRR) : [mask] "r" (mask1), [in] "r" (bsrr1));
  asm("AND %[bsrr], %[mask], %[in], ROR #26" : [bsrr] "=r" (_regMap.BSRR) : [mask] "r" (mask2), [in] "r" (bsrr2));
  asm("AND %[bsrr], %[mask], %[in], ROR #26" : [bsrr] "=r" (_regMap.BSRR) : [mask] "r" (mask3), [in] "r" (bsrr3));

  _regMap.BSRR = _clockMask; // clock high
  _regMap.BRR = _clockMask; // clock low

  asm("AND %[bsrr], %[mask], %[in], ROR #25" : [bsrr] "=r" (_regMap.BSRR) : [mask] "r" (mask0), [in] "r" (bsrr0));
  asm("AND %[bsrr], %[mask], %[in], ROR #25" : [bsrr] "=r" (_regMap.BSRR) : [mask] "r" (mask1), [in] "r" (bsrr1));
  asm("AND %[bsrr], %[mask], %[in], ROR #25" : [bsrr] "=r" (_regMap.BSRR) : [mask] "r" (mask2), [in] "r" (bsrr2));
  asm("AND %[bsrr], %[mask], %[in], ROR #25" : [bsrr] "=r" (_regMap.BSRR) : [mask] "r" (mask3), [in] "r" (bsrr3));

  _regMap.BSRR = _clockMask; // clock high
  _regMap.BRR = _clockMask; // clock low
}

bool Apa102Port::update()
{
  if (!needsUpdating()) {
    return false;
  }

  if (_moreThan4Strips) {
    writeStrips8();
  }
  else {
    writeStrips4();
  }

  for (int strip = 0; strip < kMaxStripsPerPort; ++strip) {
    if (_dataLen[strip]) {
      _data[strip]++;
      _dataLen[strip]--;
      if (!_dataLen[strip]) {
        setStripData(strip, 0, 0);
      }
    }
  }

  return needsUpdating();
}

