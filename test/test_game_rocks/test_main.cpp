//Host tests of the ROCKS game [D-157]: pio test -e native -f test_game_rocks
#include <unity.h>
#include <HostGfx.h>
#include <GameContract.h>
#include <RocksGame.h>
#include <RecordingSink.h>

void setUp(){}
void tearDown(){}

static GameInput sticks(float lx, float ly, float rx, float ry){
    GameInput input;
    input.stick[0] = lx;
    input.stick[1] = ly;
    input.stick[2] = rx;
    input.stick[3] = ry;
    return input;
}

void test_contract(){
    GameContract::check<RocksGame>();
}

void test_new_game_rocks_keep_clear_of_the_ship(){
    for(uint32_t seed = 1; seed < 50; seed++){
        RocksGame game;
        game.reset(seed);
        TEST_ASSERT_EQUAL_UINT8(3, game.getLives());
        TEST_ASSERT_EQUAL_UINT8(3, game.countRocks(0));
        //half a second without input: nothing reaches the ship in the centre
        for(int t = 0; t < 25; t++) game.update(GameInput());
        TEST_ASSERT_EQUAL_UINT8(3, game.getLives());
    }
}

void test_left_stick_moves_with_inertia_and_up_is_up(){
    RocksGame game;
    game.reset(1);
    game.placeSingleRockForTest(5, 5, 0, 0, 2);
    float y0 = game.getShipY();
    game.update(sticks(0, 1, 0, 0));
    float firstStep = y0 - game.getShipY();
    TEST_ASSERT_TRUE(firstStep > 0.0f);        //stick up: the ship goes up the screen (y falls)
    for(int t = 0; t < 30; t++) game.update(sticks(0, 1, 0, 0));
    float y1 = game.getShipY();
    game.update(sticks(0, 1, 0, 0));
    TEST_ASSERT_TRUE(y1 - game.getShipY() > firstStep * 3.0f);    //it gathers speed
}

void test_right_stick_aims_and_fires_only_past_half(){
    RocksGame game;
    game.reset(1);
    game.placeSingleRockForTest(5, 5, 0, 0, 2);
    for(int t = 0; t < 20; t++) game.update(sticks(0, 0, 0, 0.4f));
    TEST_ASSERT_FLOAT_WITHIN(0.01f, -1.0f, game.getAimY());    //aims up
    TEST_ASSERT_EQUAL_UINT8(0, game.getBulletCount());
    //held at full: about 6 shots a second, never more than 4 in the air
    int most = 0;
    for(int t = 0; t < 50; t++){
        game.update(sticks(0, 0, -1, 0));
        if(game.getBulletCount() > most) most = game.getBulletCount();
    }
    TEST_ASSERT_FLOAT_WITHIN(0.01f, -1.0f, game.getAimX());
    TEST_ASSERT_TRUE(most >= 3 && most <= 4);
}

void test_a_hit_splits_and_scores_more_for_smaller(){
    RocksGame game;
    game.reset(1);
    //a still large rock 20 px right of the ship
    game.placeSingleRockForTest(game.getShipX() + 20, game.getShipY(), 0, 0, 0);
    for(int t = 0; t < 10 && game.score() == 0; t++) game.update(sticks(0, 0, 1, 0));
    TEST_ASSERT_EQUAL_UINT32(RocksGame::pointsBySize[0], game.score());
    TEST_ASSERT_EQUAL_UINT8(2, game.countRocks(1));
    TEST_ASSERT_EQUAL_UINT8(0, game.countRocks(0));
    TEST_ASSERT_TRUE(RocksGame::pointsBySize[2] > RocksGame::pointsBySize[1]);
    TEST_ASSERT_TRUE(RocksGame::pointsBySize[1] > RocksGame::pointsBySize[0]);

    //the smallest vanish
    RocksGame small;
    small.reset(1);
    small.placeSingleRockForTest(small.getShipX() + 15, small.getShipY(), 0, 0, 2);
    for(int t = 0; t < 10 && small.score() == 0; t++) small.update(sticks(0, 0, 1, 0));
    TEST_ASSERT_EQUAL_UINT32(RocksGame::pointsBySize[2], small.score());
    TEST_ASSERT_EQUAL_UINT8(0, small.getRockCount());
}

void test_cleared_screen_brings_a_bigger_wave(){
    RocksGame game;
    game.reset(1);
    game.placeSingleRockForTest(game.getShipX() + 15, game.getShipY(), 0, 0, 2);
    for(int t = 0; t < 10 && game.getRockCount() > 0; t++) game.update(sticks(0, 0, 1, 0));
    TEST_ASSERT_EQUAL_UINT8(0, game.getRockCount());
    for(int t = 0; t < 100; t++) game.update(GameInput());
    TEST_ASSERT_EQUAL_UINT8(2, game.getWave());
    TEST_ASSERT_EQUAL_UINT8(4, game.countRocks(0));
}

void test_a_rock_costs_a_life_then_a_blinking_respawn_and_game_over(){
    RocksGame game;
    game.reset(1);
    for(uint8_t life = 3; life > 0; life--){
        //one still pebble far away keeps the wave from interfering; wait out the blink, then drop a rock on the ship
        game.placeSingleRockForTest(5, 5, 0, 0, 2);
        for(int t = 0; t < 150; t++) game.update(GameInput());
        TEST_ASSERT_FALSE(game.isInvulnerable());
        game.placeSingleRockForTest(game.getShipX(), game.getShipY(), 0, 0, 0);
        game.update(GameInput());
        TEST_ASSERT_EQUAL_UINT8(life - 1, game.getLives());
        TEST_ASSERT_FALSE(game.isShipAlive());
        game.placeSingleRockForTest(5, 5, 0, 0, 2);
        if(life > 1){
            for(int t = 0; t < 70; t++) game.update(GameInput());
            TEST_ASSERT_TRUE(game.isShipAlive());
            TEST_ASSERT_TRUE(game.isInvulnerable());
            TEST_ASSERT_FLOAT_WITHIN(0.01f, 64.0f, game.getShipX());
        }
    }
    TEST_ASSERT_FALSE(game.isOver());          //the burst plays first
    for(int t = 0; t < 70; t++) game.update(GameInput());
    TEST_ASSERT_TRUE(game.isOver());
}

void test_demo_player_scores_before_it_loses(){
    //it plays like someone competent first: points and a later wave, and it ends within 3 minutes
    for(uint32_t seed = 1; seed <= 5; seed++){
        RocksGame game;
        game.reset(seed);
        int t = 0;
        while(!game.isOver() && t < 50 * 180){
            GameInput input;
            game.demo(input);
            for(int i = 0; i < 4; i++) TEST_ASSERT_TRUE(input.stick[i] >= -1.0f && input.stick[i] <= 1.0f);
            TEST_ASSERT_FALSE(input.held[0] || input.held[1] || input.clicked[0] || input.clicked[1]);
            game.update(input);
            t++;
        }
        TEST_ASSERT_TRUE(game.isOver());
        TEST_ASSERT_TRUE(t > 50 * 20);
        TEST_ASSERT_TRUE(game.score() >= 2000);
        TEST_ASSERT_TRUE(game.getWave() >= 2);
    }
}

void test_wave_announcements_say_what_comes(){
    char label[24];
    RocksGame::waveLabel(label, sizeof(label), 2);
    TEST_ASSERT_EQUAL_STRING("WAVE 2 SAUCERS", label);
    RocksGame::waveLabel(label, sizeof(label), 3);
    TEST_ASSERT_EQUAL_STRING("WAVE 3 MINES", label);
    RocksGame::waveLabel(label, sizeof(label), 4);
    TEST_ASSERT_EQUAL_STRING("WAVE 4", label);
    RocksGame::waveLabel(label, sizeof(label), 5);
    TEST_ASSERT_EQUAL_STRING("WAVE 5 BOSS", label);
    RocksGame::waveLabel(label, sizeof(label), 6);
    TEST_ASSERT_EQUAL_STRING("WAVE 6 FASTER", label);
    RocksGame::waveLabel(label, sizeof(label), 10);
    TEST_ASSERT_EQUAL_STRING("WAVE 10 BOSS", label);
}

//runs a wave without input and records which enemies came and how many at once
static void watchEnemies(uint8_t wave, int& saucers, int& mines, int& most){
    RocksGame game;
    game.reset(3);
    game.startWaveForTest(wave);
    saucers = mines = most = 0;
    uint8_t lastSaucers = 0, lastMines = 0;
    for(int t = 0; t < 50 * 40 && !game.isOver(); t++){
        game.update(GameInput());
        uint8_t s = game.countEnemies(RocksGame::SAUCER), m = game.countEnemies(RocksGame::MINE);
        if(s > lastSaucers) saucers++;
        if(m > lastMines) mines++;
        lastSaucers = s;
        lastMines = m;
        if(s + m > most) most = s + m;
        if(game.getWave() != wave) break;
    }
}

void test_enemies_come_one_kind_at_a_time(){
    int saucers, mines, most;
    watchEnemies(1, saucers, mines, most);
    TEST_ASSERT_EQUAL_INT(0, saucers + mines);
    watchEnemies(2, saucers, mines, most);
    TEST_ASSERT_TRUE(saucers >= 1);
    TEST_ASSERT_EQUAL_INT(0, mines);
    TEST_ASSERT_EQUAL_INT(1, most);
    watchEnemies(3, saucers, mines, most);
    TEST_ASSERT_TRUE(mines >= 1);
    TEST_ASSERT_EQUAL_INT(0, saucers);
    TEST_ASSERT_EQUAL_INT(1, most);
}

void test_saucer_shots_are_slow_and_never_point_blank(){
    RocksGame game;
    game.reset(1);
    game.placeSingleRockForTest(5, 5, 0, 0, 2);
    //a saucer hovering 12 px from the ship holds its fire
    game.placeEnemyForTest(RocksGame::SAUCER, game.getShipX() + 12, game.getShipY(), 0, 0);
    for(int t = 0; t < 200; t++){
        game.update(GameInput());
        TEST_ASSERT_EQUAL_UINT8(0, game.getShotCount());
    }
    //40 px away it fires, and the shot takes more than a second to arrive
    game.placeSingleRockForTest(5, 5, 0, 0, 2);
    game.placeEnemyForTest(RocksGame::SAUCER, game.getShipX() + 40, game.getShipY(), 0, 0);
    int fired = -1;
    for(int t = 0; t < 200 && fired < 0; t++){
        game.update(GameInput());
        if(game.getShotCount() > 0) fired = t;
    }
    TEST_ASSERT_TRUE(fired >= 0);
    int hit = -1;
    for(int t = 0; t < 200 && hit < 0; t++){
        game.update(GameInput());
        if(game.getLives() < 3) hit = t;
    }
    TEST_ASSERT_TRUE(hit > 50);
}

void test_mine_creeps_after_the_ship_and_can_be_shot(){
    RocksGame game;
    game.reset(1);
    game.placeSingleRockForTest(5, 5, 0, 0, 2);
    game.placeEnemyForTest(RocksGame::MINE, game.getShipX() + 30, game.getShipY(), 0, 0);
    //slow: it needs more than two seconds for 30 px, but it gets there
    int hit = -1;
    for(int t = 0; t < 500 && hit < 0; t++){
        game.update(GameInput());
        if(game.getLives() < 3) hit = t;
    }
    TEST_ASSERT_TRUE(hit > 100);

    RocksGame shooter;
    shooter.reset(1);
    shooter.placeSingleRockForTest(5, 5, 0, 0, 2);
    shooter.placeEnemyForTest(RocksGame::MINE, shooter.getShipX() + 30, shooter.getShipY(), 0, 0);
    for(int t = 0; t < 30 && shooter.getEnemyCount() > 0; t++) shooter.update(sticks(0, 0, 1, 0));
    TEST_ASSERT_EQUAL_UINT8(0, shooter.getEnemyCount());
    TEST_ASSERT_EQUAL_UINT32(RocksGame::pointsMine, shooter.score());
    TEST_ASSERT_EQUAL_UINT8(3, shooter.getLives());
}

void test_power_ups_spread_rapid_and_shield(){
    RocksGame game;
    game.reset(1);
    game.placeSingleRockForTest(5, 5, 0, 0, 2);
    //flying over it takes it
    game.placePickupForTest(RocksGame::POWER_SPREAD, game.getShipX() + 2, game.getShipY());
    game.update(GameInput());
    TEST_ASSERT_EQUAL_UINT8(0, game.getPickupCount());
    TEST_ASSERT_EQUAL_UINT8(RocksGame::POWER_SPREAD, game.getWeapon());
    game.update(sticks(0, 0, 0, 1));
    TEST_ASSERT_EQUAL_UINT8(3, game.getBulletCount());
    //it lasts 10 s
    for(int t = 0; t < 500; t++) game.update(GameInput());
    TEST_ASSERT_EQUAL_UINT8(RocksGame::POWER_NONE, game.getWeapon());

    //rapid fire: about twice the shots of the plain gun in a second
    int plain = 0, rapid = 0;
    for(int k = 0; k < 2; k++){
        RocksGame g;
        g.reset(1);
        g.placeSingleRockForTest(5, 5, 0, 0, 2);
        if(k == 1){
            g.placePickupForTest(RocksGame::POWER_RAPID, g.getShipX(), g.getShipY());
            g.update(GameInput());
        }
        int shots = 0;
        uint8_t last = 0;
        for(int t = 0; t < 50; t++){
            g.update(sticks(0, 0, 0, 1));
            if(g.getBulletCount() > last) shots++;
            last = g.getBulletCount();
        }
        (k ? rapid : plain) = shots;
    }
    TEST_ASSERT_TRUE(rapid >= plain * 2 - 1);

    //the shield takes one hit: the rock breaks, the ship stays
    RocksGame shielded;
    shielded.reset(1);
    shielded.placeSingleRockForTest(5, 5, 0, 0, 2);
    shielded.placePickupForTest(RocksGame::POWER_SHIELD, shielded.getShipX(), shielded.getShipY());
    shielded.update(GameInput());
    TEST_ASSERT_TRUE(shielded.hasShield());
    shielded.placeSingleRockForTest(shielded.getShipX(), shielded.getShipY(), 0, 0, 0);
    shielded.update(GameInput());
    TEST_ASSERT_FALSE(shielded.hasShield());
    TEST_ASSERT_EQUAL_UINT8(3, shielded.getLives());
    TEST_ASSERT_TRUE(shielded.isShipAlive());
    TEST_ASSERT_EQUAL_UINT32(RocksGame::pointsBySize[0], shielded.score());
    //the next one costs a ship, once the short blink is over
    for(int t = 0; t < 60; t++) shielded.update(GameInput());
    shielded.placeSingleRockForTest(shielded.getShipX(), shielded.getShipY(), 0, 0, 0);
    shielded.update(GameInput());
    TEST_ASSERT_EQUAL_UINT8(2, shielded.getLives());

    //one not taken goes away after a while
    RocksGame left;
    left.reset(1);
    left.placeSingleRockForTest(5, 5, 0, 0, 2);
    left.placePickupForTest(RocksGame::POWER_RAPID, 20, 20);
    for(int t = 0; t < 50 * 10; t++) left.update(GameInput());
    TEST_ASSERT_EQUAL_UINT8(0, left.getPickupCount());
}

//one tick of a pilot that keeps level with a part of the boss, 34 px in front of it, with a shield always renewed
static void pilot(RocksGame& game, uint8_t part, bool b_fire){
    if(!game.hasShield() && game.isShipAlive()) game.placePickupForTest(RocksGame::POWER_SHIELD, game.getShipX(), game.getShipY());
    float px, py;
    game.bossPartCentre(part, px, py);
    float dy = py - game.getShipY(), dx = px - 34.0f - game.getShipX();
    game.update(sticks(dx > 1 ? 0.5f : dx < -1 ? -0.5f : 0, dy > 0.5f ? -0.6f : dy < -0.5f ? 0.6f : 0, b_fire ? 1.0f : 0.3f, 0));
}

void test_boss_every_fifth_wave_parts_first_then_a_bonus(){
    RocksGame game;
    game.reset(2);
    game.startWaveForTest(5);
    TEST_ASSERT_TRUE(game.isBossActive());
    TEST_ASSERT_EQUAL_UINT8(0, game.getRockCount());
    TEST_ASSERT_EQUAL_UINT8(0, game.getEnemyCount());
    uint8_t coreFull = game.getBossPartHealth(3);
    //the ship cannot fly into it
    float px, py;
    for(int t = 0; t < 200; t++) game.update(sticks(1, 0, 0, 0));
    game.bossPartCentre(2, px, py);
    TEST_ASSERT_TRUE(game.getShipX() < px - 14.0f);

    //the pilot takes on the first part left: a turret, the other, the plate, then the core
    uint32_t before = 0;
    for(int t = 0; t < 50 * 120 && !game.isOver() && game.getBossesBeaten() == 0; t++){
        uint8_t part = 0;
        while(part < 3 && game.getBossPartHealth(part) == 0) part++;
        before = game.score();
        pilot(game, part, true);
        //the core is covered while the plate stands
        if(game.getBossPartHealth(2) > 0) TEST_ASSERT_EQUAL_UINT8(coreFull, game.getBossPartHealth(3));
    }
    TEST_ASSERT_EQUAL_UINT8(1, game.getBossesBeaten());
    TEST_ASSERT_TRUE(game.score() - before >= RocksGame::pointsBoss);
    TEST_ASSERT_TRUE(game.isCelebrating());
    TEST_ASSERT_FALSE(game.isOver());
    //after the celebration the next wave: rocks again
    for(int t = 0; t < 50 * 8 && game.getWave() == 5; t++) game.update(GameInput());
    TEST_ASSERT_EQUAL_UINT8(6, game.getWave());
    TEST_ASSERT_TRUE(game.getRockCount() > 0);
    TEST_ASSERT_FALSE(game.isBossActive());
}

void test_bare_core_is_hurt_only_while_open(){
    RocksGame game;
    game.reset(4);
    game.startWaveForTest(5);
    TEST_ASSERT_FALSE(game.isCoreOpen());      //the plate stands
    for(int t = 0; t < 50 * 60 && game.getBossPartHealth(2) > 0; t++) pilot(game, 2, true);
    TEST_ASSERT_EQUAL_UINT8(0, game.getBossPartHealth(2));
    for(int round = 0; round < 2; round++){
        bool b_open = round == 1;
        //wait for the eye to change to the state under test, then one shot
        int t = 0;
        while(t < 400 && game.isCoreOpen() == b_open){ pilot(game, 3, false); t++; }
        while(t < 400 && game.isCoreOpen() != b_open){ pilot(game, 3, false); t++; }
        for(int k = 0; k < 10; k++) pilot(game, 3, false);      //the gun cools down, the ship settles
        uint8_t health = game.getBossPartHealth(3);
        pilot(game, 3, true);
        for(int k = 0; k < 30; k++) pilot(game, 3, false);
        if(b_open) TEST_ASSERT_EQUAL_UINT8(health - 1, game.getBossPartHealth(3));
        else TEST_ASSERT_EQUAL_UINT8(health, game.getBossPartHealth(3));
    }
}

void test_demo_reaches_the_boss_in_some_games(){
    //the attract mode should show the boss now and then, and every game still ends well inside 3 minutes
    int reached = 0;
    for(uint32_t seed = 1; seed <= 10; seed++){
        RocksGame game;
        game.reset(seed * 7919u);
        int t = 0;
        bool b_boss = false;
        while(!game.isOver() && t < 50 * 180){
            GameInput input;
            game.demo(input);
            game.update(input);
            if(game.isBossActive()) b_boss = true;
            t++;
        }
        TEST_ASSERT_TRUE(game.isOver());
        TEST_ASSERT_TRUE(t < 50 * 150);
        if(b_boss) reached++;
    }
    TEST_ASSERT_TRUE(reached >= 3);
}

//the sounds [D-157]: a falling zap for a shot; a noise burst for a rock, duller for a bigger one
void test_sound_shot_zap_and_bursts_by_size(){
    uint16_t brightness[RocksGame::sizeCount] = {0, 0, 0};
    for(uint8_t size = 0; size < RocksGame::sizeCount; size++){
        RocksGame game;
        RecordingSink sink;
        game.setSound(&sink);
        game.reset(1);
        game.placeSingleRockForTest(game.getShipX() + 20, game.getShipY(), 0, 0, size);
        game.update(sticks(0, 0, 1, 0));
        TEST_ASSERT_EQUAL_UINT8(RocksGame::FX_SHOT, game.getLastSound());
        const RecordingSink::Play* p = sink.last(SoundLayer::FRONT);
        TEST_ASSERT_FALSE(p->b_note);
        //synth notes (a timer square would cut the pulse), falling
        TEST_ASSERT_TRUE(p->cue.notes[0].kind == SoundNote::Kind::SYNTH);
        TEST_ASSERT_TRUE(p->cue.notes[p->cue.noteCount - 1].hertz < p->cue.notes[0].hertz);
        TEST_ASSERT_TRUE(p->cue.lengthMillis() < 100);
        for(int t = 0; t < 10 && game.score() == 0; t++) game.update(sticks(0, 0, 0.3f, 0));
        TEST_ASSERT_TRUE(game.score() > 0);
        TEST_ASSERT_EQUAL_UINT8(RocksGame::FX_BURST, game.getLastSound());
        p = sink.last(SoundLayer::FRONT);
        TEST_ASSERT_TRUE(p->cue.notes[0].wave == Synth::Wave::NOISE);
        for(uint16_t i = 0; i < p->cue.noteCount; i++) TEST_ASSERT_TRUE(p->cue.notes[i].kind == SoundNote::Kind::SYNTH);
        brightness[size] = p->cue.notes[0].hertz;
        TEST_ASSERT_EQUAL_UINT32(0, sink.count(SoundLayer::BACK));
    }
    TEST_ASSERT_TRUE(brightness[0] < brightness[1]);
    TEST_ASSERT_TRUE(brightness[1] < brightness[2]);
}

//much happens at once: a sound never cuts a more important one still playing (ship lost over bursts over shots)
void test_sound_ranks_keep_the_important_one(){
    RocksGame game;
    RecordingSink sink;
    game.setSound(&sink);
    game.reset(1);
    //from here the front layer always reports a sound playing
    sink.setPlaying(SoundLayer::FRONT, true);
    game.placeSingleRockForTest(game.getShipX() + 15, game.getShipY(), 0, 0, 2);
    for(int t = 0; t < 10 && game.score() == 0; t++) game.update(sticks(0, 0, 1, 0));
    TEST_ASSERT_EQUAL_UINT8(RocksGame::FX_BURST, game.getLastSound());
    //more shots do not cut the burst
    game.placeSingleRockForTest(5, 5, 0, 0, 2);
    uint32_t plays = sink.count(SoundLayer::FRONT);
    for(int t = 0; t < 30; t++) game.update(sticks(0, 0, 1, 0));
    TEST_ASSERT_EQUAL_UINT32(plays, sink.count(SoundLayer::FRONT));
    TEST_ASSERT_EQUAL_UINT8(RocksGame::FX_BURST, game.getLastSound());
    //the ship lost cuts it: a long falling noise
    game.placeSingleRockForTest(game.getShipX(), game.getShipY(), 0, 0, 0);
    game.update(GameInput());
    TEST_ASSERT_FALSE(game.isShipAlive());
    TEST_ASSERT_EQUAL_UINT8(RocksGame::FX_SHIP_LOST, game.getLastSound());
    const RecordingSink::Play* p = sink.last(SoundLayer::FRONT);
    TEST_ASSERT_TRUE(p->cue.notes[0].wave == Synth::Wave::NOISE);
    TEST_ASSERT_TRUE(p->cue.lengthMillis() >= 900);
    uint16_t first = p->cue.notes[0].hertz, lastNoise = 0;
    for(uint16_t i = 0; i < p->cue.noteCount; i++) if(p->cue.notes[i].wave == Synth::Wave::NOISE) lastNoise = p->cue.notes[i].hertz;
    TEST_ASSERT_TRUE(lastNoise < first / 4);
    //and nothing cuts the ship lost, not even a power-up (taken by the ship once back)
    plays = sink.count(SoundLayer::FRONT);
    game.placeSingleRockForTest(5, 5, 0, 0, 2);
    for(int t = 0; t < 80; t++) game.update(GameInput());
    TEST_ASSERT_TRUE(game.isShipAlive());
    game.placePickupForTest(RocksGame::POWER_RAPID, game.getShipX(), game.getShipY());
    game.update(GameInput());
    TEST_ASSERT_EQUAL_UINT8(RocksGame::POWER_RAPID, game.getWeapon());
    TEST_ASSERT_EQUAL_UINT32(plays, sink.count(SoundLayer::FRONT));
    //once the front is quiet, anything plays again
    sink.setPlaying(SoundLayer::FRONT, false);
    for(int t = 0; t < 3; t++) game.update(sticks(0, 0, -1, 0));
    TEST_ASSERT_EQUAL_UINT8(RocksGame::FX_SHOT, game.getLastSound());
}

//a saucer on the screen warbles, a mine ticks, faster the closer it is: grains, skipped while anything else plays
void test_sound_saucer_warble_and_mine_ticks(){
    RocksGame game;
    RecordingSink sink;
    game.setSound(&sink);
    game.reset(1);
    game.placeSingleRockForTest(5, 5, 0, 0, 2);
    game.placeEnemyForTest(RocksGame::SAUCER, 110, 8, 0, 0);
    uint16_t lowest = 60000, highest = 0;
    for(int t = 0; t < 40; t++){
        game.update(GameInput());
        const RecordingSink::Play* p = sink.last(SoundLayer::FRONT);
        if(!p) continue;
        TEST_ASSERT_TRUE(p->b_note);
        TEST_ASSERT_TRUE(p->note.millis < 40);
        if(p->note.hertz < lowest) lowest = p->note.hertz;
        if(p->note.hertz > highest) highest = p->note.hertz;
    }
    TEST_ASSERT_EQUAL_UINT8(RocksGame::FX_WARBLE, game.getLastSound());
    TEST_ASSERT_EQUAL_UINT32(20, sink.noteCount(SoundLayer::FRONT));
    TEST_ASSERT_TRUE(highest - lowest > 300);
    sink.setPlaying(SoundLayer::FRONT, true);
    uint32_t plays = sink.count(SoundLayer::FRONT);
    for(int t = 0; t < 20; t++) game.update(GameInput());
    TEST_ASSERT_EQUAL_UINT32(plays, sink.count(SoundLayer::FRONT));

    int ticks[2] = {0, 0};
    for(int near = 0; near < 2; near++){
        RocksGame g;
        RecordingSink s;
        g.setSound(&s);
        g.reset(1);
        g.placeSingleRockForTest(5, 5, 0, 0, 2);
        g.placeEnemyForTest(RocksGame::MINE, g.getShipX() + (near ? 14.0f : 55.0f), g.getShipY(), 0, 0);
        for(int t = 0; t < 40; t++) g.update(GameInput());
        TEST_ASSERT_TRUE(g.isShipAlive());
        TEST_ASSERT_EQUAL_UINT8(RocksGame::FX_MINE_TICK, g.getLastSound());
        ticks[near] = int(s.noteCount(SoundLayer::FRONT));
    }
    TEST_ASSERT_TRUE(ticks[0] >= 2);
    TEST_ASSERT_TRUE(ticks[1] > ticks[0] + 2);
}

//a power-up: a rising arpeggio; a cleared wave: a jingle once the last burst is over; a boss: its intro
void test_sound_power_up_wave_clear_and_boss_intro(){
    RocksGame game;
    RecordingSink sink;
    game.setSound(&sink);
    game.reset(1);
    game.placeSingleRockForTest(5, 5, 0, 0, 2);
    game.placePickupForTest(RocksGame::POWER_SPREAD, game.getShipX(), game.getShipY());
    game.update(GameInput());
    TEST_ASSERT_EQUAL_UINT8(RocksGame::FX_POWER_UP, game.getLastSound());
    const RecordingSink::Play* p = sink.last(SoundLayer::FRONT);
    for(uint16_t i = 1; i < p->cue.noteCount; i++) TEST_ASSERT_TRUE(p->cue.notes[i].hertz > p->cue.notes[i - 1].hertz);

    //the last rock: its burst, the jingle waits while the burst plays, then comes
    game.placeSingleRockForTest(game.getShipX() + 15, game.getShipY(), 0, 0, 2);
    for(int t = 0; t < 10 && game.getRockCount() > 0; t++) game.update(sticks(0, 0, 1, 0));
    TEST_ASSERT_EQUAL_UINT8(0, game.getRockCount());
    sink.setPlaying(SoundLayer::FRONT, true);
    for(int t = 0; t < 10; t++) game.update(GameInput());
    TEST_ASSERT_EQUAL_UINT8(RocksGame::FX_BURST, game.getLastSound());
    sink.setPlaying(SoundLayer::FRONT, false);
    game.update(GameInput());
    TEST_ASSERT_EQUAL_UINT8(RocksGame::FX_WAVE_CLEAR, game.getLastSound());
    TEST_ASSERT_EQUAL_UINT8(1, game.getWave());

    game.startWaveForTest(5);
    game.update(GameInput());
    TEST_ASSERT_EQUAL_UINT8(RocksGame::FX_BOSS_INTRO, game.getLastSound());
    p = sink.last(SoundLayer::FRONT);
    TEST_ASSERT_TRUE(p->cue.lengthMillis() < 1000);
}

//the music: the pulse starts in playMusic, looping on the back layer; it speeds up as the wave's rocks thin out
void test_sound_pulse_speeds_up_as_rocks_thin_out(){
    RocksGame game;
    RecordingSink sink;
    game.setSound(&sink);
    game.reset(1);
    game.playMusic();
    TEST_ASSERT_EQUAL_UINT32(1, sink.cueCount(SoundLayer::BACK));
    const RecordingSink::Play* p = sink.last(SoundLayer::BACK);
    TEST_ASSERT_EQUAL_UINT16(2, p->cue.noteCount);
    TEST_ASSERT_TRUE(p->cue.loopMillis > 0);
    TEST_ASSERT_TRUE(p->cue.notes[0].hertz != p->cue.notes[1].hertz);
    //low (ominous) but within the disc's reach: F4 and E4 on the narrowest pulse at full level
    TEST_ASSERT_TRUE(p->cue.notes[0].hertz > 300 && p->cue.notes[1].hertz > 300);
    TEST_ASSERT_TRUE(p->cue.notes[0].hertz < 400 && p->cue.notes[1].hertz < 400);
    TEST_ASSERT_TRUE(p->cue.notes[0].wave == Synth::Wave::PULSE12 && p->cue.notes[0].levelPercent == 100);
    TEST_ASSERT_TRUE(p->cue.notes[0].kind == SoundNote::Kind::SYNTH);
    TEST_ASSERT_EQUAL_UINT16(RocksGame::pulseSlowest, sink.getTempo(SoundLayer::BACK));

    //one of the three large rocks broken: a little faster
    game.placeSingleRockForTest(5, 5, 0, 0, 0);
    game.update(GameInput());
    uint16_t oneLarge = sink.getTempo(SoundLayer::BACK);
    TEST_ASSERT_TRUE(oneLarge > RocksGame::pulseSlowest);
    //one pebble left: near the fastest
    game.placeSingleRockForTest(5, 5, 0, 0, 2);
    game.update(GameInput());
    uint16_t onePebble = sink.getTempo(SoundLayer::BACK);
    TEST_ASSERT_TRUE(onePebble > oneLarge);
    TEST_ASSERT_TRUE(onePebble >= RocksGame::pulseFastest - 20 && onePebble <= RocksGame::pulseFastest);
    //playMusic again (a resume) keeps the tempo of the moment; a new wave starts slow again
    game.playMusic();
    TEST_ASSERT_EQUAL_UINT16(onePebble, sink.getTempo(SoundLayer::BACK));
    game.startWaveForTest(2);
    game.update(GameInput());
    TEST_ASSERT_EQUAL_UINT16(RocksGame::pulseSlowest, sink.getTempo(SoundLayer::BACK));
    //only the music touches the back layer
    TEST_ASSERT_EQUAL_UINT32(2, sink.count(SoundLayer::BACK));
}

//the boss fight: intro, clanks on its parts, bursts when one goes, a big blast when it is beaten, then the jingle;
//the pulse speeds up as it is worn down
void test_sound_boss_fight(){
    RocksGame game;
    RecordingSink sink;
    game.setSound(&sink);
    game.reset(2);
    game.startWaveForTest(5);
    game.playMusic();
    uint16_t startTempo = sink.getTempo(SoundLayer::BACK);
    game.update(GameInput());
    uint32_t seen = 1u << game.getLastSound();
    uint16_t fastest = 0;
    for(int t = 0; t < 50 * 120 && !game.isOver() && game.getWave() == 5; t++){
        uint8_t part = 0;
        while(part < 3 && game.getBossPartHealth(part) == 0) part++;
        if(game.isBossActive()) pilot(game, part, true);
        else game.update(GameInput());
        seen |= 1u << game.getLastSound();
        if(game.isBossActive() && sink.getTempo(SoundLayer::BACK) > fastest) fastest = sink.getTempo(SoundLayer::BACK);
    }
    TEST_ASSERT_EQUAL_UINT8(1, game.getBossesBeaten());
    TEST_ASSERT_TRUE(seen & (1u << RocksGame::FX_BOSS_INTRO));
    TEST_ASSERT_TRUE(seen & (1u << RocksGame::FX_BOSS_HIT));
    TEST_ASSERT_TRUE(seen & (1u << RocksGame::FX_BURST));
    TEST_ASSERT_TRUE(seen & (1u << RocksGame::FX_BOSS_DOWN));
    TEST_ASSERT_TRUE(seen & (1u << RocksGame::FX_WAVE_CLEAR));
    TEST_ASSERT_EQUAL_UINT16(RocksGame::pulseSlowest, startTempo);
    TEST_ASSERT_TRUE(fastest > startTempo + 100);
}

//sound never changes the game: the demo plays the same with a sink as without one (silent, the picker's demo)
void test_demo_same_with_and_without_sound(){
    RocksGame silent, heard;
    RecordingSink sink;
    heard.setSound(&sink);
    silent.reset(31);
    heard.reset(31);
    heard.playMusic();
    for(int t = 0; t < 50 * 120 && !silent.isOver(); t++){
        GameInput a, b;
        silent.demo(a);
        heard.demo(b);
        silent.update(a);
        heard.update(b);
        TEST_ASSERT_EQUAL_FLOAT(silent.getShipX(), heard.getShipX());
        TEST_ASSERT_EQUAL_FLOAT(silent.getShipY(), heard.getShipY());
        TEST_ASSERT_EQUAL_UINT8(silent.getRockCount(), heard.getRockCount());
        TEST_ASSERT_EQUAL_UINT32(silent.score(), heard.score());
    }
    TEST_ASSERT_EQUAL_UINT8(silent.getLives(), heard.getLives());
    TEST_ASSERT_TRUE(heard.isOver() == silent.isOver());
    TEST_ASSERT_TRUE(sink.count(SoundLayer::FRONT) > 100);
    TEST_ASSERT_EQUAL_UINT8(RocksGame::FX_NONE, silent.getLastSound());
}

int main(){
    UNITY_BEGIN();
    RUN_TEST(test_contract);
    RUN_TEST(test_new_game_rocks_keep_clear_of_the_ship);
    RUN_TEST(test_left_stick_moves_with_inertia_and_up_is_up);
    RUN_TEST(test_right_stick_aims_and_fires_only_past_half);
    RUN_TEST(test_a_hit_splits_and_scores_more_for_smaller);
    RUN_TEST(test_cleared_screen_brings_a_bigger_wave);
    RUN_TEST(test_a_rock_costs_a_life_then_a_blinking_respawn_and_game_over);
    RUN_TEST(test_demo_player_scores_before_it_loses);
    RUN_TEST(test_wave_announcements_say_what_comes);
    RUN_TEST(test_enemies_come_one_kind_at_a_time);
    RUN_TEST(test_saucer_shots_are_slow_and_never_point_blank);
    RUN_TEST(test_mine_creeps_after_the_ship_and_can_be_shot);
    RUN_TEST(test_power_ups_spread_rapid_and_shield);
    RUN_TEST(test_boss_every_fifth_wave_parts_first_then_a_bonus);
    RUN_TEST(test_bare_core_is_hurt_only_while_open);
    RUN_TEST(test_demo_reaches_the_boss_in_some_games);
    RUN_TEST(test_sound_shot_zap_and_bursts_by_size);
    RUN_TEST(test_sound_ranks_keep_the_important_one);
    RUN_TEST(test_sound_saucer_warble_and_mine_ticks);
    RUN_TEST(test_sound_power_up_wave_clear_and_boss_intro);
    RUN_TEST(test_sound_pulse_speeds_up_as_rocks_thin_out);
    RUN_TEST(test_sound_boss_fight);
    RUN_TEST(test_demo_same_with_and_without_sound);
    return UNITY_END();
}
