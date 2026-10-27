//Host tests of the CAVE game [D-148]: pio test -e native -f test_game_cave
#include <unity.h>
#include <HostGfx.h>
#include <GameContract.h>
#include <CaveGame.h>

void setUp(){}
void tearDown(){}

static void run(CaveGame& game, const GameInput& input, int ticks){
    for(int i = 0; i < ticks && !game.isOver(); i++) game.update(input);
}

void test_contract(){
    GameContract::check<CaveGame>();
}

void test_controls_map_to_thrust(){
    GameInput input;
    TEST_ASSERT_EQUAL_FLOAT(0.0f, CaveGame::thrustFrom(input));
    input.held[1] = true;
    TEST_ASSERT_EQUAL_FLOAT(1.0f, CaveGame::thrustFrom(input));
    input.held[1] = false;
    input.held[0] = true;
    TEST_ASSERT_EQUAL_FLOAT(1.0f, CaveGame::thrustFrom(input));
    input.held[0] = false;
    input.stick[1] = 0.5f;
    TEST_ASSERT_EQUAL_FLOAT(0.5f, CaveGame::thrustFrom(input));
    input.stick[1] = 0.0f;
    input.stick[3] = 1.0f;
    TEST_ASSERT_EQUAL_FLOAT(1.0f, CaveGame::thrustFrom(input));
    //a stick pushed down, or sideways, does nothing
    input.stick[3] = -1.0f;
    input.stick[0] = 1.0f;
    TEST_ASSERT_EQUAL_FLOAT(0.0f, CaveGame::thrustFrom(input));
}

//a click ends the ready phase: the game flies from the next tick
static void start(CaveGame& game){
    GameInput click;
    click.clicked[0] = true;
    game.update(click);
    TEST_ASSERT_FALSE(game.isReady());
}

void test_gravity_and_thrust_have_momentum(){
    CaveGame game;
    game.reset(3);
    start(game);
    float start = game.getCraftY();
    GameInput none;
    run(game, none, 30);
    TEST_ASSERT_TRUE(game.getCraftY() > start);           //sinks
    TEST_ASSERT_TRUE(game.getVerticalSpeed() > 0.0f);
    GameInput up;
    up.held[0] = true;
    game.update(up);
    //one tick of thrust slows the fall, it does not reverse it at once
    TEST_ASSERT_TRUE(game.getVerticalSpeed() > 0.0f);
    run(game, up, 25);
    TEST_ASSERT_TRUE(game.getVerticalSpeed() < 0.0f);     //climbs
    TEST_ASSERT_TRUE(game.getVerticalSpeed() >= -CaveGame::maxClimbSpeed);
}

void test_falling_crashes_then_game_over_after_the_debris(){
    CaveGame game;
    game.reset(11);
    GameInput none;
    int ticks = 0;
    while(!game.isCrashed() && ticks < 500){
        game.update(none);
        ticks++;
    }
    TEST_ASSERT_TRUE(game.isCrashed());
    TEST_ASSERT_TRUE(game.getCraftY() > 16.0f);           //on the floor, not the ceiling
    TEST_ASSERT_FALSE(game.isOver());
    uint32_t score = game.score();
    run(game, none, CaveGame::crashShowTicks - 1);
    TEST_ASSERT_FALSE(game.isOver());
    game.update(none);
    TEST_ASSERT_TRUE(game.isOver());
    TEST_ASSERT_EQUAL_UINT32(score, game.score());         //the cave stops at the crash
}

void test_holding_up_crashes_into_the_ceiling(){
    CaveGame game;
    game.reset(12);
    GameInput up;
    up.held[1] = true;
    run(game, up, 200);
    TEST_ASSERT_TRUE(game.isCrashed());
    TEST_ASSERT_TRUE(game.getCraftY() < 8.0f);
}

void test_score_is_distance_in_metres(){
    CaveGame game;
    game.reset(5);
    TEST_ASSERT_EQUAL_UINT32(0, game.score());
    //a second in the wide, level start of the cave at 30 px/s is about 7 m
    //the demo waits half a second in the ready phase, the cave stands still meanwhile
    int t = 0;
    while(game.isReady() && t < 200){
        GameInput input;
        game.demo(input);
        game.update(input);
        t++;
    }
    TEST_ASSERT_EQUAL_UINT32(0, game.score());
    for(t = 0; t < 50; t++){
        GameInput input;
        game.demo(input);
        game.update(input);
    }
    TEST_ASSERT_FALSE(game.isCrashed());
    TEST_ASSERT_UINT32_WITHIN(1, 7, game.score());
    char text[10];
    game.scoreText(text, sizeof(text), 1234);
    TEST_ASSERT_EQUAL_STRING("1234m", text);
    game.scoreText(text, sizeof(text), 99999);
    TEST_ASSERT_TRUE(strlen(text) <= 9);
}

//every column of the cave leaves a way through, however far in: the narrowest gap and a pillar's open side
void test_cave_always_leaves_room(){
    for(uint32_t seed = 1; seed <= 4; seed++){
        CaveGame game;
        game.reset(seed);
        GameInput input;
        int narrowest = 32;
        for(int t = 0; t < 50 * 120 && !game.isCrashed(); t++){
            game.demo(input);
            game.update(input);
            input = GameInput();
            int run = 0, best = 0;
            for(int16_t y = 0; y < CaveGame::height; y++){
                run = game.isSolid(CaveGame::width - 1, y) ? 0 : run + 1;
                if(run > best) best = run;
            }
            if(best < narrowest) narrowest = best;
            TEST_ASSERT_TRUE(game.ceilingAt(CaveGame::width - 1) >= 1);
            TEST_ASSERT_TRUE(game.floorAt(CaveGame::width - 1) <= CaveGame::height - 1);
        }
        TEST_ASSERT_TRUE(narrowest >= 9);
    }
}

//a new game waits for the player: no input for 2 s, the craft hovers in the middle with the hint showing and the
//cave still; after 3 s it flies on its own and sinks slowly, a resting stick is not a drop
void test_ready_phase_waits_for_the_player(){
    CaveGame game;
    game.reset(7);
    GameInput none;
    GFXcanvas1 canvas(128, 32);
    for(int t = 0; t < 100; t++){
        game.update(none);
        TEST_ASSERT_TRUE(game.isReady());
        TEST_ASSERT_FALSE(game.isCrashed());
        TEST_ASSERT_FLOAT_WITHIN(1.01f, 14.0f, game.getCraftY());
    }
    TEST_ASSERT_EQUAL_UINT32(0, game.score());
    //the hint is drawn right of the craft, with a dark column between them
    canvas.fillScreen(0);
    game.draw(canvas);
    bool b_hint = false;
    for(int x = 50; x < 128; x++) if(canvas.getPixel(x, 16)) b_hint = true;
    TEST_ASSERT_TRUE(b_hint);
    for(int y = 0; y < 32; y++) TEST_ASSERT_FALSE(canvas.getPixel(CaveGame::craftX + CaveGame::craftWidth + 1, y) && y > 8 && y < 24);

    run(game, none, 50);
    TEST_ASSERT_FALSE(game.isReady());
    float y = game.getCraftY();
    run(game, none, 25);
    TEST_ASSERT_FALSE(game.isCrashed());
    TEST_ASSERT_TRUE(game.getCraftY() > y);
    TEST_ASSERT_TRUE(game.getCraftY() - y < 3.0f);         //half a second of sinking: under 3 px
    canvas.fillScreen(0);
    game.draw(canvas);
    for(int x = 50; x < 128; x++) TEST_ASSERT_FALSE(canvas.getPixel(x, 16) && game.isSolid(int16_t(x), 16) == false);
}

//the stick starts the game only when pushed up clearly; a stick button does at once
void test_ready_phase_starts_on_click_or_stick_up(){
    CaveGame game;
    game.reset(8);
    GameInput input;
    input.stick[3] = 0.2f;
    run(game, input, 10);
    TEST_ASSERT_TRUE(game.isReady());
    input.stick[3] = -1.0f;
    run(game, input, 10);
    TEST_ASSERT_TRUE(game.isReady());
    input.stick[3] = 0.0f;
    input.stick[1] = 0.4f;
    game.update(input);
    TEST_ASSERT_FALSE(game.isReady());

    game.reset(8);
    GameInput held;
    held.held[1] = true;
    game.update(held);
    TEST_ASSERT_FALSE(game.isReady());
}

//full thrust held from the very start (stick or button): it takes about a second to reach the ceiling
void test_full_thrust_from_the_start_takes_a_second_to_the_ceiling(){
    for(int way = 0; way < 2; way++){
        CaveGame game;
        game.reset(12);
        GameInput up;
        if(way == 0) up.held[0] = true;
        else up.stick[1] = 1.0f;
        int ticks = 0;
        while(!game.isCrashed() && ticks < 500){
            game.update(up);
            ticks++;
        }
        TEST_ASSERT_TRUE(game.isCrashed());
        TEST_ASSERT_TRUE(game.getCraftY() < 8.0f);
        TEST_ASSERT_TRUE(ticks >= 48);
    }
}

//the demo only holds a stick button (never both, never the sticks) and flies well before its skill gives out
void test_demo_plays_fair_and_well_at_first(){
    for(uint32_t seed = 20; seed < 26; seed++){
        CaveGame game;
        game.reset(seed);
        for(int t = 0; t < 50 * 12; t++){
            GameInput input;
            game.demo(input);
            TEST_ASSERT_FALSE(input.held[1]);
            TEST_ASSERT_FALSE(input.clicked[1]);
            for(int i = 0; i < 4; i++) TEST_ASSERT_EQUAL_FLOAT(0.0f, input.stick[i]);
            game.update(input);
        }
        TEST_ASSERT_FALSE(game.isCrashed());
    }
}

int main(){
    UNITY_BEGIN();
    RUN_TEST(test_contract);
    RUN_TEST(test_controls_map_to_thrust);
    RUN_TEST(test_gravity_and_thrust_have_momentum);
    RUN_TEST(test_falling_crashes_then_game_over_after_the_debris);
    RUN_TEST(test_holding_up_crashes_into_the_ceiling);
    RUN_TEST(test_score_is_distance_in_metres);
    RUN_TEST(test_cave_always_leaves_room);
    RUN_TEST(test_ready_phase_waits_for_the_player);
    RUN_TEST(test_ready_phase_starts_on_click_or_stick_up);
    RUN_TEST(test_full_thrust_from_the_start_takes_a_second_to_the_ceiling);
    RUN_TEST(test_demo_plays_fair_and_well_at_first);
    return UNITY_END();
}
