#pragma once
#include <stdint.h>

//Remote settings kept in EEPROM [D-147 point 2, H12]. Every value is range checked when loaded and falls back to its
//default; only changed bytes are written, once, when the operator confirms
struct RemoteSettings{

    //EEPROM layout: byte 0 is the channel as before, history of abnormal resets from byte 4 (BootGuard)
    static constexpr uint8_t channelAddress = 0;
    static constexpr uint8_t contrastAddress = 1;
    static constexpr uint8_t dimmingAddress = 2;

    static constexpr uint8_t channelCount = 16;
    static constexpr uint8_t contrastLevels = 4;
    static constexpr uint8_t dimmingOptions = 3;

    //a blank EEPROM (0xFF) used to end on channel 15 (Radio::setChannel clamps), keep that for remotes never set
    static constexpr uint8_t defaultChannel = 15;
    //level 1 is 0x2F, the display library's own default, as the remote always ran
    static constexpr uint8_t defaultContrast = 1;
    //2 minutes
    static constexpr uint8_t defaultDimming = 2;

    uint8_t channel = defaultChannel;
    uint8_t contrast = defaultContrast;
    uint8_t dimming = defaultDimming;

    static RemoteSettings fromStored(uint8_t storedChannel, uint8_t storedContrast, uint8_t storedDimming){
        RemoteSettings settings;
        if(storedChannel < channelCount) settings.channel = storedChannel;
        if(storedContrast < contrastLevels) settings.contrast = storedContrast;
        if(storedDimming < dimmingOptions) settings.dimming = storedDimming;
        return settings;
    }

    bool operator==(const RemoteSettings& other) const {
        return channel == other.channel && contrast == other.contrast && dimming == other.dimming;
    }
    bool operator!=(const RemoteSettings& other) const { return !(*this == other); }

    //SSD1305 contrast register value for a level
    static uint8_t contrastValue(uint8_t level){
        static const uint8_t values[contrastLevels] = {0x10, 0x2F, 0x70, 0xFF};
        return values[level < contrastLevels ? level : defaultContrast];
    }

    //dimming delay, 0 = never dims
    static uint32_t dimmingDelayMillis(uint8_t option){
        static const uint32_t delays[dimmingOptions] = {0, 30000, 120000};
        return delays[option < dimmingOptions ? option : defaultDimming];
    }

    static const char* dimmingLabel(uint8_t option){
        static const char* labels[dimmingOptions] = {"Off", "30s", "2min"};
        return labels[option < dimmingOptions ? option : defaultDimming];
    }
};
