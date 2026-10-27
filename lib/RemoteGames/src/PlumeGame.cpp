//PLUME: the canister on its CO2 plume [D-157]. Design: docs/plume/README.md; in the Arcade it draws only play, ticks
//at 50 Hz with every value per second as designed, and its best scores are the Arcade's top 3. The shaft is one
//32-bit word per world column (bit y = rock at row y) in a ring generated ahead of the camera: smooth noise blobs,
//boxes stamped on them, and from time to time a plug across the whole shaft that must be blasted through. Gems and
//hearts are placed by the same generator, some in open space, some buried in a lump of rock with their own shape cut
//out of it.
#include "PlumeGame.h"
#include <stdio.h>
#include <math.h>

namespace{

    //the canister, 5 x 3, bit 4 is its lower end (screen left), the nozzle at the top end (screen right)
    const uint8_t shipRows[PlumeGame::shipHeight] = {
        0x1E,   //####.
        0x1F,   //#####
        0x1E,   //####.
    };

    const float pi = 3.14159265f;

    //the heart as it reads in the sideways grip (top row first, bit 4 the left column), drawn a quarter turn on screen
    const uint8_t heartRows[PlumeGame::heartSize] = {
        0x0A,   //.#.#.
        0x1F,   //#####
        0x1F,   //#####
        0x0E,   //.###.
        0x04,   //..#..
    };

    //collectibles: the share buried in rock and how deep, start (d = 0) and cap (d = 1)
    const float gemBuriedStart = 0.3f;              //about 40 % over a run
    const float gemBuriedCap = 0.5f;
    const float heartBuriedStart = 0.65f;           //most hearts
    const float heartBuriedCap = 0.9f;
    const float depthStart = 1.0f;                  //px of rock around a buried item, and up to 1 more at random
    const float depthCap = 3.0f;

    //difficulty knobs, start (d = 0) and cap (d = 1)
    const float coverageThresholdStart = 0.33f;     //noise above this is rock: about 8 % of the shaft
    const float coverageThresholdCap = 0.08f;       //about 35 %
    const float blobSizeStart = 24.0f;
    const float blobSizeCap = 10.0f;
    const float boxesPer100mStart = 2.0f;
    const float boxesPer100mCap = 8.0f;
    const float plugSpacingStart = 1500.0f;         //px (150 m)
    const float plugSpacingCap = 600.0f;
    const float difficultyPx = 5000.0f;             //500 m

    float clampf(float v, float lowest, float highest){ return v < lowest ? lowest : (v > highest ? highest : v); }
    float lerpf(float a, float b, float t){ return a + (b - a) * t; }
    float difficultyAt(float px){ return clampf(px / difficultyPx, 0.0f, 1.0f); }

    uint32_t hash(int32_t x, int32_t y, uint32_t seed){
        uint32_t h = uint32_t(x) * 374761393u + uint32_t(y) * 668265263u + seed * 2246822519u;
        h = (h ^ (h >> 13)) * 1274126177u;
        return h ^ (h >> 16);
    }

    //the gradients of the noise: eight directions
    const float gradientX[8] = {1.0f, 0.7071f, 0.0f, -0.7071f, -1.0f, -0.7071f, 0.0f, 0.7071f};
    const float gradientY[8] = {0.0f, 0.7071f, 1.0f, 0.7071f, 0.0f, -0.7071f, -1.0f, -0.7071f};

    float fade(float t){ return t * t * t * (t * (t * 6.0f - 15.0f) + 10.0f); }

    //sound: the ambient tune, four chords of two bars, eight steps of 250 ms a bar (16 s, then again). Our own:
    //each chord rises in a sparse arpeggio, then falls back in the second bar; a soft bass under it, a bell now and then
    const uint8_t tuneStepMillis = 250;
    const uint8_t chordNotes[4][5] = {
        {SoundKit::A3, SoundKit::E4, SoundKit::G4, SoundKit::B4, SoundKit::C5},     //A minor, ninth
        {SoundKit::F3, SoundKit::C4, SoundKit::E4, SoundKit::A4, SoundKit::B4},     //F major seventh, sharp eleventh
        {SoundKit::C4, SoundKit::G4, SoundKit::B4, SoundKit::D5, SoundKit::E5},     //C major, ninth
        {SoundKit::E3, SoundKit::B3, SoundKit::D4, SoundKit::G4, SoundKit::A4},     //E minor seventh, eleventh
    };
    const uint8_t chordBass[4] = {SoundKit::A2, SoundKit::F2, SoundKit::C3, SoundKit::E2};
    const uint8_t chordBell[4] = {SoundKit::E6, 0, SoundKit::G5, 0};
    //where the arpeggio's notes fall in the two bars, and which of the chord's notes
    const uint8_t risingSteps[5] = {0, 1, 2, 4, 5};
    const uint8_t fallingSteps[4] = {8, 10, 11, 13};
    const uint8_t fallingNotes[4] = {4, 2, 3, 1};
    const uint8_t tuneLevel = 95;
    const uint8_t bassLevel = 60;
    const uint8_t bellLevel = 60;
    //written for the disc (loud only from about 1 kHz to 5 kHz, nearly silent below 400 Hz): the arpeggio played two
    //octaves above the chords as written (E5 to E7), the bass two octaves up too (E4 to C5), the bell one
    const uint8_t arpeggioUp = 24;
    const uint8_t bassUp = 24;
    const uint8_t bellUp = 12;
};

PlumeGame::PlumeGame(){
    //SoundKit:: written out here: on the Teensy the pin names A0, A1... clash with its note names
    //the tune, written once: the player keeps a pointer to it
    SoundKit::Tracker t(tune, tuneStepMillis);
    t.instrument(0, Synth::Wave::TRIANGLE, 100, 600)
     .instrument(1, Synth::Wave::TRIANGLE, 95, 1400)
     .instrument(2, Synth::Wave::SINE, 100, 400);
    for(uint8_t c = 0; c < 4; c++){
        uint16_t at = uint16_t(c * 16);
        for(uint8_t i = 0; i < 5; i++){
            t.note(uint16_t(at + risingSteps[i]), 0, uint8_t(chordNotes[c][i] + arpeggioUp), 2, tuneLevel);
        }
        for(uint8_t i = 0; i < 4; i++){
            t.note(uint16_t(at + fallingSteps[i]), 0, uint8_t(chordNotes[c][fallingNotes[i]] + arpeggioUp), 2,
                   uint8_t(tuneLevel - 4));
        }
        t.note(at, 1, uint8_t(chordBass[c] + bassUp), 8, bassLevel);
        t.note(uint16_t(at + 8), 1, uint8_t(chordBass[c] + bassUp), 8, uint8_t(bassLevel - 10));
        if(chordBell[c]) t.note(uint16_t(at + 12), 2, uint8_t(chordBell[c] + bellUp), 4, bellLevel);
    }
    tune.cue(SoundPriority::GAME, uint16_t(64 * tuneStepMillis));

    //hull hit: a dull clunk, a low square stepping down and a knock of noise
    clunkNotes.sweep(0, 150, 70, 60, 3, Synth::Wave::SQUARE, 95, 0, 20);
    clunkNotes.burst(0, 1200, 35, 80, 20, 1);
    //repair: two rising notes
    repairNotes.synth(0, SoundKit::hertz(SoundKit::E5), 80, 88, 20, Synth::Wave::TRIANGLE);
    repairNotes.synth(90, SoundKit::hertz(SoundKit::A5), 130, 90, 90, Synth::Wave::TRIANGLE);
    //every 100 m: a ping with a soft echo
    pingNotes.synth(0, SoundKit::hertz(SoundKit::A6), 180, 92, 140, Synth::Wave::SINE);
    pingNotes.synth(0, SoundKit::hertz(SoundKit::E7), 110, 60, 80, Synth::Wave::SINE, 1);
    pingNotes.synth(220, SoundKit::hertz(SoundKit::A6), 150, 48, 110, Synth::Wave::SINE);
    //fell out: a long falling whistle
    whistleNotes.sweep(0, 1900, 220, 1150, 24, Synth::Wave::SINE, 92, 0, 220);
    //the hull gone: the canister bursts, a wide noise and a low growl under it
    burstNotes.burst(0, 2200, 320, 100, 160);
    burstNotes.sweep(0, 110, 50, 240, 3, Synth::Wave::SQUARE, 90, 1, 60);
    //gem collected: a bright two-note chime
    gemNotes.synth(0, SoundKit::hertz(SoundKit::E6), 60, 90, 30, Synth::Wave::PULSE25);
    gemNotes.synth(70, SoundKit::hertz(SoundKit::B6), 170, 92, 150, Synth::Wave::PULSE25);
    //heart collected: a warm rising arpeggio
    heartNotes.synth(0, SoundKit::hertz(SoundKit::C5), 60, 86, 20, Synth::Wave::TRIANGLE);
    heartNotes.synth(65, SoundKit::hertz(SoundKit::E5), 60, 88, 20, Synth::Wave::TRIANGLE);
    heartNotes.synth(130, SoundKit::hertz(SoundKit::G5), 60, 90, 20, Synth::Wave::TRIANGLE);
    heartNotes.synth(195, SoundKit::hertz(SoundKit::C6), 180, 92, 150, Synth::Wave::TRIANGLE);
    //a buried item freed: a soft click
    clickNotes.burst(0, 1500, 7, 66, 4);
    //sorted once (the cues are taken again when played)
    clunkNotes.sort(); repairNotes.sort(); pingNotes.sort(); whistleNotes.sort(); burstNotes.sort();
    gemNotes.sort(); heartNotes.sort(); clickNotes.sort();
}

void PlumeGame::reset(uint32_t seed){
    terrainRandom.seed(seed);
    plumeRandom.seed(seed * 2654435761u + 7u);
    demoRandom.seed(seed * 2246822519u + 1u);
    itemRandom.seed(seed * 3266489917u + 3u);
    noiseSeed = terrainRandom.next();
    for(uint16_t i = 0; i < ringSize; i++) columns[i] = 0;
    generated = 0;
    b_plugActive = false;
    nextPlug = firstPlugPx;
    for(uint8_t i = 0; i < maxBoxes; i++) boxes[i].length = 0;
    for(uint8_t i = 0; i < maxItems; i++) items[i] = Item{0, 0, ItemKind::GEM, 0, false, false, false};
    nextGem = calmPx + itemRandom.between(50, gemSpacingMinPx);
    nextHeart = calmPx + itemRandom.between(heartSpacingMinPx / 2, heartSpacingMinPx);
    gems = 0;

    shipX = startX;
    shipY = startY;
    speedX = speedY = 0.0f;
    cameraX = 0.0f;
    highestX = startX;
    repairMark = startX;
    hull = maxHull;
    graceLeft = 0;
    pipBlink = 0;
    burstTicks = 0;
    b_fellOut = false;
    frameTicks = 0;
    for(uint8_t i = 0; i < particleCount; i++) particles[i].life = 0;
    emitDebt = 0.0f;
    erodedThisTick = 0;
    crackleWait = 0;

    demoMode = DemoMode::CLIMB;
    demoDigTicks = 0;
    demoLapse = 0;
    demoRow = startY;
    demoItem = -1;
    demoPassed = -1;

    effectTicks = 0;
    lastPingMetres = 0;
    tempo = 100;
    generateUpTo(generateAhead);
}

float PlumeGame::difficulty() const { return difficultyAt(highestX - startX); }

//--- the shaft

bool PlumeGame::isRock(int32_t column, int16_t row) const {
    if(row < 0 || row >= height || column < 0 || column >= generated || generated - column > int32_t(ringSize)) return false;
    return (columns[uint32_t(column) % ringSize] >> row) & 1u;
}

void PlumeGame::setRock(int32_t column, int16_t row, bool b_rock){
    if(row < 0 || row >= height || column < 0 || column >= generated || generated - column > int32_t(ringSize)) return;
    uint32_t& bits = columns[uint32_t(column) % ringSize];
    if(b_rock) bits |= 1u << row;
    else bits &= ~(1u << row);
}

//smooth gradient noise (Perlin style), about -0.7..0.7
float PlumeGame::noise(float x, float y) const {
    int32_t x0 = int32_t(floorf(x));
    int32_t y0 = int32_t(floorf(y));
    float fx = x - float(x0);
    float fy = y - float(y0);
    float dots[4];
    for(uint8_t i = 0; i < 4; i++){
        int32_t cx = x0 + (i & 1);
        int32_t cy = y0 + (i >> 1);
        uint32_t g = hash(cx, cy, noiseSeed) & 7u;
        dots[i] = gradientX[g] * (fx - float(i & 1)) + gradientY[g] * (fy - float(i >> 1));
    }
    float u = fade(fx);
    float v = fade(fy);
    return lerpf(lerpf(dots[0], dots[1], u), lerpf(dots[2], dots[3], u), v);
}

void PlumeGame::generateUpTo(int32_t column){
    while(generated < column) generateColumn(generated++);
}

void PlumeGame::generateColumn(int32_t column){
    uint32_t bits = 0;
    float d = difficultyAt(float(column) - startX);
    if(column >= calmPx){
        //blobs: noise over a threshold, the features smaller and the threshold lower as it gets harder
        float size = lerpf(blobSizeStart, blobSizeCap, d);
        float threshold = lerpf(coverageThresholdStart, coverageThresholdCap, d);
        for(int16_t row = 0; row < height; row++){
            if(noise(float(column) / size, float(row) / size) > threshold) bits |= 1u << row;
        }

        //boxes: so many per 100 m, from small crates to long slabs
        if(terrainRandom.unit() < lerpf(boxesPer100mStart, boxesPer100mCap, d) / 1000.0f){
            for(uint8_t i = 0; i < maxBoxes; i++){
                Box& b = boxes[i];
                if(b.length && b.start + b.length > column) continue;
                b.start = column;
                b.length = uint8_t(terrainRandom.between(3, int32_t(lroundf(lerpf(6.0f, 12.0f, d)))));
                b.rows = int8_t(terrainRandom.between(3, int32_t(lroundf(lerpf(6.0f, 10.0f, d)))));
                b.top = int8_t(terrainRandom.between(1, height - 1 - b.rows));
                break;
            }
        }
    }
    for(uint8_t i = 0; i < maxBoxes; i++){
        const Box& b = boxes[i];
        if(!b.length || column < b.start || column >= b.start + b.length) continue;
        bits |= ((b.rows >= 32 ? 0xFFFFFFFFu : ((1u << b.rows) - 1u)) << b.top);
    }

    //plugs: a wall across the whole shaft with rough edges, every so often
    if(!b_plugActive && column >= nextPlug){
        b_plugActive = true;
        plug.start = column;
        plug.thickness = uint8_t(lroundf(lerpf(2.0f, 6.0f, d)));
        for(int16_t row = 0; row < height; row++){
            plug.lower[row] = int8_t(terrainRandom.between(0, 1));
            plug.upper[row] = int8_t(terrainRandom.between(0, 1));
        }
        float spacing = lerpf(plugSpacingStart, plugSpacingCap, d);
        nextPlug = column + int32_t(spacing * (0.85f + 0.3f * terrainRandom.unit()));
    }
    if(b_plugActive){
        int32_t offset = column - plug.start;
        for(int16_t row = 0; row < height; row++){
            if(offset >= plug.lower[row] && offset < int32_t(plug.thickness) + plug.upper[row]) bits |= 1u << row;
        }
        if(offset > int32_t(plug.thickness) + 2) b_plugActive = false;
    }

    //the collectibles, none in the calm start: placed here, stamped last so their rock (or their clear ring) wins
    if(column >= calmPx){
        if(column >= nextGem){
            newItem(column, ItemKind::GEM);
            nextGem = column + itemRandom.between(gemSpacingMinPx, gemSpacingMaxPx);
        }
        if(column >= nextHeart){
            newItem(column, ItemKind::HEART);
            nextHeart = column + itemRandom.between(heartSpacingMinPx, heartSpacingMaxPx);
        }
    }
    stampItems(column, bits);
    columns[uint32_t(column) % ringSize] = bits;
}

//--- the collectibles

bool PlumeGame::itemShape(ItemKind kind, int16_t col, int16_t row){
    int16_t size = itemSize(kind);
    if(col < 0 || col >= size || row < 0 || row >= size) return false;
    if(kind == ItemKind::GEM) return col == 1 || row == 1;     //a diamond
    //the heart's top towards screen right: its rows run down screen columns from the right
    return heartRows[size - 1 - col] & (0x10 >> row);
}

//an item starting at this column (its rock or its clear ring begins here); every choice from the item generator, so
//the shaft and its items are the same for a seed whatever the player does
void PlumeGame::newItem(int32_t column, ItemKind kind){
    float d = difficultyAt(float(column) - startX);
    int16_t size = itemSize(kind);
    float buried = kind == ItemKind::GEM ? lerpf(gemBuriedStart, gemBuriedCap, d) : lerpf(heartBuriedStart, heartBuriedCap, d);
    bool b_buried = itemRandom.unit() < buried;
    uint8_t depth = uint8_t(lroundf(lerpf(depthStart, depthCap, d)) + int32_t(itemRandom.below(2)));
    //near or inside a plug always buried: an open item's clear ring would cut a way through it
    int32_t reach = size + 2 * depth + 2;
    if(b_plugActive || (nextPlug >= column - 8 && nextPlug <= column + reach)) b_buried = true;
    if(!b_buried) depth = 0;
    int32_t x = column + (b_buried ? depth : 1);
    //the row: of four tries, the rockiest blob for a buried item, the most open for one in open space
    float blobSize = lerpf(blobSizeStart, blobSizeCap, d);
    int8_t y = 0;
    float best = 0.0f;
    for(uint8_t i = 0; i < 4; i++){
        int8_t row = int8_t(itemRandom.between(2, height - 2 - size));
        float rock = noise((float(x) + float(size) * 0.5f) / blobSize, (float(row) + float(size) * 0.5f) / blobSize);
        if(!b_buried) rock = -rock;
        if(i == 0 || rock > best){
            best = rock;
            y = row;
        }
    }
    //a free slot: never used, or its columns all made and it is collected or gone below the camera
    int32_t camera = cameraColumn();
    for(uint8_t i = 0; i < maxItems; i++){
        Item& it = items[i];
        if(it.b_used){
            int32_t end = it.x + itemSize(it.kind) + (it.depth ? it.depth : 1);
            if(end > column || !(it.b_taken || it.x + itemSize(it.kind) < camera)) continue;
        }
        it = Item{x, y, kind, depth, true, false, b_buried};
        return;
    }
}

//the items' part of a column: a buried item's lump of rock with its shape cut out, an open item's shape and its 1 px
//ring cleared. Collected or not, so the shaft stays the same whatever the player does
void PlumeGame::stampItems(int32_t column, uint32_t& bits) const {
    for(uint8_t i = 0; i < maxItems; i++){
        const Item& it = items[i];
        if(!it.b_used) continue;
        int16_t size = itemSize(it.kind);
        int16_t margin = it.depth ? it.depth : 1;
        int16_t col = int16_t(column - it.x);
        if(col < -margin || col >= size + margin) continue;
        for(int16_t row = int16_t(it.y - margin); row < it.y + size + margin; row++){
            if(row < 0 || row >= height) continue;
            uint32_t bit = 1u << row;
            if(it.depth && !itemShape(it.kind, col, int16_t(row - it.y))) bits |= bit;
            else bits &= ~bit;
        }
    }
}

bool PlumeGame::isItemPixel(int32_t column, int16_t row) const {
    for(uint8_t i = 0; i < maxItems; i++){
        const Item& it = items[i];
        if(it.b_used && !it.b_taken && itemShape(it.kind, int16_t(column - it.x), int16_t(row - it.y))) return true;
    }
    return false;
}

void PlumeGame::placeItem(ItemKind kind, int32_t x, int8_t y){
    for(uint8_t i = 0; i < maxItems; i++){
        Item& it = items[i];
        if(it.b_used && !it.b_taken) continue;
        it = Item{x, y, kind, 0, true, false, false};
        it.b_buried = !itemFree(it);
        return;
    }
}

//no rock touches it any more (its shape and the 1 px ring around it clear)
bool PlumeGame::itemFree(const Item& it) const {
    int16_t size = itemSize(it.kind);
    for(int16_t col = -1; col <= size; col++){
        for(int16_t row = -1; row <= size; row++){
            if(isRock(it.x + col, int16_t(it.y + row))) return false;
        }
    }
    return true;
}

//collected when the canister touches it (a canister pixel on or next to an item pixel); a buried item whose last rock
//around it was blasted away clicks
void PlumeGame::collectItems(){
    int32_t ix = lroundf(shipX);
    int16_t iy = int16_t(lroundf(shipY));
    int32_t camera = cameraColumn();
    for(uint8_t i = 0; i < maxItems; i++){
        Item& it = items[i];
        if(!it.b_used || it.b_taken) continue;
        int16_t size = itemSize(it.kind);
        if(it.x + size + it.depth >= generated || it.x + size < camera || it.x > camera + width) continue;
        if(it.b_buried && itemFree(it)){
            it.b_buried = false;
            effect(clickNotes.cue(), 2);
        }
        if(ix + shipWidth < it.x - 1 || ix > it.x + size || iy + shipHeight < it.y - 1 || iy > it.y + size) continue;
        bool b_touch = false;
        for(int16_t row = 0; row < shipHeight && !b_touch; row++){
            for(int16_t col = 0; col < shipWidth && !b_touch; col++){
                if(!(shipRows[row] & (0x10 >> col))) continue;
                for(int16_t dc = -1; dc <= 1 && !b_touch; dc++){
                    for(int16_t dr = -1; dr <= 1 && !b_touch; dr++){
                        if(itemShape(it.kind, int16_t(ix + col + dc - it.x), int16_t(iy + row + dr - it.y))) b_touch = true;
                    }
                }
            }
        }
        if(!b_touch) continue;
        it.b_taken = true;
        if(it.kind == ItemKind::GEM){
            gems++;
            effect(gemNotes.cue(), 12);
        }
        else{
            if(hull < maxHull){
                hull++;
                pipBlink = 0;
            }
            effect(heartNotes.cue(), 16);
        }
    }
}

//--- the canister

float PlumeGame::thrustFrom(const GameInput& input, float& directionX, float& directionY){
    //the stick pushed further, never both added
    float lx = input.stick[0], ly = input.stick[1];
    float rx = input.stick[2], ry = input.stick[3];
    float left = sqrtf(lx * lx + ly * ly);
    float right = sqrtf(rx * rx + ry * ry);
    float x = left >= right ? lx : rx;
    float y = left >= right ? ly : ry;
    float deflection = left >= right ? left : right;
    directionX = directionY = 0.0f;
    if(deflection <= deadzone) return 0.0f;
    //the stick aims the plume (owner, 2026-10-27): stick right sprays towards screen right and pushes the canister
    //towards screen left, stick up sprays towards the top of the screen (row 0). The thrust is the opposite way
    directionX = -x / deflection;
    directionY = y / deflection;
    return clampf((deflection - deadzone) / (1.0f - deadzone), 0.0f, 1.0f);
}

void PlumeGame::place(float x, float y, float vx, float vy){
    shipX = x;
    shipY = y;
    speedX = vx;
    speedY = vy;
    if(shipX > highestX) highestX = shipX;
    cameraX = fmaxf(cameraX, shipX - float(cameraLead));
    generateUpTo(cameraColumn() + generateAhead);
}

//the canister at (ix, iy) touches rock or a long edge of the screen; the normal points away from what it touches
bool PlumeGame::shipTouches(int16_t ix, int16_t iy, float& normalX, float& normalY) const {
    float sumX = 0.0f, sumY = 0.0f;
    bool b_touch = false;
    for(int16_t row = 0; row < shipHeight; row++){
        for(int16_t col = 0; col < shipWidth; col++){
            if(!(shipRows[row] & (0x10 >> col))) continue;
            int16_t y = int16_t(iy + row);
            if(y <= 0 || y >= height - 1 || isRock(int32_t(ix) + col, y)){
                b_touch = true;
                sumX += float(col) - 2.0f;
                sumY += float(row) - 1.0f;
            }
        }
    }
    if(!b_touch) return false;
    float length = sqrtf(sumX * sumX + sumY * sumY);
    if(length > 0.01f){
        normalX = -sumX / length;
        normalY = -sumY / length;
    }
    else{
        //touching evenly all round: back the way it came
        float speed = sqrtf(speedX * speedX + speedY * speedY);
        normalX = speed > 0.01f ? -speedX / speed : 1.0f;
        normalY = speed > 0.01f ? -speedY / speed : 0.0f;
    }
    return true;
}

//pushed out of the rock along the normal and bounced; a hit unless the grace second runs
void PlumeGame::collide(){
    float nx, ny;
    if(!shipTouches(int16_t(lroundf(shipX)), int16_t(lroundf(shipY)), nx, ny)) return;
    for(uint8_t i = 0; i < 16; i++){
        shipX += nx * 0.5f;
        shipY += ny * 0.5f;
        float ignoredX, ignoredY;
        if(!shipTouches(int16_t(lroundf(shipX)), int16_t(lroundf(shipY)), ignoredX, ignoredY)) break;
    }
    float into = speedX * nx + speedY * ny;
    if(into < 0.0f){
        speedX -= (1.0f + bounce) * into * nx;
        speedY -= (1.0f + bounce) * into * ny;
    }
    if(graceLeft == 0) hullHit();
}

void PlumeGame::hullHit(){
    hull--;
    graceLeft = graceTicks;
    pipBlink = pipBlinkTicks;
    repairMark = highestX;
    if(hull == 0) burst();
    else effect(clunkNotes.cue(), 4);
}

//the hull is gone: the canister bursts into the pieces the plume's particles become
void PlumeGame::burst(){
    burstTicks = 0;
    float cx = shipX + 2.5f, cy = shipY + 1.5f;
    for(uint8_t i = 0; i < particleCount; i++){
        Particle& p = particles[i];
        float angle = plumeRandom.unit() * 2.0f * pi;
        float speed = 15.0f + plumeRandom.unit() * 45.0f;
        p.x = cx;
        p.y = cy;
        p.vx = speedX * 0.5f + cosf(angle) * speed;
        p.vy = speedY * 0.5f + sinf(angle) * speed;
        p.life = uint8_t(10 + plumeRandom.below(burstShowTicks - 10));
    }
    effect(burstNotes.cue(), 16);
}

void PlumeGame::emit(float directionX, float directionY, float thrust){
    emitDebt += emissionRate * thrust * tickSeconds;
    //the nozzle: the side opposite the thrust, just outside the canister
    float cx = shipX + 2.5f - directionX * 3.2f;
    float cy = shipY + 1.5f - directionY * 2.2f;
    while(emitDebt >= 1.0f){
        emitDebt -= 1.0f;
        Particle* p = nullptr;
        for(uint8_t i = 0; i < particleCount; i++) if(particles[i].life == 0){ p = &particles[i]; break; }
        if(!p) continue;
        float spread = (plumeRandom.unit() * 2.0f - 1.0f) * plumeSpreadDegrees * pi / 180.0f;
        float c = cosf(spread), s = sinf(spread);
        float px = -directionX * c + directionY * s;
        float py = -directionX * s - directionY * c;
        p->vx = speedX + px * plumeSpeed;
        p->vy = speedY + py * plumeSpeed;
        //spread over the tick, so a dense plume does not leave in clumps
        float along = plumeRandom.unit() * tickSeconds;
        p->x = cx + p->vx * along;
        p->y = cy + p->vy * along;
        p->life = uint8_t(lroundf(particleLife / tickSeconds));
    }
}

//particles fly straight; one that enters rock clears that pixel and dies (never while the canister bursts)
void PlumeGame::moveParticles(){
    int32_t bottom = cameraColumn();
    bool b_erode = hull > 0;
    for(uint8_t i = 0; i < particleCount; i++){
        Particle& p = particles[i];
        if(p.life == 0) continue;
        p.life--;
        float stepX = p.vx * tickSeconds;
        float stepY = p.vy * tickSeconds;
        uint8_t steps = uint8_t(ceilf(fmaxf(fabsf(stepX), fabsf(stepY))));
        if(steps < 1) steps = 1;
        if(steps > 6) steps = 6;
        for(uint8_t k = 0; k < steps && p.life; k++){
            p.x += stepX / float(steps);
            p.y += stepY / float(steps);
            int32_t column = int32_t(floorf(p.x));
            int16_t row = int16_t(floorf(p.y));
            if(row < 0 || row >= height || column < bottom || column >= generated){
                p.life = 0;
                break;
            }
            //an item is never destroyed: the particle dies on it
            if(isItemPixel(column, row)){
                p.life = 0;
                break;
            }
            if(b_erode && isRock(column, row)){
                setRock(column, row, false);
                erodedThisTick++;
                p.life = 0;
            }
        }
    }
}

uint8_t PlumeGame::liveParticles() const {
    uint8_t n = 0;
    for(uint8_t i = 0; i < particleCount; i++) if(particles[i].life) n++;
    return n;
}

void PlumeGame::update(const GameInput& input){
    frameTicks++;
    erodedThisTick = 0;
    if(effectTicks) effectTicks--;
    if(b_fellOut) return;
    if(hull == 0){
        if(burstTicks < burstShowTicks) burstTicks++;
        moveParticles();
        return;
    }

    float directionX, directionY;
    float thrust = thrustFrom(input, directionX, directionY);
    speedX += (directionX * fullThrust * thrust - gravity) * tickSeconds;
    speedY += directionY * fullThrust * thrust * tickSeconds;
    speedX -= speedX * drag * tickSeconds;
    speedY -= speedY * drag * tickSeconds;
    float speed = sqrtf(speedX * speedX + speedY * speedY);
    if(speed > maxSpeed){
        speedX *= maxSpeed / speed;
        speedY *= maxSpeed / speed;
    }

    //moved at most half a pixel at a time, so it never passes through a pixel of rock
    uint8_t steps = uint8_t(ceilf(fmaxf(fabsf(speedX), fabsf(speedY)) * tickSeconds * 2.0f));
    if(steps < 1) steps = 1;
    for(uint8_t k = 0; k < steps && hull > 0; k++){
        shipX += speedX * tickSeconds / float(steps);
        shipY += speedY * tickSeconds / float(steps);
        collide();
        if(hull > 0) collectItems();
    }
    if(hull == 0) return;

    emit(directionX, directionY, thrust);
    moveParticles();

    //the camera only moves up; the shaft is generated ahead of it
    cameraX = fmaxf(cameraX, shipX - float(cameraLead));
    if(shipX > highestX) highestX = shipX;
    generateUpTo(cameraColumn() + generateAhead);

    if(graceLeft) graceLeft--;
    if(pipBlink) pipBlink--;
    //50 m climbed without a hit: a hull point back
    if(highestX - repairMark >= float(repairPx)){
        repairMark += float(repairPx);
        if(hull < maxHull){
            hull++;
            pipBlink = 0;
            effect(repairNotes.cue(), 12);
        }
    }

    if(shipX + float(shipWidth) <= cameraX){
        b_fellOut = true;
        effect(whistleNotes.cue(), 60);
        return;
    }
    sounds(thrust);
}

//--- sound [D-157]: the game's own notes and random numbers, never its state

void PlumeGame::effect(const SoundCue& cue, uint8_t ticks){
    sound().play(cue);
    effectTicks = ticks;
}

void PlumeGame::sounds(float thrust){
    //every 100 m: a ping
    uint32_t metres = altitude();
    if(metres / 100 > lastPingMetres / 100) effect(pingNotes.cue(), 12);
    lastPingMetres = metres;
    updateTempo();

    if(crackleWait) crackleWait--;
    if(effectTicks) return;
    //rock blasted: a tiny crackle, at most every 80 ms
    if(erodedThisTick && crackleWait == 0){
        crackleWait = 4;
        hiss.play(sound(), SoundKit::noiseBurst(0, uint16_t(hiss.random.between(2500, 4200)), 9, uint8_t(hiss.random.between(62, 78)), 5));
        return;
    }
    //the plume: soft high noise grains, denser with more thrust
    if(thrust > 0.0f){
        uint8_t period = thrust > 0.75f ? 1 : thrust > 0.5f ? 2 : thrust > 0.25f ? 3 : 5;
        if(hiss.due(period)){
            hiss.play(sound(), SoundKit::noiseBurst(0, uint16_t(hiss.random.between(5200, 7200)), 12, uint8_t(42 + 22 * thrust), 6));
        }
    }
}

//the music a little quicker as the climb gets harder, in steps of 5 %
void PlumeGame::updateTempo(){
    uint16_t wanted = uint16_t(100 + 5 * int(difficulty() * 5.0f));
    if(wanted == tempo) return;
    tempo = wanted;
    sound().setTempo(SoundLayer::BACK, tempo);
}

void PlumeGame::playMusic(){
    sound().setTempo(SoundLayer::BACK, tempo);
    sound().play(tune.cue(SoundPriority::GAME, uint16_t(64 * tuneStepMillis)), SoundLayer::BACK);
}

//--- drawing

bool PlumeGame::shipPixel(int16_t screenX, int16_t y) const {
    if(hull == 0 || b_fellOut) return false;
    if(graceLeft && (frameTicks / 4) % 2) return false;     //blinks while invulnerable
    int16_t col = int16_t(screenX - (lroundf(shipX) - cameraColumn()));
    int16_t row = int16_t(y - lroundf(shipY));
    if(col < 0 || col >= shipWidth || row < 0 || row >= shipHeight) return false;
    return shipRows[row] & (0x10 >> col);
}

bool PlumeGame::litBeforePips(int16_t screenX, int16_t y) const {
    int32_t camera = cameraColumn();
    if(isRock(camera + screenX, y) || shipPixel(screenX, y)) return true;
    for(uint8_t i = 0; i < maxItems; i++){
        const Item& it = items[i];
        if(it.b_used && !it.b_taken && !it.b_buried && itemShape(it.kind, int16_t(camera + screenX - it.x), int16_t(y - it.y))) return true;
    }
    for(uint8_t i = 0; i < particleCount; i++){
        const Particle& p = particles[i];
        if(p.life && int32_t(floorf(p.x)) - camera == screenX && int16_t(floorf(p.y)) == y) return true;
    }
    return false;
}

void PlumeGame::draw(Adafruit_GFX& out) const {
    int32_t camera = cameraColumn();
    //rock, as vertical runs
    for(int16_t x = 0; x < width; x++){
        int32_t column = camera + x;
        if(column < 0 || column >= generated) continue;
        uint32_t bits = columns[uint32_t(column) % ringSize];
        int16_t row = 0;
        while(bits && row < height){
            if(!(bits & 1u)){
                bits >>= 1;
                row++;
                continue;
            }
            int16_t start = row;
            while(bits & 1u){
                bits >>= 1;
                row++;
            }
            out.drawFastVLine(x, start, row - start, GameDraw::ON);
        }
    }
    //the items: lit once free; a buried one is its own shape left dark in the rock around it (the cut-out)
    for(uint8_t i = 0; i < maxItems; i++){
        const Item& it = items[i];
        if(!it.b_used || it.b_taken || it.b_buried) continue;
        int16_t size = itemSize(it.kind);
        for(int16_t col = 0; col < size; col++){
            for(int16_t row = 0; row < size; row++){
                if(itemShape(it.kind, col, row)) out.drawPixel(int16_t(it.x + col - camera), int16_t(it.y + row), GameDraw::ON);
            }
        }
    }
    //the plume (or the pieces of the canister)
    for(uint8_t i = 0; i < particleCount; i++){
        const Particle& p = particles[i];
        if(!p.life) continue;
        int32_t x = int32_t(floorf(p.x)) - camera;
        int16_t y = int16_t(floorf(p.y));
        if(x >= 0 && x < width && y >= 0 && y < height) out.drawPixel(int16_t(x), y, GameDraw::ON);
    }
    //the canister
    if(hull > 0 && !b_fellOut && !(graceLeft && (frameTicks / 4) % 2)){
        int16_t sx = int16_t(lroundf(shipX) - camera);
        int16_t sy = int16_t(lroundf(shipY));
        for(int16_t row = 0; row < shipHeight; row++){
            for(int16_t col = 0; col < shipWidth; col++) if(shipRows[row] & (0x10 >> col)) out.drawPixel(sx + col, sy + row, GameDraw::ON);
        }
    }
    drawHud(out);
}

bool PlumeGame::litForHud(const void* game, int16_t x, int16_t y){
    return static_cast<const PlumeGame*>(game)->litBeforePips(x, y);
}

//the top right corner, drawn inverted over what is there, read in the sideways grip (screen right up): the hull's four
//2 x 2 pips in a row along the right edge, then the altitude and the points in sideways text, 1 px apart [D-157]
void PlumeGame::drawHud(Adafruit_GFX& out) const {
    //a lost pip blinks twice before it goes
    for(uint8_t i = 0; i < maxHull; i++){
        bool b_shown = i < hull || (i == hull && pipBlink && (pipBlink / 10) % 2 == 1);
        if(!b_shown) continue;
        int16_t py = int16_t(1 + 3 * i);
        for(int16_t dy = 0; dy < 2; dy++){
            for(int16_t dx = 0; dx < 2; dx++){
                int16_t x = int16_t(width - 3 + dx), y = int16_t(py + dy);
                out.drawPixel(x, y, litBeforePips(x, y) ? GameDraw::OFF : GameDraw::ON);
            }
        }
    }
    char text[12];
    snprintf(text, sizeof(text), "%luM", (unsigned long)altitude());
    GameDraw::textSidewaysXor(out, hudAltitudeX, 1, text, litForHud, this);
    snprintf(text, sizeof(text), "%lu", (unsigned long)score());
    GameDraw::textSidewaysXor(out, hudPointsX, 1, text, litForHud, this);
}

void PlumeGame::overText(char* out, size_t size, uint32_t value) const {
    snprintf(out, size, "PTS %lu %luM", (unsigned long)value, (unsigned long)altitude());
}

//--- the demo player: climbs along the most open rows, blasts through what blocks the way in short bursts (the
//plume points up while it thrusts down, so it falls back between bursts), and now and then lets go for a while. It
//works out the thrust it wants; the stick it writes aims the plume, the opposite way

void PlumeGame::demo(GameInput& input){
    if(hull == 0 || b_fellOut) return;
    if(demoLapse){
        demoLapse--;
        return;
    }
    //a hand that rests now and then: a short rest costs some height, a long one falls out
    if(frameTicks > 400 && demoRandom.below(900) == 0){
        demoLapse = uint16_t(demoRandom.between(20, 130));
        return;
    }

    int16_t ix = int16_t(lroundf(shipX));
    int16_t iy = int16_t(lroundf(shipY));
    const int16_t look = 40;
    //how far a band of rows (the canister's three, and `room` more either side) is open ahead
    auto freeAhead = [&](int16_t top, int16_t room){
        int16_t free = 0;
        while(free < look){
            for(int16_t r = int16_t(top - room); r < top + shipHeight + room; r++) if(isRock(int32_t(ix) + free, r)) return free;
            free++;
        }
        return free;
    };
    //the rows it can move across to from here without going through rock beside it
    auto rowOpen = [&](int16_t row){
        for(int16_t c = 0; c <= shipWidth + 1; c++) if(isRock(int32_t(ix) + c, row)) return false;
        return true;
    };
    int16_t reachTop = iy, reachBottom = iy;
    while(reachTop > 2 && rowOpen(int16_t(reachTop - 1))) reachTop--;
    while(reachBottom < height - 2 - shipHeight && rowOpen(int16_t(reachBottom + shipHeight))) reachBottom++;
    int16_t bestRow = iy;
    float bestScore = -1000.0f;
    int16_t bestFree = 0;
    for(int16_t top = reachTop; top <= reachBottom; top++){
        int16_t free = freeAhead(top, 1);
        //open ahead, near, and away from the edges
        float score = float(free) - 0.9f * fabsf(float(top) - shipY) - 0.15f * fabsf(float(top) - startY);
        if(score > bestScore){
            bestScore = score;
            bestRow = top;
            bestFree = free;
        }
    }
    //now and then it goes for an item in open space ahead (it never digs for one): its row, if it can get there
    if(demoItem >= 0){
        const Item& it = items[demoItem];
        if(!it.b_used || it.b_taken || it.x + itemSize(it.kind) < ix) demoItem = -1;
    }
    if(demoItem < 0){
        for(uint8_t i = 0; i < maxItems; i++){
            const Item& it = items[i];
            if(!it.b_used || it.b_taken || it.b_buried || int8_t(i) == demoPassed) continue;
            if(it.x < ix || it.x > ix + look) continue;
            if(demoRandom.below(3) < 2) demoItem = int8_t(i);
            else demoPassed = int8_t(i);
            break;
        }
    }
    if(demoItem >= 0){
        const Item& it = items[demoItem];
        int16_t want = int16_t(it.y + itemSize(it.kind) / 2 - 1);
        if(want >= reachTop && want <= reachBottom){
            bestRow = want;
            bestFree = freeAhead(want, 1);
        }
    }
    //the gap in front of its nose, in its own rows
    int16_t hereFree = int16_t(freeAhead(iy, 0) - shipWidth);
    if(hereFree < 0) hereFree = 0;
    demoRow += clampf(float(bestRow) - demoRow, -1.0f, 1.0f);

    float wantX, wantY = clampf((demoRow - shipY) * 5.0f, -30.0f, 30.0f);
    float ax, ay;
    if(bestFree >= 14){
        demoMode = DemoMode::CLIMB;
        //forwards as far as its own rows are open: it waits while it moves across to the open ones
        wantX = clampf(float(hereFree - 4) * 3.0f, -10.0f, 32.0f);
        ax = (wantX - speedX) * 5.0f + gravity + speedX * drag;
    }
    else{
        //blocked: hold the row and close in, then a burst downwards blasts the rock above
        demoRow = shipY;
        wantY = 0.0f;
        if(demoMode == DemoMode::DIG){
            ax = -fullThrust;
            if(++demoDigTicks >= 12 || hereFree > 16) demoMode = DemoMode::CLIMB;
        }
        else{
            wantX = clampf(float(hereFree - 2) * 3.0f, -10.0f, 24.0f);
            ax = (wantX - speedX) * 5.0f + gravity + speedX * drag;
            if(hereFree <= 4 && speedX < 8.0f){
                demoMode = DemoMode::DIG;
                demoDigTicks = 0;
            }
        }
    }
    //steering across first, the rest of the thrust along the shaft
    ay = clampf((wantY - speedY) * 5.0f + speedY * drag, -fullThrust, fullThrust);
    float alongMax = sqrtf(fullThrust * fullThrust - ay * ay);
    ax = clampf(ax, -alongMax, alongMax);
    float magnitude = sqrtf(ax * ax + ay * ay);
    if(magnitude < 0.01f) return;
    float thrust = clampf(magnitude / fullThrust, 0.0f, 1.0f);
    float deflection = deadzone + (1.0f - deadzone) * thrust;
    //the stick aims the plume, the opposite of the thrust wanted
    input.stick[0] = -ax / magnitude * deflection;
    input.stick[1] = ay / magnitude * deflection;
}
