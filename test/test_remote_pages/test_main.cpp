//Host tests of the remote's pages: the page order and which pages stay open [D-161, D-163], the Motors page's roller
//phase and the vehicle state words [D-163]: pio test -e native -f test_remote_pages
#include <unity.h>
#include <math.h>
#include <string.h>
#include <HostGfx.h>
#include <PageNavigator.h>
#include <RemoteScreens.h>

void setUp(){}
void tearDown(){}

//——— PageNavigator ———

void test_page_order_status_motion_motors_link(){
    PageNavigator pages;
    TEST_ASSERT_TRUE(pages.getPage() == Page::STATUS);
    pages.update(false, true, false, false, 10);
    TEST_ASSERT_TRUE(pages.getPage() == Page::MOTION);
    pages.update(false, true, false, false, 20);
    TEST_ASSERT_TRUE(pages.getPage() == Page::MOTORS);          //right after Motion [D-163]
    pages.update(false, true, false, false, 30);
    TEST_ASSERT_TRUE(pages.getPage() == Page::LINK);
    pages.update(true, false, false, false, 40);
    TEST_ASSERT_TRUE(pages.getPage() == Page::MOTORS);
    pages.update(true, false, false, false, 50);
    TEST_ASSERT_TRUE(pages.getPage() == Page::MOTION);
}

void test_pages_wrap_and_settings_only_at_off(){
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

void test_predicates_split_stays_open_and_stopwatch(){
    for(uint8_t i = 0; i < uint8_t(Page::COUNT); i++){
        Page page = Page(i);
        //the stopwatch's controls and timer: Status and Motion only [D-160]
        TEST_ASSERT_EQUAL(page == Page::STATUS || page == Page::MOTION, isMainPage(page));
        //no return to Status: the main pages and Motors [D-163]
        TEST_ASSERT_EQUAL(page == Page::STATUS || page == Page::MOTION || page == Page::MOTORS, staysOpen(page));
    }
}

void test_motion_and_motors_never_return_link_does(){
    PageNavigator pages;
    pages.update(false, true, false, false, 1000);
    TEST_ASSERT_TRUE(pages.getPage() == Page::MOTION);
    pages.update(false, false, false, false, 600000);
    TEST_ASSERT_TRUE(pages.getPage() == Page::MOTION);
    pages.update(false, true, false, false, 600010);
    TEST_ASSERT_TRUE(pages.getPage() == Page::MOTORS);
    pages.update(false, false, false, false, 610011);
    pages.update(false, false, false, false, 1200000);
    TEST_ASSERT_TRUE(pages.getPage() == Page::MOTORS);
    //the pages after it still return after 10 s without input; menu activity keeps them
    pages.update(false, true, false, false, 1200010);
    TEST_ASSERT_TRUE(pages.getPage() == Page::LINK);
    pages.update(false, false, false, true, 1209000);
    pages.update(false, false, false, false, 1219000);
    TEST_ASSERT_TRUE(pages.getPage() == Page::LINK);
    pages.update(false, false, false, false, 1219001);
    TEST_ASSERT_TRUE(pages.getPage() == Page::STATUS);
    //a new alarm still brings Status (the board calls showStatus)
    pages.update(false, true, false, false, 1219010);
    pages.update(false, true, false, false, 1219020);
    TEST_ASSERT_TRUE(pages.getPage() == Page::MOTORS);
    pages.showStatus();
    TEST_ASSERT_TRUE(pages.getPage() == Page::STATUS);
}

void test_motors_page_has_a_title(){
    TEST_ASSERT_EQUAL_STRING("Motors", RemoteUi::pageTitle(Page::MOTORS));
    TEST_ASSERT_EQUAL_STRING("Motion", RemoteUi::pageTitle(Page::MOTION));
}

//——— the Motors page's rollers ———

void test_wheel_phase_follows_velocity_and_time(){
    //100 % for 1 s: wheelPixelsPerSecond, in 20 ms frames
    float phase = 0.0f;
    for(int i = 0; i < 50; i++) phase = RemoteUi::advanceWheelPhase(phase, 1.0f, 20);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, RemoteUi::wheelPixelsPerSecond, phase);
    //backwards scrolls the other way, half speed half as far
    phase = 0.0f;
    for(int i = 0; i < 50; i++) phase = RemoteUi::advanceWheelPhase(phase, -0.5f, 20);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, -RemoteUi::wheelPixelsPerSecond / 2.0f, phase);
    //no value: no step; a long gap is capped at 100 ms
    TEST_ASSERT_EQUAL_FLOAT(3.0f, RemoteUi::advanceWheelPhase(3.0f, NAN, 20));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, RemoteUi::wheelPixelsPerSecond * 0.1f, RemoteUi::advanceWheelPhase(0.0f, 1.0f, 5000));
    //the same inputs give the same phase (the renders are deterministic)
    TEST_ASSERT_EQUAL_FLOAT(RemoteUi::advanceWheelPhase(1.25f, 0.3f, 20), RemoteUi::advanceWheelPhase(1.25f, 0.3f, 20));
}

void test_wheel_phase_stays_bounded(){
    float phase = 0.0f;
    for(int i = 0; i < 100000; i++) phase = RemoteUi::advanceWheelPhase(phase, 1.0f, 100);
    TEST_ASSERT_TRUE(fabsf(phase) < 4000.0f);
}

//——— the vehicle state words [D-163] ———

void test_vehicle_state_words(){
    using VS = Reply::VehicleState;
    TEST_ASSERT_EQUAL_STRING("NO LINK", RemoteUi::vehicleStateText(VS::NO_LINK, 0));
    TEST_ASSERT_EQUAL_STRING("NO STACATO", RemoteUi::vehicleStateText(VS::NO_STACATO, 0));
    TEST_ASSERT_EQUAL_STRING("BUS START", RemoteUi::vehicleStateText(VS::BUS_START, 0));
    TEST_ASSERT_EQUAL_STRING("BUS OFF", RemoteUi::vehicleStateText(VS::BUS_OFF, 0));
    TEST_ASSERT_EQUAL_STRING("VEHICLE OFF", RemoteUi::vehicleStateText(VS::VEHICLE_OFF, 0));
    TEST_ASSERT_EQUAL_STRING("DISABLED", RemoteUi::vehicleStateText(VS::DISABLED, 0));
    TEST_ASSERT_EQUAL_STRING("ENABLING", RemoteUi::vehicleStateText(VS::ENABLING, 0));
    TEST_ASSERT_EQUAL_STRING("ENABLED", RemoteUi::vehicleStateText(VS::ENABLED, 0));
    TEST_ASSERT_EQUAL_STRING("DISABLING", RemoteUi::vehicleStateText(VS::DISABLING, 0));
    TEST_ASSERT_EQUAL_STRING("E-STOPPING", RemoteUi::vehicleStateText(VS::E_STOPPING, 0));
    TEST_ASSERT_EQUAL_STRING("E-STOP", RemoteUi::vehicleStateText(VS::E_STOP, 0));
    TEST_ASSERT_EQUAL_STRING("AXIS FAULT", RemoteUi::vehicleStateText(VS::AXIS_FAULT, 0));
    //UNKNOWN: the 3 bit state's text, or none
    TEST_ASSERT_EQUAL_STRING("ENABLED", RemoteUi::vehicleStateText(VS::UNKNOWN, uint8_t(RobotState::ENABLED)));
    TEST_ASSERT_EQUAL_STRING("STOPPING", RemoteUi::vehicleStateText(VS::UNKNOWN, uint8_t(RobotState::EMERGENCY_STOPPING)));
    TEST_ASSERT_NULL(RemoteUi::vehicleStateText(VS::UNKNOWN, 0));
}

//——— drawing ———

static int litPixels(const ScreenModel& m){
    GFXcanvas1 canvas(128, 32);
    canvas.fillScreen(0);
    RemoteUi::draw(canvas, m);
    int lit = 0;
    for(int y = 0; y < 32; y++) for(int x = 0; x < 128; x++) if(canvas.getPixel(x, y)) lit++;
    return lit;
}

void test_motors_rollers_move_with_the_phase_only(){
    ScreenModel m;
    m.page = Page::MOTORS;
    m.b_linkUp = true;
    GFXcanvas1 a(128, 32), b(128, 32);
    a.fillScreen(0);
    RemoteUi::draw(a, m);
    //the same model draws the same picture
    b.fillScreen(0);
    RemoteUi::draw(b, m);
    TEST_ASSERT_EQUAL_MEMORY(a.getBuffer(), b.getBuffer(), 128 * 32 / 8);
    //a phase step moves the rollers: a different picture
    m.wheelPhase[0] = 1.0f;
    b.fillScreen(0);
    RemoteUi::draw(b, m);
    TEST_ASSERT_TRUE(memcmp(a.getBuffer(), b.getBuffer(), 128 * 32 / 8) != 0);
    //a full roller period later the same again
    m.wheelPhase[0] = 4.0f;
    b.fillScreen(0);
    RemoteUi::draw(b, m);
    TEST_ASSERT_EQUAL_MEMORY(a.getBuffer(), b.getBuffer(), 128 * 32 / 8);
}

void test_every_vehicle_state_draws_on_status_and_motion(){
    for(uint8_t s = 0; s <= uint8_t(Reply::VehicleState::UNKNOWN); s++){
        for(Page page : {Page::STATUS, Page::MOTION, Page::MOTORS}){
            ScreenModel m;
            m.page = page;
            m.b_linkUp = true;
            m.status = {BigMessage::DISABLED, StatusLine::NORMAL};
            m.motion.vehicleState = Reply::VehicleState(s);
            TEST_ASSERT_TRUE(litPixels(m) > 0);
        }
    }
}

int main(int argc, char** argv){
    UNITY_BEGIN();
    RUN_TEST(test_page_order_status_motion_motors_link);
    RUN_TEST(test_pages_wrap_and_settings_only_at_off);
    RUN_TEST(test_predicates_split_stays_open_and_stopwatch);
    RUN_TEST(test_motion_and_motors_never_return_link_does);
    RUN_TEST(test_motors_page_has_a_title);
    RUN_TEST(test_wheel_phase_follows_velocity_and_time);
    RUN_TEST(test_wheel_phase_stays_bounded);
    RUN_TEST(test_vehicle_state_words);
    RUN_TEST(test_motors_rollers_move_with_the_phase_only);
    RUN_TEST(test_every_vehicle_state_draws_on_status_and_motion);
    return UNITY_END();
}
