//Host tests of the remote's pages: the page order and which pages stay open [D-161, D-163], the Motors page's roller
//phase and the vehicle state words [D-163], the Tracking page's drawing: pio test -e native -f test_remote_pages
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
    pages.update(false, true, false, false, 25);
    TEST_ASSERT_TRUE(pages.getPage() == Page::TRACKING);        //right after Motors
    pages.update(false, true, false, false, 30);
    TEST_ASSERT_TRUE(pages.getPage() == Page::LINK);
    pages.update(true, false, false, false, 35);
    TEST_ASSERT_TRUE(pages.getPage() == Page::TRACKING);
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
        //no return to Status: the main pages, Motors [D-163] and Tracking
        TEST_ASSERT_EQUAL(page == Page::STATUS || page == Page::MOTION || page == Page::MOTORS || page == Page::TRACKING,
                          staysOpen(page));
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
    //Tracking after it stays too
    pages.update(false, true, false, false, 1200005);
    TEST_ASSERT_TRUE(pages.getPage() == Page::TRACKING);
    pages.update(false, false, false, false, 1200006);
    pages.update(false, false, false, false, 1200009);
    TEST_ASSERT_TRUE(pages.getPage() == Page::TRACKING);
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
    pages.update(false, true, false, false, 1219030);
    pages.update(false, false, false, false, 1900000);
    TEST_ASSERT_TRUE(pages.getPage() == Page::TRACKING);
    pages.showStatus();
    TEST_ASSERT_TRUE(pages.getPage() == Page::STATUS);
}

void test_motors_page_has_a_title(){
    TEST_ASSERT_EQUAL_STRING("Motors", RemoteUi::pageTitle(Page::MOTORS));
    TEST_ASSERT_EQUAL_STRING("Tracking", RemoteUi::pageTitle(Page::TRACKING));
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
        for(Page page : {Page::STATUS, Page::MOTION, Page::MOTORS, Page::TRACKING}){
            ScreenModel m;
            m.page = page;
            m.b_linkUp = true;
            m.status = {BigMessage::DISABLED, StatusLine::NORMAL};
            m.motion.vehicleState = Reply::VehicleState(s);
            TEST_ASSERT_TRUE(litPixels(m) > 0);
        }
    }
}

//——— the Tracking page ———

static ScreenModel trackingModel(float x, float y, float heading){
    ScreenModel m;
    m.page = Page::TRACKING;
    m.b_linkUp = true;
    m.motionAvailability = Reply::MotionAvailability::LIVE;
    m.motion.b_velocityLive = true;
    m.motion.poseX = x;
    m.motion.poseY = y;
    m.motion.heading = heading;
    return m;
}

static void drawInto(GFXcanvas1& c, const ScreenModel& m){
    c.fillScreen(0);
    RemoteUi::draw(c, m);
}

//the rover is always the same arrowhead at the centre, pointing up, on a dark margin; the map moves under it
void test_tracking_rover_fixed_at_the_centre(){
    static const uint8_t rover[7] = { 0x20, 0x20, 0x70, 0x70, 0xF8, 0xF8, 0xD8 };
    ScreenModel models[] = {trackingModel(1.3f, 2.4f, 0.0f), trackingModel(-7.0f, 3.0f, 135.0f), trackingModel(NAN, NAN, NAN)};
    models[2].motionAvailability = Reply::MotionAvailability::LIVE;
    for(ScreenModel& m : models){
        GFXcanvas1 c(128, 32);
        drawInto(c, m);
        for(int16_t y = 12; y <= 20; y++){
            for(int16_t x = 61; x <= 67; x++){
                bool b_lit = false;
                if(y >= 13 && y <= 19 && x >= 62 && x <= 66) b_lit = (rover[y - 13] >> (7 - (x - 62))) & 1;
                TEST_ASSERT_EQUAL(b_lit, c.getPixel(x, y) != 0);
            }
        }
    }
}

void test_tracking_map_moves_and_turns_with_the_pose(){
    GFXcanvas1 a(128, 32), b(128, 32);
    drawInto(a, trackingModel(1.3f, 2.4f, 0.0f));
    drawInto(b, trackingModel(1.3f, 2.4f, 0.0f));
    TEST_ASSERT_EQUAL_MEMORY(a.getBuffer(), b.getBuffer(), 128 * 32 / 8);     //deterministic
    drawInto(b, trackingModel(1.8f, 2.4f, 0.0f));
    TEST_ASSERT_TRUE(memcmp(a.getBuffer(), b.getBuffer(), 128 * 32 / 8) != 0);
    drawInto(b, trackingModel(1.3f, 2.4f, 30.0f));
    TEST_ASSERT_TRUE(memcmp(a.getBuffer(), b.getBuffer(), 128 * 32 / 8) != 0);
    //a whole turn is the same picture
    drawInto(b, trackingModel(1.3f, 2.4f, 360.0f));
    int differ = 0;
    for(int y = 0; y < 32; y++) for(int x = 0; x < 128; x++) if(a.getPixel(x, y) != b.getPixel(x, y)) differ++;
    TEST_ASSERT_TRUE(differ < 4);       //float rounding at most
}

//at 3 px per m only the 5 m lines: another picture than at 6 px per m with the dotted metres
void test_tracking_zoom_changes_the_grid_and_the_label(){
    ScreenModel m = trackingModel(1.3f, 2.4f, 0.0f);
    m.tracking.scale = 3.0f;
    GFXcanvas1 three(128, 32), six(128, 32);
    drawInto(three, m);
    m.tracking.scale = 6.0f;
    drawInto(six, m);
    TEST_ASSERT_TRUE(memcmp(three.getBuffer(), six.getBuffer(), 128 * 32 / 8) != 0);
    //the label's bar: 5 m = 15 px at 3 px per m, 1 m = 6 px at 6 px per m (row 30 from x 0, ticks at both ends)
    for(int16_t x = 0; x <= 15; x++) TEST_ASSERT_TRUE(three.getPixel(x, 30));
    TEST_ASSERT_FALSE(three.getPixel(16, 30));
    for(int16_t x = 0; x <= 6; x++) TEST_ASSERT_TRUE(six.getPixel(x, 30));
    TEST_ASSERT_FALSE(six.getPixel(7, 30));
    TEST_ASSERT_TRUE(six.getPixel(6, 28) && six.getPixel(0, 28));
}

//no pose, no motion data or no link: no map, a line of text and the rover; the label too goes
void test_tracking_without_a_pose_draws_no_map(){
    ScreenModel cases[3] = {trackingModel(NAN, NAN, NAN), trackingModel(1.0f, 1.0f, 0.0f), trackingModel(1.0f, 1.0f, 0.0f)};
    cases[1].motionAvailability = Reply::MotionAvailability::NO_DATA;
    cases[2].motionAvailability = Reply::MotionAvailability::NO_LINK;
    cases[2].b_linkUp = false;
    for(ScreenModel& m : cases){
        GFXcanvas1 c(128, 32);
        drawInto(c, m);
        int lit = 0;
        for(int y = 8; y < 32; y++) for(int x = 0; x < 128; x++) if(c.getPixel(x, y)) lit++;
        TEST_ASSERT_EQUAL(22, lit);         //the rover only (the arrowhead's 22 pixels) below the text line
    }
}

void test_tracking_trail_drawn(){
    ScreenModel m = trackingModel(0.0f, 0.0f, 0.0f);
    m.tracking.scale = 4.0f;
    GFXcanvas1 a(128, 32), b(128, 32);
    drawInto(a, m);
    //a sample 2 m behind the rover at 4 px per m: a 2 x 2 dot 8 px below the centre
    m.tracking.trailCount = 1;
    m.tracking.trailX[0] = 0.0f;
    m.tracking.trailY[0] = -2.0f;
    drawInto(b, m);
    TEST_ASSERT_TRUE(b.getPixel(64, 24) && b.getPixel(65, 24) && b.getPixel(64, 25) && b.getPixel(65, 25));
    TEST_ASSERT_FALSE(b.getPixel(63, 23) || b.getPixel(66, 26));       //its halo
}

//——— the marks and timed targets (owner, 2026-10-28) ———

static int litIn(const GFXcanvas1& c, int16_t x0, int16_t y0, int16_t x1, int16_t y1){
    int n = 0;
    for(int16_t y = y0; y <= y1; y++) for(int16_t x = x0; x <= x1; x++) if(c.getPixel(x, y)) n++;
    return n;
}

//the drive-frame arrow in the bottom right corner only at WORLD (driveHeading set), turned by Stacato's own heading
void test_tracking_drive_arrow_only_at_world(){
    ScreenModel m = trackingModel(0.0f, 0.0f, 0.0f);
    GFXcanvas1 body(128, 32), up(128, 32), left(128, 32);
    drawInto(body, m);
    m.driveHeading = 0.0f;
    drawInto(up, m);
    m.driveHeading = 90.0f;
    drawInto(left, m);
    //at BODY no arrow and no dark disc: the corner shows the map as it is
    TEST_ASSERT_TRUE(litIn(up, 119, 23, 127, 31) > 4);
    bool b_cornerDiffers = false;
    for(int16_t y = 22; y < 32; y++) for(int16_t x = 118; x < 128; x++) if(body.getPixel(x, y) != up.getPixel(x, y)) b_cornerDiffers = true;
    TEST_ASSERT_TRUE(b_cornerDiffers);
    //upstage straight ahead: the tip above the centre; facing stage left (90) upstage is to the right
    TEST_ASSERT_TRUE(litIn(up, 121, 23, 125, 25) > litIn(up, 121, 29, 125, 31));
    TEST_ASSERT_TRUE(litIn(left, 125, 25, 127, 29) > litIn(left, 118, 25, 121, 29));
    //the dark disc keeps the grid off it at BODY too: nothing differs elsewhere
    TEST_ASSERT_EQUAL_INT(litIn(body, 0, 0, 110, 20), litIn(up, 0, 0, 110, 20));
}

//the ghost on the rover's own place: a ring of radius 6 round the arrowhead, which keeps its dark margin
void test_tracking_ghost_ring_holds_the_rover(){
    ScreenModel m = trackingModel(1.0f, 1.0f, 0.0f);
    m.run.b_run = true;
    m.run.b_ghost = true;
    m.run.ghostX = 1.0f;
    m.run.ghostY = 1.0f;
    m.run.ghostHeading = 0.0f;
    GFXcanvas1 c(128, 32);
    drawInto(c, m);
    TEST_ASSERT_TRUE(c.getPixel(64 - 6, 16) && c.getPixel(64 + 6, 16) && c.getPixel(64, 16 + 6));
    //the tick outside the rim along the heading (up)
    TEST_ASSERT_TRUE(c.getPixel(64, 16 - 7) || c.getPixel(64, 16 - 8));
    //between the ring and the rover: dark
    TEST_ASSERT_FALSE(c.getPixel(64 - 5, 16) || c.getPixel(64 + 5, 16) || c.getPixel(64, 16 + 5));
}

//a selected mark: a pill at its place; the others plain digits; the pill at the bottom names it
void test_tracking_marks_drawn_at_their_place(){
    ScreenModel m = trackingModel(0.0f, 0.0f, 0.0f);
    m.marks.set(3, Mark::fromPose(0.0f, 1.5f, 0.0f));        //1.5 m ahead: 9 px up at 6 px per m
    GFXcanvas1 plain(128, 32), selected(128, 32);
    drawInto(plain, m);
    m.trackingControl.selected = 3;
    drawInto(selected, m);
    //plain: a digit's few pixels round (64, 7); selected: a filled 9 x 9 pill there
    TEST_ASSERT_TRUE(litIn(plain, 62, 4, 66, 10) > 5);
    TEST_ASSERT_TRUE(litIn(selected, 60, 3, 68, 11) > litIn(plain, 60, 3, 68, 11) + 20);
    //the bottom pill appears only with a selection
    TEST_ASSERT_TRUE(litIn(selected, 30, 24, 110, 30) > litIn(plain, 30, 24, 110, 30) + 40);
}

//the views replace the map in edit mode; outside edit mode the map stays whatever the view was
void test_tracking_views_only_in_edit_mode(){
    ScreenModel m = trackingModel(0.0f, 0.0f, 0.0f);
    GFXcanvas1 map(128, 32), list(128, 32), drive(128, 32), editor(128, 32);
    drawInto(map, m);
    m.trackingControl.view = TrackingControlView::View::LIST;
    drawInto(drive, m);
    m.trackingControl.b_editMode = true;
    drawInto(list, m);
    m.trackingControl.b_editing = true;
    m.trackingControl.editMark = 2;
    drawInto(editor, m);
    bool b_same = true, b_listDiffers = false, b_editorDiffers = false;
    for(int16_t y = 0; y < 32; y++) for(int16_t x = 0; x < 128; x++){
        if(map.getPixel(x, y) != drive.getPixel(x, y)) b_same = false;
        if(map.getPixel(x, y) != list.getPixel(x, y)) b_listDiffers = true;
        if(list.getPixel(x, y) != editor.getPixel(x, y)) b_editorDiffers = true;
    }
    TEST_ASSERT_TRUE(b_same);
    TEST_ASSERT_TRUE(b_listDiffers);
    TEST_ASSERT_TRUE(b_editorDiffers);
    //the editor has no rover: its arrowhead's tip column (x 64, rows 13..14) is not drawn as on the map
    TEST_ASSERT_TRUE(map.getPixel(64, 13) && map.getPixel(64, 14));
    TEST_ASSERT_FALSE(editor.getPixel(64, 13) && editor.getPixel(64, 14) && editor.getPixel(63, 15) && editor.getPixel(65, 15));
}

//the Link page's history: the newest second at x 59, a lost second a full bar, the numbers kept on the right
void test_link_history_drawn(){
    ScreenModel m;
    m.page = Page::LINK;
    m.b_linkUp = true;
    m.downDbm = -62;
    m.upDbm = -58;
    LinkHistory h;
    for(int i = 0; i < 59; i++) h.update(uint32_t(i) * 1000, 0, -75, true);
    h.update(59000, 100, 0, false);
    h.view(m.linkHistory);
    GFXcanvas1 c(128, 32);
    drawInto(c, m);
    TEST_ASSERT_EQUAL_INT(23, litIn(c, 59, 9, 59, 31));          //the full bar
    TEST_ASSERT_TRUE(litIn(c, 0, 9, 57, 31) >= 58);              //the line, one pixel a second at least
    TEST_ASSERT_TRUE(litIn(c, 64, 9, 126, 31) > 40);             //the numbers
    TEST_ASSERT_EQUAL_INT(0, litIn(c, 60, 9, 62, 31));           //apart from the graph
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
    RUN_TEST(test_tracking_rover_fixed_at_the_centre);
    RUN_TEST(test_tracking_map_moves_and_turns_with_the_pose);
    RUN_TEST(test_tracking_zoom_changes_the_grid_and_the_label);
    RUN_TEST(test_tracking_without_a_pose_draws_no_map);
    RUN_TEST(test_tracking_trail_drawn);
    RUN_TEST(test_tracking_drive_arrow_only_at_world);
    RUN_TEST(test_tracking_ghost_ring_holds_the_rover);
    RUN_TEST(test_tracking_marks_drawn_at_their_place);
    RUN_TEST(test_tracking_views_only_in_edit_mode);
    RUN_TEST(test_link_history_drawn);
    return UNITY_END();
}
