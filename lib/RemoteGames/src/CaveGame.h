#pragma once
#include "Game.h"
#include <SoundKit.h>

//CAVE: a small helicopter flies through a cave that scrolls from right to left, ever faster and narrower, with
//pillars and stalactites further in. Push a stick up to climb, as much as it is pushed (at rest the craft sinks
//gently); a stick button held is full thrust. The craft has momentum and hovers until the first click or push up.
//Touching anything crashes it. Score: the distance flown in metres, plus a bonus for what it shoots.
//Shooting [owner 2026-10-28]: a stick pushed right fires (held: a shot every 0.4 s). From about 65 m on, a bat hovers
//now and then where the cave is wide, always leaving a way past it at least 9 rows tall, so a bat is never in the
//way for good: shooting is a choice, not a must. A bat shot is 10 m, a free standing pillar shot to pieces 5 m; the
//stalactites and the walls stop a shot.
//Sound [D-157]: no music; the rotor chops (noise ticks, faster with more thrust), a ping every 100 m, and the crash:
//a burst of noise and a falling whistle.
//[D-157]
class CaveGame : public Game{
public:
    CaveGame();
    const char* name() const override { return "CAVE"; }
    void reset(uint32_t seed) override;
    void update(const GameInput& input) override;
    void draw(Adafruit_GFX& out) const override;
    bool isOver() const override { return b_crashed && crashTicks >= crashShowTicks; }
    uint32_t score() const override { return distance() + bonusMetres; }
    //the metres flown, without the bonus
    uint32_t distance() const { return uint32_t(scrollPx / pixelsPerMetre); }
    void scoreText(char* out, size_t size, uint32_t value) const override;
    void demo(GameInput& input) override;
    void helpLines(const char* lines[3]) const override {
        lines[0] = "Stick up: climb";
        lines[1] = "Click held: full up";
        lines[2] = "Stick right: fire";
    }

    static const int16_t craftX = 36;               //screen column of the craft's left edge
    static const int16_t craftWidth = 8;
    static const int16_t craftHeight = 4;
    static constexpr float pixelsPerMetre = 4.0f;
    static constexpr float gravity = 50.0f;         //px/s2, down
    static constexpr float thrust = 120.0f;         //px/s2, up, at full input: the stick hovers at about 0.4
    static constexpr float maxFallSpeed = 40.0f;
    static constexpr float maxClimbSpeed = 28.0f;
    static constexpr float startStickUp = 0.3f;     //a stick pushed up this far ends the ready phase
    static const uint8_t readyTicks = 150;          //the craft hovers at most 3 s before it flies on its own
    static const uint8_t spoolTicks = 60;           //thrust and gravity come in over the first 1.2 s of flight
    static const uint8_t crashShowTicks = 25;       //half a second of debris before the game is over
    static const uint16_t pingMetres = 100;
    //shooting [owner 2026-10-28]
    static const uint8_t maxBats = 3;
    static const uint8_t maxShots = 3;
    static constexpr float fireStick = 0.6f;        //a stick's x past this fires
    static constexpr float shotSpeed = 110.0f;      //px/s across the screen
    static const uint8_t shotCooldownTicks = 20;    //a shot every 0.4 s while the stick stays pushed
    static const uint32_t batsFrom = 390;           //columns: the first ones come into view at about 65 m
    static const int16_t batWidth = 5;
    static const int16_t batHeight = 3;
    static const int16_t batSway = 2;               //it bobs this far up and down
    static const int16_t batWayPast = 9;            //rows always open beside a bat's band
    static const uint16_t batBonus = 10;            //metres
    static const uint16_t pillarBonus = 5;

    //for the tests and the demo: the state read from outside
    float getCraftY() const { return craftY; }
    float getVerticalSpeed() const { return craftSpeedY; }
    float getScrollSpeed() const;
    bool isCrashed() const { return b_crashed; }
    //before the first input: the craft hovers, the cave stands still and the hint shows
    bool isReady() const { return b_ready; }
    //a wall or pillar covers this screen pixel (outside the screen above and below counts as wall)
    bool isSolid(int16_t screenX, int16_t screenY) const;
    //the cave's open rows at a screen column, ceiling and floor only: first free row and first floor row
    int16_t ceilingAt(int16_t screenX) const;
    int16_t floorAt(int16_t screenX) const;
    //the thrust the input asks for, 0..1
    static float thrustFrom(const GameInput& input);
    //a stick pushed right: fire
    static bool firesFrom(const GameInput& input);
    //a bat covers this screen pixel (what touches: its body, the wing tips are forgiven)
    bool isBat(int16_t screenX, int16_t screenY) const;
    uint8_t getBatCount() const;
    uint8_t getShotCount() const;
    uint16_t getBonus() const { return bonusMetres; }
    //a bat's screen column and its band of rows (top, bottom) it bobs in; false if there is no such bat (tests)
    bool getBat(uint8_t index, int16_t& screenX, int16_t& bandTop, int16_t& bandBottom) const;
    //puts a bat at a screen column and row, the cave being as it is (tests)
    void placeBat(int16_t screenX, int16_t centreY);
    //the rotor's chops a second at this thrust (0..1)
    static float rotorRate(float thrust);
    //the one-shot sounds (tests compare what was played with these)
    const SoundCue& pingSound() const { return pingCue; }
    const SoundCue& crashSound() const { return crashCue; }
    const SoundCue& shotSound() const { return shotCue; }
    const SoundCue& hitSound() const { return hitCue; }

private:
    static const uint16_t ringSize = 192;           //cave columns kept: the screen plus the generator's lead
    static const uint8_t maxPillars = 6;
    static const uint8_t debrisCount = 10;
    static const uint8_t popupTicks = 35;

    enum class PillarKind : uint8_t{ FREE, FROM_CEILING, FROM_FLOOR };
    struct Pillar{
        bool b_used;
        PillarKind kind;
        uint32_t column;    //world column of its left edge
        int8_t top;         //first lit row
        int8_t bottom;      //last lit row
    };
    struct Debris{
        float x, y, vx, vy;
        bool b_big;
    };
    struct Bat{
        bool b_used;
        uint32_t column;    //world column of its left edge
        int8_t centreY;     //the middle of its band
        uint8_t phase;      //its bob's start, ticks
    };
    struct Shot{
        bool b_used;
        float x;            //screen column of its tip
        int16_t y;
    };
    struct Popup{
        uint8_t ticks;      //left to show
        int16_t x, y;
        uint16_t metres;
    };

    void generateUpTo(uint32_t column);
    void generateColumn(uint32_t column);
    void spawnPillar(uint32_t column, int16_t top, int16_t bottom);
    bool hitsSomething() const;
    void crash();
    uint32_t firstColumn() const { return uint32_t(scrollPx); }
    bool pillarCovers(const Pillar& p, uint32_t column, int16_t row) const;
    //the rotor's chop for this tick (sound only, never the game's state)
    void playRotor(float thrust);
    void spawnBat(uint32_t column);
    int16_t batTop(const Bat& b) const;     //its body's top row now
    void moveShots();
    void scored(uint16_t metres, int16_t x, int16_t y);

    GameRandom random;
    GameRandom demoRandom;      //the demo player's mistakes: the cave is the same whoever plays

    int8_t ceiling[ringSize];   //first free row below the ceiling
    int8_t floorRow[ringSize];  //first row of the floor
    Pillar pillars[maxPillars];
    uint32_t generated = 0;     //columns 0..generated-1 exist
    float caveCentre = 16.0f;
    float caveSlope = 0.0f;
    float caveSlopeTarget = 0.0f;
    int16_t segmentLeft = 0;
    float ceilingJitter = 0.0f;
    float floorJitter = 0.0f;
    int16_t pillarCooldown = 0;      //columns until the next chamber with a pillar
    int16_t chamberLeft = 0;

    float scrollPx = 0.0f;
    float craftY = 14.0f;       //top row of the craft, float
    float craftSpeedY = 0.0f;   //px/s, positive down
    uint32_t ticks = 0;         //ticks flown since the ready phase ended
    uint32_t frameTicks = 0;    //every tick, for the rotor and the bob
    bool b_ready = true;
    bool b_thrusting = false;

    bool b_crashed = false;
    uint8_t crashTicks = 0;
    float crashX = 0.0f, crashY = 0.0f;
    Debris debris[debrisCount];

    GameRandom batRandom;       //the bats only: the cave is the same with them
    Bat bats[maxBats];
    int16_t batCooldown = 0;    //columns until a bat may come
    Shot shots[maxShots];
    uint8_t shotCooldown = 0;
    uint16_t bonusMetres = 0;
    Popup popup = {0, 0, 0, 0};

    //the demo player: its decisions reach the stick a few ticks late, and its aim wanders more as the cave speeds up
    uint32_t demoQueue = 0;
    float demoAimError = 0.0f;

    //sound: never read by the game
    float rotorPhase = 0.0f;        //chops due, a chop at each whole one
    uint32_t pingsPlayed = 0;       //hundreds of metres pinged
    SoundKit::Grain rotorGrain;
    SoundKit::NoteList<4> pingNotes;
    SoundKit::NoteList<24> crashNotes;
    SoundKit::NoteList<8> shotNotes;
    SoundKit::NoteList<12> hitNotes;
    SoundCue pingCue, crashCue, shotCue, hitCue;
};
