#pragma once
#include <stdint.h>
#include "RemoteSettings.h"

//Remote display brightness [D-146 point 5]: the contrast setting, dropped to the lowest value after the dimming delay
//without activity (never off), full brightness whenever an alarm shows, whatever the setting
class Dimmer{
public:

    static constexpr uint8_t dimmedValue = 0x01;
    static constexpr uint8_t alarmValue = 0xFF;

    //b_activity: any operator input, a new big line or a new warning this loop
    //returns the SSD1305 contrast value to apply
    uint8_t update(uint8_t contrastLevel, uint8_t dimmingOption, bool b_activity, bool b_alarm, uint32_t nowMillis){
        if(b_activity || b_alarm) lastActivityMillis = nowMillis;
        if(b_alarm) {
            b_dimmed = false;
            return alarmValue;
        }
        uint32_t delayMillis = RemoteSettings::dimmingDelayMillis(dimmingOption);
        b_dimmed = delayMillis > 0 && nowMillis - lastActivityMillis > delayMillis;
        return b_dimmed ? dimmedValue : RemoteSettings::contrastValue(contrastLevel);
    }

    bool isDimmed() const { return b_dimmed; }

private:
    uint32_t lastActivityMillis = 0;
    bool b_dimmed = false;
};
