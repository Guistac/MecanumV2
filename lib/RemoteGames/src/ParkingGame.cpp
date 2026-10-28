#include "ParkingGame.h"
#include <math.h>
#include <stdio.h>

namespace{

    const float PI_F = 3.14159265f;
    const float DEG = PI_F / 180.0f;
    const float halfLength = 5.5f;      //the vehicle is 11 x 7 px
    const float halfWidth = 3.5f;
    const float acceleration = 100.0f;  //px/s2: full speed in 0.3 s, like the real vehicle's smoothed ramp
    const float turnAcceleration = 8.0f;    //rad/s2: full turn rate in about 0.26 s
    //the contact margin around a piece, from its outer pixel centres: the vehicle's rounded drawing may touch a piece
    //but never overlaps it. A turning piece needs more, both drawings are rounded at any angle
    const float margin = 0.6f;
    const float turnMargin = 1.5f;
    const uint16_t bumpCooldownTicks = 25;  //a hit into a corner costs once, not once per face

    struct Box{ int16_t x, y, w, h; };
    //a fixed piece of the set: a box, turned by angleDeg (clockwise on the screen) about its centre
    struct Piece{ int16_t x, y, w, h; int16_t angleDeg; };

    //SLIDE: a flat on a track, from box to box + (dx, dy) and back, resting at both ends. WAGON: crosses the stage from
    //box to box + (dx, dy), both off the stage, then waits off stage. TURN: a bar on a turntable, box at 0 degrees,
    //half a turn clockwise in moveTicks (counter-clockwise when dx < 0)
    enum class Kind : uint8_t { SLIDE, WAGON, TURN };

    struct Mover{
        Kind kind;
        Box box;
        int16_t dx, dy;
        uint16_t moveTicks;
        uint16_t holdTicks;
    };

    //the demo player's path; a gated point waits until the mover (1 based) stays out of the zone for leadTicks. The
    //point before a gated one is a stop, reached within 1 px; S (no mover) makes the point before a stop only, for a
    //tight corner
    const uint8_t S = 0xFF;
    struct Waypoint{
        float x, y, headingDeg;
        uint8_t gate;
        Box zone;
        uint16_t leadTicks;
    };

    //the time and the damage meter under it are shown in the top right corner, which every stage keeps free: it is a
    //wall like the set pieces
    const Box booth = {95, 0, 33, 12};

    struct Level{
        float startX, startY, startHeadingDeg;
        //the painted outline; the vehicle's corners must stay on or inside it. A slanted bay (a heading that is not a
        //multiple of 90): the box's centre is the bay's, w its length along the heading, h its width across
        Box bay;
        int16_t bayHeadingDeg;      //0 right, 90 down, 180 left, -90 up: the marked (front) side
        uint8_t pieceCount;
        Piece pieces[ParkingGame::maxPieces];
        uint8_t moverCount;
        Mover movers[ParkingGame::maxMovers];
        uint8_t waypointCount;      //the last one is the bay
        Waypoint waypoints[ParkingGame::maxWaypoints];
    };

    //in flash (PROGMEM, Teensy 4 reads its flash directly): 11 KB less in RAM1
    const Level levels[ParkingGame::levelCount] PROGMEM = {
        //1: straight ahead
        {14, 20, 0, {68, 15, 17, 11}, 0,
            1, {{36, 3, 20, 3}},
            0, {},
            1, {{76, 20, 0}}},
        //2: a quarter turn around a flat into a bay facing up
        {14, 23, 0, {72, 4, 11, 17}, -90,
            1, {{40, 0, 22, 14}},
            0, {},
            3, {{52, 24, 0}, {77, 24, -90}, {77, 12, -90}}},
        //3: strafe down into the gap between two set pieces
        {14, 8, 0, {55, 19, 15, 12}, 0,
            2, {{40, 17, 14, 15}, {71, 17, 14, 15}},
            0, {},
            2, {{62, 8, 0}, {62, 25, 0}}},
        //4: reverse into the garage, nose out
        {14, 16, 0, {76, 18, 16, 12}, 180,
            2, {{72, 13, 25, 3}, {95, 15, 3, 17}},
            0, {},
            2, {{56, 24, 180}, {84, 24, 180}}},
        //5: the first moving flat: wait until it opens the way below
        {14, 16, 0, {106, 14, 16, 12}, 0,
            0, {},
            1, {{Kind::SLIDE, {58, 0, 4, 20}, 0, 12, 75, 75}},
            3, {{44, 25.5f, 0}, {78, 25.5f, 0, 1, {52, 19, 16, 13}, 75}, {113.5f, 19.5f, 0}}},
        //6: slalom, the Mecanum way: strafe between the pieces
        {12, 25, 0, {101, 17, 16, 12}, 0,
            3, {{28, 0, 6, 19}, {52, 13, 6, 19}, {76, 0, 6, 19}},
            0, {},
            6, {{42, 25, 0}, {43, 6, 0}, {66, 6, 0}, {67, 25, 0}, {88, 25, 0}, {109, 23, 0}}},
        //7: two wagons cross the stage, one down, one up
        {12, 16, 0, {104, 16, 16, 12}, 0,
            0, {},
            2, {{Kind::WAGON, {40, -14, 10, 12}, 0, 48, 170, 60}, {Kind::WAGON, {76, 34, 10, 12}, 0, -48, 160, 100}},
            4, {{28, 16, 0}, {62, 16, 0, 1, {38, 8, 14, 16}, 70}, {98, 21.5f, 0, 2, {74, 8, 14, 16}, 70},
                {111.5f, 21.5f, 0}}},
        //8: tight: over the piano, a quarter turn under the flat, down into a narrow pocket
        {12, 6, 0, {61, 17, 11, 15}, 90,
            4, {{28, 13, 16, 9}, {62, 0, 12, 2}, {54, 18, 6, 14}, {73, 18, 6, 14}},
            0, {},
            4, {{46, 7.5f, 0}, {51, 8, 90}, {66, 9, 90}, {66, 25, 90}}},
        //9: through the gate and past the turntable while its bar lies flat
        {12, 16, 0, {104, 15, 16, 12}, 0,
            2, {{30, 0, 4, 10}, {30, 22, 4, 10}},
            1, {{Kind::TURN, {54, 15, 21, 3}, 0, 0, 360, 0}},
            5, {{40, 16, 0}, {44, 5, 0}, {86, 5, 0, 1, {48, 0, 34, 10}, 75}, {86, 20.5f, 0}, {111.5f, 20.5f, 0}}},
        //10: over the flat, then into the garage while its door is up
        {14, 24, 0, {102, 19, 16, 12}, 0,
            2, {{40, 10, 5, 22}, {92, 16, 36, 2}},
            1, {{Kind::SLIDE, {88, 18, 3, 14}, 0, -16, 75, 100}},
            4, {{22, 4.5f, 0}, {52, 5, 0}, {72, 24.5f, 0}, {109.5f, 24.5f, 0, 1, {86, 17, 7, 15}, 70}}},
        //11: two flats sliding against each other, turn round between them, reverse into the bay
        {12, 16, 0, {100, 17, 16, 12}, 180,
            0, {},
            2, {{Kind::SLIDE, {36, 0, 4, 20}, 0, 12, 75, 60}, {Kind::SLIDE, {72, 12, 4, 20}, 0, -12, 70, 80}},
            7, {{24, 25.5f, 0}, {52, 25.5f, 0, 1, {34, 19, 8, 13}, 70}, {56, 16, 180}, {60, 6, 180},
                {88, 6, 180, 2, {70, 0, 8, 12}, 75}, {88, 22.5f, 180}, {107.5f, 22.5f, 180}}},
        //12: a wagon, the turntable, and a bay facing up
        {12, 16, 0, {108, 14, 11, 17}, -90,
            0, {},
            2, {{Kind::WAGON, {30, -14, 10, 12}, 0, 48, 170, 50}, {Kind::TURN, {57, 15, 19, 3}, 0, 0, 400, 0}},
            4, {{20, 16, 0}, {47, 26, 0, 1, {29, 6, 12, 26}, 60}, {88, 26, 0, 2, {52, 20, 28, 12}, 60},
                {113, 22, -90}}},
        //13: crab walk: facing up in a corridor too low to turn in, sideways all the way and down the step
        {10, 16, -90, {104, 13, 12, 15}, -90,
            4, {{0, 7, 58, 3}, {0, 23, 48, 3}, {58, 12, 36, 3}, {48, 28, 80, 3}},
            0, {},
            5, {{30, 16, -90}, {52.5f, 16, -90}, {52.5f, 21.2f, -90, S}, {80, 21, -90, S}, {109.5f, 19.5f, -90}}},
        //14: three pivots: along the top slot, turn, down through the gap, turn, along the bottom, turn, up into the bay
        {10, 5.2f, 0, {79, 0, 11, 15}, -90,
            9, {{0, 11, 30, 2}, {0, 15, 34, 2}, {43, 15, 5, 2}, {46, 0, 2, 15}, {48, 20, 28, 2}, {76, 15, 4, 2},
                {89, 15, 6, 2}, {93, 17, 2, 15}, {72, 0, 2, 15}},
            0, {},
            11, {{24, 5.2f, 0}, {38, 7, 0}, {38, 7.2f, 90, S}, {38, 24, 90, S}, {38, 24, 0, S}, {38, 26.3f, 0, S},
                {60, 26.3f, 0, S}, {84, 26.3f, 0}, {84, 24.5f, 0, S}, {84, 24.5f, -90, S}, {84, 7.2f, -90, S}}},
        //15: over the slanted flat, then line up for the slanted bay: no room to turn between its walls
        {12, 24, 0, {77, 14, 15, 11}, 45,
            3, {{30, 20, 26, 3, -45}, {71, 24, 15, 3, 45}, {83, 12, 15, 3, 45}},
            0, {},
            5, {{22, 5, 0}, {60, 5, 0}, {72, 7, 0, S}, {72, 7, 45, S}, {84, 19, 45, S}}},
        //16: parallel parking past the piston: the lane is too low to turn in and the slot only just takes the vehicle,
        //so it turns round in the open start, reverses along the lane under the piston and strafes up into the slot,
        //nose to the left
        {14, 12, 0, {78, 9, 15, 11}, 180,
            5, {{34, 18, 19, 4}, {57, 18, 18, 4}, {75, 6, 2, 16}, {77, 6, 16, 2}, {93, 12, 35, 10}},
            1, {{Kind::SLIDE, {53, 6, 4, 14}, 0, 12, 45, 70}},
            6, {{16, 12, 180}, {16, 26.6f, 180}, {42, 26.6f, 180}, {72, 26.6f, 180, 1, {46, 21, 18, 11}, 70},
                {85, 26.6f, 180, S}, {85, 14, 180, S}}},
        //17: twin turntables turning against each other: over the first, under the second, down into the bay
        {10, 16, 0, {106, 13, 11, 17}, 90,
            0, {},
            2, {{Kind::TURN, {31, 15, 19, 3}, -1, 0, 300, 0}, {Kind::TURN, {71, 15, 19, 3}, 0, 0, 260, 0}},
            6, {{14, 3.8f, 0}, {60, 3.8f, 0, 1, {24, 0, 32, 10}, 100}, {60, 27.2f, 0, S}, {100, 27.2f, 0, 2, {64, 22, 32, 10}, 95},
                {111, 21, 0, S}, {111, 21, 90, S}}},
        //18: three crushers: under the first, over the second, under the third, each in its own time
        {12, 16, 0, {100, 17, 20, 12}, 0,
            0, {},
            3, {{Kind::SLIDE, {36, 0, 6, 20}, 0, 12, 50, 60}, {Kind::SLIDE, {62, 12, 6, 20}, 0, -12, 45, 75},
                {Kind::SLIDE, {86, 0, 6, 20}, 0, 12, 40, 50}},
            6, {{26, 25.5f, 0}, {52, 25.5f, 0, 1, {33, 20, 12, 12}, 60}, {52, 5.5f, 0, S}, {76, 5.5f, 0, 2, {59, 0, 12, 12}, 60},
                {76, 25.5f, 0, S}, {110, 22.5f, 0, 3, {83, 20, 12, 12}, 60}}},
        //19: slanted gates: each lets the vehicle through only lined up with it, a quarter turn between them
        {10, 12, 0, {99, 17, 15, 11}, -45,
            8, {{27, 20, 7, 3, 45}, {37, 10, 7, 3, 45}, {31, 23, 3, 9}, {37, 0, 3, 9},
                {56, 10, 7, 3, -45}, {66, 20, 7, 3, -45}, {60, 0, 3, 9}, {66, 23, 3, 9}},
            0, {},
            11, {{14, 12, 0, S}, {14, 12, 45, S}, {27.6f, 8.6f, 45, S}, {42.4f, 23.4f, 45, S}, {49.5f, 22, 45, S},
                {49.5f, 22, -45, S}, {56.6f, 23.4f, -45, S}, {72.5f, 7.5f, -45, S}, {80, 10, -45, S}, {84, 18, -45, S},
                {106, 22, -45, S}}},
        //20: crab under the pistons: facing down in a corridor too low to turn in, sideways past two drops
        {8, 24.8f, 90, {108, 17, 12, 15}, 90,
            3, {{0, 17, 36, 2}, {42, 17, 30, 2}, {78, 17, 24, 2}},
            2, {{Kind::SLIDE, {37, 4, 4, 12}, 0, 15, 40, 50}, {Kind::SLIDE, {73, 4, 4, 12}, 0, 15, 35, 70}},
            4, {{26, 24.8f, 90}, {55, 24.8f, 90, 1, {30, 18, 18, 14}, 50}, {91, 24.8f, 90, 2, {66, 18, 18, 14}, 50},
                {113.5f, 25, 90, S}}},
        //21: slanted crab: sideways down the slanted corridor, too narrow to turn in, out into the slanted bay
        {30, 9, -80, {97, 17, 15, 11}, -80,
            2, {{17, 3, 71, 3, 10}, {13, 20, 71, 3, 10}},
            0, {},
            3, {{55, 13.4f, -80}, {80, 17.8f, -80}, {104, 22, -80, S}}},
        //22: all of it: along the top slot, turn, down through the gap, sideways under two pistons, turn round, into
        //the slanted bay
        {10, 5.2f, 0, {105, 17, 15, 11}, -45,
            6, {{0, 11, 30, 2}, {0, 17, 34, 2}, {46, 0, 2, 17}, {43, 17, 15, 2}, {64, 17, 16, 2}, {86, 17, 10, 2}},
            2, {{Kind::SLIDE, {59, 3, 4, 12}, 0, 16, 35, 45}, {Kind::SLIDE, {81, 3, 4, 12}, 0, 16, 30, 60}},
            10, {{24, 5.2f, 0}, {38, 7, 0}, {38, 8.2f, 90, S}, {38, 24.8f, 90, S}, {50, 24.8f, 90, S},
                {70, 24.8f, 90, 1, {52, 18, 18, 14}, 50}, {97, 24.8f, 90, 2, {74, 18, 18, 14}, 50}, {110, 21.5f, 90, S},
                {110, 21.5f, -45, S}, {112, 22, -45, S}}},
    };

    float wrapAngle(float a){
        while(a > PI_F) a -= 2.0f * PI_F;
        while(a < -PI_F) a += 2.0f * PI_F;
        return a;
    }

    float clampf(float v, float lowest, float highest){ return v < lowest ? lowest : (v > highest ? highest : v); }

    //moves value toward target by at most step
    float ramp(float value, float target, float step){
        if(target > value + step) return value + step;
        if(target < value - step) return value - step;
        return target;
    }

    //a painted box as a shape over its pixel centres, grown by the margin given
    ParkingGame::Obb boxShape(const Box& b, float grow){
        ParkingGame::Obb o;
        o.cx = b.x + (b.w - 1) * 0.5f;
        o.cy = b.y + (b.h - 1) * 0.5f;
        o.hx = (b.w - 1) * 0.5f + grow;
        o.hy = (b.h - 1) * 0.5f + grow;
        o.c = 1.0f;
        o.s = 0.0f;
        return o;
    }

    ParkingGame::Obb grown(ParkingGame::Obb o, float grow){
        o.hx += grow;
        o.hy += grow;
        return o;
    }

    //separating axis test of two oriented rectangles: on overlap, the depth and the normal pushing a out of b
    bool penetration(const ParkingGame::Obb& a, const ParkingGame::Obb& b, float& depth, float& nx, float& ny){
        const float axes[4][2] = {{a.c, a.s}, {-a.s, a.c}, {b.c, b.s}, {-b.s, b.c}};
        float dx = a.cx - b.cx, dy = a.cy - b.cy;
        float best = 1e9f;
        for(int i = 0; i < 4; i++){
            float ux = axes[i][0], uy = axes[i][1];
            float ra = a.hx * fabsf(ux * a.c + uy * a.s) + a.hy * fabsf(-ux * a.s + uy * a.c);
            float rb = b.hx * fabsf(ux * b.c + uy * b.s) + b.hy * fabsf(-ux * b.s + uy * b.c);
            float d = dx * ux + dy * uy;
            float overlap = ra + rb - fabsf(d);
            if(overlap <= 0.0f) return false;
            if(overlap < best){
                best = overlap;
                nx = d < 0.0f ? -ux : ux;
                ny = d < 0.0f ? -uy : uy;
            }
        }
        depth = best;
        return true;
    }

    float margined(Kind kind){ return kind == Kind::TURN ? turnMargin : margin; }

    //a fixed piece's shape with its contact margin (a slanted one is drawn rounded: the turning margin)
    ParkingGame::Obb pieceShape(const Piece& p){
        Box b = {p.x, p.y, p.w, p.h};
        if(p.angleDeg == 0) return boxShape(b, margin);
        ParkingGame::Obb o = boxShape(b, turnMargin);
        o.c = cosf(p.angleDeg * DEG);
        o.s = sinf(p.angleDeg * DEG);
        return o;
    }

    //every pixel whose centre lies in the shape (grown by 0.35 px): an even stroke at any angle (two triangles from
    //rounded corners break up on a thin bar)
    void fillShape(Adafruit_GFX& out, const ParkingGame::Obb& m){
        float reach = m.hx + m.hy + 1.0f;
        for(int16_t py = int16_t(m.cy - reach); py <= int16_t(m.cy + reach) + 1; py++){
            for(int16_t px = int16_t(m.cx - reach); px <= int16_t(m.cx + reach) + 1; px++){
                float dx = px - m.cx, dy = py - m.cy;
                if(fabsf(dx * m.c + dy * m.s) <= m.hx + 0.35f && fabsf(-dx * m.s + dy * m.c) <= m.hy + 0.35f) out.drawPixel(px, py, GameDraw::ON);
            }
        }
    }

    bool isSlanted(int16_t headingDeg){ return headingDeg % 90 != 0; }

    //the demo player's lapses: the chance of a charge at a point on the path, rising with the stage
    float lapseChance(uint8_t level){ return 0.08f + 0.03f * level; }
    const uint16_t lapseLongestTicks = 45;
    const uint16_t wreckCardTicks = 30;     //the wreck blinks 0.6 s before the word covers it

    //sound [D-157]: the motor whines in grains of 30 ms every 40 ms, its pitch and level with the speed; sliding
    //along a wall, a scrape of noise every tick instead (synth noise: softer than the remote's square grind)
    const uint8_t motorPeriodTicks = 2;
    const uint16_t motorGrainMillis = 30;
    const float motorLowHertz = 260.0f;     //barely moving
    const float motorHighHertz = 1100.0f;   //full speed and a full turn
    const uint16_t scrapeGrainMillis = 14;
    const float scrapeMinimumSpeed = 2.0f;  //px/s along the wall: slower is a push, not a slide
};

ParkingGame::ParkingGame(){
    using namespace SoundKit;
    //impact: a crunch of noise over a low buzz that drops
    impactNotes.burst(0, 1400, 110, 100, 45, 0);
    impactNotes.sweep(0, 150, 85, 180, 6, Synth::Wave::SQUARE, 95, 1, 60);
    impactCue = impactNotes.cue();
    //wrecked: a long crash of noise, the motor dying in a falling buzz, loose parts clattering, a last hiss
    wreckNotes.burst(0, 1800, 160, 100, 120, 0);
    wreckNotes.burst(140, 900, 260, 90, 200, 1);
    wreckNotes.sweep(0, 420, 60, 700, 16, Synth::Wave::SQUARE, 95, 2, 120);
    wreckNotes.synth(260, 2300, 30, 75, 20, Synth::Wave::PULSE25, 0);
    wreckNotes.synth(360, 1900, 30, 70, 20, Synth::Wave::PULSE25, 0);
    wreckNotes.synth(430, 2600, 25, 65, 15, Synth::Wave::PULSE25, 0);
    wreckNotes.synth(540, 1700, 30, 60, 20, Synth::Wave::PULSE25, 0);
    wreckNotes.burst(720, 5000, 300, 55, 260, 0);
    wreckCue = wreckNotes.cue();
    //parked: a bell of three notes, C G C
    chimeNotes.synth(0, hertz(C6), 260, 85, 90, Synth::Wave::SQUARE, 0);
    chimeNotes.synth(90, hertz(G6), 280, 75, 110, Synth::Wave::PULSE25, 1);
    chimeNotes.synth(180, hertz(C7), 380, 85, 150, Synth::Wave::SQUARE, 0);
    chimeCue = chimeNotes.cue();
    //all twelve parked: a fanfare climbing the C chord, a second voice under its last two notes; ends after 1 s,
    //before the arcade's game over phrase
    fanfareNotes.synth(0, hertz(G5), 80, 85, 0, Synth::Wave::SQUARE, 0);
    fanfareNotes.synth(100, hertz(C6), 80, 85, 0, Synth::Wave::SQUARE, 0);
    fanfareNotes.synth(200, hertz(E6), 80, 85, 0, Synth::Wave::SQUARE, 0);
    fanfareNotes.synth(300, hertz(G6), 160, 90, 0, Synth::Wave::SQUARE, 0);
    fanfareNotes.synth(300, hertz(C5), 160, 70, 0, Synth::Wave::PULSE25, 1);
    fanfareNotes.synth(480, hertz(E6), 80, 85, 0, Synth::Wave::SQUARE, 0);
    fanfareNotes.synth(580, hertz(G6), 420, 90, 200, Synth::Wave::SQUARE, 0);
    fanfareNotes.synth(580, hertz(E5), 420, 70, 200, Synth::Wave::PULSE25, 1);
    fanfareCue = fanfareNotes.cue();
}

void ParkingGame::reset(uint32_t seed){
    random.seed(seed);
    demoRandom.seed(seed ^ 0x5A17C0DEu);
    clockTicks = 0;
    penaltyCount = 0;
    penaltyShowTicks = 0;
    damage = 0;
    b_wrecked = false;
    levelsDone = 0;
    //the demo player's temper changes a little from game to game
    demoGain = 2.6f + 0.8f * random.unit();
    startLevel(0);
}

void ParkingGame::startLevel(uint8_t index){
    level = index;
    phase = Phase::INTRO;
    phaseTicks = 0;
    const Level& l = levels[index];
    //the moving pieces start anywhere in their cycle and the vehicle a little off its mark, so no two runs are the same
    for(uint8_t i = 0; i < maxMovers; i++){
        moverPhase[i] = i < l.moverCount ? uint16_t(random.below(moverCycle(i))) : 0;
        movers[i] = moverAt(i, moverPhase[i]);
    }
    scrapeSpeed = 0.0f;
    //from stage 3 on the vehicle starts askew, 15 to 30 degrees off its mark either way [owner 2026-10-28]; where that
    //does not fit, half of it, and a tight start (a corridor too low to turn in) on its mark exactly
    float skew = (random.unit() - 0.5f) * 10.0f;
    if(index >= 2){
        float amount = 15.0f + 15.0f * random.unit();
        skew = random.below(2) ? amount : -amount;
    }
    float startX = l.startX + (random.unit() - 0.5f) * 3.0f, startY = l.startY + (random.unit() - 0.5f) * 2.0f;
    place(startX, startY, (l.startHeadingDeg + skew) * DEG);
    if(overlapsNow()) place(startX, startY, (l.startHeadingDeg + 0.5f * skew) * DEG);
    if(overlapsNow()) place(l.startX, l.startY, l.startHeadingDeg * DEG);
    freeTicks = 1000;
    parkedTicks = 0;
    waypoint = 0;
    b_demoGo = false;
    lapseTicks = 0;
    stallTicks = unwindTicks = 0;
    b_turnLate = false;
}

void ParkingGame::place(float px, float py, float headingRad){
    x = px;
    y = py;
    heading = wrapAngle(headingRad);
    forwardSpeed = strafeSpeed = turnRate = 0;
    parkedTicks = 0;
}

uint8_t ParkingGame::getMoverCount() const { return levels[level].moverCount; }

uint16_t ParkingGame::moverCycle(uint8_t index) const {
    const Mover& m = levels[level].movers[index];
    if(m.kind == Kind::SLIDE) return 2 * (m.moveTicks + m.holdTicks);
    if(m.kind == Kind::WAGON) return m.moveTicks + m.holdTicks;
    return 2 * m.moveTicks;
}

ParkingGame::Obb ParkingGame::moverAt(uint8_t index, uint16_t ticks) const {
    const Level& l = levels[level];
    if(index >= l.moverCount){
        //no piece: a point far off the stage
        Obb none = {-100, -100, 0, 0, 1, 0};
        return none;
    }
    const Mover& m = l.movers[index];
    Obb o = boxShape(m.box, 0.0f);
    ticks %= moverCycle(index);
    if(m.kind == Kind::TURN){
        float angle = (m.dx < 0 ? -PI_F : PI_F) * float(ticks) / float(m.moveTicks);
        o.c = cosf(angle);
        o.s = sinf(angle);
        return o;
    }
    //how far along its travel, 0..1; a flat starts and stops softly, a wagon rolls through at an even speed
    float along = 0.0f;
    if(m.kind == Kind::SLIDE){
        uint16_t t = ticks;
        if(t < m.holdTicks) along = 0.0f;
        else if((t -= m.holdTicks) < m.moveTicks) along = 0.5f - 0.5f * cosf(PI_F * t / m.moveTicks);
        else if((t -= m.moveTicks) < m.holdTicks) along = 1.0f;
        else along = 0.5f + 0.5f * cosf(PI_F * (t - m.holdTicks) / m.moveTicks);
    }else{
        along = ticks < m.moveTicks ? float(ticks) / m.moveTicks : 1.0f;
    }
    //whole pixels, so the piece is exactly where it is drawn
    o.cx += roundf(m.dx * along);
    o.cy += roundf(m.dy * along);
    return o;
}

ParkingGame::Obb ParkingGame::vehicleAt(float px, float py, float headingRad) const {
    Obb o = {px, py, halfLength, halfWidth, cosf(headingRad), sinf(headingRad)};
    return o;
}

void ParkingGame::corners(float px, float py, float headingRad, float* cx, float* cy) const {
    float c = cosf(headingRad), s = sinf(headingRad);
    //front left, front right, back right, back left; the right side is (-s, c) with y down
    const float along[4] = {halfLength, halfLength, -halfLength, -halfLength};
    const float across[4] = {-halfWidth, halfWidth, halfWidth, -halfWidth};
    for(int i = 0; i < 4; i++){
        cx[i] = px + along[i] * c - across[i] * s;
        cy[i] = py + along[i] * s + across[i] * c;
    }
}

bool ParkingGame::deepestContact(const Obb& v, float& depth, float& nx, float& ny) const {
    depth = 0.0f;
    float d, ux, uy;
    //the stage edge: every corner stays on the screen
    float cx[4], cy[4];
    const float ax[4] = {v.hx, v.hx, -v.hx, -v.hx};
    const float ay[4] = {-v.hy, v.hy, v.hy, -v.hy};
    float low = 0, high = 0, top = 0, bottom = 0;
    for(int i = 0; i < 4; i++){
        cx[i] = v.cx + ax[i] * v.c - ay[i] * v.s;
        cy[i] = v.cy + ax[i] * v.s + ay[i] * v.c;
        if(-cx[i] > low) low = -cx[i];
        if(cx[i] - 127.0f > high) high = cx[i] - 127.0f;
        if(-cy[i] > top) top = -cy[i];
        if(cy[i] - 31.0f > bottom) bottom = cy[i] - 31.0f;
    }
    if(low > depth){ depth = low; nx = 1; ny = 0; }
    if(high > depth){ depth = high; nx = -1; ny = 0; }
    if(top > depth){ depth = top; nx = 0; ny = 1; }
    if(bottom > depth){ depth = bottom; nx = 0; ny = -1; }
    if(penetration(v, boxShape(booth, margin), d, ux, uy) && d > depth){ depth = d; nx = ux; ny = uy; }
    const Level& l = levels[level];
    for(uint8_t i = 0; i < l.pieceCount; i++){
        if(penetration(v, pieceShape(l.pieces[i]), d, ux, uy) && d > depth){ depth = d; nx = ux; ny = uy; }
    }
    for(uint8_t i = 0; i < l.moverCount; i++){
        if(penetration(v, grown(movers[i], margined(l.movers[i].kind)), d, ux, uy) && d > depth){ depth = d; nx = ux; ny = uy; }
    }
    return depth > 0.0f;
}

bool ParkingGame::overlapsNow() const {
    float depth, nx, ny;
    return deepestContact(vehicleAt(x, y, heading), depth, nx, ny);
}

//the moving pieces advance one tick each; one that would run into the vehicle pushes it, unless the vehicle has
//nowhere to go: then the piece waits, like stage machinery stopped by its safety, so it never traps or crushes
void ParkingGame::moveMovers(){
    const Level& l = levels[level];
    for(uint8_t i = 0; i < l.moverCount; i++){
        uint16_t previous = moverPhase[i];
        Obb before = movers[i];
        moverPhase[i] = uint16_t((previous + 1) % moverCycle(i));
        movers[i] = moverAt(i, moverPhase[i]);
        float depth, nx, ny;
        if(!penetration(vehicleAt(x, y, heading), grown(movers[i], margined(l.movers[i].kind)), depth, nx, ny)) continue;
        float px = x + nx * (depth + 0.01f), py = y + ny * (depth + 0.01f);
        float d2, ux, uy;
        if(deepestContact(vehicleAt(px, py, heading), d2, ux, uy)){
            moverPhase[i] = previous;
            movers[i] = before;
            continue;
        }
        x = px;
        y = py;
        //the vehicle loses its speed toward the piece: it is pushed, it did not drive in
        float c = cosf(heading), s = sinf(heading);
        float vx = forwardSpeed * c - strafeSpeed * s, vy = forwardSpeed * s + strafeSpeed * c;
        float into = vx * nx + vy * ny;
        if(into < 0.0f){
            vx -= into * nx;
            vy -= into * ny;
            forwardSpeed = vx * c + vy * s;
            strafeSpeed = -vx * s + vy * c;
        }
    }
}

//the vehicle moves, then is pushed out of whatever it touches and slides along it: the speed into the surface is
//dropped, the speed along it kept. Only driving in faster than bumpSpeed is a bump
void ParkingGame::moveVehicle(const GameInput& input){
    const float dt = tickSeconds;
    //sticks as on the handheld in body frame: left Y forward, left X strafe right, right X turn clockwise
    forwardSpeed = ramp(forwardSpeed, input.stick[1] * maxSpeed, acceleration * dt);
    strafeSpeed = ramp(strafeSpeed, input.stick[0] * maxSpeed, acceleration * dt);
    turnRate = ramp(turnRate, input.stick[2] * maxTurnRate, turnAcceleration * dt);
    scrapeSpeed = 0.0f;
    if(forwardSpeed == 0.0f && strafeSpeed == 0.0f && turnRate == 0.0f) return;

    float c = cosf(heading), s = sinf(heading);
    float vx = forwardSpeed * c - strafeSpeed * s, vy = forwardSpeed * s + strafeSpeed * c;
    float impact = 0.0f;
    bool b_free = false;
    bool b_touched = false;
    //the move with the turn first; when the turn is what wedges it (a slot too narrow to turn in), the move without
    //the turn, so turning against a wall never stops the vehicle driving
    for(int pass = 0; pass < 2 && !b_free; pass++){
        if(pass == 1 && turnRate == 0.0f) break;
        float nh = pass == 0 ? wrapAngle(heading + turnRate * dt) : heading;
        float nx = x + vx * dt, ny = y + vy * dt;
        float wx = vx, wy = vy, hit = 0.0f;
        bool b_hit = false;
        for(int attempt = 0; attempt < 5; attempt++){
            float depth, ux, uy;
            if(!deepestContact(vehicleAt(nx, ny, nh), depth, ux, uy)){
                b_free = true;
                break;
            }
            b_hit = true;
            nx += ux * (depth + 0.01f);
            ny += uy * (depth + 0.01f);
            float into = wx * ux + wy * uy;
            if(into < 0.0f){
                if(-into > hit) hit = -into;
                wx -= into * ux;
                wy -= into * uy;
            }
        }
        if(!b_free) continue;
        x = nx;
        y = ny;
        heading = nh;
        vx = wx;
        vy = wy;
        impact = hit;
        b_touched = b_hit;
        if(pass == 1) turnRate = 0.0f;
    }
    if(!b_free){
        //wedged: stays where it was, which is always free
        vx = vy = 0.0f;
        turnRate = 0.0f;
    }
    float nc = cosf(heading), ns = sinf(heading);
    forwardSpeed = vx * nc + vy * ns;
    strafeSpeed = -vx * ns + vy * nc;
    if(impact > bumpSpeed && freeTicks >= bumpCooldownTicks){
        freeTicks = 0;
        bump(impact);
    }
    //sliding along what it touches: the scrape (sound only)
    else if(b_free && b_touched){
        float along = sqrtf(vx * vx + vy * vy);
        if(along > scrapeMinimumSpeed) scrapeSpeed = along;
    }
    if(freeTicks < 1000) freeTicks++;
}

uint8_t ParkingGame::damageFor(float impactSpeed){
    if(impactSpeed <= bumpSpeed) return 0;
    float amount = minDamage + (fullDamage - minDamage) * (impactSpeed - bumpSpeed) / (maxSpeed - bumpSpeed);
    if(amount > maxDamage) amount = maxDamage;
    return uint8_t(amount + 0.5f);
}

void ParkingGame::bump(float impactSpeed){
    penaltyCount++;
    penaltyShowTicks = 40;
    uint16_t total = uint16_t(damage + damageFor(impactSpeed));
    if(total < 100){
        damage = uint8_t(total);
        sound().play(impactCue);
        return;
    }
    //wrecked: it stops where it is, the clock with it
    damage = 100;
    b_wrecked = true;
    phase = Phase::WRECKED;
    phaseTicks = 0;
    forwardSpeed = strafeSpeed = turnRate = 0;
    scrapeSpeed = 0.0f;
    sound().play(wreckCue);
}

void ParkingGame::getBay(float& cx, float& cy, float& headingDeg, float& alongRoom, float& acrossRoom) const {
    const Level& l = levels[level];
    cx = l.bay.x + (l.bay.w - 1) * 0.5f;
    cy = l.bay.y + (l.bay.h - 1) * 0.5f;
    headingDeg = l.bayHeadingDeg;
    bool b_across = !isSlanted(l.bayHeadingDeg) && (l.bayHeadingDeg == 90 || l.bayHeadingDeg == -90);
    float length = b_across ? l.bay.h : l.bay.w, width = b_across ? l.bay.w : l.bay.h;
    alongRoom = (length - 1) * 0.5f - halfLength;
    acrossRoom = (width - 1) * 0.5f - halfWidth;
}

bool ParkingGame::isParkedNow() const {
    const Level& l = levels[level];
    float cx[4], cy[4];
    corners(x, y, heading, cx, cy);
    if(isSlanted(l.bayHeadingDeg)){
        //in the bay's own frame: along its heading, across it
        float bx = l.bay.x + (l.bay.w - 1) * 0.5f, by = l.bay.y + (l.bay.h - 1) * 0.5f;
        float c = cosf(l.bayHeadingDeg * DEG), s = sinf(l.bayHeadingDeg * DEG);
        for(int i = 0; i < 4; i++){
            float dx = cx[i] - bx, dy = cy[i] - by;
            if(fabsf(dx * c + dy * s) > (l.bay.w - 1) * 0.5f) return false;
            if(fabsf(-dx * s + dy * c) > (l.bay.h - 1) * 0.5f) return false;
        }
    }else{
        for(int i = 0; i < 4; i++){
            if(cx[i] < l.bay.x || cx[i] > l.bay.x + l.bay.w - 1) return false;
            if(cy[i] < l.bay.y || cy[i] > l.bay.y + l.bay.h - 1) return false;
        }
    }
    if(fabsf(wrapAngle(heading - l.bayHeadingDeg * DEG)) > 10.0f * DEG) return false;
    //nearly stopped
    return fabsf(forwardSpeed) < 2.0f && fabsf(strafeSpeed) < 2.0f && fabsf(turnRate) < 10.0f * DEG;
}

void ParkingGame::update(const GameInput& input){
    if(phase == Phase::OVER) return;
    phaseTicks++;
    if(penaltyShowTicks) penaltyShowTicks--;
    //the stage's machinery runs all the time, also during the intro and the flash
    moveMovers();

    if(phase == Phase::INTRO){
        if(phaseTicks >= introTicks){
            phase = Phase::DRIVE;
            phaseTicks = 0;
        }
        return;
    }
    if(phase == Phase::WRECKED){
        if(phaseTicks >= wreckTicks) phase = Phase::OVER;
        return;
    }
    if(phase == Phase::PARKED){
        if(phaseTicks >= flashTicks){
            if(level + 1 >= levelCount) phase = Phase::OVER;
            else startLevel(level + 1);
        }
        return;
    }

    //DRIVE: the clock runs
    clockTicks++;
    moveVehicle(input);
    if(phase == Phase::WRECKED) return;

    if(isParkedNow()){
        if(++parkedTicks >= parkedHoldTicks){
            levelsDone++;
            damage = damage > repairDamage ? uint8_t(damage - repairDamage) : 0;
            phase = Phase::PARKED;
            phaseTicks = 0;
            forwardSpeed = strafeSpeed = turnRate = 0;
            sound().play(levelsDone >= levelCount ? fanfareCue : chimeCue);
        }
    }else{
        parkedTicks = 0;
    }

    if(getClockTicks() >= timeLimitTicks) phase = Phase::OVER;
    if(phase == Phase::DRIVE) playGrains();
}

//the scrape while sliding along a wall (every tick, it matters more), else the motor's whine while anything moves;
//both skip a tick while the impact, the chime or the fanfare plays
void ParkingGame::playGrains(){
    using namespace SoundKit;
    if(scrapeSpeed > 0.0f){
        float amount = clampf(scrapeSpeed / maxSpeed, 0.0f, 1.0f);
        uint16_t brightness = uint16_t(1800 + motorGrain.random.between(0, 2400));
        motorGrain.play(sound(), noiseBurst(0, brightness, scrapeGrainMillis, uint8_t(55.0f + 20.0f * amount), 8));
        return;
    }
    if(forwardSpeed == 0.0f && strafeSpeed == 0.0f && turnRate == 0.0f) return;
    if(!motorGrain.due(motorPeriodTicks)) return;
    float travel = sqrtf(forwardSpeed * forwardSpeed + strafeSpeed * strafeSpeed) / maxSpeed;
    float amount = clampf(travel + 0.5f * fabsf(turnRate) / maxTurnRate, 0.0f, 1.0f);
    //a little unsteady, like four wheel motors that never quite agree
    float wobble = 1.0f + 0.02f * (motorGrain.random.unit() - 0.5f);
    uint16_t pitch = uint16_t((motorLowHertz + (motorHighHertz - motorLowHertz) * amount) * wobble);
    motorGrain.play(sound(), synthNote(0, pitch, motorGrainMillis, uint8_t(35.0f + 35.0f * amount), 0, Synth::Wave::PULSE25));
}

uint32_t ParkingGame::score() const {
    //the stages left unparked first, then the time in tenths of a second (ticks of 20 ms; at most 10 min, under
    //stageWeight): more stages always rank higher, the time decides between runs that got as far
    return uint32_t(levelCount - levelsDone) * stageWeight + getClockTicks() / 5;
}

void ParkingGame::scoreText(char* out, size_t size, uint32_t value) const {
    uint32_t left = value / stageWeight, tenths = value % stageWeight;
    //all parked: the time to the tenth; else the stages parked and the time in whole seconds
    if(left == 0) snprintf(out, size, "%lu.%lus", (unsigned long)(tenths / 10), (unsigned long)(tenths % 10));
    else{
        unsigned parked = left > levelCount ? 0u : unsigned(levelCount - left);
        snprintf(out, size, "%u/%u %lus", parked, unsigned(levelCount), (unsigned long)(tenths / 10));
    }
}

void ParkingGame::overText(char* out, size_t size, uint32_t value) const {
    if(!b_wrecked){
        scoreText(out, size, value);
        return;
    }
    uint32_t left = value / stageWeight;
    unsigned parked = left > levelCount ? 0u : unsigned(levelCount - left);
    snprintf(out, size, "WRECKED %u/%u", parked, unsigned(levelCount));
}

void ParkingGame::drawVehicle(Adafruit_GFX& out, uint16_t color) const {
    //every pixel whose centre lies in the body, as the slanted pieces (an even outline at any angle). The heading
    //[owner 2026-10-28]: the nose is pointed (its front 4 px taper to a tip) and a dark line runs from the back to
    //the taper, an arrow at any angle. Collisions keep the full rectangle
    uint16_t ink = color == GameDraw::ON ? GameDraw::OFF : GameDraw::ON;
    float c = cosf(heading), s = sinf(heading);
    const float edgeLength = halfLength - 0.1f, edgeWidth = halfWidth - 0.1f;
    const float taper = 4.0f, tipWidth = 0.6f;
    const float shaftFrom = -halfLength + 1.5f, shaftTo = edgeLength - taper;
    float reach = halfLength + 1.0f;
    for(int16_t py = int16_t(floorf(y - reach)); py <= int16_t(ceilf(y + reach)); py++){
        for(int16_t px = int16_t(floorf(x - reach)); px <= int16_t(ceilf(x + reach)); px++){
            float dx = px - x, dy = py - y;
            float a = dx * c + dy * s, b = fabsf(-dx * s + dy * c);
            if(fabsf(a) > edgeLength || b > edgeWidth) continue;
            float into = a - (edgeLength - taper);
            if(into > 0.0f && b > edgeWidth - into * (edgeWidth - tipWidth) / taper) continue;
            bool b_line = b < 0.45f && a >= shaftFrom && a <= shaftTo;
            out.drawPixel(px, py, b_line ? ink : color);
        }
    }
}

void ParkingGame::draw(Adafruit_GFX& out) const {
    const Level& l = levels[level];
    for(uint8_t i = 0; i < l.pieceCount; i++){
        const Piece& p = l.pieces[i];
        if(p.angleDeg == 0) out.fillRect(p.x, p.y, p.w, p.h, GameDraw::ON);
        else{
            Obb o = pieceShape(p);
            fillShape(out, grown(o, -turnMargin));
        }
    }

    //the moving pieces: a dark line along a wide flat or a wagon tells it from the fixed set
    for(uint8_t i = 0; i < l.moverCount; i++){
        const Obb& m = movers[i];
        if(l.movers[i].kind == Kind::TURN) fillShape(out, m);
        else{
            int16_t x0 = int16_t(lroundf(m.cx - m.hx)), y0 = int16_t(lroundf(m.cy - m.hy));
            int16_t w = int16_t(lroundf(2.0f * m.hx)) + 1, h = int16_t(lroundf(2.0f * m.hy)) + 1;
            out.fillRect(x0, y0, w, h, GameDraw::ON);
            if(w >= 5 && h >= 5){
                if(h > w) out.drawFastVLine(x0 + w / 2, y0 + 2, h - 4, GameDraw::OFF);
                else out.drawFastHLine(x0 + 2, y0 + h / 2, w - 4, GameDraw::OFF);
            }
        }
    }

    drawBay(out);
    drawBooth(out);

    char label[12];
    if(phase == Phase::INTRO){
        snprintf(label, sizeof(label), "STAGE %u/%u", unsigned(level + 1), unsigned(levelCount));
        GameDraw::pillCentered(out, 64, 12, label);
    }
    //wrecked: the vehicle blinks where it hit, then the word on a cleared screen, the stage it got to under it
    if((phase == Phase::WRECKED && phaseTicks >= wreckCardTicks) || (phase == Phase::OVER && b_wrecked)){
        out.fillRect(0, 0, width, height, GameDraw::OFF);
        GameDraw::bigTextCentered(out, 64, 14, "WRECKED");
        snprintf(label, sizeof(label), "STAGE %u/%u", unsigned(level + 1), unsigned(levelCount));
        GameDraw::pillCentered(out, 64, 19, label, false);
    }
}

//the bay: corner marks, the marked (front) side drawn in full; flashing filled once parked
void ParkingGame::drawBay(Adafruit_GFX& out) const {
    const Level& l = levels[level];
    const Box& b = l.bay;
    bool b_flash = phase == Phase::PARKED && (phaseTicks / 5) % 2 == 0;
    //a wreck blinks
    bool b_show = !(phase == Phase::WRECKED && (phaseTicks / 4) % 2 == 1);
    if(isSlanted(l.bayHeadingDeg)){
        Obb o = boxShape(b, 0.0f);
        o.c = cosf(l.bayHeadingDeg * DEG);
        o.s = sinf(l.bayHeadingDeg * DEG);
        if(b_flash){
            fillShape(out, o);
            drawVehicle(out, GameDraw::OFF);
            return;
        }
        //corners: front left, front right, back right, back left
        const float along[4] = {o.hx, o.hx, -o.hx, -o.hx};
        const float across[4] = {-o.hy, o.hy, o.hy, -o.hy};
        float px[4], py[4];
        for(int i = 0; i < 4; i++){
            px[i] = o.cx + along[i] * o.c - across[i] * o.s;
            py[i] = o.cy + along[i] * o.s + across[i] * o.c;
        }
        const float leg = 3.0f;
        for(int i = 0; i < 4; i++){
            int next = (i + 1) % 4, previous = (i + 3) % 4;
            for(int k = 0; k < 2; k++){
                int other = k ? next : previous;
                float dx = px[other] - px[i], dy = py[other] - py[i];
                float length = sqrtf(dx * dx + dy * dy);
                float ex = px[i] + dx / length * (leg - 1.0f), ey = py[i] + dy / length * (leg - 1.0f);
                out.drawLine(int16_t(lroundf(px[i])), int16_t(lroundf(py[i])), int16_t(lroundf(ex)), int16_t(lroundf(ey)), GameDraw::ON);
            }
        }
        out.drawLine(int16_t(lroundf(px[0])), int16_t(lroundf(py[0])), int16_t(lroundf(px[1])), int16_t(lroundf(py[1])), GameDraw::ON);
        if(b_show) drawVehicle(out, GameDraw::ON);
        return;
    }
    int16_t x0 = b.x, y0 = b.y, x1 = b.x + b.w - 1, y1 = b.y + b.h - 1;
    if(b_flash){
        out.fillRect(x0, y0, b.w, b.h, GameDraw::ON);
        drawVehicle(out, GameDraw::OFF);
        return;
    }
    const int16_t leg = 3;
    out.drawFastHLine(x0, y0, leg, GameDraw::ON);
    out.drawFastHLine(x1 - leg + 1, y0, leg, GameDraw::ON);
    out.drawFastHLine(x0, y1, leg, GameDraw::ON);
    out.drawFastHLine(x1 - leg + 1, y1, leg, GameDraw::ON);
    out.drawFastVLine(x0, y0, leg, GameDraw::ON);
    out.drawFastVLine(x0, y1 - leg + 1, leg, GameDraw::ON);
    out.drawFastVLine(x1, y0, leg, GameDraw::ON);
    out.drawFastVLine(x1, y1 - leg + 1, leg, GameDraw::ON);
    if(l.bayHeadingDeg == 0) out.drawFastVLine(x1, y0, b.h, GameDraw::ON);
    else if(l.bayHeadingDeg == 180) out.drawFastVLine(x0, y0, b.h, GameDraw::ON);
    else if(l.bayHeadingDeg == 90) out.drawFastHLine(x0, y1, b.w, GameDraw::ON);
    else out.drawFastHLine(x0, y0, b.w, GameDraw::ON);
    if(b_show) drawVehicle(out, GameDraw::ON);
}

//the time in its corner (or the penalty while it shows), the damage meter under it: the booth's bottom edge is the
//meter's track, the damage two rows on it from the left (one under the penalty's pill, 1 px apart), blinking just
//after a bump
void ParkingGame::drawBooth(Adafruit_GFX& out) const {
    out.drawFastVLine(booth.x, 0, booth.h, GameDraw::ON);
    out.drawFastHLine(booth.x, booth.h - 1, booth.w, GameDraw::ON);
    int16_t fill = int16_t((damage * (booth.w - 1) + 50) / 100);
    if(damage > 0 && fill < 1) fill = 1;
    bool b_blink = penaltyShowTicks && (penaltyShowTicks / 5) % 2 == 0;
    if(fill > 0 && !b_blink){
        int16_t rows = penaltyShowTicks ? 1 : 2;
        out.fillRect(booth.x + 1, booth.h - 1 - rows, fill, rows, GameDraw::ON);
    }
    if(penaltyShowTicks){
        GameDraw::pill(out, 126 - GameDraw::pillWidth("+2s") + 1, 0, "+2s", false);
    }else{
        char label[12];
        uint32_t tenths = getClockTicks() / 5;
        snprintf(label, sizeof(label), "%lu.%lu", (unsigned long)(tenths / 10), (unsigned long)(tenths % 10));
        GameDraw::textRight(out, 126, 1, label);
    }
}

//the demo player goes on to a gated point only when the moving piece stays out of the zone it drives through for
//the time it needs, predicted from the piece's cycle (as an operator who has watched it come round)
bool ParkingGame::gateOpen(uint8_t index) const {
    const Waypoint& w = levels[level].waypoints[index];
    if(w.gate == 0 || w.gate == S) return true;
    uint8_t m = w.gate - 1;
    Obb zone = boxShape(w.zone, 0.0f);
    float depth, nx, ny;
    for(uint16_t k = 0; k <= w.leadTicks; k += 3){
        if(penetration(zone, moverAt(m, uint16_t((moverPhase[m] + k) % moverCycle(m))), depth, nx, ny)) return false;
    }
    return true;
}

void ParkingGame::demo(GameInput& input){
    if(phase != Phase::DRIVE) return;
    const Level& l = levels[level];
    //stuck: did the last tick move it at all
    bool b_moved = fabsf(x - demoLastX) + fabsf(y - demoLastY) > 0.005f || fabsf(heading - demoLastHeading) > 0.001f;
    demoLastX = x;
    demoLastY = y;
    demoLastHeading = heading;
    if(unwindTicks){
        unwindTicks--;
        input.stick[2] = unwindTurn;
        return;
    }
    //a lapse: full stick one way until it hits something (or gives up), then back to the path
    if(lapseTicks){
        if(penaltyCount != lapsePenalties) lapseTicks = 0;
        else{
            lapseTicks--;
            float c = cosf(heading), s = sinf(heading);
            input.stick[1] = clampf(lapseX * c + lapseY * s, -1.0f, 1.0f);
            input.stick[0] = clampf(-lapseX * s + lapseY * c, -1.0f, 1.0f);
            return;
        }
    }
    //waiting for a moving piece: it holds at the point before the gated one until the way is clear
    if(!b_demoGo && (waypoint == 0 || gateOpen(waypoint))) b_demoGo = true;
    uint8_t target = b_demoGo ? waypoint : uint8_t(waypoint - 1);
    const Waypoint& w = l.waypoints[target];
    bool b_last = waypoint + 1 >= l.waypointCount;
    //the point before a gated one is a stop too, it may have to wait there
    bool b_stop = !b_demoGo || b_last || l.waypoints[waypoint + 1].gate != 0;
    float dx = w.x - x, dy = w.y - y;
    float distance = sqrtf(dx * dx + dy * dy);
    float headingError = wrapAngle(w.headingDeg * DEG - heading);
    bool b_precise = !b_last && l.waypoints[waypoint + 1].gate != 0;
    if(b_demoGo && !b_last && distance < (b_precise ? 1.0f : 3.0f) && fabsf(headingError) < 10.0f * DEG){
        waypoint++;
        b_demoGo = false;
        b_turnLate = false;
        //now and then it charges at the next point instead (more often on the later stages); never where pieces move,
        //where it could stop a turntable on itself while it waits for that turntable
        if(b_demoLapses && l.moverCount == 0 && demoRandom.unit() < lapseChance(level)){
            const Waypoint& n = l.waypoints[waypoint];
            float ex = n.x - x, ey = n.y - y;
            float length = sqrtf(ex * ex + ey * ey);
            if(length > 0.5f){
                lapseX = ex / length;
                lapseY = ey / length;
            }else{
                lapseX = cosf(heading);
                lapseY = sinf(heading);
            }
            lapseTicks = lapseLongestTicks;
            lapsePenalties = penaltyCount;
        }
        return demo(input);
    }
    //speed toward the point: slows down where it stops, keeps some way through the points on the path
    float speed = demoGain * distance + (b_stop ? 0.0f : 8.0f);
    if(speed > maxSpeed) speed = maxSpeed;
    float vx = 0, vy = 0;
    if(distance > 0.05f){
        vx = dx / distance * speed;
        vy = dy / distance * speed;
    }
    //world to body frame, like a skilled operator who translates and turns at once
    float c = cosf(heading), s = sinf(heading);
    float forward = vx * c + vy * s;
    float strafe = -vx * s + vy * c;
    input.stick[1] = clampf(forward / maxSpeed, -1.0f, 1.0f);
    input.stick[0] = clampf(strafe / maxSpeed, -1.0f, 1.0f);
    input.stick[2] = clampf(3.0f * headingError / maxTurnRate, -1.0f, 1.0f);
    if(b_turnLate && distance > 2.0f) input.stick[2] = 0.0f;
    //the handheld's deadband
    for(int i = 0; i < 4; i++) if(fabsf(input.stick[i]) < 0.01f) input.stick[i] = 0.0f;
    //pushing the sticks and not moving for 0.2 s: wedged by its own turn. Turn back a little, then turn at the point
    bool b_pushing = fabsf(input.stick[0]) > 0.2f || fabsf(input.stick[1]) > 0.2f || fabsf(input.stick[2]) > 0.2f;
    stallTicks = b_pushing && !b_moved ? uint8_t(stallTicks + 1) : 0;
    if(stallTicks >= 10){
        stallTicks = 0;
        unwindTicks = 15;
        unwindTurn = headingError > 0.0f ? -0.6f : 0.6f;
        b_turnLate = true;
    }
}
