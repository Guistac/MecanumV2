//The arcade's top 3s and sound mode in EEPROM [D-157]: the layout is in ArcadeScores.h
#include "ArcadeScores.h"

uint8_t ArcadeScores::slotFor(const char* gameName){
    static const char* const names[] = {"PONG", "PARKING", "CAVE", "ROCKS", "SNAKE", "BLOCKS", "PLUME"};
    for(uint8_t i = 0; i < sizeof(names) / sizeof(names[0]); i++) if(strcmp(gameName, names[i]) == 0) return i;
    return slotCount - 1;
}

bool ArcadeScores::isNameCharacter(char c){
    return (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == ' ';
}

void ArcadeScores::load(ArcadeStore* newStore){
    store = newStore;
    for(uint8_t s = 0; s < slotCount; s++) for(uint8_t r = 0; r < ranks; r++) table[s][r] = Entry();
    soundMode = 0;
    memcpy(lastName, "AAAA", nameLength + 1);
    b_formatted = store && store->read(markerOffset) == marker && store->read(versionOffset) == version;
    if(!b_formatted) return;

    uint8_t mode = store->read(soundModeOffset);
    soundMode = mode < soundModeCount ? mode : 0;
    char name[nameLength + 1] = {0, 0, 0, 0, 0};
    bool b_valid = true;
    for(uint8_t i = 0; i < nameLength; i++){
        name[i] = char(store->read(lastNameOffset + i));
        if(!isNameCharacter(name[i])) b_valid = false;
    }
    if(b_valid) memcpy(lastName, name, nameLength + 1);

    for(uint8_t s = 0; s < slotCount; s++){
        uint8_t kept = 0;
        for(uint8_t r = 0; r < ranks; r++){
            uint16_t at = slotsOffset + s * slotSize + r * entrySize;
            Entry e;
            e.b_used = true;
            for(uint8_t i = 0; i < 4; i++) e.score |= uint32_t(store->read(at + i)) << (8 * i);
            for(uint8_t i = 0; i < nameLength; i++){
                e.name[i] = char(store->read(at + 4 + i));
                if(!isNameCharacter(e.name[i])) e.b_used = false;
            }
            //an empty entry leaves no gap: the ones after it move up
            if(e.b_used) table[s][kept++] = e;
        }
    }
}

uint8_t ArcadeScores::rankFor(uint8_t slot, uint32_t score, bool b_lowerIsBetter) const {
    if(score == 0 && !b_lowerIsBetter) return noRank;
    for(uint8_t r = 0; r < ranks; r++){
        const Entry& e = entry(slot, r);
        if(!e.b_used) return r;
        if(b_lowerIsBetter ? score < e.score : score > e.score) return r;
    }
    return noRank;
}

uint8_t ArcadeScores::insert(uint8_t slot, uint32_t score, const char* name, bool b_lowerIsBetter){
    uint8_t rank = rankFor(slot, score, b_lowerIsBetter);
    if(rank == noRank) return noRank;
    slot %= slotCount;
    for(uint8_t r = ranks - 1; r > rank; r--) table[slot][r] = table[slot][r - 1];
    Entry& e = table[slot][rank];
    e.score = score;
    e.b_used = true;
    for(uint8_t i = 0; i < nameLength; i++){
        char c = name[i];
        if(!isNameCharacter(c)) c = ' ';
        e.name[i] = c;
        lastName[i] = c;
    }
    e.name[nameLength] = 0;
    lastName[nameLength] = 0;
    format();
    for(uint8_t i = 0; i < nameLength; i++) put(lastNameOffset + i, uint8_t(lastName[i]));
    writeSlot(slot);
    return rank;
}

void ArcadeScores::clear(uint8_t slot){
    slot %= slotCount;
    for(uint8_t r = 0; r < ranks; r++) table[slot][r] = Entry();
    format();
    writeSlot(slot);
}

void ArcadeScores::setSoundMode(uint8_t mode){
    mode %= soundModeCount;
    if(mode == soundMode) return;
    soundMode = mode;
    format();
    put(soundModeOffset, soundMode);
}

//the first write to a blank or foreign block writes all of it: the header, the defaults and the slots as they are
void ArcadeScores::format(){
    if(b_formatted || !store) return;
    b_formatted = true;
    writeHeader();
    for(uint8_t s = 0; s < slotCount; s++) writeSlot(s);
}

void ArcadeScores::writeHeader(){
    put(markerOffset, marker);
    put(versionOffset, version);
    put(soundModeOffset, soundMode);
    for(uint8_t i = 0; i < nameLength; i++) put(lastNameOffset + i, uint8_t(lastName[i]));
    put(lastNameOffset + nameLength, 0xFF);
}

void ArcadeScores::writeSlot(uint8_t slot){
    for(uint8_t r = 0; r < ranks; r++){
        const Entry& e = table[slot][r];
        uint16_t at = slotsOffset + slot * slotSize + r * entrySize;
        for(uint8_t i = 0; i < 4; i++) put(at + i, e.b_used ? uint8_t(e.score >> (8 * i)) : 0xFF);
        for(uint8_t i = 0; i < nameLength; i++) put(at + 4 + i, e.b_used ? uint8_t(e.name[i]) : 0xFF);
    }
}

void ArcadeScores::put(uint16_t offset, uint8_t value){
    if(store) store->write(offset, value);
}
