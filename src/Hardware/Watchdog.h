#pragma once
#include <Arduino.h>
#include <BootGuard.h>

//Teensy 4 hardware watchdog WDOG1 and the cause of the last reset [D-146, H13]. A hang reboots after the timeout;
//a fault too: the core's fault handler waits 8 s with interrupts off and never feeds it.
namespace Watchdog{

    inline void feed(){
        WDOG1_WSR = 0x5555;
        WDOG1_WSR = 0xAAAA;
    }

    //WT counts half seconds from 0.5 s: 1 = 1 s. WDE can be set only once until the next reset.
    inline void begin(){
        CCM_CCGR3 |= CCM_CCGR3_WDOG1(CCM_CCGR_ON);
        WDOG1_WMCR = 0;     //power-down counter off (it would reset after 16 s)
        //SRS and WDA high: writing 0 would trigger a software reset / assert WDOG_B
        WDOG1_WCR = WDOG_WCR_WT(1) | WDOG_WCR_WDE | WDOG_WCR_SRS | WDOG_WCR_WDA;
        feed();
    }

    //reads and clears the reset status, so the next boot sees only its own cause.
    //A crash: the core's fault handler stores its crash report first, then waits 8 s, so the watchdog resets the chip
    //before the handler marks SRC_GPR5; the report (checked by CRC, garbage after a power-on) tells it.
    //A power-on on Teensy 4 shows as a normal reset (the bootloader chip manages it), as does an upload.
    inline ResetCause readResetCause(){
        uint32_t status = SRC_SRSR;
        ResetCause cause = ResetCause::POWER_ON;
        if(CrashReport) cause = ResetCause::CRASH;
        else if(status & SRC_SRSR_WDOG_RST_B) cause = ResetCause::WATCHDOG;
        CrashReport.clear();    //also clears SRC_SRSR and SRC_GPR5
        return cause;
    }

    inline const char* text(ResetCause cause){
        switch(cause){
            case ResetCause::WATCHDOG: return "WATCHDOG";
            case ResetCause::CRASH: return "CRASH";
            default: return "POWER ON";
        }
    }

};
