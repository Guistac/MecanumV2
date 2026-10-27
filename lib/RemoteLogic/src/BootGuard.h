#pragma once
#include <stdint.h>

enum class ResetCause : uint8_t{ POWER_ON, WATCHDOG, CRASH };

//Remote boot-loop protection [D-147, H13 point 5]: after 3 watchdog or crash resets within 60 s the remote starts with
//default settings and the Status page only. The history of abnormal resets is kept in EEPROM (from byte 4) with the
//RTC time in seconds, which runs on across warm resets; a power-on clears it
struct ResetHistory{

    static constexpr uint8_t address = 4;     //EEPROM: 3 x 4 bytes of times, then the count
    static constexpr uint8_t size = 13;
    static constexpr uint8_t depth = 3;
    static constexpr uint32_t windowSeconds = 60;

    uint32_t times[depth] = {0, 0, 0};          //newest first, 0 = empty
    uint8_t count = 0;                          //abnormal resets since power-on

    void toBytes(uint8_t bytes[size]) const {
        for(int i = 0; i < depth; i++){
            for(int b = 0; b < 4; b++) bytes[i * 4 + b] = (times[i] >> (8 * b)) & 0xFF;
        }
        bytes[12] = count;
    }

    //a blank EEPROM (all 0xFF) or times in the future (RTC reset) read as empty
    static ResetHistory fromBytes(const uint8_t bytes[size], uint32_t nowSeconds){
        ResetHistory history;
        for(int i = 0; i < depth; i++){
            uint32_t time = 0;
            for(int b = 0; b < 4; b++) time |= uint32_t(bytes[i * 4 + b]) << (8 * b);
            history.times[i] = (time == 0xFFFFFFFF || time > nowSeconds) ? 0 : time;
        }
        history.count = bytes[12] == 0xFF ? 0 : bytes[12];
        return history;
    }

    //records this boot, returns true when the remote should start in safe mode
    bool onBoot(ResetCause cause, uint32_t nowSeconds){
        if(cause == ResetCause::POWER_ON){
            *this = ResetHistory();
            return false;
        }
        for(int i = depth - 1; i > 0; i--) times[i] = times[i - 1];
        times[0] = nowSeconds == 0 ? 1 : nowSeconds;
        if(count < 255) count++;
        for(int i = 0; i < depth; i++){
            if(times[i] == 0 || times[0] - times[i] > windowSeconds) return false;
        }
        return true;
    }
};
