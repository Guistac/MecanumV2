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
    bool b_creep = false;           //[D-207] uplink bit 9, only at MEDIUM (the right pushbutton's bit before D-147)
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
    //[D-207 point 6] creep counts only at MEDIUM: at OFF or FAST bit 9 is ignored
    proxy.b_creep = (controlword & 0x200) && proxy.speedSelection == SpeedMode::MEDIUM_SPEED;
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

//MANUAL_CONTROL.buttons from the proxy (the receiver's MavLink::sendMsg_MANUAL_CONTROLS, the USB remote the same):
//  0 left pushbutton (uplink bit 8, always 0 since D-147), 1 always 0 (the right pushbutton's bit, gone [D-207]),
//  2-3 stick pushes, 4 speed HIGH, 5 speed LOW (OFF, sticks off), 6 direction, 7 E-stop clear and connected,
//  8 disable, 9 enable, 10 connected (D-44), 11 creep and connected [D-207]
inline uint16_t manualControlButtons(const RemoteProxy& proxy){
    bool connected = proxy.b_connected;
    uint16_t buttons = 0;
    buttons |= uint16_t(proxy.b_leftButton) << 0;
    buttons |= uint16_t(proxy.b_leftJoystick) << 2;
    buttons |= uint16_t(proxy.b_rightJoystick) << 3;
    buttons |= uint16_t(proxy.speedSelection == SpeedMode::HIGH_SPEED) << 4;
    buttons |= uint16_t(proxy.speedSelection == SpeedMode::LOW_SPEED) << 5;
    buttons |= uint16_t(proxy.b_directionToggle) << 6;
    buttons |= uint16_t(proxy.b_safetyClear && connected) << 7;
    buttons |= uint16_t(proxy.b_shouldDisable) << 8;
    buttons |= uint16_t(proxy.b_shouldEnable) << 9;
    buttons |= uint16_t(connected) << 10;          //D-44: radio link to the handheld up
    buttons |= uint16_t(connected && proxy.b_creep) << 11;
    return buttons;
}
