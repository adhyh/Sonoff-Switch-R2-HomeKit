#pragma once
#include <Arduino.h>

// Alleen ESP32 classic toegestaan
#if !CONFIG_IDF_TARGET_ESP32
  #error This firmware supports only ESP32 classic
#endif

#if defined(SONOFF_R4_BASIC)

  #include "hw_basic_r4.h"
  using DeviceHardware = BasicR4Hardware;

#elif defined(SONOFF_R4_MINI)

  #include "hw_mini_r4.h"
  using DeviceHardware = MiniR4Hardware;

#else
  #error Define SONOFF_R4_BASIC or SONOFF_R4_MINI
#endif
