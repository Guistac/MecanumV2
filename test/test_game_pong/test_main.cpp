//Host tests of the remote's PONG game [D-157]: pio test -e native -f test_game_pong
#include <unity.h>
#include <HostGfx.h>
#include <GameContract.h>
#include <PongGame.h>
#include <RecordingSink.h>
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

//the sounds [D-157]: a square blip off a paddle, higher the faster the ball; a lower one off the walls; a longer low
//tone for a point, higher when the left player scores
void test_sounds_paddle_rises_with_speed_wall_and_point(){
    PongGame game;
    RecordingSink sink;
    game.setSound(&sink);
    game.reset(11);
    int hits = 0, walls = 0, pointsHeard = 0;
    uint16_t lastPaddle = 0;
    for(int t = 0; t < 50 * 120 && !game.isOver(); t++){
        GameInput input;
        game.demo(input);
        float speed = game.getBallSpeed();
        float vy = game.getBallVelocityY();
        uint8_t won = game.getPoints(0), lost = game.getPoints(1);
        uint32_t plays = sink.count(SoundLayer::FRONT);
        game.update(input);
        const RecordingSink::Play* p = sink.last(SoundLayer::FRONT);
        if(game.isOver()) break;
        if(game.getPoints(0) != won || game.getPoints(1) != lost){
            TEST_ASSERT_NOT_NULL(p);
            TEST_ASSERT_TRUE(p->b_note);
            TEST_ASSERT_TRUE(p->note.kind == SoundNote::Kind::SQUARE);
            TEST_ASSERT_TRUE(p->note.millis >= 200);
            TEST_ASSERT_EQUAL_UINT16(game.getPoints(0) != won ? PongGame::pointWonHertz : PongGame::pointLostHertz, p->note.hertz);
            pointsHeard++;
            lastPaddle = 0;
        }
        else if(game.getBallSpeed() > speed){
            //a wall in the same tick plays first, the paddle's blip last
            TEST_ASSERT_TRUE(sink.count(SoundLayer::FRONT) > plays);
            TEST_ASSERT_TRUE(p->note.kind == SoundNote::Kind::SQUARE);
            TEST_ASSERT_EQUAL_UINT16(PongGame::paddleHertz(game.getBallSpeed()), p->note.hertz);
            TEST_ASSERT_TRUE(p->note.hertz > lastPaddle);
            TEST_ASSERT_TRUE(p->note.hertz > PongGame::wallHertz);
            lastPaddle = p->note.hertz;
            hits++;
        }
        else if(!game.isServing() && game.getBallVelocityY() * vy < 0.0f){
            TEST_ASSERT_EQUAL_UINT32(plays + 1, sink.count(SoundLayer::FRONT));
            TEST_ASSERT_EQUAL_UINT16(PongGame::wallHertz, p->note.hertz);
            walls++;
        }
        else TEST_ASSERT_EQUAL_UINT32(plays, sink.count(SoundLayer::FRONT));
    }
    TEST_ASSERT_TRUE(hits >= 4);
    TEST_ASSERT_TRUE(walls >= 2);
    TEST_ASSERT_TRUE(pointsHeard >= 2);
    //the slowest and the fastest ball an octave apart; no music in Pong
    TEST_ASSERT_EQUAL_UINT16(988, PongGame::paddleHertz(0.0f));
    TEST_ASSERT_EQUAL_UINT16(1976, PongGame::paddleHertz(1000.0f));
    TEST_ASSERT_EQUAL_UINT32(0, sink.count(SoundLayer::BACK));
    game.playMusic();
    TEST_ASSERT_EQUAL_UINT32(0, sink.count(SoundLayer::BACK));
}

//the end: a falling jingle when the left player loses, a rising one when they win, instead of the point's tone, over
//before the Arcade's game over phrase (400 ms)
void test_sounds_win_and_lose_jingles(){
    TEST_ASSERT_TRUE(PongGame::winJingle().lengthMillis() < 400);
    TEST_ASSERT_TRUE(PongGame::loseJingle().lengthMillis() < 400);
    TEST_ASSERT_TRUE(PongGame::winJingle().notes[3].hertz > PongGame::winJingle().notes[0].hertz);
    TEST_ASSERT_TRUE(PongGame::loseJingle().notes[3].hertz < PongGame::loseJingle().notes[0].hertz);

    PongGame game;
    RecordingSink sink;
    game.setSound(&sink);
    game.reset(5);
    GameInput hide = leftStick(1.0f);
    int guard = 0;
    while(!game.isOver() && guard++ < 40) playPoint(game, hide);
    TEST_ASSERT_TRUE(game.isOver() && game.getPoints(1) == 7);
    const RecordingSink::Play* p = sink.last(SoundLayer::FRONT);
    TEST_ASSERT_FALSE(p->b_note);
    TEST_ASSERT_TRUE(p->cue.notes == PongGame::loseJingle().notes);

    //a second player who hides at the top: the left player (the demo) wins
    bool b_won = false;
    for(uint32_t seed = 1; seed < 20 && !b_won; seed++){
        sink.clear();
        game.reset(seed);
        for(int t = 0; t < 50 * 300 && !game.isOver(); t++){
            GameInput input;
            game.demo(input);
            input.stick[3] = 1.0f;
            game.update(input);
        }
        if(!game.isOver() || game.getPoints(0) != 7) continue;
        b_won = true;
        p = sink.last(SoundLayer::FRONT);
        TEST_ASSERT_FALSE(p->b_note);
        TEST_ASSERT_TRUE(p->cue.notes == PongGame::winJingle().notes);
    }
    TEST_ASSERT_TRUE(b_won);
}

//sound never changes the game: the demo plays the same with a sink as without one (silent, the picker's demo)
void test_demo_same_with_and_without_sound(){
    PongGame silent, heard;
    RecordingSink sink;
    heard.setSound(&sink);
    silent.reset(77);
    heard.reset(77);
    for(int t = 0; t < 50 * 90; t++){
        GameInput a, b;
        silent.demo(a);
        heard.demo(b);
        silent.update(a);
        heard.update(b);
        TEST_ASSERT_EQUAL_FLOAT(silent.getBallX(), heard.getBallX());
        TEST_ASSERT_EQUAL_FLOAT(silent.getBallY(), heard.getBallY());
        TEST_ASSERT_EQUAL_FLOAT(silent.getPaddleTop(0), heard.getPaddleTop(0));
        TEST_ASSERT_EQUAL_FLOAT(silent.getPaddleTop(1), heard.getPaddleTop(1));
        if(silent.isOver()) break;
    }
    TEST_ASSERT_EQUAL_UINT8(silent.getPoints(0), heard.getPoints(0));
    TEST_ASSERT_EQUAL_UINT8(silent.getPoints(1), heard.getPoints(1));
    TEST_ASSERT_TRUE(sink.count(SoundLayer::FRONT) > 10);
    //a sink set later hears only what comes after
    RecordingSink late;
    silent.setSound(&late);
    TEST_ASSERT_EQUAL_UINT32(0, late.total());
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
    RUN_TEST(test_sounds_paddle_rises_with_speed_wall_and_point);
    RUN_TEST(test_sounds_win_and_lose_jingles);
    RUN_TEST(test_demo_same_with_and_without_sound);
    return UNITY_END();
}
