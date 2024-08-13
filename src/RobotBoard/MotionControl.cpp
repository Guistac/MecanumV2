#include "RobotBoard.h"

bool RobotBoard::MotionControl::configure(RobotConfiguration& config){
    if(config.translationAccelerationOperational_mmpss <= 0.0) return false;
    if(config.rotationAccelerationOperational_degpss <= 0.0) return false;
    if(config.translationDecelerationEmergency_mmpss <= 0.0) return false;
    if(config.rotationDecelerationEmergency_degpss <= 0.0) return false;
    if(config.highSetting_translationVelocitLimit_mmps <= 0.0) return false;
    if(config.mediumSetting_translationVelocityLimit_mmps <= 0.0) return false;
    if(config.lowSetting_translationVelocityLimit_mmps <= 0.0) return false;
    if(config.highSetting_rotationVelocityLimit_degps <= 0.0) return false;
    if(config.mediumSetting_rotationVelocityLimit_degps <= 0.0) return false;
    if(config.lowSetting_rotationVelocityLimit_degps <= 0.0) return false;
    translationAccelerationOperational_mmpss = config.translationAccelerationOperational_mmpss;
    rotationAccelerationOperational_degpss = config.rotationAccelerationOperational_degpss;
    translationDecelerationEmergency_mmpss = config.translationDecelerationEmergency_mmpss;
    rotationDecelerationEmergency_degpss = config.rotationDecelerationEmergency_degpss;
    highSetting_translationVelocitLimit_mmps = config.highSetting_translationVelocitLimit_mmps;
    mediumSetting_translationVelocityLimit_mmps = config.mediumSetting_translationVelocityLimit_mmps;
    lowSetting_translationVelocityLimit_mmps = config.lowSetting_translationVelocityLimit_mmps;
    highSetting_rotationVelocityLimit_degps = config.highSetting_rotationVelocityLimit_degps;
    mediumSetting_rotationVelocityLimit_degps = config.mediumSetting_rotationVelocityLimit_degps;
    lowSetting_rotationVelocityLimit_degps = config.lowSetting_rotationVelocityLimit_degps;
    transAccTarg_mmpss = translationDecelerationEmergency_mmpss;
    rotAccTarg_degpss = rotationDecelerationEmergency_degpss;
    reset();
    setSpeedMode(SpeedMode::LOW_SPEED);
    return true;
}

void RobotBoard::MotionControl::reset(){
    xVelTarg_mmps = 0.0;
    yVelTarg_mmps = 0.0;
    rVelTarg_degps = 0.0;
    xVelActual_mmps = 0.0;
    yVelActual_mmps = 0.0;
    rVelActual_degps = 0.0;
}

void RobotBoard::MotionControl::setSpeedMode(SpeedMode mode){
    if(mode != speedMode){
        speedMode = mode;
        switch(mode){
            case SpeedMode::HIGH_SPEED:
                transVelLim_mmps = highSetting_translationVelocitLimit_mmps;
                rotVelLim_degps = highSetting_rotationVelocityLimit_degps;
                if(DEBUG_PRINTS) Serial.printf("--- Changed speed mode to HIGH (T:%.3f R:%.3f)\n", transVelLim_mmps, rotVelLim_degps);
                break;
            case SpeedMode::MEDIUM_SPEED:
                transVelLim_mmps = mediumSetting_translationVelocityLimit_mmps;
                rotVelLim_degps = mediumSetting_rotationVelocityLimit_degps;
                if(DEBUG_PRINTS) Serial.printf("--- Changed speed mode to MEDIUM (T:%.3f R:%.3f)\n", transVelLim_mmps, rotVelLim_degps);
                break;
            default:
            case SpeedMode::LOW_SPEED:
                transVelLim_mmps = lowSetting_translationVelocityLimit_mmps;
                rotVelLim_degps = lowSetting_rotationVelocityLimit_degps;
                if(DEBUG_PRINTS) Serial.printf("--- Changed speed mode to LOW (T:%.3f R:%.3f)\n", transVelLim_mmps, rotVelLim_degps);
                break;
        }
    }
}

void RobotBoard::MotionControl::setControlTargetsNormalized(float x, float y, float r){
    if(b_swapXandY) std::swap(x, y);
    if(b_invertX) x = -x;
    if(b_invertY) y = -y;
    if(b_invertR) r = -r;
    xVelTarg_mmps = map(x, -1.0, 1.0, -transVelLim_mmps, transVelLim_mmps);
    yVelTarg_mmps = map(y, -1.0, 1.0, -transVelLim_mmps, transVelLim_mmps);
    rVelTarg_degps = map(r, -1.0, 1.0, -rotVelLim_degps, rotVelLim_degps);
    xVelTarg_mmps = min(xVelTarg_mmps, transVelLim_mmps);
    xVelTarg_mmps = max(xVelTarg_mmps, -transVelLim_mmps);
    yVelTarg_mmps = min(yVelTarg_mmps, transVelLim_mmps);
    yVelTarg_mmps = max(yVelTarg_mmps, -transVelLim_mmps);
    rVelTarg_degps = min(rVelTarg_degps, rotVelLim_degps);
    rVelTarg_degps = max(rVelTarg_degps, -rotVelLim_degps);
    transAccTarg_mmpss = translationAccelerationOperational_mmpss;
    rotAccTarg_degpss = rotationAccelerationOperational_degpss;
}

void RobotBoard::MotionControl::requestEmergencyDeceleration(){
    transAccTarg_mmpss = translationDecelerationEmergency_mmpss;
    rotAccTarg_degpss = rotationDecelerationEmergency_degpss;
    xVelTarg_mmps = 0.0;
    yVelTarg_mmps = 0.0;
    rVelTarg_degps = 0.0;
}

bool RobotBoard::MotionControl::isStopped(){
    return xVelActual_mmps == 0.0 && yVelActual_mmps == 0.0 && rVelActual_degps == 0.0;
}

float RobotBoard::MotionControl::getXVelocityNormalized(){
    return map(xVelActual_mmps, -transVelLim_mmps, transVelLim_mmps, -1.0, 1.0);
}

float RobotBoard::MotionControl::getYVelocityNormalized(){
    return map(yVelActual_mmps, -transVelLim_mmps, transVelLim_mmps, -1.0, 1.0);
}

float RobotBoard::MotionControl::getRVelocityNormalized(){
    return map(rVelActual_degps, -rotVelLim_degps, rotVelLim_degps, -1.0, 1.0);
}


void RobotBoard::MotionControl::updateMotionControl(){

    uint32_t nowMicros = micros();
    if(nowMicros - lastControlLoopTime_micros <= controlLoopInterval_micros) return;
    double deltaT_seconds = double(nowMicros - lastControlLoopTime_micros) / 1000000.0;
    lastControlLoopTime_micros = nowMicros;

    float xVelocityTarget = xVelTarg_mmps;
    float yVelocityTarget = yVelTarg_mmps;
    float rVelocityTarget = rVelTarg_degps;

    //limit the translation vector magnitude to the max translation velocity
    //this effectively transforms the xy square of the left joystick into a circle
    float translationTargetVectorMagnitude = sqrt(sq(xVelocityTarget) + sq(yVelocityTarget));
    if(translationTargetVectorMagnitude != 0.0){
        xVelocityTarget /= translationTargetVectorMagnitude;
        yVelocityTarget /= translationTargetVectorMagnitude;
        if(translationTargetVectorMagnitude > transVelLim_mmps) translationTargetVectorMagnitude = transVelLim_mmps;
        xVelocityTarget *= translationTargetVectorMagnitude;
        yVelocityTarget *= translationTargetVectorMagnitude;
    }
    //limit the target velocity to the velocity limit
    if(rVelocityTarget > rotVelLim_degps) rVelocityTarget = rotVelLim_degps;
    else if(rVelocityTarget < -rotVelLim_degps) rVelocityTarget = -rotVelLim_degps;

    //update motion ramp profilers
    double deltaVTranslation_millimetersPerSecond = transAccTarg_mmpss * deltaT_seconds;
    if(xVelActual_mmps < xVelocityTarget){
        xVelActual_mmps += deltaVTranslation_millimetersPerSecond;
        if(xVelActual_mmps > xVelocityTarget) xVelActual_mmps = xVelocityTarget;
    }else if(xVelActual_mmps > xVelocityTarget){
        xVelActual_mmps -= deltaVTranslation_millimetersPerSecond;
        if(xVelActual_mmps < xVelocityTarget) xVelActual_mmps = xVelocityTarget;
    }
    if(yVelActual_mmps < yVelocityTarget){
        yVelActual_mmps += deltaVTranslation_millimetersPerSecond;
        if(yVelActual_mmps > yVelocityTarget) yVelActual_mmps = yVelocityTarget;
    }else if(yVelActual_mmps > yVelocityTarget){
        yVelActual_mmps -= deltaVTranslation_millimetersPerSecond;
        if(yVelActual_mmps < yVelocityTarget) yVelActual_mmps = yVelocityTarget;
    }
    Vec2f requestedTranslationVelocityVector{xVelActual_mmps, yVelActual_mmps};

    double deltaVRotation_degreesPerSecond = rotAccTarg_degpss * deltaT_seconds;
    if(rVelActual_degps < rVelTarg_degps){
        rVelActual_degps += deltaVRotation_degreesPerSecond;
        if(rVelActual_degps > rVelTarg_degps) rVelActual_degps = rVelTarg_degps;
    }else if(rVelActual_degps > rVelTarg_degps){
        rVelActual_degps -= deltaVRotation_degreesPerSecond;
        if(rVelActual_degps < rVelTarg_degps) rVelActual_degps = rVelTarg_degps;
    }

    //Serial.printf("%.3f %.3f (%i)\n", transAccTarg_mmpss, rotAccTarg_degpss, micros());
    //Serial.printf("DvT: %.6f DvR:%.6f (%i)\n", deltaVTranslation_millimetersPerSecond, deltaVRotation_degreesPerSecond, micros());
    //Serial.printf("Tar X:%.3f Y:%.3f R:%.3f  Vel X:%.3f Y:%.3f R:%.3f\n", xVelocityTarget, yVelocityTarget, rVelocityTarget, xVelActual_mmps, yVelActual_mmps, rVelActual_degps);

    double wheelVelocity[4];

    //calculate target velocity for each wheel
    for(int i = 0; i < 4; i++){
        //————————————X & Y component velocity————————————
        wheelVelocity[i] = 
            requestedTranslationVelocityVector.x / motors[i]->wheelFrictionVector_mmPerRev.x +
            requestedTranslationVelocityVector.y / motors[i]->wheelFrictionVector_mmPerRev.y;

        //———————Rotational Component velocity————————

        //wheel position relative to rotation center
        Vec2f relativeWheelPosition = motors[i]->wheelPosition_mm;

        //Serial.printf("%.1f %.1f ", relativeWheelPosition.x, relativeWheelPosition.y);

        float rotationRadius = sqrt(sq(relativeWheelPosition.x) + sq(relativeWheelPosition.y));
        float rotationCirclePerimeter = 2.0 * PI * rotationRadius;
        float rotationVectorMagnitude = rotationCirclePerimeter * rVelActual_degps / 360.0;

        //vector perpendicular to radius of wheel position around rotation center vector
        Vec2f wheelRotationVector;
        wheelRotationVector.x = -relativeWheelPosition.y;
        wheelRotationVector.y = relativeWheelPosition.x;

        //normalize the perpendicular vector
        double normalisationMagnitude = sqrt(sq(wheelRotationVector.x) + sq(wheelRotationVector.y));
        if(normalisationMagnitude != 0.0){
            wheelRotationVector.x /= normalisationMagnitude;
            wheelRotationVector.y /= normalisationMagnitude;

            //set the rotation vector magnitude to the rotation speed
            wheelRotationVector.x *= rotationVectorMagnitude;
            wheelRotationVector.y *= rotationVectorMagnitude;
        }else{
            wheelRotationVector.x = 0.0;
            wheelRotationVector.y = 0.0;
        }

        //decompose the rotation vector and add to wheel velocity
        wheelVelocity[i] += wheelRotationVector.x / motors[i]->wheelFrictionVector_mmPerRev.x;
        wheelVelocity[i] += wheelRotationVector.y / motors[i]->wheelFrictionVector_mmPerRev.y;
    }

    //Serial.printf("%.3f %.3f %.3f %.3f\n", wheelVelocity[0], wheelVelocity[1], wheelVelocity[2], wheelVelocity[3]);

    for(int i = 0; i < 4; i++) motors[i]->setVelocityTarget(wheelVelocity[i]);
}
