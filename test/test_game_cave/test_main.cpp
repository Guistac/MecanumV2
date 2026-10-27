//Host tests of the CAVE game [D-157]: pio test -e native -f test_game_cave
#include <unity.h>
#include <HostGfx.h>
#include <GameContract.h>
#include <CaveGame.h>
#include <RecordingSink.h>

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

//sound [D-157]: the rotor chops while the craft hovers and flies, faster with more thrust, noise ticks on the front
//layer that yield to a one-shot effect
void test_rotor_chop_rate_follows_the_thrust(){
    CaveGame game;
    RecordingSink sink;
    game.setSound(&sink);
    game.reset(3);
    //hovering in the ready phase: chops at the hover's rate
    run(game, GameInput(), 50);
    TEST_ASSERT_TRUE(game.isReady());
    uint32_t hover = sink.noteCount(SoundLayer::FRONT);
    TEST_ASSERT_TRUE(hover >= 14 && hover <= 17);
    const RecordingSink::Play* p = sink.last(SoundLayer::FRONT);
    TEST_ASSERT_TRUE(p->b_note);
    TEST_ASSERT_TRUE(p->note.wave == Synth::Wave::NOISE);
    TEST_ASSERT_EQUAL_UINT8(SoundPriority::GAME, p->priority);
    TEST_ASSERT_TRUE(p->note.millis < 20);
    TEST_ASSERT_EQUAL_UINT32(0, sink.count(SoundLayer::BACK));

    //flying 0.8 s with a little thrust, then a new game with full thrust: about twice the chops
    GameInput little;
    little.stick[1] = 0.35f;
    GameInput full;
    full.held[0] = true;
    uint32_t counts[2];
    const GameInput* inputs[2] = {&little, &full};
    for(int i = 0; i < 2; i++){
        game.reset(3);
        start(game);
        sink.clear();
        run(game, *inputs[i], 40);
        TEST_ASSERT_FALSE(game.isCrashed());
        counts[i] = sink.noteCount(SoundLayer::FRONT);
    }
    TEST_ASSERT_TRUE(counts[0] >= 11 && counts[0] <= 13);
    TEST_ASSERT_TRUE(counts[1] >= 19 && counts[1] <= 21);
    TEST_ASSERT_TRUE(CaveGame::rotorRate(1.0f) > CaveGame::rotorRate(0.0f));
    //a one-shot effect playing: the chop waits
    sink.setPlaying(SoundLayer::FRONT, true);
    uint32_t before = sink.total();
    run(game, little, 10);
    TEST_ASSERT_EQUAL_UINT32(before, sink.total());
}

//a ping at 100 m, 200 m ...; the crash: a burst of noise and a falling whistle, and the rotor stops
void test_ping_every_hundred_metres_and_the_crash(){
    CaveGame game;
    RecordingSink sink;
    game.setSound(&sink);
    game.reset(1);
    uint32_t pings = 0;
    bool b_crashSound = false;
    for(int t = 0; t < 20000 && !game.isOver(); t++){
        GameInput input;
        game.demo(input);
        uint32_t metres = game.score();
        uint32_t cues = sink.cueCount(SoundLayer::FRONT);
        bool b_wasCrashed = game.isCrashed();
        game.update(input);
        uint32_t played = sink.cueCount(SoundLayer::FRONT) - cues;
        if(!b_wasCrashed && game.isCrashed()){
            TEST_ASSERT_EQUAL_UINT32(1, played);
            TEST_ASSERT_TRUE(sink.last(SoundLayer::FRONT)->cue.notes == game.crashSound().notes);
            b_crashSound = true;
        }
        else if(game.score() / 100 > metres / 100){
            TEST_ASSERT_EQUAL_UINT32(1, played);
            //the ping is played first; the rotor's chop of that tick may follow (it yields on the board)
            bool b_ping = false;
            for(uint8_t back = 0; back < 2; back++){
                const RecordingSink::Play* p = sink.last(SoundLayer::FRONT, back);
                if(p && !p->b_note && p->cue.notes == game.pingSound().notes) b_ping = true;
            }
            TEST_ASSERT_TRUE(b_ping);
            pings++;
        }
        else TEST_ASSERT_EQUAL_UINT32(0, played);
    }
    TEST_ASSERT_TRUE(game.isOver());
    TEST_ASSERT_TRUE(b_crashSound);
    TEST_ASSERT_TRUE(game.score() >= 100);
    TEST_ASSERT_EQUAL_UINT32(game.score() / 100, pings);
    //after the crash, nothing more (the debris falls in silence)
    uint32_t before = sink.total();
    run(game, GameInput(), 50);
    TEST_ASSERT_EQUAL_UINT32(before, sink.total());

    //the crash's sound: noise, and a whistle that falls
    const SoundCue& crash = game.crashSound();
    bool b_noise = false;
    uint16_t firstWhistle = 0, lastWhistle = 0;
    for(uint16_t i = 0; i < crash.noteCount; i++){
        if(crash.notes[i].wave == Synth::Wave::NOISE) b_noise = true;
        else{
            if(!firstWhistle) firstWhistle = crash.notes[i].hertz;
            lastWhistle = crash.notes[i].hertz;
        }
    }
    TEST_ASSERT_TRUE(b_noise);
    TEST_ASSERT_TRUE(firstWhistle > 4 * lastWhistle);
}

//sound never changes the game: the demo plays the same with a sink and without; without one nothing is heard
void test_sound_leaves_the_demo_deterministic_and_silent_without_a_sink(){
    CaveGame quiet, loud;
    RecordingSink sink, detached;
    loud.setSound(&sink);
    quiet.setSound(&detached);
    quiet.setSound(nullptr);
    for(uint32_t seed = 1; seed <= 3; seed++){
        quiet.reset(seed);
        loud.reset(seed);
        for(int t = 0; t < 6000 && !quiet.isOver(); t++){
            GameInput a, b;
            quiet.demo(a);
            loud.demo(b);
            quiet.update(a);
            loud.update(b);
            TEST_ASSERT_EQUAL_FLOAT(quiet.getCraftY(), loud.getCraftY());
            TEST_ASSERT_EQUAL_UINT32(quiet.score(), loud.score());
        }
        TEST_ASSERT_TRUE(quiet.isOver() == loud.isOver());
    }
    TEST_ASSERT_TRUE(sink.total() > 100);
    TEST_ASSERT_EQUAL_UINT32(0, detached.total());
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
    RUN_TEST(test_rotor_chop_rate_follows_the_thrust);
    RUN_TEST(test_ping_every_hundred_metres_and_the_crash);
    RUN_TEST(test_sound_leaves_the_demo_deterministic_and_silent_without_a_sink);
    return UNITY_END();
}
