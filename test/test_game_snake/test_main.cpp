//Host tests of the remote's Snake game [D-148]: pio test -e native -f test_game_snake
#include <unity.h>
#include <HostGfx.h>
#include <GameContract.h>
#include <SnakeGame.h>

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
    return UNITY_END();
}
