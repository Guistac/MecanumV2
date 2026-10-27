#pragma once
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <Adafruit_GFX.h>

//The remote's games (easter egg) [D-148]: each game is one class with its rules, its drawing on the 128 x 32 screen
//and a built-in player for the attract mode. The Arcade runs them; the firmware and the Mac (tests, renderer) use the
//same code. A game never sees the pushbuttons, the speed switch or the E-stop, and never sends anything.

//The two sticks, read once per tick
struct GameInput{
    float stick[4] = {0, 0, 0, 0};      //left X, left Y, right X, right Y: -1..1, x positive right, y positive up
    bool held[2] = {false, false};      //stick buttons held: left, right
    bool clicked[2] = {false, false};   //stick buttons pressed since the last tick
};

//Small deterministic random numbers (xorshift32): a game is the same for the same seed and inputs
class GameRandom{
public:
    void seed(uint32_t value){ state = value ? value : 0x9E3779B9u; }
    uint32_t next(){
        state ^= state << 13;
        state ^= state >> 17;
        state ^= state << 5;
        return state;
    }
    //0..range-1
    uint32_t below(uint32_t range){ return range ? next() % range : 0; }
    //lowest..highest, both included
    int32_t between(int32_t lowest, int32_t highest){ return lowest + int32_t(below(uint32_t(highest - lowest + 1))); }
    //0..1
    float unit(){ return float(next() >> 8) / float(1u << 24); }
private:
    uint32_t state = 0x9E3779B9u;
};

class Game{
public:
    static constexpr float tickSeconds = 0.02f;     //the Arcade calls update() at a fixed 50 Hz
    static const int16_t width = 128;
    static const int16_t height = 32;

    virtual ~Game() = default;

    //the picker's title: upper case, at most 10 characters
    virtual const char* name() const = 0;

    //a new game; the same seed and inputs always play the same
    virtual void reset(uint32_t seed) = 0;

    //one tick (tickSeconds)
    virtual void update(const GameInput& input) = 0;

    //the whole screen: the Arcade cleared it to 0 (dark) before; lit is 1. Rules (owner): the 5 x 7 pixel font for
    //text, nothing touches (1 px gap), negative text only in a filled pill with 1 px of fill around it
    virtual void draw(Adafruit_GFX& out) const = 0;

    virtual bool isOver() const = 0;

    //the result of the game, compared for the best of the session (kept in RAM only)
    virtual uint32_t score() const = 0;
    virtual bool lowerScoreIsBetter() const { return false; }
    //at most 9 characters
    virtual void scoreText(char* out, size_t size, uint32_t value) const;

    //the attract mode: fills the input a simple built-in player gives now (the picker shows the game playing itself,
    //the renderer records it)
    virtual void demo(GameInput& input) = 0;
};

//Drawing helpers shared by the games and the Arcade, the same rules as the remote's screens
namespace GameDraw{

    static const uint16_t ON = 1;
    static const uint16_t OFF = 0;
    static const int16_t pillHeight = 9;

    //the 5 x 7 pixel font, top left at (x, y)
    void text(Adafruit_GFX& out, int16_t x, int16_t y, const char* s, uint16_t color = ON);
    int16_t textWidth(const char* s);
    void textCentered(Adafruit_GFX& out, int16_t centreX, int16_t y, const char* s, uint16_t color = ON);
    void textRight(Adafruit_GFX& out, int16_t rightX, int16_t y, const char* s, uint16_t color = ON);

    //a filled pill 9 px high with the text negative inside, 2 px at the sides; b_clear: a dark 1 px margin around it
    //first, so it never touches the game behind it. Returns its width
    int16_t pill(Adafruit_GFX& out, int16_t x, int16_t y, const char* label, bool b_clear = true);
    int16_t pillWidth(const char* label);
    void pillCentered(Adafruit_GFX& out, int16_t centreX, int16_t y, const char* label, bool b_clear = true);

    //the bold word font of the remote (FreeSansBold9pt7b), baseline at y: only for the Arcade's big words
    void bigTextCentered(Adafruit_GFX& out, int16_t centreX, int16_t baselineY, const char* s, uint16_t color = ON);
};
