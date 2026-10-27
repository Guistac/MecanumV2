#pragma once
#include "Game.h"
#include <SoundKit.h>

//Snake: steer with either stick (up, down, left, right), eat the food to grow, do not bite yourself or hit a wall.
//Open edges wrap around. A blinking diamond now and then is worth 5 for a few seconds; from level 2 a mouse runs
//from the head, worth 10. Every 10 food a new walled level, a little faster; the score counts the points.
//Sound [D-157]: an original bouncy loop behind (melody, bass, drums), faster each level; synth effects in front (eat
//blips climbing the pentatonic as the snake grows, the diamond's twinkle and arpeggio, the mouse's squeak, the level
//jingle), squares only for the death

//[D-157] The field is a grid of 42 x 10 cells at a 3 px pitch: each segment is 2 x 2, the body links its segments
//into one line so that two runs of the body side by side stay apart by 1 px of dark. Walls are hatched, never solid
class SnakeGame : public Game{
public:
    static const int16_t columns = 42;
    static const int16_t rows = 10;
    static const int16_t cellCount = columns * rows;
    static const int16_t pitch = 3;
    static const int16_t originX = 1;
    static const int16_t originY = 1;
    static const uint16_t startLength = 4;
    static const uint8_t foodPerLevel = 10;
    static const uint8_t layoutCount = 6;
    static const uint32_t foodPoints = 1;
    static const uint32_t bonusPoints = 5;
    static const uint32_t mousePoints = 10;

    enum Direction : uint8_t { RIGHT = 0, UP = 1, LEFT = 2, DOWN = 3, NONE = 4 };

    const char* name() const override { return "SNAKE"; }
    void reset(uint32_t seed) override;
    void update(const GameInput& input) override;
    void draw(Adafruit_GFX& out) const override;
    bool isOver() const override { return state == State::OVER; }
    uint32_t score() const override { return points; }
    void demo(GameInput& input) override;
    void helpLines(const char* lines[3]) const override {
        lines[0] = "ANY STICK: STEER";
        lines[1] = "EAT TO GROW, NO BITES";
        lines[2] = "DIAMOND 5  MOUSE 10";
    }
    void playMusic() override;

    //sound, for the tests: the first pitch of each effect (the eat blip: on the pentatonic from C5, one step a
    //segment grown), the music's tempo at a level
    static const uint16_t eatHertz = 523;       //C5, the first food of a level
    static const uint16_t twinkleHertz = 2637;  //E7, the diamond appears
    static const uint16_t bonusHertz = 1047;    //C6, the diamond eaten: an arpeggio up
    static const uint16_t squeakHertz = 1800;   //the mouse eaten: its squeak's sweep starts here
    static const uint16_t levelHertz = 784;     //G5, the new level's jingle
    static const uint16_t deathHertz = 1568;    //G6, the death's first square
    static const uint16_t tuneStepMillis = 170; //an eighth at tempo 100
    static const uint16_t tuneSteps = 64;       //8 bars of 4/4 in eighths
    static uint16_t musicTempo(uint16_t level);
    static uint16_t eatPitch(uint16_t grown);

    //for the tests
    uint16_t getLength() const { return length; }
    int16_t getHead() const { return segment(length - 1); }
    int16_t getFood() const { return food; }
    Direction getDirection() const { return direction; }
    bool isDying() const { return state == State::DYING; }
    bool isLevelShown() const { return state == State::LEVEL; }
    bool isOccupied(int16_t cell) const { return occupied[cell] == SNAKE; }
    bool isWall(int16_t cell) const { return occupied[cell] == WALL; }
    float getSpeed() const;                     //cells per second
    uint16_t getFoodEaten() const { return eaten; }
    uint16_t getLevel() const { return level; }
    int16_t getBonus() const { return bonus; }  //top left cell of the 2 x 2 cells, -1 when there is none
    uint16_t getBonusTicks() const { return bonusTicks; }
    int16_t getMouse() const { return mouse; }  //-1 when there is none
    //the cell next to a cell, with the wrap-around
    static int16_t neighbour(int16_t cell, uint8_t towards);
    static int16_t cellAt(int16_t column, int16_t row){ return int16_t(row * columns + column); }
    //the hand-made layouts: walls, and the tail cell of the snake at the start of the level (heading right)
    static bool layoutWall(uint8_t layout, int16_t cell);
    static int16_t layoutStart(uint8_t layout);

private:
    enum class State : uint8_t { STARTING, RUNNING, LEVEL, DYING, OVER };
    enum Cell : uint8_t { EMPTY = 0, SNAKE = 1, WALL = 2 };

    //the body, a ring of cells from the tail (index 0) to the head (index length - 1)
    int16_t segment(uint16_t index) const { return body[(tailAt + index) % cellCount]; }
    void startLevel(uint16_t number);
    void step();
    //true when nothing is on the cell: no wall, no snake, no food, bonus or mouse
    bool isFree(int16_t cell) const;
    bool inBonus(int16_t cell) const;
    bool atMouse(int16_t cell) const;
    void placeFood();
    void placeBonus();
    void placeMouse();
    void moveMouse();
    Direction requested(const GameInput& input) const;
    void queueTurn(Direction wanted);
    //true when moving onto the cell kills: a wall, the body, except the tail that moves away when it does not grow
    bool kills(int16_t cell, bool b_grows) const;
    //distances over the cells the snake could pass (the tail counts as passable) from the source cells
    void distancesFrom(const int16_t* sources, uint8_t count);
    //demo: the size of the room left after a move
    uint16_t roomAfter(int16_t newHead, bool b_grows, bool& b_reachesTail);
    void drawWalls(Adafruit_GFX& out) const;
    //sound (never game state)
    void buildTune();

    GameRandom random;
    int16_t body[cellCount];
    uint8_t occupied[cellCount];        //Cell
    uint16_t tailAt = 0;
    uint16_t length = 0;
    int16_t food = 0;
    Direction direction = RIGHT;
    Direction turns[2];                 //turns asked before the next step, so a quick turn is not lost
    uint8_t turnCount = 0;
    Direction lastRequest = NONE;
    State state = State::STARTING;
    uint16_t stateTicks = 0;
    float stepProgress = 0;             //a step is due at 1: the speed rises with every food
    uint16_t eaten = 0;                 //food of the whole game
    uint8_t levelFood = 0;              //food of this level
    uint16_t level = 1;
    uint8_t layout = 0;
    uint32_t points = 0;
    uint32_t ticks = 0;

    //the bonus diamond on 2 x 2 cells: its time left, and the steps before the next one
    int16_t bonus = -1;
    uint16_t bonusTicks = 0;
    uint16_t bonusWait = 0;
    //the mouse: its cell, the cell it slides from, its own step progress, the steps before the next one
    int16_t mouse = -1;
    int16_t mouseFrom = -1;
    uint8_t mouseSlide = 0;             //ticks since its last move
    float mouseProgress = 0;
    uint16_t mouseWait = 0;
    bool b_mouseLeft = false;           //faces left

    //the demo player
    GameRandom demoRandom;
    int16_t demoPlannedFrom = -1;
    Direction demoChoice = NONE;
    int16_t demoTargetSeen = -1;
    bool b_demoCareless = false;
    uint16_t scratchDistance[cellCount];
    int16_t scratchQueue[cellCount];

    //the music, built at the first playMusic()
    SoundKit::NoteList<224> tune;              //200 notes: 44 of melody, 32 of bass, 124 of drums
    SoundCue music;
    bool b_tuneBuilt = false;
    //an effect made at run time (the squeak's sweeps); the player reads it while it sounds
    SoundKit::NoteList<16> effect;
};
