#pragma once;

#include <USBMIDI.h>


class MidiController : public USBMidi {
public:
  void setProgramChangeCallback(void (*cb)(unsigned int, unsigned int)) {
    _programChangeCallback = cb;
  }

  void handleProgramChange(unsigned int channel, unsigned int program) override {
    if (_programChangeCallback) {
      _programChangeCallback(channel, program);
    }
  }

private:
  void (*_programChangeCallback)(unsigned int, unsigned int) = nullptr;
};
