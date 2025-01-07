#include "RemoteBoard/RemoteBoard.h"
#include "RobotBoard/RobotBoard.h"

double wheelDiameter_mm = 203.0;
double wheelCircumference_mm = PI * wheelDiameter_mm;
float frictionVectorUnit = sin(45.0) * wheelCircumference_mm;

RobotConfiguration tableFannyAlexandre{
    //Speed Settings
    .highSetting_translationVelocitLimit_mmps = 500.0,
    .mediumSetting_translationVelocityLimit_mmps = 250.0,
    .lowSetting_translationVelocityLimit_mmps = 125.0,
    .highSetting_rotationVelocityLimit_degps = 30.0,
    .mediumSetting_rotationVelocityLimit_degps = 15.0,
    .lowSetting_rotationVelocityLimit_degps = 7.5,

    //Fixed Acceleration Values
    .translationAccelerationOperational_mmpss = 300.0,
    .rotationAccelerationOperational_degpss = 15.0,
    .translationDecelerationEmergency_mmpss = 1000.0,
    .rotationDecelerationEmergency_degpss = 120.0,

    //Wheel Positions
    .frontLeft_wheelPosition_mm = Vec2f(-1187.0, 283.0),
    .backLeft_wheelPosition_mm = Vec2f(-1187.0, -442.0),
    .frontRight_wheelPosition_mm = Vec2f(1187.0, 442.0),
    .backRight_wheelPosition_mm = Vec2f(1187.0, -442.0),

    //Friction Vectors
    .frontLeft_wheelFrictionVector_mmprev = Vec2f(frictionVectorUnit, frictionVectorUnit),
    .backLeft_wheelFrictionVector_mmprev = Vec2f(-frictionVectorUnit, frictionVectorUnit),
    .frontRight_wheelFrictionVector_mmprev = Vec2f(frictionVectorUnit, -frictionVectorUnit),
    .backRight_wheelFrictionVector_mmprev = Vec2f(-frictionVectorUnit, -frictionVectorUnit),

    //Inversions
    .b_swapXandY = false,
    .b_invertX = false,
    .b_invertY = false,
    .b_invertR = true
};

RobotConfiguration tenteGotterdammerung{
    //Speed Settings
    .highSetting_translationVelocitLimit_mmps = 500.0,
    .mediumSetting_translationVelocityLimit_mmps = 250.0,
    .lowSetting_translationVelocityLimit_mmps = 125.0,
    .highSetting_rotationVelocityLimit_degps = 20.0,
    .mediumSetting_rotationVelocityLimit_degps = 10.0,
    .lowSetting_rotationVelocityLimit_degps = 5.0,

    //Fixed Acceleration Values
    .translationAccelerationOperational_mmpss = 300.0,
    .rotationAccelerationOperational_degpss = 10.0,
    .translationDecelerationEmergency_mmpss = 1000.0,
    .rotationDecelerationEmergency_degpss = 120.0,

    //Wheel Positions
    .frontLeft_wheelPosition_mm = Vec2f(-635.0, 485.0),
    .backLeft_wheelPosition_mm = Vec2f(-635.0, -335.0),
    .frontRight_wheelPosition_mm = Vec2f(635.0, 485.0),
    .backRight_wheelPosition_mm = Vec2f(635.0, -485.0),

    //Friction Vectors
    .frontLeft_wheelFrictionVector_mmprev = Vec2f(frictionVectorUnit, frictionVectorUnit),
    .backLeft_wheelFrictionVector_mmprev = Vec2f(-frictionVectorUnit, frictionVectorUnit),
    .frontRight_wheelFrictionVector_mmprev = Vec2f(frictionVectorUnit, -frictionVectorUnit),
    .backRight_wheelFrictionVector_mmprev = Vec2f(-frictionVectorUnit, -frictionVectorUnit),

    //Inversions
    .b_swapXandY = true,
    .b_invertX = true,
    .b_invertY = false,
    .b_invertR = true
};

RobotConfiguration smallCubeGotterdammerung{
    //Speed Settings
    .highSetting_translationVelocitLimit_mmps = 500.0,
    .mediumSetting_translationVelocityLimit_mmps = 250.0,
    .lowSetting_translationVelocityLimit_mmps = 125.0,
    .highSetting_rotationVelocityLimit_degps = 40.0,
    .mediumSetting_rotationVelocityLimit_degps = 20.0,
    .lowSetting_rotationVelocityLimit_degps = 10.0,

    //Fixed Acceleration Values
    .translationAccelerationOperational_mmpss = 500.0,
    .rotationAccelerationOperational_degpss = 40.0,
    .translationDecelerationEmergency_mmpss = 2000.0,
    .rotationDecelerationEmergency_degpss = 100.0,

    //Wheel Positions
    .frontLeft_wheelPosition_mm = Vec2f(-395.0, 395.0),
    .backLeft_wheelPosition_mm = Vec2f(-395.0, -395.0),
    .frontRight_wheelPosition_mm = Vec2f(395.0, 395.0),
    .backRight_wheelPosition_mm = Vec2f(395.0, -395.0),

    //Friction Vectors
    .frontLeft_wheelFrictionVector_mmprev = Vec2f(frictionVectorUnit, frictionVectorUnit),
    .backLeft_wheelFrictionVector_mmprev = Vec2f(-frictionVectorUnit, frictionVectorUnit),
    .frontRight_wheelFrictionVector_mmprev = Vec2f(frictionVectorUnit, -frictionVectorUnit),
    .backRight_wheelFrictionVector_mmprev = Vec2f(-frictionVectorUnit, -frictionVectorUnit),

    //Inversions
    .b_swapXandY = false,
    .b_invertX = false,
    .b_invertY = false,
    .b_invertR = false
};

RobotConfiguration bigCubeGotterdammerung{
    //Speed Settings
    .highSetting_translationVelocitLimit_mmps = 500.0,
    .mediumSetting_translationVelocityLimit_mmps = 250.0,
    .lowSetting_translationVelocityLimit_mmps = 125.0,
    .highSetting_rotationVelocityLimit_degps = 40.0,
    .mediumSetting_rotationVelocityLimit_degps = 20.0,
    .lowSetting_rotationVelocityLimit_degps = 10.0,

    //Fixed Acceleration Values
    .translationAccelerationOperational_mmpss = 500.0,
    .rotationAccelerationOperational_degpss = 40.0,
    .translationDecelerationEmergency_mmpss = 2000.0,
    .rotationDecelerationEmergency_degpss = 100.0,

    //Wheel Positions
    .frontLeft_wheelPosition_mm = Vec2f(-395.0, 395.0),
    .backLeft_wheelPosition_mm = Vec2f(-395.0, -395.0),
    .frontRight_wheelPosition_mm = Vec2f(395.0, 395.0),
    .backRight_wheelPosition_mm = Vec2f(395.0, -395.0),

    //Friction Vectors
    .frontLeft_wheelFrictionVector_mmprev = Vec2f(frictionVectorUnit, frictionVectorUnit),
    .backLeft_wheelFrictionVector_mmprev = Vec2f(-frictionVectorUnit, frictionVectorUnit),
    .frontRight_wheelFrictionVector_mmprev = Vec2f(frictionVectorUnit, -frictionVectorUnit),
    .backRight_wheelFrictionVector_mmprev = Vec2f(-frictionVectorUnit, -frictionVectorUnit),

    //Inversions
    .b_swapXandY = false,
    .b_invertX = false,
    .b_invertY = false,
    .b_invertR = false
};

void setup() {
  RemoteBoard::run();
  //RobotBoard::run(tenteGotterdammerung);
  //RobotBoard::run(smallCubeGotterdammerung);
  //RobotBoard::run(bigCubeGotterdammerung);
}

void loop() {}

