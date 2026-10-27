//Host tests of the remote's Snake game [D-157] and its sound: pio test -e native -f test_game_snake
#include <unity.h>
#include <HostGfx.h>
#include <GameContract.h>
#include <SnakeGame.h>
#include <RecordingSink.h>
#include <SoundKit.h>

void setUp(){}
void tearDown(){}

static GameInput push(float x, float y){
    GameInput input;
    input.stick[0] = x;
    input.stick[1] = y;
    return input;
}

//runs ticks with the same input until the head moves (one step), at most a second
static void stepOnce(SnakeGame& game, const GameInput& input){
    int16_t head = game.getHead();
    for(int t = 0; t < 50 && game.getHead() == head && !game.isDying(); t++) game.update(input);
}

void test_contract(){ GameContract::check<SnakeGame>(); }

void test_starts_still_then_moves_right(){
    SnakeGame game;
    game.reset(3);
    int16_t head = game.getHead();
    for(int t = 0; t < 20; t++) game.update(GameInput());
    TEST_ASSERT_EQUAL_INT16(head, game.getHead());
    stepOnce(game, GameInput());
    TEST_ASSERT_EQUAL_INT16(SnakeGame::neighbour(head, SnakeGame::RIGHT), game.getHead());
    TEST_ASSERT_EQUAL_UINT16(4, game.getLength());
}

void test_dominant_axis_and_no_reverse(){
    SnakeGame game;
    game.reset(3);
    //mostly sideways: right is the heading, the small up does not count
    stepOnce(game, push(0.9f, 0.3f));
    TEST_ASSERT_TRUE(game.getDirection() == SnakeGame::RIGHT);
    //straight back into the neck is ignored
    stepOnce(game, push(-1.0f, 0.0f));
    TEST_ASSERT_TRUE(game.getDirection() == SnakeGame::RIGHT);
    //below the threshold: nothing
    stepOnce(game, push(0.0f, 0.4f));
    TEST_ASSERT_TRUE(game.getDirection() == SnakeGame::RIGHT);
    //the right stick steers as well
    GameInput input;
    input.stick[3] = -0.8f;
    stepOnce(game, input);
    TEST_ASSERT_TRUE(game.getDirection() == SnakeGame::DOWN);
}

void test_quick_turns_are_buffered(){
    SnakeGame game;
    game.reset(3);
    stepOnce(game, GameInput());
    int16_t head = game.getHead();
    //up then left within one step: a U-turn over two steps, nothing lost
    game.update(push(0.0f, 1.0f));
    game.update(push(-1.0f, 0.0f));
    stepOnce(game, GameInput());
    TEST_ASSERT_TRUE(game.getDirection() == SnakeGame::UP);
    stepOnce(game, GameInput());
    TEST_ASSERT_TRUE(game.getDirection() == SnakeGame::LEFT);
    TEST_ASSERT_EQUAL_INT16(SnakeGame::neighbour(SnakeGame::neighbour(head, SnakeGame::UP), SnakeGame::LEFT),
        game.getHead());
}

void test_edges_wrap_around(){
    TEST_ASSERT_EQUAL_INT16(SnakeGame::cellAt(0, 4), SnakeGame::neighbour(SnakeGame::cellAt(SnakeGame::columns - 1, 4), SnakeGame::RIGHT));
    TEST_ASSERT_EQUAL_INT16(SnakeGame::cellAt(7, SnakeGame::rows - 1), SnakeGame::neighbour(SnakeGame::cellAt(7, 0), SnakeGame::UP));
    SnakeGame game;
    game.reset(3);
    for(int i = 0; i < SnakeGame::columns + 2 && !game.isDying(); i++) stepOnce(game, GameInput());
    //a whole lap to the right with no wall
    TEST_ASSERT_FALSE(game.isDying());
}

//plays the demo until the condition holds (or the game is over, or the time is up)
template <typename F>
static void playUntil(SnakeGame& game, F done, int seconds = 180){
    for(int t = 0; t < 50 * seconds && !done() && !game.isOver(); t++){
        GameInput input;
        game.demo(input);
        game.update(input);
    }
}

void test_eating_grows_scores_and_speeds_up(){
    SnakeGame game;
    game.reset(3);
    float slowest = game.getSpeed();
    //the demo player steers to the food until 8 are eaten (still on the first level: no mouse there)
    playUntil(game, [&]{ return game.getFoodEaten() >= 8; });
    TEST_ASSERT_EQUAL_UINT16(8, game.getFoodEaten());
    TEST_ASSERT_EQUAL_UINT16(1, game.getLevel());
    TEST_ASSERT_EQUAL_UINT16(SnakeGame::startLength + 8, game.getLength());
    //the points: 1 per food plus 5 per bonus (which does not grow the snake)
    TEST_ASSERT_EQUAL_UINT32(0, (game.score() - 8) % SnakeGame::bonusPoints);
    TEST_ASSERT_TRUE(game.getSpeed() > slowest);
    //the food is always on a free cell
    TEST_ASSERT_FALSE(game.isOccupied(game.getFood()));
    TEST_ASSERT_FALSE(game.isWall(game.getFood()));
}

void test_biting_itself_ends_the_game(){
    SnakeGame game;
    game.reset(3);
    //grow first so a tight square bites the body
    playUntil(game, [&]{ return game.getFoodEaten() >= 5; });
    TEST_ASSERT_FALSE(game.isDying());
    //turn three times the same way: the head runs into its own body
    SnakeGame::Direction d = game.getDirection();
    for(int k = 1; k <= 3 && !game.isDying(); k++){
        uint8_t turn = uint8_t((d + k) % 4);
        float x = turn == SnakeGame::RIGHT ? 1.0f : turn == SnakeGame::LEFT ? -1.0f : 0.0f;
        float y = turn == SnakeGame::UP ? 1.0f : turn == SnakeGame::DOWN ? -1.0f : 0.0f;
        stepOnce(game, push(x, y));
    }
    TEST_ASSERT_TRUE(game.isDying());
    TEST_ASSERT_FALSE(game.isOver());
    for(int t = 0; t < 100; t++) game.update(GameInput());
    TEST_ASSERT_TRUE(game.isOver());
}

void test_demo_moves_by_the_rules(){
    //the demo plays by the same rules: every step the head moves to a neighbour, never onto a wall while alive
    SnakeGame game;
    game.reset(42);
    int16_t head = game.getHead();
    for(int t = 0; t < 50 * 120 && !game.isOver(); t++){
        GameInput input;
        game.demo(input);
        uint16_t level = game.getLevel();
        game.update(input);
        if(game.getLevel() != level){
            head = game.getHead();
            continue;
        }
        if(game.getHead() != head && !game.isDying()){
            bool b_neighbour = false;
            for(uint8_t d = 0; d < 4; d++) if(SnakeGame::neighbour(head, d) == game.getHead()) b_neighbour = true;
            TEST_ASSERT_TRUE(b_neighbour);
            TEST_ASSERT_FALSE(game.isWall(game.getHead()));
            head = game.getHead();
        }
    }
}

void test_layouts(){
    //from open to tighter; every free cell reachable; the snake starts with room ahead; the score corner is free
    uint16_t wallsBefore = 0;
    for(uint8_t layout = 0; layout < SnakeGame::layoutCount; layout++){
        uint16_t walls = 0;
        int16_t firstFree = -1;
        for(int16_t cell = 0; cell < SnakeGame::cellCount; cell++){
            if(SnakeGame::layoutWall(layout, cell)) walls++;
            else if(firstFree < 0) firstFree = cell;
        }
        TEST_ASSERT_TRUE(layout == 0 ? walls == 0 : walls > 0);
        //the last one is the tightest
        if(layout == SnakeGame::layoutCount - 1) TEST_ASSERT_TRUE(walls > wallsBefore);
        if(walls > wallsBefore) wallsBefore = walls;
        static bool seen[SnakeGame::cellCount];
        static int16_t queue[SnakeGame::cellCount];
        memset(seen, 0, sizeof(seen));
        int16_t head = 0, end = 0;
        seen[firstFree] = true;
        queue[end++] = firstFree;
        while(head < end){
            int16_t cell = queue[head++];
            for(uint8_t d = 0; d < 4; d++){
                int16_t next = SnakeGame::neighbour(cell, d);
                if(seen[next] || SnakeGame::layoutWall(layout, next)) continue;
                seen[next] = true;
                queue[end++] = next;
            }
        }
        TEST_ASSERT_EQUAL_INT16(SnakeGame::cellCount - walls, end);
        if(layout > 0){
            int16_t start = SnakeGame::layoutStart(layout);
            for(int16_t i = 0; i < int16_t(SnakeGame::startLength) + 5; i++){
                TEST_ASSERT_FALSE(SnakeGame::layoutWall(layout, int16_t(start + i)));
            }
        }
        for(int16_t row = 0; row < 3; row++){
            for(int16_t column = 34; column < SnakeGame::columns; column++){
                TEST_ASSERT_FALSE(SnakeGame::layoutWall(layout, SnakeGame::cellAt(column, row)));
            }
        }
    }
}

void test_ten_food_make_a_new_level(){
    SnakeGame game;
    game.reset(11);
    playUntil(game, [&]{ return game.getLevel() > 1; });
    TEST_ASSERT_EQUAL_UINT16(2, game.getLevel());
    TEST_ASSERT_EQUAL_UINT16(SnakeGame::foodPerLevel, game.getFoodEaten());
    float speed = game.getSpeed();
    //a cleared board with the bars, the snake back to its start length at the layout's start, the pill showing
    TEST_ASSERT_TRUE(game.isLevelShown());
    TEST_ASSERT_EQUAL_UINT16(SnakeGame::startLength, game.getLength());
    TEST_ASSERT_EQUAL_INT16(SnakeGame::layoutStart(1) + 3, game.getHead());
    TEST_ASSERT_EQUAL_INT16(-1, game.getBonus());
    TEST_ASSERT_EQUAL_INT16(-1, game.getMouse());
    TEST_ASSERT_TRUE(game.isWall(SnakeGame::cellAt(10, 3)));
    //it stays still for about a second, a push meanwhile does nothing
    int16_t head = game.getHead();
    for(int t = 0; t < 50; t++) game.update(push(0.0f, 1.0f));
    TEST_ASSERT_TRUE(game.isLevelShown());
    TEST_ASSERT_EQUAL_INT16(head, game.getHead());
    for(int t = 0; t < 10; t++) game.update(GameInput());
    TEST_ASSERT_FALSE(game.isLevelShown());
    //the speed did not drop with the new level
    TEST_ASSERT_TRUE(game.getSpeed() >= speed);
}

void test_walls_kill(){
    SnakeGame game;
    game.reset(11);
    playUntil(game, [&]{ return game.getLevel() > 1; });
    for(int t = 0; t < 60; t++) game.update(GameInput());
    //the head at (13, 4) under the left bar in row 3: up runs into it
    TEST_ASSERT_EQUAL_INT16(SnakeGame::cellAt(13, 4), game.getHead());
    stepOnce(game, push(0.0f, 1.0f));
    TEST_ASSERT_TRUE(game.isDying());
    TEST_ASSERT_EQUAL_INT16(SnakeGame::cellAt(13, 4), game.getHead());
}

void test_bonus_food(){
    //over a few demo games: the bonus sits on free cells for at most 5 s, it is worth 5 and does not grow the snake
    int eatenBonus = 0;
    for(uint32_t seed = 1; seed <= 6; seed++){
        SnakeGame game;
        game.reset(seed);
        int lifetime = 0;
        for(int t = 0; t < 50 * 120 && !game.isOver(); t++){
            GameInput input;
            game.demo(input);
            int16_t bonus = game.getBonus();
            uint32_t points = game.score();
            uint16_t length = game.getLength();
            uint16_t level = game.getLevel();
            uint16_t food = game.getFoodEaten();
            game.update(input);
            if(game.getBonus() >= 0 && game.getBonus() != bonus){
                int16_t b = game.getBonus();
                int16_t cells[4] = { b, int16_t(b + 1), int16_t(b + SnakeGame::columns), int16_t(b + SnakeGame::columns + 1) };
                for(int i = 0; i < 4; i++){
                    TEST_ASSERT_FALSE(game.isOccupied(cells[i]));
                    TEST_ASSERT_FALSE(game.isWall(cells[i]));
                    TEST_ASSERT_TRUE(cells[i] != game.getFood());
                }
                lifetime = 0;
            }
            if(game.getBonus() >= 0) lifetime++;
            TEST_ASSERT_TRUE(lifetime <= 250);
            if(bonus >= 0 && game.getBonus() < 0 && game.getLevel() == level){
                if(game.score() > points){
                    TEST_ASSERT_EQUAL_UINT32(points + SnakeGame::bonusPoints + (game.getFoodEaten() - food), game.score());
                    TEST_ASSERT_EQUAL_UINT16(length + (game.getFoodEaten() - food), game.getLength());
                    eatenBonus++;
                }
            }
        }
    }
    TEST_ASSERT_TRUE(eatenBonus > 0);

    //left alone the snake runs straight along its row: the bonus comes, blinks, and goes after 5 s for nothing
    SnakeGame idle;
    idle.reset(8);
    int appeared = 0, vanished = 0, onTicks = 0;
    for(int t = 0; t < 50 * 40; t++){
        int16_t bonus = idle.getBonus();
        idle.update(GameInput());
        if(bonus < 0 && idle.getBonus() >= 0) appeared++;
        if(bonus >= 0 && idle.getBonus() < 0) vanished++;
        if(idle.getBonus() >= 0) onTicks++;
    }
    TEST_ASSERT_FALSE(idle.isDying());
    TEST_ASSERT_TRUE(appeared > 0 && vanished > 0);
    TEST_ASSERT_TRUE(onTicks <= appeared * 250);
}

void test_mouse_runs_and_is_caught(){
    //from level 2: the mouse moves one cell at a time, never onto the snake or a wall, slower than the snake;
    //caught it is worth 10 and grows the snake
    int caught = 0, mouseMoves = 0, snakeSteps = 0;
    for(uint32_t seed = 1; seed <= 4; seed++){
        SnakeGame game;
        game.reset(seed);
        for(int t = 0; t < 50 * 120 && !game.isOver(); t++){
            GameInput input;
            game.demo(input);
            int16_t mouse = game.getMouse();
            int16_t head = game.getHead();
            uint32_t points = game.score();
            uint16_t length = game.getLength();
            uint16_t level = game.getLevel();
            game.update(input);
            if(game.getLevel() == 1) TEST_ASSERT_EQUAL_INT16(-1, game.getMouse());
            if(game.getLevel() != level || game.isDying()) continue;
            if(game.getMouse() >= 0){
                TEST_ASSERT_FALSE(game.isOccupied(game.getMouse()));
                TEST_ASSERT_FALSE(game.isWall(game.getMouse()));
            }
            if(mouse >= 0 && game.getMouse() >= 0){
                if(game.getHead() != head) snakeSteps++;
                if(game.getMouse() != mouse){
                    bool b_neighbour = false;
                    for(uint8_t d = 0; d < 4; d++) if(SnakeGame::neighbour(mouse, d) == game.getMouse()) b_neighbour = true;
                    TEST_ASSERT_TRUE(b_neighbour);
                    mouseMoves++;
                }
            }
            if(mouse >= 0 && game.getMouse() < 0){
                TEST_ASSERT_TRUE(game.score() >= points + SnakeGame::mousePoints);
                TEST_ASSERT_EQUAL_UINT16(length + 1, game.getLength());
                caught++;
            }
        }
    }
    TEST_ASSERT_TRUE(caught > 0);
    TEST_ASSERT_TRUE(mouseMoves > 20);
    TEST_ASSERT_TRUE(mouseMoves * 2 <= snakeSteps + 4);
}

//sound [D-157]: every note of the plays a tick added in front is a synth note (no timer square, it would cut the
//music), unless b_squaresAllowed (the death)
static void checkNewPlays(const RecordingSink& sink, uint32_t before, bool b_squaresAllowed){
    uint32_t added = sink.count(SoundLayer::FRONT) - before;
    for(uint32_t k = 0; k < added && k < RecordingSink::keep; k++){
        const RecordingSink::Play* p = sink.last(SoundLayer::FRONT, uint8_t(k));
        TEST_ASSERT_EQUAL_UINT8(SoundPriority::GAME, p->priority);
        if(b_squaresAllowed) continue;
        if(p->b_note) TEST_ASSERT_TRUE(p->note.kind == SoundNote::Kind::SYNTH);
        else for(uint16_t i = 0; i < p->cue.noteCount; i++) TEST_ASSERT_TRUE(p->cue.notes[i].kind == SoundNote::Kind::SYNTH);
    }
}

void test_music_starts_in_play_music(){
    RecordingSink sink;
    SnakeGame game;
    game.setSound(&sink);
    game.reset(3);
    TEST_ASSERT_EQUAL_UINT32(0, sink.total());
    game.playMusic();
    TEST_ASSERT_EQUAL_UINT32(1, sink.cueCount(SoundLayer::BACK));
    TEST_ASSERT_EQUAL_UINT32(0, sink.count(SoundLayer::FRONT));
    TEST_ASSERT_EQUAL_UINT16(100, sink.getTempo(SoundLayer::BACK));
    const SoundCue& cue = sink.last(SoundLayer::BACK)->cue;
    TEST_ASSERT_EQUAL_UINT16(SnakeGame::tuneSteps * SnakeGame::tuneStepMillis, cue.loopMillis);
    TEST_ASSERT_TRUE(cue.lengthMillis() <= cue.loopMillis);
    //melody, bass and drums, in time order
    bool voices[3] = {false, false, false};
    for(uint16_t i = 0; i < cue.noteCount; i++){
        TEST_ASSERT_TRUE(cue.notes[i].voice < 3);
        voices[cue.notes[i].voice] = true;
        TEST_ASSERT_TRUE(cue.notes[i].kind == SoundNote::Kind::SYNTH);
        if(i) TEST_ASSERT_TRUE(cue.notes[i - 1].atMillis <= cue.notes[i].atMillis);
        //written for the disc: the melody from A5 on a full square, the bass short and no lower than A3
        if(cue.notes[i].voice == 0) TEST_ASSERT_TRUE(cue.notes[i].hertz >= 880 && cue.notes[i].wave == Synth::Wave::SQUARE);
        if(cue.notes[i].voice == 1) TEST_ASSERT_TRUE(cue.notes[i].hertz >= 220 && cue.notes[i].millis <= 150);
    }
    TEST_ASSERT_TRUE(voices[0] && voices[1] && voices[2]);
    //every note fits the list (none dropped): 44 of melody, 32 of bass, 4 bars of drums (4 kicks, 2 snares, 5 hats)
    TEST_ASSERT_EQUAL_UINT16(44 + 32 + 4 * (4 * SoundKit::kickNoteCount + 2 + 5), cue.noteCount);
}

void test_new_level_jingle_and_faster_music(){
    TEST_ASSERT_EQUAL_UINT16(100, SnakeGame::musicTempo(1));
    TEST_ASSERT_EQUAL_UINT16(106, SnakeGame::musicTempo(2));
    for(uint16_t l = 2; l < 40; l++) TEST_ASSERT_TRUE(SnakeGame::musicTempo(l) >= SnakeGame::musicTempo(uint16_t(l - 1)));
    TEST_ASSERT_EQUAL_UINT16(160, SnakeGame::musicTempo(11));
    TEST_ASSERT_EQUAL_UINT16(160, SnakeGame::musicTempo(1000));

    RecordingSink sink;
    SnakeGame game;
    game.setSound(&sink);
    game.reset(11);
    game.playMusic();
    playUntil(game, [&]{ return game.getLevel() > 1; });
    TEST_ASSERT_EQUAL_UINT16(2, game.getLevel());
    TEST_ASSERT_EQUAL_UINT16(SnakeGame::levelHertz, sink.lastHertz(SoundLayer::FRONT));
    TEST_ASSERT_EQUAL_UINT16(106, sink.getTempo(SoundLayer::BACK));
    //the music plays on (no restart), and on resume it starts at the level's tempo
    TEST_ASSERT_EQUAL_UINT32(1, sink.cueCount(SoundLayer::BACK));
    sink.clear();
    game.playMusic();
    TEST_ASSERT_EQUAL_UINT16(106, sink.getTempo(SoundLayer::BACK));
    TEST_ASSERT_EQUAL_UINT32(1, sink.cueCount(SoundLayer::BACK));
}

void test_eat_blips_climb_the_pentatonic(){
    TEST_ASSERT_EQUAL_UINT16(SnakeGame::eatHertz, SnakeGame::eatPitch(0));
    TEST_ASSERT_EQUAL_UINT16(587, SnakeGame::eatPitch(1));     //D5
    TEST_ASSERT_EQUAL_UINT16(659, SnakeGame::eatPitch(2));     //E5
    TEST_ASSERT_EQUAL_UINT16(784, SnakeGame::eatPitch(3));     //G5
    TEST_ASSERT_EQUAL_UINT16(SnakeGame::eatPitch(14), SnakeGame::eatPitch(40));
    RecordingSink sink;
    SnakeGame game;
    game.setSound(&sink);
    game.reset(3);
    uint16_t lastPitch = 0;
    int blips = 0;
    for(int t = 0; t < 50 * 180 && game.getFoodEaten() < 8 && !game.isOver(); t++){
        GameInput input;
        game.demo(input);
        uint16_t eaten = game.getFoodEaten();
        int16_t bonus = game.getBonus();
        uint32_t before = sink.count(SoundLayer::FRONT);
        game.update(input);
        checkNewPlays(sink, before, game.isDying());
        if(game.getFoodEaten() == eaten) continue;
        //a food (not with the diamond in the same step): a single synth blip, higher than the one before
        if(bonus >= 0 && game.getBonus() < 0) continue;
        const RecordingSink::Play* p = sink.last(SoundLayer::FRONT);
        TEST_ASSERT_TRUE(p->b_note);
        TEST_ASSERT_EQUAL_UINT16(SnakeGame::eatPitch(uint16_t(game.getLength() - SnakeGame::startLength - 1)), p->note.hertz);
        if(blips == 0) TEST_ASSERT_EQUAL_UINT16(SnakeGame::eatHertz, p->note.hertz);
        TEST_ASSERT_TRUE(p->note.hertz > lastPitch);
        TEST_ASSERT_TRUE(p->note.millis < 100);
        lastPitch = p->note.hertz;
        blips++;
    }
    TEST_ASSERT_TRUE(blips >= 6);
    //effects never touch the music
    TEST_ASSERT_EQUAL_UINT32(0, sink.count(SoundLayer::BACK));
}

void test_diamond_and_mouse_sounds(){
    //over demo games: the diamond twinkles when it appears, an arpeggio when eaten; the mouse squeaks when caught
    int twinkles = 0, arpeggios = 0, squeaks = 0;
    for(uint32_t seed = 1; seed <= 6; seed++){
        RecordingSink sink;
        SnakeGame game;
        game.setSound(&sink);
        game.reset(seed);
        game.playMusic();
        for(int t = 0; t < 50 * 120 && !game.isOver(); t++){
            GameInput input;
            game.demo(input);
            int16_t bonus = game.getBonus();
            int16_t mouse = game.getMouse();
            uint32_t points = game.score();
            uint16_t level = game.getLevel();
            uint32_t before = sink.count(SoundLayer::FRONT);
            game.update(input);
            checkNewPlays(sink, before, game.isDying());
            if(game.getLevel() != level || game.isDying()) continue;
            const RecordingSink::Play* p = sink.last(SoundLayer::FRONT);
            if(bonus >= 0 && game.getBonus() < 0 && game.score() >= points + SnakeGame::bonusPoints){
                TEST_ASSERT_EQUAL_UINT16(SnakeGame::bonusHertz, p->note.hertz);
                //up all the way
                for(uint16_t i = 1; i < p->cue.noteCount; i++) TEST_ASSERT_TRUE(p->cue.notes[i].hertz > p->cue.notes[i - 1].hertz);
                arpeggios++;
            }
            else if(mouse >= 0 && game.getMouse() < 0){
                TEST_ASSERT_EQUAL_UINT16(SnakeGame::squeakHertz, p->note.hertz);
                TEST_ASSERT_TRUE(p->cue.lengthMillis() < 150);
                squeaks++;
            }
            else if(bonus < 0 && game.getBonus() >= 0 && game.score() == points){
                TEST_ASSERT_EQUAL_UINT16(SnakeGame::twinkleHertz, p->note.hertz);
                twinkles++;
            }
        }
    }
    TEST_ASSERT_TRUE(twinkles > 0);
    TEST_ASSERT_TRUE(arpeggios > 0);
    TEST_ASSERT_TRUE(squeaks > 0);
}

void test_death_stops_the_music_and_falls(){
    RecordingSink sink;
    SnakeGame game;
    game.setSound(&sink);
    game.reset(11);
    game.playMusic();
    playUntil(game, [&]{ return game.getLevel() > 1; });
    for(int t = 0; t < 60; t++) game.update(GameInput());
    TEST_ASSERT_EQUAL_UINT32(0, sink.stopCount(SoundLayer::BACK));
    stepOnce(game, push(0.0f, 1.0f));      //into the bar above (test_walls_kill)
    TEST_ASSERT_TRUE(game.isDying());
    TEST_ASSERT_EQUAL_UINT32(1, sink.stopCount(SoundLayer::BACK));
    const RecordingSink::Play* p = sink.last(SoundLayer::FRONT);
    TEST_ASSERT_EQUAL_UINT16(SnakeGame::deathHertz, p->note.hertz);
    TEST_ASSERT_TRUE(p->note.kind == SoundNote::Kind::SQUARE);
    for(uint16_t i = 1; i < p->cue.noteCount; i++) TEST_ASSERT_TRUE(p->cue.notes[i].hertz < p->cue.notes[i - 1].hertz);
    TEST_ASSERT_TRUE(p->cue.lengthMillis() <= 600);
    //dying and over: no more sound, no music on resume
    uint32_t total = sink.total();
    for(int t = 0; t < 100; t++) game.update(GameInput());
    TEST_ASSERT_TRUE(game.isOver());
    game.playMusic();
    TEST_ASSERT_EQUAL_UINT32(total, sink.total());
}

void test_demo_without_sink_is_silent_and_the_same(){
    //sound never changes the game: with a recording sink and without, the same seed plays the same frames
    RecordingSink sink;
    SnakeGame loud, quiet;
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
    TEST_ASSERT_TRUE(sink.count(SoundLayer::FRONT) > 10);
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

int main(){
    UNITY_BEGIN();
    RUN_TEST(test_contract);
    RUN_TEST(test_starts_still_then_moves_right);
    RUN_TEST(test_dominant_axis_and_no_reverse);
    RUN_TEST(test_quick_turns_are_buffered);
    RUN_TEST(test_edges_wrap_around);
    RUN_TEST(test_eating_grows_scores_and_speeds_up);
    RUN_TEST(test_biting_itself_ends_the_game);
    RUN_TEST(test_demo_moves_by_the_rules);
    RUN_TEST(test_layouts);
    RUN_TEST(test_ten_food_make_a_new_level);
    RUN_TEST(test_walls_kill);
    RUN_TEST(test_bonus_food);
    RUN_TEST(test_mouse_runs_and_is_caught);
    RUN_TEST(test_music_starts_in_play_music);
    RUN_TEST(test_new_level_jingle_and_faster_music);
    RUN_TEST(test_eat_blips_climb_the_pentatonic);
    RUN_TEST(test_diamond_and_mouse_sounds);
    RUN_TEST(test_death_stops_the_music_and_falls);
    RUN_TEST(test_demo_without_sink_is_silent_and_the_same);
    return UNITY_END();
}
