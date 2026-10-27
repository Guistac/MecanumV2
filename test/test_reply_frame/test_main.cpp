//Host tests of the receiver's 13 byte reply [D-161, D-163]: pio test -e native
//The E-stop path first: the uplink is unchanged by D-161 and D-163. The receiver's own uplink decoder (src/Utilities/RemoteProxy.h)
//and SafetyNumber are built here and checked against a copy of the decoder as it was before (a4c8220).
#include <unity.h>
#include <string.h>
#include <Arduino.h>        //host stand-in (tools/screens/host)

//the Teensy core's float map() and random(low, high), which the receiver's decoder and SafetyNumber use
template<class T, class A, class B, class C, class D>
static T map(T x, A inMin, B inMax, C outMin, D outMax){ return (x - inMin) * (outMax - outMin) / (inMax - inMin) + outMin; }
static uint32_t randomState = 12345;
inline long random(long low, long high){
    randomState = randomState * 1103515245u + 12345u;
    return low + long((randomState >> 8) % uint32_t(high - low));
}

#include "Utilities/RemoteProxy.h"
#include "Utilities/SafetyNumber.cpp"
#include <UplinkFrame.h>
#include <ReplyFrame.h>

void setUp(){}
void tearDown(){}

using namespace Reply;

//——— references: the code as it was before D-161 (a4c8220), copied ———

//src/Utilities/RemoteProxy.h decodeRemoteFrame at a4c8220
static bool legacyDecodeRemoteFrame(const uint8_t* frame, uint8_t length, RemoteProxy& proxy){
    if(length != 8) return false;
    uint16_t controlword = frame[0] | frame[1] << 8;
    uint8_t header = controlword & 0x7;
    if(header != 0x7) return false;
    proxy.b_shouldDisable = controlword & 0x8;
    proxy.b_shouldEnable = controlword & 0x10;
    proxy.speedSelection = SpeedMode((controlword >> 5) & 0x3);
    if(uint8_t(proxy.speedSelection) == 0x3) proxy.speedSelection = SpeedMode::LOW_SPEED;
    proxy.b_directionToggle = controlword & 0x80;
    proxy.b_leftButton = controlword & 0x100;
    proxy.b_rightButton = controlword & 0x200;
    proxy.b_leftJoystick = controlword & 0x400;
    proxy.b_rightJoystick = controlword & 0x800;
    proxy.txCounter = (controlword >> 12) & 0xF;
    if(frame[2] == 127) proxy.leftJoystickX = 0.0;
    else proxy.leftJoystickX = map((float)frame[2], 0.0, 255.0, -1.0, 1.0);
    if(frame[3] == 127) proxy.leftJoystickY = 0.0;
    else proxy.leftJoystickY = map((float)frame[3], 0.0, 255.0, -1.0, 1.0);
    if(frame[4] == 127) proxy.rightJoystickX = 0.0;
    else proxy.rightJoystickX = map((float)frame[4], 0.0, 255.0, -1.0, 1.0);
    if(frame[5] == 127) proxy.rightJoystickY = 0.0;
    else proxy.rightJoystickY = map((float)frame[5], 0.0, 255.0, -1.0, 1.0);
    uint16_t safetyNumber = frame[7] << 8 | frame[6];
    proxy.b_safetyClear = Safety::isNumberClear(safetyNumber);
    return true;
}

//RemoteBoard::handleReply's acceptance at a4c8220: an old remote takes only 8 bytes with header 0b111
static bool legacyRemoteAccepts(const uint8_t* frame, uint8_t length){
    if(length != 8) return false;
    uint16_t statusword = frame[0] | frame[1] << 8;
    return (statusword & 0x7) == 0x7;
}

static bool sameProxy(const RemoteProxy& a, const RemoteProxy& b){
    return a.b_shouldDisable == b.b_shouldDisable && a.b_shouldEnable == b.b_shouldEnable
        && a.speedSelection == b.speedSelection && a.b_directionToggle == b.b_directionToggle
        && a.b_leftButton == b.b_leftButton && a.b_rightButton == b.b_rightButton
        && a.b_leftJoystick == b.b_leftJoystick && a.b_rightJoystick == b.b_rightJoystick
        && a.leftJoystickX == b.leftJoystickX && a.leftJoystickY == b.leftJoystickY
        && a.rightJoystickX == b.rightJoystickX && a.rightJoystickY == b.rightJoystickY
        && a.b_safetyClear == b.b_safetyClear && a.txCounter == b.txCounter;
}

//——— the uplink: E-stop number and every field decode as before, for both headers ———

void test_uplink_estop_number_every_value_as_before(){
    uint8_t frame[8] = {0x07, 0x30, 127, 127, 127, 127, 0, 0};
    uint32_t clearCount = 0;
    for(uint32_t n = 0; n <= 0xFFFF; n++){
        frame[6] = n & 0xFF;
        frame[7] = n >> 8;
        RemoteProxy legacy, now;
        legacy.b_safetyClear = !Safety::isNumberClear(n);   //start opposite: the decode must write it
        now.b_safetyClear = legacy.b_safetyClear;
        TEST_ASSERT_TRUE(legacyDecodeRemoteFrame(frame, 8, legacy));
        TEST_ASSERT_TRUE(decodeRemoteFrame(frame, 8, now));
        if(legacy.b_safetyClear != now.b_safetyClear){
            char message[48];
            snprintf(message, sizeof(message), "safety number %u", (unsigned)n);
            TEST_FAIL_MESSAGE(message);
        }
        if(now.b_safetyClear) clearCount++;
    }
    //1 in 333 numbers is clear: the E-stop needs a deliberate number, never a stray byte pattern
    TEST_ASSERT_TRUE(clearCount > 150 && clearCount < 250);
}

void test_uplink_generated_numbers(){
    UplinkControls controls;
    for(int i = 0; i < 5000; i++){
        bool b_clear = i % 2 == 0;
        uint16_t number = b_clear ? Safety::generateClearNumber() : Safety::generateSafeNumber();
        uint8_t frame[8];
        Uplink::encode(controls, false, uint8_t(i), number, frame);
        RemoteProxy proxy;
        proxy.b_safetyClear = !b_clear;
        TEST_ASSERT_TRUE(decodeRemoteFrame(frame, 8, proxy));
        TEST_ASSERT_EQUAL(b_clear, proxy.b_safetyClear);
    }
}

void test_uplink_all_fields_as_before(){
    //every 7th control word (all header values) with a spread of stick and safety bytes
    uint8_t sticks[] = {0, 1, 63, 126, 127, 128, 191, 254, 255};
    for(uint32_t word = 0; word <= 0xFFFF; word += 7){
        for(int s = 0; s < 9; s++){
            uint16_t number = uint16_t(word * 2654435761u >> 16);
            uint8_t frame[8] = {uint8_t(word & 0xFF), uint8_t(word >> 8), sticks[s], sticks[(s + 3) % 9],
                                sticks[(s + 5) % 9], sticks[(s + 7) % 9], uint8_t(number & 0xFF), uint8_t(number >> 8)};
            RemoteProxy legacy, now;
            bool b_legacy = legacyDecodeRemoteFrame(frame, 8, legacy);
            bool b_now = decodeRemoteFrame(frame, 8, now);
            TEST_ASSERT_EQUAL(b_legacy, b_now);
            TEST_ASSERT_EQUAL((word & 0x7) == 0x7, b_now);
            if(b_now) TEST_ASSERT_TRUE(sameProxy(legacy, now));
        }
    }
}

void test_uplink_a_reply_is_never_an_uplink(){
    //a 13 byte reply heard by another receiver (same header bits) is rejected by length: no commands, no E-stop number
    RemoteProxy proxy;
    proxy.b_safetyClear = false;
    Telemetry t;
    t.b_valid = true;
    Status s;
    s.page = 3;
    uint8_t frame[13];
    encode(s, t, 0, 0, frame);
    uint16_t clear = 333 * 50 - 33;
    TEST_ASSERT_TRUE(Safety::isNumberClear(clear));
    frame[6] = clear & 0xFF;
    frame[7] = clear >> 8;
    TEST_ASSERT_FALSE(decodeRemoteFrame(frame, 13, proxy));
    for(uint8_t length : {uint8_t(0), uint8_t(4), uint8_t(7), uint8_t(9), uint8_t(12), uint8_t(20)}) TEST_ASSERT_FALSE(decodeRemoteFrame(frame, length, proxy));
    TEST_ASSERT_FALSE(proxy.b_safetyClear);
}

//——— the reply ———

//the old status word (RobotBoard::sendData at a4c8220); D-163 gives it back unchanged (bits 10-11 0, the page in byte 12)
static uint16_t legacyStatusWord(uint8_t state, uint8_t speed, bool b_direction, bool b_function, uint8_t txCounter){
    uint16_t statusword = 0x7;
    statusword |= (state & 0x7) << 3;
    statusword |= (speed & 0x3) << 6;
    if(b_direction) statusword |= 0x100;
    if(b_function) statusword |= 0x200;
    statusword |= (txCounter & 0xF) << 12;
    return statusword;
}

void test_status_word_as_before_and_byte_12(){
    for(uint8_t state = 0; state < 8; state++)
    for(uint8_t speed = 0; speed < 4; speed++)
    for(int flags = 0; flags < 4; flags++)
    for(uint8_t counter = 0; counter < 16; counter++)
    for(uint8_t page = 0; page < 8; page++){
        Status s;
        s.state = state;
        s.speed = speed;
        s.b_direction = flags & 1;
        s.b_function = flags & 2;
        s.txCounter = counter;
        s.page = page;
        s.mavState = (state + counter) & 0xF;
        s.b_armed = counter & 1;
        uint16_t word = statusWord(s);
        //exactly the old word: bits 10-11 are 0 again
        TEST_ASSERT_EQUAL_HEX16(legacyStatusWord(state, speed, s.b_direction, s.b_function, counter), word);
        TEST_ASSERT_EQUAL_HEX16(0, word & 0x0C00);
        uint8_t byte12 = vehicleByte(s);
        TEST_ASSERT_EQUAL_HEX8(s.mavState, byte12 & 0xF);
        TEST_ASSERT_EQUAL(s.b_armed, bool(byte12 & 0x10));
        TEST_ASSERT_EQUAL_UINT8(page, byte12 >> 5);
        Status back = statusFrom(word, byte12);
        TEST_ASSERT_EQUAL_UINT8(state, back.state);
        TEST_ASSERT_EQUAL_UINT8(speed, back.speed);
        TEST_ASSERT_EQUAL(s.b_direction, back.b_direction);
        TEST_ASSERT_EQUAL(s.b_function, back.b_function);
        TEST_ASSERT_EQUAL_UINT8(counter, back.txCounter);
        TEST_ASSERT_EQUAL_UINT8(page, back.page);
        TEST_ASSERT_EQUAL_UINT8(s.mavState, back.mavState);
        TEST_ASSERT_EQUAL(s.b_armed, back.b_armed);
    }
}

void test_conversions_round_clamp_and_nan(){
    TEST_ASSERT_EQUAL_INT16(312, toMilli(0.312f));
    TEST_ASSERT_EQUAL_INT16(-48, toMilli(-0.048f));
    TEST_ASSERT_EQUAL_INT16(1, toMilli(0.0005f));
    TEST_ASSERT_EQUAL_INT16(-1, toMilli(-0.0005f));
    TEST_ASSERT_EQUAL_INT16(0, toMilli(0.0004f));
    TEST_ASSERT_EQUAL_INT16(0, toMilli(NAN));
    TEST_ASSERT_EQUAL_INT16(32767, toMilli(40.0f));
    TEST_ASSERT_EQUAL_INT16(-32767, toMilli(-40.0f));
    TEST_ASSERT_EQUAL_INT16(32767, toMilli(INFINITY));
    TEST_ASSERT_EQUAL_INT16(-32767, toMilli(-INFINITY));
    TEST_ASSERT_EQUAL_INT16(32767, toMilli(32.767f));
    TEST_ASSERT_FLOAT_WITHIN(1e-6f, 0.312f, fromMilli(312));

    TEST_ASSERT_EQUAL_INT8(127, toPercentByte(1.0f));
    TEST_ASSERT_EQUAL_INT8(-127, toPercentByte(-1.0f));
    TEST_ASSERT_EQUAL_INT8(127, toPercentByte(1.7f));
    TEST_ASSERT_EQUAL_INT8(-127, toPercentByte(-3.0f));
    TEST_ASSERT_EQUAL_INT8(127, toPercentByte(INFINITY));
    TEST_ASSERT_EQUAL_INT8(-127, toPercentByte(-INFINITY));
    TEST_ASSERT_EQUAL_INT8(64, toPercentByte(0.5f));
    TEST_ASSERT_EQUAL_INT8(-64, toPercentByte(-0.5f));
    TEST_ASSERT_EQUAL_INT8(0, toPercentByte(0.0f));
    TEST_ASSERT_EQUAL_INT8(0, toPercentByte(0.003f));
    TEST_ASSERT_EQUAL_INT8(1, toPercentByte(0.005f));
    TEST_ASSERT_EQUAL_INT8(noData, toPercentByte(NAN));
    TEST_ASSERT_TRUE(isnan(fromPercentByte(noData)));
    TEST_ASSERT_FLOAT_WITHIN(1e-6f, 1.0f, fromPercentByte(127));
    TEST_ASSERT_FLOAT_WITHIN(1e-6f, -1.0f, fromPercentByte(-127));
    //never noData from a value
    for(float v = -2.0f; v <= 2.0f; v += 0.001f) TEST_ASSERT_TRUE(toPercentByte(v) != noData);
}

static Telemetry driving(){
    Telemetry t;
    t.b_valid = true;
    t.vx = 0.312f;
    t.vy = -0.048f;
    t.vr = 0.1f;
    float velocity[4] = {0.5f, -0.25f, 1.2f, NAN};
    float torque[4] = {0.0f, 0.12f, -2.0f, -0.3f};
    for(int i = 0; i < 4; i++){
        t.wheelVelocity[i] = velocity[i];
        t.wheelTorque[i] = torque[i];
    }
    t.wheelFlags = 0x00FF;      //all enabled and ready
    t.b_poseValid = true;
    t.poseX = 1.25f;            //m
    t.poseY = -40.0f;           //clamped
    t.heading = 1.5707963f;     //90 deg counter-clockwise: 0x4000
    return t;
}

void test_reply_layout_every_page(){
    Telemetry t = driving();
    t.wheelFlags = 0x84F1;      //FL enabled, all ready, RL fault (bit 10), RR STO (bit 15)
    Status s;
    s.state = 4;
    s.speed = 2;
    s.txCounter = 0xB;
    s.mavState = MAV_ACTIVE;
    s.b_armed = true;
    uint8_t frame[13];
    for(uint8_t page = 0; page < 8; page++){
        s.page = page;
        encode(s, t, 0x5A, 0xC3, frame);
        uint16_t word = frame[0] | frame[1] << 8;
        TEST_ASSERT_EQUAL_HEX16(statusWord(s), word);
        TEST_ASSERT_EQUAL_HEX8(0x7, word & 0x7);
        //vx 312 = 0x0138, vy -48 = 0xFFD0, vr 100 mrad/s = 0x0064, little-endian
        TEST_ASSERT_EQUAL_HEX8(0x38, frame[2]);
        TEST_ASSERT_EQUAL_HEX8(0x01, frame[3]);
        TEST_ASSERT_EQUAL_HEX8(0xD0, frame[4]);
        TEST_ASSERT_EQUAL_HEX8(0xFF, frame[5]);
        TEST_ASSERT_EQUAL_HEX8(0x64, frame[6]);
        TEST_ASSERT_EQUAL_HEX8(0x00, frame[7]);
        TEST_ASSERT_EQUAL_HEX8(0x14 | page << 5, frame[12]);     //ACTIVE, armed, the page
        switch(page){
            case 0:
            case 2:
            case 4:
            case 6:
                TEST_ASSERT_EQUAL_INT8(64, int8_t(frame[8]));
                TEST_ASSERT_EQUAL_INT8(-32, int8_t(frame[9]));
                TEST_ASSERT_EQUAL_INT8(127, int8_t(frame[10]));         //clamped
                TEST_ASSERT_EQUAL_INT8(noData, int8_t(frame[11]));      //NaN
                break;
            case 1:
                TEST_ASSERT_EQUAL_INT8(0, int8_t(frame[8]));
                TEST_ASSERT_EQUAL_INT8(15, int8_t(frame[9]));
                TEST_ASSERT_EQUAL_INT8(-127, int8_t(frame[10]));
                TEST_ASSERT_EQUAL_INT8(-38, int8_t(frame[11]));
                break;
            case 3:
                TEST_ASSERT_EQUAL_HEX8(0xF1, frame[8]);
                TEST_ASSERT_EQUAL_HEX8(0x84, frame[9]);
                TEST_ASSERT_EQUAL_HEX8(0x5A, frame[10]);
                TEST_ASSERT_EQUAL_HEX8(0xC3, frame[11]);
                break;
            case 5:
                //x 1250 mm = 0x04E2, y clamped to -32767 = 0x8001
                TEST_ASSERT_EQUAL_HEX8(0xE2, frame[8]);
                TEST_ASSERT_EQUAL_HEX8(0x04, frame[9]);
                TEST_ASSERT_EQUAL_HEX8(0x01, frame[10]);
                TEST_ASSERT_EQUAL_HEX8(0x80, frame[11]);
                break;
            case 7:
                TEST_ASSERT_EQUAL_HEX8(0x00, frame[8]);     //heading 0x4000
                TEST_ASSERT_EQUAL_HEX8(0x40, frame[9]);
                TEST_ASSERT_EQUAL_HEX8(0xF1, frame[10]);    //the flag word again
                TEST_ASSERT_EQUAL_HEX8(0x84, frame[11]);
                break;
        }
        //an old remote never takes it (a mismatched pair shows NO LINK)
        TEST_ASSERT_FALSE(legacyRemoteAccepts(frame, 13));
    }
}

void test_reply_invalid_telemetry_sends_no_motion(){
    Telemetry t = driving();
    t.b_valid = false;      //stale or no Stacato: the values above must not go out
    t.b_poseValid = false;  //the same for the pose
    Status s;
    uint8_t frame[13];
    for(uint8_t page = 0; page < 8; page++){
        s.page = page;
        encode(s, t, 0x11, 0x22, frame);
        for(int i = 2; i < 8; i++) TEST_ASSERT_EQUAL_HEX8(0, frame[i]);
        TEST_ASSERT_EQUAL_HEX8(0xF | page << 5, frame[12]);       //no Stacato by default
        if(page == 3){
            TEST_ASSERT_EQUAL_HEX8(0, frame[8]);
            TEST_ASSERT_EQUAL_HEX8(0, frame[9]);
            TEST_ASSERT_EQUAL_HEX8(0x11, frame[10]);    //RSSI and battery are the receiver's own: always sent
            TEST_ASSERT_EQUAL_HEX8(0x22, frame[11]);
        }
        else if(page == 5){
            TEST_ASSERT_EQUAL_INT16(noPose, getInt16(&frame[8]));
            TEST_ASSERT_EQUAL_INT16(noPose, getInt16(&frame[10]));
        }
        else if(page == 7){
            TEST_ASSERT_EQUAL_HEX16(noHeading, getUint16(&frame[8]));
            TEST_ASSERT_EQUAL_HEX16(0, getUint16(&frame[10]));
        }
        else for(int i = 8; i < 12; i++) TEST_ASSERT_EQUAL_INT8(noData, int8_t(frame[i]));
    }
    //a valid pose with NaN values (an old Stacato's q): the markers too
    t.b_poseValid = true;
    t.poseX = NAN;
    t.heading = NAN;
    s.page = 5;
    encode(s, t, 0, 0, frame);
    TEST_ASSERT_EQUAL_INT16(noPose, getInt16(&frame[8]));
    s.page = 7;
    encode(s, t, 0, 0, frame);
    TEST_ASSERT_EQUAL_HEX16(noHeading, getUint16(&frame[8]));
}

void test_pager_sequence(){
    Pager pager;
    uint8_t expected[] = {0, 1, 2, 3, 4, 5, 6, 7, 0, 1, 2, 3, 4, 5, 6, 7, 0};
    uint8_t velocityPages = 0, flagPages = 0;
    for(uint8_t i = 0; i < sizeof(expected); i++){
        uint8_t page = pager.next();
        TEST_ASSERT_EQUAL_UINT8(expected[i], page);
        if(i < 16 && isVelocityPage(page)) velocityPages++;
        if(i < 16 && (page == PAGE_FLAGS || page == PAGE_HEADING)) flagPages++;
    }
    //over 16 replies (800 ms at 20 Hz): wheel velocities 10 Hz, flags 5 Hz
    TEST_ASSERT_EQUAL_UINT8(8, velocityPages);
    TEST_ASSERT_EQUAL_UINT8(4, flagPages);
}

void test_decode_round_trip_and_rejects(){
    Telemetry t = driving();
    t.wheelFlags = 0xA5C3;
    Status s;
    s.state = 6;
    s.speed = 1;
    s.b_direction = true;
    s.b_function = true;
    s.txCounter = 7;
    s.mavState = MAV_EMERGENCY;
    s.b_armed = false;
    MotionView v;
    for(uint8_t page = 0; page < 8; page++){
        s.page = page;
        uint8_t frame[13];
        encode(s, t, 0x60, 0x99, frame);
        Decoded d;
        TEST_ASSERT_TRUE(decode(frame, 13, d));
        TEST_ASSERT_EQUAL_UINT8(MAV_EMERGENCY, d.status.mavState);
        TEST_ASSERT_FALSE(d.status.b_armed);
        TEST_ASSERT_EQUAL_HEX8(frame[12], d.vehicleByte);
        TEST_ASSERT_EQUAL_UINT8(6, d.status.state);
        TEST_ASSERT_EQUAL_UINT8(1, d.status.speed);
        TEST_ASSERT_TRUE(d.status.b_direction && d.status.b_function);
        TEST_ASSERT_EQUAL_UINT8(7, d.status.txCounter);
        TEST_ASSERT_EQUAL_UINT8(page, d.status.page);
        TEST_ASSERT_EQUAL_INT16(312, d.vx);
        TEST_ASSERT_EQUAL_INT16(-48, d.vy);
        TEST_ASSERT_EQUAL_INT16(100, d.vr);
        TEST_ASSERT_EQUAL(page == 3, d.b_slowValues);
        if(page == 3){
            TEST_ASSERT_EQUAL_HEX8(0x60, d.rssiByte);
            TEST_ASSERT_EQUAL_HEX8(0x99, d.batteryByte);
        }
        v.apply(d, 1000);      //the same arrival time: no carry-forward here
        TEST_ASSERT_TRUE(v.vehicleState == VehicleState::E_STOP);
    }
    TEST_ASSERT_TRUE(v.b_velocityLive);
    TEST_ASSERT_FLOAT_WITHIN(1e-6f, 1.25f, v.poseX);
    TEST_ASSERT_FLOAT_WITHIN(1e-6f, -32.767f, v.poseY);
    TEST_ASSERT_FLOAT_WITHIN(1e-4f, 90.0f, v.heading);
    TEST_ASSERT_FLOAT_WITHIN(1e-6f, 0.312f, v.vx);
    TEST_ASSERT_FLOAT_WITHIN(1e-6f, -0.048f, v.vy);
    TEST_ASSERT_FLOAT_WITHIN(1e-6f, 0.1f, v.vr);
    TEST_ASSERT_FLOAT_WITHIN(0.005f, 0.5f, v.wheelVelocity[FL]);
    TEST_ASSERT_FLOAT_WITHIN(0.005f, -0.25f, v.wheelVelocity[FR]);
    TEST_ASSERT_FLOAT_WITHIN(1e-6f, 1.0f, v.wheelVelocity[RL]);
    TEST_ASSERT_TRUE(isnan(v.wheelVelocity[RR]));
    TEST_ASSERT_TRUE(v.b_torqueKnown);
    TEST_ASSERT_FLOAT_WITHIN(0.005f, 0.12f, v.wheelTorque[FR]);
    TEST_ASSERT_FLOAT_WITHIN(1e-6f, -1.0f, v.wheelTorque[RL]);
    TEST_ASSERT_TRUE(v.b_flagsKnown);
    TEST_ASSERT_EQUAL_HEX16(0xA5C3, v.wheelFlags);
    //flag word: bits 0-3 enabled, 4-7 ready, 8-11 fault, 12-15 STO; FL, FR, RL, RR
    TEST_ASSERT_TRUE(wheelFlag(0xA5C3, FL, ENABLED));
    TEST_ASSERT_TRUE(wheelFlag(0xA5C3, FR, ENABLED));
    TEST_ASSERT_FALSE(wheelFlag(0xA5C3, RL, ENABLED));
    TEST_ASSERT_TRUE(wheelFlag(0xA5C3, RR, READY));
    TEST_ASSERT_FALSE(wheelFlag(0xA5C3, FL, READY));
    TEST_ASSERT_TRUE(wheelFlag(0xA5C3, FL, FAULT));
    TEST_ASSERT_TRUE(wheelFlag(0xA5C3, RL, FAULT));
    TEST_ASSERT_FALSE(wheelFlag(0xA5C3, FR, FAULT));
    TEST_ASSERT_TRUE(wheelFlag(0xA5C3, FR, STO));
    TEST_ASSERT_TRUE(wheelFlag(0xA5C3, RR, STO));
    TEST_ASSERT_FALSE(wheelFlag(0xA5C3, RL, STO));

    //the old 8 byte reply (header 0b111, as an old receiver sends it) is not a reply any more
    uint8_t oldReply[8] = {0x17, 0x90, 0, 0, 0, 0, 150, 128};
    Decoded old;
    TEST_ASSERT_FALSE(decode(oldReply, 8, old));
    //nor the D-161 12 byte reply (a receiver flashed before D-163)
    uint8_t d161Reply[12] = {0x27, 0x70, 0x38, 0x01, 0, 0, 0, 0, 64, 64, 64, 64};
    TEST_ASSERT_FALSE(decode(d161Reply, 12, old));

    //rejected: other lengths (an uplink is 8 bytes, D-161 12), other headers; the decoded reply untouched
    uint8_t frame[14] = {0x07, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    Decoded untouched;
    untouched.vx = 1234;
    for(uint8_t length : {uint8_t(0), uint8_t(7), uint8_t(8), uint8_t(9), uint8_t(11), uint8_t(12), uint8_t(14)}) TEST_ASSERT_FALSE(decode(frame, length, untouched));
    for(uint8_t header = 0; header < 7; header++){
        frame[0] = header;
        TEST_ASSERT_FALSE(decode(frame, 13, untouched));
    }
    TEST_ASSERT_EQUAL_INT16(1234, untouched.vx);
    frame[0] = 0x07;
    TEST_ASSERT_TRUE(decode(frame, 13, untouched));
}

//——— the receiver's motion source ———

void test_motion_source_axes_and_staleness(){
    MotionSource source;
    TEST_ASSERT_FALSE(source.current(0, true).b_valid);
    //body FRD: 0.5 m/s forward, 0.1 m/s right, 0.2 rad/s clockwise
    source.onOdometry(1000, 0.5f, 0.1f, 0.2f, 0, 0, 1, 0);
    TEST_ASSERT_FALSE(source.current(1000, true).b_valid);      //no wheel data yet
    float actuator[8] = {0.1f, 0.2f, 0.3f, 0.4f, 0.0f, 0.0f, NAN, 1.5f};
    source.onActuators(1010, 0x12340F0Fu, actuator);
    Telemetry t = source.current(1010, true);
    TEST_ASSERT_TRUE(t.b_valid);
    TEST_ASSERT_FLOAT_WITHIN(1e-6f, 0.1f, t.vx);     //x right = FRD y
    TEST_ASSERT_FLOAT_WITHIN(1e-6f, 0.5f, t.vy);     //y forward = FRD x
    TEST_ASSERT_FLOAT_WITHIN(1e-6f, 0.2f, t.vr);     //clockwise as sent
    TEST_ASSERT_EQUAL_HEX16(0x0F0F, t.wheelFlags);   //the low 16 bits of active
    TEST_ASSERT_FLOAT_WITHIN(1e-6f, 0.4f, t.wheelVelocity[3]);
    TEST_ASSERT_TRUE(isnan(t.wheelTorque[2]));
    TEST_ASSERT_FLOAT_WITHIN(1e-6f, 1.5f, t.wheelTorque[3]);
    //stale after 500 ms of either message
    TEST_ASSERT_TRUE(source.current(1500, true).b_valid);
    TEST_ASSERT_FALSE(source.current(1501, true).b_valid);
    source.onOdometry(1600, 0.0f, 0.0f, 0.0f, 0, 0, 1, 0);
    TEST_ASSERT_FALSE(source.current(1600, true).b_valid);      //the wheel data is still old
    source.onActuators(1600, 0, actuator);
    TEST_ASSERT_TRUE(source.current(1600, true).b_valid);
    //without Stacato's heartbeat never valid; after the heartbeat is lost the old messages no longer count
    TEST_ASSERT_FALSE(source.current(1600, false).b_valid);
    source.clear();
    TEST_ASSERT_FALSE(source.current(1600, true).b_valid);
    //across the millis() wrap
    source.onOdometry(0xFFFFFF00u, 0.1f, 0.0f, 0.0f, 0, 0, 1, 0);
    source.onActuators(0xFFFFFF00u, 0, actuator);
    TEST_ASSERT_TRUE(source.current(0x00000010u, true).b_valid);
    TEST_ASSERT_FALSE(source.current(0x00000200u, true).b_valid);
    //stale data goes out as no motion
    Status s;
    s.page = 0;
    uint8_t frame[13];
    encode(s, source.current(0x00000200u, true), 0, 0, frame);
    TEST_ASSERT_EQUAL_HEX8(0, frame[2]);
    TEST_ASSERT_EQUAL_INT8(noData, int8_t(frame[8]));
}

void test_motion_view_availability_and_stale(){
    MotionView v;
    TEST_ASSERT_TRUE(availability(false, v) == MotionAvailability::NO_LINK);
    TEST_ASSERT_TRUE(availability(true, v) == MotionAvailability::NO_DATA);
    TEST_ASSERT_FALSE(v.b_flagsKnown);
    TEST_ASSERT_FALSE(v.b_torqueKnown);
    //live data, then the receiver's data goes stale: the next wheel velocity page says so
    Telemetry t = driving();
    Status s;
    uint8_t frame[13];
    Decoded d;
    s.page = 0;
    encode(s, t, 0, 0, frame);
    decode(frame, 13, d);
    v.apply(d, 0);
    TEST_ASSERT_TRUE(availability(true, v) == MotionAvailability::LIVE);
    t.b_valid = false;
    s.page = 1;
    encode(s, t, 0, 0, frame);
    decode(frame, 13, d);
    v.apply(d, 0);
    TEST_ASSERT_FLOAT_WITHIN(1e-6f, 0.0f, v.vx);        //zero at once
    TEST_ASSERT_TRUE(isnan(v.wheelTorque[0]));
    s.page = 2;
    encode(s, t, 0, 0, frame);
    decode(frame, 13, d);
    v.apply(d, 0);
    TEST_ASSERT_TRUE(availability(true, v) == MotionAvailability::NO_DATA);
    for(int i = 0; i < 4; i++) TEST_ASSERT_TRUE(isnan(v.wheelVelocity[i]));
    s.page = 3;
    encode(s, t, 0, 0, frame);
    decode(frame, 13, d);
    v.apply(d, 0);
    TEST_ASSERT_EQUAL_HEX16(0, v.wheelFlags);
    v.clear();
    TEST_ASSERT_FALSE(v.b_velocityLive);
    TEST_ASSERT_FALSE(v.b_flagsKnown);
}

//——— D-163: the vehicle state, the pose ———

void test_vehicle_state_every_mav_state_and_armed(){
    struct Row{ uint8_t mavState; VehicleState disarmed; VehicleState armed; };
    const Row rows[] = {
        {MAV_UNINIT, VehicleState::VEHICLE_OFF, VehicleState::VEHICLE_OFF},
        {MAV_BOOT, VehicleState::BUS_START, VehicleState::BUS_START},
        {MAV_CALIBRATING, VehicleState::ENABLING, VehicleState::ENABLING},
        {MAV_STANDBY, VehicleState::DISABLED, VehicleState::DISABLED},
        {MAV_ACTIVE, VehicleState::ENABLED, VehicleState::ENABLED},
        {MAV_CRITICAL, VehicleState::E_STOPPING, VehicleState::DISABLING},
        {MAV_EMERGENCY, VehicleState::E_STOP, VehicleState::AXIS_FAULT},
        {MAV_POWEROFF, VehicleState::BUS_OFF, VehicleState::BUS_OFF},
        {MAV_FLIGHT_TERMINATION, VehicleState::UNKNOWN, VehicleState::UNKNOWN},
        {0xF, VehicleState::NO_STACATO, VehicleState::NO_STACATO},
    };
    for(const Row& r : rows){
        TEST_ASSERT_TRUE(vehicleState(r.mavState, false) == r.disarmed);
        TEST_ASSERT_TRUE(vehicleState(r.mavState, true) == r.armed);
        //from byte 12, whatever the page
        for(uint8_t page = 0; page < 8; page++){
            TEST_ASSERT_TRUE(vehicleState(uint8_t(r.mavState | page << 5)) == r.disarmed);
            TEST_ASSERT_TRUE(vehicleState(uint8_t(r.mavState | 0x10 | page << 5)) == r.armed);
        }
    }
    for(uint8_t mavState = 9; mavState < 0xF; mavState++){
        TEST_ASSERT_TRUE(vehicleState(mavState, false) == VehicleState::UNKNOWN);
        TEST_ASSERT_TRUE(vehicleState(mavState, true) == VehicleState::UNKNOWN);
    }
    //no reply yet: NO_LINK, and the link lost clears back to it
    MotionView v;
    TEST_ASSERT_TRUE(v.vehicleState == VehicleState::NO_LINK);
    Status s;
    s.mavState = MAV_CRITICAL;
    s.b_armed = true;
    uint8_t frame[13];
    Decoded d;
    encode(s, Telemetry(), 0, 0, frame);
    TEST_ASSERT_TRUE(decode(frame, 13, d));
    v.apply(d, 0);
    TEST_ASSERT_TRUE(v.vehicleState == VehicleState::DISABLING);
    v.clear();
    TEST_ASSERT_TRUE(v.vehicleState == VehicleState::NO_LINK);
}

void test_pose_markers_clamping_and_heading_wrap(){
    TEST_ASSERT_EQUAL_INT16(noPose, toPoseMilli(NAN));
    TEST_ASSERT_EQUAL_INT16(0, toPoseMilli(0.0f));
    TEST_ASSERT_EQUAL_INT16(1250, toPoseMilli(1.25f));
    TEST_ASSERT_EQUAL_INT16(-1250, toPoseMilli(-1.25f));
    TEST_ASSERT_EQUAL_INT16(32767, toPoseMilli(32.767f));
    TEST_ASSERT_EQUAL_INT16(32767, toPoseMilli(100.0f));
    TEST_ASSERT_EQUAL_INT16(-32767, toPoseMilli(-100.0f));     //never the marker
    TEST_ASSERT_EQUAL_INT16(32767, toPoseMilli(INFINITY));
    TEST_ASSERT_EQUAL_INT16(-32767, toPoseMilli(-INFINITY));
    for(float m = -40.0f; m <= 40.0f; m += 0.0137f) TEST_ASSERT_TRUE(toPoseMilli(m) != noPose);
    TEST_ASSERT_TRUE(isnan(fromPoseMilli(noPose)));
    TEST_ASSERT_FLOAT_WITHIN(1e-6f, -32.767f, fromPoseMilli(-32767));

    const float pi = 3.14159265f;
    TEST_ASSERT_EQUAL_HEX16(0x0000, toHeadingWord(0.0f));
    TEST_ASSERT_EQUAL_HEX16(0x4000, toHeadingWord(pi / 2));
    TEST_ASSERT_EQUAL_HEX16(0x8000, toHeadingWord(pi));
    TEST_ASSERT_EQUAL_HEX16(0x8000, toHeadingWord(-pi));
    TEST_ASSERT_EQUAL_HEX16(0xC000, toHeadingWord(-pi / 2));
    TEST_ASSERT_EQUAL_HEX16(0xC000, toHeadingWord(3 * pi / 2));
    TEST_ASSERT_EQUAL_HEX16(0x4000, toHeadingWord(pi / 2 + 4 * pi));       //whole turns wrap
    TEST_ASSERT_EQUAL_HEX16(0x0000, toHeadingWord(2 * pi));
    TEST_ASSERT_EQUAL_HEX16(0x0000, toHeadingWord(-1e-7f));                //rounds up to the full turn: 0
    //one step short of a full turn: the largest real value, never the marker
    float step = 2 * pi / 65536.0f;
    TEST_ASSERT_EQUAL_HEX16(0xFFFE, toHeadingWord(-2 * step));
    TEST_ASSERT_EQUAL_HEX16(0xFFFE, toHeadingWord(-1.0f * step));
    for(float r = -7.0f; r <= 7.0f; r += 0.00037f) TEST_ASSERT_TRUE(toHeadingWord(r) != noHeading);
    TEST_ASSERT_EQUAL_HEX16(noHeading, toHeadingWord(NAN));
    TEST_ASSERT_EQUAL_HEX16(noHeading, toHeadingWord(INFINITY));

    TEST_ASSERT_TRUE(isnan(headingDegrees(noHeading)));
    TEST_ASSERT_FLOAT_WITHIN(1e-4f, 0.0f, headingDegrees(0));
    TEST_ASSERT_FLOAT_WITHIN(1e-4f, 90.0f, headingDegrees(0x4000));
    TEST_ASSERT_FLOAT_WITHIN(1e-4f, 180.0f, headingDegrees(0x8000));
    TEST_ASSERT_FLOAT_WITHIN(1e-4f, -90.0f, headingDegrees(0xC000));
    TEST_ASSERT_FLOAT_WITHIN(1e-3f, -360.0f / 65536.0f * 2, headingDegrees(0xFFFE));
    //round trip within half a step
    for(float deg = -179.9f; deg < 180.0f; deg += 0.73f)
        TEST_ASSERT_FLOAT_WITHIN(0.0028f, deg, headingDegrees(toHeadingWord(deg * pi / 180.0f)));

    TEST_ASSERT_FLOAT_WITHIN(1e-4f, -170.0f, wrapDegrees(190.0f));
    TEST_ASSERT_FLOAT_WITHIN(1e-4f, 170.0f, wrapDegrees(-190.0f));
    TEST_ASSERT_FLOAT_WITHIN(1e-4f, 180.0f, wrapDegrees(180.0f));
    TEST_ASSERT_FLOAT_WITHIN(1e-4f, 180.0f, wrapDegrees(-180.0f));
    TEST_ASSERT_FLOAT_WITHIN(1e-4f, 10.0f, wrapDegrees(730.0f));
    TEST_ASSERT_TRUE(isnan(wrapDegrees(NAN)));
}

//one reply of the given page through the wire into the view
static void receive(MotionView& v, const Telemetry& t, uint8_t page, uint32_t now){
    Status s;
    s.page = page;
    s.mavState = MAV_ACTIVE;
    s.b_armed = true;
    uint8_t frame[13];
    encode(s, t, 0, 0, frame);
    Decoded d;
    TEST_ASSERT_TRUE(decode(frame, 13, d));
    v.apply(d, now);
}

static Telemetry moving(float vx, float vy, float vrClockwise, float x, float y, float headingRad){
    Telemetry t;
    t.b_valid = true;
    t.vx = vx;
    t.vy = vy;
    t.vr = vrClockwise;
    for(int i = 0; i < 4; i++){
        t.wheelVelocity[i] = 0.2f;
        t.wheelTorque[i] = 0.1f;
    }
    t.b_poseValid = true;
    t.poseX = x;
    t.poseY = y;
    t.heading = headingRad;
    return t;
}

void test_pose_carry_forward(){
    const float pi = 3.14159265f;
    //straight line: 0.5 m/s forward, heading 0 (upstage): y grows 25 mm per reply between the pose pages
    {
        MotionView v;
        Telemetry t = moving(0.0f, 0.5f, 0.0f, 1.0f, 2.0f, 0.0f);
        uint32_t now = 1000;
        for(uint8_t page = 0; page < 8; page++, now += 50) receive(v, t, page, now);   //the first pose and heading
        TEST_ASSERT_FLOAT_WITHIN(1e-4f, 1.0f, v.poseX);
        TEST_ASSERT_FLOAT_WITHIN(1e-4f, 2.0f, v.poseY);     //no heading before page 7: nothing carried yet
        TEST_ASSERT_FLOAT_WITHIN(1e-4f, 0.0f, v.heading);
        receive(v, t, 0, now); now += 50;
        TEST_ASSERT_FLOAT_WITHIN(1e-4f, 2.025f, v.poseY);
        TEST_ASSERT_FLOAT_WITHIN(1e-4f, 1.0f, v.poseX);
        for(uint8_t page = 1; page < 5; page++, now += 50) receive(v, t, page, now);
        TEST_ASSERT_FLOAT_WITHIN(1e-4f, 2.125f, v.poseY);
        receive(v, t, 5, now); now += 50;
        TEST_ASSERT_FLOAT_WITHIN(1e-4f, 2.0f, v.poseY);     //reset on the pose page
        receive(v, t, 6, now); now += 50;
        receive(v, t, 7, now); now += 50;
        TEST_ASSERT_FLOAT_WITHIN(1e-4f, 2.05f, v.poseY);
        TEST_ASSERT_FLOAT_WITHIN(1e-4f, 0.0f, v.heading);
    }
    //heading 90 deg (counter-clockwise: facing stage left): forward is -x, right is +y
    {
        MotionView v;
        Telemetry t = moving(0.2f, 1.0f, 0.0f, 0.0f, 0.0f, pi / 2);
        receive(v, t, 4, 0);       //motion data live first: only then is anything carried forward
        receive(v, t, 5, 0);
        receive(v, t, 7, 50);
        TEST_ASSERT_FLOAT_WITHIN(1e-3f, 90.0f, v.heading);
        receive(v, t, 0, 100);
        //one 50 ms step with a heading (none before page 7): 1 m/s forward to -x, 0.2 m/s right to +y
        TEST_ASSERT_FLOAT_WITHIN(1e-4f, -0.05f, v.poseX);
        TEST_ASSERT_FLOAT_WITHIN(1e-4f, 0.01f, v.poseY);
    }
    //rotation: 1 rad/s clockwise turns the heading down by 2.865 deg per 50 ms
    {
        MotionView v;
        Telemetry t = moving(0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f);
        receive(v, t, 6, 0);
        receive(v, t, 7, 0);
        receive(v, t, 0, 50);
        TEST_ASSERT_FLOAT_WITHIN(1e-3f, -2.8648f, v.heading);
        receive(v, t, 1, 100);
        TEST_ASSERT_FLOAT_WITHIN(1e-3f, -5.7296f, v.heading);
        TEST_ASSERT_TRUE(isnan(v.poseX));       //no pose page yet: the heading alone is carried
    }
    //an arc: forward 1 m/s while turning counter-clockwise 1 rad/s, mid-step heading: close to the circle
    {
        MotionView v;
        Telemetry t = moving(0.0f, 1.0f, -1.0f, 0.0f, 0.0f, 0.0f);
        receive(v, t, 4, 0);
        receive(v, t, 5, 0);
        receive(v, t, 7, 0);
        uint32_t now = 0;
        for(int i = 0; i < 6; i++){ now += 50; receive(v, t, uint8_t(i % 2 == 0 ? 0 : 2), now); }     //0.3 s
        //on a unit circle turning left from facing +y: x = cos(0.3) - 1, y = sin(0.3)
        TEST_ASSERT_FLOAT_WITHIN(1e-3f, cosf(0.3f) - 1.0f, v.poseX);
        TEST_ASSERT_FLOAT_WITHIN(1e-3f, sinf(0.3f), v.poseY);
        TEST_ASSERT_FLOAT_WITHIN(1e-2f, 0.3f * 180.0f / pi, v.heading);
    }
    //heading wrap: from 179 deg turning counter-clockwise past 180
    {
        MotionView v;
        Telemetry t = moving(0.0f, 0.0f, -1.0f, 0.0f, 0.0f, 179.0f * pi / 180.0f);
        receive(v, t, 6, 0);
        receive(v, t, 7, 0);
        TEST_ASSERT_FLOAT_WITHIN(3e-3f, 179.0f, v.heading);
        receive(v, t, 0, 50);
        TEST_ASSERT_FLOAT_WITHIN(3e-3f, 179.0f + 2.8648f - 360.0f, v.heading);
        TEST_ASSERT_TRUE(v.heading >= -180.0f && v.heading <= 180.0f);
    }
    //dt capped at 100 ms: a 2 s gap moves the pose as one 100 ms step, never 2 s
    {
        MotionView v;
        Telemetry t = moving(0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f);
        receive(v, t, 4, 0);
        receive(v, t, 5, 0);
        receive(v, t, 7, 0);
        receive(v, t, 0, 2000);
        TEST_ASSERT_FLOAT_WITHIN(1e-4f, 0.1f, v.poseY);
        //across the millis() wrap: a 50 ms step
        MotionView w;
        receive(w, t, 4, 0xFFFFFFE0u);
        receive(w, t, 5, 0xFFFFFFE0u);
        receive(w, t, 7, 0xFFFFFFE0u);
        receive(w, t, 0, 0x00000012u);
        TEST_ASSERT_FLOAT_WITHIN(1e-4f, 0.05f, w.poseY);
    }
    //nothing carried forward before a velocity page says motion data is live
    {
        MotionView v;
        Telemetry t = moving(0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f);
        receive(v, t, 5, 0);
        receive(v, t, 7, 0);
        receive(v, t, 1, 50);
        TEST_ASSERT_FLOAT_WITHIN(1e-6f, 0.0f, v.poseY);
        receive(v, t, 2, 100);
        receive(v, t, 3, 150);
        TEST_ASSERT_FLOAT_WITHIN(1e-4f, 0.05f, v.poseY);
    }
    //NaN: no pose yet; a pose page without a value; no motion data; the link lost
    {
        MotionView v;
        Telemetry t = moving(0.0f, 1.0f, 0.0f, 3.0f, 4.0f, 0.0f);
        TEST_ASSERT_TRUE(isnan(v.poseX) && isnan(v.poseY) && isnan(v.heading));
        receive(v, t, 0, 0);
        receive(v, t, 1, 50);
        TEST_ASSERT_TRUE(isnan(v.poseX) && isnan(v.poseY) && isnan(v.heading));     //nothing to carry forward
        //no heading yet: x and y hold the page's values
        receive(v, t, 5, 100);
        receive(v, t, 6, 150);
        TEST_ASSERT_FLOAT_WITHIN(1e-6f, 3.0f, v.poseX);
        TEST_ASSERT_FLOAT_WITHIN(1e-6f, 4.0f, v.poseY);
        receive(v, t, 7, 200);
        TEST_ASSERT_FLOAT_WITHIN(1e-4f, 0.0f, v.heading);
        //the heading from an old Stacato (NaN q) only blanks the heading
        Telemetry oldStacato = t;
        oldStacato.heading = NAN;
        receive(v, oldStacato, 7, 250);
        TEST_ASSERT_TRUE(isnan(v.heading));
        TEST_ASSERT_FALSE(isnan(v.poseX));
        //a pose page without a value
        receive(v, t, 7, 300);
        Telemetry noPoseNow = t;
        noPoseNow.b_poseValid = false;
        receive(v, noPoseNow, 5, 350);
        TEST_ASSERT_TRUE(isnan(v.poseX) && isnan(v.poseY));
        receive(v, noPoseNow, 6, 400);
        TEST_ASSERT_TRUE(isnan(v.poseX) && isnan(v.poseY));     //NaN is never carried into a number
        //no motion data: the whole pose NaN
        receive(v, t, 5, 450);
        receive(v, t, 7, 500);
        TEST_ASSERT_FALSE(isnan(v.poseX) || isnan(v.heading));
        Telemetry stale = t;
        stale.b_valid = false;
        receive(v, stale, 0, 550);
        TEST_ASSERT_TRUE(isnan(v.poseX) && isnan(v.poseY) && isnan(v.heading));
        //the link lost clears it
        receive(v, t, 5, 600);
        receive(v, t, 7, 650);
        v.clear();
        TEST_ASSERT_TRUE(isnan(v.poseX) && isnan(v.poseY) && isnan(v.heading));
    }
}

void test_motion_source_pose_from_odometry(){
    const float pi = 3.14159265f;
    MotionSource source;
    float actuator[8] = {0, 0, 0, 0, 0, 0, 0, 0};
    //heading t from q = {cos(t/2), 0, 0, sin(t/2)}, counter-clockwise
    for(float deg : {0.0f, 30.0f, 90.0f, 179.0f, -45.0f, -179.0f}){
        float t = deg * pi / 180.0f;
        TEST_ASSERT_FLOAT_WITHIN(1e-4f, t, MotionSource::headingFromQuaternion(cosf(t / 2), sinf(t / 2)));
    }
    //q and -q are the same rotation
    TEST_ASSERT_FLOAT_WITHIN(1e-4f, pi / 2, MotionSource::headingFromQuaternion(-cosf(pi / 4), -sinf(pi / 4)));
    TEST_ASSERT_TRUE(isnan(MotionSource::headingFromQuaternion(NAN, 0.0f)));
    TEST_ASSERT_TRUE(isnan(MotionSource::headingFromQuaternion(1.0f, NAN)));

    float t = 60.0f * pi / 180.0f;
    source.onOdometry(1000, 0.5f, 0.0f, 0.0f, 2.5f, -1.25f, cosf(t / 2), sinf(t / 2));
    Telemetry out = source.current(1000, true);
    TEST_ASSERT_TRUE(out.b_poseValid);
    TEST_ASSERT_FALSE(out.b_valid);         //velocities need ACTUATOR_OUTPUT_STATUS too, the pose does not
    TEST_ASSERT_FLOAT_WITHIN(1e-6f, 2.5f, out.poseX);
    TEST_ASSERT_FLOAT_WITHIN(1e-6f, -1.25f, out.poseY);
    TEST_ASSERT_FLOAT_WITHIN(1e-4f, t, out.heading);
    Status s;
    s.page = PAGE_HEADING;
    uint8_t frame[13];
    encode(s, out, 0, 0, frame);
    TEST_ASSERT_EQUAL_HEX16(0x2AAB, getUint16(&frame[8]));     //60 deg: 65536 / 6 = 10922.7
    source.onActuators(1000, 0, actuator);
    TEST_ASSERT_TRUE(source.current(1000, true).b_valid);

    //stale after 500 ms of ODOMETRY: the markers
    TEST_ASSERT_TRUE(source.current(1500, true).b_poseValid);
    out = source.current(1501, true);
    TEST_ASSERT_FALSE(out.b_poseValid);
    TEST_ASSERT_TRUE(isnan(out.poseX) && isnan(out.poseY) && isnan(out.heading));
    s.page = PAGE_POSE_XY;
    encode(s, out, 0, 0, frame);
    TEST_ASSERT_EQUAL_INT16(noPose, getInt16(&frame[8]));
    TEST_ASSERT_EQUAL_INT16(noPose, getInt16(&frame[10]));
    //without Stacato's heartbeat, and after it was lost
    source.onOdometry(2000, 0, 0, 0, 1, 1, 1, 0);
    TEST_ASSERT_FALSE(source.current(2000, false).b_poseValid);
    source.clear();
    TEST_ASSERT_FALSE(source.current(2000, true).b_poseValid);
    //an old Stacato (NaN q): x and y, no heading
    source.onOdometry(3000, 0, 0, 0, 1.5f, 2.5f, NAN, NAN);
    out = source.current(3000, true);
    TEST_ASSERT_TRUE(out.b_poseValid);
    TEST_ASSERT_TRUE(isnan(out.heading));
    s.page = PAGE_HEADING;
    encode(s, out, 0, 0, frame);
    TEST_ASSERT_EQUAL_HEX16(noHeading, getUint16(&frame[8]));
    s.page = PAGE_POSE_XY;
    encode(s, out, 0, 0, frame);
    TEST_ASSERT_EQUAL_INT16(1500, getInt16(&frame[8]));
    TEST_ASSERT_EQUAL_INT16(2500, getInt16(&frame[10]));
}

int main(int argc, char** argv){
    UNITY_BEGIN();
    RUN_TEST(test_uplink_estop_number_every_value_as_before);
    RUN_TEST(test_uplink_generated_numbers);
    RUN_TEST(test_uplink_all_fields_as_before);
    RUN_TEST(test_uplink_a_reply_is_never_an_uplink);
    RUN_TEST(test_status_word_as_before_and_byte_12);
    RUN_TEST(test_conversions_round_clamp_and_nan);
    RUN_TEST(test_reply_layout_every_page);
    RUN_TEST(test_reply_invalid_telemetry_sends_no_motion);
    RUN_TEST(test_pager_sequence);
    RUN_TEST(test_decode_round_trip_and_rejects);
    RUN_TEST(test_motion_source_axes_and_staleness);
    RUN_TEST(test_motion_view_availability_and_stale);
    RUN_TEST(test_vehicle_state_every_mav_state_and_armed);
    RUN_TEST(test_pose_markers_clamping_and_heading_wrap);
    RUN_TEST(test_pose_carry_forward);
    RUN_TEST(test_motion_source_pose_from_odometry);
    return UNITY_END();
}
