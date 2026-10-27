#pragma once
#include "Game.h"
#include <SoundKit.h>

//STRIKE: striking the set, a brick-breaker of our own [D-157]. The paddle is on the left edge and moves up and down
//with the left stick (the deflection is its speed: the stick springs back, the paddle stays where it is); the right
//stick click launches the ball, which rides on the paddle until then. The set stands against the right edge: set
//pieces break (1 hit), the hatched ones take 2, steel truss never breaks, a prize piece drops a power-up that drifts
//left toward the paddle: catch it for a wide paddle, three balls or a slow ball. Twelve hand-made stages, each its own
//shape of set (a proscenium, a staircase, a flown truss, wings, a revolve...), cleared when every breakable piece is
//gone; after the last one they come round again, faster (the loop). 3 balls; the score is points per piece and a bonus
//per stage struck.
//Sound [D-157]: no music (like Pong, so the pieces stay audible), timer squares as in Pong: a blip per piece whose
//pitch climbs one note per row up the set, a lower tick off the paddle, a dull thud off the truss (a rough duet of two
//close low squares), a short rising arpeggio for a power-up caught, falling notes for a ball lost, a jingle for a stage
//struck. Each under 400 ms but the jingle (no game over follows it), so the Arcade's game over phrase is never cut.
class StrikeGame : public Game{
public:
    //the field: rows 0..30 (row 31 stays dark, so the 2 px ball never fits under the set's last row or over its first)
    static const int16_t fieldHeight = 31;
    //the paddle on the left edge
    static const int16_t paddleX = 1;
    static const int16_t paddleWidth = 2;
    static const int16_t paddleHeight = 8;
    static const int16_t widePaddleHeight = 12;
    static const int16_t paddleFace = paddleX + paddleWidth;    //the ball leaves the paddle from x 3 on
    static const int16_t ballSize = 2;
    //the set: 5 rows of 10 places, pieces 4 x 5 px on a 5 x 6 pitch, against the right edge (x 127 stays dark)
    static const uint8_t rows = 5;
    static const uint8_t columns = 10;
    static const int16_t brickWidth = 4;
    static const int16_t brickHeight = 5;
    static const int16_t pitchX = 5;
    static const int16_t pitchY = 6;
    static const int16_t setLeft = 78;          //column 0's left edge
    static const int16_t setTop = 1;            //row 0's top edge; row 4 ends at 29
    static const uint8_t stageCount = 12;
    static const uint8_t maxBalls = 3;
    static const uint8_t maxPrizes = 3;
    static const uint8_t startLives = 3;

    //a place of the set. A hatched piece (DOUBLE) is CRACKED by its first hit (drawn as a plain piece, worth more); a
    //prize piece breaks in one hit and drops its power-up
    enum class Brick : uint8_t{ EMPTY, PIECE, DOUBLE, CRACKED, TRUSS, PRIZE_WIDE, PRIZE_MULTI, PRIZE_SLOW };
    enum class PowerUp : uint8_t{ WIDE, MULTI, SLOW };
    enum class Phase : uint8_t{ INTRO, SERVE, PLAY, STRUCK, OVER };

    //speeds, px/s; times in ticks
    static constexpr float paddleSpeed = 100.0f;        //at full deflection
    static constexpr float stickDeadZone = 0.12f;
    static constexpr float baseSpeed = 70.0f;           //the ball's speed on the first round of stages
    static constexpr float loopSpeedStep = 15.0f;       //more for each time round the stages
    static constexpr float brickSpeedStep = 6.0f;       //more every 10 pieces broken since the last ball lost
    static constexpr float topSpeed = 150.0f;           //3 px a tick
    static constexpr float slowShare = 0.6f;            //the slow ball's share of the speed
    static constexpr float maxBounceAngle = 1.0f;       //rad off the paddle's tip (57 degrees), flat from its centre
    static constexpr float minHorizontal = 0.5f;        //the ball's horizontal share of its speed never drops below
    static constexpr float prizeSpeed = 25.0f;          //a power-up drifting left
    static const uint16_t introTicks = 75;              //the stage's name, 1.5 s
    static const uint16_t struckTicks = 90;             //the stage struck, its bonus, 1.8 s
    static const uint16_t wideTicks = 750;              //15 s
    static const uint16_t slowTicks = 500;              //10 s
    static const uint16_t stallTicks = 400;             //8 s without touching the paddle or a breakable piece: a nudge
    //points
    static const uint16_t piecePoints = 10;
    static const uint16_t crackPoints = 5;              //a hatched piece's first hit
    static const uint16_t doublePoints = 25;            //and its break (30 in all)
    static const uint16_t prizePoints = 15;
    static const uint16_t catchPoints = 50;
    static const uint16_t stageBonus = 100;             //times the stage's number (counting the loops)

    const char* name() const override { return "STRIKE"; }
    void reset(uint32_t seed) override;
    void update(const GameInput& input) override;
    void draw(Adafruit_GFX& out) const override;
    bool isOver() const override { return phase == Phase::OVER; }
    uint32_t score() const override { return points; }
    //the points and the stage reached: 1450 STAGE 7
    void overText(char* out, size_t size, uint32_t value) const override;
    void demo(GameInput& input) override;
    void helpLines(const char* lines[3]) const override {
        lines[0] = "L stick: paddle up/dn";
        lines[1] = "R click: launch ball";
        lines[2] = "Strike all but truss";
    }

    //a stage's name, upper case, at most 10 characters
    static const char* stageName(uint8_t stage);
    //the place in a stage's table (as written, before the game breaks anything)
    static Brick stageBrick(uint8_t stage, uint8_t row, uint8_t column);
    //the screen box of a place
    static int16_t brickX(uint8_t column){ return int16_t(setLeft + column * pitchX); }
    static int16_t brickY(uint8_t row){ return int16_t(setTop + row * pitchY); }
    //how fast the paddle moves for a stick's y (-1..1, up positive): px/s, negative = up the screen
    static float paddleVelocity(float stickY);
    //the ball's speed before the slow ball: the loop's base, the steps for the pieces broken, the top speed
    static float ballSpeedFor(uint8_t loop, uint16_t brokenSinceLost);

    //for the tests
    struct Ball{ float x, y, dx, dy; bool b_live; };    //top left px, direction (unit vector)
    Phase getPhase() const { return phase; }
    uint8_t getStage() const { return stage; }          //0..stageCount-1 within the loop
    uint8_t getLoop() const { return loop; }
    uint16_t getStageNumber() const { return uint16_t(loop) * stageCount + stage + 1; }
    uint8_t getLives() const { return lives; }
    float getPaddleTop() const { return paddleTop; }
    int16_t getPaddleHeight() const { return wideLeft > 0 ? widePaddleHeight : paddleHeight; }
    uint8_t getBallCount() const;
    const Ball& getBall(uint8_t index) const { return balls[index % maxBalls]; }
    float getSpeed() const;                             //px/s now, the slow ball included
    uint16_t getBrokenSinceLost() const { return brokenSinceLost; }
    Brick getBrick(uint8_t row, uint8_t column) const { return Brick(bricks[row % rows][column % columns]); }
    uint16_t breakablesLeft() const;
    uint16_t getWideLeft() const { return wideLeft; }
    uint16_t getSlowLeft() const { return slowLeft; }
    struct Prize{ float x, y; PowerUp kind; bool b_live; };
    const Prize& getPrize(uint8_t index) const { return prizes[index % maxPrizes]; }
    uint8_t getPrizeCount() const;
    //the demo player's lapses (now and then it misjudges a ball on purpose, more often as the game goes on, so the
    //attract mode ends): on by default; the tests switch them off to clear every stage. Kept over reset()
    void setDemoLapses(bool b_on){ b_demoLapses = b_on; }
    //tests: straight to a stage's intro (the loop given), a place of the set changed, a ball placed in flight, the
    //paddle placed, a power-up dropped where given
    void startStage(uint8_t index, uint8_t loopIndex = 0);
    void setBrick(uint8_t row, uint8_t column, Brick b){ bricks[row % rows][column % columns] = uint8_t(b); }
    void clearSet();
    void placeBall(float x, float y, float dx, float dy);
    void placePaddle(float top){ paddleTop = top; }
    void dropPrize(float x, float y, PowerUp kind);
    //the one-shot sounds (tests compare what was played with these); the piece's blip and the paddle's tick are notes
    static uint16_t brickHertz(uint8_t row);
    static const uint16_t paddleHertz = 784;            //G5, under every piece's blip
    static SoundCue thudSound();
    static SoundCue catchSound();
    static SoundCue lostSound();
    static SoundCue struckSound();

private:
    void startStageNow(uint8_t index);
    void serve();
    void launch();
    void movePaddle(float stickY);
    //one ball's tick in steps of at most 1 px on each axis; false when it left the field on the left
    bool moveBall(Ball& b, float distance);
    //the places a ball box overlaps; returns how many were hit (and hits them)
    uint8_t hitBricks(float x, float y);
    void hitBrick(uint8_t row, uint8_t column);
    void bounceOffPaddle(Ball& b);
    void catchPrize(PowerUp kind);
    void ballLost();
    void stageStruck();
    void nudge(Ball& b);
    static void keepHorizontal(Ball& b);
    //the demo's guess of where a ball's top will be when it reaches the paddle's face (bounces off the top and bottom
    //folded, the set ignored: there is none left of it)
    float predictY(const Ball& b) const;

    GameRandom random;
    Phase phase = Phase::INTRO;
    uint16_t phaseTicks = 0;
    uint8_t stage = 0;
    uint8_t loop = 0;
    uint8_t lives = startLives;
    uint32_t points = 0;
    uint8_t bricks[rows][columns] = {};
    float paddleTop = 0;
    Ball balls[maxBalls] = {};
    Prize prizes[maxPrizes] = {};
    uint16_t brokenSinceLost = 0;
    uint16_t wideLeft = 0;
    uint16_t slowLeft = 0;
    uint16_t calmTicks = 0;             //since the paddle or a breakable piece was last touched
    uint32_t frame = 0;                 //drawing only: the prize pieces' twinkle, the power-ups' last seconds

    //the demo player
    GameRandom demoRandom;
    bool b_demoLapses = true;
    float demoAim = 0;                  //where on the paddle it takes the coming ball, px from its centre
    int8_t demoBall = -1;               //the ball it is playing (the next one to reach the paddle)
    bool b_demoHeadingLeft[maxBalls] = {};
    uint8_t demoLookTicks = 0;
    float demoTarget = 0;
    uint16_t demoServeTicks = 0;
    uint32_t demoPlayedTicks = 0;

    //sound: never read by the game; the piece's sounds are made per tick (one per tick at most, the most telling)
    uint8_t soundRow = 0;
    enum class Heard : uint8_t{ NONE, PADDLE, THUD, CRACK, BREAK };
    Heard heard = Heard::NONE;
};
