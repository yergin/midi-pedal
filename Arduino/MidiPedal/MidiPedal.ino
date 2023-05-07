#include "Globals.h"

void setMidiStatus(MidiStatus status)
{
  midiStatusUpdated = millis();
  midiStatus = status;
}

void updateMidiStatus()
{
  if (millis() >= midiStatusUpdated + MidiStatusStrobe)
  {
    setMidiStatus(usbMidiConnected ? MidiStatus::UsbConnected : MidiStatus::UsbDisconnected);
  }
  leds[(int)Led::Status] = MidiStatusColors[(int)midiStatus];
}

void handleProgramChange(unsigned int channel, unsigned int program)
{
  setMidiStatus(MidiStatus::Receiving);
#if USB_SERIAL_LOGGING
  String s = String() + "Received Program Change " + String(program) + " [Ch:" + String(channel + 1) + "]\n";
  CompositeSerial.write(s.c_str());
#endif
}

void handleSysExData(unsigned char data)
{
  setMidiStatus(MidiStatus::Receiving);
#if USB_SERIAL_LOGGING
  String s = String() + "Received SysEx byte: 0x" + String(data, 16) + "\n";
  CompositeSerial.write(s.c_str());
#endif
}

void handleSysExEnd()
{
  setMidiStatus(MidiStatus::Receiving);
#if USB_SERIAL_LOGGING
  String s = String() + "End of SysEx\n";
  CompositeSerial.write(s.c_str());
#endif
}

void setupExtControllers()
{
  for (int i = 0; i < ExternalControlCount; i++)
  {
    auto& conf = extConfig[i];
    if (conf.isSwitch)
    {
      extSwitch[i].attach(PinExtControl[i], INPUT_PULLUP);
      extSwitch[i].inverted(conf.inverted);
    }
    else
    {
      extAnalog[i].setup(PinExtControl[i], 127);
      extAnalog[i].configure(conf.rawMin, conf.rawMax, conf.inverted, false);
    }
  }
}

void loadExtConfig()
{
  extConfig[0] = {
    .isSwitch = false,
    .inverted = true,
    .rawMin = 0,
    .rawMax = 1320,
  };
  extConfig[2] = extConfig[1] = extConfig[0];
  extConfig[3] =
  {
    .isSwitch = true,
    .inverted = true,
    .rawMin = 0,
    .rawMax = 1320,
  };
  setupExtControllers();
}

void setupUsb()
{
  USBComposite.clear();
  USBComposite.setManufacturerString("Tranzistor Bandicoot");
  USBComposite.setProductString("Footsy");
  USBComposite.setVendorId(0xc007);
  USBComposite.setProductId(0xf754);
#if USB_SERIAL_LOGGING
  CompositeSerial.registerComponent();
#else
  midi.registerComponent();
  midi.setProgramChangeCallback(handleProgramChange);
  midi.setSysExCallbacks(handleSysExData, handleSysExEnd);
#endif
  USBComposite.begin();
#if USB_SERIAL_LOGGING
  delay(2000);
  CompositeSerial.write("Footsy by Tranzistor Bandicoot\n");
#endif
}

void setupIoPins()
{
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, HIGH);
  ledPort.configureStrip(0, LedPinData, LedPinClock);
  for (int i = 0; i < FootSwitchCount; i++)
  {
    footSwitches[i].attach(PinFootSwitch[i], INPUT_PULLUP);
    footSwitches[i].enableDoubleTap(false);
    footSwitches[i].enableHold(false);
  }
  for (int i = 0; i < ExternalControlCount; i++)
  {
    extSensors[i].attach(PinExtSense[i], INPUT_PULLDOWN);
    extSensors[i].enableDoubleTap(false);
    extSensors[i].enableHold(false);
  }
}

HardwareSerial* serial(MidiPort port)
{
  switch (port)
  {
    case MidiPort::Din1: return &Serial1;
    case MidiPort::Din2: return &Serial3;
  }
  return nullptr;
}

void sendProgramChange(HardwareSerial* ser, int ch, int program)
{
  if (!ser)
  {
#if not USB_SERIAL_LOGGING
    midi.sendProgramChange(ch, program);
#endif
  }
  else
  {
    ser->write(0xC0 + ch);
    ser->write(program);
  }
#if USB_SERIAL_LOGGING
  String s = String() + "Ch:" + String(ch + 1) + " Prog:" + String(program) + "\n";
  CompositeSerial.write(s.c_str());
#endif
}

void sendControlChange(HardwareSerial* ser, int ch, int cc, int val)
{
  if (!ser)
  {
#if not USB_SERIAL_LOGGING
    midi.sendControlChange(ch, cc, val);
#endif
  }
  else
  {
    ser->write(0xB0 + ch);
    ser->write(cc);
    ser->write(val);
  }
#if USB_SERIAL_LOGGING
  String s = String() + "Ch:" + String(ch + 1) + " CC:" + String(cc) + " Val:" + String(val) + "\n";
  CompositeSerial.write(s.c_str());
#endif
}

void setValue(Parameter param, int16_t value)
{
  setMidiStatus(MidiStatus::Sending);
  int lsb = value & 0x7f;
  int msb = (value >> 7) & 0x7f;
  auto ser = serial(param.port);
  switch (param.type)
  {
    case ParameterType::Preset:
      for (int i = 0; i < patch.presetGroups[param.presetGroup].parameterCount; i++)
      {
        setValue(patch.presetGroups[param.presetGroup].parameters[i], patch.presetGroups[param.presetGroup].presets[lsb].values[i]);
      }
      break;

    case ParameterType::ProgramChange:
      sendProgramChange(ser, param.channel, lsb);
      break;

    case ParameterType::ControlChange:
      sendControlChange(ser, param.channel, param.controller, lsb);
      if (param.is14Bit)
      {
        sendControlChange(ser, param.channel, param.controllerMsb, msb);
      }
      break;

    case ParameterType::RegisteredParameter:
      sendControlChange(ser, param.channel, 0x65, param.rpnMsb);
      sendControlChange(ser, param.channel, 0x64, param.rpnLsb);
      if (param.is14Bit)
      {
        sendControlChange(ser, param.channel, 0x06, msb);
        sendControlChange(ser, param.channel, 0x26, lsb);
      }
      else
      {
        sendControlChange(ser, param.channel, 0x06, lsb);
      }
      break;

    case ParameterType::NonRegisteredParameter:
      sendControlChange(ser, param.channel, 0x63, param.nrpnMsb);
      sendControlChange(ser, param.channel, 0x62, param.nrpnLsb);
      if (param.is14Bit)
      {
        sendControlChange(ser, param.channel, 0x06, msb);
        sendControlChange(ser, param.channel, 0x26, lsb);
      }
      else
      {
        sendControlChange(ser, param.channel, 0x06, lsb);
      }
  }
}

void sendAllValues()
{
  for (int i = 0; i < controllerCount; i++)
  {
    setValue(controllers[i].parameter, controllers[i].value);
  }
}

void clearControllers()
{
  controllerCount = 0;
}

ControllerState* controllerState(const Parameter& parameter)
{
  for (int i = 0; i < controllerCount; i++)
  {
    if (parametersMatch(controllers[i].parameter, parameter))
    {
      return controllers + i;
    }
  }
  return nullptr;
}

ControllerState* addController(const Mapping& mapping)
{
  if (controllerState(mapping.parameter))
  {
    return nullptr;
  }
  controllers[controllerCount] = {
    .parameter = mapping.parameter,
    .value = mapping.initialValue,
  };
  return &controllers[controllerCount++];
}

void initialiseControllers()
{
  clearControllers();
  for (int i = 0; i < FootSwitchCount; i++)
  {
    const auto& mapping = patch.footSwitchMapping[i];
    if (mapping.mode == MappingMode::None)
    {
      continue;
    }
    if (auto state = addController(mapping))
    {
      setValue(state->parameter, state->value);
    }
  }
  for (int i = 0; i < ExternalControlCount; i++)
  {
    const auto& mapping = patch.extControlMapping[i];
    if (mapping.mode == MappingMode::None)
    {
      continue;
    }
    if (auto state = addController(mapping))
    {
      if (!extConfig[i].isSwitch && mapping.mode == MappingMode::Analog)
      {
        extSensors[i].update();
        if (extSensors[i].down())
        {
          state->value = extAnalog[i].value();
        }
      }
      setValue(state->parameter, state->value);
    }
  }

  sendAllValues();
}

void loadPatch(int program)
{
#if USB_SERIAL_LOGGING
  String s = String() + "Loading patch for program " + String(program) + "\n";
  CompositeSerial.write(s.c_str());
#endif
  patch.program = 0;

  patch.footSwitchMapping[0] = {};
  patch.footSwitchMapping[0].parameter.type = ParameterType::Preset,
  patch.footSwitchMapping[0].parameter.presetGroup = 0,
  patch.footSwitchMapping[0].initialValue = 0;
  patch.footSwitchMapping[0].minValue = 0;
  patch.footSwitchMapping[0].maxValue = 1;
  patch.footSwitchMapping[0].mode = MappingMode::SwitchMomentary;

  patch.footSwitchMapping[1] = {};
  patch.footSwitchMapping[1].parameter.port = MidiPort::Din1,
  patch.footSwitchMapping[1].parameter.type = ParameterType::ProgramChange,
  patch.footSwitchMapping[1].initialValue = 71;
  patch.footSwitchMapping[1].minValue = 71;
  patch.footSwitchMapping[1].maxValue = 76;
  patch.footSwitchMapping[1].mode = MappingMode::SwitchToggle;

  patch.footSwitchMapping[2] = {};
  patch.footSwitchMapping[2].parameter.port = MidiPort::Din1,
  patch.footSwitchMapping[2].parameter.type = ParameterType::ProgramChange,
  patch.footSwitchMapping[2].initialValue = 73;
  patch.footSwitchMapping[2].minValue = 73;
  patch.footSwitchMapping[2].maxValue = 76;
  patch.footSwitchMapping[2].mode = MappingMode::SwitchToggle;

  patch.extControlMapping[0] = {};
  patch.extControlMapping[0].parameter.port = MidiPort::Usb,
  patch.extControlMapping[0].parameter.type = ParameterType::ControlChange,
  patch.extControlMapping[0].parameter.controller = 11,
  patch.extControlMapping[0].initialValue = 0;
  patch.extControlMapping[0].minValue = 0;
  patch.extControlMapping[0].maxValue = 127;
  patch.extControlMapping[0].mode = MappingMode::Analog;

  patch.extControlMapping[1] = {};
  patch.extControlMapping[1].parameter.port = MidiPort::Din1,
  patch.extControlMapping[1].parameter.type = ParameterType::ControlChange,
  patch.extControlMapping[1].parameter.controller = 16,
  patch.extControlMapping[1].initialValue = 4;
  patch.extControlMapping[1].minValue = 4;
  patch.extControlMapping[1].maxValue = 64;
  patch.extControlMapping[1].mode = MappingMode::Analog;

  patch.extControlMapping[2] = {};
  patch.extControlMapping[2].parameter.port = MidiPort::Din1,
  patch.extControlMapping[2].parameter.type = ParameterType::ControlChange,
  patch.extControlMapping[2].parameter.controller = 48,
  patch.extControlMapping[2].initialValue = 0;
  patch.extControlMapping[2].minValue = 0;
  patch.extControlMapping[2].maxValue = 127;
  patch.extControlMapping[2].mode = MappingMode::Analog;

  patch.extControlMapping[3] = {};
  patch.extControlMapping[3].parameter.port = MidiPort::Usb,
  patch.extControlMapping[3].parameter.type = ParameterType::ControlChange,
  patch.extControlMapping[3].parameter.controller = 83,
  patch.extControlMapping[3].initialValue = 0;
  patch.extControlMapping[3].minValue = 0;
  patch.extControlMapping[3].maxValue = 127;
  patch.extControlMapping[3].mode = MappingMode::SwitchToggle;

  patch.presetGroupCount = 1;
  patch.presetGroups[0].parameterCount = 2;
  patch.presetGroups[0].parameters[0].port = MidiPort::Din1;
  patch.presetGroups[0].parameters[0].type = ParameterType::ControlChange;
  patch.presetGroups[0].parameters[0].controller = 5;
  patch.presetGroups[0].parameters[1].port = MidiPort::Din1;
  patch.presetGroups[0].parameters[1].type = ParameterType::NonRegisteredParameter;
  patch.presetGroups[0].parameters[1].nrpnLsb = 73;
  patch.presetGroups[0].presetCount = 2;
  patch.presetGroups[0].presets[0].values[0] = 0;
  patch.presetGroups[0].presets[0].values[1] = 0;
  patch.presetGroups[0].presets[1].values[0] = 48;
  patch.presetGroups[0].presets[1].values[1] = 2;

  initialiseControllers();
}

bool parametersMatch(const Parameter& p1, const Parameter& p2)
{
  return
    p1.port == p2.port &&
    p1.channel == p2.channel &&
    p1.is14Bit == p2.is14Bit &&
    p1.type == p2.type &&
    p1.controller == p2.controller &&
    p1.controllerMsb == p2.controllerMsb;
}

void updateControllerLed(Mapping& mapping, ControllerState& state, Colour& led)
{
  if (!parametersMatch(mapping.parameter, state.parameter))
  {
    return;
  }
  auto val = state.parameter.is14Bit ? state.value >> 7 : state.value;
  switch (mapping.mode)
  {
    case MappingMode::SwitchZoneUp:
    case MappingMode::SwitchZoneDown:
    case MappingMode::SwitchZoneUpCycle:
    case MappingMode::SwitchZoneDownCycle:
    case MappingMode::SwitchZoneReset:
      val = valueForZone(zoneForValue(val, mapping.zoneCount, mapping.minValue, mapping.maxValue), mapping.zoneCount, mapping.minValue, mapping.maxValue);
    case MappingMode::None:
    case MappingMode::Analog:
    case MappingMode::SwitchToggle:
    case MappingMode::SwitchMomentary:
    case MappingMode::SwitchUp:
    case MappingMode::SwitchDown:
    case MappingMode::SwitchReset:
      if (val < 32)
      {
        led = { 0, 95 + val, 127 - (val << 2) };
      }
      else
      {
        val = val - 32;
        led = { val * 2, 127 - val + (val >> 2), 0 };
      }
      break;
  }
}

bool extControlMapped(int index)
{
  return patch.extControlMapping[index].mode != MappingMode::None;
}

bool extControlMappedCorrectly(int index)
{
  if (patch.extControlMapping[index].mode == MappingMode::None)
  {
    return true;
  }
  return (patch.extControlMapping[index].mode == MappingMode::Analog) == !extConfig[index].isSwitch;
}

void updateControllerLeds()
{
  for (int i = 0; i < FootSwitchCount; i++)
  {
    leds[LedFootSwitchMap[i]] = { 0, 0, 0 };
  }
  for (int i = 0; i < ExternalControlCount; i++)
  {
    if (extSensors[i].down())
    {
      if (extControlMapped(i))
      {
        if (!extControlMappedCorrectly(i))
        {
          leds[LedExtControlMap[i]] = { 127, 0, 95 };
        }
      }
      else
      {
        leds[LedExtControlMap[i]] = { 191, 0, 7 };
      }
    }
    else
    {
      leds[LedExtControlMap[i]] = { 0, 0, 0 };
    }
  }
  for (int c = 0; c < controllerCount; c++)
  {
    for (int i = 0; i < FootSwitchCount; i++)
    {
      updateControllerLed(patch.footSwitchMapping[i], controllers[c], leds[LedFootSwitchMap[i]]);
    }
    for (int i = 0; i < ExternalControlCount; i++)
    {
      if (extSensors[i].down() && extControlMapped(i) && extControlMappedCorrectly(i))
      {
        updateControllerLed(patch.extControlMapping[i], controllers[c], leds[LedExtControlMap[i]]);
      }
    }
  }
}

void displayLeds()
{
  static uint8_t buf[8 + 4 * LedCount];
  uint8_t brightness = 0x1; // 0-F
  uint8_t global = 0xE0 | brightness;
  uint8_t* out = buf;
  Colour* col = leds;
  *out++ = 0x00;
  *out++ = 0x00;
  *out++ = 0x00;
  *out++ = 0x00;
  for (int i = 0; i < LedCount; i++)
  {
    *out++ = global;
    *out++ = col->b;
    *out++ = col->g;
    *out++ = col->r;
    col++;
  }
  *out++ = 0xFF;
  *out++ = 0xFF;
  *out++ = 0xFF;
  *out++ = 0xFF;
  ledPort.writeStrip(0, buf, out - buf);
  while(ledPort.update());
}

void setup()
{
  Serial1.begin(31250);
  Serial3.begin(31250);
  setupUsb();
  setupIoPins();
  loadExtConfig();
  loadPatch(0);
}

int8_t zoneForValue(int value, int count, int minValue, int maxValue)
{
  if (count < 2)
  {
    return 0;
  }
  return (int8_t)min(max(0, (value - minValue) * count / (maxValue - minValue + 1)), count - 1);
}

int8_t valueForZone(int zone, int count, int minValue, int maxValue)
{
  if (count < 2)
  {
    return minValue;
  }
  return (int8_t)min(max(minValue, zone * (maxValue - minValue + 1) / (count - 1) + minValue), maxValue);
}

void updateButtonControllerValue(Mapping& mapping, bool down)
{
  if (mapping.mode == MappingMode::None)
  {
    return;
  }
  auto& state = *controllerState(mapping.parameter);
  if (mapping.mode == MappingMode::SwitchMomentary)
  {
    state.value = down ? mapping.maxValue : mapping.minValue;
    setValue(state.parameter, state.value);
    return;
  }
  if (!down)
  {
    return;
  }
  switch (mapping.mode)
  {
    case MappingMode::None:
    case MappingMode::Analog:
    case MappingMode::SwitchMomentary:
      return;

    case MappingMode::SwitchToggle:
      state.value = state.value >= ((int)mapping.minValue + (int)mapping.maxValue) / 2 ? mapping.minValue : mapping.maxValue;
      break;

    case MappingMode::SwitchUp:
      if (state.value >= mapping.maxValue)
      {
        return;
      }
      state.value++;
      break;

    case MappingMode::SwitchDown:
      if (state.value <= mapping.minValue)
      {
        return;
      }
      state.value--;
      break;

    case MappingMode::SwitchReset:
      if (state.value == mapping.initialValue)
      {
        return;
      }
      state.value = mapping.initialValue;
      break;

    case MappingMode::SwitchZoneUp:
      {
        auto zone = zoneForValue(state.value, mapping.zoneCount, mapping.minValue, mapping.maxValue);
        if (zone == mapping.zoneCount - 1)
        {
          return;
        }
        state.value = valueForZone(zone + 1, mapping.zoneCount, mapping.minValue, mapping.maxValue);
        break;
      }

    case MappingMode::SwitchZoneDown:
      {
        auto zone = zoneForValue(state.value, mapping.zoneCount, mapping.minValue, mapping.maxValue);
        if (zone == 0) {
          return;
        }
        state.value = valueForZone(zone - 1, mapping.zoneCount, mapping.minValue, mapping.maxValue);
        break;
      }

    case MappingMode::SwitchZoneUpCycle:
      {
        auto zone = zoneForValue(state.value, mapping.zoneCount, mapping.minValue, mapping.maxValue);
        state.value = valueForZone((zone + 1) % mapping.zoneCount, mapping.zoneCount, mapping.minValue, mapping.maxValue);
        break;
      }

    case MappingMode::SwitchZoneDownCycle:
      {
        auto zone = zoneForValue(state.value, mapping.zoneCount, mapping.minValue, mapping.maxValue);
        state.value = valueForZone((zone + mapping.zoneCount - 1) % mapping.zoneCount, mapping.zoneCount, mapping.minValue, mapping.maxValue);
        break;
      }

    case MappingMode::SwitchZoneReset:
      {
        if (zoneForValue(state.value, mapping.zoneCount, mapping.minValue, mapping.maxValue) == zoneForValue(mapping.initialValue, mapping.zoneCount, mapping.minValue, mapping.maxValue))
        {
          return;
        }
        state.value = mapping.initialValue;
        break;
      }
  }
  setValue(state.parameter, state.value);
}

void updateAnalogControllerValue(Mapping& mapping, int value)
{
  if (mapping.mode != MappingMode::Analog)
  {
    return;
  }
  auto& state = *controllerState(mapping.parameter);
  state.value = min(max(mapping.minValue, value * (mapping.maxValue - mapping.minValue + 1) / 128 + mapping.minValue), mapping.maxValue);
  setValue(state.parameter, state.value);
}

void setFootSwitchDown(int index, bool down)
{
  updateButtonControllerValue(patch.footSwitchMapping[index], down);
}

void setExtSwitchDown(int index, bool down)
{
  updateButtonControllerValue(patch.extControlMapping[index], down);
}

void setExtAnalogValue(int index, int value)
{
  updateAnalogControllerValue(patch.extControlMapping[index], value);
}

void updateFootSwitch(int index)
{
  footSwitches[index].update();
  if (footSwitches[index].rose() || footSwitches[index].fell())
  {
    setFootSwitchDown(index, footSwitches[index].pressed());
  }
}

void setExtControlEnabled(int index, bool enabled)
{
  if (enabled) {
    if (extConfig[index].isSwitch)
    {
      setExtSwitchDown(index, extSwitch[index].pressed());
    }
    else
    {
      setExtAnalogValue(index, extAnalog[index].value());
    }
  }
}

void updateExtControl(int index)
{
  extSensors[index].update();
  if (extSensors[index].rose() || extSensors[index].fell())
  {
#if USB_SERIAL_LOGGING
    String s = String() + "External Pedal #" + String(index + 1) + " " + (extSensors[index].pressed() ? "plugged\n" :  "unplugged\n");
    CompositeSerial.write(s.c_str());
#endif
    setExtControlEnabled(index, extSensors[index].pressed());
  }
  if (extSensors[index].down())
  {
    if (extConfig[index].isSwitch)
    {
      extSwitch[index].update();
      if (extSwitch[index].rose() || extSwitch[index].fell())
      {
        setExtSwitchDown(index, extSwitch[index].pressed());
      }
    }
    else
    {
      if (extAnalog[index].update())
      {
        setExtAnalogValue(index, extAnalog[index].value());
      }
    }
  }
}

void updateInputs()
{
  for (int i = 0; i < FootSwitchCount; i++)
  {
    updateFootSwitch(i);
  }
  for (int i = 0; i < ExternalControlCount; i++)
  {
    updateExtControl(i);
  }
}

void setUsbMidiConnected(bool connected)
{
  if (usbMidiConnected == connected)
  {
    return;
  }
  usbMidiConnected = connected;
  if (usbMidiConnected)
  {
#if USB_SERIAL_LOGGING
    CompositeSerial.write("USB MIDI Connected.\n");
#endif
    sendAllValues();
  }
}

void pollMidi()
{
#if not USB_SERIAL_LOGGING
  midi.poll();
  setUsbMidiConnected(midi.isConnected());
#endif
}

void loop()
{
  pollMidi();
  updateInputs();
  updateMidiStatus();
  updateControllerLeds();
  displayLeds();
}
