#pragma once
#include <stdint.h>
#include "RobotStates.h"

//The creep gesture on the speed switch [D-207 point 4], decided on the remote: with the switch at the middle position
//(MEDIUM, slow), a quick flick middle, OFF, middle toggles between slow and creep. It counts when the switch is back at
//middle within windowMillis of leaving it, stayed at OFF at least minOffMillis and both sticks stayed centred from
//leaving middle to coming back. Every other arrival at middle gives slow: from FAST, from OFF after a longer stay, at
//power on, a flick with a stick out of centre; a flick through FAST never counts. A stay at OFF shorter than
//minOffMillis is contact bounce, not a flick: back at middle it keeps the speed held when middle was left. Leaving
//middle clears creep (the frame sends creep only at MEDIUM, UplinkFrame.h). The flick passes through OFF, which streams
//zero (D-93 point 1), so the speed never changes during a move. Switch state 3 (both contacts) counts as OFF, as
//RemoteBoard::isSpeedSwitchOff() reads it. Pure code, host-tested (test_remote_logic); the remote's loop thread.
class SpeedFlick{
public:
    //bench-tuned constants [D-207 point 4]
    static constexpr uint32_t windowMillis = 1000;     //leaving middle to back at middle
    static constexpr uint32_t minOffMillis = 50;       //shortest stay at OFF that is a flick, not bounce

    //speedState: the ThreePositionToggleSwitch state (SpeedMode); b_sticksCentred: both sticks within the remote's
    //centre threshold. Returns isCreep()
    bool update(uint8_t speedState, bool b_sticksCentred, uint32_t now){
        Position position = positionOf(speedState);
        if(!b_started){
            //power on: wherever the switch is, slow at middle
            b_started = true;
            previous = position;
            b_creep = false;
            b_candidate = false;
            return b_creep;
        }
        if(b_candidate && !b_sticksCentred) b_candidate = false;

        if(position != previous){
            if(previous == Position::MIDDLE){
                //leaving middle: creep clears; a flick may start, only towards OFF
                b_creepAtLeave = b_creep;
                b_creep = false;
                b_candidate = position == Position::OFF && b_sticksCentred;
                leftMiddleMillis = now;
                offMillis = now;
                b_bounceOnly = position == Position::OFF;
            }
            else if(position == Position::MIDDLE){
                //arriving at middle from OFF or FAST
                bool b_fromOff = previous == Position::OFF;
                uint32_t offStay = now - offMillis;
                bool b_inWindow = now - leftMiddleMillis <= windowMillis;
                if(b_fromOff && b_bounceOnly && offStay < minOffMillis) b_creep = b_creepAtLeave;
                else if(b_fromOff && b_candidate && b_inWindow && offStay >= minOffMillis) b_creep = !b_creepAtLeave;
                else b_creep = false;
                b_candidate = false;
                b_bounceOnly = false;
            }
            else{
                //OFF and FAST: a flick through FAST never counts
                b_candidate = false;
                b_bounceOnly = false;
            }
            previous = position;
        }
        return b_creep;
    }

    bool isCreep() const { return b_creep; }

private:
    enum class Position : uint8_t{ OFF, MIDDLE, FAST };
    static Position positionOf(uint8_t speedState){
        if(speedState == uint8_t(SpeedMode::MEDIUM_SPEED)) return Position::MIDDLE;
        if(speedState == uint8_t(SpeedMode::HIGH_SPEED)) return Position::FAST;
        return Position::OFF;       //LOW_SPEED and NO_SPEED (state 3)
    }

    bool b_started = false;
    Position previous = Position::OFF;
    bool b_creep = false;
    bool b_creepAtLeave = false;    //the flick toggles from the value held when middle was left
    bool b_candidate = false;       //middle left towards OFF with the sticks centred, nothing broke it since
    bool b_bounceOnly = false;      //middle left straight to OFF and not moved on: a short stay is bounce
    uint32_t leftMiddleMillis = 0;
    uint32_t offMillis = 0;         //when the switch reached OFF from middle
};
