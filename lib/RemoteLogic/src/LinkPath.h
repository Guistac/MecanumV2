#pragma once
#include <stdint.h>
#include "RobotStates.h"

//Which way the remote's frames go [D-159]: over the radio to the receiver, or over its own USB to Stacato, the remote
//then doing the receiver's job itself (MAVLink). USB takes over once Stacato's heartbeat has arrived on the USB for
//usbSettleMillis without a gap, and not while a vehicle is enabled, enabling or ramping down over the radio: it waits
//for that to end, never cuts a vehicle that may be moving. A ramp counts for rampMaxMillis at most: Stacato also
//reports a vehicle that is off (fieldbus not running) as E-stopping, which must not block USB for ever. Back to the
//radio as soon as Stacato's heartbeat stops (the MAVLink timeout, 500 ms): Stacato closed or the cable pulled. In USB mode the radio sends nothing and what it hears is
//ignored; the chip stays in receive (shutdown or idle would switch its power amplifier on, docs/radio/05)
class LinkPath{
public:

    enum class Path : uint8_t{ RADIO, USB };

    static constexpr uint32_t usbSettleMillis = 1000;
    static constexpr uint32_t rampMaxMillis = 5000;

    //every loop. b_stacatoOnUsb: Stacato's heartbeat is current on the USB; b_radioLinkUp and reportedState: the
    //receiver's replies over the radio. True when the path changed
    bool update(bool b_stacatoOnUsb, bool b_radioLinkUp, uint8_t reportedState, uint32_t now){
        bool b_ramp = isRamp(reportedState);
        if(b_ramp && !b_inRamp) rampStartMillis = now;
        b_inRamp = b_ramp;
        if(!b_stacatoOnUsb){
            b_settling = false;
            if(path == Path::USB){
                path = Path::RADIO;
                return true;
            }
            return false;
        }
        if(path == Path::USB) return false;
        if(!b_settling){
            b_settling = true;
            settleStartMillis = now;
        }
        if(now - settleStartMillis < usbSettleMillis) return false;
        bool b_moving = isDriving(reportedState) || (b_ramp && now - rampStartMillis < rampMaxMillis);
        if(b_radioLinkUp && b_moving) return false;
        path = Path::USB;
        return true;
    }

    Path getPath() const { return path; }
    bool isUsb() const { return path == Path::USB; }
    //Stacato is on the USB but a vehicle is enabled over the radio: USB takes over when it is disabled
    bool isUsbWaiting(bool b_stacatoOnUsb, uint32_t now) const {
        return path == Path::RADIO && b_stacatoOnUsb && b_settling && now - settleStartMillis >= usbSettleMillis;
    }

    static bool isDriving(uint8_t reportedState){
        return reportedState == uint8_t(RobotState::ENABLED) || reportedState == uint8_t(RobotState::ENABLING);
    }
    //ramping down: the vehicle may still be moving until it reports disabled or E-stopped
    static bool isRamp(uint8_t reportedState){
        return reportedState == uint8_t(RobotState::DISABLING) || reportedState == uint8_t(RobotState::EMERGENCY_STOPPING);
    }

private:
    Path path = Path::RADIO;
    bool b_settling = false;
    uint32_t settleStartMillis = 0;
    bool b_inRamp = false;
    uint32_t rampStartMillis = 0;
};
