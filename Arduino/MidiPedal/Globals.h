#pragma once

#include "Apa102Port.h"
#include "AnalogReader.h"
#include "MidiController.h"
#include <EEPROM.h>
#include <Yabl.h>

#define USB_SERIAL_LOGGING 1

enum MidiStatus
{
  kUsbMidiDisconnected,
  kUsbMidiConnected,
  kMidiSending,
  kMidiReceiving,
  kMidiStatusCount
};

enum FootSwitches
{
  kFootSwitch1 = 0,
  kFootSwitch2,
  kFootSwitch3,
  kFootSwitchCount
};

enum ExtControls
{
  kExtControl1 = 0,
  kExtControl2,
  kExtControl3,
  kExtControl4,
  kExtControlCount
};

enum Leds
{
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

enum class MidiPort : uint8_t
{
  Usb = 1,
  Din1 = 2,
  Din2 = 4,
};

struct ExtConfig
{
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

enum class DataType : uint8_t
{
  Preset,
  NoteVelocity,
  PitchBend,
  ProgramChange,
  ControlChange,
  RegisteredParameter,
  NonRegisteredParameter,
};

struct DataAssignment
{
  union {
    int8_t _header = 0;
    struct {
      MidiPort port : 2;
      int8_t channel : 4;
      int8_t is14Bit : 1;
      int8_t : 1;
    };
  };
  DataType type = DataType::ControlChange;
  union
  {
    int8_t scope = 0;
    int8_t note;
    int8_t controller;
    int8_t rpnLsb;
    int8_t nrpnLsb;
  };
  union
  {
    int8_t controllerMsb = 0;
    int8_t rpnMsb;
    int8_t nrpnMsb;
  };
} __packed;

constexpr int MaxZones = 5;
struct Mapping
{
  DataAssignment assignment = {};
  int16_t initialValue = 0;
  union
  {
    struct
    {
      int16_t minValue = 0;
      int16_t maxValue = 127;
    };
    int16_t zoneValues[MaxZones];
  };
  union
  {
    int8_t _footer = 0;
    struct
    {
      int8_t zoneCount : 3;
      int8_t useZoneValues: 1;
      MappingMode mode : 4;
    };
  };
  int8_t _reserved;
} __packed;

struct ControllerState
{
  DataAssignment assignment;
  int16_t value;
} __packed;

constexpr int MaxPresetAssignments = 8;
struct Preset
{
  int16_t values[MaxPresetAssignments];
} __packed;

constexpr int MaxPresets = 8;
struct PresetScope
{
  DataAssignment assignments[MaxPresetAssignments];
  Preset presets[MaxPresets];
  int8_t assignmentCount;
  int8_t presetCount;
} __packed;

constexpr int MaxPresetScopes = 4;
struct Patch
{
  Mapping footSwitchMapping[kFootSwitchCount];
  Mapping extControlMapping[kExtControlCount];
  PresetScope presetScopes[MaxPresetScopes];
  int8_t scopeCount;
  int8_t program;
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
  { 127, 63, 95 },
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

#if USB_SERIAL_LOGGING
inline USBCompositeSerial CompositeSerial;
#endif

inline MidiController midi;
inline bool usbMidiConnected = false;
inline MidiStatus midiStatus = kUsbMidiDisconnected;
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
