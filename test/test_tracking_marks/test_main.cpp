//Host tests of the Tracking page's marks and timed targets (owner, 2026-10-28): the display correction's snap, the
//marks' EEPROM image, the controls in edit and drive mode, the timed run (the plan's profile and its inverse, the
//ghost, the pace, the arrival, pause, rewind, refusals, teach-in, the time left), the trip readout and the link
//history: pio test -e native -f test_tracking_marks
#include <unity.h>
#include <math.h>
#include <string.h>
#include <PoseZero.h>
#include <MarkStore.h>
#include <TrackingControl.h>
#include <TimedTarget.h>
#include <TripMeter.h>
#include <LinkHistory.h>
#include <Stopwatch.h>

void setUp(){}
void tearDown(){}

static void shown(const PoseZero& z, float x, float y, float h, float& rx, float& ry, float& rh){
    rx = x; ry = y; rh = h;
    z.apply(rx, ry, rh);
}

//——— the display correction: snap ———

void test_snap_position_and_heading(){
    PoseZero z;
    TEST_ASSERT_TRUE(z.snap(3.0f, 4.0f, 90.0f, 1.0f, 2.0f, 30.0f));
    TEST_ASSERT_TRUE(z.isActive());
    float x, y, h;
    shown(z, 3.0f, 4.0f, 90.0f, x, y, h);
    TEST_ASSERT_FLOAT_WITHIN(1e-4f, 1.0f, x);
    TEST_ASSERT_FLOAT_WITHIN(1e-4f, 2.0f, y);
    TEST_ASSERT_FLOAT_WITHIN(1e-3f, 30.0f, h);
    //1 m forward (heading 90 faces world -x): in the display frame along its heading 30, (-sin 30, cos 30)
    shown(z, 2.0f, 4.0f, 90.0f, x, y, h);
    TEST_ASSERT_FLOAT_WITHIN(1e-4f, 0.5f, x);
    TEST_ASSERT_FLOAT_WITHIN(1e-4f, 2.0f + 0.8660254f, y);
    //turning 10 deg left reads 40
    shown(z, 3.0f, 4.0f, 100.0f, x, y, h);
    TEST_ASSERT_FLOAT_WITHIN(1e-3f, 40.0f, h);
}

void test_snap_blank_heading_keeps_the_heading_shown(){
    PoseZero z;
    //a zero at (3, 4) facing 90: then at (5, 5) heading 100 the page shows 10
    z.update(false, 0, 0, 0);
    z.update(true, 3.0f, 4.0f, 90.0f);
    float x, y, h;
    shown(z, 5.0f, 5.0f, 100.0f, x, y, h);
    TEST_ASSERT_FLOAT_WITHIN(1e-3f, 10.0f, h);
    //"I am on mark (1, 1)", no heading: the place snaps, the heading shown stays 10
    TEST_ASSERT_TRUE(z.snap(5.0f, 5.0f, 100.0f, 1.0f, 1.0f, NAN));
    shown(z, 5.0f, 5.0f, 100.0f, x, y, h);
    TEST_ASSERT_FLOAT_WITHIN(1e-4f, 1.0f, x);
    TEST_ASSERT_FLOAT_WITHIN(1e-4f, 1.0f, y);
    TEST_ASSERT_FLOAT_WITHIN(1e-3f, 10.0f, h);
    //before any zero a place-only snap keeps Stacato's heading
    PoseZero fresh;
    TEST_ASSERT_TRUE(fresh.snap(2.0f, 2.0f, 45.0f, 0.5f, 0.5f, NAN));
    shown(fresh, 2.0f, 2.0f, 45.0f, x, y, h);
    TEST_ASSERT_FLOAT_WITHIN(1e-4f, 0.5f, x);
    TEST_ASSERT_FLOAT_WITHIN(1e-3f, 45.0f, h);
    //no pose: nothing changes
    PoseZero none;
    TEST_ASSERT_FALSE(none.snap(NAN, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f));
    TEST_ASSERT_FALSE(none.snap(1.0f, 1.0f, NAN, 0.0f, 0.0f, 0.0f));
    TEST_ASSERT_FALSE(none.isActive());
}

//a snap to Z is a zero; the frame switch's next zero replaces a snap; a snap cancels the drift since the zero
void test_zero_and_snap_interplay(){
    PoseZero byEdge, bySnap;
    byEdge.update(false, 0, 0, 0);
    byEdge.update(true, 3.0f, -2.0f, 33.0f);
    bySnap.snap(3.0f, -2.0f, 33.0f, 0.0f, 0.0f, 0.0f);
    float ax, ay, ah, bx, by, bh;
    shown(byEdge, 4.2f, 1.1f, 70.0f, ax, ay, ah);
    shown(bySnap, 4.2f, 1.1f, 70.0f, bx, by, bh);
    TEST_ASSERT_FLOAT_WITHIN(1e-4f, ax, bx);
    TEST_ASSERT_FLOAT_WITHIN(1e-4f, ay, by);
    TEST_ASSERT_FLOAT_WITHIN(1e-3f, ah, bh);
    //drift: the zero at (0, 0, 0); the vehicle is on mark (2, 0) but the pose says (2.1, 0.05, 1)
    PoseZero z;
    z.update(false, 0, 0, 0);
    z.update(true, 0.0f, 0.0f, 0.0f);
    z.snap(2.1f, 0.05f, 1.0f, 2.0f, 0.0f, 0.0f);
    float x, y, h;
    shown(z, 2.1f, 0.05f, 1.0f, x, y, h);
    TEST_ASSERT_FLOAT_WITHIN(1e-4f, 2.0f, x);
    TEST_ASSERT_FLOAT_WITHIN(1e-4f, 0.0f, y);
    TEST_ASSERT_FLOAT_WITHIN(1e-3f, 0.0f, h);
    //the switch back to BODY and to WORLD again: a new zero, the snap is gone
    z.update(false, 2.1f, 0.05f, 1.0f);
    TEST_ASSERT_TRUE(z.update(true, 2.1f, 0.05f, 1.0f));
    shown(z, 2.1f, 0.05f, 1.0f, x, y, h);
    TEST_ASSERT_FLOAT_WITHIN(1e-4f, 0.0f, x);
    TEST_ASSERT_FLOAT_WITHIN(1e-4f, 0.0f, y);
}

//——— the marks and their EEPROM image ———

void test_marks_quantised_and_z_fixed(){
    Mark m = Mark::fromPose(1.23449f, -2.0006f, 190.04f, 12345);
    TEST_ASSERT_EQUAL_INT32(1234, m.xMm);
    TEST_ASSERT_EQUAL_INT32(-2001, m.yMm);
    TEST_ASSERT_EQUAL_INT16(-1700, m.headingTenths);    //wrapped to -170.0
    TEST_ASSERT_EQUAL_UINT16(123, m.timeTenths);        //12.3 s
    Mark blank = Mark::fromPose(0.0f, 0.0f, NAN);
    TEST_ASSERT_FALSE(blank.b_heading);
    TEST_ASSERT_FALSE(blank.b_time);
    TEST_ASSERT_TRUE(isnan(blank.heading()));
    MarkStore s;
    TEST_ASSERT_TRUE(s.isUsed(0));
    TEST_ASSERT_FALSE(s.set(0, m));                     //Z is fixed
    TEST_ASSERT_EQUAL_INT32(0, s.get(0).xMm);
    TEST_ASSERT_TRUE(s.set(3, m));
    TEST_ASSERT_FALSE(s.set(3, m));                     //no change: nothing to write
    TEST_ASSERT_TRUE(s.clear(3));
    TEST_ASSERT_FALSE(s.isUsed(3));
    TEST_ASSERT_EQUAL_UINT16(512, MarkStore::address);
    TEST_ASSERT_EQUAL_UINT16(148, MarkStore::size);
    //clear of the arcade's block (256 + 200) and inside the Teensy's 4284 bytes
    TEST_ASSERT_TRUE(MarkStore::address >= 256 + 256);
    TEST_ASSERT_TRUE(MarkStore::address + MarkStore::size <= 4284);
}

void test_marks_eeprom_round_trip(){
    MarkStore s;
    s.set(1, Mark::fromPose(1.0f, 2.0f, 90.0f, 5000));
    s.set(4, Mark::fromPose(-12.345f, 0.5f, NAN));              //a via mark, no heading
    s.set(9, Mark::fromPose(99.999f, -99.999f, -179.9f, 5999900));
    uint8_t bytes[MarkStore::size];
    s.toBytes(bytes);
    TEST_ASSERT_EQUAL_HEX8(0x4B, bytes[0]);
    TEST_ASSERT_EQUAL_HEX8(1, bytes[1]);
    for(int i = 0; i < 16; i++) TEST_ASSERT_EQUAL_HEX8(0xFF, bytes[4 + 16 + i]);   //mark 2 empty: all 0xFF
    MarkStore back;
    back.fromBytes(bytes);
    for(uint8_t k = 0; k < MarkStore::count; k++) TEST_ASSERT_TRUE(back.get(k) == s.get(k));
    TEST_ASSERT_EQUAL_UINT16(59999, back.get(9).timeTenths);
    TEST_ASSERT_FALSE(back.get(4).b_heading);
}

void test_marks_blank_foreign_and_corrupt_blocks(){
    uint8_t bytes[MarkStore::size];
    memset(bytes, 0xFF, sizeof(bytes));
    MarkStore s;
    s.set(2, Mark::fromPose(1.0f, 1.0f, 0.0f));
    s.fromBytes(bytes);                                         //blank EEPROM: all empty
    for(uint8_t k = 1; k < MarkStore::count; k++) TEST_ASSERT_FALSE(s.isUsed(k));
    TEST_ASSERT_TRUE(s.isUsed(0));
    MarkStore full;
    for(uint8_t k = 1; k < MarkStore::count; k++) full.set(k, Mark::fromPose(float(k), 0.0f, 10.0f * k, 1000 * k));
    full.toBytes(bytes);
    bytes[0] = 0xA7;                                            //another block's marker
    s.fromBytes(bytes);
    for(uint8_t k = 1; k < MarkStore::count; k++) TEST_ASSERT_FALSE(s.isUsed(k));
    full.toBytes(bytes);
    bytes[1] = 2;                                               //another layout
    s.fromBytes(bytes);
    TEST_ASSERT_FALSE(s.isUsed(1));
    //one record's byte flipped: that mark only reads as empty
    full.toBytes(bytes);
    bytes[4 + 2 * 16 + 3] ^= 0x10;                              //mark 3's x
    s.fromBytes(bytes);
    TEST_ASSERT_FALSE(s.isUsed(3));
    TEST_ASSERT_TRUE(s.isUsed(2));
    TEST_ASSERT_TRUE(s.isUsed(4));
    //a value out of range with a matching check is refused too
    full.toBytes(bytes);
    uint8_t* r = bytes + 4 + 4 * 16;                            //mark 5
    r[11] = 0xFF; r[12] = 0xFF;                                 //time 6553.5 s
    uint8_t sum = 0;
    for(int i = 0; i < 13; i++) sum = uint8_t(sum + r[i]);
    r[13] = uint8_t(sum ^ 0xA5);
    s.fromBytes(bytes);
    TEST_ASSERT_FALSE(s.isUsed(5));
    TEST_ASSERT_TRUE(s.isUsed(6));
}

//——— the controls ———

struct Rig{
    MarkStore marks;
    TrackingControl c{marks};
    TrackingControl::Inputs in;
    uint32_t t = 1000;
    Rig(){
        in.b_active = true;
        in.b_speedOff = true;
        in.b_poseKnown = true;
        in.x = 1.0f; in.y = 2.0f; in.heading = 30.0f;
        c.update(in);
    }
    uint32_t at(uint32_t now){ t = now; in.now = now; return c.update(in); }
    uint32_t step(uint32_t dt = 20){ return at(t + dt); }
    //a click: pressed now, released after 100 ms; the events of all three loops
    uint32_t click(bool b_right){
        (b_right ? in.b_rightHeld : in.b_leftHeld) = true;
        uint32_t events = step();
        events |= step(100);
        (b_right ? in.b_rightHeld : in.b_leftHeld) = false;
        return events | step();
    }
    //a hold: pressed, the events at 1 s, then released (the release's events or'ed in)
    uint32_t hold(bool b_right){
        (b_right ? in.b_rightHeld : in.b_leftHeld) = true;
        step();
        uint32_t events = 0;
        for(int i = 0; i < 55; i++) events |= step();
        (b_right ? in.b_rightHeld : in.b_leftHeld) = false;
        return events | step();
    }
    //a flick of the left stick (dx, dy) and back
    uint32_t flickLeft(float x, float y){
        in.sticks[0] = x; in.sticks[1] = y;
        uint32_t events = step();
        in.sticks[0] = 0; in.sticks[1] = 0;
        return events | step();
    }
    uint32_t pushbutton(){
        in.b_pushbutton = true;
        uint32_t events = step();
        in.b_pushbutton = false;
        return events;
    }
    void select(int8_t k){ while(c.getSelected() != k) flickLeft(0.0f, -1.0f); }
};

using TC = TrackingControl;
using Q = TrackingControlView::Question;

void test_edit_selection_and_views(){
    Rig r;
    TEST_ASSERT_EQUAL_INT8(-1, r.c.getSelected());
    TEST_ASSERT_TRUE(r.flickLeft(0.0f, -1.0f) & TC::SELECTED);      //down: next
    TEST_ASSERT_EQUAL_INT8(0, r.c.getSelected());                   //Z
    r.flickLeft(0.0f, -1.0f);
    TEST_ASSERT_EQUAL_INT8(1, r.c.getSelected());
    r.flickLeft(0.0f, 1.0f);
    r.flickLeft(0.0f, 1.0f);
    TEST_ASSERT_EQUAL_INT8(-1, r.c.getSelected());
    r.flickLeft(0.0f, 1.0f);                                        //up wraps to 9
    TEST_ASSERT_EQUAL_INT8(9, r.c.getSelected());
    //held out: once
    r.in.sticks[1] = 1.0f;
    r.step(); r.step(); r.step();
    r.in.sticks[1] = 0.0f; r.step();
    TEST_ASSERT_EQUAL_INT8(8, r.c.getSelected());
    //left / right: map -> list -> trip -> map
    TEST_ASSERT_TRUE(r.c.getView() == TrackingControlView::View::MAP);
    TEST_ASSERT_TRUE(r.flickLeft(1.0f, 0.0f) & TC::VIEW_CHANGED);
    TEST_ASSERT_TRUE(r.c.getView() == TrackingControlView::View::LIST);
    r.flickLeft(1.0f, 0.0f);
    TEST_ASSERT_TRUE(r.c.getView() == TrackingControlView::View::TRIP);
    r.flickLeft(1.0f, 0.0f);
    TEST_ASSERT_TRUE(r.c.getView() == TrackingControlView::View::MAP);
    r.flickLeft(-1.0f, 0.0f);
    TEST_ASSERT_TRUE(r.c.getView() == TrackingControlView::View::TRIP);
}

//[owner 2026-10-28] the list view's left hold clears every mark, only after a second 1 s hold; on the map it clears
//the trail as before
void test_edit_clear_all_marks_from_the_list(){
    Rig r;
    r.marks.set(2, Mark::fromPose(1.0f, 1.0f, 0.0f));
    r.marks.set(7, Mark::fromPose(2.0f, 1.0f, 0.0f));
    TEST_ASSERT_TRUE(r.hold(false) & TC::CLEAR_TRAIL);              //the map view
    r.flickLeft(1.0f, 0.0f);                                        //-> the list
    TEST_ASSERT_TRUE(r.c.view(r.t).view == TrackingControlView::View::LIST);
    TEST_ASSERT_TRUE(r.hold(false) & TC::ASKED);
    TEST_ASSERT_TRUE(r.c.view(r.t).question == Q::CLEAR_ALL);
    TEST_ASSERT_FALSE(r.click(false) & TC::ALL_CLEARED);            //a click does not confirm
    TEST_ASSERT_TRUE(r.marks.isUsed(2));
    uint32_t e = r.hold(false);
    TEST_ASSERT_TRUE((e & TC::ALL_CLEARED) && (e & TC::MARKS_CHANGED));
    TEST_ASSERT_FALSE(r.marks.isUsed(2) || r.marks.isUsed(7));
    //cancelled by the right button, nothing cleared
    r.marks.set(3, Mark::fromPose(1.0f, 2.0f, 0.0f));
    r.hold(false);
    TEST_ASSERT_TRUE(r.click(true) & TC::CANCELLED || !r.c.isAsking());
    TEST_ASSERT_TRUE(r.marks.isUsed(3));
}

void test_edit_snap_confirm_timeout_and_cancels(){
    Rig r;
    r.marks.set(3, Mark::fromPose(1.0f, 1.0f, 0.0f));
    //nothing selected, an empty mark, no pose: refused
    TEST_ASSERT_TRUE(r.click(false) & TC::REFUSED);
    r.select(4);
    TEST_ASSERT_TRUE(r.click(false) & TC::REFUSED);
    r.select(3);
    r.in.b_poseKnown = false;
    TEST_ASSERT_TRUE(r.click(false) & TC::REFUSED);
    r.in.b_poseKnown = true;
    //asked; a left click does not confirm (owner: a snap needs the hold, like an overwrite), a 1 s left hold does
    TEST_ASSERT_TRUE(r.click(false) & TC::ASKED);
    TEST_ASSERT_TRUE(r.c.view(r.t).question == Q::AT);
    TEST_ASSERT_EQUAL_UINT8(3, r.c.view(r.t).questionMark);
    TEST_ASSERT_FALSE(r.click(false) & TC::SNAP);
    TEST_ASSERT_TRUE(r.c.isAsking());
    TEST_ASSERT_TRUE(r.hold(false) & TC::SNAP);
    TEST_ASSERT_EQUAL_UINT8(3, r.c.snapMark());
    TEST_ASSERT_FALSE(r.c.isAsking());
    //3 s without an answer: gone, a late click asks again instead
    r.click(false);
    r.at(r.t + 2900);
    TEST_ASSERT_TRUE(r.c.isAsking());
    TEST_ASSERT_TRUE(r.at(r.t + 200) & TC::TIMED_OUT);
    TEST_ASSERT_FALSE(r.click(false) & TC::SNAP);
    TEST_ASSERT_TRUE(r.c.isAsking());
    //a pushbutton cancels and is spent (no page change)
    uint32_t e = r.pushbutton();
    TEST_ASSERT_TRUE((e & TC::CANCELLED) && (e & TC::BUTTON_SPENT));
    TEST_ASSERT_FALSE(r.c.isAsking());
    //the right stick button cancels, and its release stores nothing
    r.click(false);
    e = r.click(true);
    TEST_ASSERT_TRUE(e & TC::CANCELLED || !r.c.isAsking());
    TEST_ASSERT_FALSE(e & (TC::STORED | TC::ASKED));
    TEST_ASSERT_FALSE(r.c.isAsking());
    //a stick flicked cancels (and does not also select)
    r.click(false);
    TEST_ASSERT_TRUE(r.flickLeft(0.0f, -1.0f) & TC::CANCELLED);
    TEST_ASSERT_EQUAL_INT8(3, r.c.getSelected());
    //no pushbutton spent without a question
    TEST_ASSERT_FALSE(r.pushbutton() & TC::BUTTON_SPENT);
}

void test_edit_store_save_overwrite_needs_the_hold(){
    Rig r;
    //Z and nothing: refused
    TEST_ASSERT_TRUE(r.click(true) & TC::REFUSED);
    r.select(0);
    TEST_ASSERT_TRUE(r.click(true) & TC::REFUSED);
    r.select(2);
    TEST_ASSERT_TRUE(r.click(true) & TC::ASKED);
    TEST_ASSERT_TRUE(r.c.view(r.t).question == Q::SAVE);
    uint32_t e = r.click(true);
    TEST_ASSERT_TRUE((e & TC::STORED) && (e & TC::MARKS_CHANGED));
    TEST_ASSERT_EQUAL_INT32(1000, r.marks.get(2).xMm);
    TEST_ASSERT_EQUAL_INT32(2000, r.marks.get(2).yMm);
    TEST_ASSERT_EQUAL_INT16(300, r.marks.get(2).headingTenths);
    TEST_ASSERT_FALSE(r.marks.get(2).b_time);                       //the stopwatch not running
    //a used mark: OVERWRITE; a click does not confirm
    r.in.x = 5.0f;
    r.click(true);
    TEST_ASSERT_TRUE(r.c.view(r.t).question == Q::OVERWRITE);
    TEST_ASSERT_FALSE(r.click(true) & TC::STORED);
    TEST_ASSERT_TRUE(r.c.isAsking());
    TEST_ASSERT_EQUAL_INT32(1000, r.marks.get(2).xMm);
    //the hold confirms, its progress shown while held
    r.in.b_rightHeld = true;
    r.step();
    r.at(r.t + 500);
    TEST_ASSERT_FLOAT_WITHIN(0.05f, 0.5f, r.c.view(r.t).holdProgress);
    uint32_t events = 0;
    for(int i = 0; i < 30; i++) events |= r.step();
    TEST_ASSERT_TRUE(events & TC::STORED);
    TEST_ASSERT_EQUAL_INT32(5000, r.marks.get(2).xMm);
    r.in.b_rightHeld = false;
    TEST_ASSERT_EQUAL_UINT32(0, r.step());                          //the release does nothing more
    //a hold started late in the 3 s still confirms past them
    r.in.x = 6.0f;
    uint32_t asked = r.t;
    r.click(true);
    r.at(asked + 2800);
    r.in.b_rightHeld = true;
    r.step();
    events = 0;
    for(int i = 0; i < 60; i++) events |= r.step();
    r.in.b_rightHeld = false;
    r.step();
    TEST_ASSERT_TRUE(events & TC::STORED);
    TEST_ASSERT_EQUAL_INT32(6000, r.marks.get(2).xMm);
    //the left stick button cancels an overwrite
    r.click(true);
    r.in.b_leftHeld = true;
    TEST_ASSERT_TRUE(r.step() & TC::CANCELLED);
    r.in.b_leftHeld = false;
    TEST_ASSERT_FALSE(r.step() & TC::ASKED);                        //its release is spent
    //no pose at the confirm: refused, nothing stored
    r.select(5);
    r.click(true);
    r.in.b_poseKnown = false;
    TEST_ASSERT_TRUE(r.click(true) & TC::REFUSED);
    TEST_ASSERT_FALSE(r.marks.isUsed(5));
}

void test_edit_teach_in_takes_the_stopwatch_time(){
    Rig r;
    r.select(1);
    r.in.b_watchRunning = true;
    r.in.watchMillis = 12345;
    r.click(true);
    r.click(true);
    TEST_ASSERT_TRUE(r.marks.get(1).b_time);
    TEST_ASSERT_EQUAL_UINT16(123, r.marks.get(1).timeTenths);
    //stopped: no time taken
    r.in.b_watchRunning = false;
    r.select(2);
    r.click(true);
    r.click(true);
    TEST_ASSERT_FALSE(r.marks.get(2).b_time);
}

void test_edit_left_hold_clears_the_trail(){
    Rig r;
    uint32_t e = r.hold(false);
    TEST_ASSERT_TRUE(e & TC::CLEAR_TRAIL);
    TEST_ASSERT_FALSE(e & (TC::ASKED | TC::REFUSED | TC::ZOOM));
}

void test_editor_steps_fields_save_and_leave(){
    Rig r;
    //Z and nothing selected: no editor
    TEST_ASSERT_TRUE(r.hold(true) & TC::REFUSED);
    r.select(0);
    TEST_ASSERT_TRUE(r.hold(true) & TC::REFUSED);
    r.select(4);
    //an empty mark opens on the pose here, no time
    TEST_ASSERT_TRUE(r.hold(true) & TC::EDIT_OPENED);
    TEST_ASSERT_TRUE(r.c.isEditing());
    TrackingControlView v = r.c.view(r.t);
    TEST_ASSERT_EQUAL_INT32(1000, v.draft.xMm);
    TEST_ASSERT_TRUE(v.field == TrackingControlView::Field::X);
    //right stick up: +1 cm on the push, repeats after 400 ms
    r.in.sticks[3] = 1.0f;
    TEST_ASSERT_TRUE(r.step() & TC::STEP);
    TEST_ASSERT_EQUAL_INT32(1010, r.c.view(r.t).draft.xMm);
    r.at(r.t + 390);
    TEST_ASSERT_EQUAL_INT32(1010, r.c.view(r.t).draft.xMm);
    r.at(r.t + 20);
    TEST_ASSERT_EQUAL_INT32(1020, r.c.view(r.t).draft.xMm);
    //held 1.5 s: every 80 ms
    for(uint32_t i = 0; i < 200; i++) r.step(10);
    int32_t before = r.c.view(r.t).draft.xMm;
    r.at(r.t + 800);
    for(uint32_t i = 0; i < 80; i++) r.step(10);
    TEST_ASSERT_INT32_WITHIN(10, before + 10 * 10, r.c.view(r.t).draft.xMm);
    r.in.sticks[3] = 0.0f;
    r.step();
    //right: +1 m
    int32_t x = r.c.view(r.t).draft.xMm;
    r.in.sticks[2] = 1.0f; r.step(); r.in.sticks[2] = 0.0f; r.step();
    TEST_ASSERT_EQUAL_INT32(x + 1000, r.c.view(r.t).draft.xMm);
    //the field: left stick down -> y -> h -> t
    TEST_ASSERT_TRUE(r.flickLeft(0.0f, -1.0f) & TC::FIELD);
    TEST_ASSERT_TRUE(r.c.view(r.t).field == TrackingControlView::Field::Y);
    r.flickLeft(0.0f, -1.0f);
    //h: 30 deg, left: -10 deg, down -1 deg
    r.in.sticks[2] = -1.0f; r.step(); r.in.sticks[2] = 0.0f; r.step();
    r.in.sticks[3] = -1.0f; r.step(); r.in.sticks[3] = 0.0f; r.step();
    TEST_ASSERT_EQUAL_INT16(190, r.c.view(r.t).draft.headingTenths);
    //blank it with the left stick left
    TEST_ASSERT_TRUE(r.flickLeft(-1.0f, 0.0f) & TC::STEP);
    TEST_ASSERT_FALSE(r.c.view(r.t).draft.b_heading);
    //t: blank, a step starts it from 0: +1 s then down below 0 stops at 0 (limit)
    r.flickLeft(0.0f, -1.0f);
    TEST_ASSERT_TRUE(r.c.view(r.t).field == TrackingControlView::Field::T);
    r.in.sticks[2] = 1.0f; r.step(); r.in.sticks[2] = 0.0f; r.step();
    TEST_ASSERT_EQUAL_UINT16(10, r.c.view(r.t).draft.timeTenths);
    for(int i = 0; i < 10; i++){ r.in.sticks[3] = -1.0f; r.step(); r.in.sticks[3] = 0.0f; r.step(); }
    TEST_ASSERT_EQUAL_UINT16(0, r.c.view(r.t).draft.timeTenths);
    r.in.sticks[3] = -1.0f;
    TEST_ASSERT_TRUE(r.step() & TC::STEP_LIMIT);
    r.in.sticks[3] = 0.0f; r.step();
    r.in.sticks[3] = 1.0f; r.step(); r.in.sticks[3] = 0.0f; r.step();
    TEST_ASSERT_FALSE(r.marks.isUsed(4));                           //nothing stored until saved
    //right click saves
    uint32_t e = r.click(true);
    TEST_ASSERT_TRUE((e & TC::EDIT_SAVED) && (e & TC::MARKS_CHANGED));
    TEST_ASSERT_FALSE(r.c.isEditing());
    TEST_ASSERT_TRUE(r.marks.get(4).b_time);
    TEST_ASSERT_EQUAL_UINT16(1, r.marks.get(4).timeTenths);
    TEST_ASSERT_FALSE(r.marks.get(4).b_heading);
    //again, left click leaves without saving
    r.hold(true);
    r.in.sticks[3] = 1.0f; r.step(); r.in.sticks[3] = 0.0f; r.step();
    TEST_ASSERT_TRUE(r.click(false) & TC::EDIT_LEFT);
    TEST_ASSERT_EQUAL_INT32(x + 1000, r.marks.get(4).xMm);
    //a pushbutton leaves without saving, spent
    r.hold(true);
    r.in.sticks[3] = 1.0f; r.step(); r.in.sticks[3] = 0.0f; r.step();
    e = r.pushbutton();
    TEST_ASSERT_TRUE((e & TC::EDIT_LEFT) && (e & TC::BUTTON_SPENT));
    TEST_ASSERT_EQUAL_INT32(x + 1000, r.marks.get(4).xMm);
}

void test_editor_ignores_a_stick_already_out(){
    Rig r;
    r.select(1);
    r.in.b_rightHeld = true;
    r.in.sticks[3] = 0.9f;                                          //out as the editor opens
    r.step();
    for(int i = 0; i < 55; i++) r.step();
    r.in.b_rightHeld = false;
    r.step();
    TEST_ASSERT_TRUE(r.c.isEditing());
    int32_t x = r.c.view(r.t).draft.xMm;
    for(int i = 0; i < 30; i++) r.step();
    TEST_ASSERT_EQUAL_INT32(x, r.c.view(r.t).draft.xMm);
}

void test_editor_clear_needs_two_holds(){
    Rig r;
    r.marks.set(6, Mark::fromPose(1.0f, 1.0f, 0.0f, 1000));
    r.select(6);
    r.hold(true);
    TEST_ASSERT_TRUE(r.c.isEditing());
    TEST_ASSERT_TRUE(r.hold(false) & TC::ASKED);
    TEST_ASSERT_TRUE(r.c.view(r.t).question == Q::CLEAR);
    TEST_ASSERT_TRUE(r.marks.isUsed(6));
    //a click does not clear
    TEST_ASSERT_FALSE(r.click(false) & TC::MARK_CLEARED);
    TEST_ASSERT_TRUE(r.c.isAsking());
    uint32_t e = r.hold(false);
    TEST_ASSERT_TRUE((e & TC::MARK_CLEARED) && (e & TC::MARKS_CHANGED));
    TEST_ASSERT_FALSE(r.marks.isUsed(6));
    TEST_ASSERT_FALSE(r.c.isEditing());
    //cancelled by the right button or the time: the editor stays
    r.marks.set(6, Mark::fromPose(1.0f, 1.0f, 0.0f));
    r.hold(true);
    r.hold(false);
    TEST_ASSERT_TRUE(r.click(true) & TC::CANCELLED);
    TEST_ASSERT_TRUE(r.c.isEditing());
    r.hold(false);
    TEST_ASSERT_TRUE(r.at(r.t + 3100) & TC::TIMED_OUT);
    TEST_ASSERT_TRUE(r.c.isEditing());
    TEST_ASSERT_TRUE(r.marks.isUsed(6));
    //an empty mark: nothing to clear
    r.click(false);
    r.select(7);
    r.hold(true);
    TEST_ASSERT_TRUE(r.hold(false) & TC::REFUSED);
}

void test_drive_mode_clicks_and_holds(){
    Rig r;
    r.in.b_speedOff = false;
    r.step();
    r.marks.set(2, Mark::fromPose(1.0f, 0.0f, NAN));
    r.marks.set(5, Mark::fromPose(2.0f, 0.0f, NAN));
    //right click: play / pause, timed at the press
    uint32_t pressed = r.t + 20;
    TEST_ASSERT_EQUAL_UINT32(TC::PLAY_PAUSE, r.click(true));
    TEST_ASSERT_EQUAL_UINT32(pressed, r.c.pressMillis());
    //left click: zoom, never a question
    TEST_ASSERT_EQUAL_UINT32(TC::ZOOM, r.click(false));
    TEST_ASSERT_FALSE(r.c.isAsking());
    //left hold between runs: the previous stored mark (none -> the last stored, 5), no zoom on its release
    TEST_ASSERT_EQUAL_UINT32(TC::SELECTED, r.hold(false));
    TEST_ASSERT_EQUAL_INT8(5, r.c.getSelected());
    r.hold(false);
    TEST_ASSERT_EQUAL_INT8(2, r.c.getSelected());
    r.hold(false);
    TEST_ASSERT_EQUAL_INT8(0, r.c.getSelected());
    r.hold(false);
    TEST_ASSERT_EQUAL_INT8(5, r.c.getSelected());                   //wraps through Z
    //left hold during a run: rewind
    r.in.b_run = true;
    r.in.runTarget = 5;
    r.step();
    TEST_ASSERT_EQUAL_UINT32(TC::REWIND, r.hold(false));
    r.in.b_run = false;
    r.step();
    //right hold: the next stored mark (from 5: wrapping to Z)
    TEST_ASSERT_EQUAL_UINT32(TC::SELECTED, r.hold(true));
    TEST_ASSERT_EQUAL_INT8(0, r.c.getSelected());
    r.hold(true);
    TEST_ASSERT_EQUAL_INT8(2, r.c.getSelected());
    r.hold(true);
    TEST_ASSERT_EQUAL_INT8(5, r.c.getSelected());
    r.hold(true);
    TEST_ASSERT_EQUAL_INT8(0, r.c.getSelected());
    //during a run the selection is its target and the hold does nothing
    r.in.b_run = true;
    r.in.runTarget = 5;
    r.step();
    TEST_ASSERT_EQUAL_INT8(5, r.c.getSelected());
    TEST_ASSERT_EQUAL_UINT32(0, r.hold(true));
    TEST_ASSERT_EQUAL_INT8(5, r.c.getSelected());
    //the left stick does not select or switch views in drive mode
    r.in.b_run = false;
    r.flickLeft(0.0f, -1.0f);
    r.flickLeft(1.0f, 0.0f);
    TEST_ASSERT_EQUAL_INT8(5, r.c.getSelected());
    TEST_ASSERT_TRUE(r.c.getView() == TrackingControlView::View::MAP);
}

void test_modes_switch_cancel_and_inactive_does_nothing(){
    Rig r;
    r.marks.set(1, Mark::fromPose(0.0f, 0.0f, 0.0f));
    r.select(1);
    r.click(false);
    TEST_ASSERT_TRUE(r.c.isAsking());
    //the switch to SLOW: the question goes, and the left click is a zoom now
    r.in.b_speedOff = false;
    r.step();
    TEST_ASSERT_FALSE(r.c.isAsking());
    TEST_ASSERT_EQUAL_UINT32(TC::ZOOM, r.click(false));
    //a press started in one mode does nothing in the other
    r.in.b_rightHeld = true;
    r.step();
    r.in.b_speedOff = true;
    r.step();
    r.in.b_rightHeld = false;
    TEST_ASSERT_EQUAL_UINT32(0, r.step());
    //the editor closes when the switch leaves OFF
    r.hold(true);
    TEST_ASSERT_TRUE(r.c.isEditing());
    r.in.b_speedOff = false;
    r.step();
    TEST_ASSERT_FALSE(r.c.isEditing());
    //inactive (another page, the menu): nothing at all
    r.in.b_active = false;
    TEST_ASSERT_EQUAL_UINT32(0, r.click(true));
    TEST_ASSERT_EQUAL_UINT32(0, r.hold(false));
    TEST_ASSERT_EQUAL_UINT32(0, r.flickLeft(0.0f, 1.0f));
}

//——— timed targets ———

void test_profile_trapezoid_with_one_second_ramps(){
    TargetPlan p;
    TEST_ASSERT_TRUE(p.plan(0, 0, 0, 0, 4, NAN, 5.0f));
    //peak 1 / (5 - 1) = 0.25 of the move per second, reached after 1 s
    TEST_ASSERT_FLOAT_WITHIN(1e-6f, 0.0f, p.fraction(0.0f));
    TEST_ASSERT_FLOAT_WITHIN(1e-6f, 0.0f, p.fraction(-1.0f));
    TEST_ASSERT_FLOAT_WITHIN(1e-5f, 0.03125f, p.fraction(0.5f));       //0.5 * 0.25 * 0.5^2
    TEST_ASSERT_FLOAT_WITHIN(1e-5f, 0.125f, p.fraction(1.0f));
    TEST_ASSERT_FLOAT_WITHIN(1e-5f, 0.5f, p.fraction(2.5f));
    TEST_ASSERT_FLOAT_WITHIN(1e-5f, 0.125f, p.fraction(2.0f) - p.fraction(1.5f));
    TEST_ASSERT_FLOAT_WITHIN(1e-5f, 0.875f, p.fraction(4.0f));
    TEST_ASSERT_EQUAL_FLOAT(1.0f, p.fraction(5.0f));                    //ends at T exactly
    TEST_ASSERT_EQUAL_FLOAT(1.0f, p.fraction(7.0f));
    //the speed: constant in the middle, ramping at both ends
    float v1 = (p.fraction(2.0f) - p.fraction(1.9f)) / 0.1f, v2 = (p.fraction(3.1f) - p.fraction(3.0f)) / 0.1f;
    TEST_ASSERT_FLOAT_WITHIN(1e-4f, 0.25f, v1);
    TEST_ASSERT_FLOAT_WITHIN(1e-4f, 0.25f, v2);
    //monotonic, and the last step before T small (decelerating to 0)
    float last = 0.0f;
    for(int i = 1; i <= 500; i++){
        float f = p.fraction(float(i) * 0.01f);
        TEST_ASSERT_TRUE(f >= last);
        last = f;
    }
    TEST_ASSERT_TRUE(1.0f - p.fraction(4.99f) < 1e-4f);
    //the ghost: along the line
    float x, y, h;
    TEST_ASSERT_TRUE(p.ghostAt(2.5f, x, y, h));
    TEST_ASSERT_FLOAT_WITHIN(1e-4f, 0.0f, x);
    TEST_ASSERT_FLOAT_WITHIN(1e-4f, 2.0f, y);
    //no plan without a duration or a start
    TEST_ASSERT_FALSE(p.plan(0, 0, 0, 0, 4, NAN, 0.0f));
    TEST_ASSERT_FALSE(p.isValid());
    TEST_ASSERT_FALSE(p.plan(NAN, 0, 0, 0, 4, NAN, 3.0f));
}

void test_profile_triangle_below_two_seconds(){
    TargetPlan p;
    p.plan(0, 0, 0, 1, 0, NAN, 1.5f);
    //half T each way: peak 1 / 0.75 at 0.75 s
    TEST_ASSERT_FLOAT_WITHIN(1e-5f, 0.5f, p.fraction(0.75f));
    TEST_ASSERT_FLOAT_WITHIN(1e-5f, 0.5f * (1.0f / 0.75f / 0.75f) * 0.25f, p.fraction(0.5f));
    TEST_ASSERT_EQUAL_FLOAT(1.0f, p.fraction(1.5f));
    float last = 0.0f;
    for(int i = 1; i <= 150; i++){
        float f = p.fraction(float(i) * 0.01f);
        TEST_ASSERT_TRUE(f >= last);
        last = f;
    }
    //symmetric
    TEST_ASSERT_FLOAT_WITHIN(1e-5f, 1.0f - p.fraction(0.3f), p.fraction(1.2f));
    //exactly 2 s: ramps of 1 s, no constant part
    p.plan(0, 0, 0, 1, 0, NAN, 2.0f);
    TEST_ASSERT_FLOAT_WITHIN(1e-5f, 0.5f, p.fraction(1.0f));
    TEST_ASSERT_EQUAL_FLOAT(1.0f, p.fraction(2.0f));
}

void test_profile_inverse(){
    TargetPlan p;
    float durations[3] = {5.0f, 1.5f, 12.3f};
    for(float T : durations){
        p.plan(0, 0, 0, 3, 0, NAN, T);
        TEST_ASSERT_EQUAL_FLOAT(0.0f, p.timeAt(0.0f));
        TEST_ASSERT_EQUAL_FLOAT(T, p.timeAt(1.0f));
        TEST_ASSERT_EQUAL_FLOAT(T, p.timeAt(1.5f));         //clamped
        TEST_ASSERT_EQUAL_FLOAT(0.0f, p.timeAt(-0.2f));
        float last = 0.0f;
        for(int i = 1; i < 100; i++){
            float s = float(i) * 0.01f;
            float t = p.timeAt(s);
            TEST_ASSERT_TRUE(t > last);
            last = t;
            TEST_ASSERT_FLOAT_WITHIN(1e-4f, s, p.fraction(t));
        }
        TEST_ASSERT_FLOAT_WITHIN(1e-4f, T * 0.5f, p.timeAt(0.5f));
    }
}

void test_ghost_heading_shortest_turn_and_blank(){
    TargetPlan p;
    //170 -> -170: 20 deg counter-clockwise through 180
    p.plan(0, 0, 170.0f, 0, 4, -170.0f, 6.0f);
    float x, y, h;
    p.ghostAt(0.0f, x, y, h);
    TEST_ASSERT_FLOAT_WITHIN(1e-3f, 170.0f, h);
    p.ghostAt(3.0f, x, y, h);
    TEST_ASSERT_FLOAT_WITHIN(1e-3f, 2.0f, y);
    TEST_ASSERT_FLOAT_WITHIN(1e-3f, 180.0f, fabsf(h));
    p.ghostAt(6.0f, x, y, h);
    TEST_ASSERT_FLOAT_WITHIN(1e-3f, -170.0f, h);
    TEST_ASSERT_FLOAT_WITHIN(1e-4f, 4.0f, y);
    //0 -> 90 follows the profile: a quarter of the way at the time the plan is a quarter along
    p.plan(0, 0, 0.0f, 0, 4, 90.0f, 6.0f);
    p.ghostAt(p.timeAt(0.25f), x, y, h);
    TEST_ASSERT_FLOAT_WITHIN(1e-3f, 1.0f, y);
    TEST_ASSERT_FLOAT_WITHIN(1e-2f, 22.5f, h);
    //a blank heading: the start's all the way
    p.plan(1, 1, 30.0f, 3, 1, NAN, 4.0f);
    for(float t = 0.0f; t <= 4.0f; t += 0.5f){
        p.ghostAt(t, x, y, h);
        TEST_ASSERT_FLOAT_WITHIN(1e-4f, 30.0f, h);
        TEST_ASSERT_FLOAT_WITHIN(1e-4f, 1.0f, y);
    }
    TEST_ASSERT_TRUE(isnan(p.targetHeading()));
}

//a run rig: the marks, the stopwatch and the run as the board has them
struct RunRig{
    MarkStore marks;
    StopwatchControl watch;
    TargetRun run{watch};
    RunRig(){
        marks.set(3, Mark::fromPose(0.0f, 4.0f, 90.0f, 8000));    //4 m ahead, facing left, in 8 s
        marks.set(4, Mark::fromPose(2.0f, 0.0f, NAN));            //no duration
        marks.set(5, Mark::fromPose(0.0f, 0.0f, NAN, 0));         //0.0 s
        marks.set(6, Mark::fromPose(1.0f, 1.0f, NAN, 3000));      //blank heading, 3 s
    }
    TargetRun::Result play(int8_t k, float x, float y, float h, uint32_t now, bool b_pose = true){
        return run.click(marks, k, b_pose, x, y, h, now - 150, now);
    }
};

//the turn tracked apart (owner 2026-10-28): the planned heading at the elapsed time against the rover's
void test_turn_error_tracked_apart_from_translation(){
    RunRig r;
    TEST_ASSERT_EQUAL_UINT8(TargetRun::STARTED, r.play(3, 0, 0, 0, 1000));      //0 -> 90 deg over 8 s
    const TargetPlan& p = r.run.getPlan();
    TEST_ASSERT_TRUE(p.turns());
    float x, y, h;
    p.ghostAt(4.0f, x, y, h);
    //on the line on time, the heading still at 0: turn more to the left (positive), as far as the plan has turned
    r.run.update(true, x, y, 0.0f, 5000);
    TrackingRunView v;
    r.run.view(v, 5000);
    TEST_ASSERT_FLOAT_WITHIN(0.5f, h, v.turnError);
    TEST_ASSERT_TRUE(v.turnError > Pace::turnBand);
    TEST_ASSERT_TRUE(Pace::classify(v.delta) == Pace::Word::ON_TIME);         //the translation is on time
    char t[12];
    Pace::turn(t, sizeof(t), v.turnError);
    TEST_ASSERT_EQUAL_CHAR('\x02', t[0]);                                     //counter-clockwise
    //turned too far: negative, the clockwise glyph
    r.run.update(true, x, y, h + 20.0f, 5000);
    r.run.view(v, 5000);
    TEST_ASSERT_FLOAT_WITHIN(0.5f, -20.0f, v.turnError);
    Pace::turn(t, sizeof(t), v.turnError);
    TEST_ASSERT_EQUAL_CHAR('\x04', t[0]);
    //within 5 deg: no cue
    r.run.update(true, x, y, h + 4.0f, 5000);
    r.run.view(v, 5000);
    Pace::turn(t, sizeof(t), v.turnError);
    TEST_ASSERT_EQUAL_CHAR(0, t[0]);
    //a move without a turn (blank heading): no turn error at all
    RunRig n;
    TEST_ASSERT_EQUAL_UINT8(TargetRun::STARTED, n.play(6, 0, 0, 0, 1000));
    n.run.update(true, 0.1f, 0.1f, 30.0f, 2000);
    n.run.view(v, 2000);
    TEST_ASSERT_TRUE(isnan(v.turnError));
}

void test_pace_delta_at_the_band_edges(){
    RunRig r;
    TEST_ASSERT_EQUAL_UINT8(TargetRun::STARTED, r.play(3, 0, 0, 0, 1000));
    const TargetPlan& p = r.run.getPlan();
    //at 4 s the rover where the plan was at 3.7 s: 0.3 s late, still on time
    float x, y, h;
    p.ghostAt(3.7f, x, y, h);
    r.run.update(true, x, y, h, 5000);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.3f, r.run.getDelta());
    TEST_ASSERT_TRUE(Pace::classify(0.3f) == Pace::Word::ON_TIME);
    TEST_ASSERT_TRUE(Pace::classify(-0.3f) == Pace::Word::ON_TIME);
    TEST_ASSERT_TRUE(Pace::classify(0.31f) == Pace::Word::FASTER);      //late: drive faster
    TEST_ASSERT_TRUE(Pace::classify(-0.31f) == Pace::Word::SLOWER);
    //where the plan is at 4.5 s: 0.5 s early
    p.ghostAt(4.5f, x, y, h);
    r.run.update(true, x, y, h, 5000);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, -0.5f, r.run.getDelta());
    //standing at the start: the whole elapsed time late
    r.run.update(true, 0, 0, 0, 5000);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 4.0f, r.run.getDelta());
    //behind the start: clamped to the start, never negative progress
    r.run.update(true, 0, -1.0f, 0, 5000);
    TEST_ASSERT_FLOAT_WITHIN(1e-4f, 0.0f, r.run.getFix().fraction);
    TEST_ASSERT_FALSE(Pace::blinks(2.0f));
    TEST_ASSERT_TRUE(Pace::blinks(2.01f));
    TEST_ASSERT_TRUE(Pace::blinks(-2.5f));
    //the pill: a word only, no numbers; nothing on time or without a reading
    TEST_ASSERT_EQUAL_STRING("FASTER", Pace::word(0.8f));
    TEST_ASSERT_EQUAL_STRING("SLOWER", Pace::word(-0.5f));
    TEST_ASSERT_EQUAL_STRING("FASTER", Pace::word(12.3f));
    TEST_ASSERT_NULL(Pace::word(0.3f));
    TEST_ASSERT_NULL(Pace::word(-0.3f));
    TEST_ASSERT_NULL(Pace::word(NAN));
}

//the progress is the projection on the line: a sideways offset does not change it, clamped at both ends
void test_progress_is_the_projection_on_the_line(){
    TargetPlan p;
    p.plan(0, 0, 0.0f, 0, 4, 90.0f, 8.0f);
    PaceFix f = p.locate(0.2f, 1.0f, 0.0f);
    TEST_ASSERT_TRUE(f.b_valid);
    TEST_ASSERT_FLOAT_WITHIN(1e-4f, 0.25f, f.fraction);
    TEST_ASSERT_FLOAT_WITHIN(1e-4f, p.timeAt(0.25f), f.plannedSeconds);
    TEST_ASSERT_FLOAT_WITHIN(1e-4f, 0.25f, p.locate(-0.7f, 1.0f, 30.0f).fraction);
    TEST_ASSERT_FLOAT_WITHIN(1e-4f, 1.0f, p.locate(0.0f, 5.0f, 0.0f).fraction);
    TEST_ASSERT_FLOAT_WITHIN(1e-4f, 8.0f, p.locate(0.0f, 5.0f, 0.0f).plannedSeconds);
    TEST_ASSERT_FALSE(p.locate(NAN, 1.0f, 0.0f).b_valid);
    //the other way along the line
    p.plan(0, 4, 180.0f, 0, 0, 180.0f, 8.0f);
    TEST_ASSERT_FLOAT_WITHIN(1e-4f, 0.75f, p.locate(0.3f, 1.0f, 180.0f).fraction);
}

void test_arrival_by_radius_and_by_passing(){
    TargetPlan p;
    p.plan(0, 0, 0.0f, 0, 4, NAN, 8.0f);
    TEST_ASSERT_TRUE(p.reached(0.1f, 3.9f, 0.0f));              //0.14 m
    TEST_ASSERT_FALSE(p.reached(0.0f, 3.84f, 0.0f));            //0.16 m short
    TEST_ASSERT_FALSE(p.reached(0.5f, 3.9f, 0.0f));             //beside it, not past
    TEST_ASSERT_TRUE(p.reached(0.5f, 4.05f, 0.0f));             //past the square line, 0.5 m off
    TEST_ASSERT_TRUE(p.reached(-0.9f, 4.1f, 0.0f));
    TEST_ASSERT_FALSE(p.reached(1.2f, 4.1f, 0.0f));             //past it but more than 1 m away
    TEST_ASSERT_FALSE(p.reached(NAN, 4.0f, 0.0f));
    //the heading does not count for a move
    TEST_ASSERT_TRUE(p.reached(0.0f, 4.0f, 120.0f));
    //a turn on the spot (started within 0.15 m): the heading within 5 deg, or past it
    p.plan(0.05f, 3.95f, 0.0f, 0, 4, 90.0f, 3.0f);
    TEST_ASSERT_TRUE(p.isTurnOnly());
    TEST_ASSERT_FALSE(p.reached(0.0f, 4.0f, 80.0f));
    TEST_ASSERT_TRUE(p.reached(0.0f, 4.0f, 86.0f));
    TEST_ASSERT_TRUE(p.reached(0.0f, 4.0f, 100.0f));
    PaceFix f = p.locate(0.0f, 4.0f, 45.0f);
    TEST_ASSERT_FLOAT_WITHIN(1e-4f, 0.5f, f.fraction);
}

void test_run_ends_at_the_arrival_and_stops_the_stopwatch(){
    RunRig r;
    r.play(3, 0, 0, 0, 1000);
    TEST_ASSERT_TRUE(r.run.isRunning());
    TEST_ASSERT_EQUAL_INT8(3, r.run.target());
    TEST_ASSERT_TRUE(r.watch.stopwatch().isRunning());
    TEST_ASSERT_EQUAL_UINT32(0, r.watch.stopwatch().elapsed(1000));      //restarted from 0:00 at the play
    TrackingRunView v;
    r.run.update(true, 0, 2.0f, 0, 5000);
    r.run.view(v, 5000);
    TEST_ASSERT_TRUE(v.b_run && !v.b_arrived && !v.b_paused);
    TEST_ASSERT_EQUAL_UINT8(3, v.target);
    TEST_ASSERT_FLOAT_WITHIN(1e-4f, 4.0f, v.timeLeft);
    TEST_ASSERT_FLOAT_WITHIN(1e-4f, 0.0f, v.startY);                  //the dotted line from the play's pose
    TEST_ASSERT_FLOAT_WITHIN(1e-4f, 4.0f, v.targetY);
    TEST_ASSERT_TRUE(v.b_ghost);
    TEST_ASSERT_FLOAT_WITHIN(1e-3f, 2.0f, v.ghostY);
    TEST_ASSERT_FLOAT_WITHIN(1e-2f, 45.0f, v.ghostHeading);
    //arrives 0.4 s late
    TEST_ASSERT_FALSE(r.run.update(true, 0, 3.5f, 0, 9000));
    TEST_ASSERT_TRUE(r.run.update(true, 0.05f, 3.9f, 80.0f, 9400));
    TEST_ASSERT_TRUE(r.run.getState() == TargetRun::State::ARRIVED);
    TEST_ASSERT_FALSE(r.run.isRunning());
    TEST_ASSERT_EQUAL_INT8(-1, r.run.target());
    TEST_ASSERT_TRUE(r.watch.stopwatch().getState() == Stopwatch::State::STOPPED);
    TEST_ASSERT_EQUAL_UINT32(8400, r.watch.stopwatch().elapsed(20000));  //the run's time stays shown
    TEST_ASSERT_EQUAL_UINT8(3, r.run.getSplit().mark);
    TEST_ASSERT_FLOAT_WITHIN(1e-3f, 0.4f, r.run.getSplit().delta);
    char text[16];
    Pace::split(text, sizeof(text), r.run.getSplit());
    TEST_ASSERT_EQUAL_STRING("+0.4s", text);
    //the split shows for 3 s, then the plain map; the stopwatch stays stopped; no more arrivals
    TEST_ASSERT_FALSE(r.run.update(true, 0, 4.0f, 90.0f, 10000));
    r.run.view(v, 12399);
    TEST_ASSERT_TRUE(v.b_arrived && !v.b_run && !v.b_ghost);
    r.run.view(v, 12400);
    TEST_ASSERT_FALSE(v.b_arrived || v.b_run);
    TEST_ASSERT_TRUE(r.watch.stopwatch().getState() == Stopwatch::State::STOPPED);
    TEST_ASSERT_EQUAL_UINT32(8400, r.watch.stopwatch().elapsed(30000));
    r.run.acknowledge();
    TEST_ASSERT_TRUE(r.run.getState() == TargetRun::State::IDLE);
    TEST_ASSERT_EQUAL_UINT32(8400, r.watch.stopwatch().elapsed(30000));
    //early: "-0.6s"
    RunSplit early;
    early.delta = -0.6f;
    Pace::split(text, sizeof(text), early);
    TEST_ASSERT_EQUAL_STRING("-0.6s", text);
    //an arrival shown ends too when the stopwatch is reset or started again on Status
    r.play(6, 0, 0, 0, 30000);
    r.run.update(true, 1.0f, 1.0f, 0, 32000);
    TEST_ASSERT_TRUE(r.run.getState() == TargetRun::State::ARRIVED);
    r.watch.playPause(33000);                                           //Status: start
    r.run.update(true, 1.0f, 1.0f, 0, 33020);
    TEST_ASSERT_TRUE(r.run.getState() == TargetRun::State::IDLE);
}

void test_pause_and_resume_freeze_the_plan(){
    RunRig r;
    r.play(3, 0, 0, 0, 1000);
    TrackingRunView a, b;
    r.run.update(true, 0, 1.0f, 0, 3000);
    float delta = r.run.getDelta();
    //the right click during the run: paused at its press
    TEST_ASSERT_EQUAL_UINT8(TargetRun::PAUSED, r.run.click(r.marks, 3, true, 0, 1.0f, 0, 3000, 3150));
    TEST_ASSERT_TRUE(r.run.isRunning() && r.run.isPaused());
    r.run.view(a, 3150);
    //a long pause: the ghost and the pace stay; the rover moving meanwhile changes nothing and arrives nowhere
    TEST_ASSERT_FALSE(r.run.update(true, 0, 4.0f, 90.0f, 9000));
    r.run.view(b, 9000);
    TEST_ASSERT_EQUAL_FLOAT(a.ghostY, b.ghostY);
    TEST_ASSERT_EQUAL_FLOAT(a.ghostHeading, b.ghostHeading);
    TEST_ASSERT_EQUAL_FLOAT(delta, r.run.getDelta());
    TEST_ASSERT_TRUE(b.b_paused);
    TEST_ASSERT_FLOAT_WITHIN(1e-4f, 6.0f, b.timeLeft);
    //resumed: the plan goes on from 2 s
    TEST_ASSERT_EQUAL_UINT8(TargetRun::RESUMED, r.run.click(r.marks, 3, true, 0, 1.0f, 0, 10000, 10100));
    r.run.view(b, 11000);
    float x, y, h;
    r.run.getPlan().ghostAt(3.0f, x, y, h);
    TEST_ASSERT_FLOAT_WITHIN(1e-4f, y, b.ghostY);
    TEST_ASSERT_FALSE(b.b_paused);
    //the stopwatch stopped on Status pauses the run too, started there resumes it
    r.watch.playPause(12000);
    TEST_ASSERT_TRUE(r.run.isPaused());
    r.watch.playPause(13000);
    TEST_ASSERT_FALSE(r.run.isPaused());
}

void test_rewind_cancels_and_the_next_play_plans_again(){
    RunRig r;
    r.play(3, 0, 0, 0, 1000);
    r.run.update(true, 0, 1.0f, 0, 3000);
    r.run.rewind();
    TEST_ASSERT_TRUE(r.run.getState() == TargetRun::State::IDLE);
    TEST_ASSERT_TRUE(r.watch.stopwatch().getState() == Stopwatch::State::IDLE);
    TEST_ASSERT_EQUAL_INT8(-1, r.run.target());
    TrackingRunView v;
    r.run.view(v, 3100);
    TEST_ASSERT_FALSE(v.b_run || v.b_ghost);
    //the next play starts from the pose then
    r.play(3, 0, 2.0f, 0, 5000);
    float x, y, h;
    r.run.getPlan().ghostAt(0.0f, x, y, h);
    TEST_ASSERT_FLOAT_WITHIN(1e-4f, 2.0f, y);
    TEST_ASSERT_FLOAT_WITHIN(1e-4f, 8.0f, r.run.getPlan().duration());
    //the stopwatch reset on Status (or its countdown edited) cancels the run as well
    r.watch.rewind();
    r.run.update(true, 0, 2.5f, 0, 6000);
    TEST_ASSERT_FALSE(r.run.isRunning());
    //with no run, starting the stopwatch on Status starts no run
    r.watch.playPause(7000);
    r.run.update(true, 0, 2.5f, 0, 8000);
    TEST_ASSERT_TRUE(r.run.getState() == TargetRun::State::IDLE);
    TEST_ASSERT_TRUE(r.watch.stopwatch().isRunning());
}

void test_play_refusals(){
    RunRig r;
    r.watch.playPause(500);                                             //the stopwatch running from Status
    char text[24];
    struct Case{ int8_t k; float x, y, h; bool b_pose; RunRefusal why; const char* label; };
    Case cases[] = {
        {-1, 0, 0, 0, true, RunRefusal::NO_MARK, "SELECT A MARK"},
        {7, 0, 0, 0, true, RunRefusal::NO_MARK, "SELECT A MARK"},      //empty
        {0, 1, 1, 0, true, RunRefusal::ZERO, "ZERO HAS NO TIME"},
        {4, 0, 0, 0, true, RunRefusal::NO_TIME, "4 HAS NO TIME"},
        {5, 1, 1, 0, true, RunRefusal::NO_TIME, "5 HAS NO TIME"},      //0.0 s
        {3, 0, 0, 0, false, RunRefusal::NO_POSE, "NO POSITION"},
        {3, 0.1f, 3.9f, 87.0f, true, RunRefusal::ALREADY_THERE, "ALREADY AT 3"},
        {6, 1.05f, 0.9f, 140.0f, true, RunRefusal::ALREADY_THERE, "ALREADY AT 6"},  //a blank heading: the place only
    };
    uint32_t now = 1000;
    for(const Case& c : cases){
        TEST_ASSERT_EQUAL_UINT8(TargetRun::REFUSED, r.play(c.k, c.x, c.y, c.h, now, c.b_pose));
        TEST_ASSERT_TRUE(r.run.refusal(now) == c.why);
        TrackingRunView v;
        r.run.view(v, now + 100);
        Pace::refusal(text, sizeof(text), v.refusal, v.refusalMark);
        TEST_ASSERT_EQUAL_STRING(c.label, text);
        TEST_ASSERT_FALSE(r.run.isRunning());
        TEST_ASSERT_TRUE(r.watch.stopwatch().isRunning());              //untouched
        TEST_ASSERT_EQUAL_UINT32(now - 500, r.watch.stopwatch().elapsed(now));
        now += 2000;
    }
    //the refusal shows for 1.5 s
    TEST_ASSERT_TRUE(r.run.refusal(now - 2000 + 1499) == RunRefusal::ALREADY_THERE);
    TEST_ASSERT_TRUE(r.run.refusal(now - 2000 + 1500) == RunRefusal::NONE);
    //there but turned 30 deg: a turn on the spot is played
    TEST_ASSERT_EQUAL_UINT8(TargetRun::STARTED, r.play(3, 0.1f, 3.9f, 60.0f, now));
    TEST_ASSERT_TRUE(r.run.getPlan().isTurnOnly());
    TEST_ASSERT_TRUE(r.run.refusal(now) == RunRefusal::NONE);
    TEST_ASSERT_TRUE(r.run.update(true, 0.1f, 3.9f, 88.0f, now + 2000));
}

//teach-in: the mark stored while the stopwatch runs takes its elapsed time as the duration (how long the drive took)
void test_teach_in_takes_the_drive_time(){
    Rig r;
    StopwatchControl watch;
    watch.playPause(2000);                                              //started on Status from 0:00
    r.select(3);
    r.in.b_watchRunning = watch.stopwatch().isRunning();
    r.in.watchMillis = watch.stopwatch().elapsed(10400);
    r.click(true);
    r.click(true);
    TEST_ASSERT_TRUE(r.marks.get(3).b_time);
    TEST_ASSERT_EQUAL_UINT16(84, r.marks.get(3).timeTenths);
    //played back: an 8.4 s plan
    TargetRun run{watch};
    TEST_ASSERT_EQUAL_UINT8(TargetRun::STARTED, run.click(r.marks, 3, true, 5.0f, 5.0f, 0.0f, 20000, 20000));
    TEST_ASSERT_FLOAT_WITHIN(1e-4f, 8.4f, run.getPlan().duration());
}

//the right hold selects the next mark again as soon as the run ended (the board's loop: TrackingControl, TargetRun)
void test_next_mark_hold_after_a_run(){
    Rig r;
    StopwatchControl watch;
    TargetRun run{watch};
    r.marks.set(2, Mark::fromPose(0.0f, 1.0f, NAN, 2000));
    r.marks.set(5, Mark::fromPose(1.0f, 2.0f, NAN, 3000));
    r.in.b_speedOff = false;
    r.in.x = 0.0f; r.in.y = 0.0f; r.in.heading = 0.0f;
    auto loop = [&](uint32_t events){
        if(events & TC::PLAY_PAUSE) run.click(r.marks, r.c.getSelected(), true, r.in.x, r.in.y, r.in.heading, r.c.pressMillis(), r.t);
        if(events & TC::REWIND) run.rewind();
        if(events & TC::SELECTED) run.acknowledge();
        run.update(true, r.in.x, r.in.y, r.in.heading, r.t);
        r.in.b_run = run.isRunning();
        r.in.runTarget = run.target();
        return events;
    };
    loop(r.step());
    loop(r.hold(true));                                                 //Z
    loop(r.hold(true));
    TEST_ASSERT_EQUAL_INT8(2, r.c.getSelected());
    TEST_ASSERT_EQUAL_UINT32(TC::PLAY_PAUSE, loop(r.click(true)));
    TEST_ASSERT_TRUE(run.isRunning());
    //during the run the hold does nothing, the selection is the target
    TEST_ASSERT_EQUAL_UINT32(0, loop(r.hold(true)));
    TEST_ASSERT_EQUAL_INT8(2, r.c.getSelected());
    //arrived: the run ended, nothing selected by itself; the hold works again
    r.in.y = 1.0f;
    loop(r.step());
    TEST_ASSERT_TRUE(run.getState() == TargetRun::State::ARRIVED);
    TEST_ASSERT_FALSE(r.in.b_run);
    TEST_ASSERT_EQUAL_INT8(2, r.c.getSelected());
    TEST_ASSERT_EQUAL_UINT32(TC::SELECTED, loop(r.hold(true)));
    TEST_ASSERT_EQUAL_INT8(5, r.c.getSelected());
    TEST_ASSERT_TRUE(run.getState() == TargetRun::State::IDLE);        //the split acknowledged
    //and the next play goes from here
    loop(r.click(true));
    TEST_ASSERT_TRUE(run.isRunning());
    TEST_ASSERT_EQUAL_INT8(5, run.target());
    //the left hold cancels it, the hold selects again
    loop(r.hold(false));
    TEST_ASSERT_FALSE(run.isRunning());
    loop(r.hold(true));
    TEST_ASSERT_EQUAL_INT8(0, r.c.getSelected());
}

//the time left bottom left: tenths below 10 s, whole seconds from there, rounded up like a countdown
void test_time_left_text(){
    char text[12];
    Pace::timeLeft(text, sizeof(text), 4.2f);
    TEST_ASSERT_EQUAL_STRING("4.2", text);
    Pace::timeLeft(text, sizeof(text), 4.15f);
    TEST_ASSERT_EQUAL_STRING("4.2", text);
    Pace::timeLeft(text, sizeof(text), 9.9f);
    TEST_ASSERT_EQUAL_STRING("9.9", text);
    Pace::timeLeft(text, sizeof(text), 9.95f);
    TEST_ASSERT_EQUAL_STRING("10", text);
    Pace::timeLeft(text, sizeof(text), 12.3f);
    TEST_ASSERT_EQUAL_STRING("13", text);
    Pace::timeLeft(text, sizeof(text), 0.0f);
    TEST_ASSERT_EQUAL_STRING("0.0", text);
    Pace::timeLeft(text, sizeof(text), -2.0f);                          //over time
    TEST_ASSERT_EQUAL_STRING("0.0", text);
    Pace::timeLeft(text, sizeof(text), 5999.9f);
    TEST_ASSERT_EQUAL_STRING("6000", text);
}

//——— the trip readout ———

void test_trip_accumulates_ignores_jitter_and_resets(){
    TripMeter trip;
    uint32_t t = 0;
    //standing, the pose shimmering by 5 mm and 0.3 deg: nothing adds up
    for(int i = 0; i < 500; i++, t += 20) trip.update(0.005f * (i % 2), 0.0f, 0.3f * (i % 2), 0.0f, 0.0f, t);
    TEST_ASSERT_FLOAT_WITHIN(1e-4f, 0.0f, trip.distance());
    TEST_ASSERT_FLOAT_WITHIN(1e-4f, 0.0f, trip.turned());
    TEST_ASSERT_EQUAL_UINT32(0, trip.movingMillis());
    //2 m at 0.5 m/s in 4 s, then turning 90 deg left and 90 right
    for(int i = 1; i <= 200; i++, t += 20) trip.update(0.0f, 0.01f * i, 0.0f, 0.5f, 0.0f, t);
    TEST_ASSERT_FLOAT_WITHIN(0.03f, 2.0f, trip.distance());
    TEST_ASSERT_INT32_WITHIN(40, 4000, int32_t(trip.movingMillis()));
    for(int i = 1; i <= 90; i++, t += 20) trip.update(0.0f, 2.0f, float(i), 0.0f, 45.0f, t);
    for(int i = 89; i >= 0; i--, t += 20) trip.update(0.0f, 2.0f, float(i), 0.0f, -45.0f, t);
    TEST_ASSERT_FLOAT_WITHIN(2.5f, 180.0f, trip.turned());
    TEST_ASSERT_FLOAT_WITHIN(1e-4f, 0.5f, trip.topSpeed());
    //a reply gap counts no time
    trip.update(0.0f, 2.0f, 0.0f, 0.5f, 0.0f, t + 5000);
    TEST_ASSERT_INT32_WITHIN(60, 7600, int32_t(trip.movingMillis()));
    //across the wrap: -179 to 179 is 2 deg
    trip.reset();
    trip.update(0.0f, 0.0f, -179.0f, 0.0f, 0.0f, t);
    trip.update(0.0f, 0.0f, 179.0f, 0.0f, 0.0f, t + 20);
    TEST_ASSERT_FLOAT_WITHIN(1e-3f, 2.0f, trip.turned());
    //a reset: all zero, the next pose the new start (no jump counted)
    trip.reset();
    trip.update(10.0f, 10.0f, 0.0f, 0.0f, 0.0f, t + 40);
    TEST_ASSERT_FLOAT_WITHIN(1e-4f, 0.0f, trip.distance());
    TEST_ASSERT_FLOAT_WITHIN(1e-4f, 0.0f, trip.topSpeed());
    TEST_ASSERT_EQUAL_UINT32(0, trip.movingMillis());
}

//——— the link history ———

void test_link_history_ring_once_a_second(){
    LinkHistory h;
    TEST_ASSERT_EQUAL_UINT8(60, LinkHistory::capacity);
    uint32_t t = 0;
    for(int i = 0; i < 100; i++){
        for(int f = 0; f < 50; f++, t += 20) h.update(t, uint8_t(i), int16_t(-40 - i), i % 10 != 9);
    }
    TEST_ASSERT_EQUAL_UINT8(60, h.size());
    LinkHistoryView v;
    h.view(v);
    TEST_ASSERT_EQUAL_UINT8(60, v.count);
    TEST_ASSERT_EQUAL_UINT8(40, v.loss[0]);             //the oldest: second 40
    TEST_ASSERT_EQUAL_UINT8(99, v.loss[59]);
    TEST_ASSERT_EQUAL_INT8(-80, v.dbm[0]);
    TEST_ASSERT_EQUAL_INT8(LinkHistoryView::noSignal, v.dbm[59]);   //no link in second 99
    //clamped
    LinkHistory c;
    c.update(0, 250, -300, true);
    c.view(v);
    TEST_ASSERT_EQUAL_UINT8(100, v.loss[0]);
    TEST_ASSERT_EQUAL_INT8(-127, v.dbm[0]);
    TEST_ASSERT_FALSE(c.update(999, 0, -50, true));
    TEST_ASSERT_TRUE(c.update(1000, 0, -50, true));
}

//the run is the stopwatch: play / pause from the press, rewind to rest; with a countdown set the run's time still
//counts up (elapsed) while the Status page shows the countdown, and the rewind keeps the set time
void test_run_uses_the_stopwatch_and_its_countdown(){
    StopwatchControl c;
    TEST_ASSERT_EQUAL_UINT16(StopwatchControl::STARTED, c.playPause(1000));
    TEST_ASSERT_EQUAL_UINT32(500, c.stopwatch().elapsed(1500));
    TEST_ASSERT_EQUAL_UINT16(StopwatchControl::STOPPED, c.playPause(3000));
    TEST_ASSERT_EQUAL_UINT32(2000, c.stopwatch().elapsed(9000));            //paused: frozen
    TEST_ASSERT_EQUAL_UINT16(StopwatchControl::STARTED, c.playPause(10000)); //continues
    TEST_ASSERT_EQUAL_UINT32(3000, c.stopwatch().elapsed(11000));
    TEST_ASSERT_EQUAL_UINT16(StopwatchControl::RESET, c.rewind());
    TEST_ASSERT_TRUE(c.stopwatch().getState() == Stopwatch::State::IDLE);
    //a play restarts it from 0:00, the arrival stops it
    c.playPause(11500);
    TEST_ASSERT_EQUAL_UINT16(StopwatchControl::RESET | StopwatchControl::STARTED, c.restart(11600));
    TEST_ASSERT_EQUAL_UINT32(400, c.stopwatch().elapsed(12000));
    TEST_ASSERT_EQUAL_UINT16(StopwatchControl::STOPPED, c.stop(12000));
    TEST_ASSERT_EQUAL_UINT16(StopwatchControl::NONE, c.stop(12500));
    TEST_ASSERT_EQUAL_UINT32(400, c.stopwatch().elapsed(15000));
    c.rewind();
    TEST_ASSERT_EQUAL_UINT32(0, c.stopwatch().elapsed(12000));
    //a countdown of 1:00 set on Status: the run counts up, the screen counts down
    StopwatchControl::Inputs in;
    in.b_active = true;
    in.b_speedOff = true;
    in.b_leftHeld = true;
    in.now = 20000; c.update(in);
    in.now = 20060; c.update(in);                                       //the edit hold
    in.rightX = 1.0f; in.now = 20100; c.update(in);                     //+1 min
    in.rightX = 0.0f; in.b_leftHeld = false; in.now = 20200; c.update(in);
    TEST_ASSERT_EQUAL_UINT32(60000, c.stopwatch().getSetMillis());
    c.playPause(30000);
    TEST_ASSERT_EQUAL_UINT32(5000, c.stopwatch().elapsed(35000));
    TEST_ASSERT_EQUAL_UINT32(55000, c.view(35000).shownMillis);
    c.rewind();
    TEST_ASSERT_EQUAL_UINT32(60000, c.stopwatch().getSetMillis());     //the set time stays
}

int main(){
    UNITY_BEGIN();
    RUN_TEST(test_snap_position_and_heading);
    RUN_TEST(test_snap_blank_heading_keeps_the_heading_shown);
    RUN_TEST(test_zero_and_snap_interplay);
    RUN_TEST(test_marks_quantised_and_z_fixed);
    RUN_TEST(test_marks_eeprom_round_trip);
    RUN_TEST(test_marks_blank_foreign_and_corrupt_blocks);
    RUN_TEST(test_edit_selection_and_views);
    RUN_TEST(test_edit_snap_confirm_timeout_and_cancels);
    RUN_TEST(test_edit_store_save_overwrite_needs_the_hold);
    RUN_TEST(test_edit_teach_in_takes_the_stopwatch_time);
    RUN_TEST(test_edit_left_hold_clears_the_trail);
    RUN_TEST(test_editor_steps_fields_save_and_leave);
    RUN_TEST(test_editor_ignores_a_stick_already_out);
    RUN_TEST(test_editor_clear_needs_two_holds);
    RUN_TEST(test_drive_mode_clicks_and_holds);
    RUN_TEST(test_modes_switch_cancel_and_inactive_does_nothing);
    RUN_TEST(test_profile_trapezoid_with_one_second_ramps);
    RUN_TEST(test_profile_triangle_below_two_seconds);
    RUN_TEST(test_profile_inverse);
    RUN_TEST(test_ghost_heading_shortest_turn_and_blank);
    RUN_TEST(test_pace_delta_at_the_band_edges);
    RUN_TEST(test_progress_is_the_projection_on_the_line);
    RUN_TEST(test_arrival_by_radius_and_by_passing);
    RUN_TEST(test_run_ends_at_the_arrival_and_stops_the_stopwatch);
    RUN_TEST(test_pause_and_resume_freeze_the_plan);
    RUN_TEST(test_rewind_cancels_and_the_next_play_plans_again);
    RUN_TEST(test_play_refusals);
    RUN_TEST(test_teach_in_takes_the_drive_time);
    RUN_TEST(test_next_mark_hold_after_a_run);
    RUN_TEST(test_time_left_text);
    RUN_TEST(test_trip_accumulates_ignores_jitter_and_resets);
    RUN_TEST(test_link_history_ring_once_a_second);
    RUN_TEST(test_run_uses_the_stopwatch_and_its_countdown);
    RUN_TEST(test_turn_error_tracked_apart_from_translation);
    RUN_TEST(test_edit_clear_all_marks_from_the_list);
    return UNITY_END();
}
