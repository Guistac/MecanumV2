//Host tests of BLOCKS, the sideways falling-blocks game [D-157], and its sound: pio test -e native -f test_game_blocks
#include <unity.h>
#include <HostGfx.h>
#include <GameContract.h>
#include <BlocksGame.h>
#include <RecordingSink.h>
#include <SoundKit.h>

void setUp(){}
void tearDown(){}

static void idle(BlocksGame& game, int ticks){
    for(int t = 0; t < ticks; t++){
        GameInput input;
        game.update(input);
    }
}

static void hardDrop(BlocksGame& game){
    GameInput input;
    input.clicked[0] = true;
    input.held[0] = true;
    game.update(input);
}

void test_contract(){
    GameContract::check<BlocksGame>();
}

void test_seven_bag_deals_each_piece_once(){
    BlocksGame game;
    game.reset(42);
    for(int round = 0; round < 3; round++){
        bool seen[7] = {false};
        for(int i = 0; i < 7; i++){
            TEST_ASSERT_FALSE(seen[game.pieceType()]);
            seen[game.pieceType()] = true;
            //spread the drops across the well so the stack never reaches the entry
            GameInput move;
            move.stick[1] = (i & 1) ? 1.0f : -1.0f;
            for(int t = 0; t < 40; t++) game.update(move);
            hardDrop(game);
            while(game.isClearing()) idle(game, 1);
            TEST_ASSERT_FALSE(game.isOver());
        }
    }
}

void test_left_stick_moves_across_with_repeat_and_stays_in_the_well(){
    BlocksGame game;
    game.reset(7);
    int8_t start = game.pieceAcross();
    GameInput up;
    up.stick[1] = 1.0f;     //up: toward the top of the screen
    game.update(up);
    TEST_ASSERT_EQUAL_INT8(start - 1, game.pieceAcross());
    //no repeat before 0.2 s
    for(int t = 0; t < 8; t++) game.update(up);
    TEST_ASSERT_EQUAL_INT8(start - 1, game.pieceAcross());
    for(int t = 0; t < 60; t++) game.update(up);
    int8_t lowest, highest;
    game.pieceSpan(lowest, highest);
    TEST_ASSERT_EQUAL_INT8(0, lowest);
    GameInput down;
    down.stick[1] = -1.0f;
    for(int t = 0; t < 120; t++) game.update(down);
    game.pieceSpan(lowest, highest);
    TEST_ASSERT_EQUAL_INT8(BlocksGame::wellWidth - 1, highest);
    //a small wobble (a click pressing the stick a little) does not move it
    GameInput wobble;
    wobble.stick[1] = 0.3f;
    int8_t before = game.pieceAcross();
    game.update(wobble);
    TEST_ASSERT_EQUAL_INT8(before, game.pieceAcross());
}

void test_gravity_and_soft_drop_move_toward_the_floor(){
    BlocksGame game;
    game.reset(9);
    int8_t start = game.pieceAlong();
    TEST_ASSERT_EQUAL_FLOAT(0.7f, game.gravitySeconds());
    idle(game, 35);     //0.7 s: one cell
    TEST_ASSERT_EQUAL_INT8(start - 1, game.pieceAlong());
    GameInput soft;
    soft.stick[0] = -1.0f;
    for(int t = 0; t < 10; t++) game.update(soft);
    TEST_ASSERT_EQUAL_INT8(start - 6, game.pieceAlong());
}

void test_hard_drop_lands_on_the_floor_and_brings_the_next_piece(){
    BlocksGame game;
    game.reset(11);
    uint8_t next = game.nextType();
    hardDrop(game);
    TEST_ASSERT_EQUAL_UINT32(1, game.piecesPlaced());
    TEST_ASSERT_EQUAL_UINT8(next, game.pieceType());
    int lit = 0, onFloor = 0;
    for(int8_t a = 0; a < BlocksGame::wellLength; a++){
        for(int8_t b = 0; b < BlocksGame::wellWidth; b++){
            if(game.filled(a, b)){
                lit++;
                if(a == 0) onFloor++;
            }
        }
    }
    TEST_ASSERT_EQUAL_INT(4, lit);
    TEST_ASSERT_TRUE(onFloor > 0);
}

void test_rotation_by_click_and_flick_once_per_flick(){
    BlocksGame game;
    game.reset(3);
    while(game.pieceType() == 3) hardDrop(game);   //the square has nothing to show
    GameInput click;
    click.clicked[1] = true;
    game.update(click);
    TEST_ASSERT_EQUAL_UINT8(1, game.pieceRotation());
    GameInput flick;
    flick.stick[3] = 1.0f;
    for(int t = 0; t < 20; t++) game.update(flick);     //held: one turn only
    TEST_ASSERT_EQUAL_UINT8(2, game.pieceRotation());
    idle(game, 1);
    flick.stick[3] = -1.0f;
    game.update(flick);
    TEST_ASSERT_EQUAL_UINT8(1, game.pieceRotation());
}

void test_cleared_columns_score_by_count_and_level(){
    //the demo player plays until its first clear; the points match the classic table at the level of the clear
    const uint32_t table[5] = {0, 40, 100, 300, 1200};
    BlocksGame game;
    game.reset(2024);
    bool b_checked = false;
    for(int t = 0; t < 50 * 120 && !b_checked && !game.isOver(); t++){
        uint32_t before = game.score();
        uint16_t columns = game.columnsCleared();
        uint8_t level = game.level();
        GameInput input;
        game.demo(input);
        game.update(input);
        if(game.score() != before){
            TEST_ASSERT_TRUE(game.isClearing());
            while(game.isClearing()) idle(game, 1);
            uint16_t count = uint16_t(game.columnsCleared() - columns);
            TEST_ASSERT_TRUE(count >= 1 && count <= 4);
            TEST_ASSERT_EQUAL_UINT32(table[count] * level, game.score() - before);
            //no full column is left behind
            for(int8_t a = 0; a < BlocksGame::wellLength; a++){
                bool b_full = true;
                for(int8_t b = 0; b < BlocksGame::wellWidth; b++) if(!game.filled(a, b)) b_full = false;
                TEST_ASSERT_FALSE(b_full);
            }
            b_checked = true;
        }
    }
    TEST_ASSERT_TRUE(b_checked);
}

void test_stack_reaching_the_entry_ends_the_game(){
    BlocksGame game;
    game.reset(5);
    int drops = 0;
    while(!game.isOver() && drops < 100){
        hardDrop(game);
        while(game.isClearing()) idle(game, 1);
        drops++;
    }
    TEST_ASSERT_TRUE(game.isOver());
    TEST_ASSERT_TRUE(drops < 20);
    uint32_t placed = game.piecesPlaced();
    hardDrop(game);
    TEST_ASSERT_EQUAL_UINT32(placed, game.piecesPlaced());
}

void test_demo_uses_only_the_player_controls(){
    BlocksGame game;
    game.reset(77);
    for(int t = 0; t < 50 * 60 && !game.isOver(); t++){
        GameInput input;
        game.demo(input);
        TEST_ASSERT_FALSE(input.held[0] && input.held[1]);     //both together is the Arcade's way out
        TEST_ASSERT_EQUAL_FLOAT(0.0f, input.stick[2]);
        for(int i = 0; i < 4; i++) TEST_ASSERT_TRUE(input.stick[i] >= -1.0f && input.stick[i] <= 1.0f);
        game.update(input);
    }
}

//sound [D-157]: every note of the plays a tick added on a layer is a synth note (no timer square, it would cut the
//music), unless b_squaresAllowed
static void checkNewPlays(const RecordingSink& sink, SoundLayer layer, uint32_t before, bool b_squaresAllowed){
    uint32_t added = sink.count(layer) - before;
    for(uint32_t k = 0; k < added && k < RecordingSink::keep; k++){
        const RecordingSink::Play* p = sink.last(layer, uint8_t(k));
        if(p->b_note){
            if(!b_squaresAllowed) TEST_ASSERT_TRUE(p->note.kind == SoundNote::Kind::SYNTH);
            continue;
        }
        TEST_ASSERT_EQUAL_UINT8(SoundPriority::GAME, p->priority);
        for(uint16_t i = 0; i < p->cue.noteCount && !b_squaresAllowed; i++){
            TEST_ASSERT_TRUE(p->cue.notes[i].kind == SoundNote::Kind::SYNTH);
        }
    }
}

static bool hasDrums(const SoundCue& cue){
    for(uint16_t i = 0; i < cue.noteCount; i++) if(cue.notes[i].voice == 2) return true;
    return false;
}

void test_music_starts_in_play_music(){
    RecordingSink sink;
    BlocksGame game;
    game.setSound(&sink);
    game.reset(2);
    TEST_ASSERT_EQUAL_UINT32(0, sink.total());     //a new game is quiet until the Arcade asks for its music
    game.playMusic();
    TEST_ASSERT_EQUAL_UINT32(1, sink.cueCount(SoundLayer::BACK));
    TEST_ASSERT_EQUAL_UINT32(0, sink.count(SoundLayer::FRONT));
    const RecordingSink::Play* p = sink.last(SoundLayer::BACK);
    //the folk melody from its first note (E6: an octave up, where the disc plays it), looping after its 16 bars,
    //melody and bass only at level 1
    TEST_ASSERT_EQUAL_UINT16(SoundKit::hertz(SoundKit::E6), p->note.hertz);
    //written for the disc: the melody 880 Hz and up on a full square, the bass short and no lower than C4
    for(uint16_t i = 0; i < p->cue.noteCount; i++){
        const SoundNote& n = p->cue.notes[i];
        if(n.voice == 0) TEST_ASSERT_TRUE(n.hertz >= 880 && n.wave == Synth::Wave::SQUARE);
        if(n.voice == 1) TEST_ASSERT_TRUE(n.hertz >= 260 && n.millis <= 150);
    }
    //the drum version holds every note of the plain one (nothing dropped for room), and the drums
    BlocksGame other;
    const SoundCue& plain = other.tuneCue(false);
    const SoundCue& drums = other.tuneCue(true);
    uint16_t drumNotes = 0;
    for(uint16_t i = 0; i < drums.noteCount; i++) if(drums.notes[i].voice == 2) drumNotes++;
    TEST_ASSERT_EQUAL_UINT16(plain.noteCount + drumNotes, drums.noteCount);
    TEST_ASSERT_EQUAL_UINT16(8 * (3 * SoundKit::kickNoteCount + 4 + 2), drumNotes);
    TEST_ASSERT_EQUAL_UINT16(BlocksGame::tuneSteps * BlocksGame::tuneStepMillis, p->cue.loopMillis);
    TEST_ASSERT_TRUE(p->cue.lengthMillis() <= p->cue.loopMillis);
    TEST_ASSERT_TRUE(p->cue.noteCount > 100);
    TEST_ASSERT_FALSE(hasDrums(p->cue));
    for(uint16_t i = 1; i < p->cue.noteCount; i++) TEST_ASSERT_TRUE(p->cue.notes[i - 1].atMillis <= p->cue.notes[i].atMillis);
    TEST_ASSERT_EQUAL_UINT16(100, sink.getTempo(SoundLayer::BACK));
    //resumed: from the top again, the same tune
    game.playMusic();
    TEST_ASSERT_EQUAL_UINT32(2, sink.cueCount(SoundLayer::BACK));
    TEST_ASSERT_EQUAL_PTR(p->cue.notes, sink.last(SoundLayer::BACK)->cue.notes);
}

void test_music_tempo_follows_the_level_and_drums_join_at_level_3(){
    TEST_ASSERT_EQUAL_UINT16(100, BlocksGame::musicTempo(1));
    TEST_ASSERT_EQUAL_UINT16(106, BlocksGame::musicTempo(2));
    TEST_ASSERT_EQUAL_UINT16(112, BlocksGame::musicTempo(3));
    for(uint8_t l = 2; l < 30; l++) TEST_ASSERT_TRUE(BlocksGame::musicTempo(l) >= BlocksGame::musicTempo(uint8_t(l - 1)));
    TEST_ASSERT_EQUAL_UINT16(160, BlocksGame::musicTempo(11));
    TEST_ASSERT_EQUAL_UINT16(160, BlocksGame::musicTempo(200));

    //the demo of seed 2 reaches level 3
    RecordingSink sink;
    BlocksGame game;
    game.setSound(&sink);
    game.reset(2);
    game.playMusic();
    uint8_t level = 1;
    bool b_drumsStarted = false;
    for(int t = 0; t < 50 * 120 && !game.isOver() && game.level() < 3; t++){
        GameInput input;
        game.demo(input);
        uint32_t backBefore = sink.cueCount(SoundLayer::BACK);
        game.update(input);
        if(game.level() != level){
            level = game.level();
            TEST_ASSERT_EQUAL_UINT16(BlocksGame::musicTempo(level), sink.getTempo(SoundLayer::BACK));
            TEST_ASSERT_TRUE(sink.heard(SoundLayer::FRONT, BlocksGame::levelHertz));
            if(level == 2) TEST_ASSERT_EQUAL_UINT32(backBefore, sink.cueCount(SoundLayer::BACK));    //no restart
            if(level == 3){
                TEST_ASSERT_EQUAL_UINT32(backBefore + 1, sink.cueCount(SoundLayer::BACK));
                b_drumsStarted = hasDrums(sink.last(SoundLayer::BACK)->cue);
            }
        }
    }
    TEST_ASSERT_EQUAL_UINT8(3, game.level());
    TEST_ASSERT_TRUE(b_drumsStarted);
    //resumed at level 3: with the drums, at the level's tempo
    game.playMusic();
    TEST_ASSERT_TRUE(hasDrums(sink.last(SoundLayer::BACK)->cue));
    TEST_ASSERT_EQUAL_UINT16(112, sink.getTempo(SoundLayer::BACK));
}

void test_move_rotate_and_landing_sounds(){
    RecordingSink sink;
    BlocksGame game;
    game.setSound(&sink);
    game.reset(3);
    while(game.pieceType() == 3) hardDrop(game);   //the square does not turn
    sink.clear();
    GameInput up;
    up.stick[1] = 1.0f;
    game.update(up);
    TEST_ASSERT_EQUAL_UINT32(1, sink.noteCount(SoundLayer::FRONT));
    TEST_ASSERT_EQUAL_UINT16(BlocksGame::moveHertz, sink.lastHertz(SoundLayer::FRONT));
    //held without a step across: no tick
    game.update(up);
    TEST_ASSERT_EQUAL_UINT32(1, sink.count(SoundLayer::FRONT));
    GameInput click;
    click.clicked[1] = true;
    game.update(click);
    TEST_ASSERT_EQUAL_UINT16(BlocksGame::rotateHertz, sink.lastHertz(SoundLayer::FRONT));
    //a step and a turn while an effect still sounds: they give way
    sink.setPlaying(SoundLayer::FRONT, true);
    uint32_t count = sink.count(SoundLayer::FRONT);
    game.update(GameInput());
    game.update(up);
    game.update(click);
    TEST_ASSERT_EQUAL_UINT32(count, sink.count(SoundLayer::FRONT));
    sink.setPlaying(SoundLayer::FRONT, false);
    //a drop: the landing thud, a falling sweep from landHertz, synth
    hardDrop(game);
    const RecordingSink::Play* p = sink.last(SoundLayer::FRONT);
    TEST_ASSERT_FALSE(p->b_note);
    TEST_ASSERT_EQUAL_UINT16(BlocksGame::landHertz, p->note.hertz);
    TEST_ASSERT_TRUE(p->cue.notes[p->cue.noteCount - 1].hertz < BlocksGame::landHertz);
    TEST_ASSERT_TRUE(p->cue.lengthMillis() < 150);
    checkNewPlays(sink, SoundLayer::FRONT, 0, false);
    //effects never touch the music
    TEST_ASSERT_EQUAL_UINT32(0, sink.count(SoundLayer::BACK));
}

void test_clear_sweeps_and_the_four_column_fanfare(){
    //over demo games: every clear plays a rising sweep from clearHertz, higher for more columns, or the fanfare for
    //four; everything in front is synth until the game over
    int clears = 0, fours = 0;
    for(uint32_t seed = 1; seed <= 40 && (clears < 5 || fours == 0); seed++){
        RecordingSink sink;
        BlocksGame game;
        game.setSound(&sink);
        game.reset(seed);
        for(int t = 0; t < 50 * 180 && !game.isOver(); t++){
            GameInput input;
            game.demo(input);
            uint16_t columns = game.columnsCleared();
            uint32_t points = game.score();
            uint32_t before = sink.count(SoundLayer::FRONT);
            game.update(input);
            checkNewPlays(sink, SoundLayer::FRONT, before, game.isOver());
            if(game.score() == points) continue;
            while(game.isClearing()) game.update(GameInput());
            uint16_t count = uint16_t(game.columnsCleared() - columns);
            //the clear's sound was the last one at the lock, unless the level rose after the wipe
            const RecordingSink::Play* p = sink.last(SoundLayer::FRONT);
            if(p->note.hertz == BlocksGame::levelHertz) p = sink.last(SoundLayer::FRONT, 1);
            TEST_ASSERT_FALSE(p->b_note);
            if(count == 4){
                TEST_ASSERT_EQUAL_UINT16(BlocksGame::fourHertz, p->note.hertz);
                fours++;
            }
            else{
                TEST_ASSERT_EQUAL_UINT16(BlocksGame::clearHertz, p->note.hertz);
                TEST_ASSERT_EQUAL_UINT16(1000 + 500 * count, p->cue.notes[p->cue.noteCount - 1].hertz);
                clears++;
            }
            TEST_ASSERT_TRUE(p->cue.lengthMillis() < BlocksGame::clearTicks * 20);
        }
    }
    TEST_ASSERT_TRUE(clears >= 5);
    TEST_ASSERT_TRUE(fours > 0);
}

void test_game_over_stops_the_music_and_falls(){
    RecordingSink sink;
    BlocksGame game;
    game.setSound(&sink);
    game.reset(5);
    game.playMusic();
    while(!game.isOver()){
        hardDrop(game);
        while(game.isClearing()) idle(game, 1);
    }
    TEST_ASSERT_EQUAL_UINT32(1, sink.stopCount(SoundLayer::BACK));
    const RecordingSink::Play* p = sink.last(SoundLayer::FRONT);
    TEST_ASSERT_EQUAL_UINT16(BlocksGame::overHertz, p->note.hertz);
    TEST_ASSERT_TRUE(p->note.kind == SoundNote::Kind::SQUARE);
    for(uint16_t i = 1; i < p->cue.noteCount; i++) TEST_ASSERT_TRUE(p->cue.notes[i].hertz < p->cue.notes[i - 1].hertz);
    //over before the Arcade's own phrase at 400 ms
    TEST_ASSERT_TRUE(p->cue.lengthMillis() <= 400);
    //no music on resume once it is over, nothing more
    uint32_t total = sink.total();
    game.playMusic();
    idle(game, 50);
    TEST_ASSERT_EQUAL_UINT32(total, sink.total());
}

void test_demo_without_sink_is_silent_and_the_same(){
    //sound never changes the game: with a recording sink and without, the same seed plays the same frames
    RecordingSink sink;
    BlocksGame loud, quiet;
    loud.setSound(&sink);
    loud.reset(1234);
    quiet.reset(1234);
    loud.playMusic();
    quiet.playMusic();
    GFXcanvas1 a(128, 32), b(128, 32);
    for(int t = 0; t < 50 * 90; t++){
        GameInput inLoud, inQuiet;
        loud.demo(inLoud);
        quiet.demo(inQuiet);
        loud.update(inLoud);
        quiet.update(inQuiet);
        a.fillScreen(0);
        b.fillScreen(0);
        loud.draw(a);
        quiet.draw(b);
        TEST_ASSERT_EQUAL_UINT32(GameContract::frameHash(a), GameContract::frameHash(b));
        if(loud.isOver()){
            loud.reset(uint32_t(t));
            quiet.reset(uint32_t(t));
        }
    }
    TEST_ASSERT_TRUE(sink.count(SoundLayer::FRONT) > 50);
    //the sink taken away (the picker's demo): nothing more reaches it
    loud.setSound(nullptr);
    uint32_t total = sink.total();
    for(int t = 0; t < 50 * 30; t++){
        GameInput input;
        loud.demo(input);
        loud.update(input);
        if(loud.isOver()) loud.reset(uint32_t(t));
    }
    loud.playMusic();
    TEST_ASSERT_EQUAL_UINT32(total, sink.total());
}

//the score and the level read in the sideways grip (game up = screen right): the text runs down the screen, the
//letters' tops at the right; the score at the right edge, the level just right of the entry line, both centred
void test_hud_text_is_sideways(){
    BlocksGame game;
    game.reset(1);
    GFXcanvas1 canvas(128, 32);
    canvas.fillScreen(0);
    game.draw(canvas);
    //"0": 5 rows down the screen (14 to 18) in the columns 121 to 127
    int lit = 0;
    for(int16_t x = 121; x < 128; x++){
        for(int16_t y = 0; y < 32; y++){
            if(!canvas.getPixel(x, y)) continue;
            lit++;
            TEST_ASSERT_TRUE(y >= 14 && y <= 18);
        }
    }
    TEST_ASSERT_TRUE(lit > 8);
    //its top at the right: the 0's top row (screen column 127) lit across its middle, nothing in column 120
    TEST_ASSERT_TRUE(canvas.getPixel(127, 16));
    for(int16_t y = 0; y < 32; y++) TEST_ASSERT_FALSE(canvas.getPixel(120, y));
    //"L1": the L's stem along the top (column 94, rows 11 on), its foot down column 88 for 5 rows
    for(int16_t y = 11; y <= 15; y++) TEST_ASSERT_TRUE(canvas.getPixel(88, y));
    for(int16_t x = 88; x <= 94; x++) TEST_ASSERT_TRUE(canvas.getPixel(x, 11));
    TEST_ASSERT_FALSE(canvas.getPixel(94, 15));
    //1 px clear of the entry line (column 86)
    for(int16_t y = 0; y < 32; y++) TEST_ASSERT_FALSE(canvas.getPixel(87, y));
}

int main(){
    UNITY_BEGIN();
    RUN_TEST(test_contract);
    RUN_TEST(test_seven_bag_deals_each_piece_once);
    RUN_TEST(test_left_stick_moves_across_with_repeat_and_stays_in_the_well);
    RUN_TEST(test_gravity_and_soft_drop_move_toward_the_floor);
    RUN_TEST(test_hard_drop_lands_on_the_floor_and_brings_the_next_piece);
    RUN_TEST(test_rotation_by_click_and_flick_once_per_flick);
    RUN_TEST(test_cleared_columns_score_by_count_and_level);
    RUN_TEST(test_stack_reaching_the_entry_ends_the_game);
    RUN_TEST(test_demo_uses_only_the_player_controls);
    RUN_TEST(test_music_starts_in_play_music);
    RUN_TEST(test_music_tempo_follows_the_level_and_drums_join_at_level_3);
    RUN_TEST(test_move_rotate_and_landing_sounds);
    RUN_TEST(test_clear_sweeps_and_the_four_column_fanfare);
    RUN_TEST(test_game_over_stops_the_music_and_falls);
    RUN_TEST(test_demo_without_sink_is_silent_and_the_same);
    RUN_TEST(test_hud_text_is_sideways);
    return UNITY_END();
}
