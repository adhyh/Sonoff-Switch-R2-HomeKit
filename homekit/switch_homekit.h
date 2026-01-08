// homekit/switch_homekit.h
#pragma once

#include <HomeSpan.h>

// globale hardware instance komt uit je .ino
extern DeviceHardware hw;

struct SwitchHomeKit : Service::Switch {

  SpanCharacteristic *power;

  SwitchHomeKit(bool initial) : Service::Switch() {
    power = new Characteristic::On(initial);
  }

  // HomeKit -> device
 boolean update() override {

  bool requested = power->getNewVal();

  Serial.print(F("[HK] update requested="));
  Serial.print(requested);
  Serial.print(F("  logical="));
  Serial.println(hw.logicalState);

  // 🔑 CRUCIAAL: negeer HomeKit-echo
  if (requested == hw.logicalState) {
    Serial.println(F("[HK] echo ignored"));
    return true;
  }

  hw.applyFromHomeKit(requested);
  return true;
}


  // device -> HomeKit
  void sync(bool on) {
    power->setVal(on);
  }
};
