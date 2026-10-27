#include "RemoteBoard.h"
#include "Utilities/SafetyNumber.h"
#include <EEPROM.h>
#include <RobotStates.h>
#include <UplinkFrame.h>
#include <LedPattern.h>
#include "Hardware/Watchdog.h"
#ifdef REMOTE_DIRECT
#include "RobotBoard/MavLink.h"
#endif

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
        radio.setChannel(newSettings.channel);
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
    radio.setChannel(settings.channel);
    radio.initialize(1);
    Watchdog::feed();

    //the joysticks are zeroed in the loop once both are released, the remote is live meanwhile (H11)
    displayRevealTimer.setLength(.4);
    displayRevealTimer.start();
    if(resetCause != ResetCause::POWER_ON) rebootedMarkerUntilMillis = millis() + 5000;

#ifdef REMOTE_DIRECT
    directMavlink = new Mavlink();
    directMavlink->setSources(&directProxy, nullptr, 0);
    //no radio to spare: always send at the cyclic rate, so the proxy never times out between frames
    pingSendFrequency = cyclicSendFrequency;
    radioSendEvent.setFrequency(cyclicSendFrequency);
#endif
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

#ifdef REMOTE_DIRECT
    //as the receiver: the remote counts as disconnected 500 ms after its last frame
    if(directProxy.b_connected && millis() - directProxy.lastReceiveTimeMillis > 500) directProxy.b_connected = false;
    directMavlink->update();
#endif

    //the frame first, then the screen: drawing never delays the controls (H13)
    sendData();
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
    if(stickGuard.wasReleased() && !b_joystickZeroCaptured){
        leftJoystick.captureZero();
        rightJoystick.captureZero();
        b_joystickZeroCaptured = true;
        zeroCaptureMillis = now;
        thankYouUntilMillis = now + 1000;
    }

    //pushbuttons: pages, no command [D-147 point 1]
    bool b_previous = leftPushbutton.wasPushed();
    bool b_next = rightPushbutton.wasPushed();
#ifdef REMOTE_DIRECT
    bool b_chord = leftPushbutton.isPressed() && rightPushbutton.isPressed();
    if(b_chord && !b_dropoutChordHeld) b_simulatedDropout = !b_simulatedDropout;
    b_dropoutChordHeld = b_chord;
#endif
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

    bool b_menuActivity = updateMenu(now);
    Page pageBefore = pages.getPage();
    pages.update(b_previous, b_next, b_settingsAvailable, b_menuActivity, now);
    //each page its own note
    if((b_previous || b_next) && pages.getPage() != pageBefore && RemoteSounds::isAllowed(RemoteSounds::Sound::CLICK, settings.sound)){
        soundPlayer.play(RemoteSounds::pageCue(uint8_t(pages.getPage())));
    }

    //enable and disable: a tick on the press, a buzz when enable cannot work now; what the vehicle does follows as a
    //status sound (refreshGui)
    bool b_enable = rightLedButton.isPressed();
    bool b_disable = leftLedButton.isPressed();
    if(b_enable && !b_enableHeld){
        playSound(RemoteSounds::refusesEnable(status.message) ? RemoteSounds::Sound::REFUSED : RemoteSounds::Sound::PRESS);
    }
    if(b_disable && !b_disableHeld) playSound(RemoteSounds::Sound::PRESS);
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
    if(b_direction != b_previousDirection){
        playSound(b_direction ? RemoteSounds::Sound::DIRECTION_SET : RemoteSounds::Sound::DIRECTION_CLEAR);
    }
    b_previousDirection = b_direction;

    //a stick out of centre while the speed switch is at OFF does nothing: a grind for as long as it is out. Not where
    //the sticks have a job (menu, Settings page, calibration, the games) or before they are zeroed
    float deflection = 0.0f;
    for(float v : {leftJoystick.getXValue(), leftJoystick.getYValue(), rightJoystick.getXValue(), rightJoystick.getYValue()}){
        if(fabsf(v) > deflection) deflection = fabsf(v);
    }
    bool b_sticksHaveAJob = menu.isOpen() || b_calibrating || b_calibrationResult || pages.getPage() == Page::SETTINGS ||
                            arcade.isActive() || b_arcadeLoading || !b_joystickZeroCaptured;
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
        if(b_loading && !b_arcadeLoading && b_sound) soundPlayer.play(RemoteSounds::arcadeLoadingCue());
        if(!b_loading && b_arcadeLoading && !b_entering && soundPlayer.playingPriority(SoundLayer::FRONT) == SoundPriority::GAME){
            soundPlayer.stop(SoundLayer::FRONT);
        }
        b_arcadeLoading = b_loading;
        if(b_entering){
            if(b_sound) soundPlayer.play(RemoteSounds::arcadeStartCue());
            arcade.enter(now);
        }
        else return;
    }
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
    arcade.update(sticks, b_previous, b_next, now);
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
#ifndef REMOTE_DIRECT
    //the direct build does not use the radio
    in.b_radioFault = !radio.isInitialized();
    in.radioFaultMillis = radio.getFaultMillis();
#endif
    in.b_linkUp = robotProxy.b_connected;
    in.battery = batteryMonitor.getState();
    in.release = stickGuard.releaseNeeded();
    in.reportedState = robotProxy.status;
    in.sinceEstopReleaseMillis = b_estopWasReleased ? now - estopReleaseMillis : UINT32_MAX;
    in.b_thankYou = int32_t(thankYouUntilMillis - now) > 0;
    return Status::select(in);
}

void RemoteBoard::updateLeds(){
    LedInputs in;
#ifndef REMOTE_DIRECT
    in.b_radioFault = !radio.isInitialized();
#endif
    in.b_localEstop = !estop.isClear();
    in.b_finder = robotProxy.b_connected && robotProxy.functionButtonPressed;
    in.b_linkUp = robotProxy.b_connected;
    in.reportedState = robotProxy.status;
    LedOutput out = Leds::pattern(in, millis());
    //a pressed button lights itself, fully
    leftLedButton.setBrightness(leftLedButton.isPressed() ? 1.0f : (out.b_left ? out.leftLevel : 0.0f));
    rightLedButton.setBrightness(rightLedButton.isPressed() ? 1.0f : (out.b_right ? out.rightLevel : 0.0f));
}



#ifdef REMOTE_DIRECT
//The receiver's half of an exchange, done locally: decode the remote frame, and answer with the receiver's 8 byte
//status frame while Stacato's heartbeat arrives (no Stacato: no answer, the remote shows no link)
void RemoteBoard::directExchange(uint8_t* outgoingFrame){
    if(b_simulatedDropout) return;
    if(!decodeRemoteFrame(outgoingFrame, 8, directProxy)) return;
    directProxy.lastReceiveTimeMillis = millis();
    directProxy.b_connected = true;

    if(!directMavlink->b_online) return;
    uint16_t statusword = 0x7;
    statusword |= (uint8_t(directMavlink->stacatoState) & 0x7) << 3;
    statusword |= (uint8_t(directProxy.speedSelection) & 0x3) << 6;
    statusword |= (directProxy.txCounter & 0xF) << 12;
    uint8_t reply[8] = {0};
    reply[0] = statusword & 0xFF;
    reply[1] = (statusword >> 8) & 0xFF;
    reply[6] = 150;     //receiver RSSI: none
    handleReply(reply, 8);
}
#endif


void RemoteBoard::sendData(){
    if(!radioSendEvent.shouldTrigger()) return;
#ifndef REMOTE_DIRECT
    if(!radio.isInitialized()) return;
#endif

    UplinkControls controls;
    controls.b_disable = leftLedButton.isPressed();
    controls.b_enable = rightLedButton.isPressed();
    controls.speedState = speedToggle.getState();
    controls.b_direction = directionToggle.getSwitchState();
    controls.b_leftStickPressed = leftJoystick.isPressed();
    controls.b_rightStickPressed = rightJoystick.isPressed();
    controls.leftX = leftJoystick.getXValue();
    controls.leftY = leftJoystick.getYValue();
    controls.rightX = rightJoystick.getXValue();
    controls.rightY = rightJoystick.getYValue();
    //the guard catches up with the menu on the next loop: the menu itself counts too, and the games [D-148], which
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

#ifdef REMOTE_DIRECT
    robotProxy.lastSendTimeMicros = micros();
    directExchange(outgoingFrame);
#else
    radio.send(outgoingFrame, 8);
    robotProxy.lastSendTimeMicros = micros();
#endif
}

void RemoteBoard::receiveData(){
#ifndef REMOTE_DIRECT
    //radio listen
    uint8_t incomingFrame[128];
    uint8_t length = 128;
    if(radio.receive(incomingFrame, &length)) handleReply(incomingFrame, length);
#endif

    if(millis() - robotProxy.lastReceiveTime > 500) {
        robotProxy.b_connected = false;
        radioSendEvent.setFrequency(pingSendFrequency);
    }
}

void RemoteBoard::handleReply(uint8_t* incomingFrame, uint8_t length){
    robotProxy.rxCounter++;
    if(length != 8) return;

    uint16_t statusword = incomingFrame[0] | incomingFrame[1] << 8;

    uint8_t header = statusword & 0x7;
    if(header != 0x7) return;
    robotProxy.b_connected = true;
    radioSendEvent.setFrequency(cyclicSendFrequency);
    robotProxy.lastReceiveTime = millis();
    link.onReply();

    robotProxy.status = (statusword >> 3) & 0x7;
    robotProxy.speedModeDisplay = (statusword >> 6) & 0x3;
    robotProxy.directionModeDisplay = statusword & 0x100;
    robotProxy.functionButtonPressed = statusword & 0x200;
    //two bytes free 0b00001100 00000000
    uint8_t mirroredTxCounter = (statusword >> 12) & 0xF;

    robotProxy.robotRxSignalStrength = incomingFrame[6] - 150;
    robotProxy.batteryVoltage = map(float(incomingFrame[7]), 0.0, 255.0, 45.0, 70.0);

    if((robotProxy.txCounter & 0xF) == mirroredTxCounter){
        uint32_t sendToReceiveTimeMicros = micros() - robotProxy.lastSendTimeMicros;
        robotProxy.lastMessageRoundTripTimeMillis = double(sendToReceiveTimeMicros) / 1000.0;
    }
}
