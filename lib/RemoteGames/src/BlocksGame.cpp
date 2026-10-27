#include "BlocksGame.h"
#include <stdio.h>

//The well in cells: a = along (0 at the floor on the left, rising toward the entry on the right), b = across (0 at the
//top of the screen). On screen a cell is a 2 x 2 dot at x = 2 + 3a, y = 1 + 3b: the 1 px gaps keep the pieces readable
//and leave x 1 dark beside the floor line. The well fills the screen's height, so its sides are the screen's edges
//(walls at y 0 and 31 would touch the cells). Beyond the entry line at x 86 the HUD shows the score, the next piece
//(where it will come from) and the level; the left end stays clear for the stack, where the eyes are.

namespace{

    const int16_t floorX = 0;
    const int16_t wellX = 2;
    const int16_t wellY = 1;
    const int16_t entryX = wellX + BlocksGame::wellLength * BlocksGame::cellPitch;  //the dotted entry line
    const int16_t hudX = entryX + 2;

    //the seven pieces as the classic game shows them (rows from the top, floor below), in their box of 2, 3 or 4
    struct Shape{ uint8_t box; uint8_t rows[4]; };
    const Shape shapes[7] = {
        {4, {0x0, 0xF, 0x0, 0x0}},     //I
        {3, {0x4, 0x7, 0x0, 0x0}},     //J
        {3, {0x1, 0x7, 0x0, 0x0}},     //L
        {2, {0x3, 0x3, 0x0, 0x0}},     //O
        {3, {0x3, 0x6, 0x0, 0x0}},     //S
        {3, {0x2, 0x7, 0x0, 0x0}},     //T
        {3, {0x6, 0x3, 0x0, 0x0}},     //Z
    };

    const uint16_t clearPoints[5] = {0, 40, 100, 300, 1200};

    //stick handling: a deflection past half moves once, held it repeats after 0.2 s every 60 ms
    const float stickOn = 0.5f;
    const uint8_t repeatDelayTicks = 10;
    const uint8_t repeatTicks = 3;
    //the right stick rotates on a flick past 0.6 and re-arms near the centre, so holding it does not spin the piece
    const float flickOn = 0.6f;
    const float flickRearm = 0.3f;
    //a piece resting on the stack locks after 0.5 s; moving or turning it restarts that, at most 15 times
    const uint8_t lockTicks = 25;
    const uint8_t maxLockResets = 15;
    const float softDropSeconds = 0.04f;
    //a basic wall kick: in place, one cell across either way, two for the long piece, then one toward the floor (a
    //piece turned at the entry) or one away from it (a piece turned on the stack)
    const int8_t kicks[7][2] = {{0, 0}, {0, 1}, {0, -1}, {0, 2}, {0, -2}, {-1, 0}, {1, 0}};

    void drawCell(Adafruit_GFX& out, int16_t a, int16_t b){
        out.fillRect(wellX + a * BlocksGame::cellPitch, wellY + b * BlocksGame::cellPitch, 2, 2, GameDraw::ON);
    }
};

void BlocksGame::cellsOf(uint8_t type, uint8_t rotation, Cell out[4]){
    const Shape& shape = shapes[type];
    int8_t n = int8_t(shape.box);
    uint8_t count = 0;
    for(int8_t row = 0; row < n; row++){
        for(int8_t col = 0; col < n; col++){
            if(!(shape.rows[row] & (1 << (n - 1 - col)))) continue;
            //the classic picture's top row lies farthest from the floor (right), its left column at the screen's top
            int8_t a = int8_t(n - 1 - row);
            int8_t b = col;
            //clockwise on the screen (y down): (a, b) -> (n - 1 - b, a)
            for(uint8_t r = 0; r < (rotation & 3); r++){
                int8_t turned = int8_t(n - 1 - b);
                b = a;
                a = turned;
            }
            out[count].a = a;
            out[count].b = b;
            count++;
        }
    }
}

void BlocksGame::reset(uint32_t seed){
    random.seed(seed);
    memset(board, 0, sizeof(board));
    memset(clearing, 0, sizeof(clearing));
    points = 0;
    cleared = 0;
    placed = 0;
    b_over = false;
    clearTimer = 0;
    moveDirection = 0;
    moveTicks = 0;
    b_flickArmed = true;
    plannedPiece = 0xFFFFFFFFu;
    //two shuffled bags of the seven pieces
    for(uint8_t half = 0; half < 2; half++){
        for(uint8_t i = 0; i < 7; i++) bag[half * 7 + i] = i;
        for(uint8_t i = 6; i > 0; i--){
            uint8_t j = uint8_t(random.below(i + 1));
            uint8_t swap = bag[half * 7 + i];
            bag[half * 7 + i] = bag[half * 7 + j];
            bag[half * 7 + j] = swap;
        }
    }
    bagIndex = 0;
    spawn();
}

uint8_t BlocksGame::takeFromBag(){
    uint8_t type = bag[bagIndex++];
    if(bagIndex == 7){
        //the next bag becomes the current one and a fresh one is shuffled behind it
        for(uint8_t i = 0; i < 7; i++) bag[i] = bag[i + 7];
        for(uint8_t i = 0; i < 7; i++) bag[7 + i] = i;
        for(uint8_t i = 6; i > 0; i--){
            uint8_t j = uint8_t(random.below(i + 1));
            uint8_t swap = bag[7 + i];
            bag[7 + i] = bag[7 + j];
            bag[7 + j] = swap;
        }
        bagIndex = 0;
    }
    return type;
}

void BlocksGame::spawn(){
    piece.type = takeFromBag();
    piece.rotation = 0;
    uint8_t box = shapes[piece.type].box;
    piece.b = int8_t((wellWidth - box) / 2);
    //the box's far side against the entry line: the piece's farthest cells in the last column
    Cell cells[4];
    cellsOf(piece.type, 0, cells);
    int8_t farthest = 0;
    for(uint8_t i = 0; i < 4; i++) if(cells[i].a > farthest) farthest = cells[i].a;
    piece.a = int8_t(wellLength - 1 - farthest);
    fallTimer = 0.0f;
    lockTimer = 0;
    lockResets = 0;
    if(!fits(piece)) b_over = true;
}

bool BlocksGame::fits(const Piece& p) const {
    Cell cells[4];
    cellsOf(p.type, p.rotation, cells);
    for(uint8_t i = 0; i < 4; i++){
        int8_t a = int8_t(p.a + cells[i].a);
        int8_t b = int8_t(p.b + cells[i].b);
        if(a < 0 || a >= wellLength || b < 0 || b >= wellWidth) return false;
        if(board[a][b]) return false;
    }
    return true;
}

bool BlocksGame::tryMove(int8_t da, int8_t db){
    Piece moved = piece;
    moved.a = int8_t(moved.a + da);
    moved.b = int8_t(moved.b + db);
    if(!fits(moved)) return false;
    piece = moved;
    return true;
}

bool BlocksGame::tryRotate(int8_t turn){
    Piece turned = piece;
    turned.rotation = uint8_t((piece.rotation + 4 + turn) & 3);
    for(uint8_t i = 0; i < 7; i++){
        if((i == 3 || i == 4) && piece.type != 0) continue;
        Piece kicked = turned;
        kicked.a = int8_t(turned.a + kicks[i][0]);
        kicked.b = int8_t(turned.b + kicks[i][1]);
        if(fits(kicked)){
            piece = kicked;
            return true;
        }
    }
    return false;
}

int8_t BlocksGame::dropDistance(const Piece& p) const {
    Piece probe = p;
    int8_t distance = 0;
    while(true){
        probe.a--;
        if(!fits(probe)) return distance;
        distance++;
    }
}

float BlocksGame::gravitySeconds() const {
    //0.7 s per cell at level 1, about 17 % faster each level, never faster than 50 ms
    float seconds = 0.7f;
    for(uint8_t l = 1; l < level(); l++) seconds *= 0.83f;
    return seconds < 0.05f ? 0.05f : seconds;
}

void BlocksGame::lockPiece(){
    Cell cells[4];
    cellsOf(piece.type, piece.rotation, cells);
    for(uint8_t i = 0; i < 4; i++) board[piece.a + cells[i].a][piece.b + cells[i].b] = 1;
    placed++;
    uint8_t full = 0;
    for(int8_t a = 0; a < wellLength; a++){
        bool b_full = true;
        for(int8_t b = 0; b < wellWidth && b_full; b++) if(!board[a][b]) b_full = false;
        clearing[a] = b_full;
        if(b_full) full++;
    }
    if(full){
        points += uint32_t(clearPoints[full]) * level();
        clearTimer = clearTicks;     //the wipe plays, then collapse() and the next piece
        return;
    }
    spawn();
}

void BlocksGame::collapse(){
    uint8_t removed = 0;
    int8_t to = 0;
    for(int8_t from = 0; from < wellLength; from++){
        if(clearing[from]){
            removed++;
            continue;
        }
        if(to != from) memcpy(board[to], board[from], wellWidth);
        to++;
    }
    for(; to < wellLength; to++) memset(board[to], 0, wellWidth);
    memset(clearing, 0, sizeof(clearing));
    cleared = uint16_t(cleared + removed);
}

void BlocksGame::update(const GameInput& input){
    if(b_over) return;

    //the right stick's flick re-arms near the centre even while a wipe plays
    float flick = input.stick[3];
    bool b_flickUp = false, b_flickDown = false;
    if(b_flickArmed){
        if(flick > flickOn){ b_flickUp = true; b_flickArmed = false; }
        else if(flick < -flickOn){ b_flickDown = true; b_flickArmed = false; }
    }else if(flick < flickRearm && flick > -flickRearm){
        b_flickArmed = true;
    }

    int8_t direction = 0;
    if(input.stick[1] > stickOn) direction = -1;          //stick up: toward the top of the screen
    else if(input.stick[1] < -stickOn) direction = 1;

    if(clearTimer){
        clearTimer--;
        if(!clearTimer){
            collapse();
            spawn();
            //a stick still held carries on with the new piece after the repeat delay, not at once
            moveDirection = direction;
            moveTicks = 0;
        }
        return;
    }

    bool b_moved = false;
    if(direction != moveDirection){
        moveDirection = direction;
        moveTicks = 0;
        if(direction) b_moved = tryMove(0, direction);
    }else if(direction){
        moveTicks++;
        if(moveTicks >= repeatDelayTicks && (moveTicks - repeatDelayTicks) % repeatTicks == 0) b_moved = tryMove(0, direction);
        if(moveTicks > 200) moveTicks = repeatDelayTicks;
    }

    if(input.clicked[1] || b_flickUp) b_moved |= tryRotate(1);
    else if(b_flickDown) b_moved |= tryRotate(-1);

    if(input.clicked[0]){
        piece.a = int8_t(piece.a - dropDistance(piece));
        lockPiece();
        return;
    }

    if(b_moved && lockTimer && lockResets < maxLockResets){
        lockTimer = 0;
        lockResets++;
    }

    float interval = gravitySeconds();
    if(input.stick[0] < -stickOn && interval > softDropSeconds) interval = softDropSeconds;
    fallTimer += tickSeconds;
    if(fallTimer >= interval - 0.001f){     //a sum of 0.02 s steps falls just short in float
        fallTimer = 0.0f;
        if(tryMove(-1, 0)) lockTimer = 0;
    }
    if(dropDistance(piece) == 0){
        lockTimer++;
        if(lockTimer >= lockTicks) lockPiece();
    }
}

void BlocksGame::pieceSpan(int8_t& lowest, int8_t& highest) const {
    Cell cells[4];
    cellsOf(piece.type, piece.rotation, cells);
    lowest = wellWidth;
    highest = -1;
    for(uint8_t i = 0; i < 4; i++){
        int8_t b = int8_t(piece.b + cells[i].b);
        if(b < lowest) lowest = b;
        if(b > highest) highest = b;
    }
}

void BlocksGame::draw(Adafruit_GFX& out) const {
    out.drawFastVLine(floorX, 0, height, GameDraw::ON);
    for(int16_t y = 0; y < height; y += 2) out.drawPixel(entryX, y, GameDraw::ON);

    //the stack; columns being cleared first show as solid bars, then shrink to the middle
    for(int8_t a = 0; a < wellLength; a++){
        if(clearTimer && clearing[a]){
            int16_t x = wellX + a * cellPitch;
            const int16_t solid = 6;
            int16_t full = wellWidth * cellPitch - 1;
            int16_t length = full;
            if(clearTimer < clearTicks - solid) length = int16_t(full * clearTimer / (clearTicks - solid));
            if(length > 0) out.fillRect(x, wellY + (full - length) / 2, 2, length, GameDraw::ON);
            continue;
        }
        for(int8_t b = 0; b < wellWidth; b++) if(board[a][b]) drawCell(out, a, b);
    }

    if(!clearTimer && !b_over){
        Cell cells[4];
        cellsOf(piece.type, piece.rotation, cells);
        //where it will land: one dot per cell, so the aim reads without looking like a placed piece
        int8_t drop = dropDistance(piece);
        if(drop > 0){
            for(uint8_t i = 0; i < 4; i++){
                int16_t a = piece.a - drop + cells[i].a;
                int16_t b = piece.b + cells[i].b;
                out.drawPixel(wellX + a * cellPitch, wellY + b * cellPitch, GameDraw::ON);
            }
        }
        for(uint8_t i = 0; i < 4; i++) drawCell(out, piece.a + cells[i].a, piece.b + cells[i].b);
    }

    //HUD: the score at the top right, the next piece in the middle as it will enter, the level at the bottom
    char text[12];
    snprintf(text, sizeof(text), "%lu", (unsigned long)points);
    GameDraw::textRight(out, width - 1, 0, text);
    Cell next[4];
    uint8_t nextPiece = bag[bagIndex];
    cellsOf(nextPiece, 0, next);
    int8_t lowA = 4, highA = -1, lowB = 4, highB = -1;
    for(uint8_t i = 0; i < 4; i++){
        if(next[i].a < lowA) lowA = next[i].a;
        if(next[i].a > highA) highA = next[i].a;
        if(next[i].b < lowB) lowB = next[i].b;
        if(next[i].b > highB) highB = next[i].b;
    }
    int16_t spanX = int16_t((highA - lowA) * cellPitch + 2);
    int16_t spanY = int16_t((highB - lowB) * cellPitch + 2);
    int16_t originX = int16_t(108 - spanX / 2);
    int16_t originY = int16_t(16 - spanY / 2);
    for(uint8_t i = 0; i < 4; i++){
        out.fillRect(originX + (next[i].a - lowA) * cellPitch, originY + (next[i].b - lowB) * cellPitch, 2, 2,
                     GameDraw::ON);
    }
    snprintf(text, sizeof(text), "L%u", (unsigned)level());
    GameDraw::textRight(out, width - 1, height - 7, text);
}

float BlocksGame::evaluate(const Piece& p) const {
    //the well after this placement, scored the usual way: low, flat, without holes, with columns cleared
    uint8_t trial[wellLength][wellWidth];
    memcpy(trial, board, sizeof(trial));
    Cell cells[4];
    cellsOf(p.type, p.rotation, cells);
    for(uint8_t i = 0; i < 4; i++) trial[p.a + cells[i].a][p.b + cells[i].b] = 1;
    uint8_t full = 0;
    for(int8_t a = 0; a < wellLength; a++){
        bool b_full = true;
        for(int8_t b = 0; b < wellWidth && b_full; b++) if(!trial[a][b]) b_full = false;
        if(b_full) full++;
    }
    int16_t heights[wellWidth];
    int16_t total = 0, holes = 0, bumps = 0, highest = 0;
    for(int8_t b = 0; b < wellWidth; b++){
        int8_t h = 0;
        for(int8_t a = wellLength - 1; a >= 0; a--) if(trial[a][b]){ h = int8_t(a + 1); break; }
        for(int8_t a = 0; a < h; a++) if(!trial[a][b]) holes++;
        heights[b] = h;
        total += h;
        if(h > highest) highest = h;
        if(b) bumps += heights[b] > heights[b - 1] ? heights[b] - heights[b - 1] : heights[b - 1] - heights[b];
    }
    return -0.51f * total + 0.76f * full - 0.36f * holes - 0.18f * bumps - 0.1f * highest;
}

void BlocksGame::plan(){
    //every rotation at every place across, dropped straight from the entry; the best one, but not always: the higher
    //the level, the more often it settles for one of the next few, which is how it ends up topping out
    struct Option{ float value; uint8_t rotation; int8_t b; };
    Option options[40];
    uint8_t count = 0;
    for(uint8_t r = 0; r < 4; r++){
        if(piece.type == 3 && r) break;                 //the square looks the same every way
        if((piece.type == 0 || piece.type == 4 || piece.type == 6) && r > 1) break;
        for(int8_t b = -3; b < wellWidth; b++){
            Piece p = piece;
            p.rotation = r;
            p.b = b;
            //turned at the entry, the piece may have to come one cell nearer the floor to fit
            if(!fits(p)) p.a--;
            if(!fits(p)) continue;
            p.a = int8_t(p.a - dropDistance(p));
            Option o = {evaluate(p), r, b};
            //sorted, best first
            uint8_t at = count;
            while(at > 0 && options[at - 1].value < o.value){
                if(at < 40) options[at] = options[at - 1];
                at--;
            }
            if(at < 40) options[at] = o;
            if(count < 40) count++;
        }
    }
    plannedPiece = placed;
    demoTicks = 0;
    demoWait = uint8_t(random.between(6, 14));         //a moment to look at the piece
    if(!count){
        targetRotation = piece.rotation;
        targetAcross = piece.b;
        return;
    }
    uint8_t pick = 0;
    float sloppy = 0.15f + 0.18f * float(level() - 1);
    if(sloppy > 0.8f) sloppy = 0.8f;
    if(count > 1 && random.unit() < sloppy) pick = uint8_t(random.between(1, count < 9 ? count - 1 : 8));
    targetRotation = options[pick].rotation;
    targetAcross = options[pick].b;
}

void BlocksGame::demo(GameInput& input){
    if(b_over || clearTimer) return;
    if(plannedPiece != placed) plan();
    demoTicks++;
    if(demoWait){
        demoWait--;
        return;
    }
    if(demoRotateWait) demoRotateWait--;
    if(piece.rotation != targetRotation && demoTicks < 150){
        //one turn at a time, a click for clockwise, a flick down for one turn back
        if(!demoRotateWait){
            if(((piece.rotation + 3) & 3) == targetRotation) input.stick[3] = -1.0f;
            else{
                input.clicked[1] = true;
                input.held[1] = true;
            }
            demoRotateWait = 6;
        }
        return;
    }
    if(piece.b != targetAcross && demoTicks < 150){
        input.stick[1] = piece.b > targetAcross ? 1.0f : -1.0f;
        return;
    }
    //in place: drop it (now and then let it slide down instead)
    if((placed & 3) == 3 && demoTicks < 150){
        input.stick[0] = -1.0f;
        return;
    }
    input.clicked[0] = true;
    input.held[0] = true;
}
