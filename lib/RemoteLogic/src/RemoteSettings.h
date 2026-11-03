#pragma once
#include <stdint.h>
#include "RemoteSounds.h"
#include "LiIonBattery.h"

//Remote settings kept in EEPROM [D-147 point 2, H12]. Every value is range checked when loaded and falls back to its
//default; only changed bytes are written, once, when the operator confirms
struct RemoteSettings{

    //EEPROM layout: byte 0 is the channel as before, 1 contrast, 2 dimming, 3 sound, history of abnormal resets
    //4..16 (BootGuard), 17 the vehicle's pack, stick calibration 20..44 (StickCalibration); the arcade's block from
    //256 (ArcadeScores), the Tracking page's marks 512..659 (MarkStore)
    static constexpr uint8_t channelAddress = 0;
    static constexpr uint8_t contrastAddress = 1;
    static constexpr uint8_t dimmingAddress = 2;
    static constexpr uint8_t soundAddress = 3;      //[D-158]
    static constexpr uint8_t packAddress = 17;      //[owner 2026-10-28]

    static constexpr uint8_t channelCount = 16;
    //[owner 2026-10-28] after channel F the channel setting has Off: the radio sends nothing (it stays powered and
    //listening, never shut down: its amplifier comes on in shutdown) and the Status page says Radio off, LEDs dark;
    //the remote is a game console then. The USB link (D-159) still works. Stored as 16 in the channel byte
    static constexpr uint8_t radioOffChannel = channelCount;
    static constexpr uint8_t contrastLevels = 4;
    static constexpr uint8_t dimmingOptions = 3;
    //[owner 2026-10-28] the vehicle's pack, for its charge in percent from the voltage the receiver sends (45 to 70 V
    //in 255 steps): Volts shows the voltage as before; a Li-ion pack of 14, 15 or 16 cells in series the charge from
    //the remote's own Li-ion curve per cell (LiIonBattery, a resting-voltage indicator: it drops under load). Smaller
    //packs do not fit the receiver's 45 V floor
    static constexpr uint8_t packOptions = 4;
    static constexpr uint8_t defaultPack = 0;

    //a blank EEPROM (0xFF) used to end on channel 15 (Radio::setChannel clamps), keep that for remotes never set
    static constexpr uint8_t defaultChannel = 15;
    //level 1 is 0x2F, the display library's own default, as the remote always ran
    static constexpr uint8_t defaultContrast = 1;
    //2 minutes
    static constexpr uint8_t defaultDimming = 2;

    uint8_t channel = defaultChannel;
    uint8_t contrast = defaultContrast;
    uint8_t dimming = defaultDimming;
    uint8_t sound = RemoteSounds::defaultLevel;
    uint8_t pack = defaultPack;

    static RemoteSettings fromStored(uint8_t storedChannel, uint8_t storedContrast, uint8_t storedDimming,
                                     uint8_t storedSound = 0xFF, uint8_t storedPack = 0xFF){
        RemoteSettings settings;
        if(storedChannel <= radioOffChannel) settings.channel = storedChannel;
        if(storedContrast < contrastLevels) settings.contrast = storedContrast;
        if(storedDimming < dimmingOptions) settings.dimming = storedDimming;
        if(storedSound < RemoteSounds::levelCount) settings.sound = storedSound;
        if(storedPack < packOptions) settings.pack = storedPack;
        return settings;
    }

    bool isRadioOff() const { return channel == radioOffChannel; }

    bool operator==(const RemoteSettings& other) const {
        return channel == other.channel && contrast == other.contrast && dimming == other.dimming && sound == other.sound
            && pack == other.pack;
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

    //cells in series of a pack option, 0: Volts
    static uint8_t packCells(uint8_t option){
        static const uint8_t cells[packOptions] = {0, 14, 15, 16};
        return cells[option < packOptions ? option : defaultPack];
    }

    static const char* packLabel(uint8_t option){
        static const char* labels[packOptions] = {"Volts", "Li-ion 14S", "Li-ion 15S", "Li-ion 16S"};
        return labels[option < packOptions ? option : defaultPack];
    }

    //the vehicle's charge in percent from its pack voltage; -1 for Volts
    static int8_t packPercent(uint8_t option, float volts){
        uint8_t cells = packCells(option);
        if(cells == 0) return -1;
        return LiIonBattery::percentFromCellVoltage(volts / float(cells));
    }

    static const char* dimmingLabel(uint8_t option){
        static const char* labels[dimmingOptions] = {"Off", "30s", "2min"};
        return labels[option < dimmingOptions ? option : defaultDimming];
    }
};
