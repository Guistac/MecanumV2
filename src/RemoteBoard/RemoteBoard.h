
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
#include <PoseZero.h>
#include <TrackingMap.h>
#include <MarkStore.h>
#include <TrackingControl.h>
#include <TimedTarget.h>
#include <TripMeter.h>
#include <LinkHistory.h>
#include <PageNavigator.h>
#include <SpeedFlick.h>
#include <CreepView.h>
#include <StatusView.h>
#include <Dimmer.h>
#include <BootGuard.h>
#include <LinkMonitor.h>
#include <StickCalibration.h>
#include <RemoteScreens.h>
#include <Arcade.h>
#include "ArcadeEeprom.h"
#include "RemoteSound.h"
#include <SoundPlayer.h>
#include <RemoteSounds.h>
#include <Stopwatch.h>

#include <LinkPath.h>
#include <ReplyFrame.h>
#include "Utilities/RemoteProxy.h"
class Mavlink;

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
#define REMOTE_BUZZER_PIN 19
#define REMOTE_BUZZER_B_PIN 18        //the disc's other leg (220 ohm in series)

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
        bool creepDisplay = false;              //[D-207] the reply's creep echo (status word bit 10)
        int8_t robotRxSignalStrength = 0;       //from the reply's page 3 (2.5 Hz) [D-163]
        float batteryVoltage = 0.0;             //the same
        //[D-161] the motion from the reply, the last value of each paged field; [D-163] the pose carried forward
        //between its pages and the vehicle state from byte 12
        Reply::MotionView motion;
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

    //the USB remote [D-159]: with Stacato's heartbeat on its own USB the remote talks MAVLink to Stacato itself,
    //doing the receiver's job; the radio then stays in receive and never transmits (LinkPath). In USB mode both
    //pushbuttons together toggle a simulated radio dropout (not in the games)
    Mavlink* usbMavlink = nullptr;
    RemoteProxy usbProxy;
    Reply::Pager usbReplyPager;     //the reply built here, as the receiver builds it [D-161, D-163]
    LinkPath linkPath;
    bool b_simulatedDropout = false;
    bool b_dropoutChordHeld = false;
    bool b_usbFirstFrame = false;
    bool b_buttonsHeldThroughHandover = false;      //enable or disable held as the path changed: sent released until let go
    void updateLinkPath(uint32_t now);
    void usbExchange(uint8_t* outgoingFrame);

    //operator logic [D-147] (lib/RemoteLogic, tested with pio test -e native)
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
    bool b_releasePromptShown = false;      //a stick was held at power on: "thank you" once it is released
    bool b_estopWasReleased = false;
    uint32_t estopReleaseMillis = 0;
    uint32_t thankYouUntilMillis = 0;
    StatusView status = {BigMessage::UNKNOWN, StatusLine::NORMAL};
    BigMessage previousMessage = BigMessage::UNKNOWN;
    bool b_inputActivity = false;
    uint8_t appliedContrast = 0;
    void updateOperatorLogic();
    bool updateMenu(uint32_t now);
    //sounds [D-158]: the player picks the notes, sound drives the disc
    RemoteSound sound{REMOTE_BUZZER_PIN, REMOTE_BUZZER_B_PIN};
    SoundPlayer soundPlayer;     //any part plays into it (a SoundSink): the interface here, the games (Arcade)
    void playSound(RemoteSounds::Sound s){
        if(!RemoteSounds::isAllowed(s, settings.sound)) return;
        if(!soundPlayer.play(RemoteSounds::cue(s))) return;
        //an alarm sound replaces the ramp's loop; the loop itself is tracked from its start
        if(RemoteSounds::isAlarm(s)){
            b_disablingLoop = s == RemoteSounds::Sound::STATUS_DISABLING;
            disablingLoopStartMillis = millis();
        }
    }
    //[D-163] the status sound for this refresh: a change of the big message or of the vehicle state (reply byte 12)
    //picks it (RemoteSounds::forStatusChange with the vehicle states); call once per GUI refresh with the message
    //before and now
    RemoteSounds::Sound statusChangeSound(BigMessage from, BigMessage to);
    Reply::VehicleState soundVehicleState = Reply::VehicleState::NO_LINK;
    //the ramp's loop (STATUS_DISABLING) runs until the next alarm sound cuts in; refreshGui ends it on a status
    //without a sound and after RemoteSounds::disablingLoopMaxMillis
    bool b_disablingLoop = false;
    uint32_t disablingLoopStartMillis = 0;
    bool b_enableHeld = false;
    bool b_disableHeld = false;
    bool b_arcadeGateOpen = false;      //the About page offers the games (the header shows it)
    bool b_arcadeLoading = false;       //both sticks held towards them
    bool b_arcadeEstopClear = true;     //the own E-stop at the last loop: a change leaves the games
    bool b_sticksOffWarning = false;    //a stick out of centre at speed OFF: the grind, and OFF blinks on Status
    uint32_t grindMillis = 0;           //the sticks-off grind's last grain
    uint32_t grindSeed = 2463534242u;
    bool b_switchesSeen = false;        //the switches' states at power on are taken without a sound
    uint8_t previousSpeedState = 0;
    CreepView::ShownSound creepShownSound;      //the creep sounds from the shown CREEP (lib/RemoteLogic/src/CreepView.h)
    SpeedFlick speedFlick;              //[D-207] the creep gesture on the speed switch (lib/RemoteLogic/src/SpeedFlick.h)
    bool b_previousDirection = false;
    uint32_t previousSearchPhase = 0;
    void closeMenu();
    void updateLeds();
    //the rehearsal stopwatch on the main pages, Status and Motion [D-160]: display only, the stick buttons start / stop and reset it,
    //the left one held at OFF edits the countdown (lib/RemoteLogic/src/Stopwatch.h, tested with pio test -e native)
    StopwatchControl stopwatch;
    PoseZero poseZero;              //the Motion page's own zero, the frame switch to WORLD [D-163]
    void updateStopwatch(uint32_t now);
    //the Tracking page (owner, 2026-10-28): the map (lib/RemoteLogic/src/TrackingMap.h, the trail sampled in
    //fillScreenModel, cleared by a new zero or snap), the marks (MarkStore, EEPROM from byte 512), the controls in
    //edit mode (OFF) and drive mode (SLOW / FAST, TrackingControl), the timed run to a mark with its ghost, pace and
    //arrival (TargetRun, TimedTarget.h; its time is the stopwatch), the trip readout (TripMeter). Display only: nothing
    //of it is sent. Main thread
    TrackingMap trackingMap;
    MarkStore marks;
    TrackingControl trackingControl{marks};
    TargetRun timedRun{stopwatch};
    TripMeter trip;
    bool b_frameChanged = false;        //a snap this loop: the trail and the trip start over (fillScreenModel)
    uint8_t tripSince = 0xFF;           //what the trip counts from: 0 the zero, 1..9 a snap to that mark, 0xFF power on
    bool b_marksDirty = false;          //a confirmed change: written after the frame (update)
    //the Tracking page's sticks; the pushbuttons cancel a question or leave the editor there (then no page change)
    void updateTracking(bool& b_previous, bool& b_next, uint32_t now);
    //the pose as the pages show it (Stacato's through the display correction); false without a live pose
    bool displayPose(float& x, float& y, float& heading);
    void updateRun(uint32_t now);
    void loadMarks();
    void writeMarks();
    //the Link page's last 60 s (LinkHistory), sampled in fillScreenModel
    LinkHistory linkHistory;

    //the games [D-157] (lib/RemoteGames, tested with pio test -e native): their top 3s and sound mode in
    //EEPROM from byte 256, their sounds into the player unless the Sound setting is Off
    Arcade arcade;
    ArcadeEeprom arcadeStore;
    void updateArcade(bool& b_previous, bool& b_next, uint32_t now);
    void leaveArcade();
    //the channel setting is Off and no USB link carries the remote [owner 2026-10-28]
    bool isRadioOff() const { return settings.isRadioOff() && !linkPath.isUsb(); }
    //a link alarm shown while the games run: they go on unless it lasts Status::gamesLinkGraceMillis
    bool b_gamesLinkAlarm = false;
    uint32_t gamesLinkAlarmMillis = 0;
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

    //crash safety [D-147, H13]
    ResetCause resetCause = ResetCause::POWER_ON;
    ResetHistory resetHistory;
    bool b_safeMode = false;
    uint32_t rebootedMarkerUntilMillis = 0;
    void recordBoot();

    //gui [D-147]: lib/RemoteUi draws a ScreenModel into an off-screen frame, the transitions blend it onto the display
    void refreshGui();
    FrequencyTimer guiRefreshEvent;
    LengthTimer displayRevealTimer;
    void startupAnimation();
    void fillScreenModel(ScreenModel& model);
    GFXcanvas1 frame = GFXcanvas1(128, 32);
    GFXcanvas1 previousFrame = GFXcanvas1(128, 32);     //the last whole frame shown: where a transition starts
    GFXcanvas1 blended = GFXcanvas1(128, 32);
    enum class Transition{ NONE, SLIDE, WIPE };
    Transition transition = Transition::NONE;
    uint32_t transitionStartMillis = 0;
    int transitionFromView = -1;
    bool b_transitionForward = true;
    int previousView = -1;

    //hardware
    bool isLeftStickCentred();
    bool isRightStickCentred();
    bool isSpeedSwitchOff();
};
