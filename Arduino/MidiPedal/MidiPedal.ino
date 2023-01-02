#include "Globals.h"


void handleProgramChange(unsigned int channel, unsigned int program) {
  setMidiStatus(kMidiReceiving);
#if USB_SERIAL_LOGGING
  String s = String() + "Received Program Change " + String(program) + " [Ch:" + String(channel + 1) + "]\n";
  CompositeSerial.write(s.c_str());
#endif
}

void handleSysExData(unsigned char data) {
  setMidiStatus(kMidiReceiving);
#if USB_SERIAL_LOGGING
  String s = String() + "Received SysEx byte: 0x" + String(data, 16) + "\n";
  CompositeSerial.write(s.c_str());
#endif
}

void handleSysExEnd() {
  setMidiStatus(kMidiReceiving);
#if USB_SERIAL_LOGGING
  String s = String() + "End of SysEx\n";
  CompositeSerial.write(s.c_str());
#endif
}

void setupExtControllers() {
  for (int i = 0; i < kExtControlCount; i++) {
    auto& conf = extConfig[i];
    if (conf.isSwitch) {
      extSwitch[i].attach(kPinExtControl[i], INPUT_PULLUP);
      extSwitch[i].inverted(conf.inverted);
    }
    else {
      extAnalog[i].setup(kPinExtControl[i], 127);
      extAnalog[i].configure(conf.rawMin, conf.rawMax, conf.inverted, false);
    }
  }
}

void loadExtConfig() {
  extConfig[0] = {
    .isSwitch = false,
    .inverted = true,
    .rawMin = 0,
    .rawMax = 1320,
  };
  extConfig[2] = extConfig[1] = extConfig[0];
  extConfig[3] = {
    .isSwitch = true,
    .inverted = true,
    .rawMin = 0,
    .rawMax = 1320,
  };
  setupExtControllers();
}

void setupUsb() {
  USBComposite.clear();
  USBComposite.setManufacturerString("Tranzistor Bandicoot");
  USBComposite.setProductString("Footsy");
  USBComposite.setVendorId(0xc007);
  USBComposite.setProductId(0xf754);
  midi.registerComponent();
  midi.setProgramChangeCallback(handleProgramChange);
  midi.setSysExCallbacks(handleSysExData, handleSysExEnd);
#if USB_SERIAL_LOGGING
  CompositeSerial.registerComponent();
#endif
  USBComposite.begin();
#if USB_SERIAL_LOGGING
  delay(2000);
  CompositeSerial.write("Footsy by Tranzistor Bandicoot\n");
#endif
}

void setupIoPins() {
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, HIGH);
  ledPort.configureStrip(0, kLedPinData, kLedPinClock);
  for (int i = 0; i < kFootSwitchCount; i++) {
    footSwitches[i].attach(kPinFootSwitch[i], INPUT_PULLUP);
    footSwitches[i].enableDoubleTap(false);
    footSwitches[i].enableHold(false);
  }
  for (int i = 0; i < kExtControlCount; i++) {
    extSensors[i].attach(kPinExtSense[i], INPUT_PULLDOWN);
    extSensors[i].enableDoubleTap(false);
    extSensors[i].enableHold(false);
  }
}

void sendController(const ControllerState& state) {
  setMidiStatus(kMidiSending);
  midi.sendControlChange(state.ch, state.cc, state.val);
#if USB_SERIAL_LOGGING
  String s = String() + "Ch:" + String(state.ch + 1) + " CC:" + String(state.cc) + " Val:" + state.val + "\n";
  CompositeSerial.write(s.c_str());
#endif
}

void sendMidiControllers() {
  for (int i = 0; i < controllerCount; i++) {
    sendController(controllers[i]);
  }
}

void clearControllers() {
  controllerCount = 0;
}

ControllerState* controllerState(int cc) {
  for (int i = 0; i < controllerCount; i++) {
    if (controllers[i].cc == cc) {
      return controllers + i;
    }
  }
  return nullptr;
}

ControllerState* addController(const Mapping& mapping) {
  if (controllerState(mapping.controller)) {
    return nullptr;
  }
  controllers[controllerCount] = {
    .ch = mapping.channel,
    .cc = mapping.controller,
    .val = mapping.initialValue,
  };
  return &controllers[controllerCount++];
}

void initialiseControllers() {
  clearControllers();
  for (int i = 0; i < kFootSwitchCount; i++) {
    const auto& mapping = patch.footSwitchMapping[i];
    if (mapping.mode == kMappingNone) {
      continue;
    }
    if (auto state = addController(mapping)) {
      sendController(*state);
    }
  }
  for (int i = 0; i < kExtControlCount; i++) {
    const auto& mapping = patch.extControlMapping[i];
    if (mapping.mode == kMappingNone) {
      continue;
    }
    if (auto state = addController(mapping)) {
      if (!extConfig[i].isSwitch && mapping.mode == kMappingAnalog) {
        extSensors[i].update();
        if (extSensors[i].down()) {
          state->val = extAnalog[i].value();
        }
      }
      sendController(*state);
    }
  }
  if (midiConnected) {
    sendMidiControllers();
  }
}

void loadPatch(int program) {
#if USB_SERIAL_LOGGING
  String s = String() + "Loading patch for program " + String(program) + "\n";
  CompositeSerial.write(s.c_str());
#endif
  patch.program = 0;
  patch.footSwitchMapping[0] = { kMappingSwitchToggle, 0, 80, 0, 2 };
  patch.footSwitchMapping[1] = { kMappingSwitchToggle, 0, 81, 0, 2 };
  patch.footSwitchMapping[2] = { kMappingSwitchToggle, 0, 82, 0, 2 };
  patch.extControlMapping[0] = { kMappingAnalog, 0, 11, 0, 2 };
  patch.extControlMapping[1] = { kMappingAnalog, 0, 16, 0, 2 };
  patch.extControlMapping[2] = { kMappingAnalog, 0, 17, 0, 2 };
  patch.extControlMapping[3] = { kMappingSwitchToggle, 0, 83, 0, 2 };
  initialiseControllers();
}

void updateControllerLed(Mapping& mapping, ControllerState& state, Colour& led) {
  if (mapping.controller != state.cc) {
    return;
  }
  auto val = state.val;
  switch (mapping.mode) {
    case kMappingNone:
      break;
    case kMappingSwitchToggle:
    case kMappingSwitchMomentary:
      if (val < 64) {
        led = { 0, 95, 127 };
      }
      else {
        led = { 191, 64, 0 };
      }
      break;
    case kMappingSwitchZoneUp:
    case kMappingSwitchZoneDown:
    case kMappingSwitchZoneUpCycle:
    case kMappingSwitchZoneDownCycle:
    case kMappingSwitchZoneReset:
      val = valueForZone(zoneForValue(val, mapping.zoneCount), mapping.zoneCount);
    case kMappingAnalog:
    case kMappingSwitchUp:
    case kMappingSwitchDown:
    case kMappingSwitchReset:
      if (val < 32) {
        led = { 0, 95 + state.val, 127 - (state.val << 2) };
      }
      else {
        val = state.val - 32;
        led = { val * 2, 127 - val + (val >> 2), 0 };
      }
      break;
  }
}

bool extControlMapped(int index) {
  return patch.extControlMapping[index].mode != kMappingNone;
}

bool extControlMappedCorrectly(int index) {
  if (patch.extControlMapping[index].mode == kMappingNone) {
    return true;
  }
  return (patch.extControlMapping[index].mode == kMappingAnalog) == !extConfig[index].isSwitch;
}

void updateControllerLeds() {
  for (int i = 0; i < kFootSwitchCount; i++) {
    leds[kLedFootSwitchMap[i]] = { 0, 0, 0 };
  }
  for (int i = 0; i < kExtControlCount; i++) {
    if (extSensors[i].down()) {
      if (extControlMapped(i)) {
        if (!extControlMappedCorrectly(i)) {
          leds[kLedExtControlMap[i]] = { 127, 0, 95 };
        }
      }
      else {
        leds[kLedExtControlMap[i]] = { 191, 0, 7 };
      }
    }
    else {
      leds[kLedExtControlMap[i]] = { 0, 0, 0 };
    }
  }
  for (int c = 0; c < controllerCount; c++) {
    for (int i = 0; i < kFootSwitchCount; i++) {
      updateControllerLed(patch.footSwitchMapping[i], controllers[c], leds[kLedFootSwitchMap[i]]);
    }
    for (int i = 0; i < kExtControlCount; i++) {
      if (extSensors[i].down() && extControlMapped(i) && extControlMappedCorrectly(i)) {
        updateControllerLed(patch.extControlMapping[i], controllers[c], leds[kLedExtControlMap[i]]);
      }
    }
  }
}

void displayLeds() {
  static uint8_t buf[8 + 4 * kLedCount];
  uint8_t brightness = 0x1; // 0-F
  uint8_t global = 0xE0 | brightness;
  uint8_t* out = buf;
  Colour* col = leds;
  *out++ = 0x00;
  *out++ = 0x00;
  *out++ = 0x00;
  *out++ = 0x00;
  for (int i = 0; i < kLedCount; i++) {
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

void setup() {
  setupUsb();
  setupIoPins();
  loadExtConfig();
  loadPatch(0);
}

void setMidiStatus(UsbMidiStatus status) {
  midiStatusUpdated = millis(); 
  midiStatus = status;
}

void updateMidiStatus() {
  if (!midiConnected || millis() >= midiStatusUpdated + kMidiStatusStrobe) {
    setMidiStatus(midiConnected ? kMidiConnected : kMidiDisconnected);
  }
  leds[kLedStatus] = kMidiStatusColors[midiStatus];
}

int8_t zoneForValue(int value, int count) {
  if (count < 2) {
    return 0;
  }
  return (int8_t)min(value * count / 128, count - 1);
}

int8_t valueForZone(int zone, int count) {
  if (count < 2) {
    return 0;
  }
  return (int8_t)min(zone * 128 / (count - 1), 127);
}

void updateButtonControllerValue(Mapping& mapping, bool down) {
  if (mapping.mode == kMappingNone) {
    return;
  }
  auto& state = *controllerState(mapping.controller);
  if (mapping.mode == kMappingSwitchMomentary) {
    state.val = down ? 127 : 0;
    sendController(state);
    return;
  }
  if (!down) {
    return;
  }
  switch (mapping.mode) {
    case kMappingNone:
    case kMappingAnalog:
    case kMappingSwitchMomentary:
      return;
    case kMappingSwitchToggle:
      state.val = state.val >= 64 ? 0 : 127;
      break;
    case kMappingSwitchUp:
      if (state.val == 127) {
        return;
      }
      state.val++;
      break;
    case kMappingSwitchDown:
      if (state.val == 0) {
        return;
      }
      state.val--;
      break;
    case kMappingSwitchReset:
      if (state.val == mapping.initialValue) {
        return;
      }
      state.val = mapping.initialValue;
      break;
    case kMappingSwitchZoneUp:
      {
        auto zone = zoneForValue(state.val, mapping.zoneCount);
        if (zone == mapping.zoneCount - 1) {
          return;
        }
        state.val = valueForZone(zone + 1, mapping.zoneCount);
        break;
      }
    case kMappingSwitchZoneDown:
      {
        auto zone = zoneForValue(state.val, mapping.zoneCount);
        if (zone == 0) {
          return;
        }
        state.val = valueForZone(zone - 1, mapping.zoneCount);
        break;
      }
    case kMappingSwitchZoneUpCycle:
      {
        auto zone = zoneForValue(state.val, mapping.zoneCount);
        state.val = valueForZone((zone + 1) % mapping.zoneCount, mapping.zoneCount);
        break;
      }
    case kMappingSwitchZoneDownCycle:
      {
        auto zone = zoneForValue(state.val, mapping.zoneCount);
        state.val = valueForZone((zone + mapping.zoneCount - 1) % mapping.zoneCount, mapping.zoneCount);
        break;
      }
    case kMappingSwitchZoneReset:
      {
        if (zoneForValue(state.val, mapping.zoneCount) == zoneForValue(mapping.initialValue, mapping.zoneCount)) {
          return;
        }
        state.val = mapping.initialValue;
        break;
      }
  }
  sendController(state);
}

void updateAnalogControllerValue(Mapping& mapping, int value) {
  if (mapping.mode != kMappingAnalog) {
    return;
  }
  auto& state = *controllerState(mapping.controller);
  state.val = value;
  sendController(state);
}

void setFootSwitchDown(int index, bool down) {
  updateButtonControllerValue(patch.footSwitchMapping[index], down);
}

void setExtSwitchDown(int index, bool down) {
  updateButtonControllerValue(patch.extControlMapping[index], down);
}

void setExtAnalogValue(int index, int value) {
  updateAnalogControllerValue(patch.extControlMapping[index], value);
}

void updateFootSwitch(int index) {
  footSwitches[index].update();
  if (footSwitches[index].rose() || footSwitches[index].fell()) {
    setFootSwitchDown(index, footSwitches[index].pressed());
  }
}

void setExtControlEnabled(int index, bool enabled) {
  if (enabled) {
    if (extConfig[index].isSwitch) {
      setExtSwitchDown(index, extSwitch[index].pressed());
    }
    else {
      setExtAnalogValue(index, extAnalog[index].value());
    }
  }
}

void updateExtControl(int index) {
  extSensors[index].update();
  if (extSensors[index].rose() || extSensors[index].fell()) {
#if USB_SERIAL_LOGGING
    String s = String() + "External Pedal #" + String(index + 1) + " " + (extSensors[index].pressed() ? "plugged\n" :  "unplugged\n");
    CompositeSerial.write(s.c_str());
#endif
    setExtControlEnabled(index, extSensors[index].pressed());
  }
  if (extSensors[index].down()) {
    if (extConfig[index].isSwitch) {
      extSwitch[index].update();
      if (extSwitch[index].rose() || extSwitch[index].fell()) {
        setExtSwitchDown(index, extSwitch[index].pressed());
      }
    }
    else {
      if (extAnalog[index].update()) {
        setExtAnalogValue(index, extAnalog[index].value());
      }
    }
  }
}

void updateInputs() {
  for (int i = 0; i < kFootSwitchCount; i++) {
    updateFootSwitch(i);
  }
  for (int i = 0; i < kExtControlCount; i++) {
    updateExtControl(i);
  }
}

void setMidiConnected(bool connected) {
  if (midiConnected == connected) {
    return;
  }
  midiConnected = connected;
  if (midiConnected) {
#if USB_SERIAL_LOGGING
    CompositeSerial.write("USB MIDI Connected.\n");
#endif
    sendMidiControllers();
  }
}

void pollMidi() {
  midi.poll();
  setMidiConnected(midi.isConnected());
}

void loop() {
  pollMidi();
  updateInputs();
  updateMidiStatus();
  updateControllerLeds();
  displayLeds();
}
