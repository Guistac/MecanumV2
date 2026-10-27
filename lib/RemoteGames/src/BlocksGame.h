#pragma once
#include "Game.h"

//BLOCKS: the falling-blocks game turned a quarter turn. The well's floor is the line at the left, pieces enter at the
//right and fall leftward; a full column clears. Left stick up/down moves the piece across, left stick left drops it
//faster, left click drops it at once; right click or a right stick flick up rotates clockwise, a flick down rotates
//back. Every 10 columns the level rises and the pieces fall faster; the game ends when a new piece cannot enter.
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
};
