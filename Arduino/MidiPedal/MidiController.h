#pragma once;

#include <USBMIDI.h>


class MidiController : public USBMIDI {
public:
  void setProgramChangeCallback(void (*cb)(unsigned int, unsigned int)) {
    _programChangeCallback = cb;
  }

  void setSysExCallbacks(void (*data)(unsigned char), void (*end)()) {
    _sysExDataCallback = data;
    _sysExEndCallback = end;
  }

  void handleProgramChange(unsigned int channel, unsigned int program) override {
    if (_programChangeCallback) {
      _programChangeCallback(channel, program);
    }
  }

  void handleSysExData(unsigned char data) override {
    if (_sysExDataCallback) {
      _sysExDataCallback(data);
    }
  }
  
  void handleSysExEnd(void) override {
    if (_sysExEndCallback) {
      _sysExEndCallback();
    }
  }
  
private:
  void (*_programChangeCallback)(unsigned int, unsigned int) = nullptr;
  void (*_sysExDataCallback)(unsigned char) = nullptr;
  void (*_sysExEndCallback)() = nullptr;
};
