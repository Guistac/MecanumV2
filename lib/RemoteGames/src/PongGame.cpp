#include "PongGame.h"
#include <math.h>
#include <stdio.h>

constexpr float PongGame::fullTravelStick;

namespace{
    const float serveSpeed = 75.0f;         //px/s, a crossing in about 1.6 s
    const float speedStep = 8.0f;           //px/s more with each hit
    const float topSpeed = 160.0f;          //3.2 px per tick, the crossing test below never skips a paddle
    const float maxBounceAngle = 0.87f;     //50 degrees at the paddle's tip
    const float computerSpeed = 50.0f;      //px/s, fast angled balls get past it
    //a human paddle follows its stick's place with a short first order lag: within half a pixel of a full sweep in
    //5 ticks (100 ms), not laggy, but the stick's jitter is smoothed and the last fraction of a pixel ignored
    const float followShare = 0.55f;        //of the remaining distance per tick, a time constant of 25 ms
    const float followHold = 0.2f;          //px, closer than this the paddle stays (no shaking between two pixels)
    const float demoThumbSpeed = 150.0f;    //px/s, how fast the demo's thumb moves the stick's place
    const uint16_t servePauseTicks = 50;
    const uint16_t serveHiddenTicks = 15;   //the ball appears in the middle a little after the point
    const float courtBottom = float(Game::height - PongGame::ballSize);
    const float paddleBottom = float(Game::height - PongGame::paddleHeight);
    const float leftFace = float(PongGame::paddleInset + PongGame::paddleWidth);
    const float rightFace = float(Game::width - PongGame::paddleInset - PongGame::paddleWidth);
    const int16_t scoreY = 1;

    float clampf(float value, float lowest, float highest){
        return value < lowest ? lowest : (value > highest ? highest : value);
    }
}

//screen y grows downward, the stick's y upward
float PongGame::stickPaddleTop(float stickY){
    float share = clampf(stickY / fullTravelStick, -1.0f, 1.0f);
    return paddleBottom / 2.0f * (1.0f - share);
}

void PongGame::followStick(uint8_t side, float stickY){
    float target = stickPaddleTop(stickY);
    float error = target - paddleTop[side];
    if(fabsf(error) < followHold){
        //at full travel it settles against the wall (the target cannot jitter there)
        if(target <= 0.0f || target >= paddleBottom) paddleTop[side] = target;
        return;
    }
    paddleTop[side] = clampf(paddleTop[side] + error * followShare, 0.0f, paddleBottom);
}

void PongGame::reset(uint32_t seed){
    random.seed(seed);
    points[0] = points[1] = 0;
    paddleTop[0] = paddleTop[1] = paddleBottom / 2.0f;
    b_twoPlayers = false;
    b_over = false;
    for(uint8_t side = 0; side < 2; side++){
        aimOffset[side] = 0;
        aimTarget[side] = paddleBottom / 2.0f;
        lookTicks[side] = 0;
        lastBallDirection[side] = 0;
    }
    demoHand = paddleBottom / 2.0f;
    //the first serve goes to the player
    serveDirection = -1;
    serve();
}

void PongGame::serve(){
    serveTicks = servePauseTicks;
    ballX = float(width - ballSize) / 2.0f;
    ballY = float(random.between(6, 24));
    ballSpeed = serveSpeed;
    float angle = (0.15f + 0.4f * random.unit()) * (random.below(2) ? 1.0f : -1.0f);
    ballVelocityX = ballSpeed * cosf(angle) * float(serveDirection);
    ballVelocityY = ballSpeed * sinf(angle);
}

uint32_t PongGame::score() const {
    return uint32_t(points[0]) * 8u + uint32_t(winningPoints - (points[1] > winningPoints ? winningPoints : points[1]));
}

void PongGame::scoreText(char* out, size_t size, uint32_t value) const {
    snprintf(out, size, "%lu-%lu", (unsigned long)(value / 8u), (unsigned long)(winningPoints - value % 8u));
}

//where the ball's centre will be when its left edge reaches atBallX, folding the bounces off the top and bottom
float PongGame::predictBallCentreY(float atBallX) const {
    if(ballVelocityX == 0.0f) return ballY + ballSize / 2.0f;
    float t = (atBallX - ballX) / ballVelocityX;
    if(t < 0.0f) t = 0.0f;
    float y = ballY + ballVelocityY * t;
    float period = 2.0f * courtBottom;
    y = fmodf(y, period);
    if(y < 0.0f) y += period;
    if(y > courtBottom) y = period - y;
    return y + ballSize / 2.0f;
}

//A player who can be beaten: it looks at the ball only every few ticks (reaction), only once the ball has come far
//enough (reactFromX, the distance from its own paddle) and moves at a limited speed. For each ball coming it picks
//where on its paddle to take it, for the angle; with a chance that grows with the ball's speed (missChance at the
//serve's speed, twice that more at the top speed) it misjudges and is just off the end of the paddle
float PongGame::autoTarget(uint8_t side, float missChance, float reactFromX){
    int8_t direction = ballVelocityX < 0.0f ? -1 : 1;
    bool b_coming = serveTicks == 0 && direction == (side == 0 ? -1 : 1);
    //a new ball toward this side: a new aim
    if(b_coming && lastBallDirection[side] != direction){
        float hurry = (ballSpeed - serveSpeed) / (topSpeed - serveSpeed);
        bool b_miss = random.unit() < missChance * (1.0f + 2.0f * hurry);
        float offset = b_miss ? 5.5f + 2.5f * random.unit() : 4.5f * random.unit();
        //a miss stops short of the ball (a paddle sweeping across the ball would still take it)
        float faceBallX = side == 0 ? leftFace : rightFace - ballSize;
        bool b_below = predictBallCentreY(faceBallX) > paddleTop[side] + paddleHeight / 2.0f;
        if(b_miss) aimOffset[side] = b_below ? offset : -offset;
        else aimOffset[side] = random.below(2) ? offset : -offset;
        lookTicks[side] = uint8_t(random.between(4, 12));
    }
    lastBallDirection[side] = serveTicks > 0 ? 0 : direction;

    float distance = side == 0 ? ballX - leftFace : rightFace - (ballX + ballSize);
    if(!b_coming) aimTarget[side] = paddleBottom / 2.0f;      //back to the middle while the other side plays
    else if(distance < reactFromX){
        if(lookTicks[side] > 0) lookTicks[side]--;
        else{
            float faceBallX = side == 0 ? leftFace : rightFace - ballSize;
            float centre = predictBallCentreY(faceBallX);
            aimTarget[side] = centre - paddleHeight / 2.0f - aimOffset[side];
            lookTicks[side] = uint8_t(random.between(6, 10));
        }
    }
    return clampf(aimTarget[side], 0.0f, paddleBottom);
}

void PongGame::hitBall(uint8_t side){
    float offset = (ballY + ballSize / 2.0f) - (paddleTop[side] + paddleHeight / 2.0f);
    float relative = clampf(offset / 5.0f, -1.0f, 1.0f);
    float angle = relative * maxBounceAngle;
    ballSpeed = ballSpeed + speedStep > topSpeed ? topSpeed : ballSpeed + speedStep;
    ballVelocityX = ballSpeed * cosf(angle) * (side == 0 ? 1.0f : -1.0f);
    ballVelocityY = ballSpeed * sinf(angle);
}

void PongGame::scorePoint(uint8_t side){
    if(points[side] < winningPoints) points[side]++;
    if(points[side] >= winningPoints){
        b_over = true;
        return;
    }
    //served toward the side that lost the point
    serveDirection = side == 0 ? 1 : -1;
    serve();
}

void PongGame::update(const GameInput& input){
    if(b_over) return;
    if(fabsf(input.stick[3]) > 0.5f) b_twoPlayers = true;

    //the humans place their paddles, the computer moves its own at a limited speed
    followStick(0, input.stick[1]);
    if(b_twoPlayers) followStick(1, input.stick[3]);
    else{
        float error = autoTarget(1, 0.2f, 60.0f) - paddleTop[1];
        if(fabsf(error) >= 0.5f){
            float velocity = clampf(error * 9.0f, -computerSpeed, computerSpeed);
            paddleTop[1] = clampf(paddleTop[1] + velocity * tickSeconds, 0.0f, paddleBottom);
        }
    }

    if(serveTicks > 0){
        serveTicks--;
        return;
    }

    float previousX = ballX;
    ballX += ballVelocityX * tickSeconds;
    ballY += ballVelocityY * tickSeconds;
    if(ballY < 0.0f){
        ballY = -ballY;
        ballVelocityY = -ballVelocityY;
    }
    else if(ballY > courtBottom){
        ballY = 2.0f * courtBottom - ballY;
        ballVelocityY = -ballVelocityY;
    }

    //a paddle's face crossed during this tick: the ball's height at the crossing decides (no skipping at speed)
    for(uint8_t side = 0; side < 2; side++){
        bool b_crossed = side == 0 ? (ballVelocityX < 0.0f && previousX >= leftFace && ballX < leftFace)
                                   : (ballVelocityX > 0.0f && previousX + ballSize <= rightFace && ballX + ballSize > rightFace);
        if(!b_crossed) continue;
        float faceX = side == 0 ? leftFace : rightFace - ballSize;
        float fraction = (faceX - previousX) / (ballX - previousX);
        float crossingY = clampf(ballY - ballVelocityY * tickSeconds * (1.0f - fraction), 0.0f, courtBottom);
        if(crossingY + ballSize > paddleTop[side] && crossingY < paddleTop[side] + paddleHeight){
            ballX = faceX;
            ballY = crossingY;
            hitBall(side);
        }
    }

    if(ballX + ballSize < 0.0f) scorePoint(1);
    else if(ballX > float(width)) scorePoint(0);
}

void PongGame::draw(Adafruit_GFX& out) const {
    //the centre line, dashed, behind everything
    for(int16_t y = 0; y < height; y += 4) out.drawFastVLine(width / 2 - 1, y + 1, 2, GameDraw::ON);

    out.fillRect(paddleInset, int16_t(lroundf(paddleTop[0])), paddleWidth, paddleHeight, GameDraw::ON);
    out.fillRect(width - paddleInset - paddleWidth, int16_t(lroundf(paddleTop[1])), paddleWidth, paddleHeight, GameDraw::ON);
    if(serveTicks < servePauseTicks - serveHiddenTicks){
        out.fillRect(int16_t(lroundf(ballX)), int16_t(lroundf(ballY)), ballSize, ballSize, GameDraw::ON);
    }

    //the scores on top, with a dark margin: the ball passes behind them and they stay readable
    char text[4];
    snprintf(text, sizeof(text), "%u", points[0]);
    int16_t leftRight = width / 2 - 7;
    int16_t w = GameDraw::textWidth(text);
    out.fillRect(leftRight - w, scoreY - 1, w + 2, 9, GameDraw::OFF);
    GameDraw::textRight(out, leftRight, scoreY, text);
    snprintf(text, sizeof(text), "%u", points[1]);
    int16_t rightLeft = width / 2 + 6;
    w = GameDraw::textWidth(text);
    out.fillRect(rightLeft - 1, scoreY - 1, w + 2, 9, GameDraw::OFF);
    GameDraw::text(out, rightLeft, scoreY, text);
}

//A decent human on the left stick: the same built-in player as the computer, steadier, through the stick and the
//same position control as a player (its thumb moves the stick at a limited speed, it can only do what a thumb can)
void PongGame::demo(GameInput& input){
    float target = autoTarget(0, 0.17f, 80.0f);
    float step = demoThumbSpeed * tickSeconds;
    demoHand = clampf(target, demoHand - step, demoHand + step);
    //the stick's place for that paddle top, the inverse of stickPaddleTop
    float centre = paddleBottom / 2.0f;
    input.stick[1] = clampf((centre - demoHand) / centre * fullTravelStick, -1.0f, 1.0f);
}
