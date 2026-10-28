#pragma once
#include <stdint.h>
#include <Adafruit_GFX.h>
#include <StatusView.h>
#include <PageNavigator.h>
#include <SettingsMenu.h>
#include <StickCalibration.h>
#include <BootGuard.h>
#include <RobotStates.h>
#include <Stopwatch.h>
#include <ReplyFrame.h>
#include <TrackingMap.h>
#include <MarkStore.h>
#include <TrackingControl.h>
#include <TimedTarget.h>
#include <LinkHistory.h>

//The Tracking page's timed run: TrackingRunView in TimedTarget.h (TargetRun::view)

//the trip view (TripMeter)
struct TripView{
    float distance = 0.0f;              //m
    float turned = 0.0f;                //deg
    float top = 0.0f;                   //m/s
    uint32_t movingMillis = 0;
    uint8_t since = 0xFF;               //0 the zero, 1..9 a snap to that mark, 0xFF since power on
};

//Everything the remote's screens show, filled by RemoteBoard once per frame. The drawing below only reads it, so the
//same code draws on the OLED and, on the Mac, into a canvas saved as a picture (tools/screens)
struct ScreenModel{
    uint32_t now = 0;
    uint8_t frame = 0;                  //counts the frames drawn: the alive sign's travelling gap

    //link
    bool b_direct = false;              //the USB remote [D-159]: USB instead of a channel
    bool b_usbWaiting = false;          //Stacato is on the USB, a vehicle still enabled over the radio: CH and USB
                                        //alternate until it is disabled
    bool b_dropout = false;
    uint8_t channel = 0;
    bool b_radioOff = false;            //the channel setting is Off [owner 2026-10-28]
    bool b_linkUp = false;
    float signal = 0.0f;                //0..1, the weaker direction
    uint8_t rxCounter = 0;
    uint8_t loss = 0;
    int16_t downDbm = 0;
    int16_t upDbm = 0;
    float rttMs = 0.0f;

    //batteries
    float robotVoltage = 0.0f;
    uint8_t remotePercent = 0;
    BatteryState remoteBattery = BatteryState::OK;
    float remoteRestingVoltage = 0.0f;
    float remoteVoltage = 0.0f;

    //line 1 markers
    bool b_recovery = false;
    bool b_hot = false;
    bool b_safeMode = false;
    bool b_rebooted = false;

    //status
    StatusView status = {BigMessage::UNKNOWN, StatusLine::NORMAL};
    uint16_t radioFailures = 0;
    uint8_t speedState = 0;
    bool b_sticksOffWarning = false;    //a stick pushed while the speed switch is at OFF (the grind sounds)
    bool b_worldFrame = false;
    StopwatchView stopwatch;            //the rehearsal stopwatch, bottom centre of the Status page [D-160]

    //pages
    Page page = Page::STATUS;
    uint8_t pageCount = 9;          //without Settings
    //transitions that animate the header itself: a title shown instead of the page's own, and the active page dot
    //between two positions (-1: on the page's own dot)
    const char* titleOverride = nullptr;
    float pageDotPosition = -1.0f;

    //motion and motors pages [D-161, D-163]: the vehicle's state as reported (the 3 bit state; the finer one is
    //motion.vehicleState), and the motion from the reply
    uint8_t reportedState = 0;          //RobotState, 0 unknown
    Reply::MotionAvailability motionAvailability = Reply::MotionAvailability::NO_LINK;
    Reply::MotionView motion;
    //the rollers' scroll per wheel (FL, FR, RL, RR), px along the wheel, advanced by RemoteBoard from the frame time
    //and the wheel velocity (RemoteUi::advanceWheelPhase); a picture is set here directly
    float wheelPhase[4] = {0, 0, 0, 0};
    //the Tracking page: the zoom and the trail in the map frame (TrackingMap); the rover's pose is motion's (zeroed
    //with the Motion page's zero)
    TrackingView tracking;
    //the marks, the controls' state (edit mode, the question, the editor), the run and the trip [owner, 2026-10-28]
    MarkStore marks;
    TrackingControlView trackingControl;
    TrackingRunView run;
    TripView trip;
    //the drive-frame arrow: Stacato's own heading (not the display correction) at WORLD, NaN at BODY or without one
    float driveHeading = NAN;
    //the Link page's last 60 s
    LinkHistoryView linkHistory;

    //radio page
    bool b_radioUp = false;
    int temperature = 0;
    uint16_t recoveries = 0;
    uint16_t txTimeouts = 0;
    uint16_t chipResets = 0;

    //inputs page
    float sticks[4] = {0, 0, 0, 0};     //left X, left Y, right X, right Y
    bool b_stickPressed[2] = {false, false};
    bool b_pushbutton[2] = {false, false};
    bool b_disableButton = false;
    bool b_enableButton = false;
    EstopState estop = EstopState::CLEAR;
    bool b_estopNoHigh = true;          //raw contacts (pull-ups): released reads NO high, NC low
    bool b_estopNcHigh = false;

    //about page
    const char* version = "";
    uint32_t uptimeSeconds = 0;
    ResetCause resetCause = ResetCause::POWER_ON;
    uint8_t resetCount = 0;
    bool b_arcadeReady = false;         //the way into the games is open: the header shows the sticks to press
    float arcadeLoading = 0.0f;         //both sticks held, 0..1: the loading screen instead of the page

    //settings
    RemoteSettings settings;

    //menu
    bool b_menuOpen = false;
    SettingsMenu::Item menuItem = SettingsMenu::Item::CHANNEL;
    RemoteSettings pending;
    float saveProgress = 0.0f;
    const char* note = nullptr;         //short confirmation in the menu title ("Saved")

    //calibration [H15]
    bool b_calibrating = false;
    bool b_calibrationResult = false;   //after saving: the result until dismissed
    bool b_calibrationComplete = false;
    bool b_circling = false;
    bool b_keepCircling = false;        //save refused: some axis is short
    uint16_t raw[4] = {512, 512, 512, 512};
    StickCalibration calibration;       //being recorded, or the saved one on the result screen
};

namespace RemoteUi{

    void draw(Adafruit_GFX& g, const ScreenModel& m);

    const char* pageTitle(Page page);

    //[D-163] the vehicle state in capitals (NO STACATO, BUS START, ...); UNKNOWN: the 3 bit state's text or nullptr
    const char* vehicleStateText(Reply::VehicleState state, uint8_t reportedState);

    //the Motors page's rollers: px per second along the wheel at 100 % of the drive's velocity limit
    static constexpr float wheelPixelsPerSecond = 40.0f;
    //one frame's step (elapsed capped at 100 ms, NaN velocity: no step)
    float advanceWheelPhase(float phase, float velocity, uint32_t elapsedMillis);

    //parts, also used by RemoteBoard
    void drawStatus(Adafruit_GFX& g, const ScreenModel& m);
    void drawPage(Adafruit_GFX& g, const ScreenModel& m);
    void drawMenu(Adafruit_GFX& g, const ScreenModel& m);
    void drawCalibration(Adafruit_GFX& g, const ScreenModel& m);
    void drawCalibrationResult(Adafruit_GFX& g, const ScreenModel& m);
};
