//Host stand-in for Arduino's Print (screen renderer only)
#pragma once
#include <stdint.h>
#include <stddef.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
class Print{
public:
    virtual ~Print(){}
    virtual size_t write(uint8_t) = 0;
    virtual size_t write(const uint8_t* buffer, size_t size){
        size_t n = 0;
        while(size--) n += write(*buffer++);
        return n;
    }
    size_t write(const char* text){ return write((const uint8_t*)text, strlen(text)); }
    size_t print(const char* text){ return write(text); }
    size_t print(char c){ return write(uint8_t(c)); }
    size_t printf(const char* format, ...){
        char buffer[128];
        va_list args;
        va_start(args, format);
        vsnprintf(buffer, sizeof(buffer), format, args);
        va_end(args);
        return write(buffer);
    }
};
