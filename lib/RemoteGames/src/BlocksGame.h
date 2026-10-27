#pragma once
#include "Game.h"
#include <SoundKit.h>

//BLOCKS: the falling-blocks game turned a quarter turn. The well's floor is the line at the left, pieces enter at the
//right and fall leftward; a full column clears. Left stick up/down moves the piece across, left stick left drops it
//faster, left click drops it at once; right click or a right stick flick up rotates clockwise, a flick down rotates
//back. Every 10 columns the level rises and the pieces fall faster; the game ends when a new piece cannot enter.
//Sound [D-157]: Korobeiniki behind (the folk melody, our own bass, our own drums from level 3), faster each level;
//synth effects in front (move tick, rotate blip, landing thud, clear sweep or fanfare, level jingle), squares only
//for the game over
class BlocksGame : public Game{
public:
    static const int8_t wellLength = 28;    //cells from the floor (left) to the entry (right)
    static const int8_t wellWidth = 10;     //cells across, the screen's height
    static const int8_t cellPitch = 3;      //2 x 2 lit pixels and a 1 px gap
    static const int8_t clearTicks = 18;    //the wipe of cleared columns

    const char* name() const override { return "BLOCKS"; }
    void reset(uint32_t seed) override;
    void update(const GameInput& input) override;
    void draw(Adafruit_GFX& out) const override;
    bool isOver() const override { return b_over; }
    uint32_t score() const override { return points; }
    void demo(GameInput& input) override;
    void helpLines(const char* lines[3]) const override {
        lines[0] = "L UP/DN: MOVE PIECE";
        lines[1] = "L LEFT/L CLICK: DROP";
        lines[2] = "R CLICK/FLICK: ROTATE";
    }
    void playMusic() override;

    //sound, for the tests: the first pitch of each effect, the music's tempo at a level, the level the drums join
    static const uint16_t moveHertz = 2637;     //E7, a tick
    static const uint16_t rotateHertz = 1397;   //F6, a blip
    static const uint16_t landHertz = 330;      //the thud's sweep starts here
    static const uint16_t clearHertz = 523;     //the clear's sweep starts here (one to three columns)
    static const uint16_t fourHertz = 880;      //A5, the fanfare for four columns
    static const uint16_t levelHertz = 1319;    //E6, the level jingle
    static const uint16_t overHertz = 784;      //G5, the game over's first square
    static const uint8_t drumsFromLevel = 3;
    static const uint16_t tuneStepMillis = 200; //an eighth at tempo 100 (a quarter at 150 a minute)
    static const uint16_t tuneSteps = 128;      //16 bars of 4/4 in eighths: part one twice, part two twice
    static uint16_t musicTempo(uint8_t level);
    //the tune (the tests, the WAV renderer): with or without its drums, built on first use
    const SoundCue& tuneCue(bool b_withDrums);

    //for the tests
    uint8_t pieceType() const { return piece.type; }
    uint8_t nextType() const { return bag[bagIndex]; }
    uint8_t pieceRotation() const { return piece.rotation; }
    int8_t pieceAlong() const { return piece.a; }
    int8_t pieceAcross() const { return piece.b; }
    uint32_t piecesPlaced() const { return placed; }
    bool filled(int8_t a, int8_t b) const { return board[a][b] != 0; }
    uint16_t columnsCleared() const { return cleared; }
    uint8_t level() const { return uint8_t(1 + cleared / 10); }
    bool isClearing() const { return clearTimer > 0; }
    //the across extent of the active piece's cells (for the edge tests)
    void pieceSpan(int8_t& lowest, int8_t& highest) const;
    //seconds per cell of gravity at the current level
    float gravitySeconds() const;

private:
    struct Piece{
        uint8_t type = 0;
        uint8_t rotation = 0;
        int8_t a = 0;   //along: the box's corner nearest the floor
        int8_t b = 0;   //across: the box's top edge
    };
    struct Cell{ int8_t a, b; };

    static void cellsOf(uint8_t type, uint8_t rotation, Cell out[4]);
    bool fits(const Piece& p) const;
    bool tryMove(int8_t da, int8_t db);
    bool tryRotate(int8_t turn);
    void lockPiece();
    void spawn();
    uint8_t takeFromBag();
    int8_t dropDistance(const Piece& p) const;
    void collapse();

    //sound (never game state)
    void buildTune();
    void startTune();
    void playLanding(uint8_t columns);

    //the demo player
    void plan();
    float evaluate(const Piece& p) const;

    GameRandom random;
    uint8_t board[wellLength][wellWidth];
    Piece piece;
    uint8_t bag[14];            //two bags: the current one and the next, so the preview always has a piece
    uint8_t bagIndex = 0;
    uint32_t points = 0;
    uint16_t cleared = 0;
    uint32_t placed = 0;
    bool b_over = false;

    float fallTimer = 0.0f;
    uint8_t lockTimer = 0;
    uint8_t lockResets = 0;
    uint8_t clearTimer = 0;
    bool clearing[wellLength];

    //stick handling: auto-repeat across, the right stick's flick rotation
    int8_t moveDirection = 0;
    uint8_t moveTicks = 0;
    bool b_flickArmed = true;

    //the demo player's plan for the current piece
    uint32_t plannedPiece = 0xFFFFFFFFu;
    uint8_t targetRotation = 0;
    int8_t targetAcross = 0;
    uint8_t demoWait = 0;
    uint8_t demoRotateWait = 0;
    uint16_t demoTicks = 0;

    //the music, built at the first playMusic(): melody and bass, and the same with the drums
    SoundKit::NoteList<144> tune;
    SoundKit::NoteList<352> tuneDrums;         //330 notes: 138 of melody and bass, 192 of drums
    SoundCue music[2];
    bool b_tuneBuilt = false;
    //an effect made at run time (sweeps); the player reads it while it sounds
    SoundKit::NoteList<24> effect;
};
