#include "Globals.h"

void setMidiStatus(MidiStatus status)
{
  midiStatusUpdated = millis();
  midiStatus = status;
}

void updateMidiStatus()
{
  if (millis() >= midiStatusUpdated + kMidiStatusStrobe)
  {
    setMidiStatus(usbMidiConnected ? kUsbMidiConnected : kUsbMidiDisconnected);
  }
  leds[kLedStatus] = kMidiStatusColors[midiStatus];
}

void handleProgramChange(unsigned int channel, unsigned int program)
{
  setMidiStatus(kMidiReceiving);
#if USB_SERIAL_LOGGING
  String s = String() + "Received Program Change " + String(program) + " [Ch:" + String(channel + 1) + "]\n";
  CompositeSerial.write(s.c_str());
#endif
}

void handleSysExData(unsigned char data)
{
  setMidiStatus(kMidiReceiving);
#if USB_SERIAL_LOGGING
  String s = String() + "Received SysEx byte: 0x" + String(data, 16) + "\n";
  CompositeSerial.write(s.c_str());
#endif
}

void handleSysExEnd()
{
  setMidiStatus(kMidiReceiving);
#if USB_SERIAL_LOGGING
  String s = String() + "End of SysEx\n";
  CompositeSerial.write(s.c_str());
#endif
}

void setupExtControllers()
{
  for (int i = 0; i < kExtControlCount; i++)
  {
    auto& conf = extConfig[i];
    if (conf.isSwitch)
    {
      extSwitch[i].attach(kPinExtControl[i], INPUT_PULLUP);
      extSwitch[i].inverted(conf.inverted);
    }
    else
    {
      extAnalog[i].setup(kPinExtControl[i], 127);
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
  ledPort.configureStrip(0, kLedPinData, kLedPinClock);
  for (int i = 0; i < kFootSwitchCount; i++)
  {
    footSwitches[i].attach(kPinFootSwitch[i], INPUT_PULLUP);
    footSwitches[i].enableDoubleTap(false);
    footSwitches[i].enableHold(false);
  }
  for (int i = 0; i < kExtControlCount; i++)
  {
    extSensors[i].attach(kPinExtSense[i], INPUT_PULLDOWN);
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

void setValue(DataAssignment assign, int16_t value)
{
  setMidiStatus(kMidiSending);
  int lsb = value & 0x7f;
  int msb = (value >> 7) & 0x7f;
  auto ser = serial(assign.port);
  switch (assign.type)
  {
    case DataType::Preset:
      for (int i = 0; i < patch.presetScopes[assign.scope].assignmentCount; i++)
      {
        setValue(patch.presetScopes[assign.scope].assignments[i], patch.presetScopes[assign.scope].presets[lsb].values[i]);
      }
      break;

    case DataType::ProgramChange:
      sendProgramChange(ser, assign.channel, lsb);
      break;

    case DataType::ControlChange:
      sendControlChange(ser, assign.channel, assign.controller, lsb);
      if (assign.is14Bit)
      {
        sendControlChange(ser, assign.channel, assign.controllerMsb, msb);
      }
      break;

    case DataType::RegisteredParameter:
      sendControlChange(ser, assign.channel, 0x65, assign.rpnMsb);
      sendControlChange(ser, assign.channel, 0x64, assign.rpnLsb);
      if (assign.is14Bit)
      {
        sendControlChange(ser, assign.channel, 0x06, msb);
        sendControlChange(ser, assign.channel, 0x26, lsb);
      }
      else
      {
        sendControlChange(ser, assign.channel, 0x06, lsb);
      }
      break;

    case DataType::NonRegisteredParameter:
      sendControlChange(ser, assign.channel, 0x63, assign.nrpnMsb);
      sendControlChange(ser, assign.channel, 0x62, assign.nrpnLsb);
      if (assign.is14Bit)
      {
        sendControlChange(ser, assign.channel, 0x06, msb);
        sendControlChange(ser, assign.channel, 0x26, lsb);
      }
      else
      {
        sendControlChange(ser, assign.channel, 0x06, lsb);
      }
  }
}

void sendAllValues()
{
  for (int i = 0; i < controllerCount; i++)
  {
    setValue(controllers[i].assignment, controllers[i].value);
  }
}

void clearControllers()
{
  controllerCount = 0;
}

ControllerState* controllerState(const DataAssignment& assign)
{
  for (int i = 0; i < controllerCount; i++)
  {
    if (controlsMatch(controllers[i].assignment, assign))
    {
      return controllers + i;
    }
  }
  return nullptr;
}

ControllerState* addController(const Mapping& mapping)
{
  if (controllerState(mapping.assignment))
  {
    return nullptr;
  }
  controllers[controllerCount] = {
    .assignment = mapping.assignment,
    .value = mapping.initialValue,
  };
  return &controllers[controllerCount++];
}

void initialiseControllers()
{
  clearControllers();
  for (int i = 0; i < kFootSwitchCount; i++)
  {
    const auto& mapping = patch.footSwitchMapping[i];
    if (mapping.mode == kMappingNone)
    {
      continue;
    }
    if (auto state = addController(mapping))
    {
      setValue(state->assignment, state->value);
    }
  }
  for (int i = 0; i < kExtControlCount; i++)
  {
    const auto& mapping = patch.extControlMapping[i];
    if (mapping.mode == kMappingNone)
    {
      continue;
    }
    if (auto state = addController(mapping))
    {
      if (!extConfig[i].isSwitch && mapping.mode == kMappingAnalog)
      {
        extSensors[i].update();
        if (extSensors[i].down())
        {
          state->value = extAnalog[i].value();
        }
      }
      setValue(state->assignment, state->value);
    }
  }

  sendAllValues();
}

void loadPatch(int program) {
#if USB_SERIAL_LOGGING
  String s = String() + "Loading patch for program " + String(program) + "\n";
  CompositeSerial.write(s.c_str());
#endif
  patch.program = 0;

  patch.footSwitchMapping[0] = {};
  patch.footSwitchMapping[0].assignment.type = DataType::Preset,
  patch.footSwitchMapping[0].assignment.scope = 0,
  patch.footSwitchMapping[0].initialValue = 0;
  patch.footSwitchMapping[0].minValue = 0;
  patch.footSwitchMapping[0].maxValue = 1;
  patch.footSwitchMapping[0].mode = kMappingSwitchMomentary;

  patch.footSwitchMapping[1] = {};
  patch.footSwitchMapping[1].assignment.port = MidiPort::Din1,
  patch.footSwitchMapping[1].assignment.type = DataType::ProgramChange,
  patch.footSwitchMapping[1].initialValue = 71;
  patch.footSwitchMapping[1].minValue = 71;
  patch.footSwitchMapping[1].maxValue = 76;
  patch.footSwitchMapping[1].mode = kMappingSwitchToggle;

  patch.footSwitchMapping[2] = {};
  patch.footSwitchMapping[2].assignment.port = MidiPort::Din1,
  patch.footSwitchMapping[2].assignment.type = DataType::ProgramChange,
  patch.footSwitchMapping[2].initialValue = 73;
  patch.footSwitchMapping[2].minValue = 73;
  patch.footSwitchMapping[2].maxValue = 76;
  patch.footSwitchMapping[2].mode = kMappingSwitchToggle;

  patch.extControlMapping[0] = {};
  patch.extControlMapping[0].assignment.port = MidiPort::Usb,
  patch.extControlMapping[0].assignment.type = DataType::ControlChange,
  patch.extControlMapping[0].assignment.controller = 11,
  patch.extControlMapping[0].initialValue = 0;
  patch.extControlMapping[0].minValue = 0;
  patch.extControlMapping[0].maxValue = 127;
  patch.extControlMapping[0].mode = kMappingAnalog;

  patch.extControlMapping[1] = {};
  patch.extControlMapping[1].assignment.port = MidiPort::Din1,
  patch.extControlMapping[1].assignment.type = DataType::ControlChange,
  patch.extControlMapping[1].assignment.controller = 16,
  patch.extControlMapping[1].initialValue = 6;
  patch.extControlMapping[1].minValue = 6;
  patch.extControlMapping[1].maxValue = 64;
  patch.extControlMapping[1].mode = kMappingAnalog;

  patch.extControlMapping[2] = {};
  patch.extControlMapping[2].assignment.port = MidiPort::Din1,
  patch.extControlMapping[2].assignment.type = DataType::ControlChange,
  patch.extControlMapping[2].assignment.controller = 48,
  patch.extControlMapping[2].initialValue = 0;
  patch.extControlMapping[2].minValue = 0;
  patch.extControlMapping[2].maxValue = 127;
  patch.extControlMapping[2].mode = kMappingAnalog;

  patch.extControlMapping[3] = {};
  patch.extControlMapping[3].assignment.port = MidiPort::Usb,
  patch.extControlMapping[3].assignment.type = DataType::ControlChange,
  patch.extControlMapping[3].assignment.controller = 83,
  patch.extControlMapping[3].initialValue = 0;
  patch.extControlMapping[3].minValue = 0;
  patch.extControlMapping[3].maxValue = 127;
  patch.extControlMapping[3].mode = kMappingSwitchToggle;

  patch.scopeCount = 1;
  patch.presetScopes[0].assignmentCount = 2;
  patch.presetScopes[0].assignments[0].port = MidiPort::Din1;
  patch.presetScopes[0].assignments[0].type = DataType::ControlChange;
  patch.presetScopes[0].assignments[0].controller = 5;
  patch.presetScopes[0].assignments[1].port = MidiPort::Din1;
  patch.presetScopes[0].assignments[1].type = DataType::NonRegisteredParameter;
  patch.presetScopes[0].assignments[1].nrpnLsb = 73;
  patch.presetScopes[0].presetCount = 2;
  patch.presetScopes[0].presets[0].values[0] = 0;
  patch.presetScopes[0].presets[0].values[1] = 0;
  patch.presetScopes[0].presets[1].values[0] = 48;
  patch.presetScopes[0].presets[1].values[1] = 2;

  initialiseControllers();
}

bool controlsMatch(const DataAssignment& a1, const DataAssignment& a2)
{
  return
    a1.port == a2.port &&
    a1.channel == a2.channel &&
    a1.is14Bit == a2.is14Bit &&
    a1.type == a2.type &&
    a1.controller == a2.controller &&
    a1.controllerMsb == a2.controllerMsb;
}

void updateControllerLed(Mapping& mapping, ControllerState& state, Colour& led)
{
  if (!controlsMatch(mapping.assignment, state.assignment))
  {
    return;
  }
  auto val = state.assignment.is14Bit ? state.value >> 7 : state.value;
  switch (mapping.mode) {
    case kMappingSwitchZoneUp:
    case kMappingSwitchZoneDown:
    case kMappingSwitchZoneUpCycle:
    case kMappingSwitchZoneDownCycle:
    case kMappingSwitchZoneReset:
      val = valueForZone(zoneForValue(val, mapping.zoneCount, mapping.minValue, mapping.maxValue), mapping.zoneCount, mapping.minValue, mapping.maxValue);
    case kMappingNone:
    case kMappingAnalog:
    case kMappingSwitchToggle:
    case kMappingSwitchMomentary:
    case kMappingSwitchUp:
    case kMappingSwitchDown:
    case kMappingSwitchReset:
      if (val < 32) {
        led = { 0, 95 + val, 127 - (val << 2) };
      }
      else {
        val = val - 32;
        led = { val * 2, 127 - val + (val >> 2), 0 };
      }
      break;
  }
}

bool extControlMapped(int index)
{
  return patch.extControlMapping[index].mode != kMappingNone;
}

bool extControlMappedCorrectly(int index)
{
  if (patch.extControlMapping[index].mode == kMappingNone)
  {
    return true;
  }
  return (patch.extControlMapping[index].mode == kMappingAnalog) == !extConfig[index].isSwitch;
}

void updateControllerLeds()
{
  for (int i = 0; i < kFootSwitchCount; i++)
  {
    leds[kLedFootSwitchMap[i]] = { 0, 0, 0 };
  }
  for (int i = 0; i < kExtControlCount; i++)
  {
    if (extSensors[i].down())
    {
      if (extControlMapped(i))
      {
        if (!extControlMappedCorrectly(i))
        {
          leds[kLedExtControlMap[i]] = { 127, 0, 95 };
        }
      }
      else
      {
        leds[kLedExtControlMap[i]] = { 191, 0, 7 };
      }
    }
    else
    {
      leds[kLedExtControlMap[i]] = { 0, 0, 0 };
    }
  }
  for (int c = 0; c < controllerCount; c++)
  {
    for (int i = 0; i < kFootSwitchCount; i++)
    {
      updateControllerLed(patch.footSwitchMapping[i], controllers[c], leds[kLedFootSwitchMap[i]]);
    }
    for (int i = 0; i < kExtControlCount; i++)
    {
      if (extSensors[i].down() && extControlMapped(i) && extControlMappedCorrectly(i))
      {
        updateControllerLed(patch.extControlMapping[i], controllers[c], leds[kLedExtControlMap[i]]);
      }
    }
  }
}

void displayLeds()
{
  static uint8_t buf[8 + 4 * kLedCount];
  uint8_t brightness = 0x1; // 0-F
  uint8_t global = 0xE0 | brightness;
  uint8_t* out = buf;
  Colour* col = leds;
  *out++ = 0x00;
  *out++ = 0x00;
  *out++ = 0x00;
  *out++ = 0x00;
  for (int i = 0; i < kLedCount; i++)
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
  if (mapping.mode == kMappingNone)
  {
    return;
  }
  auto& state = *controllerState(mapping.assignment);
  if (mapping.mode == kMappingSwitchMomentary)
  {
    state.value = down ? mapping.maxValue : mapping.minValue;
    setValue(state.assignment, state.value);
    return;
  }
  if (!down)
  {
    return;
  }
  switch (mapping.mode)
  {
    case kMappingNone:
    case kMappingAnalog:
    case kMappingSwitchMomentary:
      return;

    case kMappingSwitchToggle:
      state.value = state.value >= ((int)mapping.minValue + (int)mapping.maxValue) / 2 ? mapping.minValue : mapping.maxValue;
      break;

    case kMappingSwitchUp:
      if (state.value >= mapping.maxValue)
      {
        return;
      }
      state.value++;
      break;

    case kMappingSwitchDown:
      if (state.value <= mapping.minValue)
      {
        return;
      }
      state.value--;
      break;

    case kMappingSwitchReset:
      if (state.value == mapping.initialValue)
      {
        return;
      }
      state.value = mapping.initialValue;
      break;

    case kMappingSwitchZoneUp:
      {
        auto zone = zoneForValue(state.value, mapping.zoneCount, mapping.minValue, mapping.maxValue);
        if (zone == mapping.zoneCount - 1)
        {
          return;
        }
        state.value = valueForZone(zone + 1, mapping.zoneCount, mapping.minValue, mapping.maxValue);
        break;
      }

    case kMappingSwitchZoneDown:
      {
        auto zone = zoneForValue(state.value, mapping.zoneCount, mapping.minValue, mapping.maxValue);
        if (zone == 0) {
          return;
        }
        state.value = valueForZone(zone - 1, mapping.zoneCount, mapping.minValue, mapping.maxValue);
        break;
      }

    case kMappingSwitchZoneUpCycle:
      {
        auto zone = zoneForValue(state.value, mapping.zoneCount, mapping.minValue, mapping.maxValue);
        state.value = valueForZone((zone + 1) % mapping.zoneCount, mapping.zoneCount, mapping.minValue, mapping.maxValue);
        break;
      }

    case kMappingSwitchZoneDownCycle:
      {
        auto zone = zoneForValue(state.value, mapping.zoneCount, mapping.minValue, mapping.maxValue);
        state.value = valueForZone((zone + mapping.zoneCount - 1) % mapping.zoneCount, mapping.zoneCount, mapping.minValue, mapping.maxValue);
        break;
      }

    case kMappingSwitchZoneReset:
      {
        if (zoneForValue(state.value, mapping.zoneCount, mapping.minValue, mapping.maxValue) == zoneForValue(mapping.initialValue, mapping.zoneCount, mapping.minValue, mapping.maxValue))
        {
          return;
        }
        state.value = mapping.initialValue;
        break;
      }
  }
  setValue(state.assignment, state.value);
}

void updateAnalogControllerValue(Mapping& mapping, int value)
{
  if (mapping.mode != kMappingAnalog)
  {
    return;
  }
  auto& state = *controllerState(mapping.assignment);
  state.value = min(max(mapping.minValue, value * (mapping.maxValue - mapping.minValue + 1) / 128 + mapping.minValue), mapping.maxValue);
  setValue(state.assignment, state.value);
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
  for (int i = 0; i < kFootSwitchCount; i++)
  {
    updateFootSwitch(i);
  }
  for (int i = 0; i < kExtControlCount; i++)
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
