//Host stand-in for the few Arduino definitions Adafruit_GFX needs (screen renderer only, never in the firmware)
#pragma once
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include <stdio.h>
#include <algorithm>
#ifndef ARDUINO
#define ARDUINO 100
#endif
#define PROGMEM
#define pgm_read_byte(addr) (*(const uint8_t *)(addr))
#define pgm_read_word(addr) (*(const uint16_t *)(addr))
#define pgm_read_dword(addr) (*(const uint32_t *)(addr))
#define pgm_read_pointer(addr) ((void *)*(void * const *)(addr))
using std::min;
using std::max;
class __FlashStringHelper;
class String{
public:
    String(const char* s = ""){ text = s; }
    const char* c_str() const { return text; }
    unsigned int length() const { return strlen(text); }
private:
    const char* text;
};
#include "Print.h"
