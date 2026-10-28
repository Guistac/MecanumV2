//CAVE: the helicopter in the cave [D-157]. A stick pushed up climbs in proportion, a stick button is full thrust,
//gravity pulls down. The craft hovers in a ready phase until the first input, so a new game never starts in a fall.
//The cave is generated column by column a little ahead of the screen; the distance flown is the score.
#include "CaveGame.h"
#include <stdio.h>
#include <math.h>

namespace{

    //the craft, 8 px wide, bit 7 is the left column: tail fin at the left, cabin and nose at the right
    const uint8_t rotorFrames[4] = {0x7E, 0x3C, 0x18, 0x3C};   //the rotor turning: wide, middle, end on, middle
    const uint8_t bodyRows[3] = {
        0x9E,   //#..####.  fin and cabin
        0xFF,   //########  boom and cabin to the nose
        0x1C,   //...###..  belly
    };
    //what touches: the rotor's hub, not its tips, so a blade passing a wall is forgiven
    const uint8_t rotorCollision = 0x3C;
    //a bat, 5 px wide (bit 4 is the left column), wings up and down; what touches is its middle 3 columns
    const uint8_t batFrames[2][3] = {{0x11, 0x0E, 0x04}, {0x04, 0x1F, 0x0A}};
    const uint8_t batCollision = 0x0E;
    const float batSwayHz = 0.8f;

    const int16_t startGap = 24;                //a roomy start: 10 px above the hovering craft, 10 below
    const int16_t minimumGap = 11;              //the craft is 4 px tall: 7 px to spare at the narrowest
    const float gapShrinkColumns = 160.0f;      //one row narrower every this many columns
    const uint32_t flatColumns = 90;            //a straight start while the gravity comes in
    const uint32_t pillarsFrom = 500;           //about 125 m
    const int16_t pillarRoom = 10;              //the open side of a pillar is at least this tall
    const float startY = 14.0f;                 //the craft's top row while it hovers: centred in the cave
    const float bobHz = 0.7f;
    const uint8_t demoStartTicks = 25;          //the demo lets the hint show half a second before it clicks
    const int16_t chamberColumns = 40;
    const float chamberWidening = 8.0f;

    float clampf(float v, float lowest, float highest){ return v < lowest ? lowest : (v > highest ? highest : v); }
    //the cave scrolls faster the further in: px/s after this many px (one px is one column)
    float scrollSpeedAfter(float px){ return clampf(30.0f + px * 0.011f, 0.0f, 85.0f); }

    //sound [D-157]: the rotor chops 9 times a second with no thrust, 25 at full (the hover, about 0.4: 15), a short
    //dull noise tick each, louder with more thrust
    const float rotorIdleRate = 9.0f;
    const float rotorFullRate = 25.0f;
    const uint16_t rotorChopMillis = 12;
    //the thrust that holds the craft (the ready phase's hover)
    const float hoverThrust = CaveGame::gravity / CaveGame::thrust;
};

CaveGame::CaveGame(){
    using namespace SoundKit;
    //every 100 m: a bright ping with its octave above, ringing out (A6, A7 qualified: Arduino names analog pins so)
    pingNotes.synth(0, hertz(SoundKit::A6), 220, 90, 80, Synth::Wave::SQUARE, 0);
    pingNotes.synth(0, hertz(SoundKit::A7), 140, 60, 45, Synth::Wave::PULSE25, 1);
    pingCue = pingNotes.cue();
    //the crash: a long burst of noise, a whistle falling under it
    crashNotes.burst(0, 2600, 420, 100, 150, 0);
    crashNotes.sweep(0, 2400, 260, 600, 20, Synth::Wave::SQUARE, 85, 1, 120);
    crashCue = crashNotes.cue();
    //a shot: a short falling zap
    shotNotes.sweep(0, 2200, 900, 45, 4, Synth::Wave::PULSE25, 60, 1);
    shotCue = shotNotes.cue();
    //a hit: a pop of noise and a rising chirp
    hitNotes.burst(0, 3000, 70, 85, 40, 0);
    hitNotes.sweep(20, 900, 1800, 90, 5, Synth::Wave::SQUARE, 70, 1, 30);
    hitCue = hitNotes.cue();
}

float CaveGame::rotorRate(float thrust){
    return rotorIdleRate + (rotorFullRate - rotorIdleRate) * clampf(thrust, 0.0f, 1.0f);
}

void CaveGame::playRotor(float push){
    rotorPhase += rotorRate(push) * tickSeconds;
    if(rotorPhase < 1.0f) return;
    rotorPhase -= float(int(rotorPhase));
    //a dull thump of noise, its brightness a little different each time
    uint16_t brightness = uint16_t(900 + rotorGrain.random.between(0, 700));
    rotorGrain.play(sound(), SoundKit::noiseBurst(0, brightness, rotorChopMillis, uint8_t(55.0f + 30.0f * push), 6));
}

void CaveGame::reset(uint32_t seed){
    random.seed(seed);
    demoRandom.seed(seed * 2654435761u + 1u);
    generated = 0;
    caveCentre = 16.0f;
    caveSlope = 0.0f;
    caveSlopeTarget = 0.0f;
    segmentLeft = 0;
    ceilingJitter = 0.0f;
    floorJitter = 0.0f;
    pillarCooldown = 0;
    chamberLeft = 0;
    for(uint8_t i = 0; i < maxPillars; i++) pillars[i].b_used = false;
    scrollPx = 0.0f;
    craftY = startY;
    craftSpeedY = 0.0f;
    ticks = 0;
    frameTicks = 0;
    b_ready = true;
    b_thrusting = false;
    b_crashed = false;
    crashTicks = 0;
    demoQueue = 0;
    demoAimError = 0.0f;
    rotorPhase = 0.0f;
    pingsPlayed = 0;
    batRandom.seed(seed ^ 0xBA7BA7u);
    for(uint8_t i = 0; i < maxBats; i++) bats[i].b_used = false;
    for(uint8_t i = 0; i < maxShots; i++) shots[i].b_used = false;
    batCooldown = 0;
    shotCooldown = 0;
    bonusMetres = 0;
    popup.ticks = 0;
    generateUpTo(ringSize - 8);
}

float CaveGame::getScrollSpeed() const {
    return scrollSpeedAfter(scrollPx);
}

float CaveGame::thrustFrom(const GameInput& input){
    if(input.held[0] || input.held[1]) return 1.0f;
    float up = input.stick[1] > input.stick[3] ? input.stick[1] : input.stick[3];
    return clampf(up, 0.0f, 1.0f);
}

bool CaveGame::firesFrom(const GameInput& input){
    return input.stick[0] > fireStick || input.stick[2] > fireStick;
}

void CaveGame::generateUpTo(uint32_t column){
    while(generated < column) generateColumn(generated++);
}

void CaveGame::generateColumn(uint32_t column){
    float gap = float(startGap) - float(column) / gapShrinkColumns;
    if(gap < float(minimumGap)) gap = float(minimumGap);
    //a pillar stands in a chamber: the cave opens up around it and runs level, so the way past is fair however
    //narrow the cave has become
    if(column >= pillarsFrom && chamberLeft == 0 && pillarCooldown == 0){
        chamberLeft = chamberColumns;
        caveSlopeTarget = 0.0f;
        segmentLeft = chamberColumns + 4;
    }
    if(chamberLeft > 0) gap += chamberWidening * sinf(3.14159265f * float(chamberColumns - chamberLeft) / float(chamberColumns));
    float half = gap * 0.5f;

    if(column >= flatColumns){
        //a smooth random walk: the slope eases towards a new target every few columns, steeper further in
        if(--segmentLeft <= 0){
            float maxSlope = 0.25f + clampf(float(column) / 8000.0f, 0.0f, 0.25f);
            //never steeper than the craft can climb at the speed the cave will scroll there (scroll = column)
            float speedThere = scrollSpeedAfter(float(column));
            if(maxSlope * speedThere > 0.8f * maxClimbSpeed) maxSlope = 0.8f * maxClimbSpeed / speedThere;
            caveSlopeTarget = (random.unit() * 2.0f - 1.0f) * maxSlope;
            segmentLeft = int16_t(random.between(8, 28));
        }
        caveSlope += clampf(caveSlopeTarget - caveSlope, -0.05f, 0.05f);
        caveCentre += caveSlope;
        //the walls also wander a little each on their own, so ceiling and floor are not parallel
        ceilingJitter = clampf(ceilingJitter + (random.unit() - 0.5f) * 0.8f, -2.0f, 0.0f);
        floorJitter = clampf(floorJitter + (random.unit() - 0.5f) * 0.8f, 0.0f, 2.0f);
    }
    //at least 1 px of ceiling and floor stays on the screen, the jitter included
    float lowest = 1.0f + half + 2.0f;
    float highest = float(height) - 1.0f - half - 2.0f;
    if(caveCentre < lowest){
        caveCentre = lowest;
        if(caveSlope < 0.0f) caveSlope = 0.0f;
        caveSlopeTarget = fabsf(caveSlopeTarget);
    }
    if(caveCentre > highest){
        caveCentre = highest;
        if(caveSlope > 0.0f) caveSlope = 0.0f;
        caveSlopeTarget = -fabsf(caveSlopeTarget);
    }
    int16_t top = int16_t(lroundf(caveCentre - half + ceilingJitter));
    int16_t bottom = int16_t(lroundf(caveCentre + half + floorJitter));
    if(top < 1) top = 1;
    if(bottom > height - 1) bottom = height - 1;
    ceiling[column % ringSize] = int8_t(top);
    floorRow[column % ringSize] = int8_t(bottom);

    if(chamberLeft > 0){
        if(chamberLeft == chamberColumns / 2){
            //in the middle of the chamber, if it did open up and run level (a pillar is fair only there)
            if(bottom - top >= 16 && fabsf(caveSlope) < 0.15f) spawnPillar(column, top, bottom);
            else pillarCooldown = 30;
        }
        chamberLeft--;
    }
    else if(column >= pillarsFrom && pillarCooldown > 0) pillarCooldown--;

    if(column >= batsFrom){
        if(batCooldown > 0) batCooldown--;
        else if(chamberLeft == 0) spawnBat(column);
    }
}

//a bat over the columns just made (column - 4 .. column), only where the cave is wide there and no pillar is near:
//its band (its height and its bob) on one side, the way past on the other at least batWayPast rows
void CaveGame::spawnBat(uint32_t column){
    int16_t top = 0, bottom = height;
    for(uint32_t c = column - (batWidth - 1); c <= column; c++){
        if(ceiling[c % ringSize] > top) top = ceiling[c % ringSize];
        if(floorRow[c % ringSize] < bottom) bottom = floorRow[c % ringSize];
    }
    const int16_t band = batHeight + 2 * batSway;
    int16_t spare = bottom - top - band - batWayPast;
    for(uint8_t i = 0; i < maxPillars; i++){
        if(pillars[i].b_used && pillars[i].column + 20 > column && pillars[i].column < column + 20) spare = -1;
    }
    if(spare < 0) return;
    Bat* slot = nullptr;
    for(uint8_t i = 0; i < maxBats && !slot; i++) if(!bats[i].b_used || bats[i].column + batWidth < firstColumn()) slot = &bats[i];
    if(!slot) return;
    int16_t offset = int16_t(batRandom.between(0, spare));
    int16_t bandTop = batRandom.below(2) ? int16_t(top + offset) : int16_t(bottom - band - offset);
    slot->b_used = true;
    slot->column = column - (batWidth - 1);
    slot->centreY = int8_t(bandTop + band / 2);
    slot->phase = uint8_t(batRandom.below(60));
    //the next one a while later, a little sooner further in
    int16_t sooner = int16_t(column / 100 > 60 ? 60 : column / 100);
    batCooldown = int16_t(batRandom.between(110, 240)) - sooner;
}

int16_t CaveGame::batTop(const Bat& b) const {
    float t = float(frameTicks + b.phase) * tickSeconds;
    return int16_t(b.centreY - batHeight / 2 + lroundf(batSway * sinf(2.0f * 3.14159265f * batSwayHz * t)));
}

bool CaveGame::isBat(int16_t screenX, int16_t screenY) const {
    uint32_t column = firstColumn() + uint32_t(screenX < 0 ? 0 : screenX);
    if(screenX < 0) return false;
    for(uint8_t i = 0; i < maxBats; i++){
        const Bat& b = bats[i];
        if(!b.b_used || column < b.column || column >= b.column + batWidth) continue;
        int16_t row = screenY - batTop(b);
        if(row < 0 || row >= batHeight) continue;
        if(batCollision & (0x10 >> (column - b.column))) return true;
    }
    return false;
}

uint8_t CaveGame::getBatCount() const {
    uint8_t n = 0;
    for(uint8_t i = 0; i < maxBats; i++) if(bats[i].b_used) n++;
    return n;
}

uint8_t CaveGame::getShotCount() const {
    uint8_t n = 0;
    for(uint8_t i = 0; i < maxShots; i++) if(shots[i].b_used) n++;
    return n;
}

bool CaveGame::getBat(uint8_t index, int16_t& screenX, int16_t& bandTop, int16_t& bandBottom) const {
    if(index >= maxBats || !bats[index].b_used) return false;
    screenX = int16_t(int32_t(bats[index].column) - int32_t(firstColumn()));
    bandTop = int16_t(bats[index].centreY - batHeight / 2 - batSway);
    bandBottom = int16_t(bandTop + batHeight + 2 * batSway - 1);
    return true;
}

void CaveGame::placeBat(int16_t screenX, int16_t centreY){
    Bat& b = bats[0];
    b.b_used = true;
    b.column = firstColumn() + uint32_t(screenX);
    b.centreY = int8_t(centreY);
    b.phase = 0;
}

void CaveGame::scored(uint16_t metres, int16_t x, int16_t y){
    bonusMetres = uint16_t(bonusMetres + metres);
    popup.ticks = popupTicks;
    popup.x = x;
    popup.y = y;
    popup.metres = metres;
    sound().play(hitCue);
}

//each shot flies on pixel by pixel: a bat it reaches is gone, a free pillar breaks, rock stops it
void CaveGame::moveShots(){
    uint32_t first = firstColumn();
    for(uint8_t i = 0; i < maxShots; i++){
        Shot& s = shots[i];
        if(!s.b_used) continue;
        float to = s.x + shotSpeed * tickSeconds;
        for(int16_t x = int16_t(s.x) + 1; x <= int16_t(to) && s.b_used; x++){
            if(x >= width){
                s.b_used = false;
                break;
            }
            uint32_t column = first + uint32_t(x);
            for(uint8_t k = 0; k < maxBats && s.b_used; k++){
                Bat& b = bats[k];
                if(!b.b_used || column < b.column || column >= b.column + batWidth) continue;
                int16_t row = s.y - batTop(b);
                if(row < -1 || row > batHeight) continue;
                b.b_used = false;
                s.b_used = false;
                scored(batBonus, x, int16_t(batTop(b) - 1));
            }
            for(uint8_t k = 0; k < maxPillars && s.b_used; k++){
                Pillar& p = pillars[k];
                if(!p.b_used || !pillarCovers(p, column, s.y)) continue;
                s.b_used = false;
                if(p.kind == PillarKind::FREE){
                    p.b_used = false;
                    scored(pillarBonus, x, int16_t(p.top - 1));
                }
            }
            if(s.b_used && isSolid(x, s.y)) s.b_used = false;
        }
        s.x = to;
    }
}

void CaveGame::spawnPillar(uint32_t column, int16_t top, int16_t bottom){
    //the slot of a pillar already scrolled away
    Pillar* slot = nullptr;
    for(uint8_t i = 0; i < maxPillars && !slot; i++) if(!pillars[i].b_used || pillars[i].column + 3 < firstColumn()) slot = &pillars[i];
    if(!slot) return;

    int16_t gap = bottom - top;
    Pillar p;
    p.b_used = true;
    p.column = column;
    uint32_t kind = random.below(3);
    int16_t freeHeight = int16_t(random.between(4, 6));
    if(kind == 0 && gap - pillarRoom - freeHeight - 3 >= 0){
        //free standing: a wide way on one side, a slot too narrow to fly on the other
        p.kind = PillarKind::FREE;
        int16_t narrow = int16_t(random.between(2, gap - pillarRoom - freeHeight));
        if(random.below(2)){
            p.top = int8_t(top + narrow);
        }
        else{
            p.top = int8_t(bottom - narrow - freeHeight);
        }
        p.bottom = int8_t(p.top + freeHeight - 1);
    }
    else{
        int16_t length = int16_t(random.between(3, gap - pillarRoom));
        if(length > 9) length = 9;
        if(random.below(2)){
            p.kind = PillarKind::FROM_CEILING;
            p.top = 0;
            p.bottom = int8_t(top + length - 1);
        }
        else{
            p.kind = PillarKind::FROM_FLOOR;
            p.top = int8_t(bottom - length);
            p.bottom = int8_t(height - 1);
        }
    }
    *slot = p;
    //the next one later, closer together further in
    int16_t closer = int16_t(column / 200 > 40 ? 40 : column / 200);
    pillarCooldown = int16_t(random.between(60, 150)) - closer;
}

//a stalactite ends in a 1 px tip, a stalagmite too; a free pillar is a block with its corners cut
bool CaveGame::pillarCovers(const Pillar& p, uint32_t column, int16_t row) const {
    if(column < p.column || column > p.column + 2 || row < p.top || row > p.bottom) return false;
    bool b_middle = column == p.column + 1;
    switch(p.kind){
        case PillarKind::FROM_CEILING: return row < p.bottom || b_middle;
        case PillarKind::FROM_FLOOR: return row > p.top || b_middle;
        case PillarKind::FREE: return (row > p.top && row < p.bottom) || b_middle;
    }
    return false;
}

int16_t CaveGame::ceilingAt(int16_t screenX) const { return ceiling[(firstColumn() + uint32_t(screenX)) % ringSize]; }
int16_t CaveGame::floorAt(int16_t screenX) const { return floorRow[(firstColumn() + uint32_t(screenX)) % ringSize]; }

bool CaveGame::isSolid(int16_t screenX, int16_t screenY) const {
    if(screenY < 0 || screenY >= height) return true;
    if(screenX < 0 || screenX >= width) return false;
    if(screenY < ceilingAt(screenX) || screenY >= floorAt(screenX)) return true;
    uint32_t column = firstColumn() + uint32_t(screenX);
    for(uint8_t i = 0; i < maxPillars; i++) if(pillars[i].b_used && pillarCovers(pillars[i], column, screenY)) return true;
    return false;
}

bool CaveGame::hitsSomething() const {
    int16_t y = int16_t(lroundf(craftY));
    for(int16_t row = 0; row < craftHeight; row++){
        uint8_t bits = row == 0 ? rotorCollision : bodyRows[row - 1];
        for(int16_t col = 0; col < craftWidth; col++){
            if((bits & (0x80 >> col)) && (isSolid(craftX + col, y + row) || isBat(craftX + col, y + row))) return true;
        }
    }
    return false;
}

void CaveGame::crash(){
    b_crashed = true;
    crashTicks = 0;
    crashX = float(craftX) + 4.0f;
    crashY = craftY + 2.0f;
    sound().play(crashCue);
    for(uint8_t i = 0; i < debrisCount; i++){
        Debris& d = debris[i];
        d.x = crashX + (random.unit() * 2.0f - 1.0f) * 3.0f;
        d.y = crashY + (random.unit() * 2.0f - 1.0f) * 1.5f;
        //flung mostly forwards and up: the craft was flying into the cave
        d.vx = (random.unit() * 2.0f - 1.0f) * 45.0f + 15.0f;
        d.vy = -random.unit() * 60.0f - 5.0f;
        d.b_big = i < 3;
    }
}

void CaveGame::update(const GameInput& input){
    if(b_crashed){
        if(crashTicks < crashShowTicks) crashTicks++;
        for(uint8_t i = 0; i < debrisCount; i++){
            Debris& d = debris[i];
            d.vy += 120.0f * tickSeconds;
            float x = d.x + d.vx * tickSeconds;
            float y = d.y + d.vy * tickSeconds;
            //a piece that hits rock stops there
            if(isSolid(int16_t(lroundf(x)), int16_t(lroundf(y)))){
                d.vx = 0.0f;
                d.vy = 0.0f;
            }
            else{
                d.x = x;
                d.y = y;
            }
        }
        return;
    }
    frameTicks++;
    if(b_ready){
        //the craft bobs gently in the middle and the cave waits: the game starts when the player does
        float up = input.stick[1] > input.stick[3] ? input.stick[1] : input.stick[3];
        bool b_go = input.clicked[0] || input.clicked[1] || input.held[0] || input.held[1] || up > startStickUp;
        if(!b_go && frameTicks < readyTicks){
            craftY = startY + sinf(2.0f * 3.14159265f * bobHz * float(frameTicks) * tickSeconds);
            playRotor(hoverThrust);
            return;
        }
        b_ready = false;
        craftSpeedY = 0.0f;
    }
    ticks++;
    scrollPx += getScrollSpeed() * tickSeconds;
    generateUpTo(firstColumn() + ringSize - 8);
    for(uint8_t i = 0; i < maxPillars; i++) if(pillars[i].b_used && pillars[i].column + 3 < firstColumn()) pillars[i].b_used = false;
    for(uint8_t i = 0; i < maxBats; i++) if(bats[i].b_used && bats[i].column + batWidth < firstColumn()) bats[i].b_used = false;
    if(popup.ticks) popup.ticks--;
    moveShots();
    if(shotCooldown) shotCooldown--;
    if(firesFrom(input) && shotCooldown == 0){
        for(uint8_t i = 0; i < maxShots; i++){
            if(shots[i].b_used) continue;
            shots[i].b_used = true;
            shots[i].x = float(craftX + craftWidth);
            shots[i].y = int16_t(lroundf(craftY)) + 2;
            shotCooldown = shotCooldownTicks;
            sound().play(shotCue);
            break;
        }
    }

    float push = thrustFrom(input);
    b_thrusting = push > 0.0f;
    //from the hover, the craft's response comes in over the first second or so: full thrust from the start takes
    //about a second to the ceiling, a stick left at rest starts a slow sink, not a drop
    float ramp = ticks < spoolTicks ? float(ticks) / float(spoolTicks) : 1.0f;
    craftSpeedY += (gravity - thrust * push) * ramp * tickSeconds;
    craftSpeedY = clampf(craftSpeedY, -maxClimbSpeed, maxFallSpeed);
    craftY += craftSpeedY * tickSeconds;

    if(hitsSomething()){
        crash();
        return;
    }
    //every 100 m a ping (it takes the tick: the rotor skips while it rings)
    uint32_t pings = distance() / pingMetres;
    if(pings > pingsPlayed){
        pingsPlayed = pings;
        sound().play(pingCue);
    }
    playRotor(push);
}

void CaveGame::draw(Adafruit_GFX& out) const {
    for(int16_t x = 0; x < width; x++){
        int16_t top = ceilingAt(x);
        int16_t bottom = floorAt(x);
        out.drawFastVLine(x, 0, top, GameDraw::ON);
        out.drawFastVLine(x, bottom, height - bottom, GameDraw::ON);
    }
    uint32_t first = firstColumn();
    for(uint8_t i = 0; i < maxPillars; i++){
        const Pillar& p = pillars[i];
        if(!p.b_used || p.column + 2 < first || p.column >= first + uint32_t(width)) continue;
        for(int16_t c = 0; c < 3; c++){
            for(int16_t row = p.top; row <= p.bottom; row++){
                if(pillarCovers(p, p.column + uint32_t(c), row)) out.drawPixel(int16_t(p.column - first) + c, row, GameDraw::ON);
            }
        }
    }

    for(uint8_t i = 0; i < maxBats; i++){
        const Bat& b = bats[i];
        if(!b.b_used || b.column + batWidth < first || b.column >= first + uint32_t(width)) continue;
        const uint8_t* frame = batFrames[(frameTicks / 6) % 2];
        int16_t x0 = int16_t(int32_t(b.column) - int32_t(first)), y0 = batTop(b);
        for(int16_t row = 0; row < batHeight; row++){
            for(int16_t col = 0; col < batWidth; col++) if(frame[row] & (0x10 >> col)) out.drawPixel(x0 + col, y0 + row, GameDraw::ON);
        }
    }
    for(uint8_t i = 0; i < maxShots; i++){
        if(shots[i].b_used) out.drawFastHLine(int16_t(shots[i].x) - 2, shots[i].y, 3, GameDraw::ON);
    }
    if(popup.ticks){
        char bonus[6];
        snprintf(bonus, sizeof(bonus), "+%u", unsigned(popup.metres));
        int16_t w = GameDraw::textWidth(bonus);
        int16_t px = popup.x - w / 2, py = popup.y - 7 - (popupTicks - popup.ticks) / 6;
        if(px < 0) px = 0;
        if(px + w > width) px = width - w;
        if(py < 0) py = 0;
        if(py > height - 8) py = height - 8;
        out.fillRect(px - 1, py - 1, w + 2, 9, GameDraw::OFF);
        GameDraw::text(out, px, py, bonus);
    }

    if(!b_crashed){
        int16_t y = int16_t(lroundf(craftY));
        for(int16_t row = 0; row < craftHeight; row++){
            uint8_t bits = row == 0 ? rotorFrames[(frameTicks / 3) % 4] : bodyRows[row - 1];
            for(int16_t col = 0; col < craftWidth; col++) if(bits & (0x80 >> col)) out.drawPixel(craftX + col, y + row, GameDraw::ON);
        }
    }
    else{
        //a flash ring for the first few ticks, then only the pieces falling
        if(crashTicks < 6) out.drawCircle(int16_t(lroundf(crashX)), int16_t(lroundf(crashY)), int16_t(1 + crashTicks / 2), GameDraw::ON);
        for(uint8_t i = 0; i < debrisCount; i++){
            const Debris& d = debris[i];
            int16_t x = int16_t(lroundf(d.x));
            int16_t y = int16_t(lroundf(d.y));
            if(d.b_big) out.fillRect(x, y, 2, 2, GameDraw::ON);
            else out.drawPixel(x, y, GameDraw::ON);
        }
    }

    //the distance at the top left, over the stretch of cave already passed (the craft flies further right), on a
    //dark patch so it never touches the rock
    char text[10];
    scoreText(text, sizeof(text), score());
    int16_t w = GameDraw::textWidth(text);
    out.fillRect(0, 0, w + 2, 9, GameDraw::OFF);
    GameDraw::text(out, 1, 1, text);

    //the hint right of the craft, in the middle of the cave, until the player starts
    if(b_ready){
        const char* hint = "CLICK TO FLY";
        GameDraw::pill(out, width - 3 - GameDraw::pillWidth(hint), 12, hint);
    }
}

void CaveGame::scoreText(char* out, size_t size, uint32_t value) const {
    snprintf(out, size, "%lum", (unsigned long)value);
}

void CaveGame::demo(GameInput& input){
    if(b_crashed) return;
    if(b_ready){
        if(frameTicks >= demoStartTicks) input.clicked[0] = true;
        return;
    }
    float speed = getScrollSpeed();
    //the way ahead: the narrowest opening over the next quarter of a second, a pillar passed on its open side
    int16_t end = int16_t(craftX + craftWidth + int16_t(speed * 0.25f));
    if(end > width - 1) end = width - 1;
    int16_t highest = 0;
    int16_t lowest = height;
    for(int16_t x = craftX - 1; x <= end; x++){
        if(ceilingAt(x) > highest) highest = ceilingAt(x);
        if(floorAt(x) < lowest) lowest = floorAt(x);
    }
    uint32_t first = firstColumn();
    for(uint8_t i = 0; i < maxPillars; i++){
        const Pillar& p = pillars[i];
        if(!p.b_used || p.column + 2 < first + uint32_t(craftX - 1) || p.column > first + uint32_t(end)) continue;
        int16_t x = int16_t(p.column - first) + 1;
        int16_t above = p.top - ceilingAt(x);
        int16_t below = floorAt(x) - p.bottom - 1;
        if(above >= below){
            if(p.top < lowest) lowest = p.top;
        }
        else if(p.bottom + 1 > highest) highest = p.bottom + 1;
    }
    //a bat ahead: passed on its open side like a pillar (its whole band, the bob included), and shot when it is in
    //line with the gun (the boom's row)
    int16_t gunRow = int16_t(lroundf(craftY)) + 2;
    for(uint8_t i = 0; i < maxBats; i++){
        int16_t bx, bandTop, bandBottom;
        if(!getBat(i, bx, bandTop, bandBottom)) continue;
        if(bx > craftX + craftWidth && bx < width - 8){
            int16_t top = batTop(bats[i]);
            if(gunRow >= top && gunRow < top + batHeight) input.stick[2] = 1.0f;
        }
        if(bx + batWidth < craftX - 1 || bx > end) continue;
        int16_t x = bx + batWidth / 2;
        if(x < 0) x = 0;
        int16_t above = bandTop - ceilingAt(x);
        int16_t below = floorAt(x) - bandBottom - 1;
        if(above >= below){
            if(bandTop < lowest) lowest = bandTop;
        }
        else if(bandBottom + 1 > highest) highest = bandBottom + 1;
    }
    float target;
    if(lowest - highest >= craftHeight + 1) target = float(highest + lowest - craftHeight) * 0.5f;
    else target = float(ceilingAt(end) + floorAt(end) - craftHeight) * 0.5f;

    //the skill gives out with the speed: the aim wanders further and the hand reacts later
    float skill = (speed - 30.0f) / 55.0f;
    float wander = 0.3f + skill * 3.5f;
    demoAimError = clampf(demoAimError + (demoRandom.unit() - 0.5f) * 0.8f, -wander, wander);
    float predicted = craftY + craftSpeedY * 0.18f;
    bool b_up = predicted > target + demoAimError;
    demoQueue = (demoQueue << 1) | (b_up ? 1u : 0u);
    uint8_t delay = uint8_t(1.0f + skill * 4.0f);
    input.held[0] = ((demoQueue >> delay) & 1u) != 0;
}
