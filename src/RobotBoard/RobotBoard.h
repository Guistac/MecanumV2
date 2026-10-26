#pragma once

#include "Hardware/Radio.h"
#include "Utilities/Timer.h"
#include "Hardware/ioDevice.h"
#include "Hardware/ServoMotor.h"
#include <RobotStates.h>
#include "Utilities/RobotConfig.h"
#include "Utilities/RemoteProxy.h"
#include <vector>

#define ROBOT_RADIO_SHUTDOWN 8
#define ROBOT_RADIO_INTERRUPT 9
#define ROBOT_RADIO_SELECT 10

#define ROBOT_REMOTE_LED_PIN 1
#define ROBOT_FL_LED_PIN 2
#define ROBOT_FR_LED_PIN 3
#define ROBOT_BL_LED_PIN 4
#define ROBOT_BR_LED_PIN 5
#define ROBOT_STATUS_LED_PIN 28
#define ROBOT_FUNCTION_LED_PIN 29

#define ROBOT_CHANNELSELECT_1_PIN 25
#define ROBOT_CHANNELSELECT_2_PIN 7
#define ROBOT_CHANNELSELECT_4_PIN 24
#define ROBOT_CHANNELSELECT_8_PIN 6
#define ROBOT_FUNCTION1BUTTON_PIN 30
#define ROBOT_FUNCTION2BUTTON_PIN 31
#define ROBOT_BATTERYREADING_PIN  26

#define FRONTLEFT_ENABLE_PIN        19
#define FRONTLEFT_PULSE_PIN         20
#define FRONTLEFT_DIRECTION_PIN     21
#define FRONTLEFT_PENDING_PIN       22
#define FRONTLEFT_ALARM_PIN         23
#define FRONTRIGHT_ENABLE_PIN        14
#define FRONTRIGHT_PULSE_PIN         15
#define FRONTRIGHT_DIRECTION_PIN     16
#define FRONTRIGHT_PENDING_PIN       17
#define FRONTRIGHT_ALARM_PIN         18
#define BACKLEFT_ENABLE_PIN         37
#define BACKLEFT_PULSE_PIN          38
#define BACKLEFT_DIRECTION_PIN      39
#define BACKLEFT_PENDING_PIN        40
#define BACKLEFT_ALARM_PIN          41
#define BACKRIGHT_ENABLE_PIN         32
#define BACKRIGHT_PULSE_PIN          33
#define BACKRIGHT_DIRECTION_PIN      34
#define BACKRIGHT_PENDING_PIN        35
#define BACKRIGHT_ALARM_PIN          36

extern ServoMotor frontLeftServo;
extern ServoMotor frontRightServo;
extern ServoMotor backLeftServo;
extern ServoMotor backRightServo;
extern std::vector<ServoMotor*> motors;

void enableAllMotors();
void disableAllMotors();
bool areAllMotorsEnabled();
bool hasAnyMotorFault();

class Mavlink;

class RobotBoard{
public:

    Mavlink* mavlink;

    Radio radio;
    LED remoteLED;
    LED functionLED;
    LED statusLED;
    LED flLED;
    LED frLED;
    LED blLED;
    LED brLED;
    RotaryDipSwitch4Bit channelSelector;
    PushButton function1Button;
    PushButton function2Button;
    BatteryLevel batteryReading;
    std::vector<IODevice*> ioDevices;
    std::vector<LED*> leds;

    RemoteProxy remoteProxy;

    RobotConfiguration configuration;
    RobotState state = RobotState::EMERGENCY_STOPPED;
    uint32_t enableRequestMillis;

    void initialize();
    void update();

    void updateIO();
    void receiveData();
    void sendData();
    void updateRobotLogic();

    struct MotionControl{
        //Live control data
        uint32_t lastControlLoopTime_micros = 0;
        uint32_t controlLoopInterval_micros = 1000;
        SpeedMode speedMode = SpeedMode::NO_SPEED;
        bool directionMode = false;
        float xVelActual_mmps = 0.0;
        float yVelActual_mmps = 0.0;
        float rVelActual_degps = 0.0;
        
        //Live Targets and limits
        float xVelTarg_mmps;
        float yVelTarg_mmps;
        float rVelTarg_degps;
        float transAccTarg_mmpss;
        float rotAccTarg_degpss;
        float transVelLim_mmps;
        float rotVelLim_degps;

        //Fixed Settings
        float translationAccelerationOperational_mmpss;
        float rotationAccelerationOperational_degpss;
        double translationDecelerationEmergency_mmpss;
        double rotationDecelerationEmergency_degpss;
        double highSetting_translationVelocitLimit_mmps;
        double mediumSetting_translationVelocityLimit_mmps;
        double lowSetting_translationVelocityLimit_mmps;
        double highSetting_rotationVelocityLimit_degps;
        double mediumSetting_rotationVelocityLimit_degps;
        double lowSetting_rotationVelocityLimit_degps;
        bool b_swapXandY;
        bool b_invertX;
        bool b_invertY;
        bool b_invertR;

        bool configure(RobotConfiguration& config);
        void reset();
        void setSpeedMode(SpeedMode mode);

        void setControlTargetsNormalized(float x, float y, float r);
        void requestEmergencyDeceleration();

        bool isStopped();
        float getXVelocityNormalized();
        float getYVelocityNormalized();
        float getRVelocityNormalized();

        void updateMotionControl();

    }motionControl;

public:

    RobotBoard(RobotConfiguration configuration_) :
        radio(ROBOT_RADIO_SHUTDOWN, ROBOT_RADIO_SELECT, ROBOT_RADIO_INTERRUPT),
        remoteLED(ROBOT_REMOTE_LED_PIN, true),
        functionLED(ROBOT_FUNCTION_LED_PIN, true),
        statusLED(ROBOT_STATUS_LED_PIN, true),
        flLED(ROBOT_FL_LED_PIN, true),
        frLED(ROBOT_FR_LED_PIN, true),
        blLED(ROBOT_BL_LED_PIN, true),
        brLED(ROBOT_BR_LED_PIN, true),
        channelSelector(ROBOT_CHANNELSELECT_1_PIN, ROBOT_CHANNELSELECT_2_PIN, ROBOT_CHANNELSELECT_4_PIN, ROBOT_CHANNELSELECT_8_PIN, INPUT_PULLUP),
        function1Button(ROBOT_FUNCTION1BUTTON_PIN, INPUT_PULLUP, false),
        function2Button(ROBOT_FUNCTION2BUTTON_PIN, INPUT_PULLUP, false),
        batteryReading(ROBOT_BATTERYREADING_PIN, 21.2121, 0.0, 70.0),
        ioDevices({
            &statusLED,
            &remoteLED,
            &flLED,
            &frLED,
            &blLED,
            &brLED,
            &functionLED,
            &channelSelector,
            &function1Button,
            &function2Button,
            &batteryReading
        }),
        leds({
            &remoteLED,
            &functionLED,
            &statusLED,
            &flLED,
            &frLED,
            &blLED,
            &brLED
        }),
        configuration(configuration_)
    {}       

    static void run(RobotConfiguration& config){
        static RobotBoard robotboard(config);
        //robotboard.pinTest();
        robotboard.initialize();
        while(true) robotboard.update();
    }

    void startupAnimation();

    void pinTest();

};