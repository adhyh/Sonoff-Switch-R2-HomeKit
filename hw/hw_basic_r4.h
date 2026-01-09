#pragma once
#include "hw_base.h"

struct BasicR4Hardware : HardwareBase {
  BasicR4Hardware() : HardwareBase(
    6,  // LED
    4,  // Relay (check PCB revision!)
    9,   // Button
    -1   // No external switch
  ) {}
};
