
#pragma once

#include "Hardware/Radio.h"
#include "Hardware/ioDevice.h"
#include "Hardware/Display.h"
#include "Utilities/Timer.h"
#include <vector>

#include <EstopContacts.h>
#include <BatteryMonitor.h>
#include <StickGuard.h>
#include <AxisStepper.h>
#include <SettingsMenu.h>
#include <PageNavigator.h>
#include <StatusView.h>
#include <Dimmer.h>
#include <BootGuard.h>
#include <LinkMonitor.h>
#include <StickCalibration.h>
#include <RemoteScreens.h>

#ifdef REMOTE_DIRECT
#include "Utilities/RemoteProxy.h"
class Mavlink;
#endif

#define REMOTE_RADIO_SHUTDOWN_PIN 8
#define REMOTE_RADIO_INTERRUPT_PIN 9
#define REMOTE_RADIO_SELECT_PIN 10

#define REMOTE_OLED_DC_PIN 7
#define REMOTE_OLED_RESET_PIN 25
#define REMOTE_OLED_CD_PIN 24

#define REMOTE_ESTOP_NO_PIN 0
#define REMOTE_ESTOP_NC_PIN 1
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
#define REMOTE_JOYSTICKRIGHT_Y_PIN A13
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
    EstopButton estopButton;
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
        bool functionButtonPressed = false;     //the receiver's remote finder button
        //the reply's motor and velocity bytes are not read: always 0 on mavlink (no motion control on the receiver)
        int8_t robotRxSignalStrength = 0;
        float batteryVoltage = 0.0;
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
        estopButton(REMOTE_ESTOP_NO_PIN, REMOTE_ESTOP_NC_PIN, false),
        ioDevices({
            &leftPushbutton,
            &rightPushbutton,
            &leftLedButton,
            &rightLedButton,
            &directionToggle,
            &speedToggle,
            &leftJoystick,
            &rightJoystick,
            &estopButton,
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
    void handleReply(uint8_t* incomingFrame, uint8_t length);
    LinkMonitor link;
    uint16_t shownRecoveries = 0;
    uint32_t recoveryMarkerUntilMillis = 0;

#ifdef REMOTE_DIRECT
    //bench build env:remote_direct: the remote talks MAVLink to Stacato over its own USB, doing the receiver's job,
    //the radio stays in receive and never transmits. Both pushbuttons together toggle a simulated radio dropout.
    Mavlink* directMavlink = nullptr;
    RemoteProxy directProxy;
    bool b_simulatedDropout = false;
    bool b_dropoutChordHeld = false;
    void directExchange(uint8_t* outgoingFrame);
#endif

    //operator logic [D-146] (lib/RemoteLogic, tested with pio test -e native)
    EstopContacts estop;
    BatteryMonitor batteryMonitor;
    StickGuard stickGuard;
    AxisStepper itemAxis;
    AxisStepper valueAxis;
    SettingsMenu menu;
    PageNavigator pages;
    Dimmer dimmer;
    RemoteSettings settings;
    bool b_settingsAvailable = false;
    bool b_joystickZeroCaptured = false;
    bool b_estopWasReleased = false;
    uint32_t estopReleaseMillis = 0;
    uint32_t thankYouUntilMillis = 0;
    StatusView status = {BigMessage::UNKNOWN, StatusLine::NORMAL};
    BigMessage previousMessage = BigMessage::UNKNOWN;
    bool b_inputActivity = false;
    uint8_t appliedContrast = 0;
    void updateOperatorLogic();
    bool updateMenu(uint32_t now);
    void closeMenu();
    void updateLeds();
    StatusView selectStatus(uint32_t now);
    void loadSettings();
    void saveSettings(const RemoteSettings& newSettings);

    //joystick calibration from the settings menu [H15]
    StickCalibration stickCalibration;
    StickCalibrator calibrator;
    bool b_calibrating = false;
    bool b_calibrationResult = false;       //after saving: the result, until a pushbutton or 10 s
    uint32_t calibrationResultUntilMillis = 0;
    bool b_keepCircling = false;
    uint32_t calibrationNoteUntilMillis = 0;
    const char* calibrationNote = nullptr;
    void loadStickCalibration();
    void updateCalibration(bool b_save, bool b_cancel, uint32_t now);
    void drawCalibration();

    //crash safety [D-146, H13]
    ResetCause resetCause = ResetCause::POWER_ON;
    ResetHistory resetHistory;
    bool b_safeMode = false;
    uint32_t rebootedMarkerUntilMillis = 0;
    void recordBoot();

    //gui [D-146]: lib/RemoteUi draws a ScreenModel into an off-screen frame, the transitions blend it onto the display
    void refreshGui();
    FrequencyTimer guiRefreshEvent;
    LengthTimer displayRevealTimer;
    void startupAnimation();
    void fillScreenModel(ScreenModel& model);
    GFXcanvas1 frame = GFXcanvas1(128, 32);
    GFXcanvas1 previousFrame = GFXcanvas1(128, 32);
    enum class Transition{ NONE, SLIDE_LEFT, SLIDE_RIGHT, WIPE };
    Transition transition = Transition::NONE;
    uint32_t transitionStartMillis = 0;
    int previousView = -1;
    void showFrame(uint32_t now);

    //hardware
    bool isLeftStickCentred();
    bool isRightStickCentred();
    bool isSpeedSwitchOff();
};
