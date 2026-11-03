#pragma once
#include <stdint.h>
#include "RobotStates.h"
#include "RemoteSounds.h"

//What the remote shows and plays for creep [D-207 point 5], from the receiver's echo (reply status word bit 10). Pure
//code, host-tested (test_remote_logic); the remote's loop thread.
namespace CreepView{

    //CREEP in place of SLOW: the echo, the link up, and this remote's own switch at middle (the echo lags a frame
    //when the switch leaves middle)
    inline bool isShown(bool b_echo, bool b_linkUp, uint8_t speedState){
        return b_echo && b_linkUp && speedState == uint8_t(SpeedMode::MEDIUM_SPEED);
    }

    //the 1 Hz flash: the filled pill for the first 0.5 s of each second, plain text for the next 0.5 s. On the
    //millis() wrap (49.7 days) the phase jumps once, as every blink on the screen does
    static constexpr uint32_t flashPeriodMillis = 1000;
    static constexpr uint32_t flashFilledMillis = 500;
    inline bool isPillFilled(uint32_t now){ return now % flashPeriodMillis < flashFilledMillis; }

    //creep on and off from the echo: a sound when the echo changes with the link up; a lost link clears it in
    //silence, so a reconnection with creep still held plays creep on again
    class EchoSound{
    public:
        RemoteSounds::Sound update(bool b_connected, bool b_echoReceived){
            bool b_echo = b_connected && b_echoReceived;
            if(b_echo == b_heard) return RemoteSounds::Sound::NONE;
            b_heard = b_echo;
            if(!b_connected) return RemoteSounds::Sound::NONE;
            return b_echo ? RemoteSounds::Sound::STATUS_CREEP_ON : RemoteSounds::Sound::STATUS_CREEP_OFF;
        }
    private:
        bool b_heard = false;
    };
};
