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

    //the effects with fixed notes (front layer; synth while the music plays, squares only for the game over)
    using W = Synth::Wave;
    const SoundNote::Kind SY = SoundNote::Kind::SYNTH, SQ = SoundNote::Kind::SQUARE;
    //four columns at once: A major up quick, the top held over its fifth
    const SoundNote fourNotes[] = {
        {0, SY, 0, W::SQUARE, BlocksGame::fourHertz, 50, 95, 0},
        {55, SY, 0, W::SQUARE, 1109, 50, 95, 0},
        {110, SY, 0, W::SQUARE, 1319, 50, 95, 0},
        {165, SY, 0, W::SQUARE, 1760, 170, 100, 90},
        {165, SY, 1, W::PULSE25, 1319, 170, 80, 90},
    };
    //level up: E6 A6 B6 E7, the last one ringing
    const SoundNote levelNotes[] = {
        {0, SY, 0, W::PULSE25, BlocksGame::levelHertz, 60, 90, 0},
        {70, SY, 0, W::PULSE25, 1760, 60, 90, 0},
        {140, SY, 0, W::PULSE25, 1976, 60, 90, 0},
        {210, SY, 0, W::PULSE25, 2637, 160, 95, 80},
    };
    //game over: G5 E5 C5 A4 falling, squares (the music has stopped), over before the Arcade's phrase at 400 ms
    const SoundNote overNotes[] = {
        {0, SQ, 0, W::SQUARE, BlocksGame::overHertz, 70, 0, 0},
        {80, SQ, 0, W::SQUARE, 659, 70, 0, 0},
        {160, SQ, 0, W::SQUARE, 523, 70, 0, 0},
        {240, SQ, 0, W::SQUARE, 440, 130, 0, 0},
    };

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
    if(!fits(piece)){
        b_over = true;
        //the music stops, a falling phrase
        sound().stop(SoundLayer::BACK);
        sound().play(soundCue(overNotes, SoundPriority::GAME));
    }
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
    playLanding(full);
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
    uint8_t before = level();
    cleared = uint16_t(cleared + removed);
    if(level() != before){
        //a jingle in front; the music faster, and from level 3 with its drums (from the top, under the jingle)
        sound().play(soundCue(levelNotes, SoundPriority::GAME));
        sound().setTempo(SoundLayer::BACK, musicTempo(level()));
        if(before < drumsFromLevel && level() >= drumsFromLevel) startTune();
    }
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

    bool b_turned = false;
    if(input.clicked[1] || b_flickUp) b_turned = tryRotate(1);
    else if(b_flickDown) b_turned = tryRotate(-1);
    //a blip for a turn, a tick for a step across; both give way to an effect still sounding
    if((b_turned || b_moved) && !sound().isPlaying(SoundLayer::FRONT)){
        SoundNote note = b_turned ? SoundKit::synthNote(0, rotateHertz, 30, 80, 15)
                                  : SoundKit::synthNote(0, moveHertz, 10, 60, 5);
        sound().playNote(note, SoundPriority::GAME);
    }
    b_moved |= b_turned;

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

    //HUD right of the entry line, its text sideways to read in the sideways grip (game up = screen right) [D-157]:
    //the score at the right edge (the top of the HUD in that grip), the next piece in the middle as it will enter, the
    //level next to the entry line; each centred across the screen
    char text[12];
    if(points < 100000) snprintf(text, sizeof(text), "%lu", (unsigned long)points);
    else snprintf(text, sizeof(text), "%luK", (unsigned long)(points / 1000));     //5 characters fit across
    GameDraw::textSidewaysCentered(out, width - 1, height / 2, text);
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
    GameDraw::textSidewaysCentered(out, hudX + 6, height / 2, text);
}

uint16_t BlocksGame::musicTempo(uint8_t level){
    //6 % faster a level, at most 160 %
    uint16_t percent = uint16_t(100 + 6 * (level > 1 ? level - 1 : 0));
    return percent < 160 ? percent : 160;
}

//Korobeiniki, the 19th-century folk song (public domain), in A minor, as our own full arrangement: the folk
//melody's two parts (the verse and the refrain), our own bridge in the relative major, the two parts once more and
//a turnaround on E7 back to the top. An eighth a step (150 quarters a minute at tempo 100), 34 bars of 4/4:
//
//  section     bars  melody                          chords (a bar, or half bars)        bass
//  A  verse    1-8   part one, twice                  Am E | Am | E | Am  (twice)        roots and fifths, octave jumps
//  B  refrain  9-16  part two, twice                  Dm | C | E | Am  (twice)           arpeggios, walking octaves
//  C  bridge  17-24  ours, in C major                 C | G | Am | Em | F | C | Dm G | E7  half time, passing notes
//  A' B'      25-32  part one, part two               as A and B                         walking octaves
//  T  turn    33-34  an E7 arpeggio falling to B      E | E7                             E D C B down to the top's A
//
//The bass, the bridge, the turnaround and the drums are ours (no other arrangement is used). The drums (from
//level 3): a march, kick on 1, snare on 3, hats between, a pickup kick before 3; the bridge in half time (one
//snare a bar); a snare fill into each new section.
//Written for the disc (loud only from about 1 kHz to 5 kHz, nearly silent below 400 Hz): the melody an octave above
//the song's usual one (A5 to B6, 880 to 1976 Hz) on a full square with a slow decay, so long notes swell down
//instead of droning; the bass from E4 (330 Hz) up on a narrow pulse, plucked short and quieter, never lower than
//the disc can play; the three voices at levels whose sum rarely reaches the synth's clip
void BlocksGame::buildTune(){
    using namespace SoundKit;
    //Arduino owns A0 to A17 (the analog pins): the A notes under other names
    const uint8_t a4 = SoundKit::A4, a5 = SoundKit::A5, a6 = SoundKit::A6;
    const uint8_t H = HOLD, R = REST;
    //the melody, an entry an eighth (HOLD: the note before goes on)
    static const uint8_t partOne[32] PROGMEM = {
        E6, H, B5, C6,  D6, H, C6, B5,      a5, H, a5, C6,  E6, H, D6, C6,
        B5, H, H, C6,   D6, H, E6, H,       C6, H, a5, H,   a5, H, H, H,
    };
    static const uint8_t partTwo[32] PROGMEM = {
        D6, H, H, F6,   a6, H, G6, F6,      E6, H, H, C6,   E6, H, D6, C6,
        B5, H, B5, C6,  D6, H, E6, H,       C6, H, a5, H,   a5, H, H, H,
    };
    //the bridge: two four-bar phrases that rhyme (a rising figure, its answer), the second a third higher
    static const uint8_t bridge[64] PROGMEM = {
        G6, H, E6, F6,  G6, H, E6, C6,      D6, H, B5, C6,  D6, H, G6, H,
        E6, H, C6, D6,  E6, F6, E6, D6,     E6, H, D6, C6,  B5, H, H, H,
        a6, H, F6, G6,  a6, H, G6, F6,      E6, H, G6, E6,  C6, H, H, H,
        D6, H, F6, H,   G6, H, F6, D6,      B5, H, D6, H,   E6, H, Gs6, H,
    };
    static const uint8_t turnaround[16] PROGMEM = {
        B5, E6, Gs6, E6, B5, E6, Gs6, B6,   Gs6, H, E6, H,  D6, C6, B5, H,
    };
    //the bass, an entry an eighth; each ending leads to the next section's first chord
    static const uint8_t bassVerse[32] PROGMEM = {      //back to the verse: G# up to A
        a4, H, E5, H,   E4, H, B4, H,       a4, H, E5, H,   a5, H, E5, H,
        E4, H, B4, H,   E5, H, B4, H,       a4, H, E5, H,   a4, H, Gs4, H,
    };
    static const uint8_t bassVerseOut[32] PROGMEM = {   //on to the refrain: C C# up to D
        a4, H, E5, H,   E4, H, B4, H,       a4, H, E5, H,   a5, H, E5, H,
        E4, H, B4, H,   E5, H, B4, H,       a4, H, E5, H,   a4, H, C5, Cs5,
    };
    static const uint8_t bassRefrain[32] PROGMEM = {    //back to the refrain
        D5, a4, D5, F5, a5, F5, D5, a4,     C5, G4, C5, E5, G5, E5, C5, G4,
        E4, E5, E4, E5, E4, E5, Gs4, B4,    a4, a5, a4, a5, a4, H, C5, Cs5,
    };
    static const uint8_t bassRefrainOut[32] PROGMEM = { //on to the bridge: A B up to C
        D5, a4, D5, F5, a5, F5, D5, a4,     C5, G4, C5, E5, G5, E5, C5, G4,
        E4, E5, E4, E5, E4, E5, Gs4, B4,    a4, a5, a4, a5, a4, H, B4, H,
    };
    static const uint8_t bassBridge[64] PROGMEM = {     //half time: the root on 1, the fifth on 3, a passing note on 4
        C5, H, R, R,    G4, H, a4, B4,      G4, H, R, R,    D5, H, B4, H,
        a4, H, R, R,    E5, H, C5, H,       E4, H, R, R,    B4, H, E4, H,
        F4, H, R, R,    C5, H, a4, H,       C5, H, R, R,    G4, H, C5, H,
        D5, H, a4, H,   G4, H, D5, H,       E4, H, Gs4, H,  B4, H, D5, H,
    };
    static const uint8_t bassVerseOctaves[32] PROGMEM = {   //the verse again, walking octaves, on to the refrain
        a4, a5, a4, a5, E4, E5, E4, E5,     a4, a5, a4, a5, a4, E5, a5, E5,
        E4, E5, E4, E5, E4, E5, Gs4, B4,    a4, a5, a4, a5, a4, H, C5, Cs5,
    };
    static const uint8_t bassRefrainLast[32] PROGMEM = {    //the refrain again, on to the turnaround's E
        D5, a4, D5, F5, a5, F5, D5, a4,     C5, G4, C5, E5, G5, E5, C5, G4,
        E4, E5, E4, E5, E4, E5, Gs4, B4,    a4, a5, a4, a5, a4, E5, C5, B4,
    };
    static const uint8_t bassTurnaround[16] PROGMEM = {     //E, then down E D C B to the top's A
        E4, H, E5, H,   Gs4, H, B4, H,      E5, H, D5, H,   C5, H, B4, H,
    };
    //the drums, a character a step: K kick, S snare, H hat
    static const char groove[] = "K.H.S.H.K.HKS.H.K.H.S.H.K.HKS.H.";
    static const char grooveFill[] = "K.H.S.H.K.HKS.H.K.H.S.H.K.S.SSS.";
    static const char halfTime[] = "K.H.H.H.S.H.H.H.K.H.H.H.S.H.H.H.K.H.H.H.S.H.H.H.K.H.H.H.S.H.S.SS";
    static const char turnFill[] = "K.H.S.H.K.S.SSSS";
    struct Section{ const uint8_t* melody; const uint8_t* bass; uint8_t steps; const char* drums; };
    static const Section form[] = {
        {partOne, bassVerse, 32, groove},           {partOne, bassVerseOut, 32, grooveFill},
        {partTwo, bassRefrain, 32, groove},         {partTwo, bassRefrainOut, 32, grooveFill},
        {bridge, bassBridge, 64, halfTime},
        {partOne, bassVerseOctaves, 32, groove},    {partTwo, bassRefrainLast, 32, grooveFill},
        {turnaround, bassTurnaround, 16, turnFill},
    };
    for(uint8_t withDrums = 0; withDrums < 2; withDrums++){
        NoteWriter& out = withDrums ? static_cast<NoteWriter&>(tuneDrums) : static_cast<NoteWriter&>(tune);
        out.clear();
        Tracker t(out, tuneStepMillis);
        //the melody nearly legato on a full square, decaying slowly (a held note falls, it does not drone); the bass
        //plucked: an eighth sounds 60 ms, a quarter 120, on a narrow pulse (bright enough for the disc, never muddy)
        t.instrument(0, Synth::Wave::SQUARE, 86, 650).instrument(1, Synth::Wave::PULSE12, 30, 70);
        uint16_t at = 0;
        for(const Section& s : form){
            t.line(0, s.melody, s.steps, at, 1, melodyLevel).line(1, s.bass, s.steps, at, 1, bassLevel);
            if(withDrums) t.drums(s.drums, at, 1, drumsLevel);
            at = uint16_t(at + s.steps);
        }
        music[withDrums] = t.cue(tuneSteps);
    }
    b_tuneBuilt = true;
}

const SoundCue& BlocksGame::tuneCue(bool b_withDrums){
    if(!b_tuneBuilt) buildTune();
    return music[b_withDrums ? 1 : 0];
}

//the tune for the level from its start, at the level's tempo
void BlocksGame::startTune(){
    if(!b_tuneBuilt) buildTune();
    sound().setTempo(SoundLayer::BACK, musicTempo(level()));
    sound().play(music[level() >= drumsFromLevel ? 1 : 0], SoundLayer::BACK);
}

void BlocksGame::playMusic(){
    if(!b_over) startTune();
}

//a piece locked: a low thud, or for cleared columns a rising sweep (higher and longer for more), a fanfare for four
void BlocksGame::playLanding(uint8_t columns){
    if(columns >= 4){
        sound().play(soundCue(fourNotes, SoundPriority::GAME));
        return;
    }
    effect.clear();
    if(columns == 0){
        effect.sweep(0, landHertz, 110, 60, 4, Synth::Wave::SQUARE, 100, 0, 30);
        effect.burst(0, 1200, 30, 60, 12, 1);
    }
    else{
        effect.sweep(0, clearHertz, uint16_t(1000 + 500 * columns), uint16_t(110 + 40 * columns), 10,
                     Synth::Wave::PULSE25, 90, 0, 40);
    }
    sound().play(effect.cue());
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
