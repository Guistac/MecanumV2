#pragma once
#include <EEPROM.h>
#include <ArcadeScores.h>

//The arcade's scores and sound mode in the remote's EEPROM [D-157]: its block (layout in lib/RemoteGames ArcadeScores.h)
//from byte 256, after the remote's own bytes 0 to 255. Writes only the bytes that change. Main thread.
class ArcadeEeprom : public ArcadeStore{
public:
    static const uint16_t base = 256;
    uint8_t read(uint16_t offset) override { return EEPROM.read(base + offset); }
    void write(uint16_t offset, uint8_t value) override {
        if(EEPROM.read(base + offset) != value) EEPROM.write(base + offset, value);
    }
};
