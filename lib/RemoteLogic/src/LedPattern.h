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
    float leftLevel = 1.0f;         //brightness while on (the NO LINK echoes are fainter)
    float rightLevel = 1.0f;
};

namespace Leds{

    //NO LINK, every 2 s in time with the searching sound (RemoteSounds, SEARCHING): left then right at full for
    //100 ms, then three echoes of the pair, 70 ms each, fainter (25, 15, 4 %: the eye reads LED brightness
    //logarithmically, so they fall faster than the sound's 70, 35, 15 %)
    inline LedOutput noLink(uint32_t now){
        static const uint16_t starts[4] = {0, 300, 600, 900};
        static const uint16_t lengths[4] = {100, 70, 70, 70};
        static const float levels[4] = {1.0f, 0.25f, 0.15f, 0.04f};
        uint32_t phase = now % 2000;
        LedOutput out;
        for(uint8_t i = 0; i < 4; i++){
            if(phase >= starts[i] && phase < uint32_t(starts[i] + lengths[i])){
                out.b_left = true;
                out.leftLevel = levels[i];
            }
            uint32_t right = starts[i] + 100u;     //the right LED 100 ms after the left, as the second note
            if(phase >= right && phase < right + lengths[i]){
                out.b_right = true;
                out.rightLevel = levels[i];
            }
        }
        return out;
    }

    inline LedOutput pattern(const LedInputs& in, uint32_t now){
        if(in.b_radioFault) return {now % 1000 < 500, now % 1000 < 500};
        if(in.b_localEstop) return {true, true};
        if(in.b_finder) return {now % 100 < 50, now % 100 > 50};
        if(!in.b_linkUp) return noLink(now);
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
