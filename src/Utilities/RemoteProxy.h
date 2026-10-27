#pragma once
#include <Arduino.h>
#include <RobotStates.h>
#include "Utilities/SafetyNumber.h"

//The remote's controls as last received in a remote frame (robot board, and the USB remote [D-159])
struct RemoteProxy{
    uint32_t lastReceiveTimeMillis = 0;
    bool b_connected = false;
    bool b_shouldDisable = false;
    bool b_shouldEnable = false;
    SpeedMode speedSelection = SpeedMode::LOW_SPEED;
    bool b_directionToggle = false;
    bool b_leftButton = false;
    bool b_rightButton = false;
    bool b_leftJoystick = false;
    bool b_rightJoystick = false;
    float leftJoystickX = 0.0;
    float leftJoystickY = 0.0;
    float rightJoystickX = 0.0;
    float rightJoystickY = 0.0;
    bool b_safetyClear = true;
    uint8_t txCounter = 0;
};

//Decodes an 8 byte remote frame into the proxy, false (proxy untouched) if the length or header is wrong.
//Connection state and receive time are left to the caller.
inline bool decodeRemoteFrame(const uint8_t* frame, uint8_t length, RemoteProxy& proxy){
    if(length != 8) return false;

    uint16_t controlword = frame[0] | frame[1] << 8;

    //check if the header matches
    uint8_t header = controlword & 0x7;
    if(header != 0x7) return false;

    proxy.b_shouldDisable = controlword & 0x8;
    proxy.b_shouldEnable = controlword & 0x10;
    proxy.speedSelection = SpeedMode((controlword >> 5) & 0x3);
    if(uint8_t(proxy.speedSelection) == 0x3) proxy.speedSelection = SpeedMode::LOW_SPEED;
    proxy.b_directionToggle = controlword & 0x80;
    proxy.b_leftButton = controlword & 0x100;
    proxy.b_rightButton = controlword & 0x200;
    proxy.b_leftJoystick = controlword & 0x400;
    proxy.b_rightJoystick = controlword & 0x800;
    proxy.txCounter = (controlword >> 12) & 0xF;

    if(frame[2] == 127) proxy.leftJoystickX = 0.0;
    else proxy.leftJoystickX = map((float)frame[2], 0.0, 255.0, -1.0, 1.0);
    if(frame[3] == 127) proxy.leftJoystickY = 0.0;
    else proxy.leftJoystickY = map((float)frame[3], 0.0, 255.0, -1.0, 1.0);
    if(frame[4] == 127) proxy.rightJoystickX = 0.0;
    else proxy.rightJoystickX = map((float)frame[4], 0.0, 255.0, -1.0, 1.0);
    if(frame[5] == 127) proxy.rightJoystickY = 0.0;
    else proxy.rightJoystickY = map((float)frame[5], 0.0, 255.0, -1.0, 1.0);

    uint16_t safetyNumber = frame[7] << 8 | frame[6];
    proxy.b_safetyClear = Safety::isNumberClear(safetyNumber);
    return true;
}
