//Host tests of the remote's PONG game [D-148]: pio test -e native -f test_game_pong
#include <unity.h>
#include <HostGfx.h>
#include <GameContract.h>
#include <PongGame.h>
#include <math.h>

void setUp(){}
void tearDown(){}

static GameInput leftStick(float y){
    GameInput input;
    input.stick[1] = y;
    return input;
}

//plays with the given input until a point is scored or the game ends; returns the ticks played
static int playPoint(PongGame& game, const GameInput& input, int maxTicks = 50 * 60){
    int before = game.getPoints(0) + game.getPoints(1);
    for(int t = 0; t < maxTicks; t++){
        game.update(input);
        if(game.isOver() || game.getPoints(0) + game.getPoints(1) != before) return t + 1;
    }
    return maxTicks;
}

void test_contract(){
    GameContract::check<PongGame>();
}

void test_left_stick_places_the_paddle(){
    PongGame game;
    game.reset(1);
    const float bottom = float(Game::height - PongGame::paddleHeight);
    //the stick's height is the paddle's: centre in the middle, full travel already at 85 % up or down
    TEST_ASSERT_EQUAL_FLOAT(bottom / 2.0f, game.getPaddleTop(0));
    for(int t = 0; t < 20; t++) game.update(leftStick(0.85f));
    TEST_ASSERT_EQUAL_FLOAT(0.0f, game.getPaddleTop(0));
    for(int t = 0; t < 20; t++) game.update(leftStick(-1.0f));
    TEST_ASSERT_EQUAL_FLOAT(bottom, game.getPaddleTop(0));
    //half way up is a quarter of the court above the middle (proportional), back in the middle when let go
    TEST_ASSERT_FLOAT_WITHIN(0.01f, bottom / 2.0f * (1.0f - 0.5f / 0.85f), PongGame::stickPaddleTop(0.5f));
    for(int t = 0; t < 20; t++) game.update(leftStick(0.5f));
    TEST_ASSERT_FLOAT_WITHIN(0.2f, PongGame::stickPaddleTop(0.5f), game.getPaddleTop(0));
    for(int t = 0; t < 20; t++) game.update(GameInput());
    TEST_ASSERT_FLOAT_WITHIN(0.2f, bottom / 2.0f, game.getPaddleTop(0));
}

void test_paddle_follows_quickly_without_jumping(){
    PongGame game;
    game.reset(1);
    const float bottom = float(Game::height - PongGame::paddleHeight);
    //a full sweep from the top to the bottom: more than half way after the first tick (not laggy), not all the way
    //(smoothed), within half a pixel after 5 ticks (100 ms)
    for(int t = 0; t < 20; t++) game.update(leftStick(1.0f));
    game.update(leftStick(-1.0f));
    TEST_ASSERT_TRUE(game.getPaddleTop(0) > bottom * 0.5f && game.getPaddleTop(0) < bottom);
    for(int t = 1; t < 5; t++) game.update(leftStick(-1.0f));
    TEST_ASSERT_FLOAT_WITHIN(0.5f, bottom, game.getPaddleTop(0));
}

void test_stick_jitter_does_not_shake_the_paddle(){
    PongGame game;
    game.reset(1);
    for(int t = 0; t < 20; t++) game.update(leftStick(0.3f));
    //a sensor noise of +-1.5 % around a held stick: the drawn paddle stays on one pixel row
    long row = lroundf(game.getPaddleTop(0));
    for(int t = 0; t < 200; t++){
        game.update(leftStick(0.3f + ((t * 7) % 5 - 2) * 0.0075f));
        TEST_ASSERT_EQUAL_INT32(row, lroundf(game.getPaddleTop(0)));
    }
}

void test_first_serve_waits_then_goes_to_the_player(){
    PongGame game;
    game.reset(7);
    TEST_ASSERT_TRUE(game.isServing());
    float x = game.getBallX();
    for(int t = 0; t < 20; t++) game.update(GameInput());
    TEST_ASSERT_EQUAL_FLOAT(x, game.getBallX());
    while(game.isServing()) game.update(GameInput());
    game.update(GameInput());
    TEST_ASSERT_TRUE(game.getBallX() < x);
}

void test_ball_stays_in_the_court_and_bounces_off_the_paddle(){
    PongGame game;
    game.reset(3);
    bool b_hit = false;
    for(int t = 0; t < 50 * 120 && !game.isOver(); t++){
        GameInput input;
        game.demo(input);
        float velocity = game.getBallVelocityX();
        bool b_serving = game.isServing();
        game.update(input);
        TEST_ASSERT_TRUE(game.getBallY() >= 0.0f && game.getBallY() <= float(Game::height - PongGame::ballSize));
        if(!b_serving && !game.isServing() && velocity < 0.0f && game.getBallVelocityX() > 0.0f){
            //the left paddle took it: the ball touches the paddle's height and leaves faster than the serve
            b_hit = true;
            float top = game.getPaddleTop(0);
            TEST_ASSERT_TRUE(game.getBallY() + PongGame::ballSize > top && game.getBallY() < top + PongGame::paddleHeight);
        }
    }
    TEST_ASSERT_TRUE(b_hit);
}

void test_ball_speeds_up_with_each_hit(){
    PongGame game;
    game.reset(11);
    float serveSpeed = 0.0f;
    float fastest = 0.0f;
    for(int t = 0; t < 50 * 60 && !game.isOver(); t++){
        GameInput input;
        game.demo(input);
        game.update(input);
        if(serveSpeed == 0.0f) serveSpeed = game.getBallSpeed();
        if(game.getBallSpeed() > fastest) fastest = game.getBallSpeed();
    }
    TEST_ASSERT_TRUE(fastest > serveSpeed + 7.0f);
}

void test_point_serves_toward_the_loser_and_seven_ends_it(){
    PongGame game;
    game.reset(5);
    //the player hides at the top: the computer scores, then serves back toward the player
    GameInput input = leftStick(1.0f);
    int guard = 0;
    while(game.getPoints(1) == 0 && guard++ < 20) playPoint(game, input);
    TEST_ASSERT_EQUAL_UINT8(1, game.getPoints(1));
    TEST_ASSERT_TRUE(game.isServing());
    TEST_ASSERT_TRUE(game.getBallVelocityX() < 0.0f);
    while(!game.isOver() && guard++ < 40) playPoint(game, input);
    TEST_ASSERT_TRUE(game.isOver());
    TEST_ASSERT_EQUAL_UINT8(7, game.getPoints(1));
    TEST_ASSERT_TRUE(game.getPoints(0) < 7);
    //no update after the end
    float x = game.getBallX();
    game.update(input);
    TEST_ASSERT_EQUAL_FLOAT(x, game.getBallX());
}

void test_score_orders_by_points_then_conceded(){
    PongGame game;
    char text[10];
    //7-4 and 5-7 shown back from the value
    game.scoreText(text, sizeof(text), 7 * 8 + (7 - 4));
    TEST_ASSERT_EQUAL_STRING("7-4", text);
    game.scoreText(text, sizeof(text), 5 * 8 + 0);
    TEST_ASSERT_EQUAL_STRING("5-7", text);
    //a new game (0-0) is worth 7; a 0-7 loss is 0, never a best
    game.reset(1);
    TEST_ASSERT_EQUAL_UINT32(7, game.score());
}

void test_right_stick_brings_in_a_second_player(){
    PongGame game;
    game.reset(2);
    GameInput input;
    input.stick[3] = 0.4f;
    game.update(input);
    TEST_ASSERT_FALSE(game.isTwoPlayers());
    input.stick[3] = 0.8f;
    game.update(input);
    TEST_ASSERT_TRUE(game.isTwoPlayers());
    //the right stick places the right paddle like the left one, for the rest of the game
    input.stick[3] = 1.0f;
    for(int t = 0; t < 10; t++) game.update(input);
    TEST_ASSERT_EQUAL_FLOAT(0.0f, game.getPaddleTop(1));
    for(int t = 0; t < 20; t++) game.update(GameInput());
    TEST_ASSERT_FLOAT_WITHIN(0.2f, PongGame::stickPaddleTop(0.0f), game.getPaddleTop(1));
    TEST_ASSERT_TRUE(game.isTwoPlayers());
    game.reset(2);
    TEST_ASSERT_FALSE(game.isTwoPlayers());
}

void test_demo_only_uses_the_left_stick(){
    PongGame game;
    game.reset(9);
    for(int t = 0; t < 50 * 30; t++){
        GameInput input;
        game.demo(input);
        TEST_ASSERT_TRUE(input.stick[1] >= -1.0f && input.stick[1] <= 1.0f);
        TEST_ASSERT_EQUAL_FLOAT(0.0f, input.stick[0]);
        TEST_ASSERT_EQUAL_FLOAT(0.0f, input.stick[2]);
        TEST_ASSERT_EQUAL_FLOAT(0.0f, input.stick[3]);
        TEST_ASSERT_FALSE(input.clicked[0] || input.clicked[1] || input.held[0] || input.held[1]);
        float before = game.getPaddleTop(0);
        game.update(input);
        TEST_ASSERT_FALSE(game.isTwoPlayers());
        //the paddle moves toward where the stick places it, never past it
        float target = PongGame::stickPaddleTop(input.stick[1]);
        float after = game.getPaddleTop(0);
        TEST_ASSERT_TRUE(fabsf(after - target) <= fabsf(before - target) + 0.001f);
        if(game.isOver()) game.reset(uint32_t(t));
    }
}

int main(int argc, char** argv){
    UNITY_BEGIN();
    RUN_TEST(test_contract);
    RUN_TEST(test_left_stick_places_the_paddle);
    RUN_TEST(test_paddle_follows_quickly_without_jumping);
    RUN_TEST(test_stick_jitter_does_not_shake_the_paddle);
    RUN_TEST(test_first_serve_waits_then_goes_to_the_player);
    RUN_TEST(test_ball_stays_in_the_court_and_bounces_off_the_paddle);
    RUN_TEST(test_ball_speeds_up_with_each_hit);
    RUN_TEST(test_point_serves_toward_the_loser_and_seven_ends_it);
    RUN_TEST(test_score_orders_by_points_then_conceded);
    RUN_TEST(test_right_stick_brings_in_a_second_player);
    RUN_TEST(test_demo_only_uses_the_left_stick);
    return UNITY_END();
}
