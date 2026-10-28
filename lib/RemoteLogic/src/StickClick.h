#pragma once
#include <stdint.h>

//A stick button read as a click or a hold (the Tracking page, owner 2026-10-28): CLICK when it is let go before
//holdMillis, HOLD once when it has been held holdMillis (its release then does nothing). Only a press that starts
//while active counts: a button held as the page or the mode changes does nothing until let go. spend() makes the
//press in progress count for nothing more (a question it cancelled).
class StickClick{
public:
    static constexpr uint32_t holdMillis = 1000;

    enum Result : uint8_t{ NONE, PRESSED, CLICK, HOLD };

    Result update(bool b_active, bool b_held, uint32_t now){
        bool b_edge = b_held && !b_was;
        b_was = b_held;
        if(!b_active){
            b_armed = false;
            return NONE;
        }
        if(b_edge){
            b_armed = true;
            b_holdDone = false;
            pressMillis = now;
            return PRESSED;
        }
        if(!b_armed) return NONE;
        if(b_held){
            if(!b_holdDone && now - pressMillis >= holdMillis){
                b_holdDone = true;
                return HOLD;
            }
            return NONE;
        }
        b_armed = false;
        return b_holdDone ? NONE : CLICK;
    }

    void spend(){ b_armed = false; }
    //held, counting towards a hold (the progress the screen shows)
    bool isPressing() const { return b_armed && b_was && !b_holdDone; }
    uint32_t pressedAt() const { return pressMillis; }
    float holdProgress(uint32_t now) const {
        if(!isPressing()) return 0.0f;
        uint32_t held = now - pressMillis;
        return held >= holdMillis ? 1.0f : float(held) / float(holdMillis);
    }

private:
    bool b_was = false;
    bool b_armed = false;
    bool b_holdDone = false;
    uint32_t pressMillis = 0;
};
