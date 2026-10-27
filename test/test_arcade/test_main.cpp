//Host tests of the remote's games menu [D-148]: pio test -e native -f test_arcade
#include <unity.h>
#include <HostGfx.h>
#include <Arcade.h>

void setUp(){}
void tearDown(){}

static GameInput sticks(bool b_left, bool b_right){
    GameInput input;
    input.held[0] = b_left;
    input.held[1] = b_right;
    return input;
}

void test_gate_needs_both_held_two_seconds(){
    Arcade arcade;
    TEST_ASSERT_FALSE(arcade.updateGate(true, false, 0));
    TEST_ASSERT_FALSE(arcade.updateGate(true, true, 100));
    TEST_ASSERT_FALSE(arcade.updateGate(true, true, 2099));
    TEST_ASSERT_TRUE(arcade.updateGate(true, true, 2100));
    //once only, then the buttons must be released first
    TEST_ASSERT_FALSE(arcade.updateGate(true, true, 5000));
    TEST_ASSERT_FALSE(arcade.updateGate(true, true, 8000));
    arcade.updateGate(true, false, 8100);
    TEST_ASSERT_FALSE(arcade.updateGate(true, true, 8200));
    TEST_ASSERT_TRUE(arcade.updateGate(true, true, 10200));
}

void test_gate_closed_resets_and_needs_release(){
    Arcade arcade;
    arcade.updateGate(true, false, 0);
    arcade.updateGate(true, true, 100);
    //leaving the About page (or the speed switch leaving OFF) restarts the count
    TEST_ASSERT_FALSE(arcade.updateGate(false, true, 1500));
    //held from before the gate opened: does not count until released
    TEST_ASSERT_FALSE(arcade.updateGate(true, true, 1600));
    TEST_ASSERT_FALSE(arcade.updateGate(true, true, 5000));
    arcade.updateGate(true, false, 5100);
    arcade.updateGate(true, true, 5200);
    TEST_ASSERT_TRUE(arcade.updateGate(true, true, 7200));
}

void test_enter_ignores_clicks_until_released(){
    Arcade arcade;
    arcade.enter(1000);
    TEST_ASSERT_TRUE(arcade.isActive());
    TEST_ASSERT_TRUE(arcade.getMode() == Arcade::Mode::PICKER);
    //still held from the gesture, then a release of one and a press again: no game yet
    arcade.update(sticks(true, true), false, false, 1020);
    arcade.update(sticks(false, true), false, false, 1040);
    arcade.update(sticks(true, true), false, false, 1060);
    TEST_ASSERT_TRUE(arcade.getMode() == Arcade::Mode::PICKER);
    arcade.update(sticks(false, false), false, false, 1080);
    arcade.update(sticks(true, false), false, false, 1100);
    TEST_ASSERT_TRUE(arcade.getMode() == Arcade::Mode::PLAYING);
}

void test_picker_keys_choose_and_wrap(){
    Arcade arcade;
    arcade.enter(0);
    TEST_ASSERT_EQUAL_UINT8(0, arcade.getSelected());
    arcade.update(sticks(false, false), true, false, 20);
    TEST_ASSERT_EQUAL_UINT8(Arcade::gameCount - 1, arcade.getSelected());
    arcade.update(sticks(false, false), false, true, 40);
    arcade.update(sticks(false, false), false, true, 60);
    TEST_ASSERT_EQUAL_UINT8(1, arcade.getSelected());
}

void test_playing_left_key_quits_right_key_pauses(){
    Arcade arcade;
    arcade.enter(0);
    arcade.update(sticks(false, false), false, false, 20);
    arcade.update(sticks(false, true), false, false, 40);
    TEST_ASSERT_TRUE(arcade.getMode() == Arcade::Mode::PLAYING);
    arcade.update(sticks(false, false), false, true, 60);
    TEST_ASSERT_TRUE(arcade.isPaused());
    arcade.update(sticks(false, false), false, true, 80);
    TEST_ASSERT_FALSE(arcade.isPaused());
    arcade.update(sticks(false, false), true, false, 100);
    TEST_ASSERT_TRUE(arcade.getMode() == Arcade::Mode::PICKER);
}

void test_the_starting_click_is_not_held_in_the_game(){
    //the Cave crashed into its ceiling: the click that started it was still held, read as full thrust
    Arcade arcade;
    arcade.enter(0);
    arcade.update(sticks(false, false), false, false, 20);
    for(uint8_t i = 0; i < Arcade::gameCount; i++){
        if(strcmp(arcade.game(arcade.getSelected()).name(), "CAVE") == 0) break;
        arcade.update(sticks(false, false), false, true, 40 + i * 20);
    }
    TEST_ASSERT_EQUAL_STRING("CAVE", arcade.game(arcade.getSelected()).name());
    uint32_t now = 1000;
    arcade.update(sticks(true, false), false, false, now);
    TEST_ASSERT_TRUE(arcade.getMode() == Arcade::Mode::PLAYING);
    //the click held on for 0.4 s: the craft must not climb (y grows downward); before the fix it took full thrust
    const CaveGame& cave = static_cast<const CaveGame&>(arcade.game(arcade.getSelected()));
    float startY = cave.getCraftY();
    for(int t = 0; t < 20; t++) arcade.update(sticks(true, false), false, false, now += 20);
    TEST_ASSERT_TRUE(cave.getCraftY() >= startY);
    //released and pressed again: now it is thrust
    arcade.update(sticks(false, false), false, false, now += 20);
    float speedBefore = cave.getVerticalSpeed();
    for(int t = 0; t < 5; t++) arcade.update(sticks(true, false), false, false, now += 20);
    TEST_ASSERT_TRUE(cave.getVerticalSpeed() < speedBefore);
}

void test_exit_gesture_and_idle(){
    Arcade arcade;
    arcade.enter(0);
    arcade.update(sticks(false, false), false, false, 20);
    arcade.update(sticks(true, true), false, false, 40);
    TEST_ASSERT_TRUE(arcade.getMode() == Arcade::Mode::PICKER);    //both together never starts a game
    arcade.update(sticks(true, true), false, false, 2000);
    TEST_ASSERT_FALSE(arcade.exitRequested());
    arcade.update(sticks(true, true), false, false, 2040);
    TEST_ASSERT_TRUE(arcade.exitRequested());
    arcade.leave();
    TEST_ASSERT_FALSE(arcade.isActive());
    //still held after leaving: the gate waits for the release
    TEST_ASSERT_FALSE(arcade.updateGate(true, true, 2100));
    TEST_ASSERT_FALSE(arcade.updateGate(true, true, 9000));

    Arcade idle;
    idle.enter(0);
    idle.update(sticks(false, false), false, false, 100000);
    TEST_ASSERT_FALSE(idle.exitRequested());
    idle.update(sticks(false, false), false, false, 120100);
    TEST_ASSERT_TRUE(idle.exitRequested());
}

void test_ticks_are_capped_after_a_long_loop(){
    //a 5 s stall (radio init, flash write) does not run 250 ticks at once
    Arcade arcade;
    arcade.enter(0);
    arcade.update(sticks(false, false), false, false, 5000);
    arcade.update(sticks(false, false), false, false, 5020);
    TEST_ASSERT_TRUE(arcade.isActive());
}

void test_every_game_draws_in_every_mode(){
    Arcade arcade;
    GFXcanvas1 canvas(128, 32);
    arcade.enter(0);
    uint32_t now = 0;
    arcade.update(sticks(false, false), false, false, now += 20);
    for(uint8_t i = 0; i < Arcade::gameCount; i++){
        for(int t = 0; t < 50; t++){
            arcade.update(sticks(false, false), false, false, now += 20);
            arcade.draw(canvas);
        }
        arcade.update(sticks(true, false), false, false, now += 20);
        arcade.update(sticks(false, false), false, false, now += 20);
        TEST_ASSERT_TRUE(arcade.getMode() == Arcade::Mode::PLAYING);
        for(int t = 0; t < 50; t++){
            arcade.update(sticks(false, false), false, false, now += 20);
            arcade.draw(canvas);
        }
        arcade.update(sticks(false, false), true, false, now += 20);
        arcade.update(sticks(false, false), false, true, now += 20);
    }
}

int main(){
    UNITY_BEGIN();
    RUN_TEST(test_gate_needs_both_held_two_seconds);
    RUN_TEST(test_gate_closed_resets_and_needs_release);
    RUN_TEST(test_enter_ignores_clicks_until_released);
    RUN_TEST(test_picker_keys_choose_and_wrap);
    RUN_TEST(test_playing_left_key_quits_right_key_pauses);
    RUN_TEST(test_the_starting_click_is_not_held_in_the_game);
    RUN_TEST(test_exit_gesture_and_idle);
    RUN_TEST(test_ticks_are_capped_after_a_long_loop);
    RUN_TEST(test_every_game_draws_in_every_mode);
    return UNITY_END();
}
