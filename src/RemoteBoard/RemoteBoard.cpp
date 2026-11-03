#include "RemoteBoard.h"
#include "Utilities/SafetyNumber.h"
#include <EEPROM.h>
#include <RobotStates.h>
#include <UplinkFrame.h>
#include <LedPattern.h>
#include "Hardware/Watchdog.h"
#include "RobotBoard/MavLink.h"

//a stick within 10 % of its rest position counts as released
static const float stickCentredThreshold = 0.1;
//after the zero is captured the joystick values are stale until their next reading (50 Hz)
static const uint32_t zeroSettleMillis = 50;
static uint32_t zeroCaptureMillis = 0;

bool RemoteBoard::isLeftStickCentred(){
    if(!b_joystickZeroCaptured) return leftJoystick.isRawBelowThreshold(stickCentredThreshold);
    return leftJoystick.isBelowThreshold(stickCentredThreshold);
}

bool RemoteBoard::isRightStickCentred(){
    if(!b_joystickZeroCaptured) return rightJoystick.isRawBelowThreshold(stickCentredThreshold);
    return rightJoystick.isBelowThreshold(stickCentredThreshold);
}

//speed switch at LOW = sticks off (D-93); state 3 (both contacts) is read as LOW too
bool RemoteBoard::isSpeedSwitchOff(){
    uint8_t state = speedToggle.getState();
    return state == uint8_t(SpeedMode::LOW_SPEED) || state == uint8_t(SpeedMode::NO_SPEED);
}



void RemoteBoard::loadSettings(){
    uint8_t storedChannel = EEPROM.read(RemoteSettings::channelAddress);
    //safe mode starts with default display settings, but keeps the channel: the E-stop needs the link
    if(b_safeMode) settings = RemoteSettings::fromStored(storedChannel, 0xFF, 0xFF);
    else settings = RemoteSettings::fromStored(storedChannel,
                                               EEPROM.read(RemoteSettings::contrastAddress),
                                               EEPROM.read(RemoteSettings::dimmingAddress),
                                               EEPROM.read(RemoteSettings::soundAddress));
}

//only changed bytes, once on confirm: a write can block for a flash erase (H12)
void RemoteBoard::saveSettings(const RemoteSettings& newSettings){
    if(newSettings.channel != settings.channel){
        EEPROM.write(RemoteSettings::channelAddress, newSettings.channel);
        //Off keeps the radio on its channel, listening; it only stops sending (sendData)
        if(!newSettings.isRadioOff()) radio.setChannel(newSettings.channel);
    }
    if(newSettings.contrast != settings.contrast) EEPROM.write(RemoteSettings::contrastAddress, newSettings.contrast);
    if(newSettings.dimming != settings.dimming) EEPROM.write(RemoteSettings::dimmingAddress, newSettings.dimming);
    if(newSettings.sound != settings.sound) EEPROM.write(RemoteSettings::soundAddress, newSettings.sound);
    settings = newSettings;
}

void RemoteBoard::loadStickCalibration(){
    uint8_t bytes[StickCalibration::size];
    for(int i = 0; i < StickCalibration::size; i++) bytes[i] = EEPROM.read(StickCalibration::address + i);
    stickCalibration = StickCalibration::fromBytes(bytes);
    leftJoystick.setCalibration(stickCalibration.axes[0], stickCalibration.axes[1]);
    rightJoystick.setCalibration(stickCalibration.axes[2], stickCalibration.axes[3]);
}

//inside the settings menu: right pushbutton saves once every axis has its range, left cancels [H15]
void RemoteBoard::updateCalibration(bool b_save, bool b_cancel, uint32_t now){
    if(b_cancel){
        b_calibrating = false;
        calibrationNote = "Cancelled";
        calibrationNoteUntilMillis = now + 1500;
        playSound(RemoteSounds::Sound::DISCARDED);
        return;
    }
    uint16_t raw[StickCalibration::axisCount] = {
        leftJoystick.getRawX(), leftJoystick.getRawY(), rightJoystick.getRawX(), rightJoystick.getRawY()
    };
    calibrator.update(raw, now);
    if(!b_save) return;
    if(!calibrator.isComplete()){
        b_keepCircling = true;
        calibrationNoteUntilMillis = now + 1500;
        playSound(RemoteSounds::Sound::REFUSED);
        return;
    }
    stickCalibration = calibrator.getResult();
    uint8_t bytes[StickCalibration::size];
    stickCalibration.toBytes(bytes);
    for(int i = 0; i < StickCalibration::size; i++) EEPROM.write(StickCalibration::address + i, bytes[i]);
    leftJoystick.setCalibration(stickCalibration.axes[0], stickCalibration.axes[1]);
    rightJoystick.setCalibration(stickCalibration.axes[2], stickCalibration.axes[3]);
    b_calibrating = false;
    b_calibrationResult = true;
    calibrationResultUntilMillis = now + 10000;
    playSound(RemoteSounds::Sound::SAVED);
}

//the marks' block (MarkStore: the layout), read once at boot; a blank or foreign block reads as no marks
void RemoteBoard::loadMarks(){
    uint8_t bytes[MarkStore::size];
    for(int i = 0; i < MarkStore::size; i++) bytes[i] = EEPROM.read(MarkStore::address + i);
    marks.fromBytes(bytes);
}

//after a confirmed change, after the frame: only the bytes that differ (a write can block for a flash erase, H12)
void RemoteBoard::writeMarks(){
    b_marksDirty = false;
    uint8_t bytes[MarkStore::size];
    marks.toBytes(bytes);
    for(int i = 0; i < MarkStore::size; i++){
        if(EEPROM.read(MarkStore::address + i) != bytes[i]) EEPROM.write(MarkStore::address + i, bytes[i]);
    }
}

void RemoteBoard::recordBoot(){
    resetCause = Watchdog::readResetCause();
    uint8_t bytes[ResetHistory::size];
    for(int i = 0; i < ResetHistory::size; i++) bytes[i] = EEPROM.read(ResetHistory::address + i);
    uint32_t nowSeconds = rtc_get();
    resetHistory = ResetHistory::fromBytes(bytes, nowSeconds);
    b_safeMode = resetHistory.onBoot(resetCause, nowSeconds);
    resetHistory.toBytes(bytes);
    for(int i = 0; i < ResetHistory::size; i++) EEPROM.write(ResetHistory::address + i, bytes[i]);
}



void RemoteBoard::initialize(){

    recordBoot();
    Watchdog::begin();

    //configure io devices, turn on leds to signal success
    for(auto ioDevice : ioDevices) ioDevice->initialize();
    leftLedButton.turnOn();
    rightLedButton.turnOn();

    //configure display
    display.initialize();
    delay(150); //before this the display won't show anything
    Watchdog::feed();

    //show a nice startup animation (the settings first: the Sound setting decides on its tune)
    loadSettings();
    startupAnimation();

    //the radio: one attempt here, radio.update() retries every 2 s while the screen shows RADIO FAULT.
    //The channel is remembered until the chip runs.
    loadStickCalibration();
    arcade.setStore(&arcadeStore);
    loadMarks();
    radio.setChannel(settings.isRadioOff() ? RemoteSettings::defaultChannel : settings.channel);
    radio.initialize(1);
    Watchdog::feed();

    //the joysticks are zeroed in the loop once both are released, the remote is live meanwhile (H11)
    displayRevealTimer.setLength(.4);
    displayRevealTimer.start();
    if(resetCause != ResetCause::POWER_ON) rebootedMarkerUntilMillis = millis() + 5000;

    //the USB remote [D-159]: listens on the USB all the time, sends its controls there only in USB mode
    usbMavlink = new Mavlink();
    usbMavlink->setSources(&usbProxy, nullptr, 0);
    usbMavlink->b_sendControls = false;
    usbMavlink->b_neverBlock = true;
}



void RemoteBoard::update(){
    Watchdog::feed();

    //update all input devices; any button or switch change keeps the display bright, the sticks only when deflected
    //(their small jitter around the rest position would keep it awake forever)
    uint32_t previousInputChange = 0;
    for(auto ioDevice : ioDevices){
        if(ioDevice == &leftJoystick || ioDevice == &rightJoystick) continue;
        previousInputChange = max(previousInputChange, ioDevice->lastInputChange_millis);
    }
    for(auto ioDevice : ioDevices) ioDevice->updateInputs();
    for(auto ioDevice : ioDevices){
        if(ioDevice == &leftJoystick || ioDevice == &rightJoystick) continue;
        if(ioDevice->lastInputChange_millis > previousInputChange) b_inputActivity = true;
    }
    if(!leftJoystick.isBelowThreshold(0.1) || !rightJoystick.isBelowThreshold(0.1)) b_inputActivity = true;

    updateOperatorLogic();
    updateLeds();

    //radio: retries a failed init, re-inits after a chip reset
    radio.update();
    if(radio.getRecoveries() != shownRecoveries){
        shownRecoveries = radio.getRecoveries();
        recoveryMarkerUntilMillis = millis() + 3000;
    }
    if(temperatureReadingEvent.shouldTrigger()) radio.updateTemperature();

    //the USB remote, as the receiver: the remote counts as disconnected 500 ms after its last frame
    if(usbProxy.b_connected && millis() - usbProxy.lastReceiveTimeMillis > 500) usbProxy.b_connected = false;
    usbMavlink->update();
    updateLinkPath(millis());

    //the frame first, then the screen: drawing never delays the controls (H13)
    sendData();
    arcade.saveDeferred();      //after the frame: an EEPROM write never delays it
    if(b_marksDirty) writeMarks();
    receiveData();
    link.update(millis());

    if(guiRefreshEvent.shouldTrigger()) refreshGui();
}



void RemoteBoard::updateOperatorLogic(){
    uint32_t now = millis();

    EstopState previousEstop = estop.getState();
    estop.update(estopButton.isNoHigh(), estopButton.isNcHigh(), now);
    if(previousEstop != EstopState::CLEAR && estop.isClear()){
        b_estopWasReleased = true;
        estopReleaseMillis = now;
    }

    batteryMonitor.update(batteryLevel.getRestingVoltage());
    b_settingsAvailable = !b_safeMode && isSpeedSwitchOff();

    //the games hold the sticks like the menu does, and afterwards both must be released first
    stickGuard.update(menu.isOpen() || arcade.isActive(), isLeftStickCentred(), isRightStickCentred());
    //thank you only after the release prompt showed (a stick held at power on), not on every boot
    if(!b_joystickZeroCaptured && stickGuard.releaseNeeded() != StickGuard::Release::NONE) b_releasePromptShown = true;
    if(stickGuard.wasReleased() && !b_joystickZeroCaptured){
        leftJoystick.captureZero();
        rightJoystick.captureZero();
        b_joystickZeroCaptured = true;
        zeroCaptureMillis = now;
        if(b_releasePromptShown) thankYouUntilMillis = now + 1000;
    }

    //pushbuttons: pages, no command [D-147 point 1]
    bool b_previous = leftPushbutton.wasPushed();
    bool b_next = rightPushbutton.wasPushed();
    bool b_chord = leftPushbutton.isPressed() && rightPushbutton.isPressed();
    if(b_chord && !b_dropoutChordHeld && linkPath.isUsb() && !arcade.isActive()) b_simulatedDropout = !b_simulatedDropout;
    b_dropoutChordHeld = b_chord;
    if(b_safeMode){
        b_previous = false;
        b_next = false;
    }
    updateArcade(b_previous, b_next, now);
    //the calibration result: any pushbutton (or 10 s) goes back to the menu
    if(b_calibrationResult){
        if(b_previous || b_next || int32_t(now - calibrationResultUntilMillis) > 0 || !menu.isOpen()) b_calibrationResult = false;
        b_previous = false;
        b_next = false;
    }
    if(b_keepCircling && int32_t(now - calibrationNoteUntilMillis) > 0) b_keepCircling = false;
    //during the calibration the pushbuttons save (right) and cancel (left)
    if(b_calibrating){
        if(b_settingsAvailable && menu.isOpen()) updateCalibration(b_next, b_previous, now);
        else b_calibrating = false;
        b_previous = false;
        b_next = false;
    }
    if((b_previous || b_next) && menu.isOpen()) closeMenu();
    //the Tracking page's controls; a pushbutton that cancels a question there changes no page
    updateTracking(b_previous, b_next, now);

    bool b_menuActivity = updateMenu(now);
    Page pageBefore = pages.getPage();
    pages.update(b_previous, b_next, b_settingsAvailable, b_menuActivity, now);
    //each page its own note
    if((b_previous || b_next) && pages.getPage() != pageBefore && RemoteSounds::isAllowed(RemoteSounds::Sound::CLICK, settings.sound)){
        soundPlayer.play(RemoteSounds::pageCue(uint8_t(pages.getPage())));
    }

    //enable and disable: a tick on the press, the refused "no-no" when the button cannot work now; what the vehicle
    //does follows as a status sound (refreshGui)
    bool b_enable = rightLedButton.isPressed();
    bool b_disable = leftLedButton.isPressed();
    //the games never enable a vehicle (sendData holds enable back while they run [D-157])
    if(b_enable && !b_enableHeld){
        bool b_refused = RemoteSounds::refusesEnable(status.message) || arcade.isActive();
        playSound(b_refused ? RemoteSounds::Sound::REFUSED : RemoteSounds::Sound::PRESS);
    }
    if(b_disable && !b_disableHeld){
        playSound(RemoteSounds::refusesDisable(status.message) ? RemoteSounds::Sound::REFUSED : RemoteSounds::Sound::PRESS);
    }
    b_enableHeld = b_enable;
    b_disableHeld = b_disable;

    //the toggle switches: a sound over their mechanical click
    uint8_t speedState = speedToggle.getState();
    bool b_direction = directionToggle.getSwitchState();
    if(!b_switchesSeen){
        //the first loop: where the switches are at power on, no sound
        previousSpeedState = speedState;
        b_previousDirection = b_direction;
        b_switchesSeen = true;
    }
    playSound(RemoteSounds::forSpeedChange(previousSpeedState, speedState));
    previousSpeedState = speedState;
    //[D-207 point 4] the creep flick, before sendData reads it: leaving middle clears creep in the same loop's frame
    speedFlick.update(speedState, isLeftStickCentred() && isRightStickCentred(), now);
    if(b_direction != b_previousDirection){
        playSound(b_direction ? RemoteSounds::Sound::DIRECTION_SET : RemoteSounds::Sound::DIRECTION_CLEAR);
    }
    b_previousDirection = b_direction;

    updateStopwatch(now);

    //a stick out of centre while the speed switch is at OFF does nothing: a grind for as long as it is out. Not where
    //the sticks have a job (menu, Settings page, calibration, the games, the stopwatch's edit hold) or before they are
    //zeroed
    float deflection = 0.0f;
    for(float v : {leftJoystick.getXValue(), leftJoystick.getYValue(), rightJoystick.getXValue(), rightJoystick.getYValue()}){
        if(fabsf(v) > deflection) deflection = fabsf(v);
    }
    bool b_sticksHaveAJob = menu.isOpen() || b_calibrating || b_calibrationResult || pages.getPage() == Page::SETTINGS ||
                            arcade.isActive() || b_arcadeLoading || !b_joystickZeroCaptured || stopwatch.sticksBusy() ||
                            pages.getPage() == Page::TRACKING;
    b_sticksOffWarning = deflection > 0.15f && isSpeedSwitchOff() && !b_sticksHaveAJob;
    bool b_grind = b_sticksOffWarning && RemoteSounds::isAllowed(RemoteSounds::Sound::CLICK, settings.sound);
    if(b_grind && now - grindMillis >= RemoteSounds::grindPeriodMillis){
        soundPlayer.playNote(RemoteSounds::grindGrain((deflection - 0.15f) / 0.85f, grindSeed), SoundPriority::INTERFACE);
        grindMillis = now;
    }

    //NO LINK: a searching ping with each LED period (setting All; Alarms only keeps the single warning). Not while the
    //games load or run: an interface sound, it would cut their tunes every 2 s
    uint32_t searchPhase = now % RemoteSounds::searchPeriodMillis;
    bool b_games = arcade.isActive() || b_arcadeLoading;
    if(status.message == BigMessage::NO_LINK && searchPhase < previousSearchPhase && !b_games) playSound(RemoteSounds::Sound::SEARCHING);
    previousSearchPhase = searchPhase;

    soundPlayer.update(now, sound);
    sound.update();
    //back on Status after 10 s without input: unsaved changes are discarded
    if(menu.isOpen() && pages.getPage() != Page::SETTINGS) closeMenu();
}

//the stopwatch [D-160]: its controls only on a main page (Status or Motion) showing a state (no alarm, no sticks to release), not in
//the menu, the calibration or the games, and not before the sticks are zeroed. It keeps running everywhere else and
//its time-up sounds wherever the remote is. Nothing of it is sent: the stick buttons go out as before
void RemoteBoard::updateStopwatch(uint32_t now){
    StopwatchControl::Inputs in;
    in.b_active = isMainPage(pages.getPage()) && !menu.isOpen() && !arcade.isActive() && !b_arcadeLoading
               && !b_calibrating && !b_calibrationResult && b_joystickZeroCaptured
               && !Status::isAlarm(status.message) && stickGuard.releaseNeeded() == StickGuard::Release::NONE;
    in.b_leftHeld = leftJoystick.isPressed();
    in.b_rightHeld = rightJoystick.isPressed();
    in.b_speedOff = isSpeedSwitchOff();
    in.rightX = rightJoystick.getXValue();
    in.rightY = rightJoystick.getYValue();
    in.now = now;
    uint16_t events = stopwatch.update(in);
    if(events != StopwatchControl::NONE) b_inputActivity = true;
    using S = RemoteSounds::Sound;
    if(events & StopwatchControl::STARTED) playSound(S::TIMER_START);
    if(events & StopwatchControl::STOPPED) playSound(S::TIMER_STOP);
    //at OFF the reset press opens edit mode at once: its own tick instead
    if(events & StopwatchControl::EDIT_ENTERED) playSound(S::TIMER_EDIT);
    else if(events & StopwatchControl::RESET) playSound(S::TIMER_RESET);
    if(events & StopwatchControl::EDIT_LEFT) playSound(S::TIMER_EDIT_DONE);
    if(events & StopwatchControl::SET_CHANGED) playSound(S::VALUE);
    if(events & (StopwatchControl::SET_LIMIT | StopwatchControl::SET_CLEARED)) playSound(S::BUMP);
    //time up: alarm priority, but it never cuts a status alarm that is sounding (the screen blinks anyway)
    if((events & StopwatchControl::TIME_UP) && soundPlayer.playingPriority(SoundLayer::FRONT) < SoundPriority::ALARM){
        playSound(S::TIME_UP);
    }
}

bool RemoteBoard::displayPose(float& x, float& y, float& heading){
    x = robotProxy.motion.poseX;
    y = robotProxy.motion.poseY;
    heading = robotProxy.motion.heading;
    bool b_live = Reply::availability(robotProxy.b_connected, robotProxy.motion) == Reply::MotionAvailability::LIVE;
    if(!b_live || isnan(x) || isnan(y) || isnan(heading)) return false;
    poseZero.apply(x, y, heading);
    return true;
}

//the Tracking page's controls (lib/RemoteLogic/src/TrackingControl.h, the control map there): only on that page, not
//in the menu, the calibration or the games, and not before the sticks are zeroed. Edit mode at OFF, drive mode at
//SLOW and FAST. Display only: the stick buttons and sticks go out as before
void RemoteBoard::updateTracking(bool& b_previous, bool& b_next, uint32_t now){
    TrackingControl::Inputs in;
    in.b_active = TrackingMap::controlsPage(pages.getPage()) && !menu.isOpen() && !arcade.isActive() && !b_arcadeLoading
               && !b_calibrating && !b_calibrationResult && b_joystickZeroCaptured;
    in.b_speedOff = isSpeedSwitchOff();
    in.sticks[0] = leftJoystick.getXValue();
    in.sticks[1] = leftJoystick.getYValue();
    in.sticks[2] = rightJoystick.getXValue();
    in.sticks[3] = rightJoystick.getYValue();
    in.b_leftHeld = leftJoystick.isPressed();
    in.b_rightHeld = rightJoystick.isPressed();
    in.b_pushbutton = b_previous || b_next;
    in.b_poseKnown = displayPose(in.x, in.y, in.heading);
    in.b_watchRunning = stopwatch.stopwatch().isRunning();
    in.watchMillis = stopwatch.stopwatch().elapsed(now);
    in.b_run = timedRun.isRunning();
    in.runTarget = timedRun.target();
    in.now = now;
    uint32_t events = trackingControl.update(in);
    if(events == TrackingControl::NONE) return;
    b_inputActivity = true;
    if(events & TrackingControl::BUTTON_SPENT){
        b_previous = false;
        b_next = false;
    }
    using S = RemoteSounds::Sound;
    using T = TrackingControl;
    if(events & T::ZOOM){ trackingMap.zoom(now); playSound(S::VALUE); }
    if(events & T::CLEAR_TRAIL){ trackingMap.clearTrail(); playSound(S::CLICK); }
    if(events & T::PLAY_PAUSE){
        //play a timed run to the selected mark (from the pose now), or pause / resume it (TargetRun)
        TargetRun::Result result = timedRun.click(marks, trackingControl.getSelected(), in.b_poseKnown, in.x, in.y, in.heading,
                                             trackingControl.pressMillis(), now);
        if(result == TargetRun::REFUSED) playSound(S::REFUSED);
        else playSound(result == TargetRun::PAUSED ? S::TIMER_STOP : S::TIMER_START);
    }
    if(events & T::REWIND){ timedRun.rewind(); playSound(S::TIMER_RESET); }
    if(events & T::SELECTED) timedRun.acknowledge();
    if(events & T::SNAP){
        uint8_t k = trackingControl.snapMark();
        const Mark& mark = marks.get(k);
        float mh = k == MarkStore::zero ? 0.0f : mark.heading();
        if(poseZero.snap(robotProxy.motion.poseX, robotProxy.motion.poseY, robotProxy.motion.heading, mark.x(), mark.y(), mh)){
            b_frameChanged = true;
            tripSince = k;
            playSound(S::SAVED);
        }
        else playSound(S::REFUSED);
    }
    //a run keeps the plan it started with: changing or clearing its mark meanwhile changes nothing until the next play
    if(events & T::MARKS_CHANGED) b_marksDirty = true;
    if(events & (T::STORED | T::EDIT_SAVED | T::MARK_CLEARED | T::ALL_CLEARED)) playSound(S::SAVED);
    if(events & (T::CANCELLED | T::TIMED_OUT | T::EDIT_LEFT)) playSound(S::DISCARDED);
    if(events & T::REFUSED) playSound(S::REFUSED);
    if(events & T::EDIT_OPENED) playSound(S::TIMER_EDIT);
    if(events & T::ASKED) playSound(S::VALUE);
    if(events & (T::SELECTED | T::VIEW_CHANGED | T::FIELD)) playSound(S::CLICK);
    if(events & T::STEP) playSound(S::VALUE);
    if(events & T::STEP_LIMIT) playSound(S::BUMP);
}

//every refresh: the timed run against the rover's displayed pose (TargetRun: the pace while the stopwatch runs, the
//arrival, which stops the stopwatch; the stopwatch reset elsewhere cancels the run)
void RemoteBoard::updateRun(uint32_t now){
    float x, y, heading;
    bool b_pose = displayPose(x, y, heading);
    if(timedRun.update(b_pose, x, y, heading, now)) playSound(RemoteSounds::Sound::TIMER_STOP);
}

//the games [D-157]: in with both stick buttons held 2 s on the About page with the speed switch at OFF (sticks
//zeroed; the own E-stop may be pressed, the safest state there is); out with the same gesture, after 2 minutes
//without input, when the speed switch leaves OFF, the own E-stop is pressed or released (released: back on Status,
//the vehicle's state in view before driving is possible again) or a new alarm shows (refreshGui). While they run, the pushbuttons are theirs and
//nothing of the sticks is sent (sendData)
void RemoteBoard::updateArcade(bool& b_previous, bool& b_next, uint32_t now){
    bool b_allowed = b_settingsAvailable && b_joystickZeroCaptured;
    bool b_estopClear = estop.isClear();
    bool b_estopChanged = b_estopClear != b_arcadeEstopClear;
    b_arcadeEstopClear = b_estopClear;
    if(!arcade.isActive()){
        bool b_bothHeld = leftJoystick.isPressed() && rightJoystick.isPressed();
        bool b_gateOpen = b_allowed && pages.getPage() == Page::ABOUT && !menu.isOpen();
        b_arcadeGateOpen = b_gateOpen;
        bool b_entering = arcade.updateGate(b_gateOpen, b_bothHeld, now);
        //the loading tune while both are held, stopped if they are let go early; a jingle as the games open
        bool b_loading = arcade.gateProgress(now) > 0.0f;
        bool b_sound = RemoteSounds::gamesAllowed(settings.sound);
        //the owner holds both sticks on purpose: the tune takes the front layer from whatever still sounds there (a
        //status chime would otherwise refuse it, games priority being the lowest); an alarm that comes later still
        //cuts in. The same for the jingle as the games open
        if(b_loading && !b_arcadeLoading && b_sound){
            soundPlayer.stop(SoundLayer::FRONT);
            soundPlayer.play(RemoteSounds::arcadeLoadingCue());
        }
        if(!b_loading && b_arcadeLoading && !b_entering && soundPlayer.playingPriority(SoundLayer::FRONT) == SoundPriority::GAME){
            soundPlayer.stop(SoundLayer::FRONT);
        }
        b_arcadeLoading = b_loading;
        if(b_entering){
            if(b_sound){
                soundPlayer.stop(SoundLayer::FRONT);
                soundPlayer.play(RemoteSounds::arcadeStartCue());
            }
            arcade.enter(now);
        }
        else return;
    }
    //the games and the arcade sound into the player, unless the Sound setting is Off (the arcade's closing jingle too)
    arcade.setSound(RemoteSounds::gamesAllowed(settings.sound) ? &soundPlayer : nullptr);
    if(!b_allowed || arcade.exitRequested() || b_estopChanged){
        leaveArcade();
        return;
    }
    GameInput sticks;
    sticks.stick[0] = leftJoystick.getXValue();
    sticks.stick[1] = leftJoystick.getYValue();
    sticks.stick[2] = rightJoystick.getXValue();
    sticks.stick[3] = rightJoystick.getYValue();
    sticks.held[0] = leftJoystick.isPressed();
    sticks.held[1] = rightJoystick.isPressed();
    //the pushbuttons held too: both held 3 s in the picker offer to clear the game's top 3
    arcade.update(sticks, b_previous, b_next, now, leftPushbutton.isPressed(), rightPushbutton.isPressed());
    b_previous = false;
    b_next = false;
}

void RemoteBoard::leaveArcade(){
    arcade.leave();
    pages.showStatus();
    itemAxis.blockUntilReleased();
    valueAxis.blockUntilReleased();
}

void RemoteBoard::closeMenu(){
    menu.close();
    b_calibrating = false;
    itemAxis.blockUntilReleased();
    valueAxis.blockUntilReleased();
}

//sticks navigate the settings menu, only with the speed switch at OFF [D-147 point 2]
bool RemoteBoard::updateMenu(uint32_t now){
    auto strongest = [](float a, float b){ return fabsf(a) > fabsf(b) ? a : b; };
    float itemValue = strongest(leftJoystick.getYValue(), rightJoystick.getYValue());
    float valueValue = strongest(leftJoystick.getXValue(), rightJoystick.getXValue());

    if(!b_settingsAvailable || !b_joystickZeroCaptured){
        //leaving OFF closes the menu at once, unsaved changes are discarded
        if(menu.isOpen()) closeMenu();
        return false;
    }
    if(b_calibrating || b_calibrationResult) return true;
    if(menu.isOpen()){
        int8_t itemStep = -itemAxis.update(itemValue, now);    //stick up: previous item
        int8_t valueStep = valueAxis.update(valueValue, now);
        uint32_t heldRight = valueAxis.heldPositiveMillis(now);
        SettingsMenu::Item itemBefore = menu.getItem();
        RemoteSettings pendingBefore = menu.getPending();
        SettingsMenu::Event event = menu.update(itemStep, valueStep, heldRight);
        //a click for a new item, a higher one for a new value, a bump at the ends
        bool b_valueItem = itemBefore == SettingsMenu::Item::CHANNEL || itemBefore == SettingsMenu::Item::CONTRAST ||
                           itemBefore == SettingsMenu::Item::DIMMING || itemBefore == SettingsMenu::Item::SOUND;
        if(itemStep != 0) playSound(menu.getItem() != itemBefore ? RemoteSounds::Sound::CLICK : RemoteSounds::Sound::BUMP);
        else if(valueStep != 0 && b_valueItem){
            playSound(menu.getPending() != pendingBefore ? RemoteSounds::Sound::VALUE : RemoteSounds::Sound::BUMP);
        }
        if(event == SettingsMenu::Event::SAVED) playSound(RemoteSounds::Sound::SAVED);
        if(event == SettingsMenu::Event::CLOSED) playSound(RemoteSounds::Sound::DISCARDED);
        if(event == SettingsMenu::Event::CALIBRATE) playSound(RemoteSounds::Sound::CLICK);
        if(event == SettingsMenu::Event::CALIBRATE){
            b_calibrating = true;
            b_keepCircling = false;
            calibrationNote = nullptr;
            calibrator.start(now);
            itemAxis.blockUntilReleased();
            valueAxis.blockUntilReleased();
            return true;
        }
        if(event == SettingsMenu::Event::SAVED) saveSettings(menu.getPending());
        if(event != SettingsMenu::Event::NONE){
            closeMenu();
            pages.showStatus();
        }
        return itemStep != 0 || valueStep != 0 || heldRight > 0;
    }
    if(pages.getPage() == Page::SETTINGS){
        itemAxis.update(itemValue, now);
        if(valueAxis.update(valueValue, now) > 0){
            menu.open(settings);
            playSound(RemoteSounds::Sound::CLICK);
            itemAxis.blockUntilReleased();
            valueAxis.blockUntilReleased();
            return true;
        }
    }
    return false;
}

StatusView RemoteBoard::selectStatus(uint32_t now){
    StatusInputs in;
    in.estop = estop.getState();
    //the USB remote does not use the radio, nor one switched off
    in.b_radioOff = isRadioOff();
    if(!linkPath.isUsb() && !in.b_radioOff){
        in.b_radioFault = !radio.isInitialized();
        in.radioFaultMillis = radio.getFaultMillis();
    }
    in.b_linkUp = robotProxy.b_connected;
    in.battery = batteryMonitor.getState();
    in.release = stickGuard.releaseNeeded();
    in.reportedState = robotProxy.status;
    in.vehicle = robotProxy.motion.vehicleState;
    in.sinceEstopReleaseMillis = b_estopWasReleased ? now - estopReleaseMillis : UINT32_MAX;
    in.b_thankYou = int32_t(thankYouUntilMillis - now) > 0;
    return Status::select(in);
}

void RemoteBoard::updateLeds(){
    LedInputs in;
    in.b_radioOff = isRadioOff();
    if(!linkPath.isUsb() && !in.b_radioOff) in.b_radioFault = !radio.isInitialized();
    in.b_localEstop = !estop.isClear();
    in.b_finder = robotProxy.b_connected && robotProxy.functionButtonPressed;
    in.b_linkUp = robotProxy.b_connected;
    in.reportedState = robotProxy.status;
    in.vehicle = robotProxy.motion.vehicleState;
    LedOutput out = Leds::pattern(in, millis());
    //a pressed button lights itself, fully
    leftLedButton.setBrightness(leftLedButton.isPressed() ? 1.0f : (out.b_left ? out.leftLevel : 0.0f));
    rightLedButton.setBrightness(rightLedButton.isPressed() ? 1.0f : (out.b_right ? out.rightLevel : 0.0f));
}



//[D-159] radio or USB: the switch itself is LinkPath's (lib/RemoteLogic). Either way the replies of the new path
//take over the link state within one frame; nothing is reset, so a handover shows no NO LINK when both answer
void RemoteBoard::updateLinkPath(uint32_t now){
    bool b_wasUsb = linkPath.isUsb();
    if(!linkPath.update(usbMavlink->b_online, robotProxy.b_connected, robotProxy.status, now)) return;
    //either way, a button held through the handover is not a press: the new path sees enable and disable released
    //until both are let go (radio to USB also syncs the MAVLink edges, usbExchange)
    b_buttonsHeldThroughHandover = rightLedButton.isPressed() || leftLedButton.isPressed();
    radio.setQuiet(linkPath.isUsb());
    //the motion comes from the other side now [D-161]: shown again once it sends it
    robotProxy.motion.clear();
    bool b_usb = linkPath.isUsb();
    usbMavlink->b_sendControls = b_usb;
    if(b_usb){
        //no radio to spare: always send at the cyclic rate, so the proxy never times out between frames
        pingSendFrequency = cyclicSendFrequency;
        b_usbFirstFrame = true;
    }
    else if(b_wasUsb){
        pingSendFrequency = 2.0;
        b_simulatedDropout = false;
        usbProxy.b_connected = false;       //Stacato gets zero sticks and no link bit, should it come back (D-44)
    }
    radioSendEvent.setFrequency(cyclicSendFrequency);
}

//The receiver's half of an exchange, done locally: decode the remote frame, and answer with the receiver's 13 byte
//reply [D-161, D-163] while Stacato's heartbeat arrives (no Stacato: no answer, the remote shows no link): the motion
//and pose Stacato sends on this USB, its MAV_STATE and armed flag in byte 12, no receiver RSSI, no vehicle battery
void RemoteBoard::usbExchange(uint8_t* outgoingFrame){
    if(b_simulatedDropout) return;
    if(!decodeRemoteFrame(outgoingFrame, 8, usbProxy)) return;
    usbProxy.lastReceiveTimeMillis = millis();
    usbProxy.b_connected = true;
    if(b_usbFirstFrame){
        usbMavlink->syncCommandEdges();
        b_usbFirstFrame = false;
    }

    if(!usbMavlink->b_online) return;
    Reply::Status status;
    status.state = uint8_t(usbMavlink->stacatoState) & 0x7;
    status.speed = uint8_t(usbProxy.speedSelection) & 0x3;
    status.b_creep = usbProxy.b_connected && usbProxy.b_creep;     //[D-207] as relayed to Stacato
    status.txCounter = usbProxy.txCounter & 0xF;
    status.page = usbReplyPager.next();
    status.mavState = usbMavlink->vehicleMavState();
    status.b_armed = usbMavlink->vehicleArmed();
    uint8_t reply[Reply::length];
    Reply::encode(status, usbMavlink->motionTelemetry(), 150, 0, reply);     //receiver RSSI: none (150)
    handleReply(reply, Reply::length);
}


void RemoteBoard::sendData(){
    if(!radioSendEvent.shouldTrigger()) return;
    if(!linkPath.isUsb() && !radio.isInitialized()) return;
    //[owner 2026-10-28] radio off: nothing goes out (the vehicle sees this remote gone, as when it is switched off)
    if(isRadioOff()) return;

    UplinkControls controls;
    controls.b_disable = leftLedButton.isPressed();
    controls.b_enable = rightLedButton.isPressed();
    if(b_buttonsHeldThroughHandover){
        if(!controls.b_enable && !controls.b_disable) b_buttonsHeldThroughHandover = false;
        controls.b_enable = false;
        controls.b_disable = false;
    }
    //the games never enable a vehicle; disable always works [D-157]
    if(arcade.isActive()) controls.b_enable = false;
    controls.speedState = speedToggle.getState();
    controls.b_creep = speedFlick.isCreep();        //sent only at MEDIUM (Uplink::encode) [D-207]
    controls.b_direction = directionToggle.getSwitchState();
    controls.b_leftStickPressed = leftJoystick.isPressed();
    controls.b_rightStickPressed = rightJoystick.isPressed();
    controls.leftX = leftJoystick.getXValue();
    controls.leftY = leftJoystick.getYValue();
    controls.rightX = rightJoystick.getXValue();
    controls.rightY = rightJoystick.getYValue();
    //the guard catches up with the menu on the next loop: the menu itself counts too, and the games [D-157], which
    //send neither the sticks nor their buttons
    bool b_neutralSticks = stickGuard.holdNeutral() || menu.isOpen() || arcade.isActive()
                        || millis() - zeroCaptureMillis < zeroSettleMillis;
    if(arcade.isActive()){
        controls.b_leftStickPressed = false;
        controls.b_rightStickPressed = false;
    }

    robotProxy.txCounter++;
    uint16_t safetyNumber = estop.isClear() ? Safety::generateClearNumber() : Safety::generateSafeNumber();
    uint8_t outgoingFrame[8];
    Uplink::encode(controls, b_neutralSticks, robotProxy.txCounter, safetyNumber, outgoingFrame);
    link.onSent();

    if(linkPath.isUsb()){
        robotProxy.lastSendTimeMicros = micros();
        usbExchange(outgoingFrame);
    }
    else{
        radio.send(outgoingFrame, 8);
        robotProxy.lastSendTimeMicros = micros();
    }
}

void RemoteBoard::receiveData(){
    //radio listen; the USB remote ignores what it hears (another remote's receiver on the channel)
    uint8_t incomingFrame[128];
    uint8_t length = 0;
    if(radio.receive(incomingFrame, sizeof(incomingFrame), &length) && !linkPath.isUsb() && !isRadioOff()) handleReply(incomingFrame, length);

    if(millis() - robotProxy.lastReceiveTime > 500) {
        robotProxy.b_connected = false;
        radioSendEvent.setFrequency(pingSendFrequency);
        //no link: no motion is shown as live [D-161]
        robotProxy.motion.clear();
        robotProxy.creepDisplay = false;    //nor creep [D-207]
    }
}

RemoteSounds::Sound RemoteBoard::statusChangeSound(BigMessage from, BigMessage to){
    Reply::VehicleState vehicleBefore = soundVehicleState;
    soundVehicleState = robotProxy.motion.vehicleState;
    if(from == to && soundVehicleState == vehicleBefore) return RemoteSounds::Sound::NONE;
    return RemoteSounds::forStatusChange(from, to, vehicleBefore, soundVehicleState);
}

void RemoteBoard::handleReply(uint8_t* incomingFrame, uint8_t length){
    robotProxy.rxCounter++;
    //the 13 byte reply [D-163]; any other length or header is not one
    Reply::Decoded reply;
    if(!Reply::decode(incomingFrame, length, reply)) return;
    robotProxy.b_connected = true;
    radioSendEvent.setFrequency(cyclicSendFrequency);
    robotProxy.lastReceiveTime = millis();
    link.onReply();

    robotProxy.status = reply.status.state;
    robotProxy.speedModeDisplay = reply.status.speed;
    robotProxy.directionModeDisplay = reply.status.b_direction;
    robotProxy.functionButtonPressed = reply.status.b_function;
    robotProxy.creepDisplay = reply.status.b_creep;
    uint8_t mirroredTxCounter = reply.status.txCounter;

    //in page 3 (2.5 Hz [D-163]), kept in between
    if(reply.b_slowValues){
        robotProxy.robotRxSignalStrength = reply.rssiByte - 150;
        robotProxy.batteryVoltage = map(float(reply.batteryByte), 0.0, 255.0, 45.0, 70.0);
    }
    //the pose carried forward from the arrival times [D-163]
    robotProxy.motion.apply(reply, millis());

    if((robotProxy.txCounter & 0xF) == mirroredTxCounter){
        uint32_t sendToReceiveTimeMicros = micros() - robotProxy.lastSendTimeMicros;
        robotProxy.lastMessageRoundTripTimeMillis = double(sendToReceiveTimeMicros) / 1000.0;
    }
}
