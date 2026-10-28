#include "RemoteBoard.h"
#include <RobotStates.h>
#include <LiIonBattery.h>
#include "Hardware/Watchdog.h"
#include "RobotBoard/MavLink.h"
#include <RemoteTransitions.h>
#include <RemoteSplash.h>
#include <StartupScore.h>

#ifndef FW_VERSION
#define FW_VERSION "unknown"
#endif

//the radio chip's temperature above which the HOT marker shows, a hint only [D-147 point 4]
static const int hotTemperatureCelsius = 70;
//transitions: pages slide with a rolling header and a gliding page dot, the menu slides, a new alarm wipes in
static const uint32_t slideMillis = 160;
static const uint32_t wipeMillis = 100;

static float easeInOutCubic(float in){
    return in < 0.5 ? (4 * pow(in,3)) : (1 - pow(-2 * in + 2, 3) / 2);
};



//everything the screens show, read once per frame
void RemoteBoard::fillScreenModel(ScreenModel& m){
    uint32_t now = millis();
    m.now = now;
    static uint8_t frameCounter = 0;
    m.frame = frameCounter++;

    m.b_direct = linkPath.isUsb();
    m.b_usbWaiting = linkPath.isUsbWaiting(usbMavlink->b_online, now);
    m.b_dropout = b_simulatedDropout;
    if(m.b_direct) m.signal = 1.0;
    else m.signal = LinkMonitor::barFraction(LinkMonitor::weaker(radio.getSignalStrength(), robotProxy.robotRxSignalStrength));
    m.channel = radio.getChannel();
    m.b_radioOff = isRadioOff();
    m.b_linkUp = robotProxy.b_connected;
    m.rxCounter = robotProxy.rxCounter;
    m.loss = link.lossPercent();
    m.downDbm = radio.getSignalStrength();
    m.upDbm = robotProxy.robotRxSignalStrength;
    m.rttMs = robotProxy.lastMessageRoundTripTimeMillis;

    m.robotVoltage = robotProxy.batteryVoltage;
    m.remoteRestingVoltage = batteryLevel.getRestingVoltage();
    m.remoteVoltage = batteryLevel.getVoltage();
    m.remotePercent = LiIonBattery::percentFromCellVoltage(m.remoteRestingVoltage);
    m.remoteBattery = batteryMonitor.getState();

    m.b_recovery = int32_t(recoveryMarkerUntilMillis - now) > 0;
    m.b_hot = radio.isInitialized() && radio.getTemperatureCelsius() > hotTemperatureCelsius;
    m.b_safeMode = b_safeMode;
    m.b_rebooted = int32_t(rebootedMarkerUntilMillis - now) > 0;

    m.status = status;
    m.radioFailures = radio.getInitFailures();
    m.speedState = speedToggle.getState();
    m.b_sticksOffWarning = b_sticksOffWarning;
    m.b_worldFrame = directionToggle.getSwitchState();
    m.stopwatch = stopwatch.view(now);

    m.reportedState = robotProxy.status;
    m.motion = robotProxy.motion;
    //the frame switch to WORLD zeroes the Motion page's pose: shown from then on relative to that moment [D-163]; a
    //snap to a mark (updateTracking) changes the frame the same way
    bool b_newZero = poseZero.update(m.b_worldFrame, robotProxy.motion.poseX, robotProxy.motion.poseY, robotProxy.motion.heading);
    if(b_newZero) tripSince = 0;
    b_newZero = b_newZero || b_frameChanged;
    b_frameChanged = false;
    //the drive-frame arrow: which way the stick forward pushes at WORLD (Stacato's upstage seen from its own heading)
    m.driveHeading = m.b_worldFrame && robotProxy.b_connected ? robotProxy.motion.heading : NAN;
    poseZero.apply(m.motion.poseX, m.motion.poseY, m.motion.heading);
    m.motionAvailability = Reply::availability(robotProxy.b_connected, robotProxy.motion);
    //the Tracking page's trail, in the same frame as the Motion page's pose (a new zero clears it); only a live pose
    //is sampled
    bool b_livePose = m.motionAvailability == Reply::MotionAvailability::LIVE && !isnan(m.motion.heading);
    trackingMap.update(b_livePose ? m.motion.poseX : NAN, b_livePose ? m.motion.poseY : NAN, b_newZero, now);
    trackingMap.follow(b_livePose ? m.motion.poseX : NAN, b_livePose ? m.motion.poseY : NAN,
                       b_livePose ? m.motion.heading : NAN, trackingMap.scaleAt(now));
    trackingMap.view(m.tracking, now);
    //the compass: the map frame's +y, from the heading with the zero applied
    m.tracking.compassHeading = b_livePose ? m.motion.heading : NAN;
    //the trip since the last zero or snap: the body speed and turn rate from the reply
    if(b_newZero) trip.reset();
    float speed = robotProxy.motion.b_velocityLive ? hypotf(robotProxy.motion.vx, robotProxy.motion.vy) : NAN;
    float turnRate = robotProxy.motion.b_velocityLive ? robotProxy.motion.vr * 57.29578f : NAN;
    trip.update(b_livePose ? m.motion.poseX : NAN, b_livePose ? m.motion.poseY : NAN, b_livePose ? m.motion.heading : NAN,
                b_livePose ? speed : NAN, b_livePose ? turnRate : NAN, now);
    m.trip.distance = trip.distance();
    m.trip.turned = trip.turned();
    m.trip.top = trip.topSpeed();
    m.trip.movingMillis = trip.movingMillis();
    m.trip.since = tripSince;
    //the marks, the controls and the run
    m.marks = marks;
    m.trackingControl = trackingControl.view(now);
    updateRun(now);
    timedRun.view(m.run, now);
    //the Link page's history, once a second
    linkHistory.update(now, m.loss, LinkMonitor::weaker(m.downDbm, m.upDbm), m.b_linkUp && !m.b_direct);
    linkHistory.view(m.linkHistory);
    //the Motors page's rollers scroll with each wheel's velocity [D-163]; nothing scrolls without a link
    static float wheelPhase[4] = {0, 0, 0, 0};
    static uint32_t previousFrameMillis = now;
    for(int i = 0; i < 4; i++){
        float velocity = robotProxy.b_connected ? robotProxy.motion.wheelVelocity[i] : NAN;
        wheelPhase[i] = RemoteUi::advanceWheelPhase(wheelPhase[i], velocity, now - previousFrameMillis);
        m.wheelPhase[i] = wheelPhase[i];
    }
    previousFrameMillis = now;

    m.page = pages.getPage();
    m.pageCount = b_settingsAvailable ? uint8_t(Page::COUNT) : uint8_t(Page::SETTINGS);

    m.b_radioUp = radio.isInitialized();
    m.temperature = radio.getTemperatureCelsius();
    m.recoveries = radio.getRecoveries();
    m.txTimeouts = radio.getTxTimeouts();
    m.chipResets = radio.getChipResets();

    m.sticks[0] = leftJoystick.getXValue();
    m.sticks[1] = leftJoystick.getYValue();
    m.sticks[2] = rightJoystick.getXValue();
    m.sticks[3] = rightJoystick.getYValue();
    m.b_stickPressed[0] = leftJoystick.isPressed();
    m.b_stickPressed[1] = rightJoystick.isPressed();
    m.b_pushbutton[0] = leftPushbutton.isPressed();
    m.b_pushbutton[1] = rightPushbutton.isPressed();
    m.b_disableButton = leftLedButton.isPressed();
    m.b_enableButton = rightLedButton.isPressed();
    m.estop = estop.getState();
    m.b_estopNoHigh = estopButton.isNoHigh();
    m.b_estopNcHigh = estopButton.isNcHigh();

    m.version = FW_VERSION;
    m.uptimeSeconds = now / 1000;
    m.resetCause = resetCause;
    m.resetCount = resetHistory.count;

    m.settings = settings;
    m.b_menuOpen = menu.isOpen();
    m.menuItem = menu.getItem();
    m.b_arcadeReady = b_arcadeGateOpen && !b_arcadeLoading;
    m.arcadeLoading = b_arcadeGateOpen ? arcade.gateProgress(millis()) : 0.0f;
    m.pending = menu.getPending();
    m.saveProgress = menu.saveProgress(valueAxis.heldPositiveMillis(now));
    bool b_noteShowing = calibrationNote && int32_t(calibrationNoteUntilMillis - now) > 0;
    m.note = b_noteShowing ? calibrationNote : nullptr;

    m.b_calibrating = b_calibrating;
    m.b_calibrationResult = b_calibrationResult;
    m.b_calibrationComplete = calibrator.isComplete();
    m.b_circling = calibrator.getStep() == StickCalibrator::Step::CIRCLE;
    m.b_keepCircling = b_keepCircling;
    m.raw[0] = leftJoystick.getRawX();
    m.raw[1] = leftJoystick.getRawY();
    m.raw[2] = rightJoystick.getRawX();
    m.raw[3] = rightJoystick.getRawY();
    m.calibration = b_calibrationResult ? stickCalibration : calibrator.getResult();
}



void RemoteBoard::refreshGui(){
    uint32_t now = millis();

    //a new alarm always shows on the Status page (closing the menu, unsaved changes discarded)
    status = selectStatus(now);
    bool b_alarm = Status::isAlarm(status.message);
    bool b_newMessage = status.message != previousMessage;
    BigMessage messageBefore = previousMessage;
    previousMessage = status.message;
    //status changes and warnings sound once [D-158]
    //every refresh: it also follows the vehicle state, which can change while the big message stays [D-163]
    RemoteSounds::Sound statusSound = statusChangeSound(messageBefore, status.message);
    if(statusSound != RemoteSounds::Sound::NONE) playSound(statusSound);
    //the ramp's loop: a new alarm sound has replaced it already (playSound); a status without a sound, or the time
    //limit, ends it here
    bool b_ramp = status.message == BigMessage::STOPPING || status.message == BigMessage::DISABLING;
    if(b_disablingLoop && ((b_newMessage && !b_ramp) || now - disablingLoopStartMillis > RemoteSounds::disablingLoopMaxMillis)){
        soundPlayer.stop(SoundLayer::FRONT);
        b_disablingLoop = false;
    }
    //[owner 2026-10-28] a radio fault or no link while the games run (a radio reset clears itself): the game goes on;
    //lasting gamesLinkGraceMillis, it is shown like any alarm
    bool b_linkAlarm = Status::isLinkAlarm(status.message);
    if(!b_linkAlarm || !arcade.isActive()) b_gamesLinkAlarm = false;
    else if(b_newMessage && !b_gamesLinkAlarm){
        b_gamesLinkAlarm = true;
        gamesLinkAlarmMillis = now;
    }
    bool b_lastedInGames = b_gamesLinkAlarm && now - gamesLinkAlarmMillis >= Status::gamesLinkGraceMillis;
    if((b_newMessage && b_alarm && !b_gamesLinkAlarm) || b_lastedInGames){
        b_gamesLinkAlarm = false;
        if(arcade.isActive()) leaveArcade();
        if(menu.isOpen()) closeMenu();
        b_calibrationResult = false;
        pages.showStatus();
    }

    //a new warning counts as activity for the dimmer
    static uint8_t previousWarnings = 0;
    uint8_t warnings = 0;
    if(int32_t(recoveryMarkerUntilMillis - now) > 0) warnings |= 0x1;
    if(radio.isInitialized() && radio.getTemperatureCelsius() > hotTemperatureCelsius) warnings |= 0x2;
    if(batteryMonitor.getState() != BatteryState::OK) warnings |= 0x4;
    bool b_newWarning = (warnings & ~previousWarnings) != 0;
    //radio hot, battery low: one warning each per power on (a temperature near its limit would toggle)
    static uint8_t warnedOnce = 0;
    if(warnings & ~warnedOnce & 0x6) playSound(RemoteSounds::Sound::WARNING);
    warnedOnce |= warnings & 0x6;
    previousWarnings = warnings;

    //in the menu the contrast being chosen shows at once, and the display stays awake
    uint8_t contrastLevel = menu.isOpen() ? menu.getPending().contrast : settings.contrast;
    bool b_activity = b_inputActivity || b_newMessage || b_newWarning || menu.isOpen() || arcade.isActive();
    uint8_t contrast = dimmer.update(contrastLevel, settings.dimming, b_activity, b_alarm, now);
    b_inputActivity = false;
    if(contrast != appliedContrast){
        display.setContrast(contrast);
        appliedContrast = contrast;
    }

    ScreenModel model;
    fillScreenModel(model);

    //which view this is: a page (0..8), the menu (10), the calibration (11) or its result (12), the games (20)
    int view = int(model.page);
    if(model.b_menuOpen) view = 10;
    if(model.b_menuOpen && model.b_calibrating) view = 11;
    if(model.b_calibrationResult) view = 12;
    if(arcade.isActive()) view = 20;
    if(previousView >= 0 && view != previousView){
        transition = Transition::SLIDE;
        transitionFromView = previousView;
        transitionStartMillis = now;
        if(view < 10 && previousView < 10){
            //pages: the next one comes in from the right, the previous one from the left (wrapping)
            b_transitionForward = (previousView + 1) % model.pageCount == view;
        }
        else b_transitionForward = view > previousView;
    }
    else if(view == int(Page::STATUS) && b_newMessage && b_alarm && !Status::isAlarm(messageBefore)){
        transition = Transition::WIPE;
        transitionStartMillis = now;
    }
    previousView = view;

    //between two pages the header rolls while its page dot glides (drawn in this frame) and the content slides
    uint32_t duration = transition == Transition::WIPE ? wipeMillis : slideMillis;
    uint32_t elapsed = now - transitionStartMillis;
    if(transition != Transition::NONE && elapsed >= duration) transition = Transition::NONE;
    float progress = RemoteTransitions::easeOut(float(elapsed) / float(duration));
    //between two pages with page dots (not those that stay open, Status, Motion and Motors, which have none)
    auto b_dotted = [](int v){ return v >= 0 && v < 10 && !staysOpen(Page(v)); };
    bool b_pageToPage = b_dotted(transitionFromView) && b_dotted(view);
    if(transition == Transition::SLIDE && b_pageToPage){
        model.pageDotPosition = float(transitionFromView) + (float(view) - float(transitionFromView)) * progress;
    }

    frame.fillScreen(0);
    if(arcade.isActive()) arcade.draw(frame);
    else RemoteUi::draw(frame, model);

    if(transition == Transition::NONE){
        display.drawBitmap(0, 0, frame.getBuffer(), 128, 32, WHITE, BLACK);
        memcpy(previousFrame.getBuffer(), frame.getBuffer(), 128 * 32 / 8);
    }
    else if(transition == Transition::SLIDE){
        bool b_pages = transitionFromView < 10 && view < 10;
        RemoteTransitions::Header header = b_pages ? RemoteTransitions::Header::ROLL : RemoteTransitions::Header::SLIDE;
        int16_t drawnFromX = b_pageToPage ? RemoteTransitions::pageDotsLeft(model.pageCount) : 128;
        RemoteTransitions::blend(blended, previousFrame, frame, header, RemoteTransitions::Content::SLIDE,
                                 progress, b_transitionForward, drawnFromX);
        display.drawBitmap(0, 0, blended.getBuffer(), 128, 32, WHITE, BLACK);
    }
    else{
        //a new alarm wipes in from the left
        int16_t shift = int16_t(progress * 128.0);
        display.drawBitmap(0, 0, previousFrame.getBuffer(), 128, 32, WHITE, BLACK);
        for(int16_t x = 0; x < shift; x++){
            for(int16_t y = 0; y < 32; y++) display.drawPixel(x, y, frame.getPixel(x, y) ? WHITE : BLACK);
        }
    }

    if(!displayRevealTimer.hasExpired()){
        int w = 128;
        int x = easeInOutCubic(displayRevealTimer.getProgress()) * w;
        display.fillRect(x,0,w-x,32,BLACK);
    }

    display.display();
}

void RemoteBoard::startupAnimation(){

    FrequencyTimer frameDrawEvent(200.0);

    //both sticks pressed at power on: the former Becker Poels startup instead of BERUPO
    leftJoystick.updateInputs();
    rightJoystick.updateInputs();
    RemoteSplash::Style style = leftJoystick.isPressed() && rightJoystick.isPressed()
        ? RemoteSplash::Style::BECKER_POELS : RemoteSplash::Style::BERUPO;
    const float duration = RemoteSplash::duration(style);
    const float fadeOutStart = RemoteSplash::fadeOutStart(style);

    //the letters on the white fill (RemoteSplash); the pushbutton LEDs blink across while the screen fills and
    //again while it clears, as before
    //the sound: BERUPO hands its score to the synth (StartupScore.h, in time with the letters and the shine; the
    //synth's interrupt plays it); Becker Poels its tune, each note started when the animation reaches it
    bool b_sound = settings.sound != uint8_t(RemoteSounds::Level::OFF);
    if(b_sound && style == RemoteSplash::Style::BERUPO){
        SynthScoreNote score[StartupScore::noteCount];
        StartupScore::build(score);
        sound.playScore(score, StartupScore::noteCount);
    }
    uint8_t noteCount = 0;
    const RemoteSplash::Note* notes = RemoteSplash::tune(style, noteCount);
    uint8_t nextNote = 0;

    LengthTimer splashTimer(duration);
    while(!splashTimer.hasExpired()){
        Watchdog::feed();
        float seconds = splashTimer.getProgress() * duration;
        sound.update();
        if(b_sound && nextNote < noteCount && seconds >= notes[nextNote].start){
            sound.tone(notes[nextNote].hertz, notes[nextNote].millis);
            nextNote++;
        }
        float ll = 0.0;
        float rl = 0.0;
        if(seconds < RemoteSplash::fillInSeconds){
            float progress = seconds / RemoteSplash::fillInSeconds;
            if(progress < 0.5){
                ll = constrain(map(progress, 0.0, 0.25, 1.0, 0.0), 0.0, 1.0);
                rl = constrain(map(progress, 0.25, 0.5, 1.0, 0.0), 0.0, 1.0);
            }else{
                ll = constrain(map(progress, 0.5, 0.75, 0.0, 1.0), 0.0, 1.0);
                rl = constrain(map(progress, 0.75, 1.0, 0.0, 1.0), 0.0, 1.0);
            }
        }else if(seconds >= fadeOutStart){
            float progress = (seconds - fadeOutStart) / RemoteSplash::fadeOutSeconds;
            ll = constrain(map(progress, 0.0, 0.5, 1.0, 0.0), 0.0, 1.0);
            rl = constrain(map(progress, 0.5, 1.0, 1.0, 0.0), 0.0, 1.0);
        }else{
            ll = 1.0;
            rl = 1.0;
        }
        leftLedButton.setBrightness(ll);
        rightLedButton.setBrightness(rl);
        if(frameDrawEvent.shouldTrigger()){
            RemoteSplash::draw(display, seconds, style);
            display.display();
        }
    }
    //the last frame drawn in the loop is a little before the end, the wipe one column short: show the cleared screen
    display.clearDisplay();
    display.display();
    if(style == RemoteSplash::Style::BECKER_POELS) sound.quiet();     //BERUPO's score plays on
    leftLedButton.setBrightness(0.0);
    rightLedButton.setBrightness(0.0);
    display.setTextColor(WHITE);
}
