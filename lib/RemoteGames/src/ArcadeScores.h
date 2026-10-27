#pragma once
#include <stdint.h>
#include <string.h>

//The arcade's scores and settings kept in EEPROM [D-157]: a top 3 per game with a 4-letter name, the sound mode and
//the last name entered. The remote keeps EEPROM bytes 0 to 255 for itself; the arcade's block starts at 256 (the
//board's ArcadeStore adds it). Layout of the block (offsets in it):
//
//  0       marker 0xA7
//  1       layout version 1
//  2       sound mode (0..3, ArcadeSoundMode)
//  3..6    last name entered (4 characters)
//  8..199  8 game slots (7 games, 1 spare) of 24 bytes: a top 3 of entries, best first. An entry is 8 bytes: the
//          score (uint32_t, little endian), then the 4 name characters. Empty when its name is not 4 valid
//          characters (A to Z, 0 to 9, space): blank EEPROM (0xFF) reads as empty
//
//A slot is the game's fixed id (slotFor: Pong 0, Parking 1, Cave 2, Rocks 3, Snake 4, Blocks 5, Plume 6), not its
//place in the picker. A wrong marker or version reads as all empty with the defaults (SOUND ON, AAAA); the whole
//block is written at the first change. Written only at a new top-3 entry, a sound mode change and a table clear
//(the owner's exception to D-147's "EEPROM only on Save").

//where the block lives: the board on EEPROM at 256 + offset (writing only bytes that change), tests in RAM
class ArcadeStore{
public:
    virtual uint8_t read(uint16_t offset) = 0;
    virtual void write(uint16_t offset, uint8_t value) = 0;
};

//a block in RAM, blank (0xFF) to begin with: tests and the screen renderer
class RamArcadeStore : public ArcadeStore{
public:
    static const uint16_t size = 256;
    RamArcadeStore(){ memset(bytes, 0xFF, sizeof(bytes)); }
    uint8_t read(uint16_t offset) override { return offset < size ? bytes[offset] : 0xFF; }
    void write(uint16_t offset, uint8_t value) override {
        if(offset >= size) return;
        if(bytes[offset] != value) changes++;
        bytes[offset] = value;
        writes++;
    }
    uint8_t bytes[size];
    uint32_t writes = 0;        //write calls
    uint32_t changes = 0;       //of them, bytes that changed
};

class ArcadeScores{
public:
    static const uint8_t marker = 0xA7;
    static const uint8_t version = 1;
    static const uint16_t markerOffset = 0;
    static const uint16_t versionOffset = 1;
    static const uint16_t soundModeOffset = 2;
    static const uint16_t lastNameOffset = 3;
    static const uint16_t slotsOffset = 8;
    static const uint8_t slotCount = 8;
    static const uint8_t ranks = 3;
    static const uint8_t nameLength = 4;
    static const uint16_t entrySize = 8;
    static const uint16_t slotSize = ranks * entrySize;
    static const uint16_t blockSize = slotsOffset + slotCount * slotSize;
    static const uint8_t soundModeCount = 4;
    static const uint8_t noRank = 0xFF;

    struct Entry{
        uint32_t score = 0;
        char name[nameLength + 1] = {0, 0, 0, 0, 0};
        bool b_used = false;
    };

    //the game's slot from its name; an unknown game gets the spare slot (7)
    static uint8_t slotFor(const char* gameName);
    //a character a name may have: A to Z, 0 to 9, space
    static bool isNameCharacter(char c);

    //reads the block (nullptr: kept in RAM only, as if blank)
    void load(ArcadeStore* store);

    const Entry& entry(uint8_t slot, uint8_t rank) const { return table[slot % slotCount][rank % ranks]; }
    bool hasEntries(uint8_t slot) const { return entry(slot, 0).b_used; }

    //the rank (0..2) a score would take in a slot, or noRank: it must be strictly better than an entry to pass it,
    //and 0 never enters unless lower is better
    uint8_t rankFor(uint8_t slot, uint32_t score, bool b_lowerIsBetter) const;
    //puts the score in (the entries below move down, the last falls out) and writes the slot and the name as the
    //last name; returns its rank or noRank
    uint8_t insert(uint8_t slot, uint32_t score, const char* name, bool b_lowerIsBetter);
    //empties a slot and writes it
    void clear(uint8_t slot);

    uint8_t getSoundMode() const { return soundMode; }
    void setSoundMode(uint8_t mode);
    const char* getLastName() const { return lastName; }

private:
    void format();
    void writeHeader();
    void writeSlot(uint8_t slot);
    void put(uint16_t offset, uint8_t value);

    ArcadeStore* store = nullptr;
    bool b_formatted = false;           //the marker and version are in the store
    Entry table[slotCount][ranks];
    uint8_t soundMode = 0;
    char lastName[nameLength + 1] = {'A', 'A', 'A', 'A', 0};
};
