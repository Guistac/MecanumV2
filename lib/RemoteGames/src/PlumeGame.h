#pragma once
#include "Game.h"
#include <SoundKit.h>

//PLUME: a small fire extinguisher canister drifting up an endless space shaft on its own CO2 plume
//(docs/plume/README.md). The remote is held sideways: up in the game is screen right, gravity pulls screen left.
//Either stick aims the plume (the one pushed further, never both added): the spray goes where the stick points,
//the canister is pushed the other way, and the plume blasts away the rock it hits. Climb as high as you can: the score is a point per metre of the highest altitude
//and 25 per gem. Gems and hearts (a hull point back) float in the shaft or sit buried in rock, cut out dark; the
//plume never destroys them, the canister collects them by touching them. Rock and
//the long screen edges cost a hull point (four, one second of grace after each, one back every 50 m climbed without
//a hit); the game is over when the hull is gone or the canister falls out of the bottom of the screen. The Arcade
//owns the title, pause and game over screens; Plume draws only play [D-157].
class PlumeGame : public Game{
public:
    PlumeGame();

    const char* name() const override { return "PLUME"; }
    void reset(uint32_t seed) override;
    void update(const GameInput& input) override;
    void draw(Adafruit_GFX& out) const override;
    bool isOver() const override { return b_fellOut || (hull == 0 && burstTicks >= burstShowTicks); }
    //the points: one per metre of the highest altitude, 25 per gem (the Arcade's top 3 ranks by these)
    uint32_t score() const override { return altitude() + gemPoints * gems; }
    //the highest altitude reached, in metres from the start
    uint32_t altitude() const { return highestX > startX ? uint32_t((highestX - startX) / pixelsPerMetre) : 0; }
    uint16_t gemsCollected() const { return gems; }
    //the game over screen: PTS 1450 412M (the points and the altitude)
    void overText(char* out, size_t size, uint32_t value) const override;
    void demo(GameInput& input) override;
    void helpLines(const char* lines[3]) const override {
        lines[0] = "STICK AIMS THE PLUME";
        lines[1] = "THE PLUME BLASTS ROCK";
        lines[2] = "CLIMB, DO NOT FALL";
    }
    void playMusic() override;

    //per second values from the design, stepped at the Arcade's 50 Hz (tickSeconds)
    static constexpr float pixelsPerMetre = 10.0f;
    static constexpr float gravity = 40.0f;         //px/s2 towards screen left (down in the game)
    static constexpr float fullThrust = 88.0f;      //px/s2 at full stick: 2.2 times gravity, about half stick hovers
    static constexpr float drag = 0.6f;             //v -= v * drag * dt
    static constexpr float maxSpeed = 90.0f;        //px/s
    static constexpr float deadzone = 0.12f;
    static constexpr float bounce = 0.4f;           //the speed into the rock comes back this much, reversed
    static constexpr float emissionRate = 120.0f;   //particles/s at full stick
    static constexpr float plumeSpeed = 70.0f;      //px/s along the plume, on top of the canister's speed
    static constexpr float plumeSpreadDegrees = 12.0f;
    static constexpr float particleLife = 0.3f;     //s
    static constexpr float startX = 12.0f;          //the canister's start: world column (screen x at the start)
    static constexpr float startY = 14.0f;          //and its top row, in the middle
    static const int16_t cameraLead = 51;           //the canister at most 40 % of the screen from the bottom edge
    static const int16_t shipWidth = 5;             //along the shaft (screen x)
    static const int16_t shipHeight = 3;
    static const uint8_t maxHull = 4;
    static const uint8_t graceTicks = 50;           //1 s without damage after a hit, the canister blinks
    static const uint8_t pipBlinkTicks = 40;        //a lost pip blinks twice before it goes
    static const uint8_t burstShowTicks = 25;       //half a second of the canister bursting before the game is over
    static const uint16_t repairPx = 500;           //50 m climbed without a hit gives a hull point back
    static const uint16_t calmPx = 200;             //the first 20 m are empty
    static const uint16_t firstPlugPx = 500;        //the first plug at 50 m, to learn digging early
    static const uint8_t particleCount = 48;
    static const uint16_t ringSize = 256;           //terrain columns kept: the screen plus the generator's lead
    static const int16_t generateAhead = 128 + 64;

    //the collectibles (docs/plume/README.md, Collectibles): gems for points, hearts for the hull
    enum class ItemKind : uint8_t{ GEM, HEART };
    struct Item{
        int32_t x;          //world column of the item's lower edge (screen left)
        int8_t y;           //screen row of its top
        ItemKind kind;
        uint8_t depth;      //rock around it when buried (0: placed in open space, its 1 px ring cleared)
        bool b_used;        //the slot holds an item
        bool b_taken;       //collected
        bool b_buried;      //rock still touches it: drawn as a dark cut-out in the rock
    };
    static const uint8_t maxItems = 6;
    static const uint8_t gemPoints = 25;
    static const int16_t gemSize = 3;               //a 3 x 3 diamond
    static const int16_t heartSize = 5;             //a 5 x 5 heart
    static const uint16_t gemSpacingMinPx = 300;    //a gem every 30 to 60 m
    static const uint16_t gemSpacingMaxPx = 600;
    static const uint16_t heartSpacingMinPx = 1200; //a heart about every 150 m
    static const uint16_t heartSpacingMaxPx = 1800;
    //the HUD's sideways text: the screen columns of the letters' tops (the pips at columns 125 and 126)
    static const int16_t hudAltitudeX = 123;
    static const int16_t hudPointsX = 115;

    //for the tests and the demo: the state read from outside
    float getShipX() const { return shipX; }        //world column of the canister's lower edge (game down)
    float getShipY() const { return shipY; }        //screen row of its top
    float getSpeedX() const { return speedX; }
    float getSpeedY() const { return speedY; }
    float getCameraX() const { return cameraX; }
    uint8_t getHull() const { return hull; }
    bool isInvulnerable() const { return graceLeft > 0; }
    bool hasFallenOut() const { return b_fellOut; }
    bool isBursting() const { return hull == 0; }
    //difficulty 0..1: min(altitude / 500 m, 1)
    float difficulty() const;
    //rock at a world column and row (rows outside 0..31 and columns not kept read as empty)
    bool isRock(int32_t column, int16_t row) const;
    //tests: rock set or cleared by hand (only in columns already generated)
    void setRock(int32_t column, int16_t row, bool b_rock);
    //tests: the canister placed and stopped
    void place(float x, float y, float vx = 0.0f, float vy = 0.0f);
    //particles alive now
    uint8_t liveParticles() const;
    //the item slots (tests, the demo)
    const Item& item(uint8_t index) const { return items[index]; }
    //an item pixel at a world column and row (an item not yet collected)
    bool isItemPixel(int32_t column, int16_t row) const;
    //tests: an item placed by hand in columns already generated (in open space, its rock left as it is)
    void placeItem(ItemKind kind, int32_t x, int8_t y);
    //the thrust the input asks for: direction (x screen right, y screen down) and 0..1; the stick aims the plume,
    //so the thrust points the opposite way
    static float thrustFrom(const GameInput& input, float& directionX, float& directionY);

private:
    struct Particle{
        float x, y, vx, vy;
        uint8_t life;       //ticks left, 0: free
    };
    struct Plug{
        int32_t start;      //first world column
        uint8_t thickness;
        int8_t lower[height];   //per row: the plug's edges move by these, so they are rough
        int8_t upper[height];
    };
    struct Box{
        int32_t start;
        uint8_t length;     //columns
        int8_t top;
        int8_t rows;
    };
    static const uint8_t maxBoxes = 4;

    void generateUpTo(int32_t column);
    void generateColumn(int32_t column);
    float noise(float x, float y) const;
    bool shipTouches(int16_t ix, int16_t iy, float& normalX, float& normalY) const;
    void newItem(int32_t column, ItemKind kind);
    void stampItems(int32_t column, uint32_t& bits) const;
    static int16_t itemSize(ItemKind kind){ return kind == ItemKind::GEM ? gemSize : heartSize; }
    static bool itemShape(ItemKind kind, int16_t col, int16_t row);
    bool itemFree(const Item& it) const;
    void collectItems();
    void drawHud(Adafruit_GFX& out) const;
    static bool litForHud(const void* game, int16_t x, int16_t y);
    void collide();
    void emit(float directionX, float directionY, float thrust);
    void moveParticles();
    void hullHit();
    void burst();
    bool shipPixel(int16_t screenX, int16_t y) const;
    bool litBeforePips(int16_t screenX, int16_t y) const;
    int32_t cameraColumn() const { return int32_t(floorf(cameraX)); }
    void sounds(float thrust);
    void updateTempo();

    GameRandom terrainRandom;   //the shaft: the same for a seed whatever the player does
    GameRandom plumeRandom;     //the plume's spread
    GameRandom demoRandom;      //the demo player's lapses
    GameRandom itemRandom;      //where the gems and hearts go (part of the shaft: the same for a seed)
    uint32_t noiseSeed = 0;

    uint32_t columns[ringSize];
    int32_t generated = 0;      //columns 0..generated-1 exist
    Plug plug = {};
    int32_t nextPlug = 0;
    bool b_plugActive = false;
    Box boxes[maxBoxes];
    Item items[maxItems];
    int32_t nextGem = 0;
    int32_t nextHeart = 0;
    uint16_t gems = 0;

    float shipX = 0.0f, shipY = 0.0f;
    float speedX = 0.0f, speedY = 0.0f;
    float cameraX = 0.0f;
    float highestX = 0.0f;
    float repairMark = 0.0f;
    uint8_t hull = maxHull;
    uint8_t graceLeft = 0;
    uint8_t pipBlink = 0;       //ticks the lost pip still blinks
    uint8_t burstTicks = 0;
    bool b_fellOut = false;
    uint32_t frameTicks = 0;

    Particle particles[particleCount];
    float emitDebt = 0.0f;
    uint8_t erodedThisTick = 0;
    uint8_t crackleWait = 0;    //ticks until the next crackle may sound

    //the demo player
    enum class DemoMode : uint8_t{ CLIMB, DIG };
    DemoMode demoMode = DemoMode::CLIMB;
    uint8_t demoDigTicks = 0;
    uint16_t demoLapse = 0;     //ticks the hand still rests (how it falls now and then)
    float demoRow = 14.0f;
    int8_t demoItem = -1;       //the item it goes for, -1: none
    int8_t demoPassed = -1;     //the last item it decided to leave

    //sound [D-157]: its own random numbers and notes; never game state
    SoundKit::NoteList<64> tune;
    SoundKit::NoteList<4> clunkNotes;
    SoundKit::NoteList<4> repairNotes;
    SoundKit::NoteList<4> pingNotes;
    SoundKit::NoteList<26> whistleNotes;
    SoundKit::NoteList<4> burstNotes;
    SoundKit::NoteList<4> gemNotes;
    SoundKit::NoteList<6> heartNotes;
    SoundKit::NoteList<2> clickNotes;
    SoundKit::Grain hiss;
    uint8_t effectTicks = 0;    //the game's own one-shot effect still sounds: no grain cuts it
    uint32_t lastPingMetres = 0;
    uint16_t tempo = 100;
    void effect(const SoundCue& cue, uint8_t ticks);
};
