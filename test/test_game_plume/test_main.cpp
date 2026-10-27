//Host tests of the PLUME game [D-157]: pio test -e native -f test_game_plume
#include <unity.h>
#include <string.h>
#include <HostGfx.h>
#include <GameContract.h>
#include <RecordingSink.h>
#include <SoundKit.h>
#include <PlumeGame.h>
#include <Arcade.h>

void setUp(){}
void tearDown(){}

static void run(PlumeGame& game, const GameInput& input, int ticks){
    for(int i = 0; i < ticks && !game.isOver(); i++) game.update(input);
}

static GameInput stick(float x, float y, bool b_right = false){
    GameInput input;
    input.stick[b_right ? 2 : 0] = x;
    input.stick[b_right ? 3 : 1] = y;
    return input;
}

//an empty stretch of shaft around a world column (the generator may have put rock there)
static void clearAround(PlumeGame& game, int32_t column, int32_t span = 12){
    for(int32_t c = column - span; c <= column + span; c++) for(int16_t r = 0; r < 32; r++) game.setRock(c, r, false);
}

//a wall of rock across the whole shaft at a world column
static void wall(PlumeGame& game, int32_t column, int16_t from = 0, int16_t to = 31){
    for(int16_t r = from; r <= to; r++) game.setRock(column, r, true);
}

void test_contract(){
    GameContract::check<PlumeGame>();
}

void test_name_score_text_and_help(){
    PlumeGame game;
    TEST_ASSERT_EQUAL_STRING("PLUME", game.name());
    char text[16];
    game.scoreText(text, sizeof(text), 1450);
    TEST_ASSERT_EQUAL_STRING("1450", text);
    //the game over screen: the points and the altitude
    game.reset(1);
    game.place(300.0f, 14.0f);
    game.overText(text, sizeof(text), 1450);
    TEST_ASSERT_EQUAL_STRING("PTS 1450 28M", text);
    TEST_ASSERT_FALSE(game.lowerScoreIsBetter());
    const char* lines[3];
    game.helpLines(lines);
    for(int i = 0; i < 3; i++){
        TEST_ASSERT_NOT_NULL(lines[i]);
        TEST_ASSERT_TRUE(strlen(lines[i]) <= 21);
        for(const char* c = lines[i]; *c; c++) TEST_ASSERT_FALSE(*c >= 'a' && *c <= 'z');
    }
}

void test_seventh_game_of_the_arcade_in_slot_6(){
    Arcade arcade;
    TEST_ASSERT_EQUAL_UINT8(7, Arcade::gameCount);
    TEST_ASSERT_EQUAL_STRING("PLUME", arcade.game(6).name());
    TEST_ASSERT_EQUAL_UINT8(6, arcade.slotOf(6));
}

void test_stick_gives_direction_and_thrust(){
    float dx, dy;
    GameInput none;
    TEST_ASSERT_EQUAL_FLOAT(0.0f, PlumeGame::thrustFrom(none, dx, dy));
    //inside the deadzone: nothing
    TEST_ASSERT_EQUAL_FLOAT(0.0f, PlumeGame::thrustFrom(stick(0.1f, 0.0f), dx, dy));
    //the stick aims the plume (owner, 2026-10-27), the thrust is the opposite way: right sprays towards screen right
    //and pushes the canister left (down the shaft); left climbs; up sprays towards row 0, pushing towards row 31
    TEST_ASSERT_EQUAL_FLOAT(1.0f, PlumeGame::thrustFrom(stick(1.0f, 0.0f), dx, dy));
    TEST_ASSERT_EQUAL_FLOAT(-1.0f, dx);
    TEST_ASSERT_EQUAL_FLOAT(0.0f, dy);
    TEST_ASSERT_EQUAL_FLOAT(1.0f, PlumeGame::thrustFrom(stick(-1.0f, 0.0f), dx, dy));
    TEST_ASSERT_EQUAL_FLOAT(1.0f, dx);
    TEST_ASSERT_EQUAL_FLOAT(1.0f, PlumeGame::thrustFrom(stick(0.0f, 1.0f), dx, dy));
    TEST_ASSERT_EQUAL_FLOAT(1.0f, dy);
    //proportional after the deadzone
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.5f, PlumeGame::thrustFrom(stick(0.56f, 0.0f), dx, dy));
    //the stick pushed further wins, the two are never added
    GameInput both = stick(0.5f, 0.0f);
    both.stick[2] = 0.0f;
    both.stick[3] = -0.9f;
    float t = PlumeGame::thrustFrom(both, dx, dy);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, (0.9f - 0.12f) / 0.88f, t);
    TEST_ASSERT_EQUAL_FLOAT(-1.0f, dy);
    TEST_ASSERT_EQUAL_FLOAT(0.0f, dx);
    GameInput same = stick(1.0f, 0.0f);
    same.stick[2] = 1.0f;
    TEST_ASSERT_EQUAL_FLOAT(1.0f, PlumeGame::thrustFrom(same, dx, dy));
}

void test_physics_step_at_50_hz(){
    PlumeGame game;
    game.reset(1);
    GameInput none;
    game.update(none);
    //one tick: gravity, then drag, per second values stepped with 0.02 s
    float v = -PlumeGame::gravity * Game::tickSeconds;
    v -= v * PlumeGame::drag * Game::tickSeconds;
    TEST_ASSERT_FLOAT_WITHIN(0.001f, v, game.getSpeedX());
    TEST_ASSERT_EQUAL_FLOAT(0.0f, game.getSpeedY());
    //full thrust up (the stick aims the plume down the shaft) climbs (2.2 g), with inertia: letting go does not stop it
    game.reset(1);
    run(game, stick(-1.0f, 0.0f), 25);
    TEST_ASSERT_TRUE(game.getSpeedX() > 15.0f);
    float x = game.getShipX();
    game.update(none);
    TEST_ASSERT_TRUE(game.getShipX() > x);
    //the speed is capped
    game.reset(1);
    game.place(60.0f, 14.0f, 200.0f, 0.0f);
    game.update(none);
    TEST_ASSERT_TRUE(game.getSpeedX() <= PlumeGame::maxSpeed + 0.001f);
    //sideways: stick down (the plume towards row 31) moves towards row 0
    game.reset(1);
    run(game, stick(0.0f, -1.0f), 10);
    TEST_ASSERT_TRUE(game.getSpeedY() < 0.0f);
    TEST_ASSERT_TRUE(game.getShipY() < PlumeGame::startY);
}

void test_shaft_per_seed_calm_start_and_plugs(){
    PlumeGame a, b, c;
    a.reset(42);
    b.reset(42);
    c.reset(43);
    //the first 20 m are empty
    for(int32_t x = 0; x < PlumeGame::calmPx; x++) for(int16_t r = 0; r < 32; r++) TEST_ASSERT_FALSE(a.isRock(x, r));
    //the same seed, the same shaft (whatever the player does), another seed, another shaft
    a.place(400.0f, 14.0f);
    b.place(400.0f, 14.0f);
    c.place(400.0f, 14.0f);
    bool b_differs = false;
    for(int32_t x = PlumeGame::calmPx; x < 540; x++){
        for(int16_t r = 0; r < 32; r++){
            TEST_ASSERT_EQUAL(a.isRock(x, r), b.isRock(x, r));
            if(a.isRock(x, r) != c.isRock(x, r)) b_differs = true;
        }
    }
    TEST_ASSERT_TRUE(b_differs);
    //the first plug closes the whole shaft, every row
    for(int16_t r = 0; r < 32; r++){
        bool b_closed = false;
        for(int32_t x = PlumeGame::firstPlugPx; x < PlumeGame::firstPlugPx + 10; x++) if(a.isRock(x, r)) b_closed = true;
        TEST_ASSERT_TRUE(b_closed);
    }
}

void test_shaft_gets_harder_up_to_500_m(){
    PlumeGame game;
    game.reset(9);
    auto coverage = [&](int32_t from){
        int rock = 0;
        for(int32_t x = from; x < from + 128; x++) for(int16_t r = 0; r < 32; r++) if(game.isRock(x, r)) rock++;
        return float(rock) / (128.0f * 32.0f);
    };
    game.place(300.0f, 14.0f);
    float low = coverage(260);
    game.place(6000.0f, 14.0f);
    float high = coverage(5960);
    TEST_ASSERT_TRUE(high > low + 0.1f);
    TEST_ASSERT_TRUE(high < 0.6f);
    TEST_ASSERT_EQUAL_FLOAT(1.0f, game.difficulty());
}

void test_plume_erodes_rock_and_never_hurts_the_canister(){
    PlumeGame game;
    RecordingSink sink;
    game.setSound(&sink);
    game.reset(5);
    game.place(100.0f, 14.0f);
    wall(game, 112);
    wall(game, 113);
    //the stick aims the plume right into the wall: the particles leave along the stick (the thrust towards screen left)
    GameInput down = stick(1.0f, 0.0f);
    int crackles = 0;
    for(int t = 0; t < 12; t++){
        uint32_t before = sink.count(SoundLayer::FRONT);
        game.update(down);
        if(sink.count(SoundLayer::FRONT) > before){
            const SoundNote& n = sink.last(SoundLayer::FRONT)->note;
            if(n.wave == Synth::Wave::NOISE && n.hertz >= 2500 && n.hertz <= 4200) crackles++;
        }
    }
    int cleared = 0;
    for(int16_t r = 0; r < 32; r++) if(!game.isRock(112, r)) cleared++;
    TEST_ASSERT_TRUE(cleared >= 3);
    TEST_ASSERT_TRUE(game.liveParticles() > 0);
    TEST_ASSERT_EQUAL_UINT8(PlumeGame::maxHull, game.getHull());
    //a crackle, rate limited (at most every 4th tick)
    TEST_ASSERT_TRUE(crackles >= 1);
    TEST_ASSERT_TRUE(crackles <= 3);
    //no thrust, no new particles: they die after 0.3 s
    GameInput none;
    run(game, none, 16);
    TEST_ASSERT_EQUAL_UINT8(0, game.liveParticles());
}

void test_hull_hit_bounces_and_grace_second(){
    PlumeGame game;
    RecordingSink sink;
    game.setSound(&sink);
    game.reset(5);
    game.place(100.0f, 14.0f, 40.0f, 0.0f);
    wall(game, 107);
    GameInput none;
    int ticks = 0;
    while(game.getHull() == PlumeGame::maxHull && ticks < 20){
        game.update(none);
        ticks++;
    }
    TEST_ASSERT_EQUAL_UINT8(PlumeGame::maxHull - 1, game.getHull());
    TEST_ASSERT_TRUE(game.isInvulnerable());
    //bounced: the speed into the rock reversed and scaled by 0.4
    TEST_ASSERT_TRUE(game.getSpeedX() < 0.0f);
    TEST_ASSERT_TRUE(game.getSpeedX() > -20.0f);
    //the clunk
    TEST_ASSERT_TRUE(sink.heard(SoundLayer::FRONT, 150));
    //within the grace second another touch costs nothing
    game.place(game.getShipX(), 14.0f, 40.0f, 0.0f);
    run(game, none, 5);
    TEST_ASSERT_EQUAL_UINT8(PlumeGame::maxHull - 1, game.getHull());
    //after it, it does
    run(game, none, PlumeGame::graceTicks);
    TEST_ASSERT_FALSE(game.isInvulnerable());
    game.place(101.0f, 14.0f, 40.0f, 0.0f);
    ticks = 0;
    while(game.getHull() == PlumeGame::maxHull - 1 && ticks < 20){
        game.update(none);
        ticks++;
    }
    TEST_ASSERT_EQUAL_UINT8(PlumeGame::maxHull - 2, game.getHull());
}

void test_long_edges_are_walls(){
    PlumeGame game;
    game.reset(5);
    game.place(100.0f, 1.4f, 0.0f, -20.0f);
    GameInput none;
    run(game, none, 5);
    TEST_ASSERT_EQUAL_UINT8(PlumeGame::maxHull - 1, game.getHull());
    TEST_ASSERT_TRUE(game.getSpeedY() > 0.0f);
    TEST_ASSERT_TRUE(lroundf(game.getShipY()) >= 1);
}

void test_camera_only_moves_up(){
    PlumeGame game;
    game.reset(2);
    TEST_ASSERT_EQUAL_FLOAT(0.0f, game.getCameraX());
    clearAround(game, 150, 40);
    game.place(150.0f, 14.0f);
    TEST_ASSERT_EQUAL_FLOAT(150.0f - PlumeGame::cameraLead, game.getCameraX());
    GameInput none;
    run(game, none, 30);
    TEST_ASSERT_TRUE(game.getShipX() < 150.0f);
    TEST_ASSERT_EQUAL_FLOAT(150.0f - PlumeGame::cameraLead, game.getCameraX());
    //the score is the highest altitude, not the current one
    TEST_ASSERT_EQUAL_UINT32(uint32_t((150.0f - PlumeGame::startX) / 10.0f), game.score());
}

void test_game_over_by_falling_out(){
    PlumeGame game;
    RecordingSink sink;
    game.setSound(&sink);
    game.reset(3);
    GameInput none;
    int ticks = 0;
    while(!game.isOver() && ticks < 500){
        game.update(none);
        ticks++;
    }
    TEST_ASSERT_TRUE(game.isOver());
    TEST_ASSERT_TRUE(game.hasFallenOut());
    TEST_ASSERT_EQUAL_UINT8(PlumeGame::maxHull, game.getHull());
    TEST_ASSERT_TRUE(game.getShipX() + PlumeGame::shipWidth <= game.getCameraX());
    TEST_ASSERT_EQUAL_UINT32(0, game.score());
    //the long falling whistle
    const RecordingSink::Play* p = sink.last(SoundLayer::FRONT);
    TEST_ASSERT_NOT_NULL(p);
    TEST_ASSERT_FALSE(p->b_note);
    TEST_ASSERT_EQUAL_UINT16(1900, p->note.hertz);
    TEST_ASSERT_TRUE(p->cue.lengthMillis() >= 1000);
    TEST_ASSERT_TRUE(p->cue.notes[p->cue.noteCount - 1].hertz < 300);
    for(uint16_t i = 0; i < p->cue.noteCount; i++) TEST_ASSERT_TRUE(p->cue.notes[i].kind == SoundNote::Kind::SYNTH);
}

void test_game_over_when_the_hull_is_gone(){
    PlumeGame game;
    RecordingSink sink;
    game.setSound(&sink);
    game.reset(4);
    GameInput none;
    for(uint8_t hit = 0; hit < PlumeGame::maxHull; hit++){
        while(game.isInvulnerable()) game.update(none);
        clearAround(game, 100);
        wall(game, 107);
        game.place(100.0f, 14.0f, 40.0f, 0.0f);
        uint8_t hull = game.getHull();
        int ticks = 0;
        while(game.getHull() == hull && ticks < 20){
            game.update(none);
            ticks++;
        }
        TEST_ASSERT_EQUAL_UINT8(PlumeGame::maxHull - hit - 1, game.getHull());
    }
    TEST_ASSERT_TRUE(game.isBursting());
    TEST_ASSERT_FALSE(game.isOver());
    TEST_ASSERT_FALSE(game.hasFallenOut());
    //the burst: a noise, synth (no timer square)
    const RecordingSink::Play* p = sink.last(SoundLayer::FRONT);
    TEST_ASSERT_NOT_NULL(p);
    TEST_ASSERT_TRUE(p->cue.lengthMillis() >= 250);
    run(game, none, PlumeGame::burstShowTicks - 1);
    TEST_ASSERT_FALSE(game.isOver());
    game.update(none);
    TEST_ASSERT_TRUE(game.isOver());
}

void test_repair_after_50_m_without_a_hit_and_ping_every_100_m(){
    PlumeGame game;
    RecordingSink sink;
    game.setSound(&sink);
    game.reset(6);
    GameInput none;
    clearAround(game, 100);
    wall(game, 107);
    game.place(100.0f, 14.0f, 40.0f, 0.0f);
    run(game, none, 20);
    TEST_ASSERT_EQUAL_UINT8(PlumeGame::maxHull - 1, game.getHull());
    run(game, none, PlumeGame::graceTicks);
    //49 m above the hit: not yet
    float hitAt = 100.0f;
    game.place(hitAt + 480.0f, 14.0f);
    clearAround(game, int32_t(hitAt + 480.0f));
    game.update(none);
    TEST_ASSERT_EQUAL_UINT8(PlumeGame::maxHull - 1, game.getHull());
    //50 m: the hull point back, two rising notes
    sink.clear();
    game.place(hitAt + 510.0f, 14.0f);
    clearAround(game, int32_t(hitAt + 510.0f));
    game.update(none);
    TEST_ASSERT_EQUAL_UINT8(PlumeGame::maxHull, game.getHull());
    const RecordingSink::Play* p = sink.last(SoundLayer::FRONT);
    TEST_ASSERT_NOT_NULL(p);
    TEST_ASSERT_EQUAL_UINT16(SoundKit::hertz(SoundKit::E5), p->note.hertz);
    TEST_ASSERT_TRUE(p->cue.notes[1].hertz > p->cue.notes[0].hertz);
    //the hull never goes over 4
    game.place(hitAt + 1200.0f, 14.0f);
    clearAround(game, int32_t(hitAt + 1200.0f));
    sink.clear();
    game.update(none);
    TEST_ASSERT_EQUAL_UINT8(PlumeGame::maxHull, game.getHull());
    //and past 100 m: the ping (A6)
    TEST_ASSERT_TRUE(game.score() >= 100);
    TEST_ASSERT_TRUE(sink.heard(SoundLayer::FRONT, SoundKit::hertz(SoundKit::A6)));
    //only once per 100 m
    sink.clear();
    run(game, none, 3);
    TEST_ASSERT_FALSE(sink.heard(SoundLayer::FRONT, SoundKit::hertz(SoundKit::A6)));
}

void test_hiss_grains_denser_with_more_thrust(){
    auto grains = [](float push){
        PlumeGame game;
        RecordingSink sink;
        game.setSound(&sink);
        game.reset(8);
        run(game, stick(-push, 0.0f), 30);
        uint32_t n = 0;
        for(uint8_t i = 0; i < RecordingSink::keep; i++){
            const RecordingSink::Play* p = sink.last(SoundLayer::FRONT, i);
            if(p && p->b_note){
                TEST_ASSERT_TRUE(p->note.wave == Synth::Wave::NOISE);
                TEST_ASSERT_TRUE(p->note.hertz >= 5200);
                TEST_ASSERT_TRUE(p->note.millis < 20);     //shorter than a tick: the layer is free for the next
                TEST_ASSERT_EQUAL_UINT8(SoundPriority::GAME, p->priority);
            }
        }
        n = sink.noteCount(SoundLayer::FRONT);
        TEST_ASSERT_EQUAL_UINT32(0, sink.count(SoundLayer::BACK));
        return n;
    };
    uint32_t none = grains(0.0f);
    uint32_t soft = grains(0.4f);
    uint32_t full = grains(1.0f);
    TEST_ASSERT_EQUAL_UINT32(0, none);
    TEST_ASSERT_TRUE(soft > 0);
    TEST_ASSERT_TRUE(full > soft * 2);
    //a one-shot effect playing in front is not cut by a grain
    PlumeGame game;
    RecordingSink sink;
    game.setSound(&sink);
    game.reset(8);
    sink.setPlaying(SoundLayer::FRONT, true);
    run(game, stick(-1.0f, 0.0f), 10);
    TEST_ASSERT_EQUAL_UINT32(0, sink.noteCount(SoundLayer::FRONT));
}

void test_music_starts_in_play_music_and_follows_the_climb(){
    PlumeGame game;
    RecordingSink sink;
    game.setSound(&sink);
    game.reset(1);
    TEST_ASSERT_EQUAL_UINT32(0, sink.count(SoundLayer::BACK));
    game.playMusic();
    TEST_ASSERT_EQUAL_UINT32(1, sink.cueCount(SoundLayer::BACK));
    const RecordingSink::Play* p = sink.last(SoundLayer::BACK);
    TEST_ASSERT_EQUAL_UINT16(16000, p->cue.loopMillis);
    TEST_ASSERT_TRUE(p->cue.noteCount >= 40);
    TEST_ASSERT_TRUE(p->cue.lengthMillis() <= p->cue.loopMillis + 1500);   //the last notes ring into the loop
    TEST_ASSERT_EQUAL_UINT16(100, sink.getTempo(SoundLayer::BACK));
    //calm (synth only, sine or triangle, never a timer square, all three back voices) but audible on the disc: the
    //arpeggio from E5 up (mostly over 900 Hz, the disc's band), the bass from E4 up
    bool b_voice[3] = {false, false, false};
    for(uint16_t i = 0; i < p->cue.noteCount; i++){
        const SoundNote& n = p->cue.notes[i];
        TEST_ASSERT_TRUE(n.kind == SoundNote::Kind::SYNTH);
        TEST_ASSERT_TRUE(n.wave == Synth::Wave::SINE || n.wave == Synth::Wave::TRIANGLE);
        TEST_ASSERT_TRUE(n.levelPercent <= 100);
        if(n.voice == 0) TEST_ASSERT_TRUE(n.hertz >= 650);
        if(n.voice == 1) TEST_ASSERT_TRUE(n.hertz >= 320);
        TEST_ASSERT_TRUE(n.voice < 3);
        b_voice[n.voice] = true;
        if(i) TEST_ASSERT_TRUE(n.atMillis >= p->cue.notes[i - 1].atMillis);
    }
    TEST_ASSERT_TRUE(b_voice[0] && b_voice[1] && b_voice[2]);
    //higher up the climb gets harder, the tune a little quicker (5 % a step, 125 % at 500 m)
    clearAround(game, 2600);
    game.place(2600.0f, 14.0f);
    game.update(GameInput());
    TEST_ASSERT_EQUAL_UINT16(110, sink.getTempo(SoundLayer::BACK));
    clearAround(game, 6000);
    game.place(6000.0f, 14.0f);
    game.update(GameInput());
    TEST_ASSERT_EQUAL_UINT16(125, sink.getTempo(SoundLayer::BACK));
    //a new game starts at the written tempo
    game.reset(2);
    sink.clear();
    game.playMusic();
    TEST_ASSERT_EQUAL_UINT16(100, sink.getTempo(SoundLayer::BACK));
}

void test_demo_climbs_digs_and_sometimes_falls(){
    int pastFirstPlug = 0;
    int fell = 0;
    for(uint32_t seed = 1; seed <= 8; seed++){
        PlumeGame game;
        game.reset(seed);
        int ticks = 0;
        while(!game.isOver() && ticks < 50 * 400){
            GameInput input;
            game.demo(input);
            //the demo only uses the left stick
            TEST_ASSERT_EQUAL_FLOAT(0.0f, input.stick[2]);
            TEST_ASSERT_EQUAL_FLOAT(0.0f, input.stick[3]);
            TEST_ASSERT_FALSE(input.held[0] || input.held[1] || input.clicked[0] || input.clicked[1]);
            game.update(input);
            ticks++;
        }
        TEST_ASSERT_TRUE(game.isOver());
        TEST_ASSERT_TRUE(game.score() >= 30);
        //past the first plug at 50 m: it blasted its way through
        if(game.score() >= 60) pastFirstPlug++;
        if(game.hasFallenOut()) fell++;
    }
    TEST_ASSERT_TRUE(pastFirstPlug >= 5);
    TEST_ASSERT_TRUE(fell >= 1);
}

//sound never touches the game: with or without a sink the demo plays the same, frame for frame
void test_demo_same_with_and_without_sound(){
    PlumeGame silent, loud;
    RecordingSink sink;
    loud.setSound(&sink);
    silent.reset(77);
    loud.reset(77);
    loud.playMusic();
    GFXcanvas1 a(128, 32), b(128, 32);
    for(int t = 0; t < 50 * 30; t++){
        GameInput ia, ib;
        silent.demo(ia);
        loud.demo(ib);
        silent.update(ia);
        loud.update(ib);
        a.fillScreen(0);
        b.fillScreen(0);
        silent.draw(a);
        loud.draw(b);
        TEST_ASSERT_EQUAL_UINT32(GameContract::frameHash(a), GameContract::frameHash(b));
        if(silent.isOver()){
            TEST_ASSERT_TRUE(loud.isOver());
            silent.reset(78 + uint32_t(t));
            loud.reset(78 + uint32_t(t));
        }
    }
    TEST_ASSERT_TRUE(sink.total() > 100);
}

void test_draws_canister_and_hull_pips(){
    PlumeGame game;
    game.reset(1);
    GFXcanvas1 canvas(128, 32);
    canvas.fillScreen(0);
    game.draw(canvas);
    //the canister, 5 x 3, nozzle towards screen right
    int16_t x = int16_t(PlumeGame::startX), y = int16_t(PlumeGame::startY);
    for(int16_t c = 0; c < 4; c++) for(int16_t r = 0; r < 3; r++) TEST_ASSERT_TRUE(canvas.getPixel(x + c, y + r));
    TEST_ASSERT_FALSE(canvas.getPixel(x + 4, y));
    TEST_ASSERT_TRUE(canvas.getPixel(x + 4, y + 1));
    TEST_ASSERT_FALSE(canvas.getPixel(x + 4, y + 2));
    //four 2 x 2 pips in the top right corner, a row down the right edge (across in the sideways grip), 1 px apart,
    //clear of the edges
    int lit = 0;
    for(int16_t px = 124; px < 128; px++) for(int16_t py = 0; py < 32; py++) if(canvas.getPixel(px, py)) lit++;
    TEST_ASSERT_EQUAL(16, lit);
    for(uint8_t i = 0; i < 4; i++){
        int16_t py = int16_t(1 + 3 * i);
        TEST_ASSERT_TRUE(canvas.getPixel(125, py) && canvas.getPixel(126, py + 1));
        TEST_ASSERT_FALSE(canvas.getPixel(125, py + 2));
    }
    for(int16_t py = 0; py < 32; py++) TEST_ASSERT_FALSE(canvas.getPixel(127, py));
    for(int16_t px = 0; px < 128; px++) TEST_ASSERT_FALSE(canvas.getPixel(px, 0));
    //the altitude (0M) and the points (0) in sideways text next to them, 1 px apart: the M's tops at column 123
    //and the 0's at 115, column 124 and 116 dark
    for(int16_t py = 0; py < 32; py++){
        TEST_ASSERT_FALSE(canvas.getPixel(124, py));
        TEST_ASSERT_FALSE(canvas.getPixel(116, py));
    }
    TEST_ASSERT_TRUE(canvas.getPixel(123, 7));      //the M's left stem, the first glyph pixel of its top row
    TEST_ASSERT_TRUE(canvas.getPixel(117, 7));
    TEST_ASSERT_TRUE(canvas.getPixel(115, 2));      //the 0's top
    //over rock the pips and the text are drawn dark (inverted)
    for(int32_t c = 100; c < 128; c++) for(int16_t r = 0; r < 31; r++) game.setRock(c, r, true);
    canvas.fillScreen(0);
    game.draw(canvas);
    TEST_ASSERT_FALSE(canvas.getPixel(125, 1));
    TEST_ASSERT_TRUE(canvas.getPixel(124, 1));
    TEST_ASSERT_FALSE(canvas.getPixel(123, 7));
    TEST_ASSERT_TRUE(canvas.getPixel(124, 7));
}

//--- the collectibles

//the item slots' contents, as found while the shaft is made
struct SeenItem{ int32_t x; int8_t y; PlumeGame::ItemKind kind; uint8_t depth; };

static int collect(PlumeGame& game, SeenItem* seen, int count, int max){
    for(uint8_t i = 0; i < PlumeGame::maxItems; i++){
        const PlumeGame::Item& it = game.item(i);
        if(!it.b_used) continue;
        bool b_known = false;
        for(int k = 0; k < count; k++) if(seen[k].x == it.x && seen[k].y == it.y) b_known = true;
        if(!b_known && count < max) seen[count++] = SeenItem{it.x, it.y, it.kind, it.depth};
    }
    return count;
}

static int16_t sizeOf(PlumeGame::ItemKind kind){ return kind == PlumeGame::ItemKind::GEM ? PlumeGame::gemSize : PlumeGame::heartSize; }

//the canister placed at a column from `from` on with no generated item within 40 px, the stretch around it cleared
static int32_t quietStretch(PlumeGame& game, int32_t from){
    for(int32_t x = from;; x += 20){
        game.place(float(x), 14.0f);
        bool b_item = false;
        for(uint8_t i = 0; i < PlumeGame::maxItems; i++){
            const PlumeGame::Item& it = game.item(i);
            if(it.b_used && !it.b_taken && it.x > x - 40 && it.x < x + 40) b_item = true;
        }
        if(b_item) continue;
        clearAround(game, x, 30);
        return x;
    }
}

void test_items_per_seed_none_in_the_calm_start_buried_ones_cut_out(){
    static SeenItem a[64], b[64], c[64];
    int na = 0, nb = 0, nc = 0;
    PlumeGame ga, gb, gc;
    ga.reset(21);
    gb.reset(21);
    gc.reset(22);
    int buriedChecked = 0;
    for(float x = 100.0f; x <= 6000.0f; x += 100.0f){
        ga.place(x, 14.0f);
        gb.place(x, 14.0f);
        gc.place(x, 14.0f);
        na = collect(ga, a, na, 64);
        nb = collect(gb, b, nb, 64);
        nc = collect(gc, c, nc, 64);
        //every buried item on the screen: its shape dark (never rock), rock all round it
        for(uint8_t i = 0; i < PlumeGame::maxItems; i++){
            const PlumeGame::Item& it = ga.item(i);
            if(!it.b_used || !it.depth || it.x < ga.getCameraX() || it.x > ga.getCameraX() + 120) continue;
            int16_t size = sizeOf(it.kind);
            for(int16_t col = -1; col <= size; col++){
                for(int16_t row = -1; row <= size; row++){
                    int32_t wx = it.x + col;
                    int16_t wy = int16_t(it.y + row);
                    if(ga.isItemPixel(wx, wy)) TEST_ASSERT_FALSE(ga.isRock(wx, wy));
                    else if(wy >= 0 && wy < 32) TEST_ASSERT_TRUE(ga.isRock(wx, wy));
                }
            }
            TEST_ASSERT_TRUE(it.b_buried);
            buriedChecked++;
        }
    }
    //the same seed, the same items; another seed, others
    TEST_ASSERT_EQUAL(na, nb);
    for(int k = 0; k < na; k++){
        TEST_ASSERT_EQUAL_INT32(a[k].x, b[k].x);
        TEST_ASSERT_EQUAL_INT8(a[k].y, b[k].y);
        TEST_ASSERT_TRUE(a[k].kind == b[k].kind);
        TEST_ASSERT_EQUAL_UINT8(a[k].depth, b[k].depth);
    }
    bool b_differs = na != nc;
    for(int k = 0; k < na && k < nc; k++) if(a[k].x != c[k].x || a[k].y != c[k].y) b_differs = true;
    TEST_ASSERT_TRUE(b_differs);
    //none in the first 20 m; inside the shaft, clear of its long edges; the rates (gems every 30 to 60 m, hearts
    //about every 150 m) over 59 m to 600 m; some buried and some not; the burial deeper higher up
    int gems = 0, hearts = 0, buriedGems = 0, buriedHearts = 0;
    int lowDepth = 0, lowCount = 0, highDepth = 0, highCount = 0;
    for(int k = 0; k < na; k++){
        TEST_ASSERT_TRUE(a[k].x >= PlumeGame::calmPx);
        TEST_ASSERT_TRUE(a[k].y >= 2 && a[k].y + sizeOf(a[k].kind) <= 30);
        if(a[k].kind == PlumeGame::ItemKind::GEM){
            gems++;
            if(a[k].depth) buriedGems++;
        }
        else{
            hearts++;
            if(a[k].depth) buriedHearts++;
        }
        if(a[k].depth && a[k].x < 2000){ lowDepth += a[k].depth; lowCount++; }
        if(a[k].depth && a[k].x > 5000){ highDepth += a[k].depth; highCount++; }
    }
    TEST_ASSERT_TRUE(gems >= 9 && gems <= 20);
    TEST_ASSERT_TRUE(hearts >= 2 && hearts <= 6);
    TEST_ASSERT_TRUE(buriedGems >= 1 && buriedGems < gems);
    TEST_ASSERT_TRUE(buriedHearts * 2 >= hearts);
    TEST_ASSERT_TRUE(buriedChecked > 0);
    TEST_ASSERT_TRUE(lowCount > 0 && highCount > 0);
    TEST_ASSERT_TRUE(float(highDepth) / float(highCount) > float(lowDepth) / float(lowCount));
}

//a buried item of a seed's shaft, with the canister placed below it in a clear stretch
static bool buriedItem(PlumeGame& game, uint32_t seed, PlumeGame::Item& found){
    game.reset(seed);
    for(float x = 300.0f; x <= 3000.0f; x += 50.0f){
        game.place(x, 14.0f);
        for(uint8_t i = 0; i < PlumeGame::maxItems; i++){
            const PlumeGame::Item& it = game.item(i);
            if(it.b_used && it.depth && it.x > game.getCameraX() + 70 && it.x < game.getCameraX() + 110){
                found = it;
                return true;
            }
        }
    }
    return false;
}

void test_the_plume_never_destroys_an_item_and_a_freed_one_clicks(){
    PlumeGame game;
    RecordingSink sink;
    game.setSound(&sink);
    PlumeGame::Item it;
    TEST_ASSERT_TRUE(buriedItem(game, 31, it));
    int16_t size = sizeOf(it.kind);
    //the canister held 14 px below it, in its rows, blasting up at it (thrust down)
    float shipX = float(it.x - it.depth - 14);
    float shipY = float(it.y + size / 2 - 1);
    for(int32_t col = int32_t(shipX) - 4; col < it.x - it.depth; col++) for(int16_t r = 1; r < 31; r++) game.setRock(col, r, false);
    auto rockAround = [&](){
        int n = 0;
        for(int16_t col = -it.depth; col < size + it.depth; col++){
            for(int16_t row = -it.depth; row < size + it.depth; row++) if(game.isRock(it.x + col, int16_t(it.y + row))) n++;
        }
        return n;
    };
    int before = rockAround();
    for(int t = 0; t < 150; t++){
        game.place(shipX, shipY);
        game.update(stick(1.0f, 0.0f));          //the plume aimed up the shaft, at the item
        for(int16_t col = 0; col < size; col++){
            for(int16_t row = 0; row < size; row++){
                if(game.isItemPixel(it.x + col, int16_t(it.y + row))) TEST_ASSERT_FALSE(game.isRock(it.x + col, int16_t(it.y + row)));
            }
        }
    }
    //the rock around it blasted, the item still there and whole
    TEST_ASSERT_TRUE(rockAround() < before);
    int pixels = 0;
    for(int16_t col = 0; col < size; col++) for(int16_t row = 0; row < size; row++) if(game.isItemPixel(it.x + col, int16_t(it.y + row))) pixels++;
    TEST_ASSERT_EQUAL(it.kind == PlumeGame::ItemKind::GEM ? 5 : 16, pixels);
    TEST_ASSERT_EQUAL_UINT16(0, game.gemsCollected());
    //the last rock around it cleared: free, a soft click (a short low noise)
    TEST_ASSERT_TRUE(game.item(0).b_buried || game.item(1).b_buried || game.item(2).b_buried ||
                     game.item(3).b_buried || game.item(4).b_buried || game.item(5).b_buried);
    for(int16_t col = -it.depth; col < size + it.depth; col++){
        for(int16_t row = -it.depth; row < size + it.depth; row++) game.setRock(it.x + col, int16_t(it.y + row), false);
    }
    sink.clear();
    game.update(GameInput());
    const RecordingSink::Play* p = sink.last(SoundLayer::FRONT);
    TEST_ASSERT_NOT_NULL(p);
    bool b_clicked = false;
    for(uint8_t i = 0; i < RecordingSink::keep; i++){
        const RecordingSink::Play* q = sink.last(SoundLayer::FRONT, i);
        if(q && !q->b_note && q->note.wave == Synth::Wave::NOISE && q->note.hertz == 1500 && q->cue.lengthMillis() < 20) b_clicked = true;
    }
    TEST_ASSERT_TRUE(b_clicked);
    for(uint8_t i = 0; i < PlumeGame::maxItems; i++){
        if(game.item(i).x == it.x && game.item(i).y == it.y) TEST_ASSERT_FALSE(game.item(i).b_buried);
    }
}

void test_collecting_a_gem_adds_25_points(){
    PlumeGame game;
    RecordingSink sink;
    game.setSound(&sink);
    game.reset(1);
    int32_t x = quietStretch(game, 300);
    uint32_t metres = game.altitude();
    TEST_ASSERT_EQUAL_UINT32(metres, game.score());
    game.placeItem(PlumeGame::ItemKind::GEM, x + 6, 14);
    TEST_ASSERT_TRUE(game.isItemPixel(x + 7, 14));
    //drawn lit in open space
    GFXcanvas1 canvas(128, 32);
    canvas.fillScreen(0);
    game.draw(canvas);
    int16_t sx = int16_t(x + 7 - int32_t(game.getCameraX()));
    TEST_ASSERT_TRUE(canvas.getPixel(sx, 15) && canvas.getPixel(sx, 14) && !canvas.getPixel(sx - 1, 14));
    //2 px away: not yet; touching it (the nose next to it): collected
    game.place(float(x), 14.0f);
    game.update(GameInput());
    TEST_ASSERT_EQUAL_UINT16(0, game.gemsCollected());
    game.place(float(x + 1), 14.0f);
    sink.clear();
    game.update(GameInput());
    TEST_ASSERT_EQUAL_UINT16(1, game.gemsCollected());
    TEST_ASSERT_FALSE(game.isItemPixel(x + 7, 14));
    //points = 1 a metre + 25 a gem
    TEST_ASSERT_EQUAL_UINT32(game.altitude() + 25, game.score());
    TEST_ASSERT_EQUAL_UINT32(metres + 25, game.score());
    //the bright two-note chime, synth
    const RecordingSink::Play* p = sink.last(SoundLayer::FRONT);
    TEST_ASSERT_NOT_NULL(p);
    TEST_ASSERT_EQUAL_UINT16(SoundKit::hertz(SoundKit::E6), p->note.hertz);
    TEST_ASSERT_EQUAL_UINT16(2, p->cue.noteCount);
    TEST_ASSERT_TRUE(p->cue.notes[1].hertz > p->cue.notes[0].hertz);
    for(uint16_t i = 0; i < p->cue.noteCount; i++) TEST_ASSERT_TRUE(p->cue.notes[i].kind == SoundNote::Kind::SYNTH);
    //higher up, the altitude adds as before
    int32_t higher = quietStretch(game, x + 300);
    TEST_ASSERT_EQUAL_UINT32(uint32_t((float(higher) - PlumeGame::startX) / 10.0f) + 25, game.score());
}

void test_a_heart_repairs_up_to_4(){
    PlumeGame game;
    RecordingSink sink;
    game.setSound(&sink);
    game.reset(6);
    GameInput none;
    int32_t x = quietStretch(game, 300);
    wall(game, x + 7);
    game.place(float(x), 14.0f, 40.0f, 0.0f);
    run(game, none, 20);
    TEST_ASSERT_EQUAL_UINT8(PlumeGame::maxHull - 1, game.getHull());
    clearAround(game, x, 30);
    //held where it is while the grace second runs out
    for(int t = 0; t < PlumeGame::graceTicks; t++){
        game.place(float(x), 14.0f);
        game.update(none);
    }
    TEST_ASSERT_EQUAL_UINT16(0, game.gemsCollected());
    game.placeItem(PlumeGame::ItemKind::HEART, x + 5, 13);
    game.place(float(x), 14.0f);
    sink.clear();
    game.update(none);
    TEST_ASSERT_EQUAL_UINT8(PlumeGame::maxHull, game.getHull());
    TEST_ASSERT_EQUAL_UINT16(0, game.gemsCollected());
    //the warm rising arpeggio, synth
    const RecordingSink::Play* p = sink.last(SoundLayer::FRONT);
    TEST_ASSERT_NOT_NULL(p);
    TEST_ASSERT_EQUAL_UINT16(SoundKit::hertz(SoundKit::C5), p->note.hertz);
    TEST_ASSERT_TRUE(p->cue.noteCount >= 3);
    for(uint16_t i = 1; i < p->cue.noteCount; i++) TEST_ASSERT_TRUE(p->cue.notes[i].hertz > p->cue.notes[i - 1].hertz);
    for(uint16_t i = 0; i < p->cue.noteCount; i++) TEST_ASSERT_TRUE(p->cue.notes[i].kind == SoundNote::Kind::SYNTH);
    //never over 4
    game.placeItem(PlumeGame::ItemKind::HEART, x + 5, 13);
    game.place(float(x), 14.0f);
    game.update(none);
    TEST_ASSERT_EQUAL_UINT8(PlumeGame::maxHull, game.getHull());
}

//the demo player picks up items now and then, and plays the same with and without a sink
void test_demo_collects_items_the_same_with_and_without_sound(){
    uint32_t gems = 0;
    for(uint32_t seed = 1; seed <= 6; seed++){
        PlumeGame silent, loud;
        RecordingSink sink;
        loud.setSound(&sink);
        silent.reset(seed);
        loud.reset(seed);
        for(int t = 0; t < 50 * 300 && !silent.isOver(); t++){
            GameInput ia, ib;
            silent.demo(ia);
            loud.demo(ib);
            silent.update(ia);
            loud.update(ib);
        }
        TEST_ASSERT_EQUAL_UINT32(silent.score(), loud.score());
        TEST_ASSERT_EQUAL_UINT16(silent.gemsCollected(), loud.gemsCollected());
        gems += silent.gemsCollected();
    }
    TEST_ASSERT_TRUE(gems >= 2);
}

//GameDraw's sideways text (Blocks and Plume): it runs down the screen from y, the letters' tops at topX. An L: its
//stem (the glyph's left column) across the screen at row y, its foot (the glyph's bottom row) down column topX - 6
void test_sideways_text_reads_in_the_quarter_turn_grip(){
    GFXcanvas1 canvas(128, 32);
    canvas.fillScreen(0);
    GameDraw::textSideways(canvas, 40, 5, "L");
    for(int16_t x = 34; x <= 40; x++) TEST_ASSERT_TRUE(canvas.getPixel(x, 5));      //the stem: glyph column 0
    for(int16_t y = 5; y <= 9; y++) TEST_ASSERT_TRUE(canvas.getPixel(34, y));       //the foot: glyph row 6
    TEST_ASSERT_FALSE(canvas.getPixel(40, 9));                                      //glyph column 4, row 0: dark
    int lit = 0;
    for(int16_t x = 0; x < 128; x++) for(int16_t y = 0; y < 32; y++) if(canvas.getPixel(x, y)) lit++;
    TEST_ASSERT_EQUAL(11, lit);
    //two letters: the second one 6 px further down the screen
    canvas.fillScreen(0);
    GameDraw::textSideways(canvas, 40, 5, "LL");
    for(int16_t x = 34; x <= 40; x++) TEST_ASSERT_TRUE(canvas.getPixel(x, 11));
    //centred: a 2 letter text (11 px) about row 16
    canvas.fillScreen(0);
    GameDraw::textSidewaysCentered(canvas, 40, 16, "LL");
    TEST_ASSERT_TRUE(canvas.getPixel(40, 11));
    TEST_ASSERT_FALSE(canvas.getPixel(40, 10));
}

int main(int, char**){
    UNITY_BEGIN();
    RUN_TEST(test_contract);
    RUN_TEST(test_name_score_text_and_help);
    RUN_TEST(test_seventh_game_of_the_arcade_in_slot_6);
    RUN_TEST(test_stick_gives_direction_and_thrust);
    RUN_TEST(test_physics_step_at_50_hz);
    RUN_TEST(test_shaft_per_seed_calm_start_and_plugs);
    RUN_TEST(test_shaft_gets_harder_up_to_500_m);
    RUN_TEST(test_plume_erodes_rock_and_never_hurts_the_canister);
    RUN_TEST(test_hull_hit_bounces_and_grace_second);
    RUN_TEST(test_long_edges_are_walls);
    RUN_TEST(test_camera_only_moves_up);
    RUN_TEST(test_game_over_by_falling_out);
    RUN_TEST(test_game_over_when_the_hull_is_gone);
    RUN_TEST(test_repair_after_50_m_without_a_hit_and_ping_every_100_m);
    RUN_TEST(test_hiss_grains_denser_with_more_thrust);
    RUN_TEST(test_music_starts_in_play_music_and_follows_the_climb);
    RUN_TEST(test_demo_climbs_digs_and_sometimes_falls);
    RUN_TEST(test_demo_same_with_and_without_sound);
    RUN_TEST(test_draws_canister_and_hull_pips);
    RUN_TEST(test_items_per_seed_none_in_the_calm_start_buried_ones_cut_out);
    RUN_TEST(test_the_plume_never_destroys_an_item_and_a_freed_one_clicks);
    RUN_TEST(test_collecting_a_gem_adds_25_points);
    RUN_TEST(test_a_heart_repairs_up_to_4);
    RUN_TEST(test_demo_collects_items_the_same_with_and_without_sound);
    RUN_TEST(test_sideways_text_reads_in_the_quarter_turn_grip);
    return UNITY_END();
}
