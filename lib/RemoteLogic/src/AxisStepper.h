#pragma once
#include <stdint.h>

//Remote menu navigation with a stick axis: one step at 60 % deflection, re-armed under 30 %, repeated while held
//(first repeat after 0.5 s, then every 0.2 s) [D-147 point 2]
class AxisStepper{
public:

    static constexpr float stepThreshold = 0.6f;
    static constexpr float rearmThreshold = 0.3f;
    static constexpr uint32_t firstRepeatMillis = 500;
    static constexpr uint32_t repeatIntervalMillis = 200;

    //value -1..1, returns -1, 0 or +1
    int8_t update(float value, uint32_t nowMillis){
        if(b_waitForRelease){
            if(value < rearmThreshold && value > -rearmThreshold) b_waitForRelease = false;
            return 0;
        }
        int8_t deflection = 0;
        if(value >= stepThreshold) deflection = 1;
        else if(value <= -stepThreshold) deflection = -1;

        if(direction != 0){
            bool b_released = direction > 0 ? value < rearmThreshold : value > -rearmThreshold;
            if(b_released){
                direction = 0;
                return 0;
            }
            if(deflection == direction && int32_t(nowMillis - nextRepeatMillis) >= 0){
                nextRepeatMillis = nowMillis + repeatIntervalMillis;
                return direction;
            }
            return 0;
        }
        if(deflection == 0) return 0;
        direction = deflection;
        holdStartMillis = nowMillis;
        nextRepeatMillis = nowMillis + firstRepeatMillis;
        return direction;
    }

    //how long the axis has been held to the positive side, 0 when it is not
    uint32_t heldPositiveMillis(uint32_t nowMillis) const {
        return direction > 0 ? nowMillis - holdStartMillis : 0;
    }

    //no step until the axis has come back under the re-arm threshold (after the menu opens or closes)
    void blockUntilReleased(){
        direction = 0;
        b_waitForRelease = true;
    }

private:
    bool b_waitForRelease = false;
    int8_t direction = 0;
    uint32_t holdStartMillis = 0;
    uint32_t nextRepeatMillis = 0;
};
