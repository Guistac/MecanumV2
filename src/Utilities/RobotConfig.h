#pragma once
#include "Vector.h"
    
#define DEBUG_PRINTS false

struct RobotConfiguration{
    //Speed Settings
    double highSetting_translationVelocitLimit_mmps = 0.0;
    double mediumSetting_translationVelocityLimit_mmps = 0.0;
    double lowSetting_translationVelocityLimit_mmps = 0.0;
    double highSetting_rotationVelocityLimit_degps = 0.0;
    double mediumSetting_rotationVelocityLimit_degps = 0.0;
    double lowSetting_rotationVelocityLimit_degps = 0.0;

    //Fixed Acceleration Values
    double translationAccelerationOperational_mmpss = 0.0;
    double rotationAccelerationOperational_degpss = 0.0;
    double translationDecelerationEmergency_mmpss = 0.0;
    double rotationDecelerationEmergency_degpss = 0.0;

    //Wheel Positions
    Vec2f frontLeft_wheelPosition_mm = Vec2f(0.0, 0.0);
    Vec2f backLeft_wheelPosition_mm = Vec2f(0.0, 0.0);
    Vec2f frontRight_wheelPosition_mm = Vec2f(0.0, 0.0);
    Vec2f backRight_wheelPosition_mm = Vec2f(0.0, 0.0);

    //Friction Vectors
    Vec2f frontLeft_wheelFrictionVector_mmprev = Vec2f(0.0, 0.0);
    Vec2f backLeft_wheelFrictionVector_mmprev = Vec2f(0.0, 0.0);
    Vec2f frontRight_wheelFrictionVector_mmprev = Vec2f(0.0, 0.0);
    Vec2f backRight_wheelFrictionVector_mmprev = Vec2f(0.0, 0.0);

    //Inversions
    bool b_swapXandY = false;
    bool b_invertX = false;
    bool b_invertY = false;
    bool b_invertR = false;

    //Battery (Li-ion pack read through the receiver board divider, sent as MAVLink BATTERY_STATUS)
    //Calibrate against a multimeter: new factor = old factor * multimeter voltage / displayed voltage
    float batteryVoltageDividerFactor = 21.2121;
    uint8_t batteryCellCount = 16;
};