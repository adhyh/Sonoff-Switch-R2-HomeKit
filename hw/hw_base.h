#pragma once

#include <Arduino.h>
#include <Bounce2.h>
#include <functional>

struct HardwareBase {

  // ================= GPIO =================
  const int ledPin;
  const int relayPin;
  const int buttonPin;
  const int switchPin;
  const bool ledActiveLow; 

  // ================= Debounce =================
  Bounce debButton;
  Bounce debSwitch;

  // ================= State =================
  bool logicalState = false;   // ENIGE bron van waarheid

  // ================= Modes =================
  bool toggleMode = false;     // hotel / toggle wiring
  bool invertRelay = false;    // NEW: invert physical output

  // ================= Timing =================
  unsigned long bootMs = 0;
  unsigned long pressStart = 0;
  bool longPressArmed = false;
  static constexpr unsigned long LONGPRESS_MS = 5000;

  // Toggle lockout
  unsigned long lastToggleMs = 0;
  static constexpr unsigned long TOGGLE_LOCKOUT_MS = 200;

  // ================= HomeKit =================
  bool homekitActive = false;

  // ================= Callbacks =================
  std::function<void(bool)> onToggle = nullptr;
  std::function<void()> onLongPress = nullptr;

  // ================= Constructor =================
  HardwareBase(int led, int relay, int button, int sw, bool ledLow)
  : ledPin(led), relayPin(relay), buttonPin(button), switchPin(sw), ledActiveLow(ledLow) {}

  // ================= Init =================
  void begin() {

    bootMs = millis();
    lastToggleMs = 0;
    pressStart = 0;
    longPressArmed = false;

    pinMode(ledPin, OUTPUT);
    pinMode(relayPin, OUTPUT);
    pinMode(buttonPin, INPUT_PULLUP);
    pinMode(switchPin, INPUT_PULLUP);

    debButton.attach(buttonPin);
    debButton.interval(30);

    debSwitch.attach(switchPin);
    debSwitch.interval(30);

    setState(false, false);

    Serial.println(F("[HW] init complete"));
  }

  // ================= Core setter =================
  void setState(bool on, bool notifyHK) {

    Serial.print(F("[HW] setState: "));
    Serial.print(logicalState);
    Serial.print(F(" -> "));
    Serial.print(on);
    Serial.print(F("  notifyHK="));
    Serial.println(notifyHK);

    logicalState = on;

    // fysieke aansturing (active LOW), met optionele invert
    bool physicalOn = invertRelay ? on : !on;
    bool ledOn = ledActiveLow ? on : !on;

    digitalWrite(relayPin, physicalOn ? LOW : HIGH);
    digitalWrite(ledPin, ledOn ? LOW : HIGH);

    if (notifyHK && homekitActive && onToggle) {
      Serial.println(F("[HW] notifying HomeKit"));
      onToggle(on);
    }
  }

  // HomeKit → device
  void applyFromHomeKit(bool on) {
    Serial.print(F("[HK->HW] requested="));
    Serial.println(on);
    setState(on, false);
  }

  // Hardware → device (+ HomeKit sync)
  void applyFromHardware(bool on) {
    Serial.print(F("[HW->SYS] requested="));
    Serial.println(on);
    setState(on, true);
  }

  // ================= Main poll =================
  void poll() {

    debButton.update();
    debSwitch.update();

    // arm long-press after boot
    if (!longPressArmed && millis() - bootMs > 3000) {
      longPressArmed = true;
      Serial.println(F("[HW] long-press armed"));
    }

    // ==================================================
    // EXTERNAL SWITCH (S2)
    // ==================================================
    if (debSwitch.changed()) {

      bool requested = (debSwitch.read() == LOW);

      if (toggleMode) {
        // hotel / impuls
        applyFromHardware(!logicalState);
      } else {
        // non-toggle: last-writer-wins
        if (requested != logicalState) {
          applyFromHardware(requested);
        }
      }
    }

    // ==================================================
    // BUTTON (on-device)
    // ==================================================
    if (debButton.fell()) {
      pressStart = millis();
      Serial.println(F("[BTN] fell"));
    }

    if (debButton.read() == LOW && longPressArmed && pressStart) {
      if (millis() - pressStart >= LONGPRESS_MS) {
        pressStart = 0;
        Serial.println(F("[BTN] long-press"));
        if (onLongPress) onLongPress();
      }
    }

    if (debButton.rose()) {
      if (pressStart) {
        Serial.println(F("[BTN] short-press -> toggle"));
        applyFromHardware(!logicalState);
        pressStart = 0;
      }
    }
  }
};
