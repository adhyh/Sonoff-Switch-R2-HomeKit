#pragma once
#include <Arduino.h>

#if defined(SONOFF_R4_BASIC)

  #include "hw_basic_r4.h"
  using DeviceHardware = BasicR4Hardware;

#elif defined(SONOFF_R4_MINI)

  #include "hw_mini_r4.h"
  using DeviceHardware = MiniR4Hardware;

#else
  #error Define SONOFF_R4_BASIC or SONOFF_R4_MINI
#endif
