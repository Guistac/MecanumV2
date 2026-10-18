#pragma once
#include <Arduino.h>

//Hardware watchdog (WDOG1) of the Teensy 4.x
//If feed() is not called within the timeout, the MCU is reset.
//The optional warning callback runs from the WDOG1 interrupt 0.5s before the reset,
//so outputs can be brought to a safe state even when the main loop is stuck.
namespace Watchdog{

    //timeout is rounded to 0.5s steps, minimum 1s. Can only be started once.
    void begin(float timeoutSeconds, void (*warningCallback)() = nullptr);

    void feed();

    //delay() that keeps feeding the watchdog
    void delayFed(uint32_t milliseconds);

}
