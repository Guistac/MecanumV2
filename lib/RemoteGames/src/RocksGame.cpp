#include "RocksGame.h"
#include <math.h>
#include <stdio.h>

const uint32_t RocksGame::pointsBySize[RocksGame::sizeCount] = {20, 50, 100};
const uint16_t RocksGame::pulseSlowest;
const uint16_t RocksGame::pulseFastest;

namespace{
    const float shipSpeed = 45.0f;              //px/s with the left stick full over
    const float shipResponse = 6.0f;            //1/s: how fast the ship's speed follows the stick (the inertia)
    const float aimThreshold = 0.25f;           //right stick past this turns the ship
    const float fireThreshold = 0.5f;           //and past this it fires
    const uint16_t fireInterval = 8;            //ticks between shots: about 6 a second
    const uint16_t rapidInterval = 4;           //with rapid fire: 12 a second
    const float spreadAngle = 0.25f;            //the side shots of the spread shot, rad
    const float bulletSpeed = 85.0f;
    const uint8_t bulletLife = 25;              //0.5 s, about 40 px: a third of the screen
    const float shipRadius = 1.8f;
    const float rockRadius[RocksGame::sizeCount] = {3.4f, 2.0f, 1.0f};
    const uint8_t rockPixels[RocksGame::sizeCount] = {7, 4, 2};
    const uint16_t burstTicks = 60;             //the burst of a lost ship before it comes back
    const uint16_t invulnerableAfterRespawn = 110;
    const uint16_t invulnerableAfterShield = 50;
    const uint16_t wavePauseTicks = 70;         //"WAVE n ..." before it comes
    const float safeSpawnDistance = 34.0f;      //new rocks never appear close to the ship
    const uint8_t puffTicks = 12;
    const uint8_t burstPuffTicks = 30;          //a big burst: the boss blowing up, the fireworks after it

    //enemies: slow and readable, so a careful player always has time to see a shot and step aside
    const float saucerRadius = 3.6f;
    const float mineRadius = 2.4f;
    const uint16_t mineWarnTicks = 50;          //a new mine blinks in place before it moves, harmless
    const float shotRadius = 1.2f;
    const float pointBlank = 18.0f;             //nothing fires at a ship closer than this
    const uint16_t shotLife = 400;            //8 s: slow shots still cross the whole screen
    const uint16_t pickupLife = 450;            //9 s on the field, blinking the last 2.5 s
    const uint16_t pickupBlink = 125;
    const uint16_t weaponLife = 500;            //10 s of spread or rapid fire
    const float pickupRadius = 5.0f;

    //the boss: 14 x 16 px at the right edge, its front (left side) carries two turrets and an armour plate over
    //its core. The ship keeps clear of it
    const int16_t bossWidth = 14;
    const int16_t bossHeight = 16;
    const float bossHomeX = float(Game::width - bossWidth - 1);
    const float bossLaneGap = 16.0f;            //the ship stays this far left of the boss
    const int16_t turretTop[2] = {1, 12};
    const int16_t plateTop = 5, plateBottom = 10;
    const int16_t coreLeft = 3, coreTop = 6;
    const uint8_t hullStart[bossHeight] = {9, 6, 6, 6, 7, 8, 8, 8, 8, 8, 8, 7, 6, 6, 6, 9};
    const uint16_t bossBlowTicks = 80;
    const uint16_t celebrationTicks = 120;
    const int16_t barLeft = 34, barWidth = 60;  //the boss's health at the bottom edge

    //the sprites, one row per byte, the leftmost pixel in the highest bit of the sprite's width. Large rocks are
    //lumpy blobs with a pit or two, so they read as stone rather than as letters
    const uint8_t largeShapes[2][7] = {
        {0x1C, 0x3E, 0x7B, 0x7F, 0x37, 0x3F, 0x18},
        {0x38, 0x7E, 0x7F, 0x6F, 0x7D, 0x3E, 0x0E},
    };
    const uint8_t mediumShapes[2][4] = {
        {0x6, 0xF, 0xF, 0x6},
        {0x6, 0xF, 0xE, 0x4},
    };
    const uint8_t smallShape[2] = {0x3, 0x3};
    //the ship in 8 directions from two drawings turned by quarter turns: pointing right, and up and right
    const uint8_t shipRight[5] = {0x10, 0x0C, 0x0F, 0x0C, 0x10};
    const uint8_t shipUpRight[5] = {0x01, 0x06, 0x1C, 0x0C, 0x04};
    //a spiked mine with a hollow heart (5 x 5): neither looks like a rock
    const uint8_t mineShape[5] = {0x15, 0x0E, 0x1B, 0x0E, 0x15};
    const uint8_t turretShape[3] = {0x07, 0x1F, 0x07};
    //the power-ups' 5 x 5 signs: a fork (spread), a lightning bolt (rapid), a ring (shield)
    const uint8_t powerShapes[3][5] = {
        {0x11, 0x0A, 0x04, 0x04, 0x04},
        {0x06, 0x0C, 0x1F, 0x06, 0x0C},
        {0x0E, 0x11, 0x11, 0x11, 0x0E},
    };

    //an n x n sprite turned clockwise by quarter turns, top left at (left, top)
    void drawSprite(Adafruit_GFX& out, const uint8_t* rows, uint8_t n, uint8_t turn, int16_t left, int16_t top){
        for(uint8_t row = 0; row < n; row++){
            for(uint8_t column = 0; column < n; column++){
                if(!((rows[row] >> (n - 1 - column)) & 1)) continue;
                int16_t x = column, y = row;
                for(uint8_t k = 0; k < turn; k++){
                    int16_t turned = int16_t(n - 1 - y);
                    y = x;
                    x = turned;
                }
                out.drawPixel(left + x, top + y, GameDraw::ON);
            }
        }
    }

    //a w x h sprite as it is, top left at (left, top)
    void drawBits(Adafruit_GFX& out, const uint8_t* rows, uint8_t w, uint8_t h, int16_t left, int16_t top){
        for(uint8_t row = 0; row < h; row++){
            for(uint8_t column = 0; column < w; column++){
                if((rows[row] >> (w - 1 - column)) & 1) out.drawPixel(left + column, top + row, GameDraw::ON);
            }
        }
    }

    float lengthOf(float x, float y){ return sqrtf(x * x + y * y); }
};

float RocksGame::wrapInto(float value, float span){
    while(value < 0.0f) value += span;
    while(value >= span) value -= span;
    return value;
}

float RocksGame::wrapDelta(float from, float to, float span){
    float d = to - from;
    if(d > span * 0.5f) d -= span;
    if(d < -span * 0.5f) d += span;
    return d;
}

void RocksGame::waveLabel(char* out, size_t size, uint8_t n){
    if(n % bossEvery == 0) snprintf(out, size, "WAVE %u BOSS", unsigned(n));
    else if(n == 2) snprintf(out, size, "WAVE 2 SAUCERS");
    else if(n == 3) snprintf(out, size, "WAVE 3 MINES");
    else if(n % bossEvery == 1 && n > 1) snprintf(out, size, "WAVE %u FASTER", unsigned(n));
    else snprintf(out, size, "WAVE %u", unsigned(n));
}

RocksGame::RocksGame(){
    buildSounds();
}

void RocksGame::reset(uint32_t seed){
    random.seed(seed);
    resetSound();
    demoRandom.seed(seed * 2654435761u + 1u);
    rockCount = 0;
    bulletCount = 0;
    enemyCount = 0;
    shotCount = 0;
    pickupCount = 0;
    for(uint8_t i = 0; i < maxPuffs; i++){
        puffs[i].age = burstPuffTicks;
        puffs[i].b_big = true;
    }
    puffNext = 0;
    boss.b_active = false;
    boss.flashTicks = 0;
    shipX = width / 2;
    shipY = height / 2;
    shipVX = shipVY = 0;
    aimX = 1;
    aimY = 0;
    lives = 3;
    points = 0;
    fireCooldown = 0;
    deathTicks = 0;
    invulnerableTicks = 0;
    waveTicks = 0;
    enemyTicks = 0;
    enemiesLeft = 0;
    weapon = POWER_NONE;
    weaponTicks = 0;
    b_shield = false;
    bossesBeaten = 0;
    bossDeathTicks = 0;
    celebrateTicks = 0;
    lastBonus = 0;
    ticks = 0;
    b_over = false;
    demoMoveX = demoMoveY = 0;
    demoAimError = 0;
    demoWander = 0;
    //some attract games are played by a stronger hand that gets to the boss, others end early
    demoStamina = 15.0f + demoRandom.unit() * 55.0f;
    demoThinkTicks = 0;
    wave = 1;
    startWave();
}

void RocksGame::startWave(){
    if(isBossWave(wave)){
        startBoss();
        return;
    }
    //more and faster rocks each wave; seven large ones already fill the 32 px high screen. Each boss beaten
    //raises the top speed
    uint8_t count = uint8_t(wave + 2 > 7 ? 7 : wave + 2);
    float speed = 4.0f + 2.5f * float(wave);
    float topSpeed = 24.0f + 3.0f * float(bossesBeaten);
    if(speed > topSpeed) speed = topSpeed;
    for(uint8_t i = 0; i < count; i++){
        float x = 0, y = 0;
        for(int attempt = 0; attempt < 30; attempt++){
            x = random.unit() * width;
            y = random.unit() * height;
            if(lengthOf(wrapDelta(shipX, x, width), wrapDelta(shipY, y, height)) > safeSpawnDistance) break;
        }
        float angle = random.unit() * 6.2831853f;
        float v = speed * (0.8f + 0.4f * random.unit());
        addRock(x, y, cosf(angle) * v, sinf(angle) * v, 0);
    }
    waveHits = remainingHits();
    //the enemies come one kind at a time: saucers in wave 2, mines in wave 3, then both, more after each boss
    if(wave == 2 || wave == 3) enemiesLeft = 2;
    else if(wave >= 4) enemiesLeft = uint8_t(3 + bossesBeaten > 6 ? 6 : 3 + bossesBeaten);
    else enemiesLeft = 0;
    enemyTicks = 180;
}

bool RocksGame::waveCleared() const {
    return rockCount == 0 && enemyCount == 0 && !boss.b_active && bossDeathTicks == 0 && celebrateTicks == 0;
}

void RocksGame::startWaveForTest(uint8_t n){
    rockCount = enemyCount = shotCount = pickupCount = 0;
    boss.b_active = false;
    bossDeathTicks = celebrateTicks = waveTicks = 0;
    wave = n;
    startWave();
}

void RocksGame::addRock(float x, float y, float vx, float vy, uint8_t size){
    if(rockCount >= maxRocks) return;
    Rock& rock = rocks[rockCount++];
    rock.x = wrapInto(x, width);
    rock.y = wrapInto(y, height);
    rock.vx = vx;
    rock.vy = vy;
    rock.size = size;
    rock.shape = uint8_t(random.below(2));
    rock.turn = uint8_t(random.below(4));
    rock.spinTicks = uint8_t(random.between(25, 50));
    rock.spin = random.below(2) ? 1 : -1;
    rock.spinCount = 0;
}

void RocksGame::placeSingleRockForTest(float x, float y, float vx, float vy, uint8_t size){
    rockCount = 0;
    enemyCount = 0;
    enemiesLeft = 0;
    shotCount = 0;
    waveTicks = 0;
    addRock(x, y, vx, vy, size);
}

void RocksGame::placeEnemyForTest(uint8_t kind, float x, float y, float vx, float vy){
    enemiesLeft = 0;
    if(enemyCount >= maxEnemies) return;
    Enemy& enemy = enemies[enemyCount++];
    enemy.kind = kind;
    enemy.x = x;
    enemy.y = enemy.baseY = y;
    enemy.vx = vx;
    enemy.vy = vy;
    enemy.phase = 0;
    enemy.fireTicks = 30;
    enemy.age = kind == MINE ? mineWarnTicks : 0;
}

void RocksGame::placeShotForTest(float x, float y, float vx, float vy){
    if(shotCount >= maxShots) return;
    Shot& shot = shots[shotCount++];
    shot.x = x;
    shot.y = y;
    shot.vx = vx;
    shot.vy = vy;
    shot.life = shotLife;
}

void RocksGame::placePickupForTest(uint8_t kind, float x, float y){
    if(pickupCount >= maxPickups) return;
    Pickup& pickup = pickups[pickupCount++];
    pickup.kind = kind;
    pickup.x = x;
    pickup.y = y;
    pickup.vx = pickup.vy = 0;
    pickup.life = pickupLife;
}

uint8_t RocksGame::countRocks(uint8_t size) const {
    uint8_t n = 0;
    for(uint8_t i = 0; i < rockCount; i++) if(rocks[i].size == size) n++;
    return n;
}

uint8_t RocksGame::countEnemies(uint8_t kind) const {
    uint8_t n = 0;
    for(uint8_t i = 0; i < enemyCount; i++) if(enemies[i].kind == kind) n++;
    return n;
}

uint8_t RocksGame::getBossHealth() const {
    if(!boss.b_active) return 0;
    return uint8_t(boss.turretHp[0] + boss.turretHp[1] + boss.plateHp + boss.coreHp);
}

uint8_t RocksGame::getBossPartHealth(uint8_t part) const {
    if(!boss.b_active) return 0;
    return part < 2 ? boss.turretHp[part] : part == 2 ? boss.plateHp : boss.coreHp;
}

void RocksGame::addPuff(float x, float y, bool b_big){
    Puff& puff = puffs[puffNext];
    puffNext = uint8_t((puffNext + 1) % maxPuffs);
    puff.x = x;
    puff.y = y;
    puff.age = 0;
    puff.b_big = b_big;
}

void RocksGame::breakRock(uint8_t index){
    Rock parent = rocks[index];
    rocks[index] = rocks[--rockCount];
    points += pointsBySize[parent.size];
    addPuff(parent.x, parent.y);
    playBurst(parent.size);
    //rocks hardly ever carry a power-up: those are the enemies' loot
    if(parent.size == 0 && random.below(25) == 0) dropPickup(parent.x, parent.y, parent.vx, parent.vy);
    if(parent.size + 1 >= sizeCount) return;
    //two smaller, faster pieces fly apart on either side of the parent's course
    float speed = lengthOf(parent.vx, parent.vy) * 1.35f + 2.0f;
    float heading = atan2f(parent.vy, parent.vx);
    for(int side = -1; side <= 1; side += 2){
        float angle = heading + float(side) * (0.4f + 0.5f * random.unit());
        addRock(parent.x, parent.y, cosf(angle) * speed, sinf(angle) * speed, uint8_t(parent.size + 1));
    }
}

void RocksGame::spawnEnemy(){
    if(enemyCount >= maxEnemies) return;
    uint8_t kind = wave == 2 ? SAUCER : wave == 3 ? MINE : uint8_t(random.below(2));
    Enemy& enemy = enemies[enemyCount++];
    enemy.kind = kind;
    enemy.phase = random.unit() * 6.28f;
    enemy.age = 0;
    if(kind == SAUCER){
        //it comes in from the side away from the ship and crosses the whole screen
        bool b_fromLeft = shipX > width / 2;
        float speed = 14.0f + 3.0f * float(bossesBeaten);
        enemy.x = b_fromLeft ? -4.0f : float(width) + 4.0f;
        enemy.baseY = 9.0f + random.unit() * 14.0f;
        enemy.y = enemy.baseY;
        enemy.vx = b_fromLeft ? speed : -speed;
        enemy.vy = 0;
        enemy.fireTicks = 60;
    } else {
        //a mine appears far from the ship and blinks a moment before it moves
        float x = 0, y = 0;
        for(int attempt = 0; attempt < 30; attempt++){
            x = random.unit() * width;
            y = 4.0f + random.unit() * (height - 8);
            if(lengthOf(wrapDelta(shipX, x, width), wrapDelta(shipY, y, height)) > 45.0f) break;
        }
        enemy.x = x;
        enemy.y = enemy.baseY = y;
        enemy.vx = enemy.vy = 0;
        enemy.fireTicks = 0;
    }
}

void RocksGame::removeEnemy(uint8_t index, bool b_shot){
    Enemy gone = enemies[index];
    enemies[index] = enemies[--enemyCount];
    if(!b_shot) return;
    points += gone.kind == SAUCER ? pointsSaucer : pointsMine;
    addPuff(gone.x, gone.y);
    playBurst(1);
    //half the saucers and a third of the mines leave a power-up
    if(random.below(gone.kind == SAUCER ? 2 : 3) == 0) dropPickup(gone.x, gone.y, gone.vx, gone.vy);
}

void RocksGame::fireShot(float x, float y, float speed, float angleOffset){
    if(shotCount >= maxShots || deathTicks > 0) return;
    float dx = shipX - x, dy = shipY - y;
    float d = lengthOf(dx, dy);
    if(d < pointBlank) return;
    float angle = atan2f(dy, dx) + angleOffset;
    Shot& shot = shots[shotCount++];
    shot.x = x;
    shot.y = y;
    shot.vx = cosf(angle) * speed;
    shot.vy = sinf(angle) * speed;
    shot.life = shotLife;
}

void RocksGame::dropPickup(float x, float y, float vx, float vy){
    if(pickupCount >= maxPickups) return;
    Pickup& pickup = pickups[pickupCount++];
    //a shield on a ship that has one is no gift: then a weapon
    pickup.kind = uint8_t(1 + random.below(b_shield ? 2 : 3));
    //it stays clear of the edges and of the HUD, so it can be read and reached
    pickup.x = x < 6 ? 6 : x > width - 7 ? float(width - 7) : x;
    float lowest = bossLane() ? 23.0f : float(height - 6);
    pickup.y = y < 13 ? 13 : y > lowest ? lowest : y;
    if(bossLane() && pickup.x > bossHomeX - bossLaneGap - 4) pickup.x = bossHomeX - bossLaneGap - 4;
    pickup.vx = vx * 0.3f;
    pickup.vy = vy * 0.3f;
    pickup.life = pickupLife;
}

void RocksGame::collect(uint8_t kind){
    if(kind == POWER_SHIELD){
        b_shield = true;
    } else {
        weapon = kind;
        weaponTicks = weaponLife;
    }
    addPuff(shipX, shipY);
    playEffect(powerUpNotes.cue(), RANK_JINGLE, FX_POWER_UP);
}

void RocksGame::startBoss(){
    boss.b_active = true;
    boss.x = float(width) + 1.0f;
    boss.y = 8.5f;
    boss.phase = 0;
    //each boss after the first is tougher
    uint8_t level = bossesBeaten;
    boss.turretMax = uint8_t(6 + level);
    boss.plateMax = uint8_t(8 + 2 * level);
    boss.coreMax = uint8_t(12 + 4 * level);
    boss.turretHp[0] = boss.turretHp[1] = boss.turretMax;
    boss.plateHp = boss.plateMax;
    boss.coreHp = boss.coreMax;
    boss.turretFire[0] = 70;
    boss.turretFire[1] = 125;
    boss.fanFire = 100;
    boss.flashTicks = 0;
    boss.coreCycle = 0;
    boss.fightTicks = 0;
    enemiesLeft = 0;
    queueJingle(FX_BOSS_INTRO);
}

void RocksGame::bossPartCentre(uint8_t part, float& x, float& y) const {
    float left = floorf(boss.x), top = floorf(boss.y);
    if(part < 2){
        x = left + 2.0f;
        y = top + float(turretTop[part]) + 1.5f;
    } else if(part == 2){
        x = left + 1.0f;
        y = top + float(plateTop + plateBottom + 1) * 0.5f;
    } else {
        x = left + float(coreLeft) + 2.0f;
        y = top + float(coreTop) + 2.0f;
    }
}

void RocksGame::updateBoss(){
    if(!boss.b_active) return;
    const float dt = tickSeconds;
    if(boss.flashTicks > 0) boss.flashTicks--;
    float total = float(boss.turretMax * 2 + boss.plateMax + boss.coreMax);
    float health = float(getBossHealth()) / total;
    //it drifts faster and fires faster as it gets damaged
    boss.phase += dt * (0.8f + 1.6f * (1.0f - health));
    boss.y = 8.5f + 4.5f * sinf(boss.phase);
    if(boss.x > bossHomeX){
        boss.x -= 15.0f * dt;
        if(boss.x < bossHomeX) boss.x = bossHomeX;
        return;
    }
    if(boss.plateHp == 0) boss.coreCycle++;
    //it fires faster as it is damaged, and also the longer the fight lasts, so hanging back does not pay
    if(boss.fightTicks < 60000) boss.fightTicks++;
    float impatience = 1.0f - float(boss.fightTicks) / (50.0f * 90.0f);
    if(impatience < 0.5f) impatience = 0.5f;
    float pace = (0.4f + 0.6f * health) * impatience;
    float shotSpeed = 20.0f + 10.0f * (1.0f - health) + 3.0f * float(bossesBeaten);
    float left = floorf(boss.x), top = floorf(boss.y);
    for(uint8_t i = 0; i < 2; i++){
        if(boss.turretHp[i] == 0) continue;
        if(--boss.turretFire[i] == 0){
            fireShot(left - 1.0f, top + float(turretTop[i]) + 1.0f, shotSpeed, (random.unit() - 0.5f) * 0.15f);
            boss.turretFire[i] = uint16_t(float(110 + 20 * i) * pace);
        }
    }
    //once the plate or both turrets are gone, the front fires fans of three, faster when both are gone
    bool b_noTurrets = boss.turretHp[0] == 0 && boss.turretHp[1] == 0;
    if((boss.plateHp == 0 || b_noTurrets) && --boss.fanFire == 0){
        float y = top + float(coreTop) + 2.0f;
        for(int k = -1; k <= 1; k++) fireShot(left - 1.0f, y, shotSpeed * 0.9f, float(k) * 0.32f);
        boss.fanFire = uint16_t(float(b_noTurrets && boss.plateHp == 0 ? 110 : 160) * pace);
    }
}

bool RocksGame::hitBoss(float x, float y){
    if(!boss.b_active) return false;
    float lx = x - floorf(boss.x), ly = y - floorf(boss.y);
    if(lx < -0.5f || lx >= bossWidth || ly < -0.5f || ly >= bossHeight) return false;
    int16_t ix = int16_t(lx < 0 ? 0 : lx), iy = int16_t(ly < 0 ? 0 : ly);
    uint8_t part = 255;
    for(uint8_t i = 0; i < 2; i++){
        if(iy >= turretTop[i] && iy <= turretTop[i] + 2 && ix <= 4){
            //a destroyed turret leaves a stub that only stops the shot
            if(boss.turretHp[i] > 0) part = i;
            else if(ix < 3) return false;
            else return true;
        }
    }
    if(part == 255 && iy >= plateTop && iy <= plateBottom && ix <= 7){
        //while the plate stands it takes every shot at the core
        if(boss.plateHp > 0) part = 2;
        else if(iy >= coreTop && iy < coreTop + 4 && ix >= 2){
            //the bare core can be hurt only while its eye is open; shut, it just stops the shot
            if(!isCoreOpen()) return true;
            part = 3;
        }
    }
    if(part == 255) return ix >= hullStart[iy];

    boss.flashTicks = 4;
    boss.flashPart = part;
    float px, py;
    bossPartCentre(part, px, py);
    if(part < 2){
        if(--boss.turretHp[part] == 0){
            points += pointsTurret;
            addPuff(px, py);
            playBurst(0);
        }
        else playEffect(bossHitNotes.cue(), RANK_SHOT, FX_BOSS_HIT);
    } else if(part == 2){
        if(--boss.plateHp == 0){
            points += pointsPlate;
            addPuff(px, py);
            boss.fanFire = 60;
            playBurst(0);
        }
        else playEffect(bossHitNotes.cue(), RANK_SHOT, FX_BOSS_HIT);
    } else if(--boss.coreHp > 0){
        playEffect(bossHitNotes.cue(), RANK_SHOT, FX_BOSS_HIT);
    } else {
        //beaten: it blows up, its shots vanish, a big bonus and a power-up for the next waves
        boss.b_active = false;
        bossDeathTicks = 1;
        bossesBeaten++;
        lastBonus = pointsBoss * bossesBeaten;
        points += lastBonus;
        shotCount = 0;
        addPuff(px, py);
        dropPickup(px - 20.0f, py, 0, 0);
        playEffect(bossDownNotes.cue(), RANK_JINGLE, FX_BOSS_DOWN);
    }
    return true;
}

bool RocksGame::hitShip(){
    if(b_shield){
        b_shield = false;
        invulnerableTicks = invulnerableAfterShield;
        addPuff(shipX, shipY);
        playBurst(1);
        return false;
    }
    killShip();
    return true;
}

void RocksGame::killShip(){
    deathTicks = 1;
    deathX = shipX;
    deathY = shipY;
    if(lives > 0) lives--;
    bulletCount = 0;
    //a fresh ship starts clean: no shots on their way, no weapon
    shotCount = 0;
    weaponTicks = 0;
    playEffect(shipLostNotes.cue(), RANK_SHIP_LOST, FX_SHIP_LOST);
}

void RocksGame::addBullet(float angle){
    if(bulletCount >= maxBullets) return;
    Bullet& bullet = bullets[bulletCount++];
    float ax = cosf(angle), ay = sinf(angle);
    bullet.x = wrapInto(shipX + ax * 3.0f, width);
    bullet.y = wrapInto(shipY + ay * 3.0f, height);
    bullet.vx = ax * bulletSpeed;
    bullet.vy = ay * bulletSpeed;
    bullet.life = bulletLife;
}

void RocksGame::fire(){
    float angle = atan2f(aimY, aimX);
    if(getWeapon() == POWER_SPREAD){
        addBullet(angle - spreadAngle);
        addBullet(angle);
        addBullet(angle + spreadAngle);
    } else {
        addBullet(angle);
    }
    fireCooldown = getWeapon() == POWER_RAPID ? rapidInterval : fireInterval;
    playEffect(zapNotes.cue(), RANK_SHOT, FX_SHOT);
}

void RocksGame::updateEnemies(){
    const float dt = tickSeconds;
    //the next enemy, while the wave's rocks last; early on one at a time, two after the first boss
    if(rockCount == 0) enemiesLeft = 0;
    uint8_t alive = uint8_t(bossesBeaten > 0 ? 2 : 1);
    if(enemiesLeft > 0 && enemyCount < alive && waveTicks == 0){
        if(enemyTicks > 0) enemyTicks--;
        else {
            spawnEnemy();
            enemiesLeft--;
            enemyTicks = uint16_t(random.between(250, 400) - 40 * (bossesBeaten > 3 ? 3 : bossesBeaten));
        }
    }
    for(uint8_t i = 0; i < enemyCount;){
        Enemy& enemy = enemies[i];
        if(enemy.age < 60000) enemy.age++;
        if(enemy.kind == SAUCER){
            enemy.phase += dt * 2.2f;
            enemy.x += enemy.vx * dt;
            enemy.y = enemy.baseY + 4.0f * sinf(enemy.phase);
            //it shoots only while it is well on the screen, so a shot never comes from nowhere
            bool b_onScreen = enemy.x > 4.0f && enemy.x < width - 5.0f;
            if(enemy.fireTicks > 0) enemy.fireTicks--;
            if(enemy.fireTicks == 0 && b_onScreen){
                fireShot(enemy.x, enemy.y, 24.0f + 4.0f * float(bossesBeaten), (random.unit() - 0.5f) * 0.2f);
                enemy.fireTicks = uint16_t(95 - 12 * (bossesBeaten > 3 ? 3 : bossesBeaten));
            }
            if(enemy.x < -6.0f || enemy.x > width + 6.0f){
                removeEnemy(i, false);
                continue;
            }
        } else if(enemy.age > mineWarnTicks){
            //a mine steers slowly after the ship, across the wrap if that is shorter
            float dx = wrapDelta(enemy.x, shipX, width), dy = wrapDelta(enemy.y, shipY, height);
            float d = lengthOf(dx, dy);
            float speed = 7.0f + 2.0f * float(bossesBeaten) + (wave > 6 ? 2.0f : 0.0f);
            if(d > 0.5f && deathTicks == 0){
                enemy.vx += (dx / d * speed - enemy.vx) * 1.2f * dt;
                enemy.vy += (dy / d * speed - enemy.vy) * 1.2f * dt;
            }
            enemy.x = wrapInto(enemy.x + enemy.vx * dt, width);
            enemy.y = wrapInto(enemy.y + enemy.vy * dt, height);
        }
        i++;
    }
}

void RocksGame::update(const GameInput& input){
    if(b_over) return;
    ticks++;
    updateSound();
    const float dt = tickSeconds;
    for(uint8_t i = 0; i < maxPuffs; i++) if(puffs[i].age < (puffs[i].b_big ? burstPuffTicks : puffTicks)) puffs[i].age++;

    //a beaten boss: bursts all over it, then a short celebration with fireworks
    if(bossDeathTicks > 0){
        bossDeathTicks++;
        if(bossDeathTicks % 10 == 0) addPuff(boss.x + random.unit() * bossWidth, boss.y + random.unit() * bossHeight, true);
        if(bossDeathTicks > bossBlowTicks){
            bossDeathTicks = 0;
            celebrateTicks = celebrationTicks;
            queueJingle(FX_WAVE_CLEAR);
        }
    } else if(celebrateTicks > 0){
        celebrateTicks--;
        if(celebrateTicks % 14 == 0 && celebrateTicks > 30) addPuff(10.0f + random.unit() * 108.0f, 4.0f + random.unit() * 24.0f, true);
    }

    //a cleared screen: a short pause with the next wave's name, then the wave
    if(waveCleared() && waveTicks == 0){
        waveTicks = wavePauseTicks;
        //a boss wave had its jingle when the boss went down
        if(!isBossWave(wave)) queueJingle(FX_WAVE_CLEAR);
    }
    if(waveTicks > 0){
        waveTicks--;
        if(waveTicks == 0 && waveCleared()){
            wave++;
            startWave();
        }
    }

    if(deathTicks > 0){
        deathTicks++;
        if(deathTicks > burstTicks){
            if(lives == 0){
                b_over = true;
                return;
            }
            //back in the centre, blinking while it cannot be hit
            deathTicks = 0;
            shipX = width / 2;
            shipY = height / 2;
            shipVX = shipVY = 0;
            invulnerableTicks = invulnerableAfterRespawn;
        }
    } else {
        float targetVX = input.stick[0] * shipSpeed;
        float targetVY = -input.stick[1] * shipSpeed;
        shipVX += (targetVX - shipVX) * shipResponse * dt;
        shipVY += (targetVY - shipVY) * shipResponse * dt;
        shipY = wrapInto(shipY + shipVY * dt, height);
        if(bossLane()){
            //the boss holds the right edge: no wrap sideways, and the ship is pushed back out of its reach
            shipX += shipVX * dt;
            float limit = boss.x - bossLaneGap;
            if(shipX > limit){
                shipX = shipX - 1.5f < limit ? limit : shipX - 1.5f;
                if(shipVX > 0) shipVX = 0;
            }
            if(shipX < 2.0f){
                shipX = 2.0f;
                if(shipVX < 0) shipVX = 0;
            }
        } else {
            shipX = wrapInto(shipX + shipVX * dt, width);
        }

        float rx = input.stick[2];
        float ry = -input.stick[3];
        float deflection = lengthOf(rx, ry);
        if(deflection > aimThreshold){
            aimX = rx / deflection;
            aimY = ry / deflection;
        }
        if(fireCooldown > 0) fireCooldown--;
        uint8_t limit = getWeapon() == POWER_SPREAD ? 9 : getWeapon() == POWER_RAPID ? 8 : 4;
        uint8_t volley = getWeapon() == POWER_SPREAD ? 3 : 1;
        if(deflection > fireThreshold && fireCooldown == 0 && bulletCount + volley <= limit) fire();
        if(invulnerableTicks > 0) invulnerableTicks--;
        if(weaponTicks > 0) weaponTicks--;
    }

    for(uint8_t i = 0; i < bulletCount;){
        Bullet& bullet = bullets[i];
        float x = bullet.x + bullet.vx * dt;
        bullet.y = wrapInto(bullet.y + bullet.vy * dt, height);
        bool b_gone = --bullet.life == 0;
        if(bossLane()){
            if(x < 0 || x >= width) b_gone = true;
            bullet.x = x;
        } else {
            bullet.x = wrapInto(x, width);
        }
        if(b_gone) bullets[i] = bullets[--bulletCount];
        else i++;
    }

    for(uint8_t i = 0; i < rockCount; i++){
        Rock& rock = rocks[i];
        rock.x = wrapInto(rock.x + rock.vx * dt, width);
        rock.y = wrapInto(rock.y + rock.vy * dt, height);
        if(++rock.spinCount >= rock.spinTicks){
            rock.spinCount = 0;
            rock.turn = uint8_t((rock.turn + 4 + rock.spin) % 4);
        }
    }

    updateEnemies();
    updateBoss();

    //the enemies' shots fly straight and leave the screen: they never wrap round onto the ship
    for(uint8_t i = 0; i < shotCount;){
        Shot& shot = shots[i];
        shot.x += shot.vx * dt;
        shot.y += shot.vy * dt;
        if(--shot.life == 0 || shot.x < -2 || shot.x > width + 2 || shot.y < -2 || shot.y > height + 2) shots[i] = shots[--shotCount];
        else i++;
    }

    //power-ups drift to a halt and are taken by flying over them
    for(uint8_t i = 0; i < pickupCount;){
        Pickup& pickup = pickups[i];
        pickup.x = wrapInto(pickup.x + pickup.vx * dt, width);
        pickup.y = wrapInto(pickup.y + pickup.vy * dt, height);
        pickup.vx *= 0.97f;
        pickup.vy *= 0.97f;
        bool b_taken = deathTicks == 0 && lengthOf(wrapDelta(shipX, pickup.x, width), wrapDelta(shipY, pickup.y, height)) < pickupRadius;
        if(b_taken) collect(pickup.kind);
        if(b_taken || --pickup.life == 0) pickups[i] = pickups[--pickupCount];
        else i++;
    }

    //shots against rocks, enemies and the boss: one shot breaks one thing
    for(uint8_t b = 0; b < bulletCount;){
        bool b_hit = false;
        for(uint8_t r = 0; r < rockCount; r++){
            float dx = wrapDelta(bullets[b].x, rocks[r].x, width);
            float dy = wrapDelta(bullets[b].y, rocks[r].y, height);
            if(lengthOf(dx, dy) < rockRadius[rocks[r].size] + 0.8f){
                breakRock(r);
                b_hit = true;
                break;
            }
        }
        for(uint8_t e = 0; e < enemyCount && !b_hit; e++){
            float dx = wrapDelta(bullets[b].x, enemies[e].x, width);
            float dy = wrapDelta(bullets[b].y, enemies[e].y, height);
            if(lengthOf(dx, dy) < (enemies[e].kind == SAUCER ? saucerRadius : mineRadius) + 0.8f){
                removeEnemy(e, true);
                b_hit = true;
            }
        }
        if(!b_hit) b_hit = hitBoss(bullets[b].x, bullets[b].y);
        if(b_hit) bullets[b] = bullets[--bulletCount];
        else b++;
    }

    if(deathTicks == 0 && invulnerableTicks == 0){
        for(uint8_t r = 0; r < rockCount; r++){
            float dx = wrapDelta(shipX, rocks[r].x, width);
            float dy = wrapDelta(shipY, rocks[r].y, height);
            if(lengthOf(dx, dy) < rockRadius[rocks[r].size] + shipRadius){
                //a shield breaks the rock it stops
                if(!hitShip()) breakRock(r);
                return;
            }
        }
        for(uint8_t e = 0; e < enemyCount; e++){
            if(enemies[e].kind == MINE && enemies[e].age <= mineWarnTicks) continue;
            float dx = wrapDelta(shipX, enemies[e].x, width);
            float dy = wrapDelta(shipY, enemies[e].y, height);
            if(lengthOf(dx, dy) < (enemies[e].kind == SAUCER ? saucerRadius : mineRadius) + shipRadius){
                if(!hitShip()) removeEnemy(e, true);
                return;
            }
        }
        for(uint8_t s = 0; s < shotCount; s++){
            if(lengthOf(shots[s].x - shipX, shots[s].y - shipY) < shotRadius + shipRadius){
                shots[s] = shots[--shotCount];
                hitShip();
                return;
            }
        }
    }
}

void RocksGame::drawRock(Adafruit_GFX& out, const Rock& rock, int16_t left, int16_t top) const {
    const uint8_t* rows = rock.size == 0 ? largeShapes[rock.shape] : rock.size == 1 ? mediumShapes[rock.shape] : smallShape;
    drawSprite(out, rows, rockPixels[rock.size], rock.turn, left, top);
}

void RocksGame::drawShip(Adafruit_GFX& out, float x, float y) const {
    //the nearest of 8 directions: drawn sprites stay crisp where a 5 px triangle at any angle breaks up
    int direction = int(lroundf(atan2f(aimY, aimX) / 0.785398f));
    direction = (direction + 8) % 8;
    int16_t left = int16_t(floorf(x)) - 2, top = int16_t(floorf(y)) - 2;
    if(direction % 2 == 0) drawSprite(out, shipRight, 5, uint8_t(direction / 2), left, top);
    else drawSprite(out, shipUpRight, 5, uint8_t(((direction + 1) / 2) % 4), left, top);
}

void RocksGame::drawBoss(Adafruit_GFX& out) const {
    int16_t left = int16_t(floorf(boss.x)), top = int16_t(floorf(boss.y));
    //the hull, with a dark seam and two ports so it reads as a ship and not as a block
    for(int16_t row = 0; row < bossHeight; row++) out.drawFastHLine(left + hullStart[row], top + row, bossWidth - hullStart[row], GameDraw::ON);
    out.drawFastVLine(left + 11, top + 4, 8, GameDraw::OFF);
    out.drawPixel(left + 9, top + 2, GameDraw::OFF);
    out.drawPixel(left + 9, top + 13, GameDraw::OFF);
    bool b_flash = boss.flashTicks > 0;
    for(uint8_t i = 0; i < 2; i++){
        if(b_flash && boss.flashPart == i) continue;
        if(boss.turretHp[i] > 0) drawBits(out, turretShape, 5, 3, left, top + turretTop[i]);
        else out.drawFastHLine(left + 3, top + turretTop[i] + 1, 2, GameDraw::ON);
    }
    if(boss.plateHp > 0){
        if(!(b_flash && boss.flashPart == 2)) out.fillRect(left, top + plateTop, 2, plateBottom - plateTop + 1, GameDraw::ON);
        //the core shows behind it as a dark eye in a ring
        out.drawRoundRect(left + coreLeft, top + coreTop, 4, 4, 1, GameDraw::ON);
    } else if(!(b_flash && boss.flashPart == 3)){
        //bare, the core opens and shuts: lit while it can be hurt, an empty ring while it is shut
        if(isCoreOpen()) out.fillRoundRect(left + coreLeft, top + coreTop, 4, 4, 1, GameDraw::ON);
        else out.drawRoundRect(left + coreLeft, top + coreTop, 4, 4, 1, GameDraw::ON);
    }
}

void RocksGame::drawHud(Adafruit_GFX& out) const {
    //the score at the top left, the power-ups held beside it, the ships left as dots at the top right, each with a
    //dark margin
    char text[12];
    snprintf(text, sizeof(text), "%lu", (unsigned long)points);
    int16_t x = GameDraw::textWidth(text);
    out.fillRect(0, 0, x + 1, 8, GameDraw::OFF);
    GameDraw::text(out, 0, 0, text);
    x += 3;
    //the weapon's sign blinks its last two seconds
    if(getWeapon() != POWER_NONE){
        out.fillRect(x - 1, 0, 7, 7, GameDraw::OFF);
        if(weaponTicks > 100 || (weaponTicks / 8) % 2) drawBits(out, powerShapes[weapon - 1], 5, 5, x, 1);
        x += 7;
    }
    if(b_shield){
        out.fillRect(x - 1, 0, 7, 7, GameDraw::OFF);
        drawBits(out, powerShapes[POWER_SHIELD - 1], 5, 5, x, 1);
    }
    for(uint8_t i = 0; i < lives; i++){
        int16_t lx = int16_t(width - 2 - i * 4);
        out.fillRect(lx - 1, 0, 4, 3, GameDraw::OFF);
        out.fillRect(lx, 0, 2, 2, GameDraw::ON);
    }
    //the boss's health: a thin bar between two end marks at the bottom edge
    if(boss.b_active){
        float total = float(boss.turretMax * 2 + boss.plateMax + boss.coreMax);
        int16_t filled = int16_t(lroundf(float(barWidth) * float(getBossHealth()) / total));
        out.fillRect(barLeft - 3, 29, barWidth + 6, 3, GameDraw::OFF);
        out.drawFastVLine(barLeft - 2, 30, 2, GameDraw::ON);
        out.drawFastVLine(barLeft + barWidth + 1, 30, 2, GameDraw::ON);
        out.fillRect(barLeft, 30, filled, 2, GameDraw::ON);
    }
}

void RocksGame::draw(Adafruit_GFX& out) const {
    //everything near an edge is drawn a second time on the far side, so it slides across the wrap
    for(uint8_t i = 0; i < rockCount; i++){
        const Rock& rock = rocks[i];
        int16_t n = rockPixels[rock.size];
        int16_t left = int16_t(floorf(rock.x)) - n / 2;
        int16_t top = int16_t(floorf(rock.y)) - n / 2;
        for(int16_t dx = -width; dx <= width; dx += width){
            for(int16_t dy = -height; dy <= height; dy += height){
                if(left + dx > -n && left + dx < width && top + dy > -n && top + dy < height) drawRock(out, rock, left + dx, top + dy);
            }
        }
    }

    //power-ups: the sign in a rounded frame, blinking before it goes
    for(uint8_t i = 0; i < pickupCount; i++){
        const Pickup& pickup = pickups[i];
        if(pickup.life < pickupBlink && (pickup.life / 6) % 2) continue;
        int16_t left = int16_t(floorf(pickup.x)) - 4, top = int16_t(floorf(pickup.y)) - 4;
        out.fillRect(left - 1, top - 1, 11, 11, GameDraw::OFF);
        out.drawRoundRect(left, top, 9, 9, 2, GameDraw::ON);
        drawBits(out, powerShapes[pickup.kind - 1], 5, 5, left + 2, top + 2);
    }

    for(uint8_t i = 0; i < enemyCount; i++){
        const Enemy& enemy = enemies[i];
        int16_t ex = int16_t(floorf(enemy.x)), ey = int16_t(floorf(enemy.y));
        if(enemy.kind == SAUCER){
            //a dome on a wide rim (9 x 4) whose dark lights run along it
            out.drawFastHLine(ex - 1, ey - 2, 3, GameDraw::ON);
            out.drawFastHLine(ex - 3, ey - 1, 7, GameDraw::ON);
            out.drawFastHLine(ex - 4, ey, 9, GameDraw::ON);
            out.drawFastHLine(ex - 2, ey + 1, 5, GameDraw::ON);
            out.drawPixel(ex - 2 + 2 * int16_t((enemy.age / 6) % 3), ey, GameDraw::OFF);
        } else {
            //a new mine blinks where it will start; a live one shows its heart lit now and then
            if(enemy.age <= mineWarnTicks && (enemy.age / 5) % 2) continue;
            for(int16_t dx = -width; dx <= width; dx += width){
                for(int16_t dy = -height; dy <= height; dy += height){
                    int16_t mx = ex + dx, my = ey + dy;
                    if(mx < -3 || mx > width + 2 || my < -3 || my > height + 2) continue;
                    drawBits(out, mineShape, 5, 5, mx - 2, my - 2);
                    if((enemy.age / 12) % 2) out.drawPixel(mx, my, GameDraw::ON);
                }
            }
        }
    }

    if(boss.b_active) drawBoss(out);
    else if(bossDeathTicks > 0 && (bossDeathTicks / 6) % 2 == 0 && bossDeathTicks < bossBlowTicks - 20) drawBoss(out);

    for(uint8_t i = 0; i < bulletCount; i++) out.drawPixel(int16_t(bullets[i].x), int16_t(bullets[i].y), GameDraw::ON);

    //the enemies' shots: small crosses, bigger than the ship's dots
    for(uint8_t i = 0; i < shotCount; i++){
        int16_t sx = int16_t(floorf(shots[i].x)), sy = int16_t(floorf(shots[i].y));
        out.drawFastHLine(sx - 1, sy, 3, GameDraw::ON);
        out.drawPixel(sx, sy - 1, GameDraw::ON);
        out.drawPixel(sx, sy + 1, GameDraw::ON);
    }

    //something broken: four dots flying apart; a big burst is a ring of eight sparks that thins out
    for(uint8_t i = 0; i < maxPuffs; i++){
        if(puffs[i].b_big){
            if(puffs[i].age >= burstPuffTicks) continue;
            float distance = 1.0f + float(puffs[i].age) * 0.3f;
            for(int k = 0; k < 8; k++){
                if(puffs[i].age > burstPuffTicks / 2 && (k & 1)) continue;
                float angle = float(k) * 0.785398f;
                out.drawPixel(int16_t(lroundf(puffs[i].x + cosf(angle) * distance)), int16_t(lroundf(puffs[i].y + sinf(angle) * distance)), GameDraw::ON);
            }
            continue;
        }
        if(puffs[i].age >= puffTicks) continue;
        int16_t spread = int16_t(2 + puffs[i].age / 3);
        int16_t px = int16_t(puffs[i].x), py = int16_t(puffs[i].y);
        out.drawPixel(px - spread, py - spread, GameDraw::ON);
        out.drawPixel(px + spread, py - spread, GameDraw::ON);
        out.drawPixel(px - spread, py + spread, GameDraw::ON);
        out.drawPixel(px + spread, py + spread, GameDraw::ON);
    }

    if(deathTicks > 0){
        //the lost ship: eight sparks flying out, then fading to a few
        float distance = 1.5f + float(deathTicks) * 0.35f;
        for(int k = 0; k < 8; k++){
            if(deathTicks > burstTicks / 2 && (k & 1)) continue;
            float angle = float(k) * 0.785398f + 0.3f;
            int16_t x = int16_t(lroundf(deathX + cosf(angle) * distance * (k % 3 == 0 ? 0.7f : 1.0f)));
            int16_t y = int16_t(lroundf(deathY + sinf(angle) * distance * (k % 3 == 0 ? 0.7f : 1.0f)));
            out.drawPixel(x, y, GameDraw::ON);
        }
    } else if(invulnerableTicks == 0 || (invulnerableTicks / 6) % 2 == 0){
        for(int16_t dx = -width; dx <= width; dx += width){
            for(int16_t dy = -height; dy <= height; dy += height){
                if(shipX + dx > -4 && shipX + dx < width + 4 && shipY + dy > -4 && shipY + dy < height + 4) drawShip(out, shipX + dx, shipY + dy);
            }
        }
    }
    //the shield: six dots circling the ship
    if(b_shield && deathTicks == 0){
        float turn = float(ticks) * 0.08f;
        for(int k = 0; k < 6; k++){
            float angle = turn + float(k) * 1.0471976f;
            out.drawPixel(int16_t(lroundf(shipX + cosf(angle) * 4.6f)), int16_t(lroundf(shipY + sinf(angle) * 4.6f)), GameDraw::ON);
        }
    }

    char label[20];
    label[0] = 0;
    if(waveTicks > 0 && !b_over) waveLabel(label, sizeof(label), uint8_t(wave + 1));
    else if(celebrateTicks > 0) snprintf(label, sizeof(label), "BOSS DOWN +%lu", (unsigned long)lastBonus);
    if(label[0]){
        int16_t w = GameDraw::textWidth(label);
        out.fillRect(width / 2 - w / 2 - 2, 11, w + 4, 9, GameDraw::OFF);
        GameDraw::textCentered(out, width / 2, 12, label);
    }

    drawHud(out);
}

void RocksGame::demo(GameInput& input){
    if(b_over || deathTicks > 0) return;
    const float seconds = float(ticks) * tickSeconds;

    //what it shoots: the nearest rock or enemy (enemies first, they shoot back), else a part of the boss
    float targetX = 0, targetY = 0, targetVX = 0, targetVY = 0;
    float nearestDistance = 1e9f;
    bool b_target = false;
    for(uint8_t i = 0; i < rockCount; i++){
        float d = lengthOf(wrapDelta(shipX, rocks[i].x, width), wrapDelta(shipY, rocks[i].y, height)) - rockRadius[rocks[i].size];
        if(d < nearestDistance){
            nearestDistance = d;
            targetX = rocks[i].x;
            targetY = rocks[i].y;
            targetVX = rocks[i].vx;
            targetVY = rocks[i].vy;
            b_target = true;
        }
    }
    for(uint8_t i = 0; i < enemyCount; i++){
        const Enemy& enemy = enemies[i];
        if(enemy.kind == SAUCER && (enemy.x < 0 || enemy.x >= width)) continue;
        float d = lengthOf(wrapDelta(shipX, enemy.x, width), wrapDelta(shipY, enemy.y, height)) - 12.0f;
        if(d < nearestDistance){
            nearestDistance = d;
            targetX = enemy.x;
            targetY = enemy.y;
            targetVX = enemy.vx;
            targetVY = enemy.kind == SAUCER ? 4.0f * 2.2f * cosf(enemy.phase) : enemy.vy;
            b_target = true;
        }
    }
    if(boss.b_active){
        //the turret nearest in height first, then the plate, then the core
        uint8_t part = 3;
        if(boss.turretHp[0] > 0 || boss.turretHp[1] > 0){
            float y0, y1, px;
            bossPartCentre(0, px, y0);
            bossPartCentre(1, px, y1);
            if(boss.turretHp[0] == 0) part = 1;
            else if(boss.turretHp[1] == 0) part = 0;
            else part = fabsf(y0 - shipY) < fabsf(y1 - shipY) ? 0 : 1;
        } else if(boss.plateHp > 0) part = 2;
        bossPartCentre(part, targetX, targetY);
        targetVX = 0;
        targetVY = 0;
        nearestDistance = lengthOf(targetX - shipX, targetY - shipY);
        b_target = true;
    }

    //it reconsiders its course a few times a second, like a thumb on a stick; its skill fades after a while
    if(demoThinkTicks == 0){
        //a tired thumb also reacts later
        float tired = (seconds - demoStamina) / 20.0f;
        tired = tired < 0.0f ? 0.0f : tired > 1.0f ? 1.0f : tired;
        demoThinkTicks = uint8_t(5 + tired * 15.0f);
        float pushX = 0, pushY = 0;
        for(uint8_t i = 0; i < rockCount; i++){
            const Rock& rock = rocks[i];
            //where the rock will be in a moment
            float rx = rock.x + rock.vx * 0.25f, ry = rock.y + rock.vy * 0.25f;
            float dx = wrapDelta(rx, shipX, width), dy = wrapDelta(ry, shipY, height);
            float d = lengthOf(dx, dy);
            float range = 9.0f + rockRadius[rock.size] * 2.0f;
            if(d < range && d > 0.01f){
                float weight = (range - d) / range;
                pushX += dx / d * weight * 2.5f;
                pushY += dy / d * weight * 2.5f;
            }
        }
        for(uint8_t i = 0; i < enemyCount; i++){
            const Enemy& enemy = enemies[i];
            float dx = wrapDelta(enemy.x + enemy.vx * 0.3f, shipX, width), dy = wrapDelta(enemy.y + enemy.vy * 0.3f, shipY, height);
            float d = lengthOf(dx, dy);
            if(d < 14.0f && d > 0.01f){
                float weight = (14.0f - d) / 14.0f;
                pushX += dx / d * weight * 3.0f;
                pushY += dy / d * weight * 3.0f;
            }
        }
        //a shot: where it passes closest in the next second, and step away from that line
        for(uint8_t i = 0; i < shotCount; i++){
            const Shot& shot = shots[i];
            float px = shot.x - shipX, py = shot.y - shipY;
            float v2 = shot.vx * shot.vx + shot.vy * shot.vy;
            float t = v2 > 0 ? -(px * shot.vx + py * shot.vy) / v2 : 0;
            if(t < 0 || t > 1.5f) continue;
            float cx = px + shot.vx * t, cy = py + shot.vy * t;
            float c = lengthOf(cx, cy);
            if(c > 8.0f) continue;
            float awayX, awayY;
            if(c > 0.3f){
                awayX = -cx / c;
                awayY = -cy / c;
            } else {
                //straight at it: sideways, towards the middle of the screen
                float v = sqrtf(v2);
                awayX = -shot.vy / v;
                awayY = shot.vx / v;
                if(awayY * (height / 2 - shipY) < 0){
                    awayX = -awayX;
                    awayY = -awayY;
                }
            }
            float weight = (8.0f - c) / 8.0f * (1.8f - t);
            pushX += awayX * weight * 4.0f;
            pushY += awayY * weight * 4.0f;
        }
        //a power-up in reach is worth a detour
        for(uint8_t i = 0; i < pickupCount; i++){
            float dx = wrapDelta(shipX, pickups[i].x, width), dy = wrapDelta(shipY, pickups[i].y, height);
            float d = lengthOf(dx, dy);
            if(d < 55.0f && d > 0.01f){
                pushX += dx / d * 0.9f;
                pushY += dy / d * 0.9f;
            }
        }
        if(boss.b_active){
            //facing the boss: within range of its front, level with the part it shoots
            pushX += (targetX - 34.0f - shipX) * 0.05f;
            pushY += (targetY - shipY) * 0.04f;
        } else {
            //nothing within range: it goes hunting instead of waiting
            if(b_target && nearestDistance > 28.0f){
                float hx = wrapDelta(shipX, targetX, width), hy = wrapDelta(shipY, targetY, height);
                float h = lengthOf(hx, hy);
                pushX += hx / h * 0.6f;
                pushY += hy / h * 0.6f;
            }
            //a weak pull back towards the middle band, so it does not sit on the wrap line
            pushY += wrapDelta(shipY, height / 2, height) * 0.02f;
        }
        //it tires: dodges less and aims worse as the game goes on, so the attract mode ends within a minute or two
        float care = 1.0f - tired;
        //and instead of holding a safe spot it starts to roam about
        demoWander += (demoRandom.unit() - 0.5f) * 0.6f;
        float roam = (1.0f - care) * 0.9f;
        demoMoveX = pushX * care + cosf(demoWander) * roam;
        demoMoveY = pushY * care + sinf(demoWander) * roam;
        float m = lengthOf(demoMoveX, demoMoveY);
        if(m > 1.0f){
            demoMoveX /= m;
            demoMoveY /= m;
        }
        float aimLimit = 0.15f + (1.0f - care) * 0.4f;
        demoAimError += (demoRandom.unit() - 0.5f) * aimLimit;
        if(demoAimError > aimLimit) demoAimError = aimLimit;
        if(demoAimError < -aimLimit) demoAimError = -aimLimit;
    }
    demoThinkTicks--;
    input.stick[0] = demoMoveX;
    input.stick[1] = -demoMoveY;

    if(!b_target) return;
    //lead the target: where it will be when the shot gets there
    float dx = wrapDelta(shipX, targetX, width), dy = wrapDelta(shipY, targetY, height);
    for(int pass = 0; pass < 2; pass++){
        float t = lengthOf(dx, dy) / bulletSpeed;
        dx = wrapDelta(shipX, targetX + targetVX * t, width);
        dy = wrapDelta(shipY, targetY + targetVY * t, height);
    }
    float angle = atan2f(dy, dx) + demoAimError;
    //it fires when the shot will get there: about 40 px
    float strength = nearestDistance < 42.0f || (boss.b_active && fabsf(dx) < 42.0f) ? 1.0f : 0.4f;
    input.stick[2] = cosf(angle) * strength;
    input.stick[3] = -sinf(angle) * strength;
}

//The sound [D-157]. Everything here only reads the game: its own random numbers, its own counters

namespace{
    using W = Synth::Wave;
    const uint8_t jingleWaitTicks = 50;         //a jingle waits up to 1 s for the burst before it, then is dropped
    const uint8_t warblePeriodTicks = 2;        //a saucer's warble: a grain every 40 ms, its pitch swinging
    const uint16_t warbleMillis = 32;
    const float warbleCentre = 1300.0f;
    const float warbleSwing = 350.0f;
    const float warbleStep = 1.1f;              //rad a grain: a swing about every 230 ms
    const uint16_t mineTickMillis = 10;
    const uint16_t pulseBeatMillis = 600;       //at tempo 100: a beat every 0.6 s, every 0.2 s at 300
}

void RocksGame::buildSounds(){
    //(SoundKit::A3 by its full name: the Teensy has an analog pin A3)
    using namespace SoundKit;
    //the pulse: F4 then E4 in turn, 150 ms held on the narrowest pulse at full level. Low enough to stay ominous, high
    //enough for the disc (nearly silent below 400 Hz: its harmonics, 1 to 4 kHz, carry the notes)
    pulseNotes.clear();
    pulseNotes.synth(0, hertz(F4), 150, 100, 0, W::PULSE12, 1);
    pulseNotes.synth(pulseBeatMillis, hertz(E4), 150, 100, 0, W::PULSE12, 1);
    //a shot: a quick falling zap
    zapNotes.clear();
    zapNotes.sweep(0, 2400, 600, 60, 6, W::SQUARE, 75, 0, 15);
    //a part of the boss hit: a dull clank
    bossHitNotes.clear();
    bossHitNotes.synth(0, 330, 30, 85, 12, W::PULSE25).burst(0, 1800, 20, 70, 8, 1);
    //a power-up: C5 E5 G5 C6 E6 up, the last one ringing
    static const uint8_t powerUp[] = {C5, E5, G5, C6, E6};
    powerUpNotes.clear();
    for(uint8_t i = 0; i < sizeof(powerUp); i++){
        bool b_last = i == sizeof(powerUp) - 1;
        powerUpNotes.synth(uint16_t(i * 45), hertz(powerUp[i]), b_last ? 140 : 40, 85, b_last ? 70 : 0);
    }
    //a wave cleared (or a boss down): G5 C6 E6 G6, then C7 over C6
    static const uint8_t cleared[] = {G5, C6, E6, G6};
    waveClearNotes.clear();
    for(uint8_t i = 0; i < sizeof(cleared); i++) waveClearNotes.synth(uint16_t(i * 80), hertz(cleared[i]), 70, 90);
    waveClearNotes.synth(320, hertz(C7), 220, 95, 110).synth(320, hertz(C6), 220, 70, 110, W::PULSE25, 1);
    //the boss comes: a warning trill E4 F4 E4 F4, then a long D4 over A3
    static const uint8_t intro[] = {E4, F4, E4, F4};
    bossIntroNotes.clear();
    for(uint8_t i = 0; i < sizeof(intro); i++) bossIntroNotes.synth(uint16_t(i * 70), hertz(intro[i]), 60, 90, 0, W::PULSE25);
    bossIntroNotes.synth(290, hertz(D4), 380, 95, 200, W::PULSE25).synth(290, hertz(SoundKit::A3), 380, 80, 200, W::SQUARE, 1);
    //the boss blows up: a long noise falling, a low tone falling under it
    bossDownNotes.clear();
    bossDownNotes.sweep(0, 2500, 200, 900, 18, W::NOISE, 100, 0, 250);
    bossDownNotes.sweep(0, 500, 100, 900, 18, W::SQUARE, 70, 1, 200);
    //the ship lost: the longest, a noise falling from bright to dull over a second, a tone falling under it
    shipLostNotes.clear();
    shipLostNotes.sweep(0, 4000, 300, 1100, 22, W::NOISE, 100, 0, 300);
    shipLostNotes.sweep(0, 700, 110, 1000, 20, W::SQUARE, 70, 1, 200);
}

void RocksGame::resetSound(){
    frontRank = RANK_GRAIN;
    lastSound = FX_NONE;
    pendingJingle = FX_NONE;
    pendingTicks = 0;
    warblePhase = 0;
    waveHits = 0;
    playedTempo = 0;
}

void RocksGame::playMusic(){
    sound().play(pulseNotes.cue(SoundPriority::GAME, uint16_t(2 * pulseBeatMillis)), SoundLayer::BACK);
    playedTempo = pulseTempo();
    sound().setTempo(SoundLayer::BACK, playedTempo);
}

bool RocksGame::playEffect(const SoundCue& cue, uint8_t rank, uint8_t effect){
    SoundSink& out = sound();
    if(out.isPlaying(SoundLayer::FRONT) && rank < frontRank) return false;
    if(!out.play(cue, SoundLayer::FRONT)) return false;
    frontRank = rank;
    lastSound = effect;
    return true;
}

//a burst of noise, duller and longer for a bigger rock (a large one with a low thump under it), each a little
//different
void RocksGame::playBurst(uint8_t size){
    if(sound().isPlaying(SoundLayer::FRONT) && RANK_BURST < frontRank) return;
    static const uint16_t brightness[sizeCount] = {1100, 2000, 3400};
    static const uint16_t millis[sizeCount] = {260, 160, 90};
    static const uint16_t decay[sizeCount] = {110, 60, 30};
    if(size >= sizeCount) size = sizeCount - 1;
    float vary = 0.92f + 0.16f * soundRandom.unit();
    burstNotes.clear();
    burstNotes.burst(0, uint16_t(brightness[size] * vary), millis[size], 100, decay[size]);
    if(size == 0) burstNotes.synth(0, 140, 90, 85, 45, W::PULSE25, 1);
    playEffect(burstNotes.cue(), RANK_BURST, FX_BURST);
}

void RocksGame::queueJingle(uint8_t effect){
    pendingJingle = effect;
    pendingTicks = jingleWaitTicks;
}

uint16_t RocksGame::remainingHits() const {
    static const uint8_t hits[sizeCount] = {7, 3, 1};
    uint16_t n = 0;
    for(uint8_t i = 0; i < rockCount; i++) n = uint16_t(n + hits[rocks[i].size < sizeCount ? rocks[i].size : 0]);
    return n;
}

uint16_t RocksGame::pulseTempo() const {
    float share;
    if(boss.b_active){
        //the boss: from its health
        float total = float(boss.turretMax * 2 + boss.plateMax + boss.coreMax);
        share = total > 0.0f ? 1.0f - float(getBossHealth()) / total : 0.0f;
    } else {
        uint16_t left = remainingHits();
        if(left == 0) share = enemyCount > 0 ? 1.0f : 0.0f;    //the calm between waves, unless a straggler flies
        else share = waveHits > left ? 1.0f - float(left) / float(waveHits) : 0.0f;
    }
    uint16_t tempo = uint16_t(float(pulseSlowest) + share * float(pulseFastest - pulseSlowest));
    return uint16_t(tempo / 5 * 5);
}

void RocksGame::updateSound(){
    SoundSink& out = sound();
    //the pulse follows what is left of the wave
    uint16_t tempo = pulseTempo();
    if(tempo != playedTempo){
        playedTempo = tempo;
        out.setTempo(SoundLayer::BACK, tempo);
    }

    //a jingle plays once the front is free of anything that matters as much as a burst
    if(pendingJingle != FX_NONE){
        bool b_free = !out.isPlaying(SoundLayer::FRONT) || frontRank < RANK_BURST;
        if(b_free){
            const SoundCue cue = pendingJingle == FX_BOSS_INTRO ? bossIntroNotes.cue() : waveClearNotes.cue();
            playEffect(cue, RANK_JINGLE, pendingJingle);
            pendingJingle = FX_NONE;
        }
        else if(--pendingTicks == 0) pendingJingle = FX_NONE;
    }

    //the grains, skipped while anything else plays in front: a saucer on the screen warbles, the nearest mine ticks
    //(faster the closer it is, quick while it blinks before it moves)
    bool b_saucer = false;
    float mineDistance = 1000.0f;
    bool b_mineWaking = false;
    for(uint8_t i = 0; i < enemyCount; i++){
        const Enemy& enemy = enemies[i];
        if(enemy.kind == SAUCER){
            if(enemy.x > 0.0f && enemy.x < float(width)) b_saucer = true;
            continue;
        }
        float d = lengthOf(wrapDelta(shipX, enemy.x, width), wrapDelta(shipY, enemy.y, height));
        if(d < mineDistance){
            mineDistance = d;
            b_mineWaking = enemy.age <= mineWarnTicks;
        }
    }
    if(b_saucer && warbleGrain.due(warblePeriodTicks)){
        warblePhase += warbleStep;
        if(warblePhase > 6.2831853f) warblePhase -= 6.2831853f;
        uint16_t pitch = uint16_t(warbleCentre + warbleSwing * sinf(warblePhase));
        if(warbleGrain.play(out, SoundKit::synthNote(0, pitch, warbleMillis, 65, 10))){
            frontRank = RANK_GRAIN;
            lastSound = FX_WARBLE;
        }
    }
    if(mineDistance < 1000.0f){
        float period = b_mineWaking ? 5.0f : 4.0f + mineDistance / 5.0f;
        if(period > 25.0f) period = 25.0f;
        uint16_t pitch = b_mineWaking ? 2200 : 2800;
        if(mineGrain.due(uint8_t(period)) && mineGrain.play(out, SoundKit::synthNote(0, pitch, mineTickMillis, 75, 5))){
            frontRank = RANK_GRAIN;
            lastSound = FX_MINE_TICK;
        }
    }
}
