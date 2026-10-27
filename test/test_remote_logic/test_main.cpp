//Host tests of the remote's decision logic [D-147]: pio test -e native
#include <unity.h>
#include <string.h>

#include <StickGuard.h>
#include <AxisStepper.h>
#include <EstopContacts.h>
#include <BatteryMonitor.h>
#include <RemoteSettings.h>
#include <SettingsMenu.h>
#include <SoundPlayer.h>
#include <RemoteSounds.h>
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
    TEST_ASSERT_TRUE(menu.getItem() == SettingsMenu::Item::SOUND);
    TEST_ASSERT_EQUAL_UINT8(uint8_t(RemoteSounds::Level::ALARMS), menu.getPending().sound);    //the default
    menu.update(0, 1, 0);
    menu.update(0, 1, 0);
    TEST_ASSERT_EQUAL_UINT8(uint8_t(RemoteSounds::Level::OFF), menu.getPending().sound);       //clamped
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

//——— Synth and RemoteSounds ———

void test_synth_rests_ramps_and_sounds(){
    Synth synth;
    TEST_ASSERT_TRUE(synth.isIdle());
    TEST_ASSERT_EQUAL_UINT8(0, synth.nextSample());
    synth.play(0, Synth::Wave::SINE, 1000.0f, 1.0f, 0.1f);
    //the first 20 ms ramp to mid-scale, the note waiting
    uint8_t last = 0;
    for(uint32_t i = 0; i < Synth::sampleRate / 50; i++){
        uint8_t duty = synth.nextSample();
        TEST_ASSERT_TRUE(duty >= last);
        last = duty;
    }
    TEST_ASSERT_UINT8_WITHIN(1, 128, last);
    //then 100 ms of 1 kHz: about 100 rising crossings of mid-scale, the full swing
    int crossings = 0;
    uint8_t lowest = 255, highest = 0, previous = 128;
    for(uint32_t i = 0; i < Synth::sampleRate / 10; i++){
        uint8_t duty = synth.nextSample();
        if(previous < 128 && duty >= 128) crossings++;
        if(duty < lowest) lowest = duty;
        if(duty > highest) highest = duty;
        previous = duty;
    }
    TEST_ASSERT_INT_WITHIN(2, 100, crossings);
    TEST_ASSERT_TRUE(lowest < 5 && highest > 250);
    //released, held 0.3 s, back to rest
    for(uint32_t i = 0; i < Synth::sampleRate / 2; i++) synth.nextSample();
    TEST_ASSERT_TRUE(synth.isIdle());
    TEST_ASSERT_EQUAL_UINT8(0, synth.nextSample());
}

void test_synth_legs_mirror_and_rest_together(){
    Synth synth;
    synth.play(0, Synth::Wave::SQUARE, 440.0f, 1.0f, 0.1f);
    //the ramp rises on both legs alike: nothing across the disc
    for(uint32_t i = 0; i < Synth::sampleRate / 50; i++){
        Synth::Pair p = synth.nextPair();
        TEST_ASSERT_EQUAL_UINT8(p.a, p.b);
    }
    //then mirrored around mid-scale; the smoothed square stays within the range
    for(uint32_t i = 0; i < Synth::sampleRate / 20; i++){
        Synth::Pair p = synth.nextPair();
        TEST_ASSERT_INT_WITHIN(1, 255, int(p.a) + int(p.b));
    }
}

void test_synth_plays_a_clip_and_ends(){
    //a clip at half the synth's rate (each sample read twice), long enough for the 3 ms attack, then released
    static int8_t clip[256];
    for(int i = 0; i < 256; i++) clip[i] = (i / 8) % 2 ? 127 : -127;
    Synth synth;
    synth.playClip(0, clip, 256, Synth::sampleRate / 2, 1.0f);
    for(uint32_t i = 0; i < Synth::sampleRate / 50; i++) synth.nextPair();     //the 20 ms ramp to mid-scale
    uint8_t highest = 0;
    for(int i = 0; i < 400; i++){
        Synth::Pair p = synth.nextPair();
        if(p.a > highest) highest = p.a;
    }
    TEST_ASSERT_TRUE(highest > 240);        //the full swing came through
    for(uint32_t i = 0; i < Synth::sampleRate / 2; i++) synth.nextPair();
    TEST_ASSERT_TRUE(synth.isIdle());
}

void test_synth_note_waits_swells_and_decays(){
    //a note 50 ms late with a 100 ms swell: silent (at rest) while it waits, rising through the swell, then falling
    Synth synth;
    synth.play(0, Synth::Wave::SQUARE, 1000.0f, 1.0f, 0.5f, 0.1f, 0.1f, 0.05f);
    auto swing = [&](float seconds){
        uint8_t lowest = 255, highest = 0;
        for(uint32_t i = 0; i < uint32_t(seconds * Synth::sampleRate); i++){
            Synth::Pair p = synth.nextPair();
            if(p.a < lowest) lowest = p.a;
            if(p.a > highest) highest = p.a;
        }
        return int(highest) - int(lowest);
    };
    //the 20 ms rest level ramp comes first (the delay counts after it)
    TEST_ASSERT_TRUE(swing(0.02f) < 140);       //the ramp only (both legs alike, no tone)
    TEST_ASSERT_TRUE(swing(0.045f) < 3);        //still waiting
    int early = swing(0.03f);                   //the swell under way (a quarter at most)
    int peak = swing(0.1f);                     //reaching the top
    int later = swing(0.1f);                    //a time constant of decay
    TEST_ASSERT_TRUE(early < peak);
    TEST_ASSERT_TRUE(peak > 240);
    TEST_ASSERT_TRUE(later < peak);
}

struct RecordedSound : public SoundOutput{
    int tones = 0, squaresOff = 0, plays = 0, releases = 0;
    uint16_t lastHertz = 0;
    uint8_t lastVoice = 255;
    uint16_t frontHertz = 0;        //the last note on the front layer's voices
    void tone(uint16_t hertz) override { tones++; lastHertz = hertz; }
    void duet(uint16_t hertz, uint16_t) override { tones++; lastHertz = hertz; }
    void quietSquares() override { squaresOff++; }
    void play(uint8_t voice, Synth::Wave, float hertz, float, float, float) override {
        plays++;
        lastHertz = uint16_t(hertz);
        lastVoice = voice;
        if(voice >= 2) frontHertz = uint16_t(hertz);
    }
    void release(uint8_t) override { releases++; }
    void playClip(uint8_t, const SoundClip&, float) override { plays++; }
    void playScore(const SynthScoreNote*, uint8_t count) override { plays += count; }
};

void test_sounds_follow_the_setting(){
    using namespace RemoteSounds;
    const uint8_t all = uint8_t(Level::ALL), alarms = uint8_t(Level::ALARMS), off = uint8_t(Level::OFF);
    TEST_ASSERT_TRUE(isAllowed(Sound::CLICK, all));
    TEST_ASSERT_FALSE(isAllowed(Sound::CLICK, alarms));
    TEST_ASSERT_TRUE(isAllowed(Sound::WARNING, alarms));
    TEST_ASSERT_TRUE(isAllowed(Sound::STATUS_ENABLED, alarms));
    TEST_ASSERT_TRUE(isAllowed(Sound::STATUS_ESTOP, alarms));
    TEST_ASSERT_FALSE(isAllowed(Sound::SEARCHING, alarms));
    TEST_ASSERT_FALSE(isAllowed(Sound::WARNING, off));
    TEST_ASSERT_TRUE(gamesAllowed(alarms));
    TEST_ASSERT_FALSE(gamesAllowed(off));
    for(uint8_t i = 1; i < uint8_t(Sound::COUNT); i++){
        SoundCue c = cue(Sound(i));
        TEST_ASSERT_NOT_NULL(c.notes);
        TEST_ASSERT_TRUE(c.noteCount > 0);
        TEST_ASSERT_EQUAL_UINT8(isAlarm(Sound(i)) ? SoundPriority::ALARM : SoundPriority::INTERFACE, c.priority);
    }
    TEST_ASSERT_EQUAL_UINT8(alarms, RemoteSettings::fromStored(0, 0, 0).sound);
    TEST_ASSERT_EQUAL_UINT8(alarms, RemoteSettings::fromStored(0, 0, 0, 7).sound);
    TEST_ASSERT_EQUAL_UINT8(off, RemoteSettings::fromStored(0, 0, 0, off).sound);
}

void test_status_changes_sound_once_and_not_at_power_on(){
    using namespace RemoteSounds;
    using M = BigMessage;
    TEST_ASSERT_TRUE(forStatusChange(M::UNKNOWN, M::ESTOP) == Sound::NONE);
    TEST_ASSERT_TRUE(forStatusChange(M::NO_LINK, M::DISABLED) == Sound::STATUS_LINK_FOUND);
    TEST_ASSERT_TRUE(forStatusChange(M::RELEASE_BOTH, M::DISABLED) == Sound::NONE);
    TEST_ASSERT_TRUE(forStatusChange(M::DISABLED, M::ENABLING) == Sound::STATUS_ENABLING);
    TEST_ASSERT_TRUE(forStatusChange(M::ENABLING, M::ENABLED) == Sound::STATUS_ENABLED);
    TEST_ASSERT_TRUE(forStatusChange(M::ENABLED, M::DISABLING) == Sound::STATUS_DISABLING);
    TEST_ASSERT_TRUE(forStatusChange(M::DISABLING, M::DISABLED) == Sound::STATUS_DISABLED);
    TEST_ASSERT_TRUE(forStatusChange(M::ENABLED, M::ESTOP) == Sound::STATUS_ESTOP);
    TEST_ASSERT_TRUE(forStatusChange(M::ENABLED, M::STOPPING) == Sound::STATUS_ESTOP);
    TEST_ASSERT_TRUE(forStatusChange(M::STOPPING, M::EXT_ESTOP) == Sound::NONE);
    TEST_ASSERT_TRUE(forStatusChange(M::ESTOP, M::EXT_ESTOP) == Sound::NONE);        //still E-stopped for a moment
    TEST_ASSERT_TRUE(forStatusChange(M::EXT_ESTOP, M::DISABLED) == Sound::STATUS_ESTOP_RELEASED);
    TEST_ASSERT_TRUE(forStatusChange(M::DISABLED, M::NO_LINK) == Sound::WARNING);
    TEST_ASSERT_TRUE(forStatusChange(M::NO_LINK, M::RADIO_FAULT) == Sound::WARNING);
    TEST_ASSERT_TRUE(forSpeedChange(uint8_t(SpeedMode::LOW_SPEED), uint8_t(SpeedMode::NO_SPEED)) == Sound::NONE);
    TEST_ASSERT_TRUE(forSpeedChange(uint8_t(SpeedMode::NO_SPEED), uint8_t(SpeedMode::MEDIUM_SPEED)) == Sound::SPEED_SLOW);
    TEST_ASSERT_TRUE(forSpeedChange(uint8_t(SpeedMode::MEDIUM_SPEED), uint8_t(SpeedMode::HIGH_SPEED)) == Sound::SPEED_FAST);
    TEST_ASSERT_TRUE(forSpeedChange(uint8_t(SpeedMode::HIGH_SPEED), uint8_t(SpeedMode::LOW_SPEED)) == Sound::SPEED_OFF);
    TEST_ASSERT_TRUE(refusesEnable(M::ESTOP));
    TEST_ASSERT_FALSE(refusesEnable(M::DISABLED));
}

void test_player_alarms_are_not_cut_by_lower_priority(){
    SoundPlayer player;
    RecordedSound out;
    player.update(0, out);
    TEST_ASSERT_TRUE(player.play(RemoteSounds::cue(RemoteSounds::Sound::WARNING)));
    for(uint32_t t = 0; t < 2000; t += 5){
        if(t == 100) TEST_ASSERT_FALSE(player.play(RemoteSounds::cue(RemoteSounds::Sound::CLICK)));
        player.update(t, out);
    }
    TEST_ASSERT_EQUAL_INT(3, out.tones);        //three beeps, once
    TEST_ASSERT_EQUAL_INT(0, out.plays);
    TEST_ASSERT_FALSE(player.isPlaying(SoundLayer::FRONT));
    TEST_ASSERT_TRUE(player.play(RemoteSounds::cue(RemoteSounds::Sound::SAVED)));
    for(uint32_t t = 2000; t < 3000; t += 5) player.update(t, out);
    TEST_ASSERT_EQUAL_INT(5, out.tones);        //two rising squares
    TEST_ASSERT_EQUAL_UINT16(2093, out.lastHertz);
    TEST_ASSERT_TRUE(player.play(RemoteSounds::cue(RemoteSounds::Sound::REFUSED)));
    for(uint32_t t = 3000; t < 3500; t += 5) player.update(t, out);
    TEST_ASSERT_EQUAL_INT(2, out.plays);        //the buzz: two synth voices
    TEST_ASSERT_TRUE(out.lastVoice >= 2);       //the front layer's voices
}

void test_player_layers_loop_and_notes_made_at_run_time(){
    static const SoundNote tune[] = {
        {0, SoundNote::Kind::SYNTH, 0, Synth::Wave::TRIANGLE, 262, 100, 50, 0},
        {200, SoundNote::Kind::SYNTH, 1, Synth::Wave::TRIANGLE, 330, 100, 50, 0},
    };
    SoundPlayer player;
    RecordedSound out;
    player.update(0, out);
    TEST_ASSERT_TRUE(player.play(soundCue(tune, SoundPriority::GAME, 400), SoundLayer::BACK));
    for(uint32_t t = 0; t < 1000; t += 5) player.update(t, out);
    TEST_ASSERT_EQUAL_INT(5, out.plays);        //0, 200, 400, 600, 800: it loops
    TEST_ASSERT_TRUE(player.isPlaying(SoundLayer::BACK));
    TEST_ASSERT_TRUE(out.lastVoice < 2);        //the back layer's voices
    //an effect in front, its pitch from the game, while the music goes on
    SoundNote hit = {0, SoundNote::Kind::SYNTH, 0, Synth::Wave::NOISE, 4000, 60, 80, 20};
    TEST_ASSERT_TRUE(player.playNote(hit, SoundPriority::GAME));
    player.update(1000, out);
    TEST_ASSERT_EQUAL_UINT16(4000, out.frontHertz);
    TEST_ASSERT_TRUE(player.isPlaying(SoundLayer::BACK));
    //an alarm in front is not cut by the game's effects
    TEST_ASSERT_TRUE(player.play(RemoteSounds::cue(RemoteSounds::Sound::WARNING)));
    TEST_ASSERT_FALSE(player.playNote(hit, SoundPriority::GAME));
    player.stop(SoundLayer::BACK);
    player.update(1005, out);
    TEST_ASSERT_FALSE(player.isPlaying(SoundLayer::BACK));
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

void test_led_no_link_flashes_and_echoes(){
    LedInputs in;
    in.b_linkUp = false;
    LedOutput out = Leds::pattern(in, 4050);                //a flash: left, full
    TEST_ASSERT_TRUE(out.b_left && !out.b_right);
    TEST_ASSERT_EQUAL_FLOAT(1.0f, out.leftLevel);
    out = Leds::pattern(in, 4150);                          //then right
    TEST_ASSERT_TRUE(!out.b_left && out.b_right);
    out = Leds::pattern(in, 4320);                          //first echo, fainter
    TEST_ASSERT_TRUE(out.b_left && out.leftLevel < 1.0f);
    out = Leds::pattern(in, 4420);
    TEST_ASSERT_TRUE(out.b_right && out.rightLevel < 1.0f);
    float second = Leds::pattern(in, 4620).leftLevel;
    float third = Leds::pattern(in, 4920).leftLevel;
    TEST_ASSERT_TRUE(third < second && second < 0.2f);
    out = Leds::pattern(in, 4380);                          //dark between the pulses
    TEST_ASSERT_FALSE(out.b_left || out.b_right);
    out = Leds::pattern(in, 5500);                          //and for the rest of the 2 s
    TEST_ASSERT_FALSE(out.b_left || out.b_right);
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
    RUN_TEST(test_synth_rests_ramps_and_sounds);
    RUN_TEST(test_synth_legs_mirror_and_rest_together);
    RUN_TEST(test_synth_plays_a_clip_and_ends);
    RUN_TEST(test_synth_note_waits_swells_and_decays);
    RUN_TEST(test_sounds_follow_the_setting);
    RUN_TEST(test_status_changes_sound_once_and_not_at_power_on);
    RUN_TEST(test_player_alarms_are_not_cut_by_lower_priority);
    RUN_TEST(test_player_layers_loop_and_notes_made_at_run_time);
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
    RUN_TEST(test_led_no_link_flashes_and_echoes);
    RUN_TEST(test_uncalibrated_output_identical_to_before);
    RUN_TEST(test_calibrated_range_and_deadband);
    RUN_TEST(test_calibration_validity_and_bytes);
    RUN_TEST(test_calibrator_rest_then_circle);
    return UNITY_END();
}
