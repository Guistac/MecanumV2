//Host tests of the PARKING game [D-157]: pio test -e native -f test_game_parking
#include <unity.h>
#include <math.h>
#include <HostGfx.h>
#include <GameContract.h>
#include <ParkingGame.h>
#include <RecordingSink.h>

void setUp(){}
void tearDown(){}

static GameInput sticks(float leftX, float leftY, float rightX){
    GameInput input;
    input.stick[0] = leftX;
    input.stick[1] = leftY;
    input.stick[2] = rightX;
    return input;
}

static void run(ParkingGame& game, const GameInput& input, int ticks){
    for(int i = 0; i < ticks; i++) game.update(input);
}

//a new game, past the stage's intro, the vehicle stopped at the pose given
static void driving(ParkingGame& game, float x, float y, float headingDeg){
    game.reset(1);
    run(game, GameInput(), ParkingGame::introTicks);
    TEST_ASSERT_TRUE(game.getPhase() == ParkingGame::Phase::DRIVE);
    game.place(x, y, headingDeg * 3.14159265f / 180.0f);
}

void test_contract(){
    GameContract::check<ParkingGame>();
}

void test_sticks_drive_in_body_frame(){
    ParkingGame game;
    //facing right: left stick up drives forward (right on the screen)
    driving(game, 30, 18, 0);
    run(game, sticks(0, 1, 0), 25);
    TEST_ASSERT_TRUE(game.getX() > 35.0f);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 18.0f, game.getY());
    //facing down: the same stick drives down the screen
    driving(game, 30, 16, 90);
    run(game, sticks(0, 1, 0), 15);
    TEST_ASSERT_TRUE(game.getY() > 19.0f);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 30.0f, game.getX());
    //facing right: left stick right strafes to the vehicle's right, down on the screen
    driving(game, 30, 14, 0);
    run(game, sticks(1, 0, 0), 15);
    TEST_ASSERT_TRUE(game.getY() > 17.0f);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 30.0f, game.getX());
    //right stick right turns clockwise on the screen (the heading grows with y down)
    driving(game, 30, 16, 0);
    run(game, sticks(0, 0, 1), 25);
    TEST_ASSERT_TRUE(game.getHeading() > 0.5f);
}

void test_speed_ramps_and_is_limited(){
    ParkingGame game;
    driving(game, 20, 18, 0);
    //the first tick only starts the ramp
    run(game, sticks(0, 1, 0), 1);
    TEST_ASSERT_TRUE(game.getX() - 20.0f < 0.1f);
    //one second at full stick: under 30 px, but more than 30 px/s minus the ramp
    run(game, sticks(0, 1, 0), 49);
    float travelled = game.getX() - 20.0f;
    TEST_ASSERT_TRUE(travelled < 30.0f);
    TEST_ASSERT_TRUE(travelled > 24.0f);
}

void test_impact_costs_two_seconds_once_without_overlap(){
    ParkingGame game;
    //under the flat of stage 1 (x 36..55, y 3..5), strafing up into it at full speed (left stick left = up)
    driving(game, 46, 14, 0);
    uint32_t before = game.getClockTicks();
    run(game, sticks(-1, 0, 0), 50);
    TEST_ASSERT_EQUAL_UINT16(1, game.getPenaltyCount());
    //never inside the flat: it may touch (its last row is 5, the margin makes 5.6) but not overlap
    TEST_ASSERT_TRUE(game.getY() - 3.5f >= 5.6f);
    TEST_ASSERT_TRUE(game.getY() - 3.5f < 6.0f);
    TEST_ASSERT_FALSE(game.overlapsNow());
    TEST_ASSERT_EQUAL_UINT32(before + 50 + ParkingGame::penaltyTicks, game.getClockTicks());
    //pushing on: no more penalty; backing off and driving in again: a second one
    run(game, sticks(-1, 0, 0), 50);
    TEST_ASSERT_EQUAL_UINT16(1, game.getPenaltyCount());
    run(game, sticks(1, 0, 0), 15);
    run(game, sticks(-1, 0, 0), 100);
    TEST_ASSERT_EQUAL_UINT16(2, game.getPenaltyCount());
}

void test_slow_graze_is_free(){
    ParkingGame game;
    //a third of the stick into the flat: 9 px/s, under the bump speed
    driving(game, 46, 14, 0);
    run(game, sticks(-0.3f, 0, 0), 100);
    TEST_ASSERT_EQUAL_UINT16(0, game.getPenaltyCount());
    TEST_ASSERT_TRUE(game.getY() - 3.5f < 6.0f);
    TEST_ASSERT_FALSE(game.overlapsNow());
}

void test_slides_along_a_wall(){
    ParkingGame game;
    //against the flat's underside, driving forward and into it: it slides along, the push into the flat is dropped
    driving(game, 30, 9.15f, 0);
    run(game, sticks(-0.7f, 0.7f, 0), 50);
    TEST_ASSERT_EQUAL_UINT16(0, game.getPenaltyCount());
    TEST_ASSERT_TRUE(game.getX() > 45.0f);
    TEST_ASSERT_FLOAT_WITHIN(0.3f, 9.1f, game.getY());
    TEST_ASSERT_FALSE(game.overlapsNow());
    //turning on the spot against it pushes it away from the flat instead of stopping it
    run(game, sticks(0, 0, 1), 25);
    TEST_ASSERT_TRUE(game.getHeading() > 0.5f);
    TEST_ASSERT_FALSE(game.overlapsNow());
}

void test_screen_edge_is_a_wall(){
    ParkingGame game;
    driving(game, 20, 20, 0);
    run(game, sticks(0, -1, 0), 100);
    TEST_ASSERT_TRUE(game.getX() - 5.5f >= 0.0f);
    TEST_ASSERT_EQUAL_UINT16(1, game.getPenaltyCount());
}

void test_parks_after_one_second_with_the_right_heading(){
    ParkingGame game;
    //stage 1's bay: x 68..84, y 15..25, front to the right; backwards in it does not count
    driving(game, 76, 20, 180);
    run(game, GameInput(), 100);
    TEST_ASSERT_TRUE(game.getPhase() == ParkingGame::Phase::DRIVE);
    //nose to the marked side, 8 degrees off: counts after 1 s
    game.place(76, 20, 8.0f * 3.14159265f / 180.0f);
    run(game, GameInput(), ParkingGame::parkedHoldTicks - 1);
    TEST_ASSERT_TRUE(game.getPhase() == ParkingGame::Phase::DRIVE);
    run(game, GameInput(), 1);
    TEST_ASSERT_TRUE(game.getPhase() == ParkingGame::Phase::PARKED);
    //the flash, then the next stage (the clock stands still meanwhile)
    uint32_t clock = game.getClockTicks();
    run(game, GameInput(), ParkingGame::flashTicks);
    TEST_ASSERT_EQUAL_UINT8(1, game.getLevel());
    TEST_ASSERT_TRUE(game.getPhase() == ParkingGame::Phase::INTRO);
    TEST_ASSERT_EQUAL_UINT32(clock, game.getClockTicks());
    //half out of the bay does not count
    run(game, GameInput(), ParkingGame::introTicks);
    game.place(77, 25, -3.14159265f / 2.0f);
    run(game, GameInput(), 100);
    TEST_ASSERT_TRUE(game.getPhase() == ParkingGame::Phase::DRIVE);
}

void test_moving_flat_pushes_never_crushes(){
    ParkingGame game;
    //stage 5: a flat slides between rows 0..19 and 12..31 at x 58..61
    game.reset(1);
    game.startStage(4);
    run(game, GameInput(), ParkingGame::introTicks);
    TEST_ASSERT_EQUAL_UINT8(1, game.getMoverCount());
    //wait until the flat is up, then park the vehicle under it
    for(int i = 0; i < 400; i++){
        ParkingGame::Obb flat = game.getMover(0);
        if(flat.cy - flat.hy < 0.5f) break;
        run(game, GameInput(), 1);
    }
    game.place(60, 24, 0);
    TEST_ASSERT_FALSE(game.overlapsNow());
    float lowest = 0;
    for(int i = 0; i < 400; i++){
        run(game, GameInput(), 1);
        TEST_ASSERT_FALSE(game.overlapsNow());
        ParkingGame::Obb flat = game.getMover(0);
        if(flat.cy + flat.hy > lowest) lowest = flat.cy + flat.hy;
    }
    //pushed down to the stage edge (within a step of the flat), where the flat stops above it; standing still is
    //never a bump
    TEST_ASSERT_TRUE(game.getY() > 26.5f && game.getY() <= 27.5f);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 60.0f, game.getX());
    TEST_ASSERT_TRUE(lowest <= game.getY() - 3.5f - 0.6f);
    TEST_ASSERT_EQUAL_UINT16(0, game.getPenaltyCount());
    //driven away, the flat goes on down to its end
    game.place(30, 24, 0);
    for(int i = 0; i < 400; i++){
        run(game, GameInput(), 1);
        ParkingGame::Obb flat = game.getMover(0);
        if(flat.cy + flat.hy > lowest) lowest = flat.cy + flat.hy;
    }
    TEST_ASSERT_EQUAL_FLOAT(31.0f, lowest);
}

void test_every_stage_starts_free(){
    ParkingGame game;
    game.reset(9);
    for(uint8_t stage = 0; stage < ParkingGame::levelCount; stage++){
        game.startStage(stage);
        for(int i = 0; i < ParkingGame::introTicks; i++){
            run(game, GameInput(), 1);
            TEST_ASSERT_FALSE(game.overlapsNow());
        }
        TEST_ASSERT_TRUE(game.getPhase() == ParkingGame::Phase::DRIVE);
        //the moving pieces start from the middle stages on
        TEST_ASSERT_EQUAL_UINT8(stage == 4 || stage == 8 || stage == 9 ? 1 : (stage == 6 || stage >= 10 ? 2 : 0), game.getMoverCount());
    }
}

void test_time_limit_ends_the_game_and_unparked_stages_cost(){
    ParkingGame game;
    game.reset(3);
    for(int i = 0; i < 20000 && !game.isOver(); i++) game.update(GameInput());
    TEST_ASSERT_TRUE(game.isOver());
    //5 min plus 60 s for each of the twelve stages
    TEST_ASSERT_EQUAL_UINT32(3000 + 12 * 600, game.score());
    char text[10];
    game.scoreText(text, sizeof(text), 483);
    TEST_ASSERT_EQUAL_STRING("48.3s", text);
}

void test_demo_parks_all_twelve_stages_fairly(){
    for(uint32_t seed = 1; seed <= 5; seed++){
        ParkingGame game;
        game.reset(seed);
        int ticks = 0;
        for(; ticks < 9000 && !game.isOver(); ticks++){
            TEST_ASSERT_FALSE(game.overlapsNow());
            GameInput input;
            game.demo(input);
            //the demo player uses only the sticks a player has, within their range
            for(int i = 0; i < 4; i++) TEST_ASSERT_TRUE(fabsf(input.stick[i]) <= 1.0f);
            TEST_ASSERT_FALSE(input.held[0] || input.held[1] || input.clicked[0] || input.clicked[1]);
            game.update(input);
        }
        TEST_ASSERT_TRUE(game.isOver());
        //well inside the 3 minutes of the contract, waiting for the moving pieces included
        TEST_ASSERT_TRUE(ticks < 130 * 50);
        //all twelve parked (no unparked stage in the score), like a skilled operator: at most one bump
        TEST_ASSERT_EQUAL_UINT32(game.getClockTicks() / 5, game.score());
        TEST_ASSERT_TRUE(game.getPenaltyCount() <= 1);
    }
}

//sound [D-157]: the motor whines while the vehicle moves, higher and louder the faster; nothing when it stands still
void test_motor_whine_follows_the_speed_and_stops_when_still(){
    ParkingGame game;
    RecordingSink sink;
    game.setSound(&sink);
    driving(game, 20, 20, 0);
    sink.clear();
    run(game, GameInput(), 50);
    TEST_ASSERT_EQUAL_UINT32(0, sink.total());
    //slowly: grains every other tick, synth notes on the front layer only
    run(game, sticks(0, 0.3f, 0), 25);
    sink.clear();
    run(game, sticks(0, 0.3f, 0), 20);
    TEST_ASSERT_EQUAL_UINT32(10, sink.noteCount(SoundLayer::FRONT));
    TEST_ASSERT_EQUAL_UINT32(0, sink.count(SoundLayer::BACK));
    const RecordingSink::Play* slow = sink.last(SoundLayer::FRONT);
    TEST_ASSERT_TRUE(slow->note.kind == SoundNote::Kind::SYNTH);
    TEST_ASSERT_EQUAL_UINT8(SoundPriority::GAME, slow->priority);
    TEST_ASSERT_TRUE(slow->note.millis < 40);       //shorter than its period
    uint16_t slowHertz = slow->note.hertz;
    uint8_t slowLevel = slow->note.levelPercent;
    //fast: higher and louder
    game.place(20, 20, 0);
    run(game, sticks(0, 1, 0), 15);
    uint16_t fastHertz = sink.lastHertz(SoundLayer::FRONT);
    TEST_ASSERT_TRUE(fastHertz > slowHertz + 300);
    TEST_ASSERT_TRUE(sink.last(SoundLayer::FRONT)->note.levelPercent > slowLevel);
    //a one-shot effect playing: the grain waits
    sink.setPlaying(SoundLayer::FRONT, true);
    uint32_t before = sink.total();
    run(game, sticks(0, 1, 0), 4);
    TEST_ASSERT_EQUAL_UINT32(before, sink.total());
    sink.setPlaying(SoundLayer::FRONT, false);
    //let go: it ramps down to a stop, then silence
    run(game, GameInput(), 30);
    before = sink.total();
    run(game, GameInput(), 50);
    TEST_ASSERT_EQUAL_UINT32(before, sink.total());
}

void test_sliding_along_a_wall_scrapes(){
    ParkingGame game;
    RecordingSink sink;
    game.setSound(&sink);
    driving(game, 30, 9.15f, 0);
    run(game, sticks(-0.7f, 0.7f, 0), 30);
    sink.clear();
    run(game, sticks(-0.7f, 0.7f, 0), 10);
    TEST_ASSERT_EQUAL_UINT16(0, game.getPenaltyCount());
    //a noise grain every tick, softer than a square grind (synth, under full level)
    TEST_ASSERT_EQUAL_UINT32(10, sink.noteCount(SoundLayer::FRONT));
    const RecordingSink::Play* p = sink.last(SoundLayer::FRONT);
    TEST_ASSERT_TRUE(p->note.kind == SoundNote::Kind::SYNTH);
    TEST_ASSERT_TRUE(p->note.wave == Synth::Wave::NOISE);
    TEST_ASSERT_TRUE(p->note.levelPercent < 80);
    TEST_ASSERT_TRUE(p->note.millis < 20);
    //driving free again: the whine, no scrape
    game.place(30, 20, 0);
    run(game, sticks(0, 0.7f, 0), 10);
    TEST_ASSERT_TRUE(sink.last(SoundLayer::FRONT)->note.wave != Synth::Wave::NOISE);
}

void test_impact_crunches_once(){
    ParkingGame game;
    RecordingSink sink;
    game.setSound(&sink);
    driving(game, 46, 14, 0);
    run(game, sticks(-1, 0, 0), 50);
    TEST_ASSERT_EQUAL_UINT16(1, game.getPenaltyCount());
    TEST_ASSERT_EQUAL_UINT32(1, sink.cueCount(SoundLayer::FRONT));
    //the crunch: noise and a low buzz
    const SoundCue& crunch = game.impactSound();
    bool b_noise = false, b_low = false;
    for(uint16_t i = 0; i < crunch.noteCount; i++){
        if(crunch.notes[i].wave == Synth::Wave::NOISE) b_noise = true;
        else if(crunch.notes[i].hertz < 200) b_low = true;
    }
    TEST_ASSERT_TRUE(b_noise && b_low);
    TEST_ASSERT_TRUE(crunch.lengthMillis() <= 250);
    bool b_heard = false;
    for(uint8_t back = 0; back < RecordingSink::keep; back++){
        const RecordingSink::Play* p = sink.last(SoundLayer::FRONT, back);
        if(p && !p->b_note && p->cue.notes == crunch.notes) b_heard = true;
    }
    TEST_ASSERT_TRUE(b_heard);
    //pushing on: no second crunch
    run(game, sticks(-1, 0, 0), 50);
    TEST_ASSERT_EQUAL_UINT32(1, sink.cueCount(SoundLayer::FRONT));
}

static bool cueHeard(const RecordingSink& sink, const SoundCue& cue){
    for(uint8_t back = 0; back < RecordingSink::keep; back++){
        const RecordingSink::Play* p = sink.last(SoundLayer::FRONT, back);
        if(p && !p->b_note && p->cue.notes == cue.notes) return true;
    }
    return false;
}

void test_parked_chimes_and_the_last_stage_plays_the_fanfare(){
    ParkingGame game;
    RecordingSink sink;
    game.setSound(&sink);
    driving(game, 76, 20, 0);
    sink.clear();
    run(game, GameInput(), ParkingGame::parkedHoldTicks);
    TEST_ASSERT_TRUE(game.getPhase() == ParkingGame::Phase::PARKED);
    TEST_ASSERT_EQUAL_UINT32(1, sink.total());
    TEST_ASSERT_TRUE(cueHeard(sink, game.chimeSound()));
    TEST_ASSERT_FALSE(cueHeard(sink, game.fanfareSound()));
    //the flash and the next intro are quiet
    run(game, GameInput(), ParkingGame::flashTicks + ParkingGame::introTicks);
    TEST_ASSERT_EQUAL_UINT32(1, sink.total());

    //a whole game by the demo player: a chime for each of the first eleven stages, the fanfare for the twelfth,
    //over within about a second
    ParkingGame full;
    full.setSound(&sink);
    full.reset(4);
    sink.clear();
    uint32_t chimes = 0;
    bool b_fanfare = false;
    for(int t = 0; t < 9000 && !full.isOver(); t++){
        GameInput input;
        full.demo(input);
        ParkingGame::Phase before = full.getPhase();
        uint32_t cues = sink.cueCount(SoundLayer::FRONT);
        full.update(input);
        if(before == ParkingGame::Phase::DRIVE && full.getPhase() == ParkingGame::Phase::PARKED){
            TEST_ASSERT_EQUAL_UINT32(cues + 1, sink.cueCount(SoundLayer::FRONT));
            const RecordingSink::Play* p = sink.last(SoundLayer::FRONT);
            if(p->cue.notes == full.chimeSound().notes) chimes++;
            if(p->cue.notes == full.fanfareSound().notes) b_fanfare = true;
        }
    }
    TEST_ASSERT_TRUE(full.isOver());
    TEST_ASSERT_EQUAL_UINT32(ParkingGame::levelCount - 1, chimes);
    TEST_ASSERT_TRUE(b_fanfare);
    TEST_ASSERT_TRUE(full.fanfareSound().lengthMillis() <= 1200);
    TEST_ASSERT_EQUAL_UINT32(0, sink.count(SoundLayer::BACK));
}

//sound never changes the game: the demo plays the same with a sink and without; without one nothing is heard
void test_sound_leaves_the_demo_deterministic_and_silent_without_a_sink(){
    ParkingGame quiet, loud;
    RecordingSink sink, detached;
    loud.setSound(&sink);
    quiet.setSound(&detached);
    quiet.setSound(nullptr);
    quiet.reset(9);
    loud.reset(9);
    for(int t = 0; t < 4000 && !quiet.isOver(); t++){
        GameInput a, b;
        quiet.demo(a);
        loud.demo(b);
        quiet.update(a);
        loud.update(b);
        TEST_ASSERT_EQUAL_FLOAT(quiet.getX(), loud.getX());
        TEST_ASSERT_EQUAL_FLOAT(quiet.getY(), loud.getY());
        TEST_ASSERT_EQUAL_FLOAT(quiet.getHeading(), loud.getHeading());
    }
    TEST_ASSERT_EQUAL_UINT32(quiet.getClockTicks(), loud.getClockTicks());
    TEST_ASSERT_EQUAL_UINT8(quiet.getLevel(), loud.getLevel());
    TEST_ASSERT_TRUE(sink.total() > 100);
    TEST_ASSERT_EQUAL_UINT32(0, detached.total());
}

int main(int argc, char** argv){
    UNITY_BEGIN();
    RUN_TEST(test_contract);
    RUN_TEST(test_sticks_drive_in_body_frame);
    RUN_TEST(test_speed_ramps_and_is_limited);
    RUN_TEST(test_impact_costs_two_seconds_once_without_overlap);
    RUN_TEST(test_slow_graze_is_free);
    RUN_TEST(test_slides_along_a_wall);
    RUN_TEST(test_screen_edge_is_a_wall);
    RUN_TEST(test_parks_after_one_second_with_the_right_heading);
    RUN_TEST(test_moving_flat_pushes_never_crushes);
    RUN_TEST(test_every_stage_starts_free);
    RUN_TEST(test_time_limit_ends_the_game_and_unparked_stages_cost);
    RUN_TEST(test_demo_parks_all_twelve_stages_fairly);
    RUN_TEST(test_motor_whine_follows_the_speed_and_stops_when_still);
    RUN_TEST(test_sliding_along_a_wall_scrapes);
    RUN_TEST(test_impact_crunches_once);
    RUN_TEST(test_parked_chimes_and_the_last_stage_plays_the_fanfare);
    RUN_TEST(test_sound_leaves_the_demo_deterministic_and_silent_without_a_sink);
    return UNITY_END();
}
