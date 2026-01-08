# Sonoff Switch HomeKit (HomeSpan)

Custom firmware for Sonoff switch devices to expose them as **native Apple HomeKit switches** using **HomeSpan**.

## What it does

- Native Apple HomeKit switch (no Homebridge)
- Runs on ESP32-based Sonoff switches
- Physical switch **always works**
  - even when Wi-Fi is down
  - even during provisioning
  - even when not paired to HomeKit
- Wi-Fi setup via WiFiManager captive portal
- Shows HomeKit pairing code during setup
- Firmware version shown:
  - in the WiFiManager portal
  - in Apple Home → Accessory Information

No cloud, no accounts, no external services.

---

## Supported hardware

Tested with:

- Sonoff Mini R4
- Sonoff Basic R4

Other Sonoff ESP32-based devices may work with small changes.

Select the hardware at compile time:

```cpp
#define SONOFF_R4_MINI
// #define SONOFF_R4_BASIC
