#include "RemoteBoard/RemoteBoard.h"
#include "RobotBoard/RobotBoard.h"

double wheelDiameter_mm = 203.0;
double wheelCircumference_mm = PI * wheelDiameter_mm;
float frictionVectorUnit = sin(45.0) * wheelCircumference_mm;

RobotConfiguration testConfig{
    //Speed Settings
    .highSetting_translationVelocitLimit_mmps = 400.0,
    .mediumSetting_translationVelocityLimit_mmps = 200.0,
    .lowSetting_translationVelocityLimit_mmps = 100.0,
    .highSetting_rotationVelocityLimit_degps = 32.0,
    .mediumSetting_rotationVelocityLimit_degps = 16.0,
    .lowSetting_rotationVelocityLimit_degps = 8.0,

    //Fixed Acceleration Values
    .translationAccelerationOperational_mmpss = 200.0,
    .rotationAccelerationOperational_degpss = 16.0,
    .translationDecelerationEmergency_mmpss = 1000.0,
    .rotationDecelerationEmergency_degpss = 80.0,

    //Wheel Positions
    .frontLeft_wheelPosition_mm = Vec2f(-275.0, 130.0),
    .backLeft_wheelPosition_mm = Vec2f(-275.0, -130.0),
    .frontRight_wheelPosition_mm = Vec2f(275.0, 130.0),
    .backRight_wheelPosition_mm = Vec2f(275.0, -130.0),

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

RobotConfiguration robotRectangle{
    //Speed Settings
    .highSetting_translationVelocitLimit_mmps = 500.0,
    .mediumSetting_translationVelocityLimit_mmps = 250.0,
    .lowSetting_translationVelocityLimit_mmps = 125.0,
    .highSetting_rotationVelocityLimit_degps = 60.0,
    .mediumSetting_rotationVelocityLimit_degps = 40.0,
    .lowSetting_rotationVelocityLimit_degps = 20.0,

    //Fixed Acceleration Values
    .translationAccelerationOperational_mmpss = 300.0,
    .rotationAccelerationOperational_degpss = 30.0,
    .translationDecelerationEmergency_mmpss = 1000.0,
    .rotationDecelerationEmergency_degpss = 120.0,

    //Wheel Positions
    .frontLeft_wheelPosition_mm = Vec2f(-275.0, 130.0),
    .backLeft_wheelPosition_mm = Vec2f(-275.0, -130.0),
    .frontRight_wheelPosition_mm = Vec2f(275.0, 130.0),
    .backRight_wheelPosition_mm = Vec2f(275.0, -130.0),

    //Friction Vectors
    .frontLeft_wheelFrictionVector_mmprev = Vec2f(frictionVectorUnit, frictionVectorUnit),
    .backLeft_wheelFrictionVector_mmprev = Vec2f(-frictionVectorUnit, frictionVectorUnit),
    .frontRight_wheelFrictionVector_mmprev = Vec2f(frictionVectorUnit, -frictionVectorUnit),
    .backRight_wheelFrictionVector_mmprev = Vec2f(-frictionVectorUnit, -frictionVectorUnit),

    //Inversions
    .b_swapXandY = false,
    .b_invertX = true,
    .b_invertY = true,
    .b_invertR = false
};

RobotConfiguration robotCarre{
    //Speed Settings
    .highSetting_translationVelocitLimit_mmps = 500.0,
    .mediumSetting_translationVelocityLimit_mmps = 250.0,
    .lowSetting_translationVelocityLimit_mmps = 125.0,
    .highSetting_rotationVelocityLimit_degps = 60.0,
    .mediumSetting_rotationVelocityLimit_degps = 40.0,
    .lowSetting_rotationVelocityLimit_degps = 20.0,

    //Fixed Acceleration Values
    .translationAccelerationOperational_mmpss = 300.0,
    .rotationAccelerationOperational_degpss = 30.0,
    .translationDecelerationEmergency_mmpss = 1000.0,
    .rotationDecelerationEmergency_degpss = 120.0,

    //Wheel Positions
    .frontLeft_wheelPosition_mm = Vec2f(-140.0, 180.0),
    .backLeft_wheelPosition_mm = Vec2f(-140.0, -180.0),
    .frontRight_wheelPosition_mm = Vec2f(140.0, 180.0),
    .backRight_wheelPosition_mm = Vec2f(140.0, -180.0),

    //Friction Vectors
    .frontLeft_wheelFrictionVector_mmprev = Vec2f(frictionVectorUnit, frictionVectorUnit),
    .backLeft_wheelFrictionVector_mmprev = Vec2f(-frictionVectorUnit, frictionVectorUnit),
    .frontRight_wheelFrictionVector_mmprev = Vec2f(frictionVectorUnit, -frictionVectorUnit),
    .backRight_wheelFrictionVector_mmprev = Vec2f(-frictionVectorUnit, -frictionVectorUnit),

    //Inversions
    .b_swapXandY = true,
    .b_invertX = true,
    .b_invertY = false,
    .b_invertR = false
};

void setup() {
  RemoteBoard::run();
  //RobotBoard::run(robotCarre);
  //RobotBoard::run(robotRectangle);
}

void loop() {}

