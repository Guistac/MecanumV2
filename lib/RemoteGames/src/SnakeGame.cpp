#include "SnakeGame.h"
#include <stdio.h>

static const uint8_t startTicks = 25;           //the snake shows still for half a second before it moves
static const uint8_t levelTicks = 55;           //the "LEVEL n" pill over the new layout, about a second
static const float slowestSpeed = 6.0f;         //cells per second at the start
static const float speedPerFood = 0.12f;        //so the speed keeps rising slowly across the levels
static const float fastestSpeed = 11.5f;
static const float mouseSpeedShare = 0.5f;      //the mouse steps at half the snake's speed, it can be caught
static const uint8_t mouseSlideTicks = 6;       //it scurries to its next cell, then sits
static const uint8_t mouseAlarm = 10;           //it runs from the head nearer than this (steps), else it wanders
static const uint16_t bonusLife = 250;          //5 s on the board
static const uint16_t bonusHurry = 75;          //the last 1.5 s it blinks fast
static const uint16_t dyingTicks = 70;
static const uint16_t far = 0xFFFF;

//the effects with fixed notes (front layer; synth while the music plays, squares only for the death)
namespace{
    using W = Synth::Wave;
    const SoundNote::Kind SY = SoundNote::Kind::SYNTH, SQ = SoundNote::Kind::SQUARE;
    //the diamond appears: E7 G7 E7 A7, soft and quick
    const SoundNote twinkleNotes[] = {
        {0, SY, 0, W::SQUARE, SnakeGame::twinkleHertz, 25, 70, 10},
        {45, SY, 0, W::SQUARE, 3136, 25, 70, 10},
        {90, SY, 0, W::SQUARE, 2637, 25, 70, 10},
        {135, SY, 0, W::SQUARE, 3520, 40, 75, 20},
    };
    //the diamond eaten: C6 E6 G6 C7 E7 up, the last one ringing
    const SoundNote bonusNotes[] = {
        {0, SY, 0, W::SQUARE, SnakeGame::bonusHertz, 40, 90, 0},
        {40, SY, 0, W::SQUARE, 1319, 40, 90, 0},
        {80, SY, 0, W::SQUARE, 1568, 40, 90, 0},
        {120, SY, 0, W::SQUARE, 2093, 40, 90, 0},
        {160, SY, 0, W::SQUARE, 2637, 110, 95, 50},
    };
    //a new level: G5 C6 E6, then G6 over C6
    const SoundNote levelNotes[] = {
        {0, SY, 0, W::PULSE25, SnakeGame::levelHertz, 60, 90, 0},
        {70, SY, 0, W::PULSE25, 1047, 60, 90, 0},
        {140, SY, 0, W::PULSE25, 1319, 60, 90, 0},
        {210, SY, 0, W::PULSE25, 1568, 180, 95, 80},
        {210, SY, 1, W::SQUARE, 1047, 180, 70, 80},
    };
    //the death: G6 D6 A5 E5 C5 falling, squares (the music has stopped)
    const SoundNote deathNotes[] = {
        {0, SQ, 0, W::SQUARE, SnakeGame::deathHertz, 70, 0, 0},
        {80, SQ, 0, W::SQUARE, 1175, 70, 0, 0},
        {160, SQ, 0, W::SQUARE, 880, 70, 0, 0},
        {240, SQ, 0, W::SQUARE, 659, 70, 0, 0},
        {320, SQ, 0, W::SQUARE, 523, 180, 0, 0},
    };
};

//The layouts, one string per row ('#' a wall), from open to tighter; level n plays layout (n - 1) % layoutCount.
//The top right corner (rows 0..2, columns 34..41) stays free for the score, every free cell is reachable (tests)
static const char* const layouts[SnakeGame::layoutCount - 1][SnakeGame::rows] = {
    {   //bars
        "..........................................",
        "..........................................",
        "..........................................",
        "......##########..........##########......",
        "..........................................",
        "..........................................",
        "......##########..........##########......",
        "..........................................",
        "..........................................",
        "..........................................",
    },
    {   //three rooms with doors, the outer ones joined over the wrapped side edges
        "..........#....................#..........",
        "..........#....................#..........",
        "..........#....................#..........",
        "..........#.........##.........#..........",
        "....................##....................",
        "....................##....................",
        "..........#.........##.........#..........",
        "..........#....................#..........",
        "..........#....................#..........",
        "..........#....................#..........",
    },
    {   //a box with a door in each side
        "..........................................",
        "..........................................",
        "..........................................",
        "....###############..###############......",
        "....#..............................#......",
        "..........................................",
        "....#..............................#......",
        "....###############..###############......",
        "..........................................",
        "..........................................",
    },
    {   //a comb: the snake weaves up and down between the teeth
        "#.....#.......#.......#.......#...........",
        "#.....#.......#.......#.......#...........",
        "#.....#.......#.......#.......#...........",
        "#.....#...#...#...#...#...#...#...#...#...",
        "..........#.......#.......#.......#...#...",
        "..........#.......#.......#.......#...#...",
        "#.....#...#...#...#...#...#...#...#...#...",
        "#.....#...#...#...#...#...#...#...#...#...",
        "#.........#.......#.......#.......#.......",
        "#.........#.......#.......#.......#.......",
    },
    {   //a maze
        "...#.................#....................",
        "...#..#######..#.....#.....######.........",
        "...#..#........#.....#..........#.........",
        "......#..#######..#######..###..#..####...",
        "######...#..............#....#.......#####",
        "..............#####.....#....#............",
        "...#..####........#..####..###..######..##",
        "...#.....#..###...#.....................#.",
        "...#.....#....#...######.....#####...#..#.",
        "...#..........#..................#...#....",
    },
};
static const int16_t layoutStarts[SnakeGame::layoutCount - 1][2] = { {10, 4}, {8, 4}, {10, 4}, {1, 4}, {10, 4} };

static uint8_t opposite(uint8_t direction){ return uint8_t((direction + 2) % 4); }

bool SnakeGame::layoutWall(uint8_t layout, int16_t cell){
    if(layout == 0 || layout >= layoutCount) return false;
    return layouts[layout - 1][cell / columns][cell % columns] == '#';
}

int16_t SnakeGame::layoutStart(uint8_t layout){
    if(layout == 0 || layout >= layoutCount) return -1;
    return cellAt(layoutStarts[layout - 1][0], layoutStarts[layout - 1][1]);
}

int16_t SnakeGame::neighbour(int16_t cell, uint8_t towards){
    int16_t column = cell % columns;
    int16_t row = cell / columns;
    switch(towards){
        case RIGHT: column = int16_t((column + 1) % columns); break;
        case LEFT: column = int16_t((column + columns - 1) % columns); break;
        case UP: row = int16_t((row + rows - 1) % rows); break;    //row 0 is the top of the screen
        case DOWN: row = int16_t((row + 1) % rows); break;
        default: break;
    }
    return cellAt(column, row);
}

//the steps between two cells over the wrapped board, walls ignored
static int16_t wrappedDistance(int16_t a, int16_t b){
    int16_t dx = int16_t(a % SnakeGame::columns - b % SnakeGame::columns);
    int16_t dy = int16_t(a / SnakeGame::columns - b / SnakeGame::columns);
    if(dx < 0) dx = int16_t(-dx);
    if(dy < 0) dy = int16_t(-dy);
    if(SnakeGame::columns - dx < dx) dx = int16_t(SnakeGame::columns - dx);
    if(SnakeGame::rows - dy < dy) dy = int16_t(SnakeGame::rows - dy);
    return int16_t(dx + dy);
}

float SnakeGame::getSpeed() const {
    float speed = slowestSpeed + speedPerFood * float(eaten);
    return speed < fastestSpeed ? speed : fastestSpeed;
}

void SnakeGame::reset(uint32_t seed){
    random.seed(seed);
    demoRandom.seed(seed * 2654435761u + 1u);
    eaten = 0;
    points = 0;
    ticks = 0;
    startLevel(1);
}

//a new layout with the snake back at its start length, nothing else on the board
void SnakeGame::startLevel(uint16_t number){
    level = number;
    layout = uint8_t((number - 1) % layoutCount);
    for(int16_t cell = 0; cell < cellCount; cell++) occupied[cell] = layoutWall(layout, cell) ? WALL : EMPTY;
    int16_t start = layoutStart(layout);
    if(start < 0){
        //the open board: the start row varies a little so the demo games differ
        start = cellAt(int16_t(random.between(4, columns / 2 - 6)), int16_t(random.between(3, rows - 4)));
    }
    tailAt = 0;
    length = 0;
    for(uint16_t i = 0; i < startLength; i++){
        int16_t cell = int16_t(start + i);
        body[i] = cell;
        occupied[cell] = SNAKE;
        length++;
    }
    direction = RIGHT;
    turnCount = 0;
    state = number > 1 ? State::LEVEL : State::STARTING;
    stateTicks = 0;
    stepProgress = 0;
    levelFood = 0;
    bonus = -1;
    bonusTicks = 0;
    bonusWait = uint16_t(random.between(40, 90));
    mouse = -1;
    mouseFrom = -1;
    mouseSlide = mouseSlideTicks;
    mouseProgress = 0;
    mouseWait = uint16_t(random.between(25, 60));
    demoPlannedFrom = -1;
    demoChoice = NONE;
    demoTargetSeen = -1;
    b_demoCareless = false;
    food = -1;
    placeFood();
}

bool SnakeGame::inBonus(int16_t cell) const {
    if(bonus < 0) return false;
    return cell == bonus || cell == bonus + 1 || cell == bonus + columns || cell == bonus + columns + 1;
}

//the mouse is caught on its cell, and on the one it is still sliding away from
bool SnakeGame::atMouse(int16_t cell) const {
    if(mouse < 0) return false;
    return cell == mouse || (mouseSlide < mouseSlideTicks && cell == mouseFrom);
}

bool SnakeGame::isFree(int16_t cell) const {
    return occupied[cell] == EMPTY && cell != food && !inBonus(cell) && !atMouse(cell);
}

void SnakeGame::placeFood(){
    //the n-th free cell, so every free cell is as likely
    food = -1;
    uint32_t freeCells = 0;
    for(int16_t cell = 0; cell < cellCount; cell++) if(isFree(cell)) freeCells++;
    if(freeCells == 0) return;
    uint32_t pick = random.below(freeCells);
    for(int16_t cell = 0; cell < cellCount; cell++){
        if(!isFree(cell)) continue;
        if(pick-- == 0){
            food = cell;
            return;
        }
    }
}

//on 2 x 2 free cells (not over a wrapped edge, so it draws whole), not right next to the head
void SnakeGame::placeBonus(){
    int16_t head = getHead();
    uint32_t count = 0;
    for(int pass = 0; pass < 2; pass++){
        uint32_t pick = pass ? random.below(count) : 0;
        for(int16_t cell = 0; cell < cellCount; cell++){
            if(cell % columns == columns - 1 || cell / columns == rows - 1) continue;
            if(!isFree(cell) || !isFree(int16_t(cell + 1)) || !isFree(int16_t(cell + columns))
                || !isFree(int16_t(cell + columns + 1))) continue;
            if(wrappedDistance(cell, head) < 5) continue;
            if(pass == 0) count++;
            else if(pick-- == 0){
                bonus = cell;
                bonusTicks = bonusLife;
                return;
            }
        }
        if(count == 0) return;
    }
}

//away from the head, on a cell it can leave
void SnakeGame::placeMouse(){
    int16_t head = getHead();
    uint32_t count = 0;
    for(int pass = 0; pass < 2; pass++){
        uint32_t pick = pass ? random.below(count) : 0;
        for(int16_t cell = 0; cell < cellCount; cell++){
            if(!isFree(cell) || wrappedDistance(cell, head) < 12) continue;
            bool b_exit = false;
            for(uint8_t d = 0; d < 4; d++) if(isFree(neighbour(cell, d))) b_exit = true;
            if(!b_exit) continue;
            if(pass == 0) count++;
            else if(pick-- == 0){
                mouse = cell;
                mouseFrom = cell;
                mouseSlide = mouseSlideTicks;
                mouseProgress = 0;
                return;
            }
        }
        if(count == 0) return;
    }
}

//one mouse step: near the head it takes the free cell furthest from it (the way the snake must go round), so it
//can be cornered; far away it wanders and sits now and then. Never onto the snake, a wall or the food
void SnakeGame::moveMouse(){
    int16_t head = getHead();
    distancesFrom(&head, 1);
    uint16_t here = scratchDistance[mouse];
    bool b_alarmed = here < mouseAlarm;
    if(!b_alarmed && random.below(2)) return;
    int16_t best = mouse;
    uint16_t bestDistance = b_alarmed ? here : 0;
    uint8_t ties = 1;
    for(uint8_t d = 0; d < 4; d++){
        int16_t next = neighbour(mouse, d);
        if(occupied[next] != EMPTY || next == food || inBonus(next)) continue;
        //wandering: any free cell is as good; alarmed: further is better, a tie is a coin toss
        uint16_t distance = b_alarmed ? scratchDistance[next] : 1;
        if(distance > bestDistance){
            best = next;
            bestDistance = distance;
            ties = 1;
        } else if(distance == bestDistance && random.below(++ties) == 0) best = next;
    }
    if(best == mouse) return;
    int16_t dx = int16_t(best % columns - mouse % columns);
    if(dx == 1 || dx < -1) b_mouseLeft = false;
    else if(dx == -1 || dx > 1) b_mouseLeft = true;
    mouseFrom = mouse;
    mouse = best;
    //over a wrapped edge it jumps, a slide would cross the screen
    int16_t dy = int16_t(mouse / columns - mouseFrom / columns);
    bool b_wrapped = dx > 1 || dx < -1 || dy > 1 || dy < -1;
    mouseSlide = b_wrapped ? mouseSlideTicks : 0;
}

//the stick pushed furthest from the centre decides, along its dominant axis
SnakeGame::Direction SnakeGame::requested(const GameInput& input) const {
    float best = 0.5f;
    Direction wanted = NONE;
    for(int i = 0; i < 4; i++){
        float value = input.stick[i];
        float size = value < 0 ? -value : value;
        if(size <= best) continue;
        best = size;
        bool b_horizontal = (i % 2) == 0;
        if(b_horizontal) wanted = value > 0 ? RIGHT : LEFT;
        else wanted = value > 0 ? UP : DOWN;
    }
    return wanted;
}

//a turn counts against the last one asked (or the heading): never straight on, never back into the neck
void SnakeGame::queueTurn(Direction wanted){
    if(wanted == NONE || turnCount >= 2) return;
    Direction reference = turnCount ? turns[turnCount - 1] : direction;
    if(wanted == reference || wanted == opposite(reference)) return;
    turns[turnCount++] = wanted;
}

bool SnakeGame::kills(int16_t cell, bool b_grows) const {
    if(occupied[cell] == WALL) return true;
    if(occupied[cell] != SNAKE) return false;
    //the tail moves away in the same step unless the snake grows
    return b_grows || cell != segment(0);
}

void SnakeGame::update(const GameInput& input){
    ticks++;
    stateTicks++;
    if(mouseSlide < mouseSlideTicks) mouseSlide++;
    if(state == State::OVER) return;
    if(state == State::DYING){
        if(stateTicks >= dyingTicks) state = State::OVER;
        return;
    }

    //a new push, or a push held while nothing is queued (held up while going down, then the snake turns left)
    Direction wanted = requested(input);
    if(state == State::LEVEL){
        //the pill shows the new layout still; a stick held through it counts as a new push after it
        lastRequest = NONE;
        if(stateTicks < levelTicks) return;
        state = State::STARTING;
        stateTicks = 0;
    }
    if(wanted != NONE && (wanted != lastRequest || turnCount == 0)) queueTurn(wanted);
    lastRequest = wanted;

    if(state == State::STARTING){
        //a steer starts it at once, else it starts by itself
        if(stateTicks < startTicks && turnCount == 0) return;
        state = State::RUNNING;
        stateTicks = 0;
        stepProgress = 1.0f;
    }

    if(bonus >= 0 && --bonusTicks == 0){
        bonus = -1;
        bonusWait = uint16_t(random.between(50, 110));
    }
    if(mouse >= 0){
        mouseProgress += getSpeed() * mouseSpeedShare * tickSeconds;
        if(mouseProgress >= 1.0f){
            mouseProgress -= 1.0f;
            moveMouse();
        }
    }
    stepProgress += getSpeed() * tickSeconds;
    if(stepProgress >= 1.0f){
        stepProgress -= 1.0f;
        step();
    }
}

void SnakeGame::step(){
    if(turnCount){
        direction = turns[0];
        turns[0] = turns[1];
        turnCount--;
    }
    int16_t next = neighbour(getHead(), direction);
    bool b_food = next == food;
    bool b_mouse = atMouse(next);
    bool b_bonus = inBonus(next);
    bool b_grows = b_food || b_mouse;
    if(kills(next, b_grows)){
        state = State::DYING;
        stateTicks = 0;
        //the end: the music stops, falling notes
        sound().stop(SoundLayer::BACK);
        sound().play(soundCue(deathNotes, SoundPriority::GAME));
        return;
    }
    if(!b_grows){
        occupied[segment(0)] = EMPTY;
        tailAt = uint16_t((tailAt + 1) % cellCount);
        length--;
    }
    body[(tailAt + length) % cellCount] = next;
    occupied[next] = SNAKE;
    length++;

    if(b_bonus){
        points += bonusPoints;
        bonus = -1;
        bonusWait = uint16_t(random.between(50, 110));
    }
    if(b_mouse){
        points += mousePoints;
        mouse = -1;
        mouseWait = uint16_t(random.between(60, 120));
    }
    if(b_food){
        points += foodPoints;
        eaten++;
        levelFood++;
        if(levelFood >= foodPerLevel){
            startLevel(uint16_t(level + 1));
            //a jingle, the music a little faster
            sound().play(soundCue(levelNotes, SoundPriority::GAME));
            sound().setTempo(SoundLayer::BACK, musicTempo(level));
            return;
        }
        placeFood();
    }
    //the extras come after a number of steps, so a slow player sees as many as a quick one
    bool b_bonusAppears = false;
    if(bonus < 0 && bonusWait && --bonusWait == 0){
        placeBonus();
        b_bonusAppears = bonus >= 0;
    }
    if(level > 1 && mouse < 0 && mouseWait && --mouseWait == 0) placeMouse();

    //one effect a step, the most telling one
    if(b_bonus) sound().play(soundCue(bonusNotes, SoundPriority::GAME));
    else if(b_mouse){
        //a squeak: two quick chirps up
        effect.clear();
        effect.sweep(0, squeakHertz, 3400, 50, 5, Synth::Wave::PULSE25, 85);
        effect.sweep(70, 2200, 3800, 40, 4, Synth::Wave::PULSE25, 80, 0, 15);
        sound().play(effect.cue());
    }
    else if(b_food){
        sound().playNote(SoundKit::synthNote(0, eatPitch(uint16_t(length - startLength - 1)), 45, 90, 25),
                         SoundPriority::GAME);
    }
    else if(b_bonusAppears) sound().play(soundCue(twinkleNotes, SoundPriority::GAME));
}

uint16_t SnakeGame::musicTempo(uint16_t level){
    //6 % faster a level, at most 160 %
    uint32_t percent = 100u + 6u * (level > 1 ? level - 1u : 0u);
    return uint16_t(percent < 160 ? percent : 160);
}

//the eat blip: one step up the C pentatonic from C5 a segment grown in this level, up to A7
uint16_t SnakeGame::eatPitch(uint16_t grown){
    return SoundKit::hertz(SoundKit::pentatonic(SoundKit::C5, uint8_t(grown < 14 ? grown : 14)));
}

//Our own tune, in C: a hopping melody of short notes over C Am F G, a bass that bounces root, octave, fifth, octave,
//the drums with a skip (kick on 1 and the "and" of 2, snare on 3).
//Written for the disc (loud only from about 1 kHz to 5 kHz, nearly silent below 400 Hz): the melody from A5 to D7
//(880 to 2349 Hz) on a full square, the bass an octave up (A3 to G5) on a narrow pulse, short, quieter than the
//melody; the synth clips the sum of its voices at 1, so the bass is plucked and the drums sit under both
void SnakeGame::buildTune(){
    using namespace SoundKit;
    //Arduino owns A0 to A17 (the analog pins): the A notes under other names
    const uint8_t a3 = SoundKit::A3, a4 = SoundKit::A4, a5 = SoundKit::A5, a6 = SoundKit::A6;
    static const uint8_t melody[64] PROGMEM = {
        C6, REST, E6, G6,   REST, G6, a6, G6,       //C
        E6, REST, C6, REST, a5, C6, E6, REST,       //Am
        F6, REST, a6, C7,   REST, C7, D7, C7,       //F
        B6, REST, G6, REST, D6, REST, G6, REST,     //G
        C6, REST, E6, G6,   REST, G6, a6, G6,       //C
        C7, REST, a6, REST, E6, REST, a6, G6,       //Am
        F6, a6, C7, a6,     G6, B6, D7, B6,         //F G
        C7, REST, G6, E6,   C6, REST, REST, REST,   //C
    };
    //a quarter an entry
    static const uint8_t bass[32] PROGMEM = {
        C4, C5, G4, C5,   a3, a4, E4, a4,   F4, F5, C5, F5,   G4, G5, D5, G5,
        C4, C5, G4, C5,   a3, a4, E4, a4,   F4, F5, G4, G5,   C4, C5, G4, C5,
    };
    tune.clear();
    Tracker t(tune, tuneStepMillis);
    t.instrument(0, Synth::Wave::SQUARE, 80, 200).instrument(1, Synth::Wave::PULSE12, 40, 60);
    t.line(0, melody, 64, 0, 1, 92).line(1, bass, 32, 0, 2, 50);
    t.drums("K.HKS.H.K.HKS.HH", 0, tuneSteps / 16, 70);
    music = t.cue(tuneSteps);
    b_tuneBuilt = true;
}

void SnakeGame::playMusic(){
    if(state == State::DYING || state == State::OVER) return;
    if(!b_tuneBuilt) buildTune();
    sound().setTempo(SoundLayer::BACK, musicTempo(level));
    sound().play(music, SoundLayer::BACK);
}

//walls are hatched (every other pixel, also over the 1 px links between wall cells), so they never read as the snake
void SnakeGame::drawWalls(Adafruit_GFX& out) const {
    for(int16_t cell = 0; cell < cellCount; cell++){
        if(occupied[cell] != WALL) continue;
        int16_t column = cell % columns;
        int16_t row = cell / columns;
        bool b_right = column < columns - 1 && occupied[cell + 1] == WALL;
        bool b_below = row < rows - 1 && occupied[cell + columns] == WALL;
        bool b_corner = b_right && b_below && occupied[cell + columns + 1] == WALL;
        int16_t x0 = int16_t(originX + column * pitch);
        int16_t y0 = int16_t(originY + row * pitch);
        for(int16_t dy = 0; dy < 3; dy++){
            for(int16_t dx = 0; dx < 3; dx++){
                if(dx == 2 && dy == 2){
                    if(!b_corner) continue;
                } else if(dx == 2 && !b_right) continue;
                else if(dy == 2 && !b_below) continue;
                int16_t x = int16_t(x0 + dx), y = int16_t(y0 + dy);
                if(((x + y) & 1) == 0) out.drawPixel(x, y, GameDraw::ON);
            }
        }
    }
}

void SnakeGame::draw(Adafruit_GFX& out) const {
    drawWalls(out);

    //dying: the body flashes about 2.5 times a second around the head that hit, which stays lit to show where
    bool b_bodyVisible = state != State::DYING || (stateTicks % 20) < 12;
    for(uint16_t i = 0; i < length; i++){
        int16_t cell = segment(i);
        int16_t x = int16_t(originX + (cell % columns) * pitch);
        int16_t y = int16_t(originY + (cell / columns) * pitch);
        bool b_head = i == length - 1;
        if(!b_head && !b_bodyVisible) continue;
        out.fillRect(x, y, 2, 2, GameDraw::ON);
        //the link to the segment before fills the 1 px gap, except across a wrapped edge, and the head stays
        //apart from its neck so it reads as the head
        if(i == 0 || b_head) continue;
        int16_t before = segment(uint16_t(i - 1));
        int16_t dx = int16_t(before % columns - cell % columns);
        int16_t dy = int16_t(before / columns - cell / columns);
        if(dx == 1) out.drawFastVLine(x + 2, y, 2, GameDraw::ON);
        else if(dx == -1) out.drawFastVLine(x - 1, y, 2, GameDraw::ON);
        else if(dy == 1) out.drawFastHLine(x, y + 2, 2, GameDraw::ON);
        else if(dy == -1) out.drawFastHLine(x, y - 1, 2, GameDraw::ON);
    }

    //the food twinkles between its two diagonals, never a solid square like the snake
    if(food >= 0){
        int16_t x = int16_t(originX + (food % columns) * pitch);
        int16_t y = int16_t(originY + (food / columns) * pitch);
        bool b_phase = (ticks / 15) % 2 == 0;
        out.drawPixel(x + (b_phase ? 0 : 1), y, GameDraw::ON);
        out.drawPixel(x + (b_phase ? 1 : 0), y + 1, GameDraw::ON);
    }

    //the bonus: a diamond outline over its 2 x 2 cells, off a moment now and then, blinking fast before it goes
    if(bonus >= 0){
        bool b_lit = bonusTicks > bonusHurry ? (bonusTicks % 40) >= 8 : (bonusTicks % 12) >= 6;
        if(b_lit){
            int16_t x = int16_t(originX + (bonus % columns) * pitch + 2);
            int16_t y = int16_t(originY + (bonus / columns) * pitch + 2);
            out.drawLine(x, y - 2, x + 2, y, GameDraw::ON);
            out.drawLine(x + 2, y, x, y + 2, GameDraw::ON);
            out.drawLine(x, y + 2, x - 2, y, GameDraw::ON);
            out.drawLine(x - 2, y, x, y - 2, GameDraw::ON);
        }
    }

    //the mouse: a body with its head up front and a tail that flicks; it scurries from cell to cell
    if(mouse >= 0){
        float share = float(mouseSlide) / float(mouseSlideTicks);
        float fromX = float(originX + (mouseFrom % columns) * pitch);
        float fromY = float(originY + (mouseFrom / columns) * pitch);
        float toX = float(originX + (mouse % columns) * pitch);
        float toY = float(originY + (mouse / columns) * pitch);
        int16_t x = int16_t(fromX + (toX - fromX) * share + 0.5f);
        int16_t y = int16_t(fromY + (toY - fromY) * share + 0.5f);
        int16_t front = b_mouseLeft ? x : int16_t(x + 1);
        int16_t back = b_mouseLeft ? int16_t(x + 1) : x;
        out.drawPixel(front, y, GameDraw::ON);
        out.drawPixel(x, y + 1, GameDraw::ON);
        out.drawPixel(x + 1, y + 1, GameDraw::ON);
        //the tail in the gap behind it, only when the cell behind is empty (nothing touches)
        int16_t behind = neighbour(mouse, b_mouseLeft ? RIGHT : LEFT);
        bool b_wrapped = b_mouseLeft ? mouse % columns == columns - 1 : mouse % columns == 0;
        if(!b_wrapped && occupied[behind] == EMPTY && behind != food && !inBonus(behind)){
            int16_t tailX = b_mouseLeft ? int16_t(back + 1) : int16_t(back - 1);
            out.drawPixel(tailX, (ticks / 8) % 2 ? y + 1 : y, GameDraw::ON);
        }
    }

    if(state == State::LEVEL){
        char text[12];
        snprintf(text, sizeof(text), "LEVEL %u", (unsigned)level);
        GameDraw::pillCentered(out, 64, 12, text);
        return;
    }

    //the score in the top right corner, only while nothing of the game is under it (it never hides the snake)
    char text[12];
    snprintf(text, sizeof(text), "%lu", (unsigned long)points);
    int16_t left = int16_t(width - GameDraw::textWidth(text));
    //cells whose 2 x 2 square (with the 1 px gap around it) reaches the text area
    int16_t firstColumn = int16_t((left - 2 - originX) / pitch);
    if(firstColumn < 0) firstColumn = 0;
    int16_t lastRow = int16_t((7 + 1 - originY) / pitch);
    for(int16_t row = 0; row <= lastRow; row++){
        for(int16_t column = firstColumn; column < columns; column++){
            int16_t x = int16_t(originX + column * pitch);
            int16_t y = int16_t(originY + row * pitch);
            if(x + 1 < left - 2 || y > 7) continue;
            int16_t cell = cellAt(column, row);
            if(occupied[cell] != EMPTY || cell == food || inBonus(cell) || atMouse(cell)) return;
        }
    }
    GameDraw::text(out, left, 0, text);
}

void SnakeGame::distancesFrom(const int16_t* sources, uint8_t count){
    for(int16_t i = 0; i < cellCount; i++) scratchDistance[i] = far;
    int16_t tail = segment(0);
    int16_t head = 0, end = 0;
    for(uint8_t i = 0; i < count; i++){
        if(sources[i] < 0 || scratchDistance[sources[i]] == 0) continue;
        scratchDistance[sources[i]] = 0;
        scratchQueue[end++] = sources[i];
    }
    while(head < end){
        int16_t cell = scratchQueue[head++];
        for(uint8_t d = 0; d < 4; d++){
            int16_t next = neighbour(cell, d);
            if(scratchDistance[next] != far) continue;
            if(occupied[next] != EMPTY && next != tail) continue;
            scratchDistance[next] = uint16_t(scratchDistance[cell] + 1);
            scratchQueue[end++] = next;
        }
    }
}

//the free cells the head could still reach after moving to newHead; also whether it can follow its own tail out
uint16_t SnakeGame::roomAfter(int16_t newHead, bool b_grows, bool& b_reachesTail){
    int16_t tail = segment(0);
    //the tail cell after the move: it stays when growing, else the next segment becomes the tail
    int16_t newTail = b_grows ? tail : segment(1);
    for(int16_t i = 0; i < cellCount; i++) scratchDistance[i] = occupied[i] != EMPTY ? 1 : 0;
    if(!b_grows) scratchDistance[tail] = 0;
    scratchDistance[newHead] = 1;
    scratchDistance[newTail] = 0;
    b_reachesTail = false;
    int16_t head = 0, end = 0;
    scratchQueue[end++] = newHead;
    uint16_t room = 0;
    while(head < end){
        int16_t cell = scratchQueue[head++];
        for(uint8_t d = 0; d < 4; d++){
            int16_t next = neighbour(cell, d);
            if(scratchDistance[next]) continue;
            scratchDistance[next] = 1;
            if(next == newTail) b_reachesTail = true;
            room++;
            scratchQueue[end++] = next;
        }
    }
    return room;
}

//a fair player: the shortest way to the food, or to the bonus when it gets there in time, or after the mouse when it
//is not much further, avoiding dead ends it can see; now and then it does not look and steers into a pocket, which
//ends a demo game in about a minute
void SnakeGame::demo(GameInput& input){
    if(state == State::DYING || state == State::OVER || state == State::LEVEL) return;
    int16_t headCell = getHead();
    if(headCell != demoPlannedFrom){
        demoPlannedFrom = headCell;
        //what to go for, by the way there from the head
        distancesFrom(&headCell, 1);
        uint16_t toFood = food >= 0 ? scratchDistance[food] : far;
        int16_t targets[4] = { food, -1, -1, -1 };
        uint8_t targetCount = 1;
        int16_t targetKey = food;
        if(bonus >= 0){
            int16_t cells[4] = { bonus, int16_t(bonus + 1), int16_t(bonus + columns), int16_t(bonus + columns + 1) };
            uint16_t toBonus = far;
            for(uint8_t i = 0; i < 4; i++) if(scratchDistance[cells[i]] < toBonus) toBonus = scratchDistance[cells[i]];
            float ticksThere = float(toBonus) / getSpeed() / tickSeconds;
            if(toBonus != far && ticksThere + 15.0f < float(bonusTicks)){
                for(uint8_t i = 0; i < 4; i++) targets[i] = cells[i];
                targetCount = 4;
                targetKey = int16_t(cellCount + bonus);
            }
        }
        if(targetCount == 1 && mouse >= 0 && scratchDistance[mouse] != far && scratchDistance[mouse] < toFood + 12){
            targets[0] = mouse;
            targetKey = int16_t(2 * cellCount);
        }
        //once per target it decides whether it looks ahead on the way there; it gets sloppier as the game goes on
        if(targetKey != demoTargetSeen){
            demoTargetSeen = targetKey;
            b_demoCareless = demoRandom.unit() < 0.08f + 0.012f * float(eaten);
        }
        bool b_careless = b_demoCareless;
        distancesFrom(targets, targetCount);
        uint16_t toTarget[3];
        Direction options[3];
        uint8_t count = 0;
        for(uint8_t d = 0; d < 4; d++){
            if(d == opposite(direction)) continue;
            int16_t next = neighbour(headCell, d);
            if(kills(next, next == food || atMouse(next))) continue;
            options[count] = Direction(d);
            toTarget[count] = scratchDistance[next];
            count++;
        }
        Direction choice = NONE;
        if(count){
            //rated by distance to the target; straight on wins a tie, as a person would keep the stick still
            uint32_t bestScore = 0xFFFFFFFFu;
            uint16_t bestRoom = 0;
            Direction roomiest = options[0];
            for(uint8_t i = 0; i < count; i++){
                int16_t next = neighbour(headCell, options[i]);
                bool b_safe = true;
                if(!b_careless){
                    bool b_tail = false;
                    uint16_t room = roomAfter(next, next == food || atMouse(next), b_tail);
                    b_safe = b_tail || room >= length;
                    if(room > bestRoom){
                        bestRoom = room;
                        roomiest = options[i];
                    }
                }
                if(!b_safe) continue;
                uint32_t rating = uint32_t(toTarget[i]) * 2u + (options[i] == direction ? 0u : 1u);
                if(rating < bestScore){
                    bestScore = rating;
                    choice = options[i];
                }
            }
            if(choice == NONE) choice = roomiest;
        }
        //and at speed a thumb is not perfect: now and then a turn comes one step late, which in a corridor runs
        //into the wall ahead, or a turn it did not mean slips in, which may run into the body
        float sloppy = 0.0015f * float(eaten);
        if(choice != direction && demoRandom.unit() < 2.0f * sloppy) choice = direction;
        else if(demoRandom.unit() < sloppy){
            Direction slip = Direction((direction + (demoRandom.below(2) ? 1 : 3)) % 4);
            if(occupied[neighbour(headCell, slip)] != WALL) choice = slip;
        }
        demoChoice = choice;
    }
    if(demoChoice == NONE || demoChoice == direction) return;
    switch(demoChoice){
        case RIGHT: input.stick[0] = 1.0f; break;
        case LEFT: input.stick[0] = -1.0f; break;
        case UP: input.stick[1] = 1.0f; break;
        case DOWN: input.stick[1] = -1.0f; break;
        default: break;
    }
}
