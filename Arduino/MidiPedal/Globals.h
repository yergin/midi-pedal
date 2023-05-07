#pragma once

#include "Apa102Port.h"
#include "AnalogReader.h"
#include "MidiController.h"
#include <EEPROM.h>
#include <Yabl.h>

#define USB_SERIAL_LOGGING 1

enum class MidiStatus
{
  UsbDisconnected,
  UsbConnected,
  Sending,
  Receiving,
  Count
};

inline constexpr int MidiStatusCount = (int)MidiStatus::Count;

enum class FootSwitch
{
  S1 = 0,
  S2,
  S3,
  Count
};

inline constexpr int FootSwitchCount = (int)FootSwitch::Count;

enum class ExternalControl
{
  X1 = 0,
  X2,
  X3,
  X4,
  Count
};

inline constexpr int ExternalControlCount = (int)ExternalControl::Count;

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

enum class ParameterType : uint8_t
{
  Preset,
  NoteVelocity,
  PitchBend,
  ProgramChange,
  ControlChange,
  RegisteredParameter,
  NonRegisteredParameter,
};

struct Parameter
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
  ParameterType type = ParameterType::ControlChange;
  union
  {
    int8_t presetGroup = 0;
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
  Parameter parameter = {};
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
  Parameter parameter;
  int16_t value;
} __packed;

constexpr int MaxPresetParameters = 8;
struct Preset
{
  int16_t values[MaxPresetParameters];
} __packed;

constexpr int MaxPresets = 8;
struct PresetGroup
{
  Parameter parameters[MaxPresetParameters];
  Preset presets[MaxPresets];
  int8_t parameterCount;
  int8_t presetCount;
} __packed;

constexpr int MaxPresetGroups = 4;
struct Patch
{
  Mapping footSwitchMapping[FootSwitchCount];
  Mapping extControlMapping[ExternalControlCount];
  PresetGroup presetGroups[MaxPresetGroups];
  int8_t presetGroupCount;
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

constexpr int kLedFootSwitchMap[FootSwitchCount] = { kLedFootSwitch1, kLedFootSwitch2, kLedFootSwitch3 };
constexpr int kLedExtControlMap[ExternalControlCount] = { kLedExtControl1, kLedExtControl2, kLedExtControl3, kLedExtControl4 };

constexpr int kPinFootSwitch[FootSwitchCount] = { PB3, PB4, PB5 };
constexpr int kPinExtControl[ExternalControlCount] = { PA0, PA1, PA2, PA3 };
constexpr int kPinExtSense[ExternalControlCount] = { PB12, PB13, PB14, PB15 };

inline Colour MidiStatusColors[MidiStatusCount] = {
  { 127, 63, 95 },
  { 95, 95, 95 },
  { 0, 127, 0 },
  { 127, 0, 255 },
};

constexpr int MidiStatusStrobe = 200;

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
inline MidiStatus midiStatus = MidiStatus::UsbDisconnected;
inline uint32_t midiStatusUpdated = 0;
inline ExtConfig extConfig[ExternalControlCount];
inline ControllerState controllers[FootSwitchCount + ExternalControlCount];
inline uint8_t controllerCount = 0;
inline Patch patch;
inline Apa102Port ledPort(*kLedPort);
inline Button footSwitches[FootSwitchCount];
inline Button extSensors[ExternalControlCount];
inline Button extSwitch[ExternalControlCount];
inline AnalogReader extAnalog[ExternalControlCount];
