#pragma once
#include "Game.h"
#include <SoundKit.h>

//PARKING: drive the Mecanum stage vehicle (top view) into the bay of each of 22 stages, nose to the marked side, as
//fast as possible; from stage 5 on, flats, wagons and turntables move; stages 13 to 22 force the Mecanum moves: crab
//along corridors too low to turn in, turn on the spot between slots, strafe sideways into a slot, slanted bays and
//timed machinery. Controls as on the real handheld in body frame: left stick Y forward and back, left stick X
//strafes, right stick X turns (right = clockwise on the screen).
//The vehicle slides along walls; only an impact (driving in fast) costs 2 s and damages the vehicle by the impact's
//speed (the meter under the time; grazes stay free). Damage carries over from stage to stage, each parked stage
//repairs a little; at 100 % the vehicle is wrecked and the run ends.
//Sound [D-157]: no music; the motor whines (grains, pitch with the speed, none when still), a wall scrapes while the
//vehicle slides along it, an impact crunches, a parked stage chimes, the last one plays a fanfare, a wreck crashes.

//22 hand-made stages driven against the clock, holonomic vehicle with ramped speeds like the real one [D-157]
class ParkingGame : public Game{
public:
    ParkingGame();
    const char* name() const override { return "PARKING"; }
    void reset(uint32_t seed) override;
    void update(const GameInput& input) override;
    void draw(Adafruit_GFX& out) const override;
    bool isOver() const override { return phase == Phase::OVER; }
    //total time in tenths of a second, penalties included; a stage left unparked at the time limit adds 60 s
    uint32_t score() const override;
    bool lowerScoreIsBetter() const override { return true; }
    void scoreText(char* out, size_t size, uint32_t value) const override;
    //WRECKED and the score when the vehicle was wrecked
    void overText(char* out, size_t size, uint32_t value) const override;
    void demo(GameInput& input) override;
    void helpLines(const char* lines[3]) const override {
        lines[0] = "L stick: drive/strafe";
        lines[1] = "R stick x: turn";
        lines[2] = "Hit: +2 s and damage";
    }

    enum class Phase : uint8_t { INTRO, DRIVE, PARKED, WRECKED, OVER };

    static const uint8_t levelCount = 22;
    static const uint8_t maxPieces = 10;
    static const uint8_t maxMovers = 3;
    static const uint8_t maxWaypoints = 14;
    static constexpr float maxSpeed = 30.0f;            //px/s
    static constexpr float maxTurnRate = 2.0944f;       //rad/s (120 deg/s)
    static constexpr float bumpSpeed = 12.0f;           //px/s into a surface (40 % of top speed): slower is a graze
    static const uint16_t introTicks = 50;
    static const uint16_t parkedHoldTicks = 50;         //1 s parked before it counts
    static const uint16_t flashTicks = 40;
    static const uint16_t penaltyTicks = 100;           //2 s per bump
    static const uint32_t timeLimitTicks = 30000;       //10 min
    //damage [owner 2026-10-27]: a bump (impact over bumpSpeed) adds minDamage at the bump speed up to fullDamage at
    //top speed, at most maxDamage (a diagonal crash is faster than top speed); a parked stage repairs repairDamage
    static const uint8_t minDamage = 6;
    static const uint8_t fullDamage = 32;
    static const uint8_t maxDamage = 40;
    static const uint8_t repairDamage = 5;
    static const uint16_t wreckTicks = 90;              //the wreck shown 1.8 s, then the game is over
    //percent of damage an impact at this speed (px/s into the surface) does: 0 for a graze
    static uint8_t damageFor(float impactSpeed);

    //an oriented rectangle: centre, half sizes, direction of its x half size
    struct Obb{ float cx, cy, hx, hy, c, s; };

    //for the tests
    Phase getPhase() const { return phase; }
    uint8_t getLevel() const { return level; }
    float getX() const { return x; }
    float getY() const { return y; }
    float getHeading() const { return heading; }        //rad, 0 = facing right, positive = clockwise on screen
    uint32_t getClockTicks() const { return clockTicks + penaltyCount * uint32_t(penaltyTicks); }
    uint16_t getPenaltyCount() const { return penaltyCount; }
    uint8_t getDamage() const { return damage; }        //percent, 100 = wrecked
    bool isWrecked() const { return b_wrecked; }
    uint8_t getLevelsDone() const { return levelsDone; }
    //the vehicle's damage set (tests): 0..99
    void setDamage(uint8_t percent){ damage = percent > 99 ? 99 : percent; }
    //the demo player's lapses (a charge at full stick now and then, so the attract run ends in a wreck): on by
    //default; the tests switch them off to drive every stage cleanly (each stage's solution). Kept over reset()
    void setDemoLapses(bool b_on){ b_demoLapses = b_on; }
    uint8_t getMoverCount() const;
    //a moving piece's shape now, without the contact margin
    Obb getMover(uint8_t index) const { return movers[index]; }
    bool isParkedNow() const;
    //true when the vehicle overlaps a piece, a moving piece, the clock's corner or leaves the stage (never in play)
    bool overlapsNow() const;
    //places the vehicle (tests): stopped, at the pose given
    void place(float px, float py, float headingRad);
    //jumps to a stage's intro (tests)
    void startStage(uint8_t index){ startLevel(index); }
    //the one-shot sounds (tests compare what was played with these)
    const SoundCue& impactSound() const { return impactCue; }
    const SoundCue& chimeSound() const { return chimeCue; }
    const SoundCue& fanfareSound() const { return fanfareCue; }
    const SoundCue& wreckSound() const { return wreckCue; }

private:
    void startLevel(uint8_t index);
    Obb vehicleAt(float px, float py, float headingRad) const;
    Obb moverAt(uint8_t index, uint16_t moverPhase) const;
    uint16_t moverCycle(uint8_t index) const;
    //the deepest contact of the vehicle shape with anything: depth > 0 and the normal pointing out of the obstacle
    bool deepestContact(const Obb& vehicle, float& depth, float& nx, float& ny) const;
    void moveMovers();
    void moveVehicle(const GameInput& input);
    bool gateOpen(uint8_t waypointIndex) const;
    void corners(float px, float py, float headingRad, float* cx, float* cy) const;
    void drawVehicle(Adafruit_GFX& out, uint16_t color) const;
    void drawBay(Adafruit_GFX& out) const;
    void drawBooth(Adafruit_GFX& out) const;
    //a bump: the 2 s, the damage by its speed, the wreck at 100 %
    void bump(float impactSpeed);
    //the motor's whine or the wall's scrape for this tick (sound only, never the game's state)
    void playGrains();

    GameRandom random;
    Phase phase = Phase::INTRO;
    uint8_t level = 0;
    uint16_t phaseTicks = 0;
    float x = 0, y = 0, heading = 0;
    float forwardSpeed = 0, strafeSpeed = 0, turnRate = 0;   //body frame, ramped
    uint32_t clockTicks = 0;
    uint16_t penaltyCount = 0;
    uint16_t penaltyShowTicks = 0;
    uint8_t damage = 0;             //percent
    bool b_wrecked = false;
    uint16_t freeTicks = 0;         //ticks since the last bump: one hit into a corner costs once, not once per face
    uint16_t parkedTicks = 0;
    uint8_t levelsDone = 0;
    uint16_t moverPhase[maxMovers] = {};    //ticks into each moving piece's cycle; it stands still while blocked
    Obb movers[maxMovers];
    //the demo player
    uint8_t waypoint = 0;
    bool b_demoGo = false;          //the moving piece it waited for is out of the way: go, do not stop halfway
    float demoGain = 3.0f;
    GameRandom demoRandom;          //the demo player's lapses only: the stages are the same whoever plays
    bool b_demoLapses = true;
    uint16_t lapseTicks = 0;        //charging: ticks left
    float lapseX = 0, lapseY = 0;   //the charge's direction on the screen
    uint16_t lapsePenalties = 0;    //the bump count when the charge began: it ends at the first bump
    //stuck (turning in a slot too narrow to turn in): it turns back a little, then turns only at the point itself
    float demoLastX = 0, demoLastY = 0, demoLastHeading = 0;
    uint8_t stallTicks = 0;
    uint8_t unwindTicks = 0;
    float unwindTurn = 0;
    bool b_turnLate = false;

    //sound: never read by the game. The speed along a wall while sliding on it this tick (0: not sliding)
    float scrapeSpeed = 0.0f;
    SoundKit::Grain motorGrain;
    SoundKit::NoteList<12> impactNotes;
    SoundKit::NoteList<6> chimeNotes;
    SoundKit::NoteList<10> fanfareNotes;
    SoundKit::NoteList<40> wreckNotes;
    SoundCue impactCue, chimeCue, fanfareCue, wreckCue;
};
