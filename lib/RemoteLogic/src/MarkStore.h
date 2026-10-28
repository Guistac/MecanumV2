#pragma once
#include <stdint.h>
#include <math.h>

//The Tracking page's marks (owner, 2026-10-28): Z, the zero, and marks 1 to 9, each a place in the display frame
//(the frame the Motion and Tracking pages show: Stacato's pose through the display correction, PoseZero): x, y in m,
//a heading in deg (counter-clockwise, may be blank: a place only) and a time: a DURATION, how long the move there
//should take (may be blank: no timed run to it; TimedTarget.h). Kept as whole units: mm, tenths of a degree, tenths of
//a second, so what is stored is what is shown. Z is always 0, 0, 0 without a duration and never stored. (The layout is
//unchanged from the ordered route's arrival times: a stored time now reads as a duration.)
//
//EEPROM (the board writes only the bytes that change, once per confirmed change), the block at byte 512 (after the
//remote's own bytes 0 to 255 and the arcade's 256 to 511), 148 bytes, 512 to 659:
//  0       marker 0x4B
//  1       layout version 1
//  2, 3    0xFF (spare)
//  4..147  marks 1 to 9, 16 bytes each:
//          0      flags: bit 0 used, bit 1 heading, bit 2 time
//          1..4   x in mm, int32 little endian
//          5..8   y in mm, int32 little endian
//          9, 10  heading in tenths of a degree, int16 little endian (-1800..1799)
//          11, 12 duration in tenths of a second, uint16 little endian (0..59999: 99:59.9)
//          13     check: the sum of bytes 0..12, xor 0xA5
//          14, 15 0xFF (spare)
//A wrong marker or version (blank EEPROM, another layout) reads as all empty; a record whose check or values are wrong
//reads as that mark empty.
struct Mark{
    bool b_used = false;
    int32_t xMm = 0;
    int32_t yMm = 0;
    bool b_heading = false;
    int16_t headingTenths = 0;
    bool b_time = false;
    uint16_t timeTenths = 0;

    static constexpr int32_t limitMm = 99999;           //+-99.999 m
    static constexpr uint16_t maxTimeTenths = 59999;    //99:59.9

    float x() const { return float(xMm) * 0.001f; }
    float y() const { return float(yMm) * 0.001f; }
    float heading() const { return b_heading ? float(headingTenths) * 0.1f : NAN; }
    uint32_t timeMillis() const { return uint32_t(timeTenths) * 100u; }

    //a heading in tenths, wrapped to -1800..1799
    static int16_t wrapTenths(int32_t tenths){
        tenths %= 3600;
        if(tenths < 0) tenths += 3600;
        if(tenths >= 1800) tenths -= 3600;
        return int16_t(tenths);
    }
    static int32_t clampMm(int32_t mm){ return mm > limitMm ? limitMm : (mm < -limitMm ? -limitMm : mm); }

    //a mark from a pose in the display frame (heading NaN: blank) and a time (negative: blank)
    static Mark fromPose(float x, float y, float headingDeg, int32_t timeMillis = -1){
        Mark m;
        m.b_used = true;
        m.xMm = clampMm(int32_t(lroundf(x * 1000.0f)));
        m.yMm = clampMm(int32_t(lroundf(y * 1000.0f)));
        m.b_heading = !isnan(headingDeg);
        m.headingTenths = m.b_heading ? wrapTenths(int32_t(lroundf(headingDeg * 10.0f))) : 0;
        m.b_time = timeMillis >= 0;
        uint32_t tenths = m.b_time ? (uint32_t(timeMillis) + 50u) / 100u : 0;
        m.timeTenths = uint16_t(tenths > maxTimeTenths ? maxTimeTenths : tenths);
        return m;
    }

    bool operator==(const Mark& o) const {
        if(b_used != o.b_used) return false;
        if(!b_used) return true;
        return xMm == o.xMm && yMm == o.yMm && b_heading == o.b_heading && (!b_heading || headingTenths == o.headingTenths)
            && b_time == o.b_time && (!b_time || timeTenths == o.timeTenths);
    }
    bool operator!=(const Mark& o) const { return !(*this == o); }
};

class MarkStore{
public:
    static constexpr uint8_t count = 10;                //Z and 1..9
    static constexpr uint8_t zero = 0;
    static constexpr uint16_t address = 512;
    static constexpr uint8_t marker = 0x4B;
    static constexpr uint8_t version = 1;
    static constexpr uint16_t headerSize = 4;
    static constexpr uint16_t recordSize = 16;
    static constexpr uint16_t size = headerSize + 9 * recordSize;      //148: bytes 512..659

    MarkStore(){ clearAll(); }

    const Mark& get(uint8_t k) const { return marks[k < count ? k : 0]; }
    bool isUsed(uint8_t k) const { return k < count && marks[k].b_used; }

    //marks 1..9 only (Z is fixed); returns true when it changed
    bool set(uint8_t k, const Mark& m){
        if(k == zero || k >= count) return false;
        Mark n = m;
        if(n.b_used){
            n.xMm = Mark::clampMm(n.xMm);
            n.yMm = Mark::clampMm(n.yMm);
            n.headingTenths = n.b_heading ? Mark::wrapTenths(n.headingTenths) : 0;
            if(!n.b_time) n.timeTenths = 0;
            if(n.timeTenths > Mark::maxTimeTenths) n.timeTenths = Mark::maxTimeTenths;
        }
        else n = Mark();
        if(n == marks[k]) return false;
        marks[k] = n;
        return true;
    }
    bool clear(uint8_t k){ return set(k, Mark()); }

    void clearAll(){
        for(uint8_t k = 0; k < count; k++) marks[k] = Mark();
        marks[zero].b_used = true;
        marks[zero].b_heading = true;
    }

    //the whole image (the board compares it with the EEPROM and writes the bytes that differ)
    void toBytes(uint8_t bytes[size]) const {
        for(uint16_t i = 0; i < size; i++) bytes[i] = 0xFF;
        bytes[0] = marker;
        bytes[1] = version;
        for(uint8_t k = 1; k < count; k++){
            uint8_t* r = bytes + headerSize + (k - 1) * recordSize;
            const Mark& m = marks[k];
            if(!m.b_used) continue;     //an empty mark stays all 0xFF
            r[0] = uint8_t(0x01 | (m.b_heading ? 0x02 : 0) | (m.b_time ? 0x04 : 0));
            put32(r + 1, uint32_t(m.xMm));
            put32(r + 5, uint32_t(m.yMm));
            uint16_t h = uint16_t(m.b_heading ? m.headingTenths : 0);
            r[9] = uint8_t(h);
            r[10] = uint8_t(h >> 8);
            uint16_t t = m.b_time ? m.timeTenths : 0;
            r[11] = uint8_t(t);
            r[12] = uint8_t(t >> 8);
            r[13] = check(r);
        }
    }

    void fromBytes(const uint8_t bytes[size]){
        clearAll();
        if(bytes[0] != marker || bytes[1] != version) return;
        for(uint8_t k = 1; k < count; k++){
            const uint8_t* r = bytes + headerSize + (k - 1) * recordSize;
            if(r[0] == 0xFF || (r[0] & 0x01) == 0 || (r[0] & 0xF8) != 0 || r[13] != check(r)) continue;
            Mark m;
            m.b_used = true;
            m.b_heading = (r[0] & 0x02) != 0;
            m.b_time = (r[0] & 0x04) != 0;
            m.xMm = int32_t(get32(r + 1));
            m.yMm = int32_t(get32(r + 5));
            int16_t h = int16_t(uint16_t(r[9]) | uint16_t(r[10]) << 8);
            uint16_t t = uint16_t(uint16_t(r[11]) | uint16_t(r[12]) << 8);
            bool b_valid = m.xMm >= -Mark::limitMm && m.xMm <= Mark::limitMm && m.yMm >= -Mark::limitMm && m.yMm <= Mark::limitMm
                        && (!m.b_heading || (h >= -1800 && h < 1800)) && (!m.b_time || t <= Mark::maxTimeTenths);
            if(!b_valid) continue;
            m.headingTenths = m.b_heading ? h : 0;
            m.timeTenths = m.b_time ? t : 0;
            marks[k] = m;
        }
    }

private:
    Mark marks[count];

    static void put32(uint8_t* p, uint32_t v){ for(int i = 0; i < 4; i++) p[i] = uint8_t(v >> (8 * i)); }
    static uint32_t get32(const uint8_t* p){
        uint32_t v = 0;
        for(int i = 0; i < 4; i++) v |= uint32_t(p[i]) << (8 * i);
        return v;
    }
    static uint8_t check(const uint8_t* r){
        uint8_t sum = 0;
        for(int i = 0; i < 13; i++) sum = uint8_t(sum + r[i]);
        return uint8_t(sum ^ 0xA5);
    }
};
