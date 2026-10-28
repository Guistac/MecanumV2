#pragma once
#include <stdint.h>
#include "RobotStates.h"
#include "ReplyFrame.h"

//Remote enable (right) and disable (left) button LEDs [D-147 point 6]: the D-27 patterns as amended by the owner on
//2026-10-28 (pattern()), plus radio fault (both blink together at 1 Hz), unknown state (both off). The caller lights a
//pressed button itself
struct LedInputs{
    bool b_radioOff = false;        //the channel setting is Off: both dark [owner 2026-10-28]
    bool b_radioFault = false;
    bool b_localEstop = false;
    bool b_finder = false;          //the receiver's function button: remote finder
    bool b_linkUp = false;
    uint8_t reportedState = 0;
    //[D-163] Stacato's finer state: its disable ramp also arrives as the 3 bit E-stopping, this tells it apart
    Reply::VehicleState vehicle = Reply::VehicleState::UNKNOWN;
};

struct LedOutput{
    bool b_left = false;
    bool b_right = false;
    float leftLevel = 1.0f;         //brightness while on (the NO LINK echoes are fainter)
    float rightLevel = 1.0f;
};

namespace Leds{

    //a state on its way (enabling, disabling): its button flashes 60 ms on, 60 ms off, about 8 Hz
    static constexpr uint32_t changingPeriodMillis = 120;
    inline bool changingFlash(uint32_t now){ return now % changingPeriodMillis < changingPeriodMillis / 2; }

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

    //any E-stop (own, its wiring, or Stacato E-stopped): both together, 500 ms on in every 800 ms, in step with the
    //E-STOP banner's pulse (RemoteScreens: blink(now, 800, 500))
    static constexpr uint32_t estopPeriodMillis = 800;
    static constexpr uint32_t estopOnMillis = 500;
    inline bool estopFlash(uint32_t now){ return now % estopPeriodMillis < estopOnMillis; }
    //an E-stop ramp running: both together, very fast (40 ms on, 40 off)
    inline bool rampFlash(uint32_t now){ return now % 80 < 40; }

    //no Stacato: a standby breathing, the two buttons fading in turn over 2 s (a triangle wave, one rising while the
    //other falls)
    inline LedOutput standby(uint32_t now){
        float phase = float(now % 2000) / 2000.0f;
        float rise = phase < 0.5f ? phase * 2.0f : (1.0f - phase) * 2.0f;
        LedOutput out;
        out.b_left = true;
        out.b_right = true;
        out.leftLevel = 0.05f + 0.95f * rise;
        out.rightLevel = 0.05f + 0.95f * (1.0f - rise);
        return out;
    }

    //Stacato's fieldbus off: both together, a slow dim breathing (3 s, at most 30 %)
    inline LedOutput busOff(uint32_t now){
        float phase = float(now % 3000) / 3000.0f;
        float rise = phase < 0.5f ? phase * 2.0f : (1.0f - phase) * 2.0f;
        LedOutput out;
        out.b_left = true;
        out.b_right = true;
        out.leftLevel = out.rightLevel = 0.02f + 0.28f * rise;
        return out;
    }

    //[D-163] the patterns, owner 2026-10-28 (amends D-27): the finer vehicle state picks them where the 3 bit state
    //lumps states together (Stacato's BOOT, POWEROFF, UNINIT and both ramps arrive as E-stopping)
    inline LedOutput pattern(const LedInputs& in, uint32_t now){
        if(in.b_radioOff) return {false, false};
        if(in.b_radioFault) return {now % 1000 < 500, now % 1000 < 500};
        if(in.b_localEstop) return {estopFlash(now), estopFlash(now)};
        if(in.b_finder) return {now % 100 < 50, now % 100 > 50};
        if(!in.b_linkUp) return noLink(now);
        switch(in.vehicle){
            case Reply::VehicleState::NO_STACATO: return standby(now);
            case Reply::VehicleState::BUS_OFF: return busOff(now);
            case Reply::VehicleState::VEHICLE_OFF: return {false, false};
            case Reply::VehicleState::BUS_START: return {changingFlash(now), changingFlash(now)};
            default: break;
        }
        switch(RobotState(in.reportedState)){
            case RobotState::ENABLED:               return {false, true};
            case RobotState::ENABLING:              return {false, changingFlash(now)};
            case RobotState::DISABLED:              return {true, false};
            case RobotState::DISABLING:             return {changingFlash(now), false};
            case RobotState::EMERGENCY_STOPPING:
                //a normal disable ramp: the disabling flash; an E-stop ramp: both together, very fast
                if(in.vehicle == Reply::VehicleState::DISABLING) return {changingFlash(now), false};
                return {rampFlash(now), rampFlash(now)};
            case RobotState::EMERGENCY_STOPPED:
                //an axis fault (EMERGENCY while armed): alternating at 2 Hz; an E-stop: together, slowly, with the banner
                if(in.vehicle == Reply::VehicleState::AXIS_FAULT) return {now % 500 < 250, (now + 250) % 500 < 250};
                return {estopFlash(now), estopFlash(now)};
            default:                                return {false, false};
        }
    }

};
