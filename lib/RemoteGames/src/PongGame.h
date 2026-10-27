#pragma once
#include "Game.h"

//PONG: the classic, first to 7 points. The left stick places the left paddle (centre is the middle of the court,
//pushed up or down is the top or bottom), the computer plays the right one. A second player takes the right paddle by
//pushing the right stick past half way: two people sharing one remote, for the rest of that game.

//The left player's paddle against the computer (or a second player) [D-157]. The ball bounces off the top and bottom
//edges, speeds up a little with each hit and leaves a paddle at an angle set by where it hit it. After a point it is
//served again from the middle, toward the side that lost the point.
//Sound [D-157]: no music (the classic), timer squares only: a high blip off a paddle that rises with the ball's
//speed, a lower one off the top and bottom, a longer low tone for a point (higher when the left player scores), and
//a jingle at the end, rising when the left player wins, falling when they lose. All shorter than 400 ms, so they end
//before the Arcade's game over phrase.
class PongGame : public Game{
public:
    static const uint8_t winningPoints = 7;

    //the court: paddles 2 x 8 px, 2 px in from the edges, a 2 x 2 ball
    static const int16_t paddleWidth = 2;
    static const int16_t paddleHeight = 8;
    static const int16_t paddleInset = 2;
    static const int16_t ballSize = 2;

    const char* name() const override { return "PONG"; }
    void reset(uint32_t seed) override;
    void update(const GameInput& input) override;
    void draw(Adafruit_GFX& out) const override;
    bool isOver() const override { return b_over; }
    //the left player's points first, then fewer points conceded: points * 8 + (7 - opponent's points). A lost game
    //with no point at all is 0 (never a best); scoreText shows it back as "7-4"
    uint32_t score() const override;
    void scoreText(char* out, size_t size, uint32_t value) const override;
    void demo(GameInput& input) override;
    void helpLines(const char* lines[3]) const override {
        lines[0] = "L STICK: YOUR PADDLE";
        lines[1] = "R STICK: 2ND PLAYER";
        lines[2] = "FIRST TO 7 WINS";
    }

    //where a human's stick (y, -1..1, up positive) places the paddle's top once it has followed: full travel at
    //fullTravelStick, the very ends of a thumb stick are hard to hold
    static float stickPaddleTop(float stickY);
    static constexpr float fullTravelStick = 0.85f;

    //for the tests: side 0 is the left player, 1 the right
    uint8_t getPoints(uint8_t side) const { return points[side & 1]; }
    float getPaddleTop(uint8_t side) const { return paddleTop[side & 1]; }
    float getBallX() const { return ballX; }
    float getBallY() const { return ballY; }
    float getBallVelocityX() const { return ballVelocityX; }
    float getBallVelocityY() const { return ballVelocityY; }
    float getBallSpeed() const { return ballSpeed; }
    bool isServing() const { return serveTicks > 0; }
    bool isTwoPlayers() const { return b_twoPlayers; }
    //the sounds' pitches and the end jingles, for the tests
    static uint16_t paddleHertz(float ballSpeed);
    static const uint16_t wallHertz = 494;
    static const uint16_t pointWonHertz = 294;
    static const uint16_t pointLostHertz = 220;
    static SoundCue winJingle();
    static SoundCue loseJingle();

private:
    //the built-in player of one side (the computer on the right, the demo on the left): the paddle top it wants
    float autoTarget(uint8_t side, float missChance, float reactFromX);
    //a human paddle: the stick's height places it, followed quickly
    void followStick(uint8_t side, float stickY);
    float predictBallCentreY(float atBallX) const;
    void hitBall(uint8_t side);
    void scorePoint(uint8_t side);
    void serve();

    GameRandom random;

    float ballX = 0, ballY = 0;             //top left of the ball, px
    float ballVelocityX = 0, ballVelocityY = 0;
    float ballSpeed = 0;
    float paddleTop[2] = {0, 0};
    uint8_t points[2] = {0, 0};
    uint16_t serveTicks = 0;                //the pause before a serve, the ball waits in the middle
    int8_t serveDirection = -1;             //-1 toward the left player
    bool b_twoPlayers = false;
    bool b_over = false;

    //the built-in players, per side: where they aim the ball on their paddle, when they next look at the ball again
    float aimOffset[2] = {0, 0};
    float aimTarget[2] = {0, 0};
    uint8_t lookTicks[2] = {0, 0};
    int8_t lastBallDirection[2] = {0, 0};
    float demoHand = 0;                     //the demo's thumb: the paddle top it holds the stick at, moved at a thumb's speed
};
