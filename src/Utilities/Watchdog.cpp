#include "Watchdog.h"

//WDOG1 interrupt control register bits (not defined in imxrt.h)
#define WDOG_WICR_WIE       ((uint16_t)(1 << 15))
#define WDOG_WICR_WTIS      ((uint16_t)(1 << 14))
#define WDOG_WICR_WICT(n)   ((uint16_t)((n) & 0xFF))

namespace Watchdog{

    static bool b_started = false;
    static void (*warning)() = nullptr;

    static void warningIsr(){
        WDOG1_WICR |= WDOG_WICR_WTIS; //clear the interrupt flag, don't feed: the reset must still happen
        if(warning) warning();
    }

    void begin(float timeoutSeconds, void (*warningCallback)()){
        if(b_started) return; //the enable bit is write once
        b_started = true;

        //timeout = (WT + 1) * 0.5s
        int wt = int(timeoutSeconds * 2.0 + 0.5) - 1;
        wt = constrain(wt, 1, 255);

        CCM_CCGR3 |= CCM_CCGR3_WDOG1(CCM_CCGR_ON);
        WDOG1_WMCR = 0; //disable the power down counter

        if(warningCallback){
            warning = warningCallback;
            attachInterruptVector(IRQ_WDOG1, warningIsr);
            NVIC_SET_PRIORITY(IRQ_WDOG1, 0); //must preempt everything else
            NVIC_ENABLE_IRQ(IRQ_WDOG1);
            WDOG1_WICR = WDOG_WICR_WIE | WDOG_WICR_WICT(1); //interrupt 0.5s before the timeout
        }

        //SRS and WDA must stay set, writing 0 to them asserts a reset
        //WDBG: pause while the core is halted, the Teensy bootloader halts it to start an upload
        WDOG1_WCR = WDOG_WCR_WT(wt) | WDOG_WCR_SRS | WDOG_WCR_WDA | WDOG_WCR_WDBG | WDOG_WCR_WDE;
        feed();
    }

    void feed(){
        if(!b_started) return;
        //the two service writes must not be split by an interrupt
        uint32_t primask;
        __asm__ volatile("mrs %0, primask" : "=r" (primask));
        __disable_irq();
        WDOG1_WSR = 0x5555;
        WDOG1_WSR = 0xAAAA;
        if(!(primask & 1)) __enable_irq();
    }

    void delayFed(uint32_t milliseconds){
        uint32_t start = millis();
        while(millis() - start < milliseconds){
            feed();
            delay(1);
        }
        feed();
    }

}
