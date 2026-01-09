#pragma once

#include "hw_base.h"   // moet absoluut eerst

struct MiniR4Hardware : HardwareBase {
  MiniR4Hardware() : HardwareBase(
    19,  // LED
    26,  // Relay
    0,   // Button
    27,  // S2
    true // LED active low
  ) {}
};
