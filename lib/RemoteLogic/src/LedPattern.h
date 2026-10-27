#pragma once
#include <stdint.h>
#include "RobotStates.h"

//Remote enable (right) and disable (left) button LEDs [D-147 point 6]: the D-27 patterns, plus radio fault (both
//blink together at 1 Hz), own E-stop (both on), unknown state (both off). The caller lights a pressed button itself
struct LedInputs{
    bool b_radioFault = false;
    bool b_localEstop = false;
    bool b_finder = false;          //the receiver's function button: remote finder
    bool b_linkUp = false;
    uint8_t reportedState = 0;
};

struct LedOutput{
    bool b_left = false;
    bool b_right = false;
};

namespace Leds{

    inline LedOutput pattern(const LedInputs& in, uint32_t now){
        if(in.b_radioFault) return {now % 1000 < 500, now % 1000 < 500};
        if(in.b_localEstop) return {true, true};
        if(in.b_finder) return {now % 100 < 50, now % 100 > 50};
        if(!in.b_linkUp) return {now % 2000 < 100, (now - 100) % 2000 < 100};
        switch(RobotState(in.reportedState)){
            case RobotState::ENABLED:               return {false, true};
            case RobotState::ENABLING:              return {false, now % 200 < 100};
            case RobotState::DISABLED:              return {true, false};
            case RobotState::DISABLING:             return {now % 200 < 100, false};
            case RobotState::EMERGENCY_STOPPING:    return {true, true};
            case RobotState::EMERGENCY_STOPPED:     return {now % 500 < 250, (now + 250) % 500 < 250};
            default:                                return {false, false};
        }
    }

};
