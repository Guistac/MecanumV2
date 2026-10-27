//STRIKE, the remote's brick-breaker: striking the set [D-157]. Rules and stages in StrikeGame.h and docs/arcade/README.md
#include "StrikeGame.h"
#include <math.h>
#include <stdio.h>

const uint16_t StrikeGame::paddleHertz;

namespace{

    //the stages, as written: 5 rows of 10 places, column 0 nearest the paddle. '.' nothing, 'o' a set piece, '2' a
    //hatched piece (2 hits), '#' truss (never breaks), 'w' 'm' 's' a prize piece dropping the wide paddle, three balls,
    //the slow ball. In flash (PROGMEM, the Teensy 4 reads its flash directly)
    struct Stage{
        char name[11];
        char rows[StrikeGame::rows][StrikeGame::columns + 1];
    };

    const Stage stages[StrikeGame::stageCount] PROGMEM = {
        //flats standing in a row: open lanes between them, no truss yet
        {"FLATS", {
            ".o.o.2.o.o",
            ".o.o.o.o.o",
            ".o.w.o.m.o",
            ".o.o.o.o.o",
            ".o.o.2.o.o"}},
        //three rostra, one above the other: shots along the empty levels reach the back
        {"PLATFORMS", {
            "oooooo2ooo",
            "..........",
            "..oowoo2oo",
            "..........",
            "oooooo2ooo"}},
        //a flight of steps up to the right, the nosings hatched, a truss stringer under the top
        {"STAIRCASE", {
            "........2o",
            "......2ooo",
            "....2ooomo",
            "..2ooooo#o",
            "2oooooo###"}},
        //the proscenium arch: a truss portal open only in the middle row, the set behind it
        {"PROSCENIUM", {
            "#2ooooooo2",
            "#oo2oo2ooo",
            ".ooowooooo",
            "#oo2oo2ooo",
            "#2ooooooo2"}},
        //masking wings hung from the top and standing from the floor, in turn: a slalom
        {"WINGS", {
            "#o2oo.#o2o",
            "#ooos.#oo.",
            "#oo#oo#oo#",
            ".2o#o2.o2#",
            ".oo#2o.oo#"}},
        //two truss battens flown across the set, a gap in each: three lanes of pieces
        {"BATTENS", {
            "oo2oooo2oo",
            "##.####.##",
            "ooooowoooo",
            "####.###.#",
            "oo2oooo2oo"}},
        //the revolve: a ring of pieces round a truss hub, room to go round behind it
        {"REVOLVE", {
            "...oo2oo..",
            "..o2o#o2o.",
            "..oo###mo.",
            "..o2o#o2o.",
            "...oo2oo..",
        }},
        //the trap room: cells of pieces between truss joists, reached from the corridor in the middle row
        {"TRAP ROOM", {
            ".oo#oo#oo#",
            ".o2#2o#s2#",
            "..........",
            ".o2#2o#o2#",
            ".oo#oo#oo#"}},
        //barn doors: truss leaves angled in from the corners, a narrow slot between them
        {"BARN DOORS", {
            "#2oooooo2o",
            ".#o2oo2ooo",
            "..oowmoooo",
            ".#o2oo2ooo",
            "#2oooooo2o"}},
        //the orchestra pit: walled in front, open only along the top and the bottom edges
        {"ORCH PIT", {
            "..o2oooo2o",
            "#ooo2oooo#",
            "#o2ooso2o#",
            "#ooo2oooo#",
            "..o2oooo2o"}},
        //the fly tower: three truss towers, each open at one end, a zigzag of pieces behind them
        {"FLY TOWER", {
            ".#oo..o#o2",
            ".#o2#oo#oo",
            ".#oo#2w..2",
            "..2o#oo#oo",
            "..oo#oo#o2"}},
        //the load out: everything at once, most of it hatched
        {"LOAD OUT", {
            "#2o2#o2o22",
            ".o#oo2o#2o",
            ".2o.#mo#2o",
            ".o#oo2o#2o",
            "#2o2#o2o22"}},
    };

    StrikeGame::Brick brickFor(char c){
        switch(c){
            case 'o': return StrikeGame::Brick::PIECE;
            case '2': return StrikeGame::Brick::DOUBLE;
            case '#': return StrikeGame::Brick::TRUSS;
            case 'w': return StrikeGame::Brick::PRIZE_WIDE;
            case 'm': return StrikeGame::Brick::PRIZE_MULTI;
            case 's': return StrikeGame::Brick::PRIZE_SLOW;
            default: return StrikeGame::Brick::EMPTY;
        }
    }

    bool isBreakable(uint8_t b){
        StrikeGame::Brick k = StrikeGame::Brick(b);
        return k != StrikeGame::Brick::EMPTY && k != StrikeGame::Brick::TRUSS;
    }

    float clampf(float value, float lowest, float highest){
        return value < lowest ? lowest : (value > highest ? highest : value);
    }

    const float ballBottom = float(StrikeGame::fieldHeight - StrikeGame::ballSize);    //the ball's top, lowest
    const float rideX = float(StrikeGame::paddleFace + 1);     //the ball on the paddle, 1 px off its face
    const float launchAngle = 0.45f;                            //rad, toward the middle of the field
    const float multiSpread = 0.4f;                             //rad between the three balls
    const float nudgeAngle = 0.25f;
    //the score at the top of the gap between the paddle and the set; the ball passes over it with a dark margin
    const int16_t hudX = 16;            //clear of the picker's sound icon (top left, x 0 to 13) too
    const int16_t hudY = 1;
    const int16_t gapCentre = 39;                               //the middle of the gap, for the stage's cards

    //the power-ups' glyphs, 3 x 5, a row's 3 bits from the left: the wide paddle (a tall bar with end caps), three
    //balls (three short ones in a triangle), the slow ball (a pause sign)
    const uint8_t glyphs[3][5] = {
        {0b111, 0b010, 0b010, 0b010, 0b111},
        {0b010, 0b010, 0b000, 0b101, 0b101},
        {0b101, 0b101, 0b101, 0b101, 0b101},
    };

    //the sounds: timer squares, Strike has no music to cut. The pieces on the C major pentatonic from C6 (the bottom
    //row) to A6 (the top), the paddle a G5 under them all
    const uint16_t blipMillis = 30;
    const uint16_t crackMillis = 22;
    const uint16_t tickMillis = 18;
    const SoundNote::Kind SQ = SoundNote::Kind::SQUARE;
    //a dull thud off the truss: a rough buzz of two close low squares (a duet, D5 and D#5), under everything else
    const SoundNote thudNotes[] = {
        {0, SoundNote::Kind::DUET, 0, Synth::Wave::SQUARE, 587, 40, 0, 0, 622},
    };
    //power-up caught: C6 E6 G6 C7, quick
    const SoundNote catchNotes[] = {
        {0, SQ, 0, Synth::Wave::SQUARE, 1047, 40, 0, 0, 0},
        {45, SQ, 0, Synth::Wave::SQUARE, 1319, 40, 0, 0, 0},
        {90, SQ, 0, Synth::Wave::SQUARE, 1568, 40, 0, 0, 0},
        {135, SQ, 0, Synth::Wave::SQUARE, 2093, 70, 0, 0, 0},
    };
    //ball lost: G5 E5 C5 G4 falling, over in 350 ms (before the Arcade's game over phrase at 400 ms)
    const SoundNote lostNotes[] = {
        {0, SQ, 0, Synth::Wave::SQUARE, 784, 70, 0, 0, 0},
        {80, SQ, 0, Synth::Wave::SQUARE, 659, 70, 0, 0, 0},
        {160, SQ, 0, Synth::Wave::SQUARE, 523, 70, 0, 0, 0},
        {240, SQ, 0, Synth::Wave::SQUARE, 392, 110, 0, 0, 0},
    };
    //stage struck: C6 E6 G6, E6 G6 C7 ringing
    const SoundNote struckNotes[] = {
        {0, SQ, 0, Synth::Wave::SQUARE, 1047, 60, 0, 0, 0},
        {70, SQ, 0, Synth::Wave::SQUARE, 1319, 60, 0, 0, 0},
        {140, SQ, 0, Synth::Wave::SQUARE, 1568, 90, 0, 0, 0},
        {250, SQ, 0, Synth::Wave::SQUARE, 1319, 60, 0, 0, 0},
        {320, SQ, 0, Synth::Wave::SQUARE, 1568, 60, 0, 0, 0},
        {390, SQ, 0, Synth::Wave::SQUARE, 2093, 220, 0, 0, 0},
    };
}

const char* StrikeGame::stageName(uint8_t index){ return stages[index % stageCount].name; }

StrikeGame::Brick StrikeGame::stageBrick(uint8_t index, uint8_t row, uint8_t column){
    return brickFor(stages[index % stageCount].rows[row % rows][column % columns]);
}

uint16_t StrikeGame::brickHertz(uint8_t row){
    static const uint16_t pitches[rows] = {1760, 1568, 1319, 1175, 1047};      //A6 G6 E6 D6 C6, top row first
    return pitches[row % rows];
}

SoundCue StrikeGame::thudSound(){ return soundCue(thudNotes, SoundPriority::GAME); }
SoundCue StrikeGame::catchSound(){ return soundCue(catchNotes, SoundPriority::GAME); }
SoundCue StrikeGame::lostSound(){ return soundCue(lostNotes, SoundPriority::GAME); }
SoundCue StrikeGame::struckSound(){ return soundCue(struckNotes, SoundPriority::GAME); }

//analog: no movement in the dead zone, then the speed grows with the deflection to full speed at full deflection
float StrikeGame::paddleVelocity(float stickY){
    float magnitude = fabsf(stickY);
    if(magnitude <= stickDeadZone) return 0.0f;
    float share = clampf((magnitude - stickDeadZone) / (1.0f - stickDeadZone), 0.0f, 1.0f);
    //screen y grows downward, the stick's y upward
    return stickY > 0.0f ? -paddleSpeed * share : paddleSpeed * share;
}

float StrikeGame::ballSpeedFor(uint8_t loopIndex, uint16_t brokenSinceLost){
    float speed = baseSpeed + loopSpeedStep * float(loopIndex) + brickSpeedStep * float(brokenSinceLost / 10);
    return speed > topSpeed ? topSpeed : speed;
}

float StrikeGame::getSpeed() const {
    return ballSpeedFor(loop, brokenSinceLost) * (slowLeft > 0 ? slowShare : 1.0f);
}

uint8_t StrikeGame::getBallCount() const {
    uint8_t n = 0;
    for(const Ball& b : balls) if(b.b_live) n++;
    return n;
}

uint8_t StrikeGame::getPrizeCount() const {
    uint8_t n = 0;
    for(const Prize& p : prizes) if(p.b_live) n++;
    return n;
}

uint16_t StrikeGame::breakablesLeft() const {
    uint16_t n = 0;
    for(uint8_t r = 0; r < rows; r++) for(uint8_t c = 0; c < columns; c++) if(isBreakable(bricks[r][c])) n++;
    return n;
}

void StrikeGame::overText(char* out, size_t size, uint32_t value) const {
    snprintf(out, size, "%lu STAGE %u", (unsigned long)value, unsigned(getStageNumber()));
}

void StrikeGame::reset(uint32_t seed){
    random.seed(seed);
    demoRandom.seed(seed * 2654435761u + 1u);
    points = 0;
    lives = startLives;
    brokenSinceLost = 0;
    frame = 0;
    demoPlayedTicks = 0;
    paddleTop = float(fieldHeight - paddleHeight) / 2.0f;
    startStage(0, 0);
}

void StrikeGame::startStage(uint8_t index, uint8_t loopIndex){
    loop = loopIndex;
    startStageNow(index % stageCount);
}

void StrikeGame::startStageNow(uint8_t index){
    stage = index;
    for(uint8_t r = 0; r < rows; r++) for(uint8_t c = 0; c < columns; c++) bricks[r][c] = uint8_t(stageBrick(stage, r, c));
    if(wideLeft > 0) paddleTop += float(widePaddleHeight - paddleHeight) / 2.0f;
    wideLeft = slowLeft = 0;
    serve();
    phase = Phase::INTRO;
    phaseTicks = 0;
}

void StrikeGame::clearSet(){
    for(uint8_t r = 0; r < rows; r++) for(uint8_t c = 0; c < columns; c++) bricks[r][c] = uint8_t(Brick::EMPTY);
}

//the ball back on the paddle, the power-ups in flight gone
void StrikeGame::serve(){
    phase = Phase::SERVE;
    phaseTicks = 0;
    for(Ball& b : balls) b = Ball{0, 0, 1, 0, false};
    for(Prize& p : prizes) p.b_live = false;
    balls[0].b_live = true;
    balls[0].x = rideX;
    balls[0].y = paddleTop + float(getPaddleHeight() - ballSize) / 2.0f;
    calmTicks = 0;
    demoServeTicks = uint16_t(demoRandom.between(15, 60));
    demoBall = -1;
    for(bool& b : b_demoHeadingLeft) b = false;
}

//off toward the middle of the field: down from the top half, up from the bottom half
void StrikeGame::launch(){
    Ball& b = balls[0];
    float centre = paddleTop + float(getPaddleHeight()) / 2.0f;
    float angle = centre < float(fieldHeight) / 2.0f ? launchAngle : -launchAngle;
    b.dx = cosf(angle);
    b.dy = sinf(angle);
    phase = Phase::PLAY;
    phaseTicks = 0;
    calmTicks = 0;
}

void StrikeGame::placeBall(float x, float y, float dx, float dy){
    for(Ball& b : balls) b.b_live = false;
    float length = sqrtf(dx * dx + dy * dy);
    balls[0] = Ball{x, y, dx / length, dy / length, true};
    phase = Phase::PLAY;
    phaseTicks = 0;
}

void StrikeGame::dropPrize(float x, float y, PowerUp kind){
    for(Prize& p : prizes){
        if(p.b_live) continue;
        p = Prize{x, y, kind, true};
        return;
    }
}

void StrikeGame::movePaddle(float stickY){
    float h = float(getPaddleHeight());
    paddleTop = clampf(paddleTop + paddleVelocity(stickY) * tickSeconds, 0.0f, float(fieldHeight) - h);
}

//never steeper than the minimum horizontal share: it cannot bounce up and down for ever
void StrikeGame::keepHorizontal(Ball& b){
    if(fabsf(b.dx) >= minHorizontal) return;
    b.dx = b.dx < 0.0f ? -minHorizontal : minHorizontal;
    float vertical = sqrtf(1.0f - minHorizontal * minHorizontal);
    b.dy = b.dy < 0.0f ? -vertical : vertical;
}

//turned a little, one way or the other: a ball caught in a loop that touches nothing breakable finds a way out
void StrikeGame::nudge(Ball& b){
    float angle = random.below(2) ? nudgeAngle : -nudgeAngle;
    float c = cosf(angle), s = sinf(angle);
    float dx = b.dx * c - b.dy * s;
    float dy = b.dx * s + b.dy * c;
    //never turned back toward the other side
    if((dx < 0.0f) != (b.dx < 0.0f)) dx = -dx;
    b.dx = dx;
    b.dy = dy;
    keepHorizontal(b);
}

//the hit's place on the paddle sets the angle: its centre flat, its ends steep
void StrikeGame::bounceOffPaddle(Ball& b){
    float half = float(getPaddleHeight()) / 2.0f;
    float offset = (b.y + float(ballSize) / 2.0f) - (paddleTop + half);
    float relative = clampf(offset / (half + 1.0f), -1.0f, 1.0f);
    float angle = relative * maxBounceAngle;
    b.dx = cosf(angle);
    b.dy = sinf(angle);
    keepHorizontal(b);
    calmTicks = 0;
    if(heard == Heard::NONE) heard = Heard::PADDLE;
}

void StrikeGame::hitBrick(uint8_t row, uint8_t column){
    Brick b = Brick(bricks[row][column]);
    switch(b){
        case Brick::EMPTY: return;
        case Brick::TRUSS:
            if(heard == Heard::NONE || heard == Heard::PADDLE) heard = Heard::THUD;
            return;
        case Brick::DOUBLE:
            bricks[row][column] = uint8_t(Brick::CRACKED);
            points += crackPoints;
            if(heard != Heard::BREAK){
                heard = Heard::CRACK;
                soundRow = row;
            }
            calmTicks = 0;
            return;
        case Brick::PRIZE_WIDE: case Brick::PRIZE_MULTI: case Brick::PRIZE_SLOW:
            dropPrize(float(brickX(column)), float(brickY(row)), PowerUp(uint8_t(b) - uint8_t(Brick::PRIZE_WIDE)));
            points += prizePoints;
            break;
        case Brick::PIECE:
            points += piecePoints;
            break;
        case Brick::CRACKED:
            points += doublePoints;
            break;
    }
    //a piece broken
    bricks[row][column] = uint8_t(Brick::EMPTY);
    brokenSinceLost++;
    calmTicks = 0;
    heard = Heard::BREAK;
    soundRow = row;
}

//the places the ball's box at (x, y) overlaps, all hit; how many there were
uint8_t StrikeGame::hitBricks(float x, float y){
    if(x + ballSize <= float(setLeft)) return 0;
    uint8_t hits = 0;
    for(uint8_t c = 0; c < columns; c++){
        float bx = float(brickX(c));
        if(x >= bx + brickWidth || x + ballSize <= bx) continue;
        for(uint8_t r = 0; r < rows; r++){
            float by = float(brickY(r));
            if(y >= by + brickHeight || y + ballSize <= by) continue;
            if(bricks[r][c] == uint8_t(Brick::EMPTY)) continue;
            hitBrick(r, c);
            hits++;
        }
    }
    return hits;
}

//in steps of at most 1 px on each axis, x then y: the ball never passes through a piece (4 px) or the paddle (2 px).
//A step into something does not happen, the direction on that axis turns instead (a corner: both axes in turn)
bool StrikeGame::moveBall(Ball& b, float distance){
    int steps = int(ceilf(distance));
    if(steps < 1) steps = 1;
    float step = distance / float(steps);
    for(int i = 0; i < steps; i++){
        float nx = b.x + b.dx * step;
        if(nx > float(width - ballSize)) b.dx = -fabsf(b.dx);
        else if(b.dx < 0.0f && b.x >= float(paddleFace) && nx < float(paddleFace) &&
                b.y + ballSize > paddleTop && b.y < paddleTop + float(getPaddleHeight())){
            b.x = float(paddleFace);
            bounceOffPaddle(b);
        }
        else if(hitBricks(nx, b.y) > 0) b.dx = -b.dx;
        else b.x = nx;

        float ny = b.y + b.dy * step;
        if(ny < 0.0f) b.dy = fabsf(b.dy);
        else if(ny > ballBottom) b.dy = -fabsf(b.dy);
        else if(hitBricks(b.x, ny) > 0) b.dy = -b.dy;
        else b.y = ny;

        if(b.x + ballSize < 0.0f) return false;
    }
    return true;
}

void StrikeGame::catchPrize(PowerUp kind){
    points += catchPoints;
    switch(kind){
        case PowerUp::WIDE:
            if(wideLeft == 0) paddleTop -= float(widePaddleHeight - paddleHeight) / 2.0f;
            wideLeft = wideTicks;
            paddleTop = clampf(paddleTop, 0.0f, float(fieldHeight - widePaddleHeight));
            break;
        case PowerUp::SLOW:
            slowLeft = slowTicks;
            break;
        case PowerUp::MULTI:{
            //the new balls leave from the first one in flight, spread either side of its way
            const Ball* from = nullptr;
            for(const Ball& b : balls) if(b.b_live){ from = &b; break; }
            if(!from) break;
            Ball source = *from;
            float angles[2] = {multiSpread, -multiSpread};
            uint8_t added = 0;
            for(Ball& b : balls){
                if(b.b_live || added >= 2) continue;
                float a = angles[added++];
                float c = cosf(a), s = sinf(a);
                b = source;
                b.dx = source.dx * c - source.dy * s;
                b.dy = source.dx * s + source.dy * c;
                if((b.dx < 0.0f) != (source.dx < 0.0f)) b.dx = -b.dx;
                keepHorizontal(b);
            }
            break;
        }
    }
    sound().play(catchSound());
}

void StrikeGame::ballLost(){
    if(lives > 0) lives--;
    sound().play(lostSound());
    if(lives == 0){
        phase = Phase::OVER;
        for(Prize& p : prizes) p.b_live = false;
        return;
    }
    brokenSinceLost = 0;
    if(wideLeft > 0) paddleTop += float(widePaddleHeight - paddleHeight) / 2.0f;
    wideLeft = slowLeft = 0;
    serve();
}

void StrikeGame::stageStruck(){
    points += uint32_t(stageBonus) * getStageNumber();
    phase = Phase::STRUCK;
    phaseTicks = 0;
    for(Ball& b : balls) b.b_live = false;
    for(Prize& p : prizes) p.b_live = false;
    sound().play(struckSound());
}

void StrikeGame::update(const GameInput& input){
    if(phase == Phase::OVER) return;
    frame++;
    movePaddle(input.stick[1]);

    switch(phase){
        case Phase::INTRO:
        case Phase::SERVE:
            //the ball rides on the paddle; the click launches it once the stage's name has gone
            balls[0].x = rideX;
            balls[0].y = paddleTop + float(getPaddleHeight() - ballSize) / 2.0f;
            if(phase == Phase::INTRO){
                if(++phaseTicks >= introTicks){
                    phase = Phase::SERVE;
                    phaseTicks = 0;
                }
            }
            else if(input.clicked[1]) launch();
            return;
        case Phase::STRUCK:
            if(++phaseTicks >= struckTicks){
                uint8_t next = uint8_t(stage + 1);
                if(next >= stageCount){
                    next = 0;
                    if(loop < 255) loop++;
                }
                startStageNow(next);
            }
            return;
        default: break;
    }

    //play
    heard = Heard::NONE;
    float distance = getSpeed() * tickSeconds;
    for(Ball& b : balls){
        if(!b.b_live) continue;
        if(!moveBall(b, distance)) b.b_live = false;
    }
    switch(heard){
        case Heard::BREAK: sound().playNote(SoundKit::squareNote(0, brickHertz(soundRow), blipMillis), SoundPriority::GAME); break;
        case Heard::CRACK: sound().playNote(SoundKit::squareNote(0, brickHertz(soundRow) / 2, crackMillis), SoundPriority::GAME); break;
        case Heard::THUD: sound().play(thudSound()); break;
        case Heard::PADDLE: sound().playNote(SoundKit::squareNote(0, paddleHertz, tickMillis), SoundPriority::GAME); break;
        default: break;
    }

    if(breakablesLeft() == 0){
        stageStruck();
        return;
    }
    if(getBallCount() == 0){
        ballLost();
        return;
    }

    //the power-ups drift left: caught when one touches the paddle
    float h = float(getPaddleHeight());
    for(Prize& p : prizes){
        if(!p.b_live) continue;
        p.x -= prizeSpeed * tickSeconds;
        bool b_caught = p.x < float(paddleFace) && p.x + 3.0f > float(paddleX) && p.y + 5.0f > paddleTop && p.y < paddleTop + h;
        if(b_caught){
            p.b_live = false;
            catchPrize(p.kind);
        }
        else if(p.x + 3.0f < 0.0f) p.b_live = false;
    }

    if(wideLeft > 0 && --wideLeft == 0){
        paddleTop = clampf(paddleTop + float(widePaddleHeight - paddleHeight) / 2.0f, 0.0f, float(fieldHeight - paddleHeight));
    }
    if(slowLeft > 0) slowLeft--;
    if(++calmTicks >= stallTicks){
        calmTicks = 0;
        for(Ball& b : balls) if(b.b_live) nudge(b);
    }
}

float StrikeGame::predictY(const Ball& b) const {
    if(b.dx >= 0.0f) return b.y;
    float t = (b.x - float(paddleFace)) / -b.dx;
    if(t < 0.0f) t = 0.0f;
    float y = b.y + b.dy * t;
    float period = 2.0f * ballBottom;
    y = fmodf(y, period);
    if(y < 0.0f) y += period;
    if(y > ballBottom) y = period - y;
    return y;
}

//A fair hand on the left stick: it follows the ball that will reach the paddle first and takes it at a place on the
//paddle it picks for each ball (the angles explore the set); with lapses on it looks only every few ticks and now and
//then misjudges a ball on purpose, more often the longer it has played, so the attract mode ends. Launches by itself
void StrikeGame::demo(GameInput& input){
    demoPlayedTicks++;
    float h = float(getPaddleHeight());
    float centre = paddleTop + h / 2.0f;
    float target = centre;

    if(phase == Phase::SERVE){
        if(demoServeTicks > 0) demoServeTicks--;
        else input.clicked[1] = true;
        return;
    }
    if(phase != Phase::PLAY) return;

    //the ball that reaches the paddle first; a new aim for each ball turning toward it
    int8_t next = -1;
    float soonest = 1e9f;
    for(uint8_t i = 0; i < maxBalls; i++){
        const Ball& b = balls[i];
        bool b_left = b.b_live && b.dx < 0.0f && b.x >= float(paddleFace) - 0.5f;
        bool b_turned = b_left && !b_demoHeadingLeft[i];
        b_demoHeadingLeft[i] = b_left;
        if(!b_left) continue;
        float t = (b.x - float(paddleFace)) / -b.dx;
        if(t < soonest){
            soonest = t;
            next = int8_t(i);
        }
        if(b_turned && int8_t(i) == demoBall) demoBall = -2;        //the same ball coming again: a new aim
    }
    if(next >= 0){
        if(next != demoBall){
            demoBall = next;
            float minutes = float(demoPlayedTicks) * tickSeconds / 60.0f;
            float missChance = 0.04f + 0.12f * minutes;
            float predicted = predictY(balls[next]) + float(ballSize) / 2.0f;
            if(b_demoLapses && demoRandom.unit() < missChance){
                //just off the end of the paddle, on the side it is already on (a paddle sweeping across would take it)
                float off = h / 2.0f + 3.0f + 2.0f * demoRandom.unit();
                demoAim = centre < predicted ? off : -off;
            }
            else demoAim = (demoRandom.unit() * 2.0f - 1.0f) * (h / 2.0f - 0.5f);
            demoLookTicks = 0;
        }
        if(!b_demoLapses || demoLookTicks == 0){
            demoTarget = predictY(balls[next]) + float(ballSize) / 2.0f - demoAim;
            demoLookTicks = uint8_t(demoRandom.between(2, 6));
        }
        else demoLookTicks--;
        target = demoTarget;
    }
    else{
        //nothing coming: a power-up drifting in, else the middle of the field
        demoBall = -1;
        target = float(fieldHeight) / 2.0f;
        for(const Prize& p : prizes) if(p.b_live){ target = p.y + 2.5f; break; }
    }
    float error = centre - target;
    if(fabsf(error) < 0.4f) return;
    //stick up moves the paddle up: a target above (smaller y) is a positive stick
    float stick = clampf(error * 0.3f, -1.0f, 1.0f);
    if(fabsf(stick) < stickDeadZone + 0.02f) stick = stick < 0.0f ? -(stickDeadZone + 0.02f) : stickDeadZone + 0.02f;
    input.stick[1] = stick;
}

namespace{
    void drawGlyph(Adafruit_GFX& out, int16_t x, int16_t y, uint8_t kind){
        for(int16_t r = 0; r < 5; r++){
            for(int16_t c = 0; c < 3; c++) if(glyphs[kind % 3][r] & (0b100 >> c)) out.drawPixel(x + c, y + r, GameDraw::ON);
        }
    }
}

void StrikeGame::draw(Adafruit_GFX& out) const {
    //the set
    for(uint8_t r = 0; r < rows; r++){
        for(uint8_t c = 0; c < columns; c++){
            Brick b = Brick(bricks[r][c]);
            if(b == Brick::EMPTY) continue;
            int16_t x = brickX(c), y = brickY(r);
            switch(b){
                case Brick::DOUBLE:
                    //hatched until its first hit
                    for(int16_t py = 0; py < brickHeight; py++){
                        for(int16_t px = 0; px < brickWidth; px++) if(((px + py) & 1) == 0) out.drawPixel(x + px, y + py, GameDraw::ON);
                    }
                    break;
                case Brick::TRUSS:
                    //steel: solid with a notch in each side
                    out.fillRect(x, y, brickWidth, brickHeight, GameDraw::ON);
                    out.drawPixel(x, y + 2, GameDraw::OFF);
                    out.drawPixel(x + brickWidth - 1, y + 2, GameDraw::OFF);
                    break;
                case Brick::PRIZE_WIDE: case Brick::PRIZE_MULTI: case Brick::PRIZE_SLOW:
                    //a piece with two dark slots that twinkles (solid a quarter of the time)
                    out.fillRect(x, y, brickWidth, brickHeight, GameDraw::ON);
                    if(((frame + uint32_t(r * 7 + c * 3)) / 10) % 4 != 3){
                        out.drawFastHLine(x + 1, y + 1, 2, GameDraw::OFF);
                        out.drawFastHLine(x + 1, y + 3, 2, GameDraw::OFF);
                    }
                    break;
                default:
                    out.fillRect(x, y, brickWidth, brickHeight, GameDraw::ON);
                    break;
            }
        }
    }

    out.fillRect(paddleX, int16_t(lroundf(paddleTop)), paddleWidth, getPaddleHeight(), GameDraw::ON);

    char text[16];
    bool b_cards = phase == Phase::INTRO || phase == Phase::STRUCK;
    //the score and the balls left (small balls), at the top of the gap
    int16_t hudRight = hudX;
    if(!b_cards){
        snprintf(text, sizeof(text), "%lu", (unsigned long)points);
        GameDraw::text(out, hudX, hudY, text);
        hudRight = int16_t(hudX + GameDraw::textWidth(text) + 2);
        for(uint8_t i = 0; i < lives; i++){
            out.fillRect(hudRight, hudY + 3, ballSize, ballSize, GameDraw::ON);
            hudRight = int16_t(hudRight + ballSize + 2);
        }
        //the power-ups running, their glyphs
        if(wideLeft > 0 && (wideLeft > 100 || (frame / 8) % 2 == 0)){
            drawGlyph(out, hudRight + 1, hudY + 1, 0);
            hudRight = int16_t(hudRight + 5);
        }
        if(slowLeft > 0 && (slowLeft > 100 || (frame / 8) % 2 == 0)){
            drawGlyph(out, hudRight + 1, hudY + 1, 2);
            hudRight = int16_t(hudRight + 5);
        }
    }
    //what moves over the score gets a dark margin, so it stays seen and touches nothing there
    auto overHud = [&](int16_t x, int16_t y, int16_t w, int16_t h){
        return !b_cards && x - 1 < hudRight && x + w + 1 > hudX - 1 && y - 1 < hudY + 7 && y + h + 1 > hudY - 1;
    };
    for(const Prize& p : prizes){
        if(!p.b_live) continue;
        int16_t x = int16_t(lroundf(p.x)), y = int16_t(lroundf(p.y));
        if(overHud(x, y, 3, 5)) out.fillRect(x - 1, y - 1, 5, 7, GameDraw::OFF);
        drawGlyph(out, x, y, uint8_t(p.kind));
    }
    for(const Ball& b : balls){
        if(!b.b_live || phase == Phase::STRUCK) continue;
        int16_t x = int16_t(lroundf(b.x)), y = int16_t(lroundf(b.y));
        if(overHud(x, y, ballSize, ballSize)) out.fillRect(x - 1, y - 1, ballSize + 2, ballSize + 2, GameDraw::OFF);
        out.fillRect(x, y, ballSize, ballSize, GameDraw::ON);
    }

    //the stage's cards in the gap: its number and name, then what striking it was worth
    if(phase == Phase::INTRO){
        if(loop > 0) snprintf(text, sizeof(text), "STAGE %u L%u", unsigned(stage + 1), unsigned(loop + 1));
        else snprintf(text, sizeof(text), "STAGE %u", unsigned(stage + 1));
        GameDraw::textCentered(out, gapCentre, 6, text);
        GameDraw::pillCentered(out, gapCentre, 16, stageName(stage));
    }
    else if(phase == Phase::STRUCK){
        snprintf(text, sizeof(text), "BONUS %u", unsigned(uint32_t(stageBonus) * getStageNumber()));
        GameDraw::textCentered(out, gapCentre, 6, text);
        GameDraw::pillCentered(out, gapCentre, 16, "SET STRUCK");
    }
}
