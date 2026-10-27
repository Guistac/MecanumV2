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

    //SLIDE: a flat on a track, from box to box + (dx, dy) and back, resting at both ends. WAGON: crosses the stage from
    //box to box + (dx, dy), both off the stage, then waits off stage. TURN: a bar on a turntable, box at 0 degrees,
    //half a turn clockwise in moveTicks
    enum class Kind : uint8_t { SLIDE, WAGON, TURN };

    struct Mover{
        Kind kind;
        Box box;
        int16_t dx, dy;
        uint16_t moveTicks;
        uint16_t holdTicks;
    };

    //the demo player's path; a gated point waits until the mover (1 based) stays out of the zone for leadTicks
    struct Waypoint{
        float x, y, headingDeg;
        uint8_t gate;
        Box zone;
        uint16_t leadTicks;
    };

    //the time is shown in the top right corner, which every stage keeps free: it is a wall like the set pieces
    const Box booth = {95, 0, 33, 11};

    struct Level{
        float startX, startY, startHeadingDeg;
        Box bay;                    //the painted outline; the vehicle's corners must stay on or inside it
        int16_t bayHeadingDeg;      //0 right, 90 down, 180 left, -90 up: the marked (front) side
        uint8_t pieceCount;
        Box pieces[4];
        uint8_t moverCount;
        Mover movers[ParkingGame::maxMovers];
        uint8_t waypointCount;      //the last one is the bay
        Waypoint waypoints[8];
    };

    const Level levels[ParkingGame::levelCount] = {
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
            2, {{72, 12, 25, 3}, {95, 15, 3, 17}},
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
        {12, 6, 0, {61, 18, 11, 14}, 90,
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
};

void ParkingGame::reset(uint32_t seed){
    random.seed(seed);
    clockTicks = 0;
    penaltyCount = 0;
    penaltyShowTicks = 0;
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
    place(l.startX + (random.unit() - 0.5f) * 3.0f, l.startY + (random.unit() - 0.5f) * 2.0f,
          (l.startHeadingDeg + (random.unit() - 0.5f) * 10.0f) * DEG);
    freeTicks = 1000;
    parkedTicks = 0;
    waypoint = 0;
    b_demoGo = false;
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
        float angle = PI_F * float(ticks) / float(m.moveTicks);
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
        if(penetration(v, boxShape(l.pieces[i], margin), d, ux, uy) && d > depth){ depth = d; nx = ux; ny = uy; }
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
    if(forwardSpeed == 0.0f && strafeSpeed == 0.0f && turnRate == 0.0f) return;

    float c = cosf(heading), s = sinf(heading);
    float vx = forwardSpeed * c - strafeSpeed * s, vy = forwardSpeed * s + strafeSpeed * c;
    float nh = wrapAngle(heading + turnRate * dt);
    float nx = x + vx * dt, ny = y + vy * dt;
    float impact = 0.0f;
    bool b_free = false;
    for(int attempt = 0; attempt < 5; attempt++){
        float depth, ux, uy;
        if(!deepestContact(vehicleAt(nx, ny, nh), depth, ux, uy)){
            b_free = true;
            break;
        }
        nx += ux * (depth + 0.01f);
        ny += uy * (depth + 0.01f);
        float into = vx * ux + vy * uy;
        if(into < 0.0f){
            if(-into > impact) impact = -into;
            vx -= into * ux;
            vy -= into * uy;
        }
    }
    if(b_free){
        x = nx;
        y = ny;
        heading = nh;
    }else{
        //wedged (a turn in a pocket): stays where it was, which is always free
        vx = vy = 0.0f;
        turnRate = 0.0f;
    }
    float nc = cosf(heading), ns = sinf(heading);
    forwardSpeed = vx * nc + vy * ns;
    strafeSpeed = -vx * ns + vy * nc;
    if(impact > bumpSpeed && freeTicks >= bumpCooldownTicks){
        penaltyCount++;
        penaltyShowTicks = 40;
        freeTicks = 0;
    }
    if(freeTicks < 1000) freeTicks++;
}

bool ParkingGame::isParkedNow() const {
    const Level& l = levels[level];
    float cx[4], cy[4];
    corners(x, y, heading, cx, cy);
    for(int i = 0; i < 4; i++){
        if(cx[i] < l.bay.x || cx[i] > l.bay.x + l.bay.w - 1) return false;
        if(cy[i] < l.bay.y || cy[i] > l.bay.y + l.bay.h - 1) return false;
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

    if(isParkedNow()){
        if(++parkedTicks >= parkedHoldTicks){
            levelsDone++;
            phase = Phase::PARKED;
            phaseTicks = 0;
            forwardSpeed = strafeSpeed = turnRate = 0;
        }
    }else{
        parkedTicks = 0;
    }

    if(getClockTicks() >= timeLimitTicks) phase = Phase::OVER;
}

uint32_t ParkingGame::score() const {
    //ticks of 20 ms to tenths of a second
    return getClockTicks() / 5 + uint32_t(levelCount - levelsDone) * 600;
}

void ParkingGame::scoreText(char* out, size_t size, uint32_t value) const {
    snprintf(out, size, "%lu.%lus", (unsigned long)(value / 10), (unsigned long)(value % 10));
}

void ParkingGame::drawVehicle(Adafruit_GFX& out, uint16_t color) const {
    float cx[4], cy[4];
    corners(x, y, heading, cx, cy);
    int16_t px[4], py[4];
    for(int i = 0; i < 4; i++){
        px[i] = int16_t(lroundf(cx[i]));
        py[i] = int16_t(lroundf(cy[i]));
    }
    out.fillTriangle(px[0], py[0], px[1], py[1], px[2], py[2], color);
    out.fillTriangle(px[0], py[0], px[2], py[2], px[3], py[3], color);
    //the front: a dark windscreen line across the body, 2 px behind the nose
    float c = cosf(heading), s = sinf(heading);
    float along = halfLength - 2.0f, across = halfWidth - 1.5f;
    int16_t ax = int16_t(lroundf(x + along * c + across * s)), ay = int16_t(lroundf(y + along * s - across * c));
    int16_t bx = int16_t(lroundf(x + along * c - across * s)), by = int16_t(lroundf(y + along * s + across * c));
    out.drawLine(ax, ay, bx, by, color == GameDraw::ON ? GameDraw::OFF : GameDraw::ON);
}

void ParkingGame::draw(Adafruit_GFX& out) const {
    const Level& l = levels[level];
    for(uint8_t i = 0; i < l.pieceCount; i++) out.fillRect(l.pieces[i].x, l.pieces[i].y, l.pieces[i].w, l.pieces[i].h, GameDraw::ON);

    //the moving pieces: a dark line along a wide flat or a wagon tells it from the fixed set
    for(uint8_t i = 0; i < l.moverCount; i++){
        const Obb& m = movers[i];
        if(l.movers[i].kind == Kind::TURN){
            //every pixel whose centre lies in the bar: an even 3 px stroke at any angle (two triangles from rounded
            //corners break up on a thin bar)
            float reach = m.hx + m.hy + 1.0f;
            for(int16_t py = int16_t(m.cy - reach); py <= int16_t(m.cy + reach) + 1; py++){
                for(int16_t px = int16_t(m.cx - reach); px <= int16_t(m.cx + reach) + 1; px++){
                    float dx = px - m.cx, dy = py - m.cy;
                    if(fabsf(dx * m.c + dy * m.s) <= m.hx + 0.35f && fabsf(-dx * m.s + dy * m.c) <= m.hy + 0.35f) out.drawPixel(px, py, GameDraw::ON);
                }
            }
        }else{
            int16_t x0 = int16_t(lroundf(m.cx - m.hx)), y0 = int16_t(lroundf(m.cy - m.hy));
            int16_t w = int16_t(lroundf(2.0f * m.hx)) + 1, h = int16_t(lroundf(2.0f * m.hy)) + 1;
            out.fillRect(x0, y0, w, h, GameDraw::ON);
            if(w >= 5 && h >= 5){
                if(h > w) out.drawFastVLine(x0 + w / 2, y0 + 2, h - 4, GameDraw::OFF);
                else out.drawFastHLine(x0 + 2, y0 + h / 2, w - 4, GameDraw::OFF);
            }
        }
    }

    //the bay: corner marks, the marked (front) side drawn in full
    const Box& b = l.bay;
    int16_t x0 = b.x, y0 = b.y, x1 = b.x + b.w - 1, y1 = b.y + b.h - 1;
    bool b_flash = phase == Phase::PARKED;
    if(b_flash && (phaseTicks / 5) % 2 == 0){
        out.fillRect(x0, y0, b.w, b.h, GameDraw::ON);
        drawVehicle(out, GameDraw::OFF);
    }else{
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
        drawVehicle(out, GameDraw::ON);
    }

    //the time in its corner, or the penalty while it shows
    out.drawFastVLine(booth.x, 0, booth.h, GameDraw::ON);
    out.drawFastHLine(booth.x, booth.h - 1, booth.w, GameDraw::ON);
    char label[12];
    if(penaltyShowTicks){
        GameDraw::pill(out, 126 - GameDraw::pillWidth("+2s") + 1, 0, "+2s", false);
    }else{
        uint32_t tenths = getClockTicks() / 5;
        snprintf(label, sizeof(label), "%lu.%lu", (unsigned long)(tenths / 10), (unsigned long)(tenths % 10));
        GameDraw::textRight(out, 126, 1, label);
    }

    if(phase == Phase::INTRO){
        snprintf(label, sizeof(label), "STAGE %u/%u", unsigned(level + 1), unsigned(levelCount));
        GameDraw::pillCentered(out, 64, 12, label);
    }
}

//the demo player goes on to a gated point only when the moving piece stays out of the zone it drives through for
//the time it needs, predicted from the piece's cycle (as an operator who has watched it come round)
bool ParkingGame::gateOpen(uint8_t index) const {
    const Waypoint& w = levels[level].waypoints[index];
    if(w.gate == 0) return true;
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
    if(b_demoGo && !b_last && distance < 3.0f && fabsf(headingError) < 10.0f * DEG){
        waypoint++;
        b_demoGo = false;
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
    //the handheld's deadband
    for(int i = 0; i < 4; i++) if(fabsf(input.stick[i]) < 0.01f) input.stick[i] = 0.0f;
}
