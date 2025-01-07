#pragma once
#include <Arduino.h>

namespace Safety{
    uint16_t generateSafeNumber();
    uint16_t generateClearNumber();
    bool isNumberClear(uint16_t input);
};