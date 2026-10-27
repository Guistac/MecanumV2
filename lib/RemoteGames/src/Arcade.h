#pragma once
#include "Game.h"
#include "PongGame.h"
#include "ParkingGame.h"
#include "CaveGame.h"
#include "RocksGame.h"
#include "SnakeGame.h"
#include "BlocksGame.h"

//The remote's games [D-148]. In: both stick buttons held 2 s on the About page with the speed switch at OFF (the
//firmware checks the rest: sticks zeroed, own E-stop clear). Then a picker: the pushbuttons choose a game, which
//plays itself behind its name; a stick click starts it. Playing: the left pushbutton goes back to the picker, the
//right one pauses. Game over: a click plays again, a pushbutton goes back to the picker. Out: both stick buttons held
//2 s again, 2 minutes without input, or the firmware leaves (speed switch off OFF, own E-stop, a new alarm).
class Arcade{
public:

    enum class Mode : uint8_t{ PICKER, PLAYING, OVER };

    static constexpr uint32_t gestureMillis = 2000;     //both stick buttons held, to come in and to go out
    static constexpr uint32_t idleExitMillis = 120000;
    static constexpr uint32_t overLockMillis = 600;     //a click right at game over does not replay at once
    static constexpr uint8_t maxTicksPerUpdate = 5;     //after a long loop the game catches up this far, no more

    static const uint8_t gameCount = 6;

    Arcade();

    //every loop while the arcade is closed: b_allowed when the way in is open (About page, speed at OFF...);
    //true once when the gesture is complete (then call enter)
    bool updateGate(bool b_allowed, bool b_bothSticksHeld, uint32_t now);
    //how far the gesture is, 0..1 (the About page can show it)
    float gateProgress(uint32_t now) const;

    void enter(uint32_t now);
    void leave();
    bool isActive() const { return b_active; }
    //the arcade wants to close (gesture or idle): the firmware calls leave()
    bool exitRequested() const { return b_exitRequested; }

    //every loop while active: the sticks (input.clicked is ignored, the Arcade finds the clicks itself) and the
    //pushbuttons pressed since the last call
    void update(const GameInput& sticks, bool b_previousKey, bool b_nextKey, uint32_t now);

    void draw(Adafruit_GFX& out) const;

    Mode getMode() const { return mode; }
    uint8_t getSelected() const { return selected; }
    bool isPaused() const { return b_paused; }
    Game& game(uint8_t index);
    const Game& game(uint8_t index) const;

private:
    void startDemo();
    void startGame(const bool b_held[2], uint32_t now);
    void tick(const GameInput& input);
    void recordScore();

    PongGame pong;
    ParkingGame parking;
    CaveGame cave;
    RocksGame rocks;
    SnakeGame snake;
    BlocksGame blocks;
    Game* games[gameCount];

    bool b_active = false;
    bool b_exitRequested = false;
    Mode mode = Mode::PICKER;
    uint8_t selected = 0;
    bool b_paused = false;
    uint32_t seedCounter = 1;

    //the best of the session per game (RAM only: the EEPROM is written only on SAVE, D-147)
    bool b_hasBest[gameCount];
    uint32_t best[gameCount];
    bool b_newBest = false;

    uint32_t lastTickMillis = 0;
    uint32_t overMillis = 0;
    uint32_t lastInputMillis = 0;
    bool b_previousHeld[2] = {false, false};
    bool b_clicksBlocked = true;            //after coming in, until both stick buttons are released
    bool b_pendingClick[2] = {false, false};
    //a button still held from the click that started the game: the game sees it released until it is let go
    //(else a game reading held buttons, Cave's thrust, starts with that click)
    bool b_heldFromStart[2] = {false, false};

    //the gesture, in and out: both held since; it re-arms only once both are released
    bool b_gestureHeld = false;
    bool b_gestureArmed = false;
    uint32_t gestureStartMillis = 0;
    bool updateGesture(bool b_bothHeld, bool b_anyHeld, uint32_t now);
};
