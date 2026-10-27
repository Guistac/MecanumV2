//Host tests of BLOCKS, the sideways falling-blocks game [D-148]: pio test -e native -f test_game_blocks
#include <unity.h>
#include <HostGfx.h>
#include <GameContract.h>
#include <BlocksGame.h>

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
    return UNITY_END();
}
