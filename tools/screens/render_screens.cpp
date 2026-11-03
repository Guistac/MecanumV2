//Draws the remote's screens for a set of example states into a 128 x 32 canvas and writes each as a PBM picture.
//Built and run by tools/screens/render.py on the Mac; the firmware draws the same code on the OLED.
#include <Adafruit_GFX.h>
#include <RemoteScreens.h>
#include <RemoteTransitions.h>
#include <RemoteSplash.h>
#include <stdio.h>
#include <math.h>
#include <string>
#include <vector>
#include <initializer_list>

struct Example{
    std::string name;
    ScreenModel model;
};

static ScreenModel base(){
    ScreenModel m;
    m.now = 100;
    m.channel = 3;
    m.b_linkUp = true;
    m.signal = 0.7f;
    m.rxCounter = 1;
    m.loss = 2;
    m.downDbm = -62;
    m.upDbm = -58;
    m.rttMs = 8.1f;
    m.robotVoltage = 56.3f;
    m.remotePercent = 80;
    m.remoteRestingVoltage = 3.98f;
    m.remoteVoltage = 3.95f;
    m.status = {BigMessage::ENABLED, StatusLine::NORMAL};
    m.speedState = 1;
    m.version = "7d818fd";
    m.uptimeSeconds = 5025;
    m.temperature = 34;
    m.radioFailures = 0;
    return m;
}

static void writePbm(GFXcanvas1& canvas, const std::string& path){
    FILE* f = fopen(path.c_str(), "w");
    fprintf(f, "P1\n128 32\n");
    for(int y = 0; y < 32; y++){
        for(int x = 0; x < 128; x++) fprintf(f, "%d ", canvas.getPixel(x, y) ? 1 : 0);
        fprintf(f, "\n");
    }
    fclose(f);
}

int main(int argc, char** argv){
    std::string outDir = argc > 1 ? argv[1] : ".";
    std::vector<Example> examples;
    auto add = [&](const char* name, ScreenModel m){ examples.push_back({name, m}); };

    { ScreenModel m = base(); add("01_enabled_slow", m); }
    { ScreenModel m = base(); m.status.message = BigMessage::DISABLED; m.speedState = 0; add("02_disabled_off", m); }
    { ScreenModel m = base(); m.status.message = BigMessage::ENABLING; m.speedState = 2; m.b_worldFrame = true; add("03_enabling_fast_world", m); }
    { ScreenModel m = base(); m.status = {BigMessage::ESTOP, StatusLine::NORMAL}; add("04_estop", m); }
    { ScreenModel m = base(); m.status = {BigMessage::ESTOP, StatusLine::NORMAL}; m.now = 600; add("05_estop_pulse", m); }
    { ScreenModel m = base(); m.b_linkUp = false; m.status = {BigMessage::NO_LINK, StatusLine::ESTOP_NOT_CONNECTED}; add("06_no_link", m); }
    { ScreenModel m = base(); m.b_linkUp = false; m.status = {BigMessage::RADIO_FAULT, StatusLine::RADIO_RETRY}; m.radioFailures = 3; add("07_radio_fault", m); }
    { ScreenModel m = base(); m.status = {BigMessage::ESTOP_WIRING, StatusLine::NORMAL}; add("08_estop_wiring", m); }
    { ScreenModel m = base(); m.status = {BigMessage::EXT_ESTOP, StatusLine::NORMAL}; m.b_recovery = true; add("09_ext_estop_rst", m); }
    { ScreenModel m = base(); m.status = {BigMessage::RELEASE_LEFT, StatusLine::NORMAL}; add("10_release_left", m); }
    { ScreenModel m = base(); m.status = {BigMessage::DISABLED, StatusLine::THANK_YOU}; m.speedState = 0; add("11_thank_you", m); }
    { ScreenModel m = base(); m.status = {BigMessage::DISABLED, StatusLine::THANK_YOU}; m.speedState = 1; m.b_worldFrame = true; add("11b_thank_you_slow_world", m); }
    { ScreenModel m = base(); m.status = {BigMessage::BATTERY_CRITICAL, StatusLine::NORMAL}; m.remotePercent = 3; m.remoteBattery = BatteryState::CRITICAL; add("12_battery_critical", m); }
    { ScreenModel m = base(); m.b_hot = true; m.b_safeMode = true; m.status.message = BigMessage::DISABLED; add("13_hot_safe", m); }
    { ScreenModel m = base(); m.b_direct = true; m.status.message = BigMessage::STOPPING; add("14_direct_stopping", m); }
    { ScreenModel m = base(); m.status.message = BigMessage::DISABLING; m.b_rebooted = true; add("15_disabling_rebooted", m); }
    { ScreenModel m = base(); m.b_direct = true; m.b_dropout = true; m.b_linkUp = false; m.status = {BigMessage::NO_LINK, StatusLine::ESTOP_NOT_CONNECTED}; add("16_direct_dropout", m); }
    { ScreenModel m = base(); m.status.message = BigMessage::ENABLED; m.remotePercent = 12; m.remoteBattery = BatteryState::LOW_CHARGE; m.speedState = 2; m.b_worldFrame = true; add("17_enabled_battery_low", m); }
    { ScreenModel m = base(); m.status = {BigMessage::RELEASE_BOTH, StatusLine::NORMAL}; m.speedState = 0; add("18_release_both", m); }
    { ScreenModel m = base(); m.status = {BigMessage::ESTOP, StatusLine::NOT_CONNECTED}; m.b_linkUp = false; add("19_estop_not_connected", m); }
    { ScreenModel m = base(); m.b_linkUp = false; m.speedState = 0; m.b_radioOff = true; m.status = {BigMessage::RADIO_OFF, StatusLine::NORMAL}; add("19b_radio_off", m); }
    //the stopwatch [D-160], bottom centre of the Status page
    auto timer = [](uint32_t setMillis, uint32_t elapsed, Stopwatch::State state){
        StopwatchView v;
        v.state = state;
        v.b_countdown = setMillis > 0;
        v.setMillis = setMillis;
        if(!v.b_countdown) v.shownMillis = elapsed;
        else if(elapsed >= setMillis){ v.b_overtime = true; v.shownMillis = elapsed - setMillis; v.remainingFraction = 0.0f; }
        else{ v.shownMillis = (setMillis - elapsed + 99) / 100 * 100; v.remainingFraction = float(setMillis - elapsed) / float(setMillis); }
        return v;
    };
    { ScreenModel m = base(); m.status.message = BigMessage::DISABLED; m.speedState = 0; add("01s1_timer_idle", m); }
    { ScreenModel m = base(); m.stopwatch = timer(0, 133400, Stopwatch::State::RUNNING); add("01s2_timer_counting", m); }
    { ScreenModel m = base(); m.stopwatch = timer(0, 725000, Stopwatch::State::RUNNING); m.b_worldFrame = true; add("01s3_timer_counting_12min_world", m); }
    { ScreenModel m = base(); m.stopwatch = timer(0, 133400, Stopwatch::State::STOPPED); m.now = 0; add("01s4_timer_stopped_shown", m); }
    { ScreenModel m = base(); m.stopwatch = timer(0, 133400, Stopwatch::State::STOPPED); m.now = 1000; add("01s5_timer_stopped_blank", m); }
    { ScreenModel m = base(); m.stopwatch = timer(150000, 0, Stopwatch::State::IDLE); m.status.message = BigMessage::DISABLED; m.speedState = 0; add("01s6_countdown_idle_2m30_full_bar", m); }
    { ScreenModel m = base(); m.stopwatch = timer(300000, 150000, Stopwatch::State::RUNNING); add("01s7_countdown_half", m); }
    { ScreenModel m = base(); m.stopwatch = timer(300000, 50000, Stopwatch::State::RUNNING); m.speedState = 2; m.b_worldFrame = true; add("01s8_countdown_fast_world", m); }
    { ScreenModel m = base(); m.stopwatch = timer(300000, 285000, Stopwatch::State::RUNNING); m.b_worldFrame = true; add("01s9_countdown_slow_world_nearly", m); }
    { ScreenModel m = base(); m.stopwatch = timer(300000, 150000, Stopwatch::State::RUNNING); m.status.message = BigMessage::DISABLING; add("01s9b_countdown_half_disabling", m); }
    { ScreenModel m = base(); m.stopwatch = timer(300000, 305000, Stopwatch::State::RUNNING); m.now = 0; add("01sa_overtime_pill_bar", m); }
    { ScreenModel m = base(); m.stopwatch = timer(300000, 305000, Stopwatch::State::RUNNING); m.now = 600; add("01sb_overtime_plain_no_bar", m); }
    { ScreenModel m = base(); m.stopwatch = timer(300000, 372300, Stopwatch::State::STOPPED); m.now = 600; m.b_worldFrame = true; add("01sb2_overtime_stopped_world", m); }
    { ScreenModel m = base(); m.stopwatch = timer(5999000, 0, Stopwatch::State::IDLE); m.b_worldFrame = true; add("01sc_countdown_99m59_world", m); }
    { ScreenModel m = base(); m.status = {BigMessage::RELEASE_RIGHT, StatusLine::NORMAL}; m.stopwatch = timer(0, 133400, Stopwatch::State::RUNNING); m.frame = 6; add("01sd_release_alive_dot", m); }
    { ScreenModel m = base(); m.status = {BigMessage::RELEASE_LEFT, StatusLine::NORMAL}; m.stopwatch = timer(300000, 150000, Stopwatch::State::RUNNING); m.b_worldFrame = true; add("01se_release_countdown_world", m); }
    { ScreenModel m = base(); m.status = {BigMessage::ESTOP, StatusLine::NORMAL}; m.stopwatch = timer(300000, 150000, Stopwatch::State::RUNNING); add("01se2_alarm_hides_bar", m); }
    { ScreenModel m = base(); m.status.message = BigMessage::DISABLED; m.speedState = 0; m.stopwatch.b_editing = true; add("01sf_edit_0m00", m); }
    { ScreenModel m = base(); m.status.message = BigMessage::DISABLED; m.speedState = 0; m.stopwatch = timer(150000, 0, Stopwatch::State::IDLE); m.stopwatch.b_editing = true; add("01sg_edit_2m30", m); }
    { ScreenModel m = base(); m.status.message = BigMessage::DISABLED; m.speedState = 0; m.b_worldFrame = true; m.stopwatch = timer(5999000, 0, Stopwatch::State::IDLE); m.stopwatch.b_editing = true; add("01sh_edit_99m59_world", m); }
    { ScreenModel m = base(); m.status.message = BigMessage::DISABLED; m.b_rebooted = true; m.stopwatch = timer(300000, 150000, Stopwatch::State::RUNNING); add("01si_marker_over_timer", m); }
    { ScreenModel m = base(); m.page = Page::LINK; add("20_page_link", m); }
    { ScreenModel m = base(); m.page = Page::LINK; m.b_linkUp = false; m.downDbm = -103; add("20b_page_link_none", m); }
    { ScreenModel m = base(); m.page = Page::RADIO; m.b_radioUp = true; m.recoveries = 2; m.txTimeouts = 1; add("21b_page_radio_up", m); }
    { ScreenModel m = base(); m.page = Page::ABOUT; m.b_safeMode = true; m.resetCount = 3; m.resetCause = ResetCause::WATCHDOG; add("24b_page_about_safe", m); }
    { ScreenModel m = base(); m.page = Page::RADIO; m.recoveries = 1; add("21_page_radio", m); }
    { ScreenModel m = base(); m.page = Page::BATTERY; add("22_page_battery", m); }
    { ScreenModel m = base(); m.page = Page::BATTERY; m.settings.pack = 3; m.robotVoltage = 60.1f; add("22b_page_battery_pack", m); }
    { ScreenModel m = base(); m.page = Page::INPUTS; m.sticks[0] = 0.5f; m.sticks[1] = -0.2f; m.b_enableButton = true; add("23_page_inputs", m); }
    { ScreenModel m = base(); m.page = Page::ABOUT; add("24_page_about", m); }
    { ScreenModel m = base(); m.speedState = uint8_t(SpeedMode::LOW_SPEED); m.status.message = BigMessage::DISABLED; m.b_sticksOffWarning = true; m.now = 0; add("01z_status_sticks_off_blink", m); }
    { ScreenModel m = base(); m.page = Page::ABOUT; m.b_arcadeReady = true; m.now = 0; add("24c_page_about_play_up", m); }
    { ScreenModel m = base(); m.page = Page::ABOUT; m.b_arcadeReady = true; m.now = 100; add("24d_page_about_play_pressed", m); }
    { ScreenModel m = base(); m.page = Page::ABOUT; m.arcadeLoading = 0.45f; m.now = 500; add("24e_arcade_loading", m); }
    { ScreenModel m = base(); m.page = Page::SETTINGS; m.pageCount = 10; m.speedState = 0; add("25_page_settings", m); }
    //the Motion page [D-161, D-163], a main page with the timer [D-160]: two columns on the text lines from row 9 (5x
    //live, 6x without motion data or editing; render.py)
    using VS = Reply::VehicleState;
    struct StateCase{ VS state; RobotState reported; const char* name; };
    //every vehicle state with the 3 bit state the receiver really sends beside it (src/RobotBoard/MavLink.h: BOOT,
    //POWEROFF, UNINIT, CRITICAL and anything unknown are E-stopping, EMERGENCY E-stopped; without Stacato the
    //receiver's own state, disabled at rest)
    const StateCase states[] = {
        {VS::NO_STACATO, RobotState::DISABLED, "no_stacato"},
        {VS::BUS_START, RobotState::EMERGENCY_STOPPING, "bus_start"},
        {VS::BUS_OFF, RobotState::EMERGENCY_STOPPING, "bus_off"},
        {VS::VEHICLE_OFF, RobotState::EMERGENCY_STOPPING, "vehicle_off"},
        {VS::DISABLED, RobotState::DISABLED, "disabled"},
        {VS::ENABLING, RobotState::ENABLING, "enabling"},
        {VS::ENABLED, RobotState::ENABLED, "enabled"},
        {VS::DISABLING, RobotState::EMERGENCY_STOPPING, "disabling_ramp"},
        {VS::E_STOPPING, RobotState::EMERGENCY_STOPPING, "e_stopping"},
        {VS::E_STOP, RobotState::EMERGENCY_STOPPED, "e_stop"},
        {VS::AXIS_FAULT, RobotState::EMERGENCY_STOPPED, "axis_fault"},
        {VS::UNKNOWN, RobotState::EMERGENCY_STOPPING, "unknown"},
    };
    //the Status message the board would select from the 3 bit state (StatusView)
    auto statusFor = [](RobotState reported, VS vehicle){
        StatusInputs in;
        in.b_linkUp = true;
        in.reportedState = uint8_t(reported);
        in.vehicle = vehicle;
        return Status::select(in);
    };
    auto motion = [](float vx, float vy, float vr, std::initializer_list<float> velocity, std::initializer_list<float> torque, uint16_t flags){
        Reply::MotionView v;
        v.b_velocityLive = true;
        v.vx = vx;
        v.vy = vy;
        v.vr = vr;
        int i = 0;
        for(float f : velocity) v.wheelVelocity[i++] = f;
        i = 0;
        for(float f : torque) v.wheelTorque[i++] = f;
        v.b_torqueKnown = true;
        v.wheelFlags = flags;
        v.b_flagsKnown = true;
        v.poseX = 12.345f;
        v.poseY = -3.2f;
        v.heading = 179.94f;
        v.vehicleState = VS::ENABLED;
        return v;
    };
    auto live = [&](VS state, RobotState reported, Reply::MotionView v){
        ScreenModel m = base();
        m.page = Page::MOTION;
        m.reportedState = uint8_t(reported);
        m.status = statusFor(reported, state);
        m.motionAvailability = Reply::MotionAvailability::LIVE;
        v.vehicleState = state;
        m.motion = v;
        return m;
    };
    Reply::MotionView driving = motion(0.312f, -0.048f, 0.1f, {0.45f, -0.2f, 1.0f, -1.0f}, {0.3f, -0.15f, 0.8f, -0.6f}, 0x00FF);
    Reply::MotionView atRest = motion(0.0f, 0.0f, 0.0f, {0, 0, 0, 0}, {0, 0, 0, 0}, 0x00F0);
    //Status: every vehicle state (0v_)
    for(const StateCase& c : states){
        ScreenModel m = live(c.state, c.reported, atRest);
        m.page = Page::STATUS;
        add((std::string("0v_status_") + c.name).c_str(), m);
    }
    { ScreenModel m = live(VS::AXIS_FAULT, RobotState::EMERGENCY_STOPPED, atRest); m.page = Page::STATUS; m.now = 600; add("0v_status_axis_fault_pulse", m); }
    { ScreenModel m = live(VS::E_STOPPING, RobotState::EMERGENCY_STOPPING, atRest); m.page = Page::STATUS; m.b_worldFrame = true; m.now = 200; add("0v_status_e_stopping_plain", m); }
    { ScreenModel m = live(VS::VEHICLE_OFF, RobotState::EMERGENCY_STOPPING, atRest); m.page = Page::STATUS; m.b_worldFrame = true; m.speedState = 2;
      m.stopwatch = timer(300000, 150000, Stopwatch::State::RUNNING); add("0v_status_vehicle_off_countdown_world", m); }
    { ScreenModel m = live(VS::NO_STACATO, RobotState::DISABLED, atRest); m.page = Page::STATUS; m.b_worldFrame = true; add("0v_status_no_stacato_world", m); }
    //without Stacato the receiver's own state starts E-stopped: still No Stacato, not the E-stop banner
    { ScreenModel m = live(VS::NO_STACATO, RobotState::EMERGENCY_STOPPED, atRest); m.page = Page::STATUS; add("0v_status_no_stacato_receiver_estopped", m); }
    //Motion: every vehicle state in the title (5v_)
    for(const StateCase& c : states){
        ScreenModel m = live(c.state, c.reported, driving);
        add((std::string("5v_motion_") + c.name).c_str(), m);
    }
    { ScreenModel m = base(); m.page = Page::MOTION; m.b_linkUp = false; m.motionAvailability = Reply::MotionAvailability::NO_LINK; add("60_motion_no_link", m); }
    { ScreenModel m = base(); m.page = Page::MOTION; m.reportedState = uint8_t(RobotState::DISABLED); m.motion.vehicleState = VS::NO_STACATO;
      m.motionAvailability = Reply::MotionAvailability::NO_DATA; add("61_motion_no_data", m); }
    { ScreenModel m = base(); m.page = Page::MOTION; m.reportedState = uint8_t(RobotState::DISABLED); m.motion.vehicleState = VS::DISABLED;
      m.motionAvailability = Reply::MotionAvailability::NO_DATA; m.stopwatch = timer(300000, 150000, Stopwatch::State::RUNNING); add("62_motion_no_data_countdown", m); }
    add("50_motion_driving", live(VS::ENABLED, RobotState::ENABLED, driving));
    add("51_motion_disabled_at_rest", live(VS::DISABLED, RobotState::DISABLED, atRest));
    add("52_motion_widest_values", live(VS::ENABLED, RobotState::ENABLED, [&]{ Reply::MotionView v = motion(-1.25f, -12.5f, -2.2f, {0, 0, 0, 0}, {0, 0, 0, 0}, 0x000F);
        v.poseX = -99.99f; v.poseY = -123.4f; v.heading = -179.9f; return v; }()));
    { Reply::MotionView v = motion(0.02f, 1.5f, 0.0f, {0.8f, 0.8f, 0.8f, 0.8f}, {0, 0, 0, 0}, 0); v.poseX = v.poseY = v.heading = NAN;
      add("53_motion_without_pose", live(VS::ENABLING, RobotState::ENABLING, v)); }
    { Reply::MotionView v = driving; v.heading = NAN; add("53b_motion_pose_without_heading", live(VS::ENABLED, RobotState::ENABLED, v)); }
    { Reply::MotionView v = driving; v.vr = 22.0f; v.heading = 0.04f; v.poseX = 0.0f; v.poseY = 0.004f; add("53c_motion_fast_turn_at_origin", live(VS::ENABLED, RobotState::ENABLED, v)); }
    //the timer on the Motion page [D-160]
    { ScreenModel m = live(VS::ENABLED, RobotState::ENABLED, driving); m.stopwatch = timer(0, 133400, Stopwatch::State::RUNNING); add("56_motion_timer_counting", m); }
    { ScreenModel m = live(VS::ENABLED, RobotState::ENABLED, driving); m.stopwatch = timer(0, 133400, Stopwatch::State::STOPPED); m.now = 1000; add("56b_motion_timer_stopped_blank", m); }
    { ScreenModel m = live(VS::ENABLED, RobotState::ENABLED, driving); m.stopwatch = timer(300000, 150000, Stopwatch::State::RUNNING); add("57_motion_countdown_half", m); }
    { ScreenModel m = live(VS::ENABLED, RobotState::ENABLED, driving); m.stopwatch = timer(300000, 285000, Stopwatch::State::RUNNING); add("57b_motion_countdown_nearly", m); }
    { ScreenModel m = live(VS::ENABLED, RobotState::ENABLED, driving); m.stopwatch = timer(300000, 305000, Stopwatch::State::RUNNING); m.now = 0; add("58_motion_overtime_shown", m); }
    { ScreenModel m = live(VS::ENABLED, RobotState::ENABLED, driving); m.stopwatch = timer(300000, 305000, Stopwatch::State::RUNNING); m.now = 600; add("58b_motion_overtime_blank", m); }
    { ScreenModel m = live(VS::DISABLED, RobotState::DISABLED, driving); m.stopwatch = timer(300000, 372300, Stopwatch::State::STOPPED); m.now = 600; add("58c_motion_overtime_stopped", m); }
    { ScreenModel m = live(VS::VEHICLE_OFF, RobotState::DISABLED, driving); m.stopwatch = timer(60000, 814500, Stopwatch::State::RUNNING); m.now = 0; add("58d_motion_widest_title", m); }
    { ScreenModel m = live(VS::E_STOPPING, RobotState::EMERGENCY_STOPPING, driving); m.stopwatch = timer(60000, 814500, Stopwatch::State::RUNNING); m.now = 0; add("58e_motion_widest_title_e_stopping", m); }
    { ScreenModel m = live(VS::E_STOP, RobotState::EMERGENCY_STOPPED, atRest); m.stopwatch = timer(300000, 150000, Stopwatch::State::RUNNING); add("59_motion_estop_countdown", m); }
    { ScreenModel m = live(VS::DISABLED, RobotState::DISABLED, driving); m.speedState = 0; m.stopwatch = timer(150000, 0, Stopwatch::State::IDLE); m.stopwatch.b_editing = true; add("63_motion_edit_2m30", m); }
    { ScreenModel m = live(VS::ENABLED, RobotState::ENABLED, driving); m.b_worldFrame = true;
      m.motion.poseX = 0.12f; m.motion.poseY = 2.5f; m.motion.heading = -12.3f; add("56c_motion_pose_zeroed", m); }
    { ScreenModel m = live(VS::DISABLED, RobotState::DISABLED, driving); m.speedState = 0; m.stopwatch.b_editing = true; add("63b_motion_edit_0m00", m); }
    //the Motors page [D-163] (7x; render.py): every wheel state, values, animation phases
    auto motors = [&](VS state, RobotState reported, Reply::MotionView v){
        ScreenModel m = live(state, reported, v);
        m.page = Page::MOTORS;
        return m;
    };
    {
        std::string p = "7_motors_";
        add((p + "driving").c_str(), motors(VS::ENABLED, RobotState::ENABLED, driving));
        //ON, RDY, OFF, -- (FL enabled, FR ready, RL neither, RR enabled and ready)
        { Reply::MotionView v = motion(0, 0, 0, {0.05f, 0, 0, -0.004f}, {0.02f, 0, 0, 0}, 0x00A9);
          add((p + "states_on_rdy_off").c_str(), motors(VS::ENABLING, RobotState::ENABLING, v)); }
        { Reply::MotionView v = atRest; v.b_flagsKnown = false; v.b_torqueKnown = false; for(int i = 0; i < 4; i++) v.wheelVelocity[i] = NAN;
          add((p + "unknown").c_str(), motors(VS::ENABLED, RobotState::ENABLED, v)); }
        //FL fault, FR STO, RL fault and STO (FLT at 0 s, STO at 1 s), RR ready
        { Reply::MotionView v = motion(0, 0, 0, {0, 0, 0, 0}, {0, 0, 0, 0}, 0x6580);
          ScreenModel m = motors(VS::AXIS_FAULT, RobotState::EMERGENCY_STOPPED, v); m.now = 0; add((p + "fault_sto_flt").c_str(), m);
          m.now = 1000; add((p + "fault_sto_sto").c_str(), m); }
        { Reply::MotionView v = motion(0, 0, 0, {0, 0, 0, 0}, {0, 0, 0, 0}, 0xF000);
          add((p + "estop_all_sto").c_str(), motors(VS::E_STOP, RobotState::EMERGENCY_STOPPED, v)); }
        { ScreenModel m = base(); m.page = Page::MOTORS; m.b_linkUp = false; m.motionAvailability = Reply::MotionAvailability::NO_LINK;
          add((p + "no_link").c_str(), m); }
        //the rollers scrolling: four frames 25 ms apart (the phase as RemoteBoard advances it)
        for(int frame = 0; frame < 4; frame++){
            ScreenModel m = motors(VS::ENABLED, RobotState::ENABLED, driving);
            for(int i = 0; i < 4; i++){
                float phase = 0.0f;
                for(int f = 0; f < frame; f++) phase = RemoteUi::advanceWheelPhase(phase, m.motion.wheelVelocity[i], 25);
                m.wheelPhase[i] = phase;
            }
            m.frame = uint8_t(frame * 3);
            add((p + "phase_" + std::to_string(frame)).c_str(), m);
        }
    }
    //the Tracking page (8x; render.py): the map round the rover at the three zoom steps, turned, zeroed, a trail
    auto tracking = [&](float x, float y, float heading, uint8_t zoomClicks){
        ScreenModel m = live(VS::ENABLED, RobotState::ENABLED, driving);
        m.page = Page::TRACKING;
        m.motion.poseX = x;
        m.motion.poseY = y;
        m.motion.heading = heading;
        TrackingMap map;
        for(uint8_t i = 0; i < zoomClicks; i++) map.zoom();
        map.view(m.tracking);
        m.tracking.compassHeading = heading;              //not zeroed: the compass from the same heading
        return m;
    };
    {
        std::string p = "8_tracking_";
        add((p + "zoom6_start").c_str(), tracking(1.3f, 2.4f, 0.0f, 0));
        add((p + "zoom12").c_str(), tracking(1.3f, 2.4f, 0.0f, 1));
        add((p + "zoom24").c_str(), tracking(1.3f, 2.4f, 0.0f, 2));
        add((p + "zoom3").c_str(), tracking(1.3f, 2.4f, 0.0f, 3));
        add((p + "turned_20_zoom6").c_str(), tracking(1.3f, 2.4f, 20.0f, 0));
        add((p + "turned_20_zoom24").c_str(), tracking(1.3f, 2.4f, 20.0f, 2));
        add((p + "turned_30").c_str(), tracking(1.3f, 2.4f, 30.0f, 0));
        add((p + "turned_135_zoom12").c_str(), tracking(0.6f, -0.4f, 135.0f, 1));
        { ScreenModel m = tracking(0.4f, 1.1f, -20.0f, 0); m.tracking.compassHeading = 70.0f; add((p + "zeroed").c_str(), m); }
        //the glide between 6 and 12 px per m, a quarter and halfway
        { ScreenModel m = tracking(1.3f, 2.4f, 0.0f, 0); TrackingMap g; g.zoom(0); g.view(m.tracking, 125);
          m.tracking.compassHeading = 0.0f; add((p + "glide_125ms").c_str(), m); }
        { ScreenModel m = tracking(1.3f, 2.4f, 0.0f, 0); TrackingMap g; g.zoom(0); g.view(m.tracking, 250);
          m.tracking.compassHeading = 0.0f; add((p + "glide_250ms").c_str(), m); }
        //a trail: 12 s on a circle of about 1.9 m (0.5 m/s, turning left 15 deg/s; samples every 200 ms, the last 10 s kept)
        {
            TrackingMap map;
            float x = 0.0f, y = 0.0f, heading = 0.0f;
            for(uint32_t t = 0; t <= 12000; t += 20){
                map.update(x, y, false, t);
                float r = heading * 0.017453293f;
                x += -sinf(r) * 0.5f * 0.02f;
                y += cosf(r) * 0.5f * 0.02f;
                heading += 15.0f * 0.02f;
            }
            ScreenModel m = tracking(x, y, heading, 0);
            map.view(m.tracking);
            m.tracking.compassHeading = heading;
            add((p + "trail").c_str(), m);
            map.zoom();
            ScreenModel m8 = tracking(x, y, heading, 1);
            map.view(m8.tracking);
            m8.tracking.compassHeading = heading;
            add((p + "trail_zoom12").c_str(), m8);
        }
        { ScreenModel m = tracking(NAN, NAN, NAN, 0); add((p + "no_position").c_str(), m); }
                { ScreenModel m = tracking(1.0f, 1.0f, 0.0f, 0); m.motionAvailability = Reply::MotionAvailability::NO_DATA; add((p + "no_motion_data").c_str(), m); }
        { ScreenModel m = tracking(1.0f, 1.0f, 0.0f, 0); m.b_linkUp = false; m.motionAvailability = Reply::MotionAvailability::NO_LINK; add((p + "no_link").c_str(), m); }
        { ScreenModel m = tracking(1.3f, 2.4f, 0.0f, 0); m.frame = 6; add((p + "alive_step").c_str(), m); }
    }
    //the marks and timed targets (owner, 2026-10-28): 8m_ the map with marks, questions, refusals and the drive arrow,
    //8r_ a timed run (the ghost, the pace, the arrival), 9l_ the mark list, 9t_ the trip, 9e_ the number editor
    //(render.py: their gaps)
    {
        MarkStore marks;
        marks.set(1, Mark::fromPose(0.0f, 0.0f, 0.0f, 0));
        marks.set(2, Mark::fromPose(0.0f, 2.0f, NAN));
        marks.set(3, Mark::fromPose(1.5f, 3.5f, -90.0f, 8000));
        marks.set(4, Mark::fromPose(3.5f, 2.0f, 180.0f, 12500));
        marks.set(6, Mark::fromPose(-2.25f, -1.0f, 45.0f));
        auto marked = [&](float x, float y, float heading, int8_t selected, bool b_edit){
            ScreenModel m = tracking(x, y, heading, 0);
            m.marks = marks;
            m.trackingControl.selected = selected;
            m.trackingControl.b_editMode = b_edit;
            m.speedState = b_edit ? 0 : 1;
            return m;
        };
        //a timed run to mark k (3: 8 s, 4: 12.5 s) from (0, 0) facing 0, as RemoteBoard drives TargetRun: elapsed s
        //into the run, the rover where the plan was at planned s, cross m to the right of the line and turned by turn
        //deg against the plan there; paused: the stopwatch stopped at elapsed
        auto running = [&](float elapsed, float planned, float cross, float turn, bool b_paused, uint8_t k = 3){
            StopwatchControl watch;
            TargetRun run{watch};
            run.click(marks, int8_t(k), true, 0.0f, 0.0f, 0.0f, 1000, 1000);
            const TargetPlan& p = run.getPlan();
            float gx, gy, gh;
            p.ghostAt(planned, gx, gy, gh);
            //the line's right: (dy, -dx) of its direction
            float lx = marks.get(k).x(), ly = marks.get(k).y(), length = hypotf(lx, ly);
            float x = gx + cross * ly / length, y = gy - cross * lx / length, heading = gh - turn;
            uint32_t now = 1000 + uint32_t(lroundf(elapsed * 1000.0f));
            run.update(true, x, y, heading, now);
            if(b_paused) run.click(marks, int8_t(k), true, x, y, heading, now, now);
            ScreenModel m = marked(x, y, heading, int8_t(k), false);
            run.view(m.run, now);
            return m;
        };
        //arrived at mark 3 after seconds (its duration 8 s)
        auto arrived = [&](float seconds){
            StopwatchControl watch;
            TargetRun run{watch};
            run.click(marks, 3, true, 0.0f, 0.0f, 0.0f, 1000, 1000);
            uint32_t now = 1000 + uint32_t(lroundf(seconds * 1000.0f));
            run.update(true, 1.45f, 3.42f, -86.0f, now);
            ScreenModel m = marked(1.45f, 3.42f, -86.0f, 3, false);
            run.view(m.run, now + 500);
            return m;
        };
        //a refused play with mark k selected (the pill shows 1.5 s)
        auto refused = [&](int8_t k, float x, float y, float heading, bool b_pose){
            StopwatchControl watch;
            TargetRun run{watch};
            run.click(marks, k, b_pose, x, y, heading, 1000, 1000);
            ScreenModel m = marked(x, y, heading, k, false);
            run.view(m.run, 1200);
            return m;
        };
        add("8m_marks_drive_none_selected", marked(0.4f, 0.8f, 10.0f, -1, false));
        add("8m_marks_drive_selected_3", marked(0.4f, 0.8f, 10.0f, 3, false));
        add("8m_marks_drive_selected_z", marked(0.4f, 0.8f, 10.0f, 0, false));
        { ScreenModel m = marked(0.4f, 0.8f, 10.0f, 4, false); m.b_worldFrame = true; m.driveHeading = 35.0f; add("8m_marks_world_drive_arrow", m); }
        { ScreenModel m = marked(0.4f, 0.8f, 10.0f, 4, false); m.b_worldFrame = true; m.driveHeading = 35.0f;
          TrackingMap z; z.zoom(); z.zoom(); z.view(m.tracking); m.tracking.compassHeading = 10.0f; add("8m_marks_zoom24_pill_shortened", m); }
        { ScreenModel m = marked(0.4f, 0.8f, 10.0f, 6, false); m.b_worldFrame = true; m.driveHeading = -120.0f; add("8m_marks_selected_6_arrow_back", m); }
        add("8m_edit_selected_2", marked(0.4f, 0.8f, 10.0f, 2, true));
        add("8m_edit_selected_empty_5", marked(0.4f, 0.8f, 10.0f, 5, true));
        add("8m_edit_none_no_marks", marked(0.4f, 0.8f, 10.0f, -1, true));
        { ScreenModel m = marked(0.4f, 0.8f, 10.0f, 3, true); m.trackingControl.view = TrackingControlView::View::LIST;
          m.trackingControl.question = TrackingControlView::Question::CLEAR_ALL; add("9l_list_clear_all", m); }
        using Q = TrackingControlView::Question;
        { ScreenModel m = marked(0.4f, 0.8f, 10.0f, 3, true); m.trackingControl.question = Q::AT; m.trackingControl.questionMark = 3; add("8m_question_at_3", m); }
        { ScreenModel m = marked(0.4f, 0.8f, 10.0f, 0, true); m.trackingControl.question = Q::AT; m.trackingControl.questionMark = 0; add("8m_question_at_z", m); }
        { ScreenModel m = marked(0.4f, 0.8f, 10.0f, 5, true); m.trackingControl.question = Q::SAVE; m.trackingControl.questionMark = 5; add("8m_question_save_5", m); }
        { ScreenModel m = marked(0.4f, 0.8f, 10.0f, 3, true); m.trackingControl.question = Q::OVERWRITE; m.trackingControl.questionMark = 3;
          m.now = 100; add("8m_question_overwrite_shown", m);
          m.now = 400; add("8m_question_overwrite_blank", m);
          m.trackingControl.holdProgress = 0.55f; add("8m_question_overwrite_holding", m); }
        add("8r_run_on_time_no_text", running(4.0f, 4.0f, 0.0f, 0.0f, false));
        add("8r_run_on_time_offset", running(4.1f, 4.0f, 0.3f, 15.0f, false));
        add("8r_run_late_faster", running(5.0f, 4.2f, 0.0f, 0.0f, false));
        add("8r_run_early_slower", running(3.0f, 3.5f, 0.0f, 0.0f, false));
        { ScreenModel m = running(6.5f, 4.0f, 0.0f, 0.0f, false); m.now = 100; add("8r_run_far_behind_filled", m);
          m.now = 600; add("8r_run_far_behind_plain", m); }
        add("8r_run_time_left_whole_seconds", running(1.5f, 1.0f, 0.0f, 0.0f, false, 4));
        add("8r_run_paused", running(3.0f, 3.0f, 0.0f, 0.0f, true));
        //the turn tracked apart: on time but the heading 25 deg behind the plan (turn more left), and late while turned
        //20 deg too far (turn back right)
        add("8r_run_turn_lag_on_time", running(4.0f, 4.0f, 0.0f, 25.0f, false));
        add("8r_run_turn_ahead_late", running(5.0f, 4.2f, 0.0f, -20.0f, false));
        { ScreenModel m = running(5.0f, 4.2f, 0.0f, 25.0f, false); m.b_worldFrame = true; m.driveHeading = -100.0f; add("8r_run_turn_world_arrow", m); }
        { ScreenModel m = running(5.0f, 4.2f, -0.25f, 15.0f, false); m.b_worldFrame = true; m.driveHeading = -100.0f; add("8r_run_world_arrow", m); }
        add("8r_arrived_late", arrived(8.4f));
        add("8r_arrived_early", arrived(7.4f));
        add("8m_refused_no_time_2", refused(2, 0.4f, 0.8f, 10.0f, true));
        add("8m_refused_z", refused(0, 0.4f, 0.8f, 10.0f, true));
        add("8m_refused_select_a_mark", refused(-1, 0.4f, 0.8f, 10.0f, true));
        add("8m_refused_already_at_3", refused(3, 1.45f, 3.45f, -88.0f, true));
        //the views
        { ScreenModel m = marked(0.4f, 0.8f, 10.0f, 3, true); m.trackingControl.view = TrackingControlView::View::LIST; add("9l_list_selected_3", m); }
        { ScreenModel m = marked(0.4f, 0.8f, 10.0f, 0, true); m.trackingControl.view = TrackingControlView::View::LIST; add("9l_list_selected_z", m); }
        { ScreenModel m = marked(0.4f, 0.8f, 10.0f, 9, true); m.trackingControl.view = TrackingControlView::View::LIST;
          m.marks.set(9, Mark::fromPose(-12.34f, -15.5f, -179.0f, 512300)); add("9l_list_widest_9", m); }
        { ScreenModel m = marked(0.4f, 0.8f, 10.0f, 3, true); m.trackingControl.view = TrackingControlView::View::LIST;
          m.trackingControl.question = Q::AT; m.trackingControl.questionMark = 3; add("9l_list_question", m); }
        { ScreenModel m = marked(0.4f, 0.8f, 10.0f, 3, true); m.trackingControl.view = TrackingControlView::View::TRIP;
          m.trip.distance = 12.35f; m.trip.turned = 245.0f; m.trip.top = 0.85f; m.trip.movingMillis = 102000; m.trip.since = 3; add("9t_trip_since_snap", m); }
        { ScreenModel m = marked(0.4f, 0.8f, 10.0f, 3, true); m.trackingControl.view = TrackingControlView::View::TRIP; m.trip.since = 0; add("9t_trip_zero", m); }
        auto editing = [&](uint8_t k, TrackingControlView::Field field){
            ScreenModel m = marked(0.4f, 0.8f, 10.0f, int8_t(k), true);
            m.trackingControl.b_editing = true;
            m.trackingControl.editMark = k;
            m.trackingControl.draft = marks.get(k);
            m.trackingControl.field = field;
            return m;
        };
        add("9e_editor_x", editing(3, TrackingControlView::Field::X));
        add("9e_editor_t", editing(3, TrackingControlView::Field::T));
        add("9e_editor_h_blank", editing(2, TrackingControlView::Field::H));
        { ScreenModel m = editing(4, TrackingControlView::Field::Y); m.trackingControl.draft = Mark::fromPose(-12.345f, -99.999f, -179.9f, 5999900); add("9e_editor_widest", m); }
        { ScreenModel m = editing(3, TrackingControlView::Field::X); m.trackingControl.question = Q::CLEAR; m.trackingControl.questionMark = 3; m.now = 100; add("9e_editor_clear_question", m);
          m.trackingControl.holdProgress = 0.4f; add("9e_editor_clear_holding", m); }
    }
    //the Link page's history: a minute with a fade and two dropouts
    {
        ScreenModel m = base();
        m.page = Page::LINK;
        LinkHistory h;
        for(int i = 0; i < 60; i++){
            uint8_t loss = (i >= 20 && i < 24) ? uint8_t(10 + 20 * (i - 20)) : (i == 41 ? 100 : (i % 13 == 0 ? 5 : 0));
            int dbm = -60 - int(25.0f * sinf(float(i) * 0.12f)) - (i >= 20 && i < 24 ? 15 : 0);
            h.update(uint32_t(i) * 1000, loss, int16_t(dbm), i != 41);
        }
        h.view(m.linkHistory);
        add("20c_page_link_history", m);
        LinkHistory s;
        for(int i = 0; i < 12; i++) s.update(uint32_t(i) * 1000, 0, int16_t(-58 - i), true);
        s.view(m.linkHistory);
        add("20d_page_link_history_first_12s", m);
        m.b_linkUp = false;
        s.update(12000, 100, 0, false);
        s.view(m.linkHistory);
        add("20e_page_link_history_lost", m);
    }
    { ScreenModel m = base(); m.b_menuOpen = true; m.speedState = 0; m.pending.channel = 5; add("30_menu_channel", m); }
    { ScreenModel m = base(); m.b_menuOpen = true; m.speedState = 0; m.pending.channel = RemoteSettings::radioOffChannel; add("30b_menu_radio_off", m); }
    { ScreenModel m = base(); m.b_menuOpen = true; m.speedState = 0; m.menuItem = SettingsMenu::Item::STICKS; add("31_menu_sticks", m); }
    { ScreenModel m = base(); m.b_menuOpen = true; m.speedState = 0; m.menuItem = SettingsMenu::Item::PACK; m.pending.pack = 3; add("31b_menu_pack", m); }
    { ScreenModel m = base(); m.settings.pack = 3; m.robotVoltage = 60.1f; add("01b_enabled_pack_percent", m); }
    { ScreenModel m = base(); m.b_menuOpen = true; m.speedState = 0; m.menuItem = SettingsMenu::Item::SAVE; m.pending.contrast = 2; m.saveProgress = 0.6f; add("32_menu_save", m); }
    { ScreenModel m = base(); m.b_menuOpen = true; m.speedState = 0; m.menuItem = SettingsMenu::Item::DIMMING; m.pending.dimming = 1; add("33_menu_dimming", m); }
    { ScreenModel m = base(); m.b_menuOpen = true; m.speedState = 0; m.menuItem = SettingsMenu::Item::EXIT; m.pending.contrast = 3; add("34_menu_exit", m); }
    { ScreenModel m = base(); m.b_menuOpen = true; m.speedState = 0; m.menuItem = SettingsMenu::Item::STICKS; m.note = "Saved"; add("35_menu_saved_note", m); }
    { ScreenModel m = base(); m.b_menuOpen = true; m.b_calibrating = true; m.raw[0] = 700; m.raw[1] = 400; add("40_calibration_release", m); }
    {
        ScreenModel m = base(); m.b_menuOpen = true; m.b_calibrating = true; m.b_circling = true; m.b_keepCircling = true;
        for(int i = 0; i < 4; i++){ m.calibration.axes[i] = {300, 510, 760}; }
        m.raw[2] = 700; m.raw[3] = 520;
        add("40b_calibration_further", m);
    }
    {
        ScreenModel m = base(); m.b_menuOpen = true; m.b_calibrating = true; m.b_circling = true; m.b_calibrationComplete = true;
        for(int i = 0; i < 4; i++){ m.calibration.axes[i] = {60, 510, 990}; }
        m.calibration.axes[1] = {120, 520, 870};
        m.raw[0] = 900; m.raw[1] = 300; m.raw[2] = 510; m.raw[3] = 990;
        add("41_calibration_circle", m);
    }
    {
        ScreenModel m = base(); m.b_calibrationResult = true;
        for(int i = 0; i < 4; i++){ m.calibration.axes[i] = {60, 510, 990}; }
        m.calibration.axes[1] = {120, 540, 870};
        add("42_calibration_result", m);
    }

    for(auto& example : examples){
        GFXcanvas1 canvas(128, 32);
        canvas.fillScreen(0);
        RemoteUi::draw(canvas, example.model);
        writePbm(canvas, outDir + "/" + example.name + ".pbm");
    }
    //transitions: one frame per 20 ms (the display's 50 Hz) over the 160 ms of a page change, forward then back
    //(t_<idea>_<f|b><frame>.pbm)
    struct Idea{
        const char* name;
        RemoteTransitions::Header header;
        RemoteTransitions::Content content;
        int headerDrawing;      //0: none, 1: split-flap title, 2: title swaps halfway and the dot glides, 3: only the dot glides
        bool b_fromStatus;
    };
    const Idea ideas[] = {
        {"0_now_all_slides", RemoteTransitions::Header::SLIDE, RemoteTransitions::Content::SLIDE, 0, false},
        {"A_header_rolls", RemoteTransitions::Header::ROLL, RemoteTransitions::Content::SLIDE, 0, false},
        {"A2_header_rolls_from_main", RemoteTransitions::Header::ROLL, RemoteTransitions::Content::SLIDE, 0, true},
        {"B_header_slides_content_dissolves", RemoteTransitions::Header::SLIDE, RemoteTransitions::Content::DISSOLVE, 0, false},
        {"C_header_lags", RemoteTransitions::Header::LAG, RemoteTransitions::Content::SLIDE, 0, false},
        {"D_split_flap_title", RemoteTransitions::Header::DRAWN, RemoteTransitions::Content::SLIDE, 1, false},
        {"E_header_still_dot_glides", RemoteTransitions::Header::DRAWN, RemoteTransitions::Content::SLIDE, 2, false},
        {"AE_header_rolls_dot_glides", RemoteTransitions::Header::ROLL, RemoteTransitions::Content::SLIDE, 3, false},
        {"AE2_from_main", RemoteTransitions::Header::ROLL, RemoteTransitions::Content::SLIDE, 3, true},
    };
    const int frames = 9;
    for(const Idea& idea : ideas) for(int direction = 0; direction < 2; direction++){
        bool b_forward = direction == 0;
        ScreenModel first = base();
        first.page = idea.b_fromStatus ? Page::STATUS : Page::LINK;
        ScreenModel second = base();
        second.page = idea.b_fromStatus ? Page::LINK : Page::RADIO;
        ScreenModel fromModel = b_forward ? first : second;
        ScreenModel toModel = b_forward ? second : first;
        GFXcanvas1 from(128, 32), to(128, 32), header(128, 32), out(128, 32);
        from.fillScreen(0);
        RemoteUi::draw(from, fromModel);
        for(int frame = 0; frame < frames; frame++){
            float progress = RemoteTransitions::easeOut(frame / float(frames - 1));
            to.fillScreen(0);
            RemoteUi::draw(to, toModel);
            if(idea.headerDrawing){
                //the header drawn for this frame, over the new frame's top rows
                ScreenModel headerModel = toModel;
                char title[16];
                if(idea.headerDrawing == 1){
                    RemoteTransitions::splitFlap(title, sizeof(title), RemoteUi::pageTitle(fromModel.page), RemoteUi::pageTitle(toModel.page), progress);
                    headerModel.titleOverride = title;
                }
                else{
                    if(idea.headerDrawing == 2 && progress < 0.5f) headerModel.page = fromModel.page;
                    if(fromModel.page != Page::STATUS)
                        headerModel.pageDotPosition = float(fromModel.page) + (float(toModel.page) - float(fromModel.page)) * progress;
                }
                header.fillScreen(0);
                RemoteUi::draw(header, headerModel);
                for(int y = 0; y < RemoteTransitions::headerRows; y++){
                    for(int x = 0; x < 128; x++) to.drawPixel(x, y, header.getPixel(x, y));
                }
            }
            //A + E: the dots glide in the drawn frame while the rest of the header rolls (from the main screen, which has
            //no dots, everything rolls)
            int16_t drawnFromX = 128;
            if(idea.headerDrawing == 3 && fromModel.page != Page::STATUS && toModel.page != Page::STATUS)
                drawnFromX = RemoteTransitions::pageDotsLeft(toModel.pageCount);
            RemoteTransitions::blend(out, from, to, idea.header, idea.content, progress, b_forward, drawnFromX);
            char name[64];
            snprintf(name, sizeof(name), "/t_%s_%c%d.pbm", idea.name, b_forward ? 'f' : 'b', frame);
            writePbm(out, outDir + name);
        }
    }

    //the alive sign: 24 frames each of the main screen (state dot) and a page (page dot), a GIF at 50 Hz each
    //(g_<name>_<frame>.pbm)
    for(int frame = 0; frame < 24; frame++){
        for(int page = 0; page < 3; page++){
            ScreenModel m = base();
            m.frame = frame;
            m.rxCounter = frame / 2;
            if(page == 1) m.status.message = BigMessage::DISABLED;
            if(page == 2) m.page = Page::BATTERY;
            GFXcanvas1 canvas(128, 32);
            canvas.fillScreen(0);
            RemoteUi::draw(canvas, m);
            char name[64];
            snprintf(name, sizeof(name), "/g_alive_%s_%d.pbm", page == 0 ? "enabled" : (page == 1 ? "disabled" : "page"), frame);
            writePbm(canvas, outDir + name);
        }
    }

    //the Motors page's rollers at 50 Hz, 2 s (g_motors_<frame>.pbm), RemoteBoard's phase steps [D-163]
    {
        ScreenModel m = base();
        m.page = Page::MOTORS;
        m.motionAvailability = Reply::MotionAvailability::LIVE;
        m.motion.b_velocityLive = true;
        m.motion.b_flagsKnown = true;
        m.motion.b_torqueKnown = true;
        m.motion.wheelFlags = 0x00FF;
        const float velocity[4] = {0.45f, -0.2f, 1.0f, -1.0f};
        const float torque[4] = {0.3f, -0.15f, 0.8f, -0.6f};
        for(int i = 0; i < 4; i++){ m.motion.wheelVelocity[i] = velocity[i]; m.motion.wheelTorque[i] = torque[i]; }
        for(int frame = 0; frame < 100; frame++){
            m.now = 100 + frame * 20;
            m.frame = uint8_t(frame);
            GFXcanvas1 canvas(128, 32);
            canvas.fillScreen(0);
            RemoteUi::draw(canvas, m);
            char name[64];
            snprintf(name, sizeof(name), "/g_motors_%d.pbm", frame);
            writePbm(canvas, outDir + name);
            for(int i = 0; i < 4; i++) m.wheelPhase[i] = RemoteUi::advanceWheelPhase(m.wheelPhase[i], velocity[i], 20);
        }
    }

    //the startup animations at 50 Hz (g_splash_<frame>.pbm, g_splash_becker_poels_<frame>.pbm)
    for(auto style : {RemoteSplash::Style::BERUPO, RemoteSplash::Style::BECKER_POELS}){
        for(int frame = 0; frame * 0.02f <= RemoteSplash::duration(style); frame++){
            GFXcanvas1 canvas(128, 32);
            RemoteSplash::draw(canvas, frame * 0.02f, style);
            char name[64];
            snprintf(name, sizeof(name), style == RemoteSplash::Style::BERUPO ? "/g_splash_%d.pbm" : "/g_splash_becker_poels_%d.pbm", frame);
            writePbm(canvas, outDir + name);
        }
    }

    printf("%zu screens\n", examples.size());
    return 0;
}
