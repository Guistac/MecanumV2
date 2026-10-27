#pragma once
#include "Game.h"
#include "PongGame.h"
#include "ParkingGame.h"
#include "CaveGame.h"
#include "RocksGame.h"
#include "SnakeGame.h"
#include "BlocksGame.h"
#include "PlumeGame.h"
#include "ArcadeScores.h"
#include "ArcadeSounds.h"
#include "StickFlick.h"

//The remote's games [D-157]. In: both stick buttons held 2 s on the About page with the speed switch at OFF
//(the firmware checks the rest: sticks zeroed; the own E-stop may be pressed). Then a picker: the pushbuttons choose a
//game, which plays itself behind its name (silent); a stick click starts it. In the picker a stick flick up changes
//the sound mode (the corner icon has an up arrow beside it, and each opening shows FLICK UP: SOUND for 2 s) (SOUND ON, MUSIC OFF, FX OFF, MUTE; kept in EEPROM), down shows the game's top 3, right its help;
//both pushbuttons held 3 s offer to clear its top 3. Playing: the left pushbutton goes back to the picker, the right
//one pauses (the pause screen is the help card; flicks up and down change the sound mode there). Game over: the
//game's rank when it entered its top 3, then the name entry; a click plays again, a pushbutton goes back to the
//picker. Out: both stick buttons held 2 s again, 2 minutes without input, or the firmware leaves (speed switch off
//OFF, own E-stop pressed or released, a new alarm). Sound: the board's sink through the ArcadeSink (the sound mode);
//nothing when the board gives none (Sound setting Off). The picker (its cards too) has a drum beat behind it
//(ArcadeSounds::pickerBeat, music), stopped when a game starts and when the arcade closes.
class Arcade{
public:

    enum class Mode : uint8_t{ PICKER, PLAYING, OVER, NAME };
    //what covers the picker: nothing, the help card, the top 3 card, the clear question (a pill)
    enum class Card : uint8_t{ NONE, HELP, BEST, CLEAR };

    static constexpr uint32_t gestureMillis = 2000;     //both stick buttons held, to come in and to go out
    static constexpr uint32_t idleExitMillis = 120000;
    static constexpr uint32_t overLockMillis = 600;     //a click right at game over does not replay at once
    static constexpr uint8_t maxTicksPerUpdate = 5;     //after a long loop the game catches up this far, no more
    static constexpr uint32_t cardMillis = 8000;        //a card closes on its own
    static constexpr uint32_t modePillMillis = 1500;    //the new sound mode in the bottom pill
    static constexpr uint32_t hintPillMillis = 2000;    //on opening, how to change it (hintText) in the bottom pill
    static const char* hintText(){ return "FLICK UP: SOUND"; }
    static constexpr uint32_t clearHoldMillis = 3000;   //both pushbuttons held: the clear question
    static constexpr uint32_t clearAnswerMillis = 3000; //a stick click within this clears
    static constexpr uint32_t nameDelayMillis = 1500;   //the game over screen before the name entry
    static constexpr uint32_t letterRepeatDelayMillis = 400;
    static constexpr uint32_t letterRepeatMillis = 150;

    static const uint8_t gameCount = 7;

    Arcade();

    //every loop while the arcade is closed: b_allowed when the way in is open (About page, speed at OFF...);
    //true once when the gesture is complete (then call enter)
    bool updateGate(bool b_allowed, bool b_bothSticksHeld, uint32_t now);
    //how far the gesture is, 0..1 (the About page can show it)
    float gateProgress(uint32_t now) const;

    void enter(uint32_t now);
    void leave();
    //a name entry cut short by leave() is written here: the firmware calls it every loop after sending its frame
    void saveDeferred();
    bool isActive() const { return b_active; }
    //the arcade wants to close (gesture or idle): the firmware calls leave()
    bool exitRequested() const { return b_exitRequested; }

    //every loop while active: the sticks (input.clicked is ignored, the Arcade finds the clicks itself), the
    //pushbuttons pressed since the last call and whether they are held now
    void update(const GameInput& sticks, bool b_previousKey, bool b_nextKey, uint32_t now,
                bool b_previousHeld = false, bool b_nextHeld = false);

    //where the arcade and the played game sound (the board's player), nullptr for silence (Sound setting Off)
    void setSound(SoundSink* sink);
    //where the top 3s and the sound mode are kept (read now); nullptr: RAM only
    void setStore(ArcadeStore* store);

    void draw(Adafruit_GFX& out) const;

    Mode getMode() const { return mode; }
    Card getCard() const { return card; }
    uint8_t getSelected() const { return selected; }
    bool isPaused() const { return b_paused; }
    //the bottom pill of the picker shows the opening hint (hintText) or the new sound mode
    bool showsHint() const { return b_hintPill; }
    bool showsModePill() const { return b_modePill; }
    ArcadeSoundMode getSoundMode() const { return soundMode; }
    const ArcadeScores& getScores() const { return scores; }
    //the rank the last game took in its top 3 (0 = best), or ArcadeScores::noRank
    uint8_t getOverRank() const { return overRank; }
    //the name entry: the four characters and the one edited
    const char* getName() const { return name; }
    uint8_t getNameCursor() const { return nameCursor; }
    //the top 3 slot of a game (ArcadeScores::slotFor its name)
    uint8_t slotOf(uint8_t index) const;
    Game& game(uint8_t index);
    const Game& game(uint8_t index) const;
    //tests: this game in that place of the picker instead of the built-in one
    void useGame(uint8_t index, Game& g){ games[index % gameCount] = &g; }

    //the characters of a name, in the order up steps through them
    static const char* nameCharacters(){ return "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789 "; }

private:
    bool b_saveDeferred = false;
    bool b_saveDeferredAsEdited = false;
    void startDemo();
    void startGame(const bool b_held[2], uint32_t now);
    void toPicker();
    void tick(const GameInput& input, uint32_t now);
    void gameOver(uint32_t now);
    void openName(uint32_t now);
    void saveName(bool b_asEdited, uint32_t now);
    void stepLetter(int8_t direction);
    void openCard(Card newCard, uint32_t now);
    void closeCard();
    void changeSoundMode(ArcadeSoundMode newMode, uint32_t now);
    void updatePicker(Flick flick, const bool b_click[2], const GameInput& sticks, bool b_previousKey, bool b_nextKey,
                      bool b_bothKeysHeld, uint32_t now);
    void updateName(Flick flick, const bool b_click[2], const GameInput& sticks, bool b_key, uint32_t now);
    //the arcade's own sounds (effects: not in FX OFF or MUTE)
    void cue(const SoundCue& c, SoundLayer layer = SoundLayer::FRONT);
    //the picker's beat on the back layer, at the written tempo (music: not in MUSIC OFF or MUTE)
    void startBeat();

    void drawPicker(Adafruit_GFX& out) const;
    void drawHelp(Adafruit_GFX& out, const char* firstLine, const char* firstLineRight, const char* lastLine) const;
    void drawBest(Adafruit_GFX& out) const;
    void drawName(Adafruit_GFX& out) const;
    //the mode's icon and, right of it, an up arrow (a flick up changes it); x..x+12, y..y+6
    void drawSoundIcon(Adafruit_GFX& out, int16_t x, int16_t y) const;

    PongGame pong;
    ParkingGame parking;
    CaveGame cave;
    RocksGame rocks;
    SnakeGame snake;
    BlocksGame blocks;
    PlumeGame plume;
    Game* games[gameCount];

    bool b_active = false;
    bool b_exitRequested = false;
    Mode mode = Mode::PICKER;
    uint8_t selected = 0;
    bool b_paused = false;
    uint32_t seedCounter = 1;

    //sound: the board's sink (nullptr: silent), the mode, the filtering sink the played game sounds into
    SoundSink* output = nullptr;
    ArcadeSoundMode soundMode = ArcadeSoundMode::SOUND_ON;
    ArcadeSink gameSink;
    uint32_t modeChangeMillis = 0;
    bool b_modePill = false;
    uint32_t hintStartMillis = 0;
    bool b_hintPill = false;            //the arcade just opened: how to change the sound mode

    //the top 3s (EEPROM through the store) and the last game's place in them
    ArcadeScores scores;
    uint8_t overRank = ArcadeScores::noRank;
    bool b_namePending = false;         //the game entered its top 3, the name is not saved yet
    uint32_t overScore = 0;
    char name[ArcadeScores::nameLength + 1] = {'A', 'A', 'A', 'A', 0};
    uint8_t nameCursor = 0;
    int8_t letterHold = 0;              //a stick held up (1) or down (-1) on the name entry
    uint32_t nextLetterMillis = 0;

    //the picker's cards and the clear question
    Card card = Card::NONE;
    uint32_t cardOpenMillis = 0;
    bool b_clearArmed = true;           //both pushbuttons released since the last question
    bool b_clearHeld = false;
    uint32_t clearHoldStartMillis = 0;
    StickFlick flicks;
    uint32_t lastNow = 0;

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
