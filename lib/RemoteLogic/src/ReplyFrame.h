#pragma once
#include <stdint.h>
#include <math.h>

//The receiver's 13 byte reply to each remote frame [D-161, D-163]. Pure code, used by the receiver (RobotBoard), the
//USB remote (RemoteBoard::usbExchange) and the remote's reply decoder; tested on the host. The remote and the receiver
//are always flashed together, there is no compatibility with an older format: the remote takes only a 13 byte reply
//(the D-161 12 byte reply and the 8 byte reply before it are invalid frames: a mismatched pair shows NO LINK). The
//uplink is unchanged (UplinkFrame.h, 8 bytes, header 0b111, the E-stop number in bytes 6 and 7).
//  0-1   the status word as before D-161: bits 0-2 header 0b111, 3-5 state, 6-7 speed echo, 8 direction echo, 9
//        function button, 10 creep echo [D-207] (the creep the receiver relays to Stacato), 11 0 (D-161 had the page
//        in 10-11), 12-15 tx counter mirror
//  2-3   vx, 4-5 vy (int16 little-endian, mm/s), 6-7 vr (int16, mrad/s), in every reply
//  8-11  the page's slot, pages in the order 0..7, one per reply (20 Hz replies):
//        0, 2, 4, 6  the four wheel velocities (int8 each, -127..127 = -100..100 % of the drive's velocity limit,
//                    clamped; -128 no value)                                                       10 Hz
//        1           the four wheel torques (int8, % of the rated torque, as above)                2.5 Hz
//        3           the wheel flag word (uint16 LE), the receiver's RSSI of the remote + 150, the vehicle battery
//                    byte (45..70 V on 0..255)                                                     2.5 Hz
//        5           pose x, pose y (int16 LE, mm, clamped to +-32767; -32768 no value)            2.5 Hz
//        7           heading (uint16 LE over a full turn: deg = v * 360 / 65536, counter-clockwise positive, 0
//                    upstage; 0xFFFF no value), then the wheel flag word again                      2.5 Hz
//        so the flag word at 5 Hz (pages 3 and 7)
//  12    bits 0-3 Stacato's HEARTBEAT.system_status (MAV_STATE 0..8) as received, 0xF without Stacato's heartbeat;
//        bit 4 MAV_MODE_FLAG_SAFETY_ARMED of base_mode (set: no E-stop latched in Stacato); bits 5-7 the page
//On air 22 bytes, 9.17 ms at 19.2 kbit/s (docs/remote-display/README.md, Airtime). Axes on the radio and the screen:
//x+ right, y+ forward, yaw positive clockwise; the pose in Stacato's world frame (x stage right, y upstage, heading
//counter-clockwise, 0 upstage). Wheels FL, FR, RL, RR.
namespace Reply{

    static constexpr uint8_t statusHeader = 0x7;        //bits 0-2 of the status word, as before
    static constexpr uint8_t length = 13;
    static constexpr uint8_t pageCount = 8;
    static constexpr int8_t noData = -128;              //a wheel byte without a value (outside -127..127)
    static constexpr int16_t noPose = -32768;           //a pose coordinate without a value (outside +-32767)
    static constexpr uint16_t noHeading = 0xFFFF;       //a heading without a value (the largest real one is 0xFFFE)
    static constexpr uint8_t noStacato = 0xF;           //byte 12's state nibble without Stacato's heartbeat

    //the pages other than the wheel velocities (0, 2, 4, 6)
    enum Page : uint8_t{ PAGE_TORQUE = 1, PAGE_FLAGS = 3, PAGE_POSE_XY = 5, PAGE_HEADING = 7 };
    inline bool isVelocityPage(uint8_t page){ return (page & 0x1) == 0; }

    //MAVLink's MAV_STATE (HEARTBEAT.system_status) as Stacato sends it; here because the host tests have no MAVLink
    enum MavState : uint8_t{
        MAV_UNINIT = 0, MAV_BOOT = 1, MAV_CALIBRATING = 2, MAV_STANDBY = 3, MAV_ACTIVE = 4, MAV_CRITICAL = 5,
        MAV_EMERGENCY = 6, MAV_POWEROFF = 7, MAV_FLIGHT_TERMINATION = 8
    };

    //wheel flag word (ACTUATOR_OUTPUT_STATUS.active, D-161 point 1): bit (group * 4 + wheel)
    enum WheelFlag : uint8_t{ ENABLED = 0, READY = 1, FAULT = 2, STO = 3 };
    enum Wheel : uint8_t{ FL = 0, FR = 1, RL = 2, RR = 3 };
    inline bool wheelFlag(uint16_t flags, uint8_t wheel, WheelFlag flag){ return (flags >> (flag * 4 + wheel)) & 0x1; }

    //——— the status word (bytes 0-1) and byte 12 ———

    struct Status{
        uint8_t state = 0;              //RobotState, 3 bits: drives every decision on the remote
        uint8_t speed = 0;              //SpeedMode echo, 2 bits
        bool b_direction = false;
        bool b_function = false;        //the receiver's remote finder button
        bool b_creep = false;           //[D-207] bit 10: the creep the receiver relays (MANUAL_CONTROL bit 11)
        uint8_t txCounter = 0;          //the remote's counter mirrored, 4 bits
        uint8_t page = 0;               //the slot in bytes 8-11, 3 bits, in byte 12 [D-163]
        uint8_t mavState = noStacato;   //[D-163] Stacato's MAV_STATE as received, 4 bits; 0xF: no Stacato heartbeat
        bool b_armed = false;           //[D-163] MAV_MODE_FLAG_SAFETY_ARMED: no E-stop latched in Stacato
    };

    //bit 10 creep [D-207]; bit 11 stays 0 [D-163]
    inline uint16_t statusWord(const Status& s){
        uint16_t word = statusHeader;
        word |= (s.state & 0x7) << 3;
        word |= (s.speed & 0x3) << 6;
        if(s.b_direction) word |= 0x100;
        if(s.b_function) word |= 0x200;
        if(s.b_creep) word |= 0x400;
        word |= (s.txCounter & 0xF) << 12;
        return word;
    }

    inline uint8_t vehicleByte(const Status& s){
        return uint8_t((s.mavState & 0xF) | (s.b_armed ? 0x10 : 0x00) | (s.page & 0x7) << 5);
    }

    //the status word and byte 12
    inline Status statusFrom(uint16_t word, uint8_t byte12){
        Status s;
        s.state = (word >> 3) & 0x7;
        s.speed = (word >> 6) & 0x3;
        s.b_direction = word & 0x100;
        s.b_function = word & 0x200;
        s.b_creep = word & 0x400;
        s.txCounter = (word >> 12) & 0xF;
        s.mavState = byte12 & 0xF;
        s.b_armed = byte12 & 0x10;
        s.page = (byte12 >> 5) & 0x7;
        return s;
    }

    //——— the vehicle state as shown [D-163] ———

    //from the reply's byte 12 (Stacato's MAV_STATE and armed flag, or no Stacato); NO_LINK while no reply arrives.
    //Display only, and the choice between the disabling and the E-stop sounds: every decision (LEDs, enable,
    //LinkPath, arcade, safety) keeps the 3 bit state
    enum class VehicleState : uint8_t{
        NO_LINK, NO_STACATO, BUS_START, BUS_OFF, VEHICLE_OFF, DISABLED, ENABLING, ENABLED, DISABLING, E_STOPPING,
        E_STOP, AXIS_FAULT, UNKNOWN
    };

    //  0xF no Stacato, BOOT bus start, POWEROFF bus off, UNINIT vehicle off, STANDBY disabled, CALIBRATING enabling,
    //  ACTIVE enabled, CRITICAL armed disabling (a disable ramp), CRITICAL not armed E-stopping (an E-stop ramp),
    //  EMERGENCY not armed E-stop, EMERGENCY armed axis fault; anything else unknown
    inline VehicleState vehicleState(uint8_t mavState, bool b_armed){
        switch(mavState){
            case noStacato:         return VehicleState::NO_STACATO;
            case MAV_BOOT:          return VehicleState::BUS_START;
            case MAV_POWEROFF:      return VehicleState::BUS_OFF;
            case MAV_UNINIT:        return VehicleState::VEHICLE_OFF;
            case MAV_STANDBY:       return VehicleState::DISABLED;
            case MAV_CALIBRATING:   return VehicleState::ENABLING;
            case MAV_ACTIVE:        return VehicleState::ENABLED;
            case MAV_CRITICAL:      return b_armed ? VehicleState::DISABLING : VehicleState::E_STOPPING;
            case MAV_EMERGENCY:     return b_armed ? VehicleState::AXIS_FAULT : VehicleState::E_STOP;
            default:                return VehicleState::UNKNOWN;
        }
    }
    //byte 12 as received (the page bits are ignored)
    inline VehicleState vehicleState(uint8_t byte12){ return vehicleState(byte12 & 0xF, byte12 & 0x10); }

    //——— conversions ———

    //m/s to mm/s, rad/s to mrad/s, rounded; NaN 0, clamped to +-32767
    inline int16_t toMilli(float value){
        if(isnan(value)) return 0;
        float milli = value * 1000.0f;
        if(milli >= 32767.0f) return 32767;
        if(milli <= -32767.0f) return -32767;
        return int16_t(lroundf(milli));
    }
    inline float fromMilli(int16_t value){ return float(value) / 1000.0f; }

    //a normalized wheel value (1.0 = the drive's velocity limit or rated torque) to -127..127, rounded and clamped;
    //NaN has no value: noData
    inline int8_t toPercentByte(float normalized){
        if(isnan(normalized)) return noData;
        float scaled = normalized * 127.0f;
        if(scaled >= 127.0f) return 127;
        if(scaled <= -127.0f) return -127;
        return int8_t(lroundf(scaled));
    }
    //back to -1..1; noData is NaN
    inline float fromPercentByte(int8_t value){ return value == noData ? NAN : float(value) / 127.0f; }

    //[D-163] a pose coordinate, m to mm, rounded, clamped to +-32767; NaN: noPose
    inline int16_t toPoseMilli(float meters){
        if(isnan(meters)) return noPose;
        float milli = meters * 1000.0f;
        if(milli >= 32767.0f) return 32767;
        if(milli <= -32767.0f) return -32767;
        return int16_t(lroundf(milli));
    }
    inline float fromPoseMilli(int16_t value){ return value == noPose ? NAN : float(value) / 1000.0f; }

    //[D-163] a heading in rad (any number of turns) to a uint16 over a full turn, rounded; a value rounding to 0xFFFF
    //is sent as 0xFFFE (0xFFFF is the marker); NaN or infinite: noHeading
    inline uint16_t toHeadingWord(float radians){
        if(isnan(radians) || isinf(radians)) return noHeading;
        const float turn = 6.28318530718f;
        float fraction = fmodf(radians, turn) / turn;
        if(fraction < 0.0f) fraction += 1.0f;
        long word = lroundf(fraction * 65536.0f);
        if(word >= 65536) word = 0;
        if(word > 0xFFFE) word = 0xFFFE;
        return uint16_t(word);
    }

    //deg wrapped to -180..180 (180 stays 180, -180 reads as 180); NaN and infinite are NaN
    inline float wrapDegrees(float degrees){
        if(isnan(degrees) || isinf(degrees)) return NAN;
        degrees = fmodf(degrees, 360.0f);
        if(degrees > 180.0f) degrees -= 360.0f;
        if(degrees <= -180.0f) degrees += 360.0f;
        return degrees;
    }

    //to deg, -180..180; noHeading is NaN
    inline float headingDegrees(uint16_t word){
        if(word == noHeading) return NAN;
        return wrapDegrees(float(word) * 360.0f / 65536.0f);
    }

    //——— what the reply carries ———

    //in the radio's axes (x+ right, y+ forward, yaw + clockwise), SI units; wheels normalized as above
    struct Telemetry{
        bool b_valid = false;           //false: zero velocities, wheel bytes noData and zero flags are sent
        float vx = 0.0f;                //m/s, right
        float vy = 0.0f;                //m/s, forward
        float vr = 0.0f;                //rad/s, clockwise
        float wheelVelocity[4] = {0, 0, 0, 0};
        float wheelTorque[4] = {0, 0, 0, 0};
        uint16_t wheelFlags = 0;
        //[D-163] the pose in Stacato's world frame; false: the no-value markers are sent. A NaN value is sent as no
        //value too (the heading from an old Stacato's NaN q)
        bool b_poseValid = false;
        float poseX = NAN;              //m, stage right
        float poseY = NAN;              //m, upstage
        float heading = NAN;            //rad, counter-clockwise, 0 upstage
    };

    //——— encoding ———

    static inline void putInt16(uint8_t* at, int16_t value){
        at[0] = uint16_t(value) & 0xFF;
        at[1] = (uint16_t(value) >> 8) & 0xFF;
    }
    static inline void putUint16(uint8_t* at, uint16_t value){
        at[0] = value & 0xFF;
        at[1] = (value >> 8) & 0xFF;
    }
    static inline int16_t getInt16(const uint8_t* at){ return int16_t(uint16_t(at[0]) | uint16_t(at[1]) << 8); }
    static inline uint16_t getUint16(const uint8_t* at){ return uint16_t(at[0]) | uint16_t(at[1]) << 8; }

    //status.page picks the slot (Pager), status.mavState and b_armed go to byte 12. Invalid telemetry is sent as no
    //motion, never old values; an invalid pose as the no-value markers
    inline void encode(const Status& status, const Telemetry& t, uint8_t rssiByte, uint8_t batteryByte,
                       uint8_t frame[length]){
        putUint16(&frame[0], statusWord(status));
        putInt16(&frame[2], t.b_valid ? toMilli(t.vx) : 0);
        putInt16(&frame[4], t.b_valid ? toMilli(t.vy) : 0);
        putInt16(&frame[6], t.b_valid ? toMilli(t.vr) : 0);
        uint16_t flags = t.b_valid ? t.wheelFlags : 0;
        switch(status.page & 0x7){
            case 0:
            case 2:
            case 4:
            case 6:
                for(int i = 0; i < 4; i++) frame[8 + i] = uint8_t(t.b_valid ? toPercentByte(t.wheelVelocity[i]) : noData);
                break;
            case PAGE_TORQUE:
                for(int i = 0; i < 4; i++) frame[8 + i] = uint8_t(t.b_valid ? toPercentByte(t.wheelTorque[i]) : noData);
                break;
            case PAGE_FLAGS:
                putUint16(&frame[8], flags);
                frame[10] = rssiByte;
                frame[11] = batteryByte;
                break;
            case PAGE_POSE_XY:
                putInt16(&frame[8], t.b_poseValid ? toPoseMilli(t.poseX) : noPose);
                putInt16(&frame[10], t.b_poseValid ? toPoseMilli(t.poseY) : noPose);
                break;
            case PAGE_HEADING:
                putUint16(&frame[8], t.b_poseValid ? toHeadingWord(t.heading) : noHeading);
                putUint16(&frame[10], flags);
                break;
        }
        frame[12] = vehicleByte(status);
    }

    //the receiver's page sequence 0..7, one step per reply
    class Pager{
    public:
        uint8_t next(){
            uint8_t p = page;
            page = (page + 1) % pageCount;
            return p;
        }
    private:
        uint8_t page = 0;
    };

    //——— decoding (the remote) ———

    struct Decoded{
        Status status;
        bool b_slowValues = false;      //rssiByte and batteryByte are in this reply (page 3)
        uint8_t rssiByte = 0;
        uint8_t batteryByte = 0;
        int16_t vx = 0;                 //mm/s
        int16_t vy = 0;
        int16_t vr = 0;                 //mrad/s
        uint8_t slot[4] = {0, 0, 0, 0};
        uint8_t vehicleByte = noStacato;    //byte 12 as received
    };

    //false (decoded untouched): not a reply. Any length but 13 is not one (the D-161 12 byte reply neither)
    inline bool decode(const uint8_t* frame, uint8_t frameLength, Decoded& d){
        if(frameLength != length) return false;
        uint16_t word = getUint16(&frame[0]);
        if((word & 0x7) != statusHeader) return false;
        d.status = statusFrom(word, frame[12]);
        d.vehicleByte = frame[12];
        d.vx = getInt16(&frame[2]);
        d.vy = getInt16(&frame[4]);
        d.vr = getInt16(&frame[6]);
        for(int i = 0; i < 4; i++) d.slot[i] = frame[8 + i];
        d.b_slowValues = d.status.page == PAGE_FLAGS;
        if(d.b_slowValues){
            d.rssiByte = frame[10];
            d.batteryByte = frame[11];
        }
        return true;
    }

    //——— the receiver's motion data (from Stacato's MAVLink) ———

    //ODOMETRY in body FRD (vx forward, vy right, yawspeed clockwise; [D-163] the pose x, y in m and q, the heading
    //t = 2 * atan2(q[3], q[0]), counter-clockwise, 0 upstage) and ACTUATOR_OUTPUT_STATUS (active = the flag word,
    //actuator[0..3] wheel velocity, [4..7] torque, normalized) as last received, each with its time. Stale after
    //staleMillis or without Stacato's heartbeat: then the reply says no motion (Telemetry.b_valid false) and no pose
    //(b_poseValid false). The velocities need both messages fresh (Stacato sends them together at 20 Hz), the pose
    //only ODOMETRY
    class MotionSource{
    public:

        static constexpr uint32_t staleMillis = 500;

        //the MAVLink parser (the receiver's loop; the USB remote's loop)
        void onOdometry(uint32_t now, float vxForward, float vyRight, float yawspeedClockwise,
                        float x, float y, float q0, float q3){
            //radio axes: x right = FRD y, y forward = FRD x, yaw clockwise = FRD yawspeed
            t.vx = vyRight;
            t.vy = vxForward;
            t.vr = yawspeedClockwise;
            t.poseX = x;
            t.poseY = y;
            t.heading = headingFromQuaternion(q0, q3);
            odometryMillis = now;
            b_odometry = true;
        }

        //rad, -pi..pi; NaN when q[0] or q[3] is NaN (an old Stacato)
        static float headingFromQuaternion(float q0, float q3){
            if(isnan(q0) || isnan(q3)) return NAN;
            const float pi = 3.14159265359f;
            float heading = 2.0f * atan2f(q3, q0);
            if(heading > pi) heading -= 2.0f * pi;
            if(heading < -pi) heading += 2.0f * pi;
            return heading;
        }

        void onActuators(uint32_t now, uint32_t active, const float* actuator){
            t.wheelFlags = uint16_t(active & 0xFFFF);
            for(int i = 0; i < 4; i++){
                t.wheelVelocity[i] = actuator[i];
                t.wheelTorque[i] = actuator[4 + i];
            }
            actuatorMillis = now;
            b_actuators = true;
        }

        //Stacato's heartbeat stopped: nothing received before counts any more
        void clear(){
            b_odometry = false;
            b_actuators = false;
        }

        Telemetry current(uint32_t now, bool b_stacatoOnline) const {
            Telemetry out = t;
            bool b_odometryFresh = b_stacatoOnline && b_odometry && now - odometryMillis <= staleMillis;
            out.b_valid = b_odometryFresh && b_actuators && now - actuatorMillis <= staleMillis;
            out.b_poseValid = b_odometryFresh;
            if(!out.b_poseValid){
                out.poseX = NAN;
                out.poseY = NAN;
                out.heading = NAN;
            }
            return out;
        }

    private:
        Telemetry t;
        bool b_odometry = false;
        bool b_actuators = false;
        uint32_t odometryMillis = 0;
        uint32_t actuatorMillis = 0;
    };

    //——— what the remote keeps (the Motion and Motors pages) ———

    //The last value of each paged field. The wheel velocity slot (every 100 ms) says whether motion data is live:
    //all four noData means the receiver has none (no Stacato, stale). Torques are known from their first page on
    //(within 400 ms), the flags within 200 ms
    struct MotionView{
        bool b_velocityLive = false;    //the last wheel velocity slot carried values
        float vx = 0.0f;                //m/s, right
        float vy = 0.0f;                //m/s, forward
        float vr = 0.0f;                //rad/s, clockwise
        float wheelVelocity[4] = {NAN, NAN, NAN, NAN};   //-1..1 of the drive limit, NaN: no value
        float wheelTorque[4] = {NAN, NAN, NAN, NAN};     //-1..1 of rated torque, NaN: no value
        bool b_torqueKnown = false;
        uint16_t wheelFlags = 0;
        bool b_flagsKnown = false;
        //[D-163] the pose in Stacato's world frame (x stage right, y upstage; heading in deg, counter-clockwise
        //positive, 0 upstage, wrapped to -180..180), carried forward between pose pages from vx, vy, vr; NaN: no value
        float poseX = NAN;              //m
        float poseY = NAN;              //m
        float heading = NAN;            //deg
        VehicleState vehicleState = VehicleState::NO_LINK;

        //the carry-forward's longest step: a gap between replies never extrapolates further
        static constexpr uint32_t maxStepMillis = 100;

        //each decoded reply with its arrival time (millis). Between pose pages the pose is carried forward while
        //motion data is live: the body velocity (radio axes: x right, y forward, yaw clockwise) turned into the world
        //frame with the heading t (counter-clockwise): dX = (vx cos t - vy sin t) dt, dY = (vx sin t + vy cos t) dt,
        //dt_heading = -vr dt, dt the time since the last reply (at most maxStepMillis), the velocities the mean of
        //this reply's and the last one's, t at mid-step. Then a pose page sets its values. No motion data (a velocity
        //page without values) or a pose page without a value: NaN
        void apply(const Decoded& d, uint32_t nowMillis){
            float lastVx = vx, lastVy = vy, lastVr = vr;
            vx = fromMilli(d.vx);
            vy = fromMilli(d.vy);
            vr = fromMilli(d.vr);
            vehicleState = Reply::vehicleState(d.vehicleByte);

            if(b_arrived && b_velocityLive){
                uint32_t stepMillis = nowMillis - lastArrivalMillis;
                if(stepMillis > maxStepMillis) stepMillis = maxStepMillis;
                carryForward((lastVx + vx) * 0.5f, (lastVy + vy) * 0.5f, (lastVr + vr) * 0.5f, float(stepMillis) / 1000.0f);
            }
            lastArrivalMillis = nowMillis;
            b_arrived = true;

            switch(d.status.page){
                case 0:
                case 2:
                case 4:
                case 6:{
                    bool b_any = false;
                    for(int i = 0; i < 4; i++){
                        wheelVelocity[i] = fromPercentByte(int8_t(d.slot[i]));
                        if(int8_t(d.slot[i]) != noData) b_any = true;
                    }
                    b_velocityLive = b_any;
                    if(!b_velocityLive){
                        poseX = NAN;
                        poseY = NAN;
                        heading = NAN;
                    }
                }break;
                case PAGE_TORQUE:
                    for(int i = 0; i < 4; i++) wheelTorque[i] = fromPercentByte(int8_t(d.slot[i]));
                    b_torqueKnown = true;
                    break;
                case PAGE_FLAGS:
                    wheelFlags = getUint16(&d.slot[0]);
                    b_flagsKnown = true;
                    break;
                case PAGE_POSE_XY:
                    poseX = fromPoseMilli(getInt16(&d.slot[0]));
                    poseY = fromPoseMilli(getInt16(&d.slot[2]));
                    break;
                case PAGE_HEADING:
                    heading = headingDegrees(getUint16(&d.slot[0]));
                    wheelFlags = getUint16(&d.slot[2]);
                    b_flagsKnown = true;
                    break;
            }
        }

        //one carry-forward step (body velocity in radio axes, dt in s). x and y need a heading: without one they
        //keep their values until the next pose page
        void carryForward(float bodyVx, float bodyVy, float bodyVrClockwise, float dt){
            if(isnan(heading)) return;
            const float degPerRad = 57.2957795131f;
            float turnDegrees = -bodyVrClockwise * dt * degPerRad;
            float midHeading = (heading + turnDegrees * 0.5f) / degPerRad;
            float c = cosf(midHeading), s = sinf(midHeading);
            if(!isnan(poseX)) poseX += (bodyVx * c - bodyVy * s) * dt;
            if(!isnan(poseY)) poseY += (bodyVx * s + bodyVy * c) * dt;
            heading = wrapDegrees(heading + turnDegrees);
        }

        //the link lost: nothing shown as live until the next replies
        void clear(){ *this = MotionView(); }

    private:
        uint32_t lastArrivalMillis = 0;
        bool b_arrived = false;
    };

    //what the Motion page can show
    enum class MotionAvailability : uint8_t{
        NO_LINK,            //no reply
        NO_DATA,            //replies, but the receiver has no fresh motion from Stacato (none sent, stale, no heartbeat)
        LIVE
    };

    inline MotionAvailability availability(bool b_linkUp, const MotionView& v){
        if(!b_linkUp) return MotionAvailability::NO_LINK;
        if(!v.b_velocityLive) return MotionAvailability::NO_DATA;
        return MotionAvailability::LIVE;
    }

};
