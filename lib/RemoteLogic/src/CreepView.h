#pragma once
#include <stdint.h>
#include "RobotStates.h"
#include "RemoteSounds.h"

//What the remote shows and plays for creep [D-207 point 5, D-208 point 5]: the receiver's echo (reply status word bit
//10) with the link up, the remote's own flick state without one. Pure code, host-tested (test_remote_logic); the
//remote's loop thread.
namespace CreepView{

    //CREEP in place of SLOW, with this remote's own switch at middle [D-208 point 5]: with the link up from the
    //receiver's echo (an old receiver never echoes: no CREEP the vehicle cannot run), with no link from the remote's
    //own flick state, as SLOW, FAST, WORLD and BODY come from its own switches
    inline bool isShown(bool b_echo, bool b_localCreep, bool b_linkUp, uint8_t speedState){
        if(speedState != uint8_t(SpeedMode::MEDIUM_SPEED)) return false;
        return b_linkUp ? b_echo : b_localCreep;
    }

    //the 1 Hz flash: the filled pill for the first 0.5 s of each second, plain text for the next 0.5 s. On the
    //millis() wrap (49.7 days) the phase jumps once, as every blink on the screen does
    static constexpr uint32_t flashPeriodMillis = 1000;
    static constexpr uint32_t flashFilledMillis = 500;
    inline bool isPillFilled(uint32_t now){ return now % flashPeriodMillis < flashFilledMillis; }

    //creep on and off follow what the screen shows [D-208 point 5]: one sound whenever the shown creep changes,
    //whatever changed it (the flick, the echo, the link going down or up); none when a link coming up confirms what
    //was already shown
    class ShownSound{
    public:
        RemoteSounds::Sound update(bool b_shown){
            if(b_shown == b_heard) return RemoteSounds::Sound::NONE;
            b_heard = b_shown;
            return b_shown ? RemoteSounds::Sound::STATUS_CREEP_ON : RemoteSounds::Sound::STATUS_CREEP_OFF;
        }
    private:
        bool b_heard = false;
    };
};
