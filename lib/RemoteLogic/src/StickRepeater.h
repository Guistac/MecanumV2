#pragma once
#include <stdint.h>

//One stick axis stepping a value (the stopwatch's set time [D-160], the marks' number editor): a step on the push
//(60 %), re-armed under 30 %, repeated while held: the first repeat after firstRepeatMillis, then every repeatMillis,
//every fastRepeatMillis once held fastAfterMillis. start() with the stick's value when the stepping begins: a stick
//already out does nothing until it has come back
struct StickRepeater{
    static constexpr float stepThreshold = 0.6f;
    static constexpr float rearmThreshold = 0.3f;
    static constexpr uint32_t firstRepeatMillis = 400;

    int8_t direction = 0;
    bool b_waitForRelease = false;
    uint32_t holdStartMillis = 0;
    uint32_t nextMillis = 0;

    void start(float value){
        direction = 0;
        b_waitForRelease = value >= rearmThreshold || value <= -rearmThreshold;
    }

    //returns -1, 0 or +1
    int8_t update(float value, uint32_t now, uint32_t repeatMillis, uint32_t fastAfterMillis, uint32_t fastRepeatMillis){
        if(b_waitForRelease){
            if(value < rearmThreshold && value > -rearmThreshold) b_waitForRelease = false;
            return 0;
        }
        if(direction != 0){
            bool b_released = direction > 0 ? value < rearmThreshold : value > -rearmThreshold;
            if(b_released){ direction = 0; return 0; }
            if(int32_t(now - nextMillis) < 0) return 0;
            nextMillis = now + (now - holdStartMillis >= fastAfterMillis ? fastRepeatMillis : repeatMillis);
            return direction;
        }
        if(value >= stepThreshold) direction = 1;
        else if(value <= -stepThreshold) direction = -1;
        else return 0;
        holdStartMillis = now;
        nextMillis = now + firstRepeatMillis;
        return direction;
    }
};
