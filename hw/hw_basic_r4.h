#pragma once
#include "hw_base.h"

struct BasicR4Hardware : HardwareBase {
  BasicR4Hardware() : HardwareBase(
    13,  // LED
    12,  // Relay (check PCB revision!)
    0,   // Button
    -1   // No external switch
  ) {}
};
