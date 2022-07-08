#pragma once

#include <Arduino.h>


class AnalogReader {
public:
  static constexpr int kAdcMax = 4095;

  AnalogReader() {}

  void setup(int pin, int maxValue = 127) {
    _pin = pin;
    _maxValue = maxValue;
    pinMode(pin, INPUT_ANALOG);
    _rawReference = analogRead(_pin);
    _raw[0] = _raw[1] = _raw[2] = _raw[3] = _rawAvg = _rawReference;
    endCalibration();
  }

  void configure(int rawMin = 0, int rawMax = kAdcMax, bool invert = false, bool active = true) {
    _calibrating = false;
    _rawMin = rawMin;
    _rawMax = rawMax;
    _invert = invert;
    _active = active;
    _schmitt = 2 * max(16, (_rawMax - _rawMin + 1) / (_maxValue + 1));
    _halfSchmitt = _schmitt / 2;
    _lowerBound = _upperBound = -1;
    _value = -1;
    update();
  }

  void startCalibration() {
    _rawMin = kAdcMax;
    _rawMax = 0;
    _calibrating = true;
  }

  void endCalibration() {
    configure(_rawMin, _rawMax, _invert, _active);
  }

  bool update() {
    _index = (_index + 1) % 4;
    _raw[_index] = analogRead(_pin);
    _rawAvg = (_raw[0] + _raw[1] + _raw[2] + _raw[3]) >> 2;
    if (_calibrating) {
      _rawMin = min(_rawMin, _rawAvg);
      _rawMax = max(_rawMax, _rawAvg);
      return false;
    }
    if (_lowerBound < _rawAvg && _rawAvg < _upperBound) {
      return false;
    }
    _rawAvg = min(max(_rawMin, _rawAvg), _rawMax);
    if (_active) {
      return setValue((_rawAvg - _rawMin) * _maxValue / (_rawMax - _rawMin));
    }
    return setValue((_rawAvg - _rawMin) * (kAdcMax - _rawMax) * _maxValue / (_rawMax - _rawMin) / (kAdcMax - _rawAvg));
  }

  void updateBounds(int direction) {
    _lowerBound = direction < 0 ? _rawReference - _halfSchmitt : _rawReference - _schmitt;
    _upperBound = direction > 0 ? _rawReference + _halfSchmitt : _rawReference + _schmitt;
  }

  bool setValue(int val) {
    if (_value == val) {
      return false;
    }
    _rawReference = _rawAvg;
    updateBounds(_value == -1 ? 0 : val - _value);
    _value = val;
    return true;
  }

  int value() const { return _invert ? _maxValue - _value : _value; }
  int raw() const { return _raw[_index]; }
  int rawAvg() const { return _rawAvg; }
  int rawMin() const { return _rawMin; }
  int rawMax() const { return _rawMax; }
  int schmitt() const { return _schmitt; }
  bool invert() const { return _invert; }

private:
  int _pin;
  int _maxValue;
  bool _invert = false;
  bool _active = true;
  int _rawMin = 0;
  int _rawMax = kAdcMax;
  int _rawReference = 0;
  int _lowerBound = 0;
  int _upperBound = 0;
  int _schmitt = 0;
  int _halfSchmitt = 0;
  bool _calibrating = false;
  int _raw[4] = { 0, 0, 0, 0 };
  int _rawAvg = 0;
  int _index = 0;
  int _value = -1;
};
