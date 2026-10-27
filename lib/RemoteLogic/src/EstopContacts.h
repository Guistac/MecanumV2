#pragma once
#include <stdint.h>

enum class EstopState{ CLEAR, PRESSED, WIRING };

//Remote E-stop with a normally open and a normally closed contact, both on pull-ups. Clear only with NO open (high)
//and NC closed (low), as before; any other reading is pressed. Both closed (both low) for longer than 100 ms cannot
//happen with a working switch: a wiring fault (a short), still pressed [D-146 point 3].
//The remotes have only the NC contact connected (owner, 2026-10-27: released NO1 NC0, pressed NO1 NC1), so NO1 NC1 is
//an ordinary press; NO0 NC1 is a press with a working NO contact.
class EstopContacts{
public:

    static constexpr uint32_t wiringFaultMillis = 100;

    EstopState update(bool b_noHigh, bool b_ncHigh, uint32_t nowMillis){
        bool b_bothClosed = !b_noHigh && !b_ncHigh;
        if(!b_bothClosed) b_disagreeing = false;
        else if(!b_disagreeing){
            b_disagreeing = true;
            disagreeStartMillis = nowMillis;
        }

        if(b_noHigh && !b_ncHigh) state = EstopState::CLEAR;
        else if(b_disagreeing && nowMillis - disagreeStartMillis > wiringFaultMillis) state = EstopState::WIRING;
        else state = EstopState::PRESSED;
        return state;
    }

    EstopState getState() const { return state; }
    bool isClear() const { return state == EstopState::CLEAR; }

private:
    EstopState state = EstopState::PRESSED;
    bool b_disagreeing = false;
    uint32_t disagreeStartMillis = 0;
};
