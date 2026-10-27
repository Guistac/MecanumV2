#pragma once
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <Adafruit_GFX.h>
#include <Sound.h>

//The remote's games (easter egg) [D-157]: each game is one class with its rules, its drawing on the 128 x 32 screen
//and a built-in player for the attract mode. The Arcade runs them; the firmware and the Mac (tests, renderer) use the
//same code. A game never sees the pushbuttons, the speed switch or the E-stop, and never sends anything. It makes
//sound only through sound() (lib/RemoteAudio/src/SoundKit.h says how) [D-157].

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

    //the result of the game, ranked in the Arcade's top 3 (kept in EEPROM): higher is better unless
    //lowerScoreIsBetter(); 0 never enters (unless lower is better)
    virtual uint32_t score() const = 0;
    virtual bool lowerScoreIsBetter() const { return false; }
    //at most 9 characters
    virtual void scoreText(char* out, size_t size, uint32_t value) const;
    //the score on the game over screen, at most 15 characters: scoreText unless the game says more (Plume: the points
    //and the altitude, PTS 1450 412M)
    virtual void overText(char* out, size_t size, uint32_t value) const { scoreText(out, size, value); }

    //the attract mode: fills the input a simple built-in player gives now (the picker shows the game playing itself,
    //the renderer records it)
    virtual void demo(GameInput& input) = 0;

    //how to play, for the help card and the pause screen: up to 3 lines of at most 21 characters (5 x 7 font), upper
    //case, abbreviated (L STICK, CLICK, UP/DN); unused lines nullptr
    virtual void helpLines(const char* lines[3]) const { lines[0] = lines[1] = lines[2] = nullptr; }

    //sound [D-158]: the Arcade starts the game's music here when it starts, on resume and when music is switched back
    //on (back layer, looping, the tempo for the current level); the game changes the tempo itself as its level rises
    virtual void playMusic(){}

    //where the game's sounds go: the Arcade's filtering sink while played, nothing (silent) in the picker's demo, a
    //RecordingSink in tests. nullptr: silent
    void setSound(SoundSink* sink){ soundSink = sink; }

protected:
    //the game's sounds go here (never nullptr: a silent sink when none is set)
    SoundSink& sound() const;

private:
    SoundSink* soundSink = nullptr;
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

    //the 5 x 7 font a quarter turn, for the games played with the remote held sideways (turned a quarter turn counter-
    //clockwise, game up = screen right: Blocks, Plume) [D-157]: the text runs from the screen's top edge towards its
    //bottom edge, the letters' tops towards the screen's right edge. topX: the screen column of the letters' tops (the
    //text covers topX - 6 to topX), y: the screen row it starts at; its length down the screen is textWidth(s)
    void textSideways(Adafruit_GFX& out, int16_t topX, int16_t y, const char* s, uint16_t color = ON);
    void textSidewaysCentered(Adafruit_GFX& out, int16_t topX, int16_t centreY, const char* s, uint16_t color = ON);
    //the same drawn inverted over what is there (XOR), so it reads over rock: lit(context, x, y) says whether the
    //screen pixel is lit before the text (a game knows its own frame; the display cannot be read back)
    typedef bool (*LitQuery)(const void* context, int16_t x, int16_t y);
    void textSidewaysXor(Adafruit_GFX& out, int16_t topX, int16_t y, const char* s, LitQuery lit, const void* context);

    //the bold word font of the remote (FreeSansBold9pt7b), baseline at y: only for the Arcade's big words
    void bigTextCentered(Adafruit_GFX& out, int16_t centreX, int16_t baselineY, const char* s, uint16_t color = ON);
};
