#pragma once
#include "Game.h"

//PARKING: drive the Mecanum stage vehicle (top view) into the bay of each of twelve stages, nose to the marked side,
//as fast as possible; from stage 5 on, flats, wagons and a turntable move. Controls as on the real handheld in body
//frame: left stick Y forward and back, left stick X strafes, right stick X turns (right = clockwise on the screen).
//The vehicle slides along walls; only an impact (driving in fast) costs 2 s.

//Twelve hand-made stages driven against the clock, holonomic vehicle with ramped speeds like the real one [D-148]
class ParkingGame : public Game{
public:
    const char* name() const override { return "PARKING"; }
    void reset(uint32_t seed) override;
    void update(const GameInput& input) override;
    void draw(Adafruit_GFX& out) const override;
    bool isOver() const override { return phase == Phase::OVER; }
    //total time in tenths of a second, penalties included; a stage left unparked at the time limit adds 60 s
    uint32_t score() const override;
    bool lowerScoreIsBetter() const override { return true; }
    void scoreText(char* out, size_t size, uint32_t value) const override;
    void demo(GameInput& input) override;

    enum class Phase : uint8_t { INTRO, DRIVE, PARKED, OVER };

    static const uint8_t levelCount = 12;
    static const uint8_t maxMovers = 2;
    static constexpr float maxSpeed = 30.0f;            //px/s
    static constexpr float maxTurnRate = 2.0944f;       //rad/s (120 deg/s)
    static constexpr float bumpSpeed = 12.0f;           //px/s into a surface (40 % of top speed): slower is a graze
    static const uint16_t introTicks = 50;
    static const uint16_t parkedHoldTicks = 50;         //1 s parked before it counts
    static const uint16_t flashTicks = 40;
    static const uint16_t penaltyTicks = 100;           //2 s per bump
    static const uint32_t timeLimitTicks = 15000;       //5 min

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

    GameRandom random;
    Phase phase = Phase::INTRO;
    uint8_t level = 0;
    uint16_t phaseTicks = 0;
    float x = 0, y = 0, heading = 0;
    float forwardSpeed = 0, strafeSpeed = 0, turnRate = 0;   //body frame, ramped
    uint32_t clockTicks = 0;
    uint16_t penaltyCount = 0;
    uint16_t penaltyShowTicks = 0;
    uint16_t freeTicks = 0;         //ticks since the last bump: one hit into a corner costs once, not once per face
    uint16_t parkedTicks = 0;
    uint8_t levelsDone = 0;
    uint16_t moverPhase[maxMovers] = {0, 0};    //ticks into each moving piece's cycle; it stands still while blocked
    Obb movers[maxMovers];
    //the demo player
    uint8_t waypoint = 0;
    bool b_demoGo = false;          //the moving piece it waited for is out of the way: go, do not stop halfway
    float demoGain = 3.0f;
};
