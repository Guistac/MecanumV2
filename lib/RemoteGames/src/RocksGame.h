#pragma once
#include "Game.h"
#include <SoundKit.h>

//ROCKS: a twin-stick shooter in the spirit of Asteroids. The left stick flies the ship (with a little inertia), the
//right stick aims it and, pushed past half way, fires. Rocks split in two when hit; saucers (from wave 2) shoot slow
//shots at the ship, mines (from wave 3) creep after it; every 5th wave is a boss. Enemies drop power-ups: spread
//shot, rapid fire, a shield. Three ships; anything that touches the ship costs one.

//The game [D-157]: the screen wraps at every edge for the ship, the rocks and the shots, except across the boss
//Sound [D-157]: the music is a tension pulse, two low notes in turn (F4 and E4, as low as the disc plays) that
//speed up as the wave's rocks thin out (or the boss is worn down). Effects are synth notes in front (a timer square
//would cut the pulse), ranked because much happens at once: the ship lost over the jingles (power-up, wave clear,
//boss intro, boss down) over the bursts over the shots over the grains (the saucer's warble, the mine's ticking); a
//sound never cuts a more important one still playing, and the jingles at the end of a wave wait for the last burst
class RocksGame : public Game{
public:
    RocksGame();
    const char* name() const override { return "ROCKS"; }
    void reset(uint32_t seed) override;
    void update(const GameInput& input) override;
    void draw(Adafruit_GFX& out) const override;
    bool isOver() const override { return b_over; }
    uint32_t score() const override { return points; }
    void demo(GameInput& input) override;
    void helpLines(const char* lines[3]) const override {
        lines[0] = "L STICK: FLY";
        lines[1] = "R STICK: AIM + FIRE";
        lines[2] = "SHOOT ROCKS. 3 SHIPS";
    }
    void playMusic() override;

    //rock sizes: 0 large (7 px), 1 medium (4 px), 2 small (2 px)
    static const uint8_t sizeCount = 3;
    static const uint32_t pointsBySize[sizeCount];
    static const uint32_t pointsSaucer = 200;
    static const uint32_t pointsMine = 150;
    static const uint32_t pointsTurret = 300;
    static const uint32_t pointsPlate = 200;
    static const uint32_t pointsBoss = 2000;     //times the boss's number (1, 2, ...)
    static const uint8_t bossEvery = 5;

    enum EnemyKind : uint8_t { SAUCER, MINE };
    enum PowerKind : uint8_t { POWER_NONE, POWER_SPREAD, POWER_RAPID, POWER_SHIELD };
    //what the game last played in front (for the tests)
    enum SoundEffect : uint8_t { FX_NONE, FX_WARBLE, FX_MINE_TICK, FX_SHOT, FX_BOSS_HIT, FX_BURST, FX_POWER_UP,
                                 FX_WAVE_CLEAR, FX_BOSS_INTRO, FX_BOSS_DOWN, FX_SHIP_LOST };
    //the pulse's speed, percent: 100 at a wave's start, up to 300 as its last rock goes
    static const uint16_t pulseSlowest = 100;
    static const uint16_t pulseFastest = 300;

    //for the tests
    uint8_t getLives() const { return lives; }
    uint8_t getWave() const { return wave; }
    uint8_t getRockCount() const { return rockCount; }
    uint8_t getBulletCount() const { return bulletCount; }
    uint8_t getEnemyCount() const { return enemyCount; }
    uint8_t getShotCount() const { return shotCount; }
    uint8_t getPickupCount() const { return pickupCount; }
    uint8_t countRocks(uint8_t size) const;
    uint8_t countEnemies(uint8_t kind) const;
    float getShipX() const { return shipX; }
    float getShipY() const { return shipY; }
    float getAimX() const { return aimX; }
    float getAimY() const { return aimY; }
    uint8_t getLastSound() const { return lastSound; }
    //the pulse's speed for what is left of the wave now
    uint16_t pulseTempo() const;
    bool isShipAlive() const { return deathTicks == 0; }
    bool isInvulnerable() const { return invulnerableTicks > 0; }
    uint8_t getWeapon() const { return weaponTicks > 0 ? weapon : uint8_t(POWER_NONE); }
    bool hasShield() const { return b_shield; }
    bool isBossActive() const { return boss.b_active; }
    //the bare core: open (and hurt by shots) for 1.4 s, shut for 1.4 s
    bool isCoreOpen() const { return boss.b_active && boss.plateHp == 0 && (boss.coreCycle % 140) < 70; }
    bool isBossWave(uint8_t n) const { return n % bossEvery == 0; }
    uint8_t getBossHealth() const;
    //0, 1 the turrets, 2 the plate, 3 the core
    uint8_t getBossPartHealth(uint8_t part) const;
    uint8_t getBossesBeaten() const { return bossesBeaten; }
    bool isCelebrating() const { return celebrateTicks > 0 || bossDeathTicks > 0; }
    //the text announcing wave n, e.g. "WAVE 5 BOSS"
    static void waveLabel(char* out, size_t size, uint8_t n);
    //test set-ups: they replace what is on the screen
    void placeSingleRockForTest(float x, float y, float vx, float vy, uint8_t size);
    void placeEnemyForTest(uint8_t kind, float x, float y, float vx, float vy);
    void placeShotForTest(float x, float y, float vx, float vy);
    void placePickupForTest(uint8_t kind, float x, float y);
    void startWaveForTest(uint8_t n);
    //the boss's parts in screen coordinates, for tests and the demo: 0, 1 the turrets, 2 the plate, 3 the core
    void bossPartCentre(uint8_t part, float& x, float& y) const;

private:
    struct Rock{
        float x, y, vx, vy;
        uint8_t size;
        uint8_t shape;          //which outline of its size
        uint8_t turn;           //quarter turns, 0..3
        uint8_t spinTicks;      //ticks per quarter turn
        int8_t spin;            //+1 or -1
        uint8_t spinCount;
    };
    struct Bullet{
        float x, y, vx, vy;
        uint8_t life;           //ticks left
    };
    //a few dots flying apart where something broke
    struct Puff{
        float x, y;
        uint8_t age;
        bool b_big;             //a ring of eight sparks instead of four dots
    };
    struct Enemy{
        float x, y, vx, vy;
        float baseY, phase;     //the saucer weaves about its line
        uint16_t fireTicks;     //the saucer's next shot
        uint16_t age;
        uint8_t kind;
    };
    //the enemies' slow shots
    struct Shot{
        float x, y, vx, vy;
        uint16_t life;
    };
    struct Pickup{
        float x, y, vx, vy;
        uint16_t life;
        uint8_t kind;
    };
    struct Boss{
        bool b_active;
        float x, y, phase;
        uint8_t turretHp[2], plateHp, coreHp;
        uint8_t turretMax, plateMax, coreMax;
        uint16_t turretFire[2], fanFire;
        uint8_t flashTicks;     //a part just hit shows dark for a moment
        uint8_t flashPart;
        uint16_t coreCycle;     //the bare core opens and shuts in turn
        uint16_t fightTicks;
    };

    static const uint8_t maxRocks = 40;
    static const uint8_t maxBullets = 12;
    static const uint8_t maxPuffs = 8;
    static const uint8_t maxEnemies = 3;
    static const uint8_t maxShots = 16;
    static const uint8_t maxPickups = 2;

    GameRandom random;
    GameRandom demoRandom;
    Rock rocks[maxRocks];
    Bullet bullets[maxBullets];
    Puff puffs[maxPuffs];
    Enemy enemies[maxEnemies];
    Shot shots[maxShots];
    Pickup pickups[maxPickups];
    Boss boss;
    uint8_t rockCount = 0;
    uint8_t bulletCount = 0;
    uint8_t puffNext = 0;
    uint8_t enemyCount = 0;
    uint8_t shotCount = 0;
    uint8_t pickupCount = 0;

    float shipX = 64, shipY = 16, shipVX = 0, shipVY = 0;
    float aimX = 1, aimY = 0;               //unit vector, screen axes (y down)
    uint8_t lives = 3;
    uint8_t wave = 0;
    uint32_t points = 0;
    uint16_t fireCooldown = 0;
    uint16_t deathTicks = 0;                //counting the burst after a hit, 0 while the ship lives
    float deathX = 0, deathY = 0;
    uint16_t invulnerableTicks = 0;
    uint16_t waveTicks = 0;                 //the pause with the wave's name before it comes
    uint16_t enemyTicks = 0;                //until the next enemy comes
    uint8_t enemiesLeft = 0;                //enemies still to come this wave
    uint8_t weapon = POWER_NONE;
    uint16_t weaponTicks = 0;
    bool b_shield = false;
    uint8_t bossesBeaten = 0;
    uint16_t bossDeathTicks = 0;            //the boss blowing up
    uint16_t celebrateTicks = 0;            //"BOSS DOWN" and the bonus
    uint32_t lastBonus = 0;
    uint32_t ticks = 0;
    bool b_over = false;

    //the demo player's state: it decides a few times a second, with a little aim error
    float demoMoveX = 0, demoMoveY = 0;
    float demoAimError = 0;
    float demoWander = 0;                   //the direction it roams once tired
    float demoStamina = 45;                 //seconds before it starts to tire, different each game
    uint8_t demoThinkTicks = 0;

    void startWave();
    bool waveCleared() const;
    void addRock(float x, float y, float vx, float vy, uint8_t size);
    void breakRock(uint8_t index);
    void spawnEnemy();
    void removeEnemy(uint8_t index, bool b_shot);
    void updateEnemies();
    void fireShot(float x, float y, float speed, float angleOffset);
    void dropPickup(float x, float y, float vx, float vy);
    void collect(uint8_t kind);
    void updateBoss();
    bool hitBoss(float x, float y);
    void startBoss();
    void addPuff(float x, float y, bool b_big = false);
    //something touched the ship: the shield takes it, or the ship is lost. True when the ship was lost
    bool hitShip();
    void killShip();
    void fire();
    void addBullet(float angle);
    bool bossLane() const { return boss.b_active || bossDeathTicks > 0; }
    //the shortest way from a to b on the wrapping screen
    static float wrapDelta(float from, float to, float span);
    static float wrapInto(float value, float span);
    void drawRock(Adafruit_GFX& out, const Rock& rock, int16_t x, int16_t y) const;
    void drawShip(Adafruit_GFX& out, float x, float y) const;
    void drawBoss(Adafruit_GFX& out) const;
    void drawHud(Adafruit_GFX& out) const;

    //sound [D-157]: none of this touches the game's state or its random numbers (the demo stays the same with or
    //without sound). Ranks: a sound never cuts a playing one of a higher rank
    enum SoundRank : uint8_t { RANK_GRAIN, RANK_SHOT, RANK_BURST, RANK_JINGLE, RANK_SHIP_LOST };
    SoundKit::NoteList<4> pulseNotes;
    SoundKit::NoteList<8> zapNotes;
    SoundKit::NoteList<8> burstNotes;           //made for each burst (its size, a little random)
    SoundKit::NoteList<4> bossHitNotes;
    SoundKit::NoteList<8> powerUpNotes;
    SoundKit::NoteList<8> waveClearNotes;
    SoundKit::NoteList<8> bossIntroNotes;
    SoundKit::NoteList<40> bossDownNotes;
    SoundKit::NoteList<48> shipLostNotes;
    SoundKit::SoundRandom soundRandom;
    SoundKit::Grain warbleGrain;
    SoundKit::Grain mineGrain;
    float warblePhase = 0;
    uint8_t frontRank = RANK_GRAIN;             //of the last sound played in front
    uint8_t lastSound = FX_NONE;
    uint8_t pendingJingle = FX_NONE;            //a jingle waiting for the front to be free
    uint8_t pendingTicks = 0;                   //how long it still waits before it is dropped
    uint16_t waveHits = 0;                      //the shots the wave's rocks took at its start (the pulse's measure)
    uint16_t playedTempo = 0;                   //the pulse's tempo last set

    void buildSounds();
    void resetSound();
    //plays a cue in front unless a more important sound still plays there
    bool playEffect(const SoundCue& cue, uint8_t rank, uint8_t effect);
    void playBurst(uint8_t size);
    void queueJingle(uint8_t effect);
    //every tick: the grains, a waiting jingle, the pulse's tempo
    void updateSound();
    //the shots it takes to clear the rocks there are: a large rock 7, a medium 3, a small 1
    uint16_t remainingHits() const;
};
