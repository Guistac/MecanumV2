//Host tests of the remote's games menu [D-157]: pio test -e native -f test_arcade
#include <unity.h>
#include <ctype.h>
#include <HostGfx.h>
#include <Arcade.h>
#include <RecordingSink.h>
#include <SoundKit.h>

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

//——— sound, cards, scores [D-157] ———

//a game the tests drive: it ends when told, with the score it is given; a note every tick, a tune in playMusic()
class TuneGame : public Game{
public:
    const char* name() const override { return "TUNE"; }
    void reset(uint32_t) override {
        b_over = false;
        ticks = 0;
    }
    void update(const GameInput&) override {
        ticks++;
        sound().playNote(SoundKit::synthNote(0, 1000, 10, 60), SoundPriority::GAME);
        if(endAt > 0 && ticks >= endAt) b_over = true;
    }
    void draw(Adafruit_GFX& out) const override {
        if(b_fill) out.fillScreen(1);
        else out.drawPixel(64, 16, 1);
    }
    bool isOver() const override { return b_over; }
    uint32_t score() const override { return points; }
    void overText(char* out, size_t size, uint32_t value) const override {
        if(over) snprintf(out, size, "%s", over);
        else Game::overText(out, size, value);
    }
    bool lowerScoreIsBetter() const override { return b_lower; }
    void demo(GameInput&) override {}
    void helpLines(const char* lines[3]) const override {
        lines[0] = "TEST LINE";
        lines[1] = nullptr;
        lines[2] = nullptr;
    }
    void playMusic() override {
        musicCalls++;
        static const SoundNote tune[] = {{0, SoundNote::Kind::SYNTH, 0, Synth::Wave::TRIANGLE, 330, 100, 70, 0}};
        sound().play(soundCue(tune, SoundPriority::GAME, 400), SoundLayer::BACK);
    }
    uint32_t points = 100;
    const char* over = nullptr;     //the game over text, when it says more than the score
    bool b_fill = false;            //draws the whole screen lit
    bool b_lower = false;
    int endAt = 0;
    int musicCalls = 0;
    int ticks = 0;
    bool b_over = false;
};

//the arcade with the TuneGame first, a RAM store and a recording sink, entered and the gesture's buttons released
struct Rig{
    Arcade arcade;
    RamArcadeStore store;
    RecordingSink sink;
    TuneGame tune;
    uint32_t now = 0;
    Rig(){
        arcade.useGame(0, tune);
        arcade.setStore(&store);
        arcade.setSound(&sink);
        arcade.enter(0);
        step();
    }
    void step(GameInput in = GameInput(), bool b_previous = false, bool b_next = false, bool b_previousHeld = false,
              bool b_nextHeld = false){
        now += 20;
        arcade.update(in, b_previous, b_next, now, b_previousHeld, b_nextHeld);
    }
    void idle(int ticks){ for(int i = 0; i < ticks; i++) step(); }
    void flick(float x, float y, int stick = 0){
        GameInput in;
        in.stick[stick * 2] = x;
        in.stick[stick * 2 + 1] = y;
        step(in);
        step();
    }
    void click(int button = 0){
        step(sticks(button == 0, button == 1));
        step();
    }
    void key(bool b_next){
        step(GameInput(), !b_next, b_next);
        step();
    }
    //plays the TuneGame to its end (it ends after `ticks` ticks)
    void playToOver(uint32_t points, int ticks = 10){
        tune.points = points;
        tune.endAt = ticks;
        click();
        for(int i = 0; i < ticks + 5 && arcade.getMode() == Arcade::Mode::PLAYING; i++) step();
    }
};

//the game over pill shows the game's overText (the score unless the game says more) and the rank; the dark box
//around it grows when the pill is wider than it
void test_game_over_shows_the_games_over_text(){
    Rig r;
    r.tune.b_fill = true;
    GFXcanvas1 canvas(128, 32);
    r.playToOver(0);
    r.arcade.draw(canvas);
    //the default box, 96 px: lit game left of it
    TEST_ASSERT_TRUE(canvas.getPixel(15, 23));
    TEST_ASSERT_FALSE(canvas.getPixel(16, 23));
    //a longer one with the rank (the screen before the name entry)
    Rig longer;
    longer.tune.b_fill = true;
    longer.tune.over = "PTS 1450 412M";
    longer.playToOver(1450);
    TEST_ASSERT_TRUE(longer.arcade.getMode() == Arcade::Mode::OVER);
    TEST_ASSERT_EQUAL_UINT8(0, longer.arcade.getOverRank());
    canvas.fillScreen(0);
    longer.arcade.draw(canvas);
    //"PTS 1450 412M 1ST": 17 characters, a pill 105 px wide from column 12, the box 2 px wider each side
    int16_t pill = GameDraw::pillWidth("PTS 1450 412M 1ST");
    int16_t left = int16_t(64 - pill / 2);
    TEST_ASSERT_TRUE(canvas.getPixel(left, 23));
    TEST_ASSERT_TRUE(canvas.getPixel(left + pill - 1, 23));
    TEST_ASSERT_FALSE(canvas.getPixel(left - 1, 23));
    TEST_ASSERT_FALSE(canvas.getPixel(left - 2, 23));
    TEST_ASSERT_TRUE(canvas.getPixel(left - 3, 23));
}

void test_flicks_need_the_rest_first_and_one_per_push(){
    StickFlick f;
    float s[4] = {0, 0.8f, 0, 0};
    TEST_ASSERT_TRUE(f.update(s) == Flick::NONE);      //pushed from the start: not a flick
    s[1] = 0.2f;
    TEST_ASSERT_TRUE(f.update(s) == Flick::NONE);
    s[1] = 0.5f;
    TEST_ASSERT_TRUE(f.update(s) == Flick::NONE);      //not far enough yet
    s[1] = 0.65f;
    TEST_ASSERT_TRUE(f.update(s) == Flick::UP);
    TEST_ASSERT_TRUE(f.update(s) == Flick::NONE);      //held: once
    s[1] = 0.4f;
    TEST_ASSERT_TRUE(f.update(s) == Flick::NONE);
    s[1] = 0.7f;
    TEST_ASSERT_TRUE(f.update(s) == Flick::NONE);      //not back under 0.3 in between
    s[1] = 0.1f;
    f.update(s);
    s[0] = -0.9f;
    s[1] = 0.6f;
    TEST_ASSERT_TRUE(f.update(s) == Flick::LEFT);      //the larger axis decides
    s[0] = s[1] = 0;
    f.update(s);
    s[3] = -0.7f;                                       //the right stick too
    TEST_ASSERT_TRUE(f.update(s) == Flick::DOWN);
    s[3] = 0;
    f.update(s);
    s[2] = 0.7f;
    TEST_ASSERT_TRUE(f.update(s) == Flick::RIGHT);
    f.disarm();
    s[2] = 0;
    s[0] = 0.7f;
    TEST_ASSERT_TRUE(f.update(s) == Flick::NONE);      //disarmed: back to rest first
}

void test_sound_modes_filter_the_layers(){
    using namespace ArcadeSoundModes;
    static const SoundNote n[] = {{0, SoundNote::Kind::SYNTH, 0, Synth::Wave::SQUARE, 500, 20, 70, 0}};
    SoundCue c = soundCue(n, SoundPriority::GAME);
    ArcadeSink sink;
    TEST_ASSERT_FALSE(sink.play(c));                    //no output (Sound setting Off): nothing
    RecordingSink rec;
    sink.setOutput(&rec);
    const bool expect[4][2] = {{true, true}, {true, false}, {false, true}, {false, false}};  //front, back
    for(uint8_t m = 0; m < 4; m++){
        sink.setMode(ArcadeSoundMode(m));
        TEST_ASSERT_EQUAL(expect[m][0], sink.play(c, SoundLayer::FRONT));
        TEST_ASSERT_EQUAL(expect[m][1], sink.play(c, SoundLayer::BACK));
        TEST_ASSERT_EQUAL(expect[m][0], sink.playNote(n[0], SoundPriority::GAME, SoundLayer::FRONT));
    }
    TEST_ASSERT_EQUAL_UINT32(4, rec.count(SoundLayer::FRONT));
    TEST_ASSERT_EQUAL_UINT32(2, rec.count(SoundLayer::BACK));
    //stops and the tempo always pass
    sink.setMode(ArcadeSoundMode::MUTE);
    sink.stop(SoundLayer::BACK);
    sink.setTempo(SoundLayer::BACK, 130);
    TEST_ASSERT_EQUAL_UINT32(1, rec.stopCount(SoundLayer::BACK));
    TEST_ASSERT_EQUAL_UINT16(130, rec.getTempo(SoundLayer::BACK));
    TEST_ASSERT_EQUAL_STRING("MUSIC OFF", label(ArcadeSoundMode::MUSIC_OFF));
    TEST_ASSERT_TRUE(next(ArcadeSoundMode::MUTE) == ArcadeSoundMode::SOUND_ON);
    TEST_ASSERT_TRUE(previous(ArcadeSoundMode::SOUND_ON) == ArcadeSoundMode::MUTE);
}

void test_picker_flick_up_changes_the_sound_mode_and_keeps_it(){
    Rig r;
    TEST_ASSERT_TRUE(r.arcade.getSoundMode() == ArcadeSoundMode::SOUND_ON);
    r.sink.clear();
    r.flick(0, 1);
    TEST_ASSERT_TRUE(r.arcade.getSoundMode() == ArcadeSoundMode::MUSIC_OFF);
    TEST_ASSERT_EQUAL_UINT32(1, r.sink.count(SoundLayer::FRONT));      //a blip: effects still on
    TEST_ASSERT_EQUAL_HEX8(ArcadeScores::marker, r.store.bytes[0]);     //the first change writes the block
    TEST_ASSERT_EQUAL_UINT8(1, r.store.bytes[2]);
    r.flick(0, 1);
    TEST_ASSERT_TRUE(r.arcade.getSoundMode() == ArcadeSoundMode::FX_OFF);
    r.flick(0, 1, 1);                                                   //the right stick flicks too
    TEST_ASSERT_TRUE(r.arcade.getSoundMode() == ArcadeSoundMode::MUTE);
    TEST_ASSERT_EQUAL_UINT32(1, r.sink.count(SoundLayer::FRONT));      //no blip in FX OFF or MUTE
    TEST_ASSERT_TRUE(r.sink.stopCount(SoundLayer::BACK) > 0);
    //kept: another arcade on the same store comes up muted
    Arcade other;
    other.setStore(&r.store);
    TEST_ASSERT_TRUE(other.getSoundMode() == ArcadeSoundMode::MUTE);
    r.flick(0, 1);
    TEST_ASSERT_TRUE(r.arcade.getSoundMode() == ArcadeSoundMode::SOUND_ON);
    TEST_ASSERT_EQUAL_UINT32(2, r.sink.count(SoundLayer::FRONT));
    TEST_ASSERT_TRUE(r.arcade.getCard() == Arcade::Card::NONE);
}

void test_cards_open_and_close(){
    Rig r;
    uint8_t selected = r.arcade.getSelected();
    r.flick(1, 0);
    TEST_ASSERT_TRUE(r.arcade.getCard() == Arcade::Card::HELP);
    r.flick(-1, 0);                                                     //the opposite flick
    TEST_ASSERT_TRUE(r.arcade.getCard() == Arcade::Card::NONE);
    r.flick(0, -1);
    TEST_ASSERT_TRUE(r.arcade.getCard() == Arcade::Card::BEST);
    r.key(true);                                                        //a pushbutton: back, the same game
    TEST_ASSERT_TRUE(r.arcade.getCard() == Arcade::Card::NONE);
    TEST_ASSERT_EQUAL_UINT8(selected, r.arcade.getSelected());
    r.flick(1, 0);
    r.click();                                                          //a click closes, does not start the game
    TEST_ASSERT_TRUE(r.arcade.getCard() == Arcade::Card::NONE);
    TEST_ASSERT_TRUE(r.arcade.getMode() == Arcade::Mode::PICKER);
    r.flick(0, -1);
    r.idle(int(Arcade::cardMillis / 20) - 5);
    TEST_ASSERT_TRUE(r.arcade.getCard() == Arcade::Card::BEST);
    r.idle(10);                                                         //8 s: closed on its own
    TEST_ASSERT_TRUE(r.arcade.getCard() == Arcade::Card::NONE);
    GFXcanvas1 canvas(128, 32);
    r.flick(1, 0);
    r.arcade.draw(canvas);
    r.flick(-1, 0);
    r.flick(0, -1);
    r.arcade.draw(canvas);
}

void test_picker_demo_is_silent_and_the_game_sounds_when_played(){
    Rig r;
    r.sink.clear();
    r.idle(50);
    TEST_ASSERT_TRUE(r.tune.ticks > 0);
    TEST_ASSERT_EQUAL_UINT32(0, r.sink.total());                        //the demo plays, nothing heard
    TEST_ASSERT_EQUAL_INT(0, r.tune.musicCalls);
    r.click();
    TEST_ASSERT_TRUE(r.arcade.getMode() == Arcade::Mode::PLAYING);
    TEST_ASSERT_EQUAL_INT(1, r.tune.musicCalls);                        //on start
    TEST_ASSERT_EQUAL_UINT16(330, r.sink.lastHertz(SoundLayer::BACK));
    TEST_ASSERT_EQUAL_UINT16(100, r.sink.getTempo(SoundLayer::BACK));
    r.idle(3);
    TEST_ASSERT_EQUAL_UINT16(1000, r.sink.lastHertz(SoundLayer::FRONT));
    //pause: the music stops; resume: it starts again
    uint32_t stops = r.sink.stopCount(SoundLayer::BACK);
    r.key(true);
    TEST_ASSERT_TRUE(r.arcade.isPaused());
    TEST_ASSERT_TRUE(r.sink.stopCount(SoundLayer::BACK) > stops);
    r.key(true);
    TEST_ASSERT_FALSE(r.arcade.isPaused());
    TEST_ASSERT_EQUAL_INT(2, r.tune.musicCalls);
    //back to the picker: the music stops, the demo is silent again
    r.key(false);
    TEST_ASSERT_TRUE(r.arcade.getMode() == Arcade::Mode::PICKER);
    r.sink.clear();
    r.idle(20);
    TEST_ASSERT_EQUAL_UINT32(0, r.sink.total());
}

void test_pause_flicks_change_the_mode_and_music_comes_back(){
    Rig r;
    r.flick(0, 1);                                                      //MUSIC OFF
    r.click();
    TEST_ASSERT_EQUAL_INT(1, r.tune.musicCalls);
    r.sink.clear();
    r.idle(3);
    TEST_ASSERT_EQUAL_UINT32(0, r.sink.count(SoundLayer::BACK));       //called, but dropped
    TEST_ASSERT_TRUE(r.sink.count(SoundLayer::FRONT) > 0);             //effects still on
    r.key(true);                                                        //pause
    //the sticks were the game's: back to rest first, then flicks change the mode (up next, down back)
    r.flick(0, 1);
    TEST_ASSERT_TRUE(r.arcade.getSoundMode() == ArcadeSoundMode::FX_OFF);
    r.flick(0, -1);
    r.flick(0, -1);
    TEST_ASSERT_TRUE(r.arcade.getSoundMode() == ArcadeSoundMode::SOUND_ON);
    TEST_ASSERT_TRUE(r.arcade.isPaused());
    GFXcanvas1 canvas(128, 32);
    r.arcade.draw(canvas);
    r.sink.clear();
    r.key(true);                                                        //resume: the music from its start
    TEST_ASSERT_EQUAL_UINT16(330, r.sink.lastHertz(SoundLayer::BACK));
    //MUTE: nothing at all
    r.key(true);
    r.flick(0, -1);
    TEST_ASSERT_TRUE(r.arcade.getSoundMode() == ArcadeSoundMode::MUTE);
    r.key(true);
    r.sink.clear();
    r.idle(5);
    TEST_ASSERT_EQUAL_UINT32(0, r.sink.total());
}

void test_game_over_phrase_waits_behind_and_fanfare_for_top_3(){
    Rig r;
    r.playToOver(0);                                                    //0 never enters
    TEST_ASSERT_TRUE(r.arcade.getMode() == Arcade::Mode::OVER);
    TEST_ASSERT_EQUAL_UINT8(ArcadeScores::noRank, r.arcade.getOverRank());
    const RecordingSink::Play* p = r.sink.last(SoundLayer::BACK);
    TEST_ASSERT_NOT_NULL(p);
    TEST_ASSERT_FALSE(p->b_note);
    TEST_ASSERT_EQUAL_UINT16(ArcadeSounds::gameOverDelayMillis, p->note.atMillis);    //first note at 400 ms
    TEST_ASSERT_TRUE(p->cue.notes == ArcadeSounds::gameOver().notes);
    TEST_ASSERT_TRUE(r.sink.stopCount(SoundLayer::BACK) > 0);
    r.idle(40);
    TEST_ASSERT_TRUE(r.arcade.getMode() == Arcade::Mode::OVER);         //no name entry
    r.click();                                                          //plays again
    TEST_ASSERT_TRUE(r.arcade.getMode() == Arcade::Mode::PLAYING);
    r.tune.points = 500;
    r.tune.endAt = 5;
    r.idle(10);
    TEST_ASSERT_EQUAL_UINT8(0, r.arcade.getOverRank());
    p = r.sink.last(SoundLayer::BACK);
    TEST_ASSERT_TRUE(p->cue.notes == ArcadeSounds::fanfare().notes);
    TEST_ASSERT_EQUAL_UINT16(ArcadeSounds::gameOverDelayMillis, p->note.atMillis);
}

void test_top_3_ranks_higher_or_lower_strictly_better(){
    ArcadeScores s;
    s.load(nullptr);
    TEST_ASSERT_EQUAL_UINT8(ArcadeScores::noRank, s.rankFor(0, 0, false));
    TEST_ASSERT_EQUAL_UINT8(0, s.insert(0, 100, "AAAA", false));
    TEST_ASSERT_EQUAL_UINT8(0, s.insert(0, 200, "BBBB", false));
    TEST_ASSERT_EQUAL_UINT8(2, s.insert(0, 50, "CCCC", false));
    TEST_ASSERT_EQUAL_UINT8(2, s.insert(0, 100, "DDDD", false));        //a tie goes below
    TEST_ASSERT_EQUAL_UINT32(200, s.entry(0, 0).score);
    TEST_ASSERT_EQUAL_STRING("AAAA", s.entry(0, 1).name);
    TEST_ASSERT_EQUAL_STRING("DDDD", s.entry(0, 2).name);               //50 fell out
    TEST_ASSERT_EQUAL_UINT8(ArcadeScores::noRank, s.insert(0, 100, "EEEE", false));     //equal to the last: out
    TEST_ASSERT_EQUAL_UINT8(1, s.rankFor(0, 150, false));
    //lower is better (Parking's time); 0 enters there
    TEST_ASSERT_EQUAL_UINT8(0, s.insert(1, 300, "LEO ", true));
    TEST_ASSERT_EQUAL_UINT8(0, s.insert(1, 250, "ANNA", true));
    TEST_ASSERT_EQUAL_UINT8(2, s.insert(1, 300, "BOB ", true));         //a tie goes below
    TEST_ASSERT_EQUAL_UINT8(0, s.rankFor(1, 0, true));
    TEST_ASSERT_EQUAL_UINT8(ArcadeScores::noRank, s.rankFor(1, 301, true));
    TEST_ASSERT_EQUAL_STRING("ANNA", s.entry(1, 0).name);
    TEST_ASSERT_EQUAL_STRING("LEO ", s.entry(1, 1).name);
    TEST_ASSERT_EQUAL_STRING("BOB ", s.getLastName());
    //clear one slot, the others stay
    s.clear(0);
    TEST_ASSERT_FALSE(s.hasEntries(0));
    TEST_ASSERT_TRUE(s.hasEntries(1));
    //fixed slots by name, an unknown game in the spare one
    TEST_ASSERT_EQUAL_UINT8(0, ArcadeScores::slotFor("PONG"));
    TEST_ASSERT_EQUAL_UINT8(5, ArcadeScores::slotFor("BLOCKS"));
    TEST_ASSERT_EQUAL_UINT8(6, ArcadeScores::slotFor("PLUME"));
    TEST_ASSERT_EQUAL_UINT8(7, ArcadeScores::slotFor("TUNE"));
    Arcade arcade;
    for(uint8_t i = 0; i < Arcade::gameCount; i++) TEST_ASSERT_TRUE(arcade.slotOf(i) < 7);
}

void test_eeprom_layout_bytes(){
    RamArcadeStore store;
    ArcadeScores s;
    s.load(&store);
    TEST_ASSERT_EQUAL_UINT32(0, store.writes);                          //reading writes nothing
    TEST_ASSERT_EQUAL_UINT16(200, ArcadeScores::blockSize);
    s.insert(4, 0x01020304u, "LEO ", false);
    const uint8_t* b = store.bytes;
    TEST_ASSERT_EQUAL_HEX8(0xA7, b[0]);
    TEST_ASSERT_EQUAL_HEX8(1, b[1]);
    TEST_ASSERT_EQUAL_HEX8(0, b[2]);                                    //SOUND ON
    TEST_ASSERT_EQUAL_MEMORY("LEO ", &b[3], 4);                         //the last name
    const uint8_t* e = &b[8 + 4 * 24];                                  //Snake's slot, rank 1
    const uint8_t expected[8] = {0x04, 0x03, 0x02, 0x01, 'L', 'E', 'O', ' '};
    TEST_ASSERT_EQUAL_HEX8_ARRAY(expected, e, 8);
    for(int i = 8; i < 24; i++) TEST_ASSERT_EQUAL_HEX8(0xFF, e[i]);    //ranks 2 and 3 empty
    for(int i = 8; i < 8 + 4 * 24; i++) TEST_ASSERT_EQUAL_HEX8(0xFF, b[i]);
    for(int i = 200; i < 256; i++) TEST_ASSERT_EQUAL_HEX8(0xFF, b[i]); //nothing past the block
    //read back
    ArcadeScores again;
    again.load(&store);
    TEST_ASSERT_EQUAL_UINT32(0x01020304u, again.entry(4, 0).score);
    TEST_ASSERT_EQUAL_STRING("LEO ", again.entry(4, 0).name);
    TEST_ASSERT_EQUAL_STRING("LEO ", again.getLastName());
    //only the bytes that change: a second entry below changes 8 bytes of the slot
    store.changes = 0;
    again.insert(4, 10, "ANNA", false);
    TEST_ASSERT_EQUAL_UINT32(8 + 4, store.changes);                     //the entry and the last name
}

void test_blank_and_foreign_eeprom_read_as_empty(){
    RamArcadeStore store;
    ArcadeScores s;
    s.load(&store);
    TEST_ASSERT_EQUAL_UINT8(0, s.getSoundMode());
    TEST_ASSERT_EQUAL_STRING("AAAA", s.getLastName());
    for(uint8_t slot = 0; slot < ArcadeScores::slotCount; slot++) TEST_ASSERT_FALSE(s.hasEntries(slot));
    //something else in those bytes (a wrong marker, valid looking entries): all empty, the defaults
    const uint8_t entry[8] = {100, 0, 0, 0, 'Z', 'O', 'E', ' '};
    memcpy(&store.bytes[8], entry, 8);
    store.bytes[2] = 2;
    store.bytes[0] = 0x00;
    store.bytes[1] = 1;
    s.load(&store);
    TEST_ASSERT_FALSE(s.hasEntries(0));
    TEST_ASSERT_EQUAL_UINT8(0, s.getSoundMode());
    //the right marker, a wrong version: the same
    store.bytes[0] = ArcadeScores::marker;
    store.bytes[1] = 2;
    s.load(&store);
    TEST_ASSERT_FALSE(s.hasEntries(0));
    //the first change writes the whole block: the old entry is gone from the store too
    s.setSoundMode(3);
    TEST_ASSERT_EQUAL_HEX8(1, store.bytes[1]);
    TEST_ASSERT_EQUAL_HEX8(3, store.bytes[2]);
    TEST_ASSERT_EQUAL_HEX8(0xFF, store.bytes[12]);
    s.load(&store);
    TEST_ASSERT_EQUAL_UINT8(3, s.getSoundMode());
    TEST_ASSERT_FALSE(s.hasEntries(0));
    //right marker and version: an entry with a bad name character is empty, the next moves up
    memcpy(&store.bytes[8], entry, 8);
    store.bytes[12] = 'z';
    memcpy(&store.bytes[16], entry, 8);
    s.load(&store);
    TEST_ASSERT_TRUE(s.hasEntries(0));
    TEST_ASSERT_EQUAL_STRING("ZOE ", s.entry(0, 0).name);
    TEST_ASSERT_FALSE(s.entry(0, 1).b_used);
    //a sound mode out of range reads as SOUND ON
    store.bytes[2] = 9;
    s.load(&store);
    TEST_ASSERT_EQUAL_UINT8(0, s.getSoundMode());
}

void test_name_entry_letters_cursor_and_save(){
    Rig r;
    r.playToOver(1234);
    TEST_ASSERT_TRUE(r.arcade.getMode() == Arcade::Mode::OVER);
    TEST_ASSERT_EQUAL_UINT8(0, r.arcade.getOverRank());
    GFXcanvas1 canvas(128, 32);
    r.arcade.draw(canvas);
    r.idle(int(Arcade::nameDelayMillis / 20) + 1);                      //the name entry follows on its own
    TEST_ASSERT_TRUE(r.arcade.getMode() == Arcade::Mode::NAME);
    TEST_ASSERT_EQUAL_STRING("AAAA", r.arcade.getName());               //the last name, AAAA at first
    r.flick(0, 1);
    TEST_ASSERT_EQUAL_STRING("BAAA", r.arcade.getName());
    //held up: the next after 400 ms, then every 150 ms
    GameInput up;
    up.stick[1] = 1;
    r.step(up);                                                         //flick: C
    for(int i = 0; i < 19; i++) r.step(up);                             //380 ms more
    TEST_ASSERT_EQUAL_STRING("CAAA", r.arcade.getName());
    r.step(up);                                                         //400 ms: D
    TEST_ASSERT_EQUAL_STRING("DAAA", r.arcade.getName());
    for(int i = 0; i < 7; i++) r.step(up);                              //+140 ms
    TEST_ASSERT_EQUAL_STRING("DAAA", r.arcade.getName());
    r.step(up);                                                         //+160 ms: E
    TEST_ASSERT_EQUAL_STRING("EAAA", r.arcade.getName());
    r.step();
    //the cursor: right flicks, wraps left
    r.flick(1, 0);
    TEST_ASSERT_EQUAL_UINT8(1, r.arcade.getNameCursor());
    r.flick(0, -1);                                                     //down from A: space (the end of the list)
    TEST_ASSERT_EQUAL_STRING("E AA", r.arcade.getName());
    r.flick(0, -1);
    TEST_ASSERT_EQUAL_STRING("E9AA", r.arcade.getName());
    r.flick(-1, 0);
    r.flick(-1, 0);
    TEST_ASSERT_EQUAL_UINT8(3, r.arcade.getNameCursor());
    r.arcade.draw(canvas);
    r.click();                                                          //saved as edited
    TEST_ASSERT_TRUE(r.arcade.getMode() == Arcade::Mode::OVER);
    const ArcadeScores& s = r.arcade.getScores();
    uint8_t slot = r.arcade.slotOf(0);
    TEST_ASSERT_EQUAL_STRING("E9AA", s.entry(slot, 0).name);
    TEST_ASSERT_EQUAL_UINT32(1234, s.entry(slot, 0).score);
    TEST_ASSERT_EQUAL_STRING("E9AA", s.getLastName());
    TEST_ASSERT_EQUAL_HEX8('E', r.store.bytes[3]);
    TEST_ASSERT_EQUAL_UINT8(0, r.arcade.getOverRank());
    r.arcade.draw(canvas);
    //the click that saved does not replay at once; a later one does
    r.click();
    TEST_ASSERT_TRUE(r.arcade.getMode() == Arcade::Mode::OVER);
    r.idle(30);
    //a second game into 2nd place: a pushbutton saves it under the last name, as it was
    r.click();
    TEST_ASSERT_TRUE(r.arcade.getMode() == Arcade::Mode::PLAYING);
    r.tune.points = 999;
    r.tune.endAt = 3;
    r.idle(6);
    TEST_ASSERT_EQUAL_UINT8(1, r.arcade.getOverRank());
    r.idle(int(Arcade::nameDelayMillis / 20) + 1);
    TEST_ASSERT_TRUE(r.arcade.getMode() == Arcade::Mode::NAME);
    TEST_ASSERT_EQUAL_STRING("E9AA", r.arcade.getName());
    r.flick(0, 1);                                                      //edited, then a pushbutton: not this
    r.key(true);
    TEST_ASSERT_TRUE(r.arcade.getMode() == Arcade::Mode::OVER);
    TEST_ASSERT_EQUAL_STRING("E9AA", s.entry(slot, 1).name);
    TEST_ASSERT_EQUAL_UINT32(999, s.entry(slot, 1).score);
}

void test_name_entry_saved_when_the_arcade_closes(){
    Rig r;
    r.playToOver(77);
    r.idle(int(Arcade::nameDelayMillis / 20) + 1);
    r.flick(0, 1);
    TEST_ASSERT_TRUE(r.arcade.getMode() == Arcade::Mode::NAME);
    r.sink.clear();
    r.arcade.leave();                                                   //idle exit: as it stands
    TEST_ASSERT_FALSE(r.arcade.isActive());
    //written after the loop's frame, not inside leave() (an EEPROM write never delays an E-stop frame)
    TEST_ASSERT_EQUAL_STRING("", r.arcade.getScores().entry(r.arcade.slotOf(0), 0).name);
    r.arcade.saveDeferred();
    TEST_ASSERT_EQUAL_STRING("BAAA", r.arcade.getScores().entry(r.arcade.slotOf(0), 0).name);
    //the closing jingle (reversed opening) in front, the music stopped
    TEST_ASSERT_TRUE(r.sink.last(SoundLayer::FRONT)->cue.notes == ArcadeSounds::closing().notes);
    TEST_ASSERT_TRUE(r.sink.stopCount(SoundLayer::BACK) > 0);
    //closed during the game over screen, before the name entry: saved under the last name
    Rig q;
    q.playToOver(55);
    q.arcade.leave();
    q.arcade.saveDeferred();
    TEST_ASSERT_EQUAL_STRING("AAAA", q.arcade.getScores().entry(q.arcade.slotOf(0), 0).name);
    TEST_ASSERT_EQUAL_UINT32(55, q.arcade.getScores().entry(q.arcade.slotOf(0), 0).score);
}

void test_both_pushbuttons_held_clear_the_top_3(){
    Rig r;
    r.playToOver(300);
    r.idle(int(Arcade::nameDelayMillis / 20) + 1);
    r.click();
    r.key(false);                                                       //back to the picker
    TEST_ASSERT_TRUE(r.arcade.getMode() == Arcade::Mode::PICKER);
    uint8_t slot = r.arcade.slotOf(0);
    TEST_ASSERT_TRUE(r.arcade.getScores().hasEntries(slot));
    r.idle(2);
    //held 3 s: the question; not before
    r.step(GameInput(), true, true, true, true);
    for(int i = 0; i < 148; i++) r.step(GameInput(), false, false, true, true);
    TEST_ASSERT_TRUE(r.arcade.getCard() == Arcade::Card::NONE);
    r.step(GameInput(), false, false, true, true);
    r.step(GameInput(), false, false, true, true);
    TEST_ASSERT_TRUE(r.arcade.getCard() == Arcade::Card::CLEAR);
    GFXcanvas1 canvas(128, 32);
    r.arcade.draw(canvas);
    //held on: asked once; anything but a click cancels, and 3 s do too
    for(int i = 0; i < 20; i++) r.step(GameInput(), false, false, true, true);
    r.idle(int(Arcade::clearAnswerMillis / 20) + 1);
    TEST_ASSERT_TRUE(r.arcade.getCard() == Arcade::Card::NONE);
    TEST_ASSERT_TRUE(r.arcade.getScores().hasEntries(slot));
    //again, and a click: cleared, in EEPROM too
    r.step(GameInput(), true, true, true, true);
    for(int i = 0; i < 152; i++) r.step(GameInput(), false, false, true, true);
    TEST_ASSERT_TRUE(r.arcade.getCard() == Arcade::Card::CLEAR);
    r.idle(2);
    r.sink.clear();
    r.click();
    TEST_ASSERT_TRUE(r.arcade.getCard() == Arcade::Card::NONE);
    TEST_ASSERT_TRUE(r.arcade.getMode() == Arcade::Mode::PICKER);      //the click did not start the game
    TEST_ASSERT_FALSE(r.arcade.getScores().hasEntries(slot));
    TEST_ASSERT_EQUAL_HEX8(0xFF, r.store.bytes[8 + slot * 24 + 4]);
    TEST_ASSERT_TRUE(r.sink.last(SoundLayer::FRONT)->cue.notes == ArcadeSounds::cleared().notes);
}

void test_every_game_has_help_lines_that_fit(){
    Arcade arcade;
    for(uint8_t i = 0; i < Arcade::gameCount; i++){
        const char* lines[3] = {"x", "x", "x"};
        arcade.game(i).helpLines(lines);
        TEST_ASSERT_NOT_NULL_MESSAGE(lines[0], arcade.game(i).name());
        for(const char* line : lines){
            if(!line) continue;
            TEST_ASSERT_TRUE_MESSAGE(strlen(line) <= 21, line);
            for(const char* c = line; *c; c++) TEST_ASSERT_FALSE_MESSAGE(islower(*c), line);
        }
    }
}

void test_pickers_blips_and_sound_off_is_silent(){
    Rig r;
    r.sink.clear();
    r.key(true);
    TEST_ASSERT_EQUAL_UINT32(1, r.sink.count(SoundLayer::FRONT));
    uint16_t first = r.sink.lastHertz(SoundLayer::FRONT);
    r.key(true);
    TEST_ASSERT_TRUE(r.sink.lastHertz(SoundLayer::FRONT) > first);     //a step up the pentatonic
    //both pressed in the same loop: no change of game
    uint8_t selected = r.arcade.getSelected();
    r.step(GameInput(), true, true);
    TEST_ASSERT_EQUAL_UINT8(selected, r.arcade.getSelected());
    //the board gives no sink (Sound setting Off): nothing anywhere
    r.arcade.setSound(nullptr);
    r.sink.clear();
    r.key(false);
    r.key(false);
    r.click();
    r.idle(5);
    r.key(true);
    r.arcade.leave();
    TEST_ASSERT_EQUAL_UINT32(0, r.sink.total());
}

static bool beatIsLast(const RecordingSink& sink){
    const RecordingSink::Play* p = sink.last(SoundLayer::BACK);
    return p && !p->b_note && p->cue.notes == ArcadeSounds::pickerBeat().notes;
}

//the picker's drum beat: behind the picker and its cards, from the opening and every return to it; a game, the game
//over phrase, the name entry and leaving end it; music, so MUSIC OFF and MUTE silence it
void test_picker_beat_behind_the_picker_only(){
    SoundCue beat = ArcadeSounds::pickerBeat();
    TEST_ASSERT_EQUAL_UINT16(ArcadeSounds::pickerBeatSteps * ArcadeSounds::pickerBeatStepMillis, beat.loopMillis);
    TEST_ASSERT_TRUE(beat.lengthMillis() <= beat.loopMillis);
    bool b_drums = false, b_bass = false;
    for(uint16_t i = 0; i < beat.noteCount; i++){
        const SoundNote& n = beat.notes[i];
        TEST_ASSERT_TRUE(n.kind == SoundNote::Kind::SYNTH);         //a square would cut it
        if(n.voice == 2) b_drums = true;
        if(n.voice == 1){
            b_bass = true;
            TEST_ASSERT_TRUE(n.hertz >= 300);                       //where the disc plays it
        }
        if(i) TEST_ASSERT_TRUE(n.atMillis >= beat.notes[i - 1].atMillis);
    }
    TEST_ASSERT_TRUE(b_drums && b_bass);
    //110 to 125 a minute: a quarter is four steps
    uint32_t quarter = 4u * ArcadeSounds::pickerBeatStepMillis;
    TEST_ASSERT_TRUE(quarter >= 480 && quarter <= 545);

    Rig r;
    TEST_ASSERT_TRUE(beatIsLast(r.sink));                           //the arcade opened
    TEST_ASSERT_EQUAL_UINT16(100, r.sink.getTempo(SoundLayer::BACK));
    //the cards keep it
    uint32_t backPlays = r.sink.count(SoundLayer::BACK);
    r.flick(1, 0);
    r.flick(-1, 0);
    r.flick(0, -1);
    r.key(true);
    TEST_ASSERT_EQUAL_UINT32(backPlays, r.sink.count(SoundLayer::BACK));
    //a game starts: the beat stops, the game's music takes the layer
    uint32_t stops = r.sink.stopCount(SoundLayer::BACK);
    TEST_ASSERT_EQUAL_UINT8(0, r.arcade.getSelected());            //still the TuneGame
    r.click();
    TEST_ASSERT_TRUE(r.arcade.getMode() == Arcade::Mode::PLAYING);
    TEST_ASSERT_TRUE(r.sink.stopCount(SoundLayer::BACK) > stops);
    TEST_ASSERT_EQUAL_UINT16(330, r.sink.lastHertz(SoundLayer::BACK));
    //back to the picker: the beat again, at the written tempo
    r.key(false);
    TEST_ASSERT_TRUE(r.arcade.getMode() == Arcade::Mode::PICKER);
    TEST_ASSERT_TRUE(beatIsLast(r.sink));
    TEST_ASSERT_EQUAL_UINT16(100, r.sink.getTempo(SoundLayer::BACK));
    //game over: the phrase, not the beat; the name entry neither; the picker again: the beat
    r.playToOver(500);
    TEST_ASSERT_FALSE(beatIsLast(r.sink));
    r.idle(int(Arcade::nameDelayMillis / 20) + 2);
    TEST_ASSERT_TRUE(r.arcade.getMode() == Arcade::Mode::NAME);
    TEST_ASSERT_FALSE(beatIsLast(r.sink));
    r.click();                                                      //saved: back on the game over screen
    TEST_ASSERT_FALSE(beatIsLast(r.sink));
    r.key(true);
    TEST_ASSERT_TRUE(r.arcade.getMode() == Arcade::Mode::PICKER);
    TEST_ASSERT_TRUE(beatIsLast(r.sink));
    //MUSIC OFF stops it, FX OFF lets it back, MUTE stops it, SOUND ON again
    stops = r.sink.stopCount(SoundLayer::BACK);
    backPlays = r.sink.count(SoundLayer::BACK);
    r.flick(0, 1);
    TEST_ASSERT_TRUE(r.arcade.getSoundMode() == ArcadeSoundMode::MUSIC_OFF);
    TEST_ASSERT_TRUE(r.sink.stopCount(SoundLayer::BACK) > stops);
    TEST_ASSERT_EQUAL_UINT32(backPlays, r.sink.count(SoundLayer::BACK));
    r.flick(0, 1);
    TEST_ASSERT_TRUE(r.arcade.getSoundMode() == ArcadeSoundMode::FX_OFF);
    TEST_ASSERT_EQUAL_UINT32(backPlays + 1, r.sink.count(SoundLayer::BACK));
    TEST_ASSERT_TRUE(beatIsLast(r.sink));
    r.flick(0, 1);
    TEST_ASSERT_TRUE(r.arcade.getSoundMode() == ArcadeSoundMode::MUTE);
    //muted, back from a game: no beat
    r.click();
    backPlays = r.sink.count(SoundLayer::BACK);
    r.key(false);
    TEST_ASSERT_EQUAL_UINT32(backPlays, r.sink.count(SoundLayer::BACK));
    r.flick(0, 1);
    TEST_ASSERT_TRUE(r.arcade.getSoundMode() == ArcadeSoundMode::SOUND_ON);
    TEST_ASSERT_TRUE(beatIsLast(r.sink));
    //leaving stops it
    stops = r.sink.stopCount(SoundLayer::BACK);
    backPlays = r.sink.count(SoundLayer::BACK);
    r.arcade.leave();
    TEST_ASSERT_TRUE(r.sink.stopCount(SoundLayer::BACK) > stops);
    TEST_ASSERT_EQUAL_UINT32(backPlays, r.sink.count(SoundLayer::BACK));
    //an arcade opening muted: no beat
    Arcade muted;
    RecordingSink quiet;
    r.store.bytes[2] = uint8_t(ArcadeSoundMode::MUTE);
    muted.setStore(&r.store);
    TEST_ASSERT_TRUE(muted.getSoundMode() == ArcadeSoundMode::MUTE);
    muted.setSound(&quiet);
    muted.enter(0);
    TEST_ASSERT_EQUAL_UINT32(0, quiet.count(SoundLayer::BACK));
}

//muting made easy to find: an up arrow by the corner icon, SOUND: <mode> in the pill, and on every opening the pill
//says FLICK UP: SOUND for 2 s before the record line
void test_sound_hint_arrow_and_mode_pill(){
    using namespace ArcadeSoundModes;
    TEST_ASSERT_EQUAL_STRING("SOUND: ON", pillLabel(ArcadeSoundMode::SOUND_ON));
    TEST_ASSERT_EQUAL_STRING("SOUND: MUSIC OFF", pillLabel(ArcadeSoundMode::MUSIC_OFF));
    TEST_ASSERT_EQUAL_STRING("SOUND: FX OFF", pillLabel(ArcadeSoundMode::FX_OFF));
    TEST_ASSERT_EQUAL_STRING("SOUND: MUTE", pillLabel(ArcadeSoundMode::MUTE));
    TEST_ASSERT_EQUAL_STRING("FLICK UP: SOUND", Arcade::hintText());
    //every pill the picker shows leaves a dark column between itself and the icon's arrow (x 0 to 12)
    for(uint8_t m = 0; m < uint8_t(ArcadeSoundMode::COUNT); m++){
        TEST_ASSERT_TRUE(64 - GameDraw::pillWidth(pillLabel(ArcadeSoundMode(m))) / 2 >= 15);
    }
    TEST_ASSERT_TRUE(64 - GameDraw::pillWidth(Arcade::hintText()) / 2 >= 15);

    Rig r;
    r.tune.b_fill = true;                                           //the game behind lit everywhere
    TEST_ASSERT_TRUE(r.arcade.showsHint());
    GFXcanvas1 canvas(128, 32);
    r.arcade.draw(canvas);
    //the arrow's tip and shaft right of the icon, a dark column between them and after it
    TEST_ASSERT_TRUE(canvas.getPixel(10, 25));
    TEST_ASSERT_TRUE(canvas.getPixel(10, 31));
    TEST_ASSERT_TRUE(canvas.getPixel(8, 27) && canvas.getPixel(12, 27));
    for(int16_t y = 23; y < 32; y++){
        TEST_ASSERT_FALSE(canvas.getPixel(7, y));
        TEST_ASSERT_FALSE(canvas.getPixel(13, y));
    }
    //the hint pill lit over the bottom rows, wider than CLICK TO PLAY
    int16_t hintLeft = int16_t(64 - GameDraw::pillWidth(Arcade::hintText()) / 2);
    TEST_ASSERT_TRUE(canvas.getPixel(hintLeft + 1, 24));
    //2 s later: the record line (none yet: CLICK TO PLAY)
    r.idle(int(Arcade::hintPillMillis / 20) - 3);
    TEST_ASSERT_TRUE(r.arcade.showsHint());
    r.idle(4);
    TEST_ASSERT_FALSE(r.arcade.showsHint());
    r.tune.b_fill = false;                                          //the game behind dark: only the pills show
    r.arcade.draw(canvas);
    TEST_ASSERT_FALSE(canvas.getPixel(hintLeft + 1, 24));          //CLICK TO PLAY is narrower
    TEST_ASSERT_TRUE(canvas.getPixel(int16_t(64 - GameDraw::pillWidth("CLICK TO PLAY") / 2 + 1), 24));
    //a flick up: the mode in the pill (the hint would be gone at once too)
    r.flick(0, 1);
    TEST_ASSERT_TRUE(r.arcade.showsModePill());
    r.arcade.draw(canvas);
    for(int16_t y = 23; y < 32; y++) TEST_ASSERT_FALSE(canvas.getPixel(13, y));
    TEST_ASSERT_TRUE(canvas.getPixel(15 + 1, 24));                 //SOUND: MUSIC OFF, the widest, from x 15
    //back from a game: no hint (only on opening); opening again: the hint again
    r.click();
    r.key(false);
    TEST_ASSERT_FALSE(r.arcade.showsHint());
    r.arcade.leave();
    r.arcade.enter(r.now);
    TEST_ASSERT_TRUE(r.arcade.showsHint());
    //paused, the first line says the flick: ^v and the mode
    r.idle(2);
    r.step(GameInput());
    r.click();
    TEST_ASSERT_TRUE(r.arcade.getMode() == Arcade::Mode::PLAYING);
    TEST_ASSERT_FALSE(r.arcade.showsHint());
    r.key(true);
    TEST_ASSERT_TRUE(r.arcade.isPaused());
    r.arcade.draw(canvas);
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
    RUN_TEST(test_flicks_need_the_rest_first_and_one_per_push);
    RUN_TEST(test_sound_modes_filter_the_layers);
    RUN_TEST(test_picker_flick_up_changes_the_sound_mode_and_keeps_it);
    RUN_TEST(test_cards_open_and_close);
    RUN_TEST(test_picker_demo_is_silent_and_the_game_sounds_when_played);
    RUN_TEST(test_pause_flicks_change_the_mode_and_music_comes_back);
    RUN_TEST(test_game_over_phrase_waits_behind_and_fanfare_for_top_3);
    RUN_TEST(test_game_over_shows_the_games_over_text);
    RUN_TEST(test_top_3_ranks_higher_or_lower_strictly_better);
    RUN_TEST(test_eeprom_layout_bytes);
    RUN_TEST(test_blank_and_foreign_eeprom_read_as_empty);
    RUN_TEST(test_name_entry_letters_cursor_and_save);
    RUN_TEST(test_name_entry_saved_when_the_arcade_closes);
    RUN_TEST(test_both_pushbuttons_held_clear_the_top_3);
    RUN_TEST(test_every_game_has_help_lines_that_fit);
    RUN_TEST(test_pickers_blips_and_sound_off_is_silent);
    RUN_TEST(test_picker_beat_behind_the_picker_only);
    RUN_TEST(test_sound_hint_arrow_and_mode_pill);
    return UNITY_END();
}
