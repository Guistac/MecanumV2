//Host tests of the remote's decision logic [D-146]: pio test -e native
#include <unity.h>
#include <string.h>

#include <StickGuard.h>
#include <AxisStepper.h>
#include <EstopContacts.h>
#include <BatteryMonitor.h>
#include <RemoteSettings.h>
#include <SettingsMenu.h>
#include <PageNavigator.h>
#include <StatusView.h>
#include <Dimmer.h>
#include <BootGuard.h>
#include <UplinkFrame.h>
#include <LinkMonitor.h>
#include <LedPattern.h>
#include <StickCalibration.h>

void setUp(){}
void tearDown(){}

//——— StickGuard ———

void test_stickguard_holds_from_boot_until_both_centred(){
    StickGuard guard;
    guard.update(false, true, false);
    TEST_ASSERT_TRUE(guard.holdNeutral());
    TEST_ASSERT_TRUE(guard.releaseNeeded() == StickGuard::Release::RIGHT);
    guard.update(false, false, false);
    TEST_ASSERT_TRUE(guard.releaseNeeded() == StickGuard::Release::BOTH);
    guard.update(false, true, true);
    TEST_ASSERT_FALSE(guard.holdNeutral());
    TEST_ASSERT_TRUE(guard.wasReleased());
    TEST_ASSERT_FALSE(guard.wasReleased());
    //moving a stick afterwards is normal driving
    guard.update(false, false, true);
    TEST_ASSERT_FALSE(guard.holdNeutral());
}

void test_stickguard_holds_in_menu_and_after_until_centred(){
    StickGuard guard;
    guard.update(false, true, true);
    guard.wasReleased();
    guard.update(true, false, true);
    TEST_ASSERT_TRUE(guard.holdNeutral());
    TEST_ASSERT_TRUE(guard.releaseNeeded() == StickGuard::Release::NONE);
    //menu closed with the left stick still deflected (navigating, or the switch moved out of OFF)
    guard.update(false, false, true);
    TEST_ASSERT_TRUE(guard.holdNeutral());
    TEST_ASSERT_TRUE(guard.releaseNeeded() == StickGuard::Release::LEFT);
    guard.update(false, true, true);
    TEST_ASSERT_FALSE(guard.holdNeutral());
}

//——— AxisStepper ———

void test_axisstepper_steps_rearms_and_repeats(){
    AxisStepper axis;
    TEST_ASSERT_EQUAL_INT8(0, axis.update(0.5f, 0));
    TEST_ASSERT_EQUAL_INT8(1, axis.update(0.7f, 10));
    TEST_ASSERT_EQUAL_INT8(0, axis.update(0.9f, 20));
    TEST_ASSERT_EQUAL_INT8(0, axis.update(0.9f, 509));
    TEST_ASSERT_EQUAL_INT8(1, axis.update(0.9f, 510));
    TEST_ASSERT_EQUAL_INT8(0, axis.update(0.9f, 709));
    TEST_ASSERT_EQUAL_INT8(1, axis.update(0.9f, 710));
    TEST_ASSERT_EQUAL_UINT32(700, axis.heldPositiveMillis(710));
    //back under 30 % re-arms, between 30 and 60 % does not
    TEST_ASSERT_EQUAL_INT8(0, axis.update(0.4f, 720));
    TEST_ASSERT_EQUAL_INT8(0, axis.update(0.7f, 730));
    TEST_ASSERT_EQUAL_INT8(0, axis.update(0.2f, 740));
    TEST_ASSERT_EQUAL_UINT32(0, axis.heldPositiveMillis(740));
    TEST_ASSERT_EQUAL_INT8(-1, axis.update(-0.8f, 750));
}

void test_axisstepper_block_until_released(){
    AxisStepper axis;
    axis.update(0.9f, 0);
    axis.blockUntilReleased();
    TEST_ASSERT_EQUAL_INT8(0, axis.update(0.9f, 600));
    TEST_ASSERT_EQUAL_INT8(0, axis.update(0.9f, 2000));
    TEST_ASSERT_EQUAL_INT8(0, axis.update(0.1f, 2010));
    TEST_ASSERT_EQUAL_INT8(1, axis.update(0.9f, 2020));
}

//——— EstopContacts ———

void test_estop_clear_pressed_and_wiring(){
    EstopContacts estop;
    //released: NO open (high), NC closed (low)
    TEST_ASSERT_TRUE(estop.update(true, false, 0) == EstopState::CLEAR);
    //pressed on the remotes (only NC connected): NO1 NC1, held as long as it likes, never a wiring fault
    TEST_ASSERT_TRUE(estop.update(true, true, 10) == EstopState::PRESSED);
    TEST_ASSERT_TRUE(estop.update(true, true, 5000) == EstopState::PRESSED);
    TEST_ASSERT_FALSE(estop.isClear());
    //pressed with a working NO contact: NO0 NC1
    TEST_ASSERT_TRUE(estop.update(false, true, 5010) == EstopState::PRESSED);
    //both closed: impossible for the switch, pressed at once, a wiring fault after 100 ms
    TEST_ASSERT_TRUE(estop.update(false, false, 6000) == EstopState::PRESSED);
    TEST_ASSERT_TRUE(estop.update(false, false, 6100) == EstopState::PRESSED);
    TEST_ASSERT_TRUE(estop.update(false, false, 6101) == EstopState::WIRING);
    TEST_ASSERT_FALSE(estop.isClear());
    //a short glitch through both-closed is no wiring fault
    TEST_ASSERT_TRUE(estop.update(true, false, 6200) == EstopState::CLEAR);
    TEST_ASSERT_TRUE(estop.update(false, false, 6210) == EstopState::PRESSED);
    TEST_ASSERT_TRUE(estop.update(true, false, 6250) == EstopState::CLEAR);
}

//——— BatteryMonitor ———

void test_battery_thresholds_with_hysteresis(){
    BatteryMonitor battery;
    TEST_ASSERT_TRUE(battery.update(3.8f) == BatteryState::OK);
    TEST_ASSERT_TRUE(battery.update(3.49f) == BatteryState::LOW_CHARGE);
    TEST_ASSERT_TRUE(battery.update(3.52f) == BatteryState::LOW_CHARGE);
    TEST_ASSERT_TRUE(battery.update(3.29f) == BatteryState::CRITICAL);
    TEST_ASSERT_TRUE(battery.update(3.32f) == BatteryState::CRITICAL);
    TEST_ASSERT_TRUE(battery.update(3.36f) == BatteryState::LOW_CHARGE);
    TEST_ASSERT_TRUE(battery.update(3.56f) == BatteryState::OK);
}

//——— RemoteSettings ———

void test_settings_validated_on_load(){
    RemoteSettings blank = RemoteSettings::fromStored(0xFF, 0xFF, 0xFF);
    TEST_ASSERT_EQUAL_UINT8(15, blank.channel);
    TEST_ASSERT_EQUAL_UINT8(1, blank.contrast);
    TEST_ASSERT_EQUAL_UINT8(2, blank.dimming);
    TEST_ASSERT_EQUAL_HEX8(0x2F, RemoteSettings::contrastValue(blank.contrast));
    RemoteSettings stored = RemoteSettings::fromStored(3, 2, 0);
    TEST_ASSERT_EQUAL_UINT8(3, stored.channel);
    TEST_ASSERT_EQUAL_UINT8(2, stored.contrast);
    TEST_ASSERT_EQUAL_UINT8(0, stored.dimming);
    RemoteSettings bad = RemoteSettings::fromStored(16, 4, 3);
    TEST_ASSERT_TRUE(bad == RemoteSettings());
}

//——— SettingsMenu ———

void test_menu_changes_are_pending_until_save_held(){
    RemoteSettings saved = RemoteSettings::fromStored(15, 1, 2);
    SettingsMenu menu;
    menu.open(saved);
    TEST_ASSERT_TRUE(menu.getItem() == SettingsMenu::Item::CHANNEL);
    menu.update(0, 1, 0);
    TEST_ASSERT_EQUAL_UINT8(0, menu.getPending().channel);   //wraps 15 to 0
    menu.update(0, -1, 0);
    menu.update(0, -1, 0);
    TEST_ASSERT_EQUAL_UINT8(14, menu.getPending().channel);
    menu.update(1, 0, 0);
    TEST_ASSERT_TRUE(menu.getItem() == SettingsMenu::Item::CONTRAST);
    menu.update(0, 1, 0);
    menu.update(0, 1, 0);
    menu.update(0, 1, 0);
    TEST_ASSERT_EQUAL_UINT8(3, menu.getPending().contrast);  //clamped
    TEST_ASSERT_EQUAL_UINT8(15, saved.channel);
    menu.update(1, 0, 0);
    menu.update(1, 0, 0);
    TEST_ASSERT_TRUE(menu.getItem() == SettingsMenu::Item::STICKS);
    TEST_ASSERT_TRUE(menu.update(0, 1, 0) == SettingsMenu::Event::CALIBRATE);
    TEST_ASSERT_TRUE(menu.isOpen());
    menu.update(1, 0, 0);
    TEST_ASSERT_TRUE(menu.getItem() == SettingsMenu::Item::SAVE);
    TEST_ASSERT_TRUE(menu.update(0, 1, 0) == SettingsMenu::Event::NONE);
    TEST_ASSERT_TRUE(menu.update(0, 0, 999) == SettingsMenu::Event::NONE);
    TEST_ASSERT_TRUE(menu.isOpen());
    TEST_ASSERT_TRUE(menu.update(0, 0, 1000) == SettingsMenu::Event::SAVED);
    TEST_ASSERT_FALSE(menu.isOpen());
    TEST_ASSERT_EQUAL_UINT8(14, menu.getPending().channel);
}

void test_menu_exit_and_item_bounds(){
    SettingsMenu menu;
    menu.open(RemoteSettings());
    menu.update(-1, 0, 0);
    TEST_ASSERT_TRUE(menu.getItem() == SettingsMenu::Item::CHANNEL);
    for(int i = 0; i < 10; i++) menu.update(1, 0, 0);
    TEST_ASSERT_TRUE(menu.getItem() == SettingsMenu::Item::EXIT);
    TEST_ASSERT_TRUE(menu.update(0, -1, 0) == SettingsMenu::Event::NONE);
    TEST_ASSERT_TRUE(menu.update(0, 1, 0) == SettingsMenu::Event::CLOSED);
    TEST_ASSERT_FALSE(menu.isOpen());
    TEST_ASSERT_TRUE(menu.update(0, 1, 5000) == SettingsMenu::Event::NONE);
}

//——— PageNavigator ———

void test_pages_cycle_and_settings_only_at_off(){
    PageNavigator pages;
    pages.update(true, false, false, false, 0);
    TEST_ASSERT_TRUE(pages.getPage() == Page::ABOUT);           //wraps backwards, no settings page
    pages.update(false, true, false, false, 10);
    TEST_ASSERT_TRUE(pages.getPage() == Page::STATUS);
    pages.update(true, false, true, false, 20);
    TEST_ASSERT_TRUE(pages.getPage() == Page::SETTINGS);
    pages.update(false, false, false, false, 30);               //switch left OFF
    TEST_ASSERT_TRUE(pages.getPage() == Page::STATUS);
}

void test_pages_return_to_status_after_10s(){
    PageNavigator pages;
    pages.update(false, true, false, false, 1000);
    TEST_ASSERT_TRUE(pages.getPage() == Page::LINK);
    pages.update(false, false, false, true, 9000);              //menu activity keeps it
    pages.update(false, false, false, false, 19000);
    TEST_ASSERT_TRUE(pages.getPage() == Page::LINK);
    pages.update(false, false, false, false, 19001);
    TEST_ASSERT_TRUE(pages.getPage() == Page::STATUS);
}

//——— StatusView ———

static StatusInputs linkUpEnabled(){
    StatusInputs in;
    in.b_linkUp = true;
    in.reportedState = uint8_t(RobotState::ENABLED);
    return in;
}

void test_status_priority(){
    StatusInputs in = linkUpEnabled();
    TEST_ASSERT_TRUE(Status::select(in).message == BigMessage::ENABLED);
    in.release = StickGuard::Release::LEFT;
    TEST_ASSERT_TRUE(Status::select(in).message == BigMessage::RELEASE_LEFT);
    in.battery = BatteryState::CRITICAL;
    TEST_ASSERT_TRUE(Status::select(in).message == BigMessage::BATTERY_CRITICAL);
    in.battery = BatteryState::LOW_CHARGE;
    TEST_ASSERT_TRUE(Status::select(in).message == BigMessage::RELEASE_LEFT);
    in.b_linkUp = false;
    StatusView view = Status::select(in);
    TEST_ASSERT_TRUE(view.message == BigMessage::NO_LINK);
    TEST_ASSERT_TRUE(view.line == StatusLine::ESTOP_NOT_CONNECTED);
    in.b_radioFault = true;
    in.radioFaultMillis = 1000;
    view = Status::select(in);
    TEST_ASSERT_TRUE(view.message == BigMessage::RADIO_FAULT);
    TEST_ASSERT_TRUE(view.line == StatusLine::RADIO_RETRY);
    in.radioFaultMillis = 30001;
    TEST_ASSERT_TRUE(Status::select(in).line == StatusLine::RESTART_REMOTE);
    in.estop = EstopState::PRESSED;
    view = Status::select(in);
    TEST_ASSERT_TRUE(view.message == BigMessage::ESTOP);
    TEST_ASSERT_TRUE(view.line == StatusLine::NOT_CONNECTED);
    in.estop = EstopState::WIRING;
    TEST_ASSERT_TRUE(Status::select(in).message == BigMessage::ESTOP_WIRING);
}

void test_status_estop_with_link_and_ext_estop(){
    StatusInputs in = linkUpEnabled();
    in.estop = EstopState::PRESSED;
    StatusView view = Status::select(in);
    TEST_ASSERT_TRUE(view.message == BigMessage::ESTOP);
    TEST_ASSERT_TRUE(view.line == StatusLine::NORMAL);
    //released, Stacato still E-stopped: own E-stop for 500 ms, then an E-stop elsewhere
    in.estop = EstopState::CLEAR;
    in.reportedState = uint8_t(RobotState::EMERGENCY_STOPPED);
    in.sinceEstopReleaseMillis = 499;
    TEST_ASSERT_TRUE(Status::select(in).message == BigMessage::ESTOP);
    in.sinceEstopReleaseMillis = 500;
    TEST_ASSERT_TRUE(Status::select(in).message == BigMessage::EXT_ESTOP);
    in.reportedState = uint8_t(RobotState::EMERGENCY_STOPPING);
    TEST_ASSERT_TRUE(Status::select(in).message == BigMessage::STOPPING);
    in.reportedState = 0;
    TEST_ASSERT_TRUE(Status::select(in).message == BigMessage::UNKNOWN);
}

void test_status_thank_you_and_texts(){
    StatusInputs in = linkUpEnabled();
    in.b_thankYou = true;
    TEST_ASSERT_TRUE(Status::select(in).line == StatusLine::THANK_YOU);
    TEST_ASSERT_TRUE(Status::isAlarm(BigMessage::EXT_ESTOP));
    TEST_ASSERT_FALSE(Status::isAlarm(BigMessage::ENABLED));
    TEST_ASSERT_FALSE(Status::isAlarm(BigMessage::RELEASE_BOTH));
    TEST_ASSERT_EQUAL_STRING("E-STOP WIRING", Status::text(BigMessage::ESTOP_WIRING));
    TEST_ASSERT_EQUAL_STRING("E-STOP NOT CONNECTED", Status::text(StatusLine::ESTOP_NOT_CONNECTED));
    TEST_ASSERT_EQUAL_STRING("OFF", Status::speedText(0));
    TEST_ASSERT_EQUAL_STRING("SLOW", Status::speedText(1));
    TEST_ASSERT_EQUAL_STRING("FAST", Status::speedText(2));
    TEST_ASSERT_EQUAL_STRING("OFF", Status::speedText(3));
}

//——— Dimmer ———

void test_dimmer(){
    Dimmer dimmer;
    TEST_ASSERT_EQUAL_HEX8(0x2F, dimmer.update(1, 1, true, false, 0));
    TEST_ASSERT_EQUAL_HEX8(0x2F, dimmer.update(1, 1, false, false, 30000));
    TEST_ASSERT_EQUAL_HEX8(Dimmer::dimmedValue, dimmer.update(1, 1, false, false, 30001));
    TEST_ASSERT_TRUE(dimmer.isDimmed());
    //an alarm wakes the display at the set contrast and keeps it from dimming
    TEST_ASSERT_EQUAL_HEX8(0x10, dimmer.update(0, 1, false, true, 40000));
    TEST_ASSERT_FALSE(dimmer.isDimmed());
    TEST_ASSERT_EQUAL_HEX8(0x10, dimmer.update(0, 1, false, true, 200000));
    TEST_ASSERT_EQUAL_HEX8(0x10, dimmer.update(0, 1, false, false, 200001));
    TEST_ASSERT_EQUAL_HEX8(0x10, dimmer.update(0, 1, true, false, 229000));
    //dimming off never dims
    TEST_ASSERT_EQUAL_HEX8(0x10, dimmer.update(0, 0, false, false, 10000000));
}

//——— ResetHistory ———

void test_boot_loop_needs_three_resets_within_60s(){
    ResetHistory history;
    TEST_ASSERT_FALSE(history.onBoot(ResetCause::WATCHDOG, 1000));
    TEST_ASSERT_FALSE(history.onBoot(ResetCause::CRASH, 1030));
    TEST_ASSERT_TRUE(history.onBoot(ResetCause::WATCHDOG, 1060));
    TEST_ASSERT_EQUAL_UINT8(3, history.count);
    TEST_ASSERT_FALSE(history.onBoot(ResetCause::WATCHDOG, 1200));
    TEST_ASSERT_FALSE(history.onBoot(ResetCause::POWER_ON, 1201));
    TEST_ASSERT_EQUAL_UINT8(0, history.count);
    TEST_ASSERT_FALSE(history.onBoot(ResetCause::WATCHDOG, 1210));
    TEST_ASSERT_FALSE(history.onBoot(ResetCause::WATCHDOG, 1220));
}

void test_reset_history_bytes(){
    ResetHistory history;
    history.onBoot(ResetCause::CRASH, 123456);
    uint8_t bytes[ResetHistory::size];
    history.toBytes(bytes);
    ResetHistory read = ResetHistory::fromBytes(bytes, 123500);
    TEST_ASSERT_EQUAL_UINT32(123456, read.times[0]);
    TEST_ASSERT_EQUAL_UINT8(1, read.count);
    //RTC went back (power loss): times in the future are dropped
    read = ResetHistory::fromBytes(bytes, 100);
    TEST_ASSERT_EQUAL_UINT32(0, read.times[0]);
    uint8_t blank[ResetHistory::size];
    memset(blank, 0xFF, sizeof(blank));
    read = ResetHistory::fromBytes(blank, 5000);
    TEST_ASSERT_EQUAL_UINT32(0, read.times[2]);
    TEST_ASSERT_EQUAL_UINT8(0, read.count);
}

//——— Uplink frame ———

void test_uplink_stick_bytes_as_before(){
    TEST_ASSERT_EQUAL_UINT8(127, Uplink::stickToByte(0.0f));
    TEST_ASSERT_EQUAL_UINT8(255, Uplink::stickToByte(1.0f));
    TEST_ASSERT_EQUAL_UINT8(0, Uplink::stickToByte(-1.0f));
    TEST_ASSERT_EQUAL_UINT8(191, Uplink::stickToByte(0.5f));
    TEST_ASSERT_EQUAL_UINT8(255, Uplink::stickToByte(2.0f));
}

void test_uplink_frame_layout_and_guards(){
    UplinkControls controls;
    controls.b_enable = true;
    controls.speedState = 2;
    controls.b_direction = true;
    controls.b_rightStickPressed = true;
    controls.leftX = 1.0f;
    controls.rightY = -1.0f;
    uint8_t frame[8];
    Uplink::encode(controls, false, 0x13, 0xBEEF, frame);
    uint16_t word = frame[0] | frame[1] << 8;
    TEST_ASSERT_EQUAL_HEX16(0x7, word & 0x7);
    TEST_ASSERT_TRUE(word & 0x10);
    TEST_ASSERT_FALSE(word & 0x8);
    TEST_ASSERT_EQUAL_UINT16(2, (word >> 5) & 0x3);
    TEST_ASSERT_TRUE(word & 0x80);
    TEST_ASSERT_EQUAL_HEX16(0, word & 0x300);    //pushbuttons never sent
    TEST_ASSERT_TRUE(word & 0x800);
    TEST_ASSERT_EQUAL_UINT16(0x3, word >> 12);
    TEST_ASSERT_EQUAL_UINT8(255, frame[2]);
    TEST_ASSERT_EQUAL_UINT8(127, frame[3]);
    TEST_ASSERT_EQUAL_UINT8(0, frame[5]);
    TEST_ASSERT_EQUAL_HEX8(0xEF, frame[6]);
    TEST_ASSERT_EQUAL_HEX8(0xBE, frame[7]);
    //guard: sticks neutral, everything else as it is
    Uplink::encode(controls, true, 0x13, 0xBEEF, frame);
    for(int i = 2; i < 6; i++) TEST_ASSERT_EQUAL_UINT8(127, frame[i]);
    TEST_ASSERT_EQUAL_HEX16(word, frame[0] | frame[1] << 8);
}

//——— LinkMonitor ———

void test_link_loss_and_bar(){
    LinkMonitor link;
    for(int i = 0; i < 20; i++) link.onSent();
    for(int i = 0; i < 19; i++) link.onReply();
    link.update(999);
    TEST_ASSERT_EQUAL_UINT8(100, link.lossPercent());    //nothing measured yet
    link.update(1000);
    TEST_ASSERT_EQUAL_UINT8(5, link.lossPercent());
    link.update(2000);
    TEST_ASSERT_EQUAL_UINT8(100, link.lossPercent());    //nothing sent in that second
    TEST_ASSERT_EQUAL_INT16(-80, LinkMonitor::weaker(-62, -80));
    TEST_ASSERT_EQUAL_FLOAT(0.0f, LinkMonitor::barFraction(-120));
    TEST_ASSERT_EQUAL_FLOAT(0.5f, LinkMonitor::barFraction(-70));
    TEST_ASSERT_EQUAL_FLOAT(1.0f, LinkMonitor::barFraction(-10));
}

//——— LEDs ———

void test_led_patterns(){
    LedInputs in;
    in.b_linkUp = true;
    in.reportedState = uint8_t(RobotState::ENABLED);
    LedOutput out = Leds::pattern(in, 0);
    TEST_ASSERT_FALSE(out.b_left);
    TEST_ASSERT_TRUE(out.b_right);
    in.reportedState = 0;
    out = Leds::pattern(in, 0);
    TEST_ASSERT_FALSE(out.b_left || out.b_right);           //unknown: both off
    in.b_finder = true;
    out = Leds::pattern(in, 10);
    TEST_ASSERT_TRUE(out.b_left && !out.b_right);
    in.b_localEstop = true;
    out = Leds::pattern(in, 10);
    TEST_ASSERT_TRUE(out.b_left && out.b_right);
    in.b_radioFault = true;
    out = Leds::pattern(in, 600);
    TEST_ASSERT_FALSE(out.b_left || out.b_right);           //together, off half of each second
    out = Leds::pattern(in, 100);
    TEST_ASSERT_TRUE(out.b_left && out.b_right);
}

//——— Stick calibration [H15] ———

//the former Joystick2Axis::readingToValue, fixed range 0..1023
static float formerReadingToValue(uint16_t reading, int zeroOffset, bool b_invert){
    float minLevel = 0.01;
    float maxLevel = 0.99;
    float normalized;
    if(reading >= zeroOffset) normalized = (float(reading) - (float)zeroOffset) * ((float)1.0 - (float)0.0) / ((float)1023 - (float)zeroOffset) + (float)0.0;
    else normalized = (float(reading) - (float)0) * ((float)0.0 - (float)-1.0) / ((float)zeroOffset - (float)0) + (float)-1.0;
    bool sign = normalized > 0.0;
    float absolute = sign ? normalized : -normalized;
    float output;
    if(absolute > maxLevel) output = 1.0;
    else if(absolute < minLevel) return 0.0;
    else {
        output = (absolute - minLevel) * ((float)1.0 - (float)0.0) / (maxLevel - minLevel) + (float)0.0;
        if(output > 1.0) output = 1.0;
        else if(output < 0.0) output = 0.0;
    }
    if(!sign) output = -output;
    if(b_invert) output = -output;
    return output;
}

void test_uncalibrated_output_identical_to_before(){
    AxisCalibration uncalibrated;
    const int zeros[] = {100, 400, 500, 511, 512, 530, 700, 900};
    for(int zero : zeros){
        for(int reading = 0; reading <= 1023; reading++){
            for(int invert = 0; invert < 2; invert++){
                float before = formerReadingToValue(reading, zero, invert);
                float now = StickMath::normalize(reading, zero, uncalibrated.low, uncalibrated.high, invert);
                TEST_ASSERT_EQUAL_FLOAT(before, now);
            }
        }
    }
}

void test_calibrated_range_and_deadband(){
    //a stick reaching only 200..800, resting at 510
    TEST_ASSERT_EQUAL_FLOAT(1.0f, StickMath::normalize(800, 510, 200, 800, false));
    TEST_ASSERT_EQUAL_FLOAT(-1.0f, StickMath::normalize(200, 510, 200, 800, false));
    TEST_ASSERT_EQUAL_FLOAT(1.0f, StickMath::normalize(900, 510, 200, 800, false));   //beyond: saturated
    float half = StickMath::normalize(655, 510, 200, 800, false);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.5f, half);
    //deadband kept: within 1 % of the range around the zero reads exactly 0
    TEST_ASSERT_EQUAL_FLOAT(0.0f, StickMath::normalize(512, 510, 200, 800, false));
    TEST_ASSERT_EQUAL_FLOAT(0.0f, StickMath::normalize(508, 510, 200, 800, false));
    TEST_ASSERT_TRUE(StickMath::normalize(520, 510, 200, 800, false) > 0.0f);
    TEST_ASSERT_TRUE(StickMath::normalize(655, 510, 200, 800, true) < 0.0f);
    //zero outside the calibrated range (odd boot): no division by zero
    TEST_ASSERT_EQUAL_FLOAT(1.0f, StickMath::normalize(900, 850, 200, 800, false));
}

void test_calibration_validity_and_bytes(){
    AxisCalibration axis;
    TEST_ASSERT_TRUE(axis.isValid());
    axis.low = 300;
    TEST_ASSERT_FALSE(axis.isValid());                  //only 212 below the centre
    StickCalibration calibration;
    calibration.axes[1].low = 100;
    calibration.axes[1].centre = 450;
    calibration.axes[1].high = 900;
    uint8_t bytes[StickCalibration::size];
    calibration.toBytes(bytes);
    StickCalibration read = StickCalibration::fromBytes(bytes);
    TEST_ASSERT_EQUAL_UINT16(450, read.axes[1].centre);
    TEST_ASSERT_EQUAL_UINT16(900, read.axes[1].high);
    bytes[1 + 1 * 6 + 2] = 0x10;                        //axis 1 centre 0x10C2: invalid, back to default
    read = StickCalibration::fromBytes(bytes);
    TEST_ASSERT_EQUAL_UINT16(512, read.axes[1].centre);
    uint8_t blank[StickCalibration::size];
    memset(blank, 0xFF, sizeof(blank));
    read = StickCalibration::fromBytes(blank);
    TEST_ASSERT_EQUAL_UINT16(1023, read.axes[3].high);
    //released at boot: near the calibrated rest position, not mid scale
    AxisCalibration offCentre;
    offCentre.low = 150;
    offCentre.centre = 650;
    offCentre.high = 1000;
    TEST_ASSERT_TRUE(StickMath::isNearCentre(660, offCentre, 0.1f));
    TEST_ASSERT_FALSE(StickMath::isNearCentre(512, offCentre, 0.1f));
}

void test_calibrator_rest_then_circle(){
    StickCalibrator calibrator;
    calibrator.start(0);
    uint16_t raw[4] = {500, 520, 505, 515};
    //a stick still moving restarts the rest window
    for(uint32_t t = 0; t < 600; t += 20) calibrator.update(raw, t);
    raw[0] = 600;
    calibrator.update(raw, 600);
    raw[0] = 500;
    for(uint32_t t = 620; t < 1300; t += 20) calibrator.update(raw, t);
    TEST_ASSERT_TRUE(calibrator.getStep() == StickCalibrator::Step::RELEASE);
    calibrator.update(raw, 1340);
    TEST_ASSERT_TRUE(calibrator.getStep() == StickCalibrator::Step::CIRCLE);
    TEST_ASSERT_EQUAL_UINT16(520, calibrator.getResult().axes[1].centre);
    TEST_ASSERT_FALSE(calibrator.isComplete());
    //circle: every axis to both ends
    for(int i = 0; i < 4; i++){
        uint16_t low[4] = {raw[0], raw[1], raw[2], raw[3]};
        uint16_t high[4] = {raw[0], raw[1], raw[2], raw[3]};
        low[i] = 60;
        high[i] = 980;
        calibrator.update(low, 2000);
        calibrator.update(high, 2020);
    }
    TEST_ASSERT_TRUE(calibrator.isComplete());
    TEST_ASSERT_EQUAL_UINT16(60, calibrator.getResult().axes[2].low);
    TEST_ASSERT_EQUAL_UINT16(980, calibrator.getResult().axes[2].high);
}

int main(){
    UNITY_BEGIN();
    RUN_TEST(test_stickguard_holds_from_boot_until_both_centred);
    RUN_TEST(test_stickguard_holds_in_menu_and_after_until_centred);
    RUN_TEST(test_axisstepper_steps_rearms_and_repeats);
    RUN_TEST(test_axisstepper_block_until_released);
    RUN_TEST(test_estop_clear_pressed_and_wiring);
    RUN_TEST(test_battery_thresholds_with_hysteresis);
    RUN_TEST(test_settings_validated_on_load);
    RUN_TEST(test_menu_changes_are_pending_until_save_held);
    RUN_TEST(test_menu_exit_and_item_bounds);
    RUN_TEST(test_pages_cycle_and_settings_only_at_off);
    RUN_TEST(test_pages_return_to_status_after_10s);
    RUN_TEST(test_status_priority);
    RUN_TEST(test_status_estop_with_link_and_ext_estop);
    RUN_TEST(test_status_thank_you_and_texts);
    RUN_TEST(test_dimmer);
    RUN_TEST(test_boot_loop_needs_three_resets_within_60s);
    RUN_TEST(test_reset_history_bytes);
    RUN_TEST(test_uplink_stick_bytes_as_before);
    RUN_TEST(test_uplink_frame_layout_and_guards);
    RUN_TEST(test_link_loss_and_bar);
    RUN_TEST(test_led_patterns);
    RUN_TEST(test_uncalibrated_output_identical_to_before);
    RUN_TEST(test_calibrated_range_and_deadband);
    RUN_TEST(test_calibration_validity_and_bytes);
    RUN_TEST(test_calibrator_rest_then_circle);
    return UNITY_END();
}
