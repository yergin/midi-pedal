#pragma once

#include "Apa102Port.h"
#include "AnalogReader.h"
#include "MidiController.h"
#include <EEPROM.h>
#include <Yabl.h>


enum UsbMidiStatus {
  kMidiDisconnected,
  kMidiConnected,
  kMidiSending,
  kMidiReceiving,
  kMidiStatusCount
};

enum FootSwitches {
  kFootSwitch1 = 0,
  kFootSwitch2,
  kFootSwitch3,
  kFootSwitchCount
};

enum ExtControls {
  kExtControl1 = 0,
  kExtControl2,
  kExtControl3,
  kExtControl4,
  kExtControlCount
};

enum Leds {
  kLedStatus,
  kLedExtControl1,
  kLedExtControl2,
  kLedExtControl3,
  kLedExtControl4,
  kLedFootSwitch3,
  kLedFootSwitch2,
  kLedFootSwitch1,
  kLedCount
};

struct ExtConfig {
  bool isSwitch;
  bool inverted;
  uint16_t rawMin;
  uint16_t rawMax;
} __packed;

enum MappingMode : uint8_t {
  kMappingNone,
  kMappingAnalog,
  kMappingSwitchToggle,
  kMappingSwitchMomentary,
  kMappingSwitchUp,
  kMappingSwitchDown,
  kMappingSwitchReset,
  kMappingSwitchZoneUp,
  kMappingSwitchZoneDown,
  kMappingSwitchZoneUpCycle,
  kMappingSwitchZoneDownCycle,
  kMappingSwitchZoneReset,
};

struct Mapping {
  MappingMode mode;
  int8_t channel;
  int8_t controller;
  int8_t initialValue;
  int8_t zoneCount;
} __packed;

struct Patch {
  Mapping footSwitchMapping[kFootSwitchCount];
  Mapping extControlMapping[kExtControlCount];
  int8_t program;
} __packed;

struct ControllerState {
  int8_t ch;
  int8_t cc;
  int8_t val;
} __packed;

struct Colour {
  union {
    struct {
      uint8_t r;
      uint8_t g;
      uint8_t b;
    } __packed;
    uint8_t raw[3];
  };
} __packed;

constexpr int kPinDisplayScl = PB6;
constexpr int kPinDisplaySda = PB7;

#define kLedPort GPIOB_BASE
constexpr int kLedPinData = 9;
constexpr int kLedPinClock = 8;
constexpr int kPinLedPortData = PB9;
constexpr int kPinLedPortClock = PB8;

constexpr int kLedFootSwitchMap[kFootSwitchCount] = { kLedFootSwitch1, kLedFootSwitch2, kLedFootSwitch3 };
constexpr int kLedExtControlMap[kExtControlCount] = { kLedExtControl1, kLedExtControl2, kLedExtControl3, kLedExtControl4 };

constexpr int kPinFootSwitch[kFootSwitchCount] = { PB3, PB4, PB5 };
constexpr int kPinExtControl[kExtControlCount] = { PA0, PA1, PA2, PA3 };
constexpr int kPinExtSense[kExtControlCount] = { PB12, PB13, PB14, PB15 };

inline Colour kMidiStatusColors[kMidiStatusCount] = {
  { 191, 0, 0 },
  { 95, 95, 95 },
  { 0, 127, 0 },
  { 127, 0, 255 },
};

constexpr int kMidiStatusStrobe = 200;

inline Colour leds[kLedCount] = {
  { 0, 0, 0 },
  { 0, 0, 0 },
  { 0, 0, 0 },
  { 0, 0, 0 },
  { 0, 0, 0 },
  { 0, 0, 0 },
  { 0, 0, 0 },
  { 0, 0, 0 },
};

inline MidiController midi;
inline bool midiConnected = false;
inline UsbMidiStatus midiStatus = kMidiDisconnected;
inline uint32_t midiStatusUpdated = 0;
inline ExtConfig extConfig[kExtControlCount];
inline ControllerState controllers[kFootSwitchCount + kExtControlCount];
inline uint8_t controllerCount = 0;
inline Patch patch;
inline Apa102Port ledPort(*kLedPort);
inline Button footSwitches[kFootSwitchCount];
inline Button extSensors[kExtControlCount];
inline Button extSwitch[kExtControlCount];
inline AnalogReader extAnalog[kExtControlCount];
