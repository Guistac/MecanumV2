#pragma once
#include <stdint.h>
#include "RemoteSettings.h"

//Remote display brightness [D-146 point 5, amended 2026-10-27]: the contrast setting, dropped to the lowest value after
//the dimming delay without activity (never off); never dimmed while an alarm shows. An alarm used to force full
//brightness, which hid the contrast setting while an alarm stayed (NO LINK on the bench)
class Dimmer{
public:

    static constexpr uint8_t dimmedValue = 0x01;

    //b_activity: any operator input, a new big line or a new warning this loop
    //returns the SSD1305 contrast value to apply
    uint8_t update(uint8_t contrastLevel, uint8_t dimmingOption, bool b_activity, bool b_alarm, uint32_t nowMillis){
        if(b_activity || b_alarm) lastActivityMillis = nowMillis;
        uint32_t delayMillis = RemoteSettings::dimmingDelayMillis(dimmingOption);
        b_dimmed = delayMillis > 0 && nowMillis - lastActivityMillis > delayMillis;
        return b_dimmed ? dimmedValue : RemoteSettings::contrastValue(contrastLevel);
    }

    bool isDimmed() const { return b_dimmed; }

private:
    uint32_t lastActivityMillis = 0;
    bool b_dimmed = false;
};
