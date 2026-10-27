//Host tests of the remote's STRIKE game (the brick-breaker) [D-157]: pio test -e native -f test_game_strike
#include <unity.h>
#include <HostGfx.h>
#include <GameContract.h>
#include <StrikeGame.h>
#include <Arcade.h>
#include <ArcadeScores.h>
#include <RecordingSink.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

void setUp(){}
void tearDown(){}

typedef StrikeGame::Brick Brick;
typedef StrikeGame::Phase Phase;
typedef StrikeGame::PowerUp PowerUp;

static GameInput leftStick(float y){
    GameInput input;
    input.stick[1] = y;
    return input;
}

static GameInput rightClick(){
    GameInput input;
    input.clicked[1] = true;
    return input;
}

//a game past its first stage's intro, the ball on the paddle
static void toServe(StrikeGame& game, uint32_t seed = 1){
    game.reset(seed);
    while(game.getPhase() == Phase::INTRO) game.update(GameInput());
}

//true when a live ball's box overlaps a place of the set that is not empty (never in play)
static bool ballInsideSet(const StrikeGame& game){
    for(uint8_t i = 0; i < StrikeGame::maxBalls; i++){
        const StrikeGame::Ball& b = game.getBall(i);
        if(!b.b_live) continue;
        for(uint8_t r = 0; r < StrikeGame::rows; r++){
            for(uint8_t c = 0; c < StrikeGame::columns; c++){
                if(game.getBrick(r, c) == Brick::EMPTY) continue;
                float x = StrikeGame::brickX(c), y = StrikeGame::brickY(r);
                if(b.x < x + StrikeGame::brickWidth && b.x + StrikeGame::ballSize > x &&
                   b.y < y + StrikeGame::brickHeight && b.y + StrikeGame::ballSize > y) return true;
            }
        }
    }
    return false;
}

//an empty set but for a truss wall in column 8 with a piece behind it (never reached: the stage is never struck)
static void guardedSet(StrikeGame& game){
    game.clearSet();
    for(uint8_t r = 0; r < StrikeGame::rows; r++) game.setBrick(r, 8, Brick::TRUSS);
    game.setBrick(2, 9, Brick::PIECE);
}

void test_contract(){
    GameContract::check<StrikeGame>();
}

//——— the paddle ———

void test_paddle_speed_is_the_stick_deflection(){
    //a dead zone, then the speed grows with the deflection, full speed at full deflection; up the screen for stick up
    TEST_ASSERT_EQUAL_FLOAT(0.0f, StrikeGame::paddleVelocity(0.0f));
    TEST_ASSERT_EQUAL_FLOAT(0.0f, StrikeGame::paddleVelocity(StrikeGame::stickDeadZone));
    TEST_ASSERT_EQUAL_FLOAT(0.0f, StrikeGame::paddleVelocity(-0.1f));
    TEST_ASSERT_EQUAL_FLOAT(-StrikeGame::paddleSpeed, StrikeGame::paddleVelocity(1.0f));
    TEST_ASSERT_EQUAL_FLOAT(StrikeGame::paddleSpeed, StrikeGame::paddleVelocity(-1.0f));
    float half = StrikeGame::stickDeadZone + (1.0f - StrikeGame::stickDeadZone) / 2.0f;
    TEST_ASSERT_FLOAT_WITHIN(0.01f, -StrikeGame::paddleSpeed / 2.0f, StrikeGame::paddleVelocity(half));
    TEST_ASSERT_TRUE(fabsf(StrikeGame::paddleVelocity(0.3f)) < fabsf(StrikeGame::paddleVelocity(0.6f)));

    StrikeGame game;
    toServe(game);
    float top = game.getPaddleTop();
    game.update(leftStick(1.0f));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, top - StrikeGame::paddleSpeed * Game::tickSeconds, game.getPaddleTop());
    //let go, it stays where it is (no position mapping: the stick springs back)
    top = game.getPaddleTop();
    for(int t = 0; t < 20; t++) game.update(GameInput());
    TEST_ASSERT_EQUAL_FLOAT(top, game.getPaddleTop());
}

void test_paddle_stays_on_the_field(){
    StrikeGame game;
    toServe(game);
    for(int t = 0; t < 50; t++) game.update(leftStick(1.0f));
    TEST_ASSERT_EQUAL_FLOAT(0.0f, game.getPaddleTop());
    for(int t = 0; t < 50; t++) game.update(leftStick(-1.0f));
    TEST_ASSERT_EQUAL_FLOAT(float(StrikeGame::fieldHeight - StrikeGame::paddleHeight), game.getPaddleTop());
    //wide, the same limits for its height
    game.placeBall(60, 15, 1, 0);
    game.dropPrize(2, game.getPaddleTop() + 2, PowerUp::WIDE);
    game.update(GameInput());
    TEST_ASSERT_EQUAL_INT16(StrikeGame::widePaddleHeight, game.getPaddleHeight());
    for(int t = 0; t < 50; t++) game.update(leftStick(-1.0f));
    TEST_ASSERT_EQUAL_FLOAT(float(StrikeGame::fieldHeight - StrikeGame::widePaddleHeight), game.getPaddleTop());
}

//——— the launch ———

void test_ball_rides_the_paddle_until_the_right_click(){
    StrikeGame game;
    game.reset(4);
    TEST_ASSERT_TRUE(game.getPhase() == Phase::INTRO);
    //a click during the stage's name does nothing; the ball follows the paddle
    game.update(rightClick());
    TEST_ASSERT_TRUE(game.getPhase() == Phase::INTRO);
    for(int t = 0; t < 10; t++) game.update(leftStick(1.0f));
    const StrikeGame::Ball& b = game.getBall(0);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, game.getPaddleTop() + (StrikeGame::paddleHeight - StrikeGame::ballSize) / 2.0f, b.y);
    TEST_ASSERT_TRUE(b.x > StrikeGame::paddleFace);
    while(game.getPhase() == Phase::INTRO) game.update(GameInput());
    TEST_ASSERT_TRUE(game.getPhase() == Phase::SERVE);
    //waits for ever; the left click does not launch
    GameInput left;
    left.clicked[0] = true;
    for(int t = 0; t < 500; t++) game.update(t == 10 ? left : GameInput());
    TEST_ASSERT_TRUE(game.getPhase() == Phase::SERVE);
    float x = b.x;
    game.update(rightClick());
    TEST_ASSERT_TRUE(game.getPhase() == Phase::PLAY);
    game.update(GameInput());
    TEST_ASSERT_TRUE(b.x > x);
    //off toward the middle: the paddle was at the top, so down
    TEST_ASSERT_TRUE(b.dy > 0.0f);
    TEST_ASSERT_TRUE(b.dx >= StrikeGame::minHorizontal);
}

//——— the physics ———

//a ball sent at the paddle hitting it at `offset` px from its centre (ball centre); returns the ball after the hit
static StrikeGame::Ball bounceAt(float offset){
    StrikeGame game;
    toServe(game);
    guardedSet(game);
    game.placePaddle(10.0f);
    float centre = 10.0f + StrikeGame::paddleHeight / 2.0f;
    game.placeBall(10.0f, centre + offset - 1.0f, -1.0f, 0.0f);
    for(int t = 0; t < 20 && game.getBall(0).dx < 0.0f; t++) game.update(GameInput());
    return game.getBall(0);
}

void test_bounce_angle_from_the_place_on_the_paddle(){
    StrikeGame::Ball centre = bounceAt(0.0f);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 1.0f, centre.dx);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, centre.dy);
    //further from the centre, steeper; above goes up, below goes down; never steeper than the tip's angle
    float lastDy = -2.0f;
    for(float offset = -4.5f; offset <= 4.5f; offset += 0.5f){
        StrikeGame::Ball b = bounceAt(offset);
        TEST_ASSERT_TRUE(b.dx >= StrikeGame::minHorizontal - 1e-5f);
        TEST_ASSERT_TRUE(b.dy >= lastDy);
        if(offset < -0.4f) TEST_ASSERT_TRUE(b.dy < 0.0f);
        if(offset > 0.4f) TEST_ASSERT_TRUE(b.dy > 0.0f);
        lastDy = b.dy;
    }
    StrikeGame::Ball tip = bounceAt(4.9f);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, sinf(StrikeGame::maxBounceAngle * 4.9f / 5.0f), tip.dy);    //4.9 of the 5 px to the tip
    TEST_ASSERT_TRUE(cosf(StrikeGame::maxBounceAngle) >= StrikeGame::minHorizontal);
}

//a steep ball (tests place one) is made no steeper than the minimum horizontal share at its first bounce off
//anything that changes its angle; whatever happens in play, a ball never goes up and down for ever
void test_ball_keeps_a_minimum_horizontal_speed(){
    for(uint32_t seed = 1; seed <= 6; seed++){
        StrikeGame game;
        game.setDemoLapses(seed % 2 == 0);
        game.reset(seed);
        game.startStage(uint8_t(seed * 2 % StrikeGame::stageCount));
        for(int t = 0; t < 50 * 120 && !game.isOver(); t++){
            GameInput input;
            game.demo(input);
            game.update(input);
            for(uint8_t i = 0; i < StrikeGame::maxBalls; i++){
                const StrikeGame::Ball& b = game.getBall(i);
                if(!b.b_live) continue;
                TEST_ASSERT_TRUE(fabsf(b.dx) >= StrikeGame::minHorizontal - 1e-4f);
                TEST_ASSERT_FLOAT_WITHIN(1e-3f, 1.0f, b.dx * b.dx + b.dy * b.dy);
            }
        }
    }
}

void test_speed_rises_every_ten_pieces_and_resets_on_a_lost_ball(){
    TEST_ASSERT_EQUAL_FLOAT(StrikeGame::baseSpeed, StrikeGame::ballSpeedFor(0, 0));
    TEST_ASSERT_EQUAL_FLOAT(StrikeGame::baseSpeed, StrikeGame::ballSpeedFor(0, 9));
    TEST_ASSERT_EQUAL_FLOAT(StrikeGame::baseSpeed + StrikeGame::brickSpeedStep, StrikeGame::ballSpeedFor(0, 10));
    TEST_ASSERT_EQUAL_FLOAT(StrikeGame::baseSpeed + 2 * StrikeGame::brickSpeedStep, StrikeGame::ballSpeedFor(0, 25));
    TEST_ASSERT_EQUAL_FLOAT(StrikeGame::baseSpeed + StrikeGame::loopSpeedStep, StrikeGame::ballSpeedFor(1, 0));
    TEST_ASSERT_EQUAL_FLOAT(StrikeGame::topSpeed, StrikeGame::ballSpeedFor(3, 500));

    //played: the demo breaks ten pieces, the speed is up a step; then the ball is let go, back to the start
    StrikeGame game;
    game.setDemoLapses(false);
    game.reset(3);
    for(int t = 0; t < 50 * 120 && game.getBrokenSinceLost() < 10; t++){
        GameInput input;
        game.demo(input);
        game.update(input);
    }
    TEST_ASSERT_TRUE(game.getBrokenSinceLost() >= 10);
    TEST_ASSERT_TRUE(game.getSpeed() > StrikeGame::baseSpeed);
    uint8_t lives = game.getLives();
    for(int t = 0; t < 50 * 30 && game.getLives() == lives; t++) game.update(leftStick(game.getBall(0).y < 15 ? -1.0f : 1.0f));
    TEST_ASSERT_EQUAL_UINT8(lives - 1, game.getLives());
    TEST_ASSERT_EQUAL_UINT16(0, game.getBrokenSinceLost());
    TEST_ASSERT_EQUAL_FLOAT(StrikeGame::baseSpeed, game.getSpeed());
}

//at the top speed (3 px a tick) a ball shot at a wall of truss from every angle never gets into it or past it, and a
//ball shot at the paddle from every angle and height is never let through it
void test_no_tunnelling_at_top_speed(){
    for(int a = -60; a <= 60; a += 1){
        float angle = float(a) * 0.0175f;
        for(int start = 0; start < 6; start++){
            StrikeGame game;
            toServe(game);
            game.startStage(0, 10);         //far round the loop: the top speed
            while(game.getPhase() == Phase::INTRO) game.update(GameInput());
            game.clearSet();
            for(uint8_t r = 0; r < StrikeGame::rows; r++) game.setBrick(r, 3, Brick::TRUSS);
            game.setBrick(2, 9, Brick::PIECE);      //behind the wall: never reached
            game.placeBall(30.0f + start * 1.37f, 4.0f + start * 4.1f, cosf(angle), sinf(angle));
            TEST_ASSERT_EQUAL_FLOAT(StrikeGame::topSpeed, game.getSpeed());
            game.placePaddle(0.0f);
            for(int t = 0; t < 150; t++){
                const StrikeGame::Ball& b = game.getBall(0);
                //the paddle follows the ball exactly: it is never lost
                game.placePaddle(fminf(fmaxf(b.y - 3.0f, 0.0f), float(StrikeGame::fieldHeight - StrikeGame::paddleHeight)));
                game.update(GameInput());
                TEST_ASSERT_FALSE(ballInsideSet(game));
                TEST_ASSERT_TRUE(game.getBall(0).b_live);
                TEST_ASSERT_TRUE(game.getBall(0).x + StrikeGame::ballSize <= StrikeGame::brickX(3));
                TEST_ASSERT_TRUE(game.getBall(0).x >= StrikeGame::paddleFace);
                TEST_ASSERT_TRUE(game.getBall(0).y >= 0.0f && game.getBall(0).y <= StrikeGame::fieldHeight - StrikeGame::ballSize);
            }
            TEST_ASSERT_EQUAL_UINT8(3, game.getLives());
            TEST_ASSERT_TRUE(game.getBrick(2, 9) == Brick::PIECE);
        }
    }
}

//a ball into the corner between two pieces breaks both and comes back the way it came; into one piece's corner it
//turns on one axis and goes on
void test_corner_hits(){
    StrikeGame game;
    toServe(game);
    game.clearSet();
    game.setBrick(0, 9, Brick::TRUSS);
    //an inner corner: a piece at (2, 1) and one right below its left neighbour (3, 0)... the ball comes from below left
    game.setBrick(1, 1, Brick::PIECE);
    game.setBrick(2, 0, Brick::PIECE);
    game.setBrick(0, 8, Brick::PIECE);
    //aimed at the inside corner between (1, 1)'s left side... and (2, 0)'s top
    float cornerX = StrikeGame::brickX(1), cornerY = StrikeGame::brickY(2);
    game.placeBall(cornerX - 8.0f, cornerY - 8.0f, 1.0f, 1.0f);
    for(int t = 0; t < 30 && game.getBall(0).dx > 0.0f; t++) game.update(GameInput());
    const StrikeGame::Ball& b = game.getBall(0);
    TEST_ASSERT_TRUE(b.dx < 0.0f);
    TEST_ASSERT_TRUE(b.dy < 0.0f);
    TEST_ASSERT_TRUE(game.getBrick(1, 1) == Brick::EMPTY);
    TEST_ASSERT_TRUE(game.getBrick(2, 0) == Brick::EMPTY);
    TEST_ASSERT_FALSE(ballInsideSet(game));

    //a lone piece's top left corner from up left: one of the two axes turns, the ball never ends up inside it
    StrikeGame single;
    toServe(single);
    guardedSet(single);
    single.setBrick(2, 4, Brick::TRUSS);
    float x = StrikeGame::brickX(4), y = StrikeGame::brickY(2);
    single.placeBall(x - 9.0f, y - 9.0f, 1.0f, 1.0f);
    for(int t = 0; t < 10; t++){
        single.update(GameInput());
        TEST_ASSERT_FALSE(ballInsideSet(single));
    }
    const StrikeGame::Ball& s = single.getBall(0);
    TEST_ASSERT_TRUE(s.dx < 0.0f || s.dy < 0.0f);
}

//——— the set ———

void test_each_kind_of_piece(){
    StrikeGame game;
    toServe(game);
    guardedSet(game);
    //a set piece: one hit, 10 points, the ball comes back
    game.setBrick(2, 0, Brick::PIECE);
    uint32_t points = game.score();
    game.placeBall(60.0f, StrikeGame::brickY(2) + 1.5f, 1.0f, 0.0f);
    for(int t = 0; t < 20 && game.getBall(0).dx > 0.0f; t++) game.update(GameInput());
    TEST_ASSERT_TRUE(game.getBrick(2, 0) == Brick::EMPTY);
    TEST_ASSERT_EQUAL_UINT32(points + StrikeGame::piecePoints, game.score());
    TEST_ASSERT_TRUE(game.getBall(0).dx < 0.0f);

    //a hatched piece: cracked by the first hit (5), broken by the second (25)
    game.setBrick(2, 0, Brick::DOUBLE);
    points = game.score();
    game.placeBall(60.0f, StrikeGame::brickY(2) + 1.5f, 1.0f, 0.0f);
    for(int t = 0; t < 20 && game.getBall(0).dx > 0.0f; t++) game.update(GameInput());
    TEST_ASSERT_TRUE(game.getBrick(2, 0) == Brick::CRACKED);
    TEST_ASSERT_EQUAL_UINT32(points + StrikeGame::crackPoints, game.score());
    game.placeBall(60.0f, StrikeGame::brickY(2) + 1.5f, 1.0f, 0.0f);
    for(int t = 0; t < 20 && game.getBall(0).dx > 0.0f; t++) game.update(GameInput());
    TEST_ASSERT_TRUE(game.getBrick(2, 0) == Brick::EMPTY);
    TEST_ASSERT_EQUAL_UINT32(points + StrikeGame::crackPoints + StrikeGame::doublePoints, game.score());

    //truss: the ball bounces, nothing breaks, no points, and it does not count as a breakable piece
    game.setBrick(2, 0, Brick::TRUSS);
    points = game.score();
    uint16_t left = game.breakablesLeft();
    for(int k = 0; k < 5; k++){
        game.placeBall(60.0f, StrikeGame::brickY(2) + 1.5f, 1.0f, 0.0f);
        for(int t = 0; t < 20 && game.getBall(0).dx > 0.0f; t++) game.update(GameInput());
        TEST_ASSERT_TRUE(game.getBall(0).dx < 0.0f);
    }
    TEST_ASSERT_TRUE(game.getBrick(2, 0) == Brick::TRUSS);
    TEST_ASSERT_EQUAL_UINT32(points, game.score());
    TEST_ASSERT_EQUAL_UINT16(left, game.breakablesLeft());

    //a prize piece: one hit, 15 points, its power-up drops where it was and drifts left
    game.setBrick(2, 0, Brick::PRIZE_SLOW);
    points = game.score();
    game.placeBall(60.0f, StrikeGame::brickY(2) + 1.5f, 1.0f, 0.0f);
    for(int t = 0; t < 20 && game.getBall(0).dx > 0.0f; t++) game.update(GameInput());
    TEST_ASSERT_TRUE(game.getBrick(2, 0) == Brick::EMPTY);
    TEST_ASSERT_EQUAL_UINT32(points + StrikeGame::prizePoints, game.score());
    TEST_ASSERT_EQUAL_UINT8(1, game.getPrizeCount());
    const StrikeGame::Prize& p = game.getPrize(0);
    TEST_ASSERT_TRUE(p.kind == PowerUp::SLOW);
    TEST_ASSERT_EQUAL_FLOAT(float(StrikeGame::brickY(2)), p.y);
    float x = p.x;
    game.update(GameInput());
    TEST_ASSERT_FLOAT_WITHIN(0.001f, x - StrikeGame::prizeSpeed * Game::tickSeconds, p.x);
}

//——— the power-ups ———

//breaks the prize piece in front of the ball and keeps the paddle under its power-up until it is caught
static void breakAndCatch(StrikeGame& game, Brick prize){
    game.setBrick(2, 0, prize);
    game.placeBall(60.0f, StrikeGame::brickY(2) + 1.5f, 1.0f, 0.0f);
    for(int t = 0; t < 400 && (game.getPrizeCount() > 0 || game.getBrick(2, 0) != Brick::EMPTY); t++){
        //keep the ball in play away from the prize: it is put back in the middle, heading right, now and then
        const StrikeGame::Ball& b = game.getBall(0);
        if(b.dx < 0.0f && b.x < 40.0f) game.placeBall(60.0f, 20.0f, 1.0f, 0.0f);
        float target = game.getPrizeCount() > 0 ? game.getPrize(0).y + 2.5f : 15.0f;
        float centre = game.getPaddleTop() + game.getPaddleHeight() / 2.0f;
        game.update(leftStick(fabsf(centre - target) < 1.0f ? 0.0f : (centre > target ? 1.0f : -1.0f)));
    }
}

void test_power_up_wide_paddle_and_its_end(){
    StrikeGame game;
    RecordingSink sink;
    game.setSound(&sink);
    toServe(game);
    guardedSet(game);
    uint32_t points = game.score();
    breakAndCatch(game, Brick::PRIZE_WIDE);
    TEST_ASSERT_EQUAL_UINT8(0, game.getPrizeCount());
    TEST_ASSERT_EQUAL_INT16(StrikeGame::widePaddleHeight, game.getPaddleHeight());
    TEST_ASSERT_TRUE(game.getWideLeft() > 0);
    TEST_ASSERT_EQUAL_UINT32(points + StrikeGame::prizePoints + StrikeGame::catchPoints, game.score());
    TEST_ASSERT_TRUE(sink.last(SoundLayer::FRONT)->cue.notes == StrikeGame::catchSound().notes);
    //the ball kept in play until it runs out
    for(int t = 0; t < StrikeGame::wideTicks + 10 && game.getWideLeft() > 0; t++){
        if(game.getBall(0).x < 40.0f) game.placeBall(60.0f, 20.0f, 1.0f, 0.0f);
        game.update(GameInput());
    }
    TEST_ASSERT_EQUAL_UINT16(0, game.getWideLeft());
    TEST_ASSERT_EQUAL_INT16(StrikeGame::paddleHeight, game.getPaddleHeight());
    TEST_ASSERT_TRUE(game.getPaddleTop() >= 0.0f && game.getPaddleTop() <= StrikeGame::fieldHeight - StrikeGame::paddleHeight);
}

void test_power_up_slow_ball_and_its_end(){
    StrikeGame game;
    toServe(game);
    guardedSet(game);
    float speed = game.getSpeed();
    breakAndCatch(game, Brick::PRIZE_SLOW);
    TEST_ASSERT_TRUE(game.getSlowLeft() > 0);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, speed * StrikeGame::slowShare, game.getSpeed());
    //the ball really moves slower
    game.placeBall(30.0f, 15.0f, 1.0f, 0.0f);
    game.update(GameInput());
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 30.0f + speed * StrikeGame::slowShare * Game::tickSeconds, game.getBall(0).x);
    for(int t = 0; t < StrikeGame::slowTicks + 10 && game.getSlowLeft() > 0; t++){
        if(game.getBall(0).x < 40.0f) game.placeBall(60.0f, 20.0f, 1.0f, 0.0f);
        game.update(GameInput());
    }
    TEST_ASSERT_EQUAL_UINT16(0, game.getSlowLeft());
    TEST_ASSERT_EQUAL_FLOAT(speed, game.getSpeed());
}

void test_power_up_three_balls_and_one_life_for_all(){
    StrikeGame game;
    toServe(game);
    guardedSet(game);
    breakAndCatch(game, Brick::PRIZE_MULTI);
    TEST_ASSERT_EQUAL_UINT8(3, game.getBallCount());
    //spread: three directions, all heading the same way
    const StrikeGame::Ball& a = game.getBall(0);
    const StrikeGame::Ball& b = game.getBall(1);
    const StrikeGame::Ball& c = game.getBall(2);
    TEST_ASSERT_TRUE(a.dy != b.dy && b.dy != c.dy && a.dy != c.dy);
    TEST_ASSERT_TRUE((a.dx > 0.0f) == (b.dx > 0.0f) && (b.dx > 0.0f) == (c.dx > 0.0f));
    //caught again with three in play: still three
    game.dropPrize(2.0f, game.getPaddleTop() + 1.0f, PowerUp::MULTI);
    game.update(GameInput());
    TEST_ASSERT_EQUAL_UINT8(3, game.getBallCount());
    //the paddle hides at the top: the balls go one by one, only the last one costs a ball
    uint8_t lives = game.getLives();
    bool b_fewer = false;
    for(int t = 0; t < 50 * 60 && game.getLives() == lives; t++){
        game.update(leftStick(1.0f));
        if(game.getLives() == lives && game.getBallCount() < 3 && game.getBallCount() > 0) b_fewer = true;
    }
    TEST_ASSERT_TRUE(b_fewer);
    TEST_ASSERT_EQUAL_UINT8(lives - 1, game.getLives());
    TEST_ASSERT_EQUAL_UINT8(1, game.getBallCount());
    TEST_ASSERT_TRUE(game.getPhase() == Phase::SERVE);
}

void test_power_up_missed_drifts_off_and_lost_ball_ends_the_effects(){
    StrikeGame game;
    toServe(game);
    guardedSet(game);
    game.placeBall(60.0f, 20.0f, 1.0f, 0.0f);
    game.placePaddle(0.0f);
    game.dropPrize(40.0f, 20.0f, PowerUp::WIDE);
    for(int t = 0; t < 150; t++){
        if(game.getBall(0).x < 40.0f) game.placeBall(60.0f, 20.0f, 1.0f, 0.0f);
        game.update(GameInput());
    }
    TEST_ASSERT_EQUAL_UINT8(0, game.getPrizeCount());
    TEST_ASSERT_EQUAL_INT16(StrikeGame::paddleHeight, game.getPaddleHeight());

    //wide and slow running, the ball lost: both end, the prizes in flight go
    game.dropPrize(2.0f, 1.0f, PowerUp::WIDE);
    game.update(GameInput());
    game.dropPrize(2.0f, 1.0f, PowerUp::SLOW);
    game.update(GameInput());
    TEST_ASSERT_TRUE(game.getWideLeft() > 0 && game.getSlowLeft() > 0);
    game.dropPrize(60.0f, 20.0f, PowerUp::MULTI);
    game.placeBall(20.0f, 20.0f, -1.0f, 0.0f);
    for(int t = 0; t < 100 && game.getPhase() == Phase::PLAY; t++) game.update(GameInput());
    TEST_ASSERT_TRUE(game.getPhase() == Phase::SERVE);
    TEST_ASSERT_EQUAL_UINT16(0, game.getWideLeft());
    TEST_ASSERT_EQUAL_UINT16(0, game.getSlowLeft());
    TEST_ASSERT_EQUAL_UINT8(0, game.getPrizeCount());
    TEST_ASSERT_EQUAL_INT16(StrikeGame::paddleHeight, game.getPaddleHeight());
}

//——— balls, stages, the loop ———

void test_three_balls_then_game_over(){
    StrikeGame game;
    RecordingSink sink;
    game.setSound(&sink);
    toServe(game);
    TEST_ASSERT_EQUAL_UINT8(3, game.getLives());
    for(uint8_t lost = 1; lost <= 3; lost++){
        game.update(rightClick());
        //the paddle runs away from the ball
        for(int t = 0; t < 50 * 30 && game.getPhase() == Phase::PLAY; t++){
            game.update(leftStick(game.getBall(0).y < 15 ? -1.0f : 1.0f));
        }
        TEST_ASSERT_EQUAL_UINT8(3 - lost, game.getLives());
        TEST_ASSERT_TRUE(sink.last(SoundLayer::FRONT)->cue.notes == StrikeGame::lostSound().notes);
        TEST_ASSERT_TRUE(game.getPhase() == (lost < 3 ? Phase::SERVE : Phase::OVER));
    }
    TEST_ASSERT_TRUE(game.isOver());
    //nothing moves after the end
    float top = game.getPaddleTop();
    game.update(leftStick(1.0f));
    TEST_ASSERT_EQUAL_FLOAT(top, game.getPaddleTop());
    char text[16];
    game.overText(text, sizeof(text), 1450);
    TEST_ASSERT_EQUAL_STRING("1450 STAGE 1", text);
    //the falling notes end before the Arcade's game over phrase (400 ms)
    TEST_ASSERT_TRUE(StrikeGame::lostSound().lengthMillis() < 400);
}

//the last breakable piece: the stage is struck (bonus by its number), the next one's name, the ball on the paddle
void test_stage_struck_and_the_next_one(){
    StrikeGame game;
    RecordingSink sink;
    game.setSound(&sink);
    toServe(game);
    game.clearSet();
    game.setBrick(1, 3, Brick::TRUSS);
    game.setBrick(2, 0, Brick::PIECE);
    uint32_t points = game.score();
    game.placeBall(60.0f, StrikeGame::brickY(2) + 1.5f, 1.0f, 0.0f);
    for(int t = 0; t < 20 && game.getPhase() == Phase::PLAY; t++) game.update(GameInput());
    TEST_ASSERT_TRUE(game.getPhase() == Phase::STRUCK);
    TEST_ASSERT_EQUAL_UINT32(points + StrikeGame::piecePoints + StrikeGame::stageBonus * 1, game.score());
    TEST_ASSERT_TRUE(sink.last(SoundLayer::FRONT)->cue.notes == StrikeGame::struckSound().notes);
    TEST_ASSERT_EQUAL_UINT8(0, game.getBallCount());
    for(int t = 0; t < StrikeGame::struckTicks; t++) game.update(GameInput());
    TEST_ASSERT_TRUE(game.getPhase() == Phase::INTRO);
    TEST_ASSERT_EQUAL_UINT8(1, game.getStage());
    TEST_ASSERT_EQUAL_UINT16(2, game.getStageNumber());
    TEST_ASSERT_TRUE(game.getBrick(0, 0) == StrikeGame::stageBrick(1, 0, 0));
    TEST_ASSERT_EQUAL_UINT8(1, game.getBallCount());
    TEST_ASSERT_EQUAL_UINT8(3, game.getLives());
}

//after the last stage the first comes again, faster, and its bonus counts the loop
void test_after_the_last_stage_the_loop_comes_round_faster(){
    StrikeGame game;
    toServe(game);
    game.startStage(StrikeGame::stageCount - 1);
    while(game.getPhase() == Phase::INTRO) game.update(GameInput());
    float speed = game.getSpeed();
    game.clearSet();
    game.setBrick(2, 0, Brick::PIECE);
    uint32_t points = game.score();
    game.placeBall(60.0f, StrikeGame::brickY(2) + 1.5f, 1.0f, 0.0f);
    for(int t = 0; t < 20 && game.getPhase() == Phase::PLAY; t++) game.update(GameInput());
    TEST_ASSERT_EQUAL_UINT32(points + StrikeGame::piecePoints + StrikeGame::stageBonus * StrikeGame::stageCount, game.score());
    for(int t = 0; t < StrikeGame::struckTicks; t++) game.update(GameInput());
    TEST_ASSERT_EQUAL_UINT8(0, game.getStage());
    TEST_ASSERT_EQUAL_UINT8(1, game.getLoop());
    TEST_ASSERT_EQUAL_UINT16(StrikeGame::stageCount + 1, game.getStageNumber());
    TEST_ASSERT_TRUE(game.getSpeed() > speed);
    TEST_ASSERT_EQUAL_FLOAT(StrikeGame::ballSpeedFor(1, game.getBrokenSinceLost()), game.getSpeed());
    TEST_ASSERT_TRUE(game.getBrick(1, 1) == StrikeGame::stageBrick(0, 1, 1));
}

//every stage as written: a name, only known places, inside the field and clear of the paddle's side, something to
//break, truss from the third stage on, later stages harder (more hatched pieces and truss)
void test_every_stage_is_valid(){
    for(uint8_t s = 0; s < StrikeGame::stageCount; s++){
        const char* name = StrikeGame::stageName(s);
        TEST_ASSERT_TRUE(strlen(name) > 0 && strlen(name) <= 10);
        for(uint8_t o = 0; o < s; o++) TEST_ASSERT_TRUE(strcmp(name, StrikeGame::stageName(o)) != 0);
        //the name's pill fits the gap between the paddle and the set
        TEST_ASSERT_TRUE(GameDraw::pillWidth(name) + 2 <= StrikeGame::setLeft - StrikeGame::paddleFace - 4);
        uint16_t breakable = 0, truss = 0, doubles = 0;
        for(uint8_t r = 0; r < StrikeGame::rows; r++){
            for(uint8_t c = 0; c < StrikeGame::columns; c++){
                Brick b = StrikeGame::stageBrick(s, r, c);
                if(b == Brick::EMPTY) continue;
                TEST_ASSERT_TRUE(b != Brick::CRACKED);
                if(b == Brick::TRUSS) truss++;
                else breakable++;
                if(b == Brick::DOUBLE) doubles++;
                //inside the field, never over the last column (dark) or the dark last row, never touching another
                int16_t x = StrikeGame::brickX(c), y = StrikeGame::brickY(r);
                TEST_ASSERT_TRUE(x >= StrikeGame::paddleFace + 40 && x + StrikeGame::brickWidth <= Game::width - 1);
                TEST_ASSERT_TRUE(y >= 1 && y + StrikeGame::brickHeight <= StrikeGame::fieldHeight - 1);
            }
        }
        TEST_ASSERT_TRUE(breakable > 0);
        if(s >= 2) TEST_ASSERT_TRUE(truss > 0);
        if(s >= StrikeGame::stageCount - 2) TEST_ASSERT_TRUE(truss >= 8 && doubles >= 6);
    }
    //the 1 px gaps: neighbouring places never touch (pitch minus size)
    TEST_ASSERT_EQUAL_INT16(1, StrikeGame::pitchX - StrikeGame::brickWidth);
    TEST_ASSERT_EQUAL_INT16(1, StrikeGame::pitchY - StrikeGame::brickHeight);
    //a ball (2 px) fits neither over the first row nor under the last
    TEST_ASSERT_TRUE(StrikeGame::brickY(0) < StrikeGame::ballSize);
    TEST_ASSERT_TRUE(StrikeGame::fieldHeight - (StrikeGame::brickY(StrikeGame::rows - 1) + StrikeGame::brickHeight) < StrikeGame::ballSize);
}

//A search-driven pilot for the proof that every stage can be struck. It plays only through the stick and the click,
//like a player. For each ball coming toward the paddle it tries every place on the paddle (1 px apart) in copies of
//the game (the game is deterministic), each copy's paddle put right under the ball, and takes the place whose shot
//scores soonest (a random one when none scores within 4.4 s); then it steers the paddle there with the stick
struct SearchPilot{
    typedef StrikeGame::Ball Ball;
    int8_t ball = -1;
    bool b_heading[StrikeGame::maxBalls] = {};
    float aim = 0;
    GameRandom random;
    explicit SearchPilot(uint32_t seed){ random.seed(seed); }

    //where a ball's top will be at the paddle's face (the top and bottom bounces folded)
    static float predict(const Ball& b){
        const float bottom = float(StrikeGame::fieldHeight - StrikeGame::ballSize);
        if(b.dx >= 0.0f) return b.y;
        float t = (b.x - float(StrikeGame::paddleFace)) / -b.dx;
        float y = b.y + b.dy * t, period = 2.0f * bottom;
        y = fmodf(y, period);
        if(y < 0.0f) y += period;
        if(y > bottom) y = period - y;
        return y;
    }
    static int8_t soonest(const StrikeGame& g){
        int8_t next = -1;
        float best = 1e9f;
        for(uint8_t i = 0; i < StrikeGame::maxBalls; i++){
            const Ball& b = g.getBall(i);
            if(!b.b_live || b.dx >= 0.0f || b.x < StrikeGame::paddleFace - 0.5f) continue;
            float t = (b.x - StrikeGame::paddleFace) / -b.dx;
            if(t < best){
                best = t;
                next = int8_t(i);
            }
        }
        return next;
    }
    //ticks until a copy scores, ball i taken at `offset` px from the paddle's centre
    static int tryAim(const StrikeGame& g, uint8_t i, float offset){
        StrikeGame copy = g;
        copy.setSound(nullptr);
        uint32_t points = copy.score();
        bool b_hit = false;
        for(int t = 0; t < 220; t++){
            const Ball& b = copy.getBall(i);
            if(!b_hit && b.b_live && b.dx < 0.0f){
                float h = copy.getPaddleHeight();
                float top = predict(b) + 1.0f - offset - h / 2.0f;
                copy.placePaddle(fminf(fmaxf(top, 0.0f), StrikeGame::fieldHeight - h));
            }
            else b_hit = true;
            copy.update(GameInput());
            if(copy.getPhase() == Phase::STRUCK) return t;
            if(copy.getPhase() != Phase::PLAY) return 100000;
            if(b_hit && copy.score() > points) return t;
        }
        return 10000;
    }
    GameInput next(const StrikeGame& g){
        GameInput input;
        if(g.getPhase() == Phase::SERVE){
            input.clicked[1] = true;
            return input;
        }
        if(g.getPhase() != Phase::PLAY) return input;
        int8_t n = soonest(g);
        for(uint8_t i = 0; i < StrikeGame::maxBalls; i++){
            const Ball& b = g.getBall(i);
            bool b_left = b.b_live && b.dx < 0.0f;
            if(b_left && !b_heading[i] && int8_t(i) == ball) ball = -1;
            b_heading[i] = b_left;
        }
        float h = g.getPaddleHeight();
        float target = StrikeGame::fieldHeight / 2.0f;
        if(n >= 0){
            if(n != ball){
                ball = n;
                int best = 1 << 30;
                float half = h / 2.0f - 0.5f;
                for(float o = -half; o <= half + 0.01f; o += 1.0f){
                    int t = tryAim(g, uint8_t(n), o);
                    if(t < best || (t == best && random.below(2))){
                        best = t;
                        aim = o;
                    }
                }
                if(best >= 10000) aim = (random.unit() * 2.0f - 1.0f) * half;
            }
            target = predict(g.getBall(n)) + 1.0f - aim;
        }
        float error = g.getPaddleTop() + h / 2.0f - target;
        if(fabsf(error) > 0.3f){
            float stick = fminf(fmaxf(error * 0.5f, -1.0f), 1.0f);
            if(fabsf(stick) < 0.15f) stick = stick < 0.0f ? -0.15f : 0.15f;
            input.stick[1] = stick;
        }
        return input;
    }
};

//every stage can be struck, proven by playing it with the search pilot: from its intro, five seeds, on the first round
//and the faster second one, within 6 minutes and without losing a ball; no ball ever gets inside a piece
void test_every_stage_can_be_struck(){
    for(uint8_t s = 0; s < StrikeGame::stageCount; s++){
        for(uint8_t loopIndex = 0; loopIndex < 2; loopIndex++){
            for(uint32_t seed = 1; seed <= 5; seed++){
                StrikeGame game;
                game.reset(seed * 31u + s);
                game.startStage(s, loopIndex);
                SearchPilot pilot(seed);
                for(int t = 0; t < 50 * 360 && game.getPhase() != Phase::STRUCK && !game.isOver(); t++){
                    game.update(pilot.next(game));
                    if(ballInsideSet(game)) TEST_FAIL_MESSAGE("a ball inside a piece");
                }
                char message[48];
                snprintf(message, sizeof(message), "stage %u loop %u seed %lu", unsigned(s + 1), unsigned(loopIndex),
                         (unsigned long)seed);
                TEST_ASSERT_TRUE_MESSAGE(game.getPhase() == Phase::STRUCK, message);
                TEST_ASSERT_EQUAL_UINT8_MESSAGE(StrikeGame::startLives, game.getLives(), message);
            }
        }
    }
}

//the demo player without its lapses (aiming at random places on its paddle) never misses a ball it can reach and
//strikes the first stages on its own
void test_demo_without_lapses_keeps_the_ball(){
    for(uint32_t seed = 1; seed <= 4; seed++){
        StrikeGame game;
        game.setDemoLapses(false);
        game.reset(seed);
        for(int t = 0; t < 50 * 240 && game.getStageNumber() < 3; t++){
            GameInput input;
            game.demo(input);
            game.update(input);
        }
        TEST_ASSERT_TRUE(game.getStageNumber() >= 3);
        TEST_ASSERT_EQUAL_UINT8(StrikeGame::startLives, game.getLives());
    }
}

//——— score, slot, sound, demo ———

//Strike takes the spare slot 7 of the arcade's EEPROM block: the other games keep theirs, the layout and its version
//stay, a block saved before Strike existed reads the same
void test_score_slot_7_keeps_the_saved_scores(){
    TEST_ASSERT_EQUAL_UINT8(7, ArcadeScores::slotFor("STRIKE"));
    static const char* const before[] = {"PONG", "PARKING", "CAVE", "ROCKS", "SNAKE", "BLOCKS", "PLUME"};
    for(uint8_t i = 0; i < 7; i++) TEST_ASSERT_EQUAL_UINT8(i, ArcadeScores::slotFor(before[i]));
    TEST_ASSERT_EQUAL_UINT8(1, ArcadeScores::version);
    TEST_ASSERT_EQUAL_UINT8(8, ArcadeScores::slotCount);
    TEST_ASSERT_EQUAL_UINT16(8 + 8 * 24, ArcadeScores::blockSize);

    //a block written by the arcade of seven games: a score in every one of their slots
    RamArcadeStore store;
    {
        ArcadeScores old;
        old.load(&store);
        for(uint8_t slot = 0; slot < 7; slot++) old.insert(slot, 100u + slot, "LEO ", slot == 1);
    }
    uint8_t saved[ArcadeScores::blockSize];
    memcpy(saved, store.bytes, sizeof(saved));
    ArcadeScores now;
    now.load(&store);
    for(uint8_t slot = 0; slot < 7; slot++){
        TEST_ASSERT_TRUE(now.hasEntries(slot));
        TEST_ASSERT_EQUAL_UINT32(100u + slot, now.entry(slot, 0).score);
    }
    TEST_ASSERT_FALSE(now.hasEntries(7));
    //Strike's score goes in slot 7, higher is better; nothing else changes
    StrikeGame game;
    TEST_ASSERT_FALSE(game.lowerScoreIsBetter());
    TEST_ASSERT_EQUAL_UINT8(0, now.insert(ArcadeScores::slotFor(game.name()), 2450, "ANNA", game.lowerScoreIsBetter()));
    TEST_ASSERT_EQUAL_UINT8(1, now.insert(7, 900, "BOB ", false));
    TEST_ASSERT_EQUAL_UINT8(ArcadeScores::noRank, now.rankFor(7, 0, false));
    for(uint16_t i = 0; i < ArcadeScores::slotsOffset + 7 * ArcadeScores::slotSize; i++){
        if(i >= ArcadeScores::lastNameOffset && i < ArcadeScores::lastNameOffset + 4) continue;   //the last name entered
        TEST_ASSERT_EQUAL_HEX8(saved[i], store.bytes[i]);
    }
    //in the Arcade: the eighth game, its own slot, every game's slot different
    Arcade arcade;
    TEST_ASSERT_EQUAL_UINT8(8, Arcade::gameCount);
    TEST_ASSERT_EQUAL_STRING("STRIKE", arcade.game(7).name());
    TEST_ASSERT_EQUAL_UINT8(7, arcade.slotOf(7));
    for(uint8_t i = 0; i < Arcade::gameCount; i++) for(uint8_t j = 0; j < i; j++) TEST_ASSERT_TRUE(arcade.slotOf(i) != arcade.slotOf(j));
}

void test_help_lines_fit_the_card(){
    StrikeGame game;
    const char* lines[3];
    game.helpLines(lines);
    for(int i = 0; i < 3; i++){
        TEST_ASSERT_NOT_NULL(lines[i]);
        TEST_ASSERT_TRUE(strlen(lines[i]) <= 21);
        TEST_ASSERT_FALSE(lines[i][0] >= 'a' && lines[i][0] <= 'z');     //sentence case
    }
    TEST_ASSERT_NOT_NULL(strstr(lines[0], "L stick"));
    TEST_ASSERT_NOT_NULL(strstr(lines[1], "R click"));
}

//the sounds [D-157]: a square blip per piece one note higher per row up the set, an octave under it for a crack, a
//rough thud off the truss, a lower tick off the paddle; no music
void test_sounds_fire_on_the_events(){
    for(uint8_t r = 1; r < StrikeGame::rows; r++) TEST_ASSERT_TRUE(StrikeGame::brickHertz(r - 1) > StrikeGame::brickHertz(r));
    for(uint8_t r = 0; r < StrikeGame::rows; r++) TEST_ASSERT_TRUE(StrikeGame::paddleHertz < StrikeGame::brickHertz(r));

    StrikeGame game;
    RecordingSink sink;
    game.setSound(&sink);
    toServe(game);
    guardedSet(game);
    for(uint8_t r = 0; r < StrikeGame::rows; r++){
        game.setBrick(r, 0, Brick::PIECE);
        game.placeBall(60.0f, StrikeGame::brickY(r) + 1.5f, 1.0f, 0.0f);
        sink.clear();
        for(int t = 0; t < 20 && game.getBall(0).dx > 0.0f; t++) game.update(GameInput());
        const RecordingSink::Play* p = sink.last(SoundLayer::FRONT);
        TEST_ASSERT_NOT_NULL(p);
        TEST_ASSERT_TRUE(p->b_note && p->note.kind == SoundNote::Kind::SQUARE);
        TEST_ASSERT_EQUAL_UINT16(StrikeGame::brickHertz(r), p->note.hertz);
        TEST_ASSERT_TRUE(p->note.millis <= 60);
    }
    game.setBrick(2, 0, Brick::DOUBLE);
    game.placeBall(60.0f, StrikeGame::brickY(2) + 1.5f, 1.0f, 0.0f);
    sink.clear();
    for(int t = 0; t < 20 && game.getBall(0).dx > 0.0f; t++) game.update(GameInput());
    TEST_ASSERT_EQUAL_UINT16(StrikeGame::brickHertz(2) / 2, sink.lastHertz(SoundLayer::FRONT));
    game.setBrick(2, 0, Brick::TRUSS);
    game.placeBall(60.0f, StrikeGame::brickY(2) + 1.5f, 1.0f, 0.0f);
    sink.clear();
    for(int t = 0; t < 20 && game.getBall(0).dx > 0.0f; t++) game.update(GameInput());
    TEST_ASSERT_TRUE(sink.last(SoundLayer::FRONT)->cue.notes == StrikeGame::thudSound().notes);
    //the thud a rough duet, lower than everything else; squares and duets as in Pong: no music to cut
    const SoundNote& thud = StrikeGame::thudSound().notes[0];
    TEST_ASSERT_TRUE(thud.kind == SoundNote::Kind::DUET);
    TEST_ASSERT_TRUE(thud.hertz < StrikeGame::paddleHertz && thud.hertzB < StrikeGame::paddleHertz);
    TEST_ASSERT_TRUE(thud.hertz >= 400 && thud.hertzB != thud.hertz);
    TEST_ASSERT_TRUE(StrikeGame::thudSound().lengthMillis() <= 60);
    //the paddle's tick
    game.placePaddle(10.0f);
    game.placeBall(20.0f, 13.0f, -1.0f, 0.0f);
    sink.clear();
    for(int t = 0; t < 20 && game.getBall(0).dx < 0.0f; t++) game.update(GameInput());
    TEST_ASSERT_EQUAL_UINT16(StrikeGame::paddleHertz, sink.lastHertz(SoundLayer::FRONT));
    TEST_ASSERT_TRUE(sink.last(SoundLayer::FRONT)->b_note);
    //the arpeggio rises, the lost notes fall, the jingle ends high; all short but the jingle
    SoundCue c = StrikeGame::catchSound();
    for(uint16_t i = 1; i < c.noteCount; i++) TEST_ASSERT_TRUE(c.notes[i].hertz > c.notes[i - 1].hertz);
    SoundCue l = StrikeGame::lostSound();
    for(uint16_t i = 1; i < l.noteCount; i++) TEST_ASSERT_TRUE(l.notes[i].hertz < l.notes[i - 1].hertz);
    TEST_ASSERT_TRUE(c.lengthMillis() < 250);
    TEST_ASSERT_TRUE(StrikeGame::struckSound().lengthMillis() < StrikeGame::struckTicks * 20);
    //everything a game priority; no music in Strike
    TEST_ASSERT_EQUAL_UINT32(0, sink.count(SoundLayer::BACK));
    game.playMusic();
    TEST_ASSERT_EQUAL_UINT32(0, sink.count(SoundLayer::BACK));
    TEST_ASSERT_EQUAL_UINT8(SoundPriority::GAME, sink.last(SoundLayer::FRONT)->priority);
}

//sound never changes the game: the demo plays the same with a sink and without; without one nothing is heard
void test_demo_deterministic_and_silent_without_a_sink(){
    StrikeGame quiet, heard;
    RecordingSink sink;
    heard.setSound(&sink);
    quiet.reset(77);
    heard.reset(77);
    for(int t = 0; t < 50 * 120; t++){
        GameInput a, b;
        quiet.demo(a);
        heard.demo(b);
        TEST_ASSERT_EQUAL_FLOAT(a.stick[1], b.stick[1]);
        TEST_ASSERT_EQUAL(a.clicked[1], b.clicked[1]);
        quiet.update(a);
        heard.update(b);
        TEST_ASSERT_EQUAL_FLOAT(quiet.getPaddleTop(), heard.getPaddleTop());
        TEST_ASSERT_EQUAL_FLOAT(quiet.getBall(0).x, heard.getBall(0).x);
        TEST_ASSERT_EQUAL_FLOAT(quiet.getBall(0).y, heard.getBall(0).y);
        if(quiet.isOver()) break;
    }
    TEST_ASSERT_EQUAL_UINT32(quiet.score(), heard.score());
    TEST_ASSERT_EQUAL_UINT8(quiet.getLives(), heard.getLives());
    TEST_ASSERT_TRUE(sink.count(SoundLayer::FRONT) > 20);
    //a sink set later hears only what comes after; the silent game played into nothing
    RecordingSink late;
    quiet.setSound(&late);
    TEST_ASSERT_EQUAL_UINT32(0, late.total());
}

//the demo uses what a player has (the left stick's y and the right click), launches by itself, and ends its games
//well inside the picker's 3 minutes, sometimes later stages too
void test_demo_plays_like_a_player_and_ends(){
    int ended = 0;
    uint32_t longest = 0;
    for(uint32_t seed = 1; seed <= 12; seed++){
        StrikeGame game;
        game.reset(seed);
        uint32_t t = 0;
        bool b_launched = false;
        for(; t < 50 * 240 && !game.isOver(); t++){
            GameInput input;
            game.demo(input);
            TEST_ASSERT_EQUAL_FLOAT(0.0f, input.stick[0]);
            TEST_ASSERT_EQUAL_FLOAT(0.0f, input.stick[2]);
            TEST_ASSERT_EQUAL_FLOAT(0.0f, input.stick[3]);
            TEST_ASSERT_TRUE(input.stick[1] >= -1.0f && input.stick[1] <= 1.0f);
            TEST_ASSERT_FALSE(input.clicked[0] || input.held[0] || input.held[1]);
            game.update(input);
            if(game.getPhase() == Phase::PLAY) b_launched = true;
        }
        TEST_ASSERT_TRUE(b_launched);
        if(game.isOver()){
            ended++;
            if(t > longest) longest = t;
        }
    }
    TEST_ASSERT_EQUAL_INT(12, ended);
    TEST_ASSERT_TRUE(longest < 50 * 180);
}

int main(int argc, char** argv){
    UNITY_BEGIN();
    RUN_TEST(test_contract);
    RUN_TEST(test_paddle_speed_is_the_stick_deflection);
    RUN_TEST(test_paddle_stays_on_the_field);
    RUN_TEST(test_ball_rides_the_paddle_until_the_right_click);
    RUN_TEST(test_bounce_angle_from_the_place_on_the_paddle);
    RUN_TEST(test_ball_keeps_a_minimum_horizontal_speed);
    RUN_TEST(test_speed_rises_every_ten_pieces_and_resets_on_a_lost_ball);
    RUN_TEST(test_no_tunnelling_at_top_speed);
    RUN_TEST(test_corner_hits);
    RUN_TEST(test_each_kind_of_piece);
    RUN_TEST(test_power_up_wide_paddle_and_its_end);
    RUN_TEST(test_power_up_slow_ball_and_its_end);
    RUN_TEST(test_power_up_three_balls_and_one_life_for_all);
    RUN_TEST(test_power_up_missed_drifts_off_and_lost_ball_ends_the_effects);
    RUN_TEST(test_three_balls_then_game_over);
    RUN_TEST(test_stage_struck_and_the_next_one);
    RUN_TEST(test_after_the_last_stage_the_loop_comes_round_faster);
    RUN_TEST(test_every_stage_is_valid);
    RUN_TEST(test_every_stage_can_be_struck);
    RUN_TEST(test_demo_without_lapses_keeps_the_ball);
    RUN_TEST(test_score_slot_7_keeps_the_saved_scores);
    RUN_TEST(test_help_lines_fit_the_card);
    RUN_TEST(test_sounds_fire_on_the_events);
    RUN_TEST(test_demo_deterministic_and_silent_without_a_sink);
    RUN_TEST(test_demo_plays_like_a_player_and_ends);
    return UNITY_END();
}
