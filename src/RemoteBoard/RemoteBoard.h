
#pragma once

#include "Hardware/Radio.h"
#include "Hardware/ioDevice.h"
#include "Hardware/Display.h"
#include "Utilities/Timer.h"
#include <vector>

#define REMOTE_RADIO_SHUTDOWN_PIN 8
#define REMOTE_RADIO_INTERRUPT_PIN 9
#define REMOTE_RADIO_SELECT_PIN 10

#define REMOTE_OLED_DC_PIN 7
#define REMOTE_OLED_RESET_PIN 25
#define REMOTE_OLED_CD_PIN 24

#define REMOTE_BUTTON_LEFT_PIN 2
#define REMOTE_BUTTON_RIGHT_PIN 34
#define REMOTE_LEDBUTTON_LEFT_PIN 3
#define REMOTE_LEDBUTTON_LEFT_LED_PIN 4
#define REMOTE_LEDBUTTON_RIGHT_PIN 35
#define REMOTE_LEDBUTTON_RIGHT_LED_PIN 33
#define REMOTE_SPEEDTOGGLE_HIGH_PIN 5
#define REMOTE_SPEEDTOGGLE_LOW_PIN 6
#define REMOTE_DIRECTIONTOGGLE_PIN 36
#define REMOTE_JOYSTICKLEFT_X_PIN A8
#define REMOTE_JOYSTICKLEFT_Y_PIN A9
#define REMOTE_JOYSTICKLEFT_BUTTON_PIN 21
#define REMOTE_JOYSTICKRIGHT_X_PIN A12
#define REMOTE_JOYSTICKRIGHT_Y_PIN 13
#define REMOTE_JOYSTICKRIGHT_BUTTON_PIN 28
#define REMOTE_BATTERYREADING_PIN A1

class RemoteBoard{
private:
    
    Radio radio;
    Display display;
    PushButton leftPushbutton;
    PushButton rightPushbutton;
    PushButtonWithLED leftLedButton;
    PushButtonWithLED rightLedButton;
    ToggleSwitch directionToggle;
    ThreePositionToggleSwitch speedToggle;
    Joystick2AxisWithPushbutton leftJoystick;
    Joystick2AxisWithPushbutton rightJoystick;
    BatteryLevel batteryLevel;
    std::vector<IODevice*> ioDevices;

    struct RobotProxy{
        bool b_connected = false;
        uint32_t lastSendTimeMicros = 0;
        uint32_t lastReceiveTime;
        float lastMessageRoundTripTimeMillis = 0;
        uint8_t txCounter = 0;
        uint8_t rxCounter = 0;
        
        uint8_t status = 0;
        uint8_t speedModeDisplay = 0;
        bool directionModeDisplay = false;
        bool functionButtonPressed = false;
        bool frontLeft_alarm = false;
        bool backLeft_alarm = false;
        bool frontRight_alarm = false;
        bool backRight_alarm = false;
        bool frontLeft_enabled = false;
        bool backLeft_enabled = false;
        bool frontRight_enabled = false;
        bool backRight_enabled = false;
        float xVelocity = 0.0;
        float yVelocity = 0.0;
        float rVelocity = 0.0;
        int8_t robotRxSignalStrength = 0;
        uint8_t batteryVoltage = 0;
    }robotProxy;

public:

    RemoteBoard() : 
        radio(REMOTE_RADIO_SHUTDOWN_PIN, REMOTE_RADIO_SELECT_PIN, REMOTE_RADIO_INTERRUPT_PIN),
        display(REMOTE_OLED_DC_PIN, REMOTE_OLED_RESET_PIN, REMOTE_OLED_CD_PIN),
        leftPushbutton(REMOTE_BUTTON_LEFT_PIN, INPUT_PULLUP, false),
        rightPushbutton(REMOTE_BUTTON_RIGHT_PIN, INPUT_PULLUP, false),
        leftLedButton(REMOTE_LEDBUTTON_LEFT_PIN, INPUT_PULLUP, false, REMOTE_LEDBUTTON_LEFT_LED_PIN, true),
        rightLedButton(REMOTE_LEDBUTTON_RIGHT_PIN, INPUT_PULLUP, false, REMOTE_LEDBUTTON_RIGHT_LED_PIN, true),
        directionToggle(REMOTE_DIRECTIONTOGGLE_PIN, INPUT_PULLUP, true),
        speedToggle(REMOTE_SPEEDTOGGLE_HIGH_PIN, REMOTE_SPEEDTOGGLE_LOW_PIN, INPUT_PULLUP),
        leftJoystick(REMOTE_JOYSTICKLEFT_X_PIN, REMOTE_JOYSTICKLEFT_Y_PIN, false, true, REMOTE_JOYSTICKLEFT_BUTTON_PIN, INPUT_PULLUP, false),
        rightJoystick(REMOTE_JOYSTICKRIGHT_X_PIN, REMOTE_JOYSTICKRIGHT_Y_PIN, false, true, REMOTE_JOYSTICKRIGHT_BUTTON_PIN, INPUT_PULLUP, false),
        batteryLevel(REMOTE_BATTERYREADING_PIN, 2.0, 2.6, 4.2),
        ioDevices({
            &leftPushbutton,
            &rightPushbutton,
            &leftLedButton,
            &rightLedButton,
            &directionToggle,
            &speedToggle,
            &leftJoystick,
            &rightJoystick,
            &batteryLevel}
        ){
            guiRefreshEvent.setFrequency(50.0);
            radioSendEvent.setFrequency(pingSendFrequency);
            temperatureReadingEvent.setFrequency(5.0);
        }

    static void run(){
        static RemoteBoard remoteBoard;
        remoteBoard.initialize();
        while(true) remoteBoard.update();
    }

private:

    //runtime
    void initialize();
    void update();

    //radio
    FrequencyTimer radioSendEvent;
    FrequencyTimer temperatureReadingEvent;
    float pingSendFrequency = 2.0;
    float cyclicSendFrequency = 20.0;
    void sendData();
    void receiveData();

    //gui
    void refreshGui();
    FrequencyTimer guiRefreshEvent;
    LengthTimer displayRevealTimer;
    void startupAnimation();
    void inputDeviceTestGui(int x, int y);
    void radioInitFailedScreen();
    void releaseJoysticksScreen(bool released);

    //hardware
    bool areJoysticksReleased();

    //eeprom
    void saveRadioChannel(uint8_t channel);
    uint8_t loadRadioChannel();
    uint8_t savedRadioChannel = 255;
    bool b_editingRadioChannel = false;
    bool b_channelSelectorButtonsPressed = false;
    float frequencyEditPressDuration = 2.0;

};
