#pragma once
#include <stdint.h>

//The remote's 8 byte radio frame (unchanged layout, decoded by RemoteProxy.h on the receiver)
struct UplinkControls{
    bool b_disable = false;
    bool b_enable = false;
    uint8_t speedState = 0;         //ThreePositionToggleSwitch state, 0 = LOW = sticks off (D-93)
    bool b_direction = false;
    bool b_leftStickPressed = false;
    bool b_rightStickPressed = false;
    float leftX = 0.0f;
    float leftY = 0.0f;
    float rightX = 0.0f;
    float rightY = 0.0f;
};

namespace Uplink{

    //-1..1 to 0..255 as the remote always sent it (Teensy map() in float, truncated): 0 is 127, read as 0 by the receiver
    inline uint8_t stickToByte(float value){
        if(value < -1.0f) value = -1.0f;
        if(value > 1.0f) value = 1.0f;
        return uint8_t((value - (-1.0f)) * (255.0f - 0.0f) / (1.0f - (-1.0f)) + 0.0f);
    }

    //the pushbuttons are page keys and send no command: bits 8 and 9 stay 0 [D-147 point 1]
    //b_neutralSticks: the stick guard holds the sticks (menu open, not yet released)
    inline void encode(const UplinkControls& controls, bool b_neutralSticks, uint8_t counter, uint16_t safetyNumber, uint8_t frame[8]){
        uint16_t controlWord = 0x7; //3 bit header
        if(controls.b_disable)              controlWord |= 0x8;
        if(controls.b_enable)               controlWord |= 0x10;
        controlWord |= (controls.speedState & 0x3) << 5;
        if(controls.b_direction)            controlWord |= 0x80;
        if(controls.b_leftStickPressed)     controlWord |= 0x400;
        if(controls.b_rightStickPressed)    controlWord |= 0x800;
        controlWord |= (counter & 0xF) << 12;

        frame[0] = controlWord & 0xFF;
        frame[1] = (controlWord >> 8) & 0xFF;
        frame[2] = stickToByte(b_neutralSticks ? 0.0f : controls.leftX);
        frame[3] = stickToByte(b_neutralSticks ? 0.0f : controls.leftY);
        frame[4] = stickToByte(b_neutralSticks ? 0.0f : controls.rightX);
        frame[5] = stickToByte(b_neutralSticks ? 0.0f : controls.rightY);
        frame[6] = safetyNumber & 0xFF;
        frame[7] = safetyNumber >> 8;
    }

};
