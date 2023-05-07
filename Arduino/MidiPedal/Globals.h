#pragma once

#include "Apa102Port.h"
#include "AnalogReader.h"
#include "MidiController.h"
#include <EEPROM.h>
#include <Yabl.h>

#define USB_SERIAL_LOGGING 1

enum class MidiStatus : uint8_t
{
  UsbDisconnected,
  UsbConnected,
  Sending,
  Receiving,
  Count
};

inline constexpr int MidiStatusCount = (int)MidiStatus::Count;

enum class FootSwitch : uint8_t
{
  S1 = 0,
  S2,
  S3,
  Count
};

inline constexpr int FootSwitchCount = (int)FootSwitch::Count;

enum class ExternalControl : uint8_t
{
  X1 = 0,
  X2,
  X3,
  X4,
  Count
};

inline constexpr int ExternalControlCount = (int)ExternalControl::Count;

enum class Led : uint8_t
{
  Status,
  X1,
  X2,
  X3,
  X4,
  S3,
  S2,
  S1,
  Count
};

inline constexpr int LedCount = (int)Led::Count;

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

enum class MappingMode : uint8_t {
  None,
  Analog,
  SwitchToggle,
  SwitchMomentary,
  SwitchUp,
  SwitchDown,
  SwitchReset,
  SwitchZoneUp,
  SwitchZoneDown,
  SwitchZoneUpCycle,
  SwitchZoneDownCycle,
  SwitchZoneReset,
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
    } __packed;
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

struct Colour
{
  union
  {
    struct
    {
      uint8_t r;
      uint8_t g;
      uint8_t b;
    } __packed;
    uint8_t raw[3];
  };
} __packed;

constexpr int PinDisplayScl = PB6;
constexpr int PinDisplaySda = PB7;

#define LedPort GPIOB_BASE
constexpr int LedPinData = 9;
constexpr int LedPinClock = 8;
constexpr int PinLedPortData = PB9;
constexpr int PinLedPortClock = PB8;

constexpr int LedFootSwitchMap[FootSwitchCount] = { (int)Led::S1, (int)Led::S2, (int)Led::S3 };
constexpr int LedExtControlMap[ExternalControlCount] = { (int)Led::X1, (int)Led::X2, (int)Led::X3, (int)Led::X4 };

constexpr int PinFootSwitch[FootSwitchCount] = { PB3, PB4, PB5 };
constexpr int PinExtControl[ExternalControlCount] = { PA0, PA1, PA2, PA3 };
constexpr int PinExtSense[ExternalControlCount] = { PB12, PB13, PB14, PB15 };

inline Colour MidiStatusColors[MidiStatusCount] = {
  { 127, 63, 95 },
  { 95, 95, 95 },
  { 0, 127, 0 },
  { 127, 0, 255 },
};

constexpr int MidiStatusStrobe = 200;

inline Colour leds[LedCount] = {
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
inline Apa102Port ledPort(*LedPort);
inline Button footSwitches[FootSwitchCount];
inline Button extSensors[ExternalControlCount];
inline Button extSwitch[ExternalControlCount];
inline AnalogReader extAnalog[ExternalControlCount];
