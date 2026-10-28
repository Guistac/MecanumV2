#pragma once
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "MarkStore.h"
#include "Stopwatch.h"

//Timed targets (owner, 2026-10-28, replacing the ordered route), display only: nothing drives. Every mark is an
//independent target; its time is a DURATION, how long the move there should take (blank: no timed run to it).
//
//PLAY (TrackingControl, drive mode: the right click) with a selected mark that has a duration and a known pose: the
//remote takes the pose now as the start and plans a straight move from it to the mark over the mark's duration T
//(TargetPlan): the position along the straight line start -> target, the heading from the start's to the mark's by
//the shortest turn (a blank heading keeps the start's). The speed profile is smooth: constant acceleration for
//rampSeconds (1 s), constant speed, constant deceleration for the last 1 s (a trapezoid in speed); with T < 2 s a
//triangle (T / 2 each way). The heading follows the same profile.
//
//PACE: the rover's position projected onto the start -> target line as a fraction (clamped 0..1); the planned time
//for that fraction from the inverse of the profile; delta = elapsed - planned (late positive). Exact on a straight
//leg. The page shows only FASTER / SLOWER outside +-0.3 s (owner, 2026-10-28: the operator drives by eye, the screen
//only says whether to hurry), the time left and the ghost on a dotted line start -> target. A start already within
//arriveRadius of the target (only the heading is off) is a turn on the spot: its fraction is the heading turned so far.
//
//ARRIVAL: within arriveRadius (0.15 m) of the target, or past the line through the target square to the leg within
//passReach (1 m); a turn on the spot: the heading within alignedDegrees (5 deg) of the mark's, or past it. The run ends
//there: the stopwatch stops (its elapsed time is the run's), the split "+0.4s" (arrival - T) shows for
//arrivalShownMillis (3 s), then the plain map. Nothing selects the next mark by itself.
//
//The run's time IS the stopwatch (StopwatchControl, the one the Status and Motion pages show): play restarts it from
//0:00 (count up; a set countdown stays set and counts down on Status), pause and resume stop and start it, the
//rewind (left hold) resets it and cancels the run. The stopwatch's own controls on Status and Motion act on the same
//timer: stopping it there pauses the run, starting it resumes, resetting it (or editing the countdown, which resets)
//cancels the run; with no run they only run the stopwatch, they never start a run.

//what the plan says about a pose
struct PaceFix{
    bool b_valid = false;
    float fraction = 0.0f;          //0..1 of the move done
    float plannedSeconds = NAN;     //when the plan is at that fraction
};

class TargetPlan{
public:
    static constexpr float rampSeconds = 1.0f;
    static constexpr float arriveRadius = 0.15f;    //m
    static constexpr float passReach = 1.0f;        //m: past the target's square line, only this close counts
    static constexpr float alignedDegrees = 5.0f;   //a turn on the spot arrives within this; play refuses within it

    //a straight move from (x0, y0, h0) to (x1, y1, h1) (h1 NaN: keep h0) over seconds; false (no plan) without a
    //start pose or a positive duration
    bool plan(float x0, float y0, float h0, float x1, float y1, float h1, float seconds){
        b_valid = false;
        if(isnan(x0) || isnan(y0) || isnan(h0) || isnan(x1) || isnan(y1) || !(seconds > 0.0f)) return false;
        sx = x0; sy = y0; sh = h0;
        tx = x1; ty = y1; th = h1;
        T = seconds;
        dx = x1 - x0;
        dy = y1 - y0;
        length = hypotf(dx, dy);
        turnTotal = isnan(h1) ? 0.0f : wrap(h1 - h0);
        b_turnOnly = length <= arriveRadius;
        ramp = T * 0.5f < rampSeconds ? T * 0.5f : rampSeconds;
        peak = 1.0f / (T - ramp);
        accel = peak / ramp;
        b_valid = true;
        return true;
    }

    bool isValid() const { return b_valid; }
    bool isTurnOnly() const { return b_turnOnly; }
    //the plan turns on the way (a heading at the target differing from the start's)
    bool turns() const { return b_valid && fabsf(turnTotal) > 1e-3f; }
    float duration() const { return T; }
    float startX() const { return sx; }
    float startY() const { return sy; }
    float targetX() const { return tx; }
    float targetY() const { return ty; }
    float targetHeading() const { return th; }
    float startHeading() const { return sh; }

    //the fraction of the move done t s after the start: 0 before, 1 from T on (exactly)
    float fraction(float t) const {
        if(!b_valid || t <= 0.0f) return 0.0f;
        if(t >= T) return 1.0f;
        if(t < ramp) return 0.5f * accel * t * t;
        if(t > T - ramp){
            float r = T - t;
            return 1.0f - 0.5f * accel * r * r;
        }
        return 0.5f * peak * ramp + peak * (t - ramp);
    }

    //the inverse: when the plan is at fraction s (clamped 0..1)
    float timeAt(float s) const {
        if(!b_valid || s <= 0.0f) return 0.0f;
        if(s >= 1.0f) return T;
        float s1 = 0.5f * peak * ramp;
        if(s < s1) return sqrtf(2.0f * s / accel);
        if(s > 1.0f - s1) return T - sqrtf(2.0f * (1.0f - s) / accel);
        return ramp + (s - s1) / peak;
    }

    //the planned heading at fraction s
    float headingAtFraction(float s) const { return wrap(sh + s * turnTotal); }

    //where the plan is t s after the start
    bool ghostAt(float t, float& x, float& y, float& heading) const {
        if(!b_valid) return false;
        float s = fraction(t);
        x = sx + s * dx;
        y = sy + s * dy;
        heading = headingAtFraction(s);
        return true;
    }

    PaceFix locate(float x, float y, float heading) const {
        PaceFix fix;
        if(!b_valid || isnan(x) || isnan(y)) return fix;
        float s;
        if(b_turnOnly){
            if(isnan(heading) || fabsf(turnTotal) < 1e-3f) return fix;
            s = wrap(heading - sh) / turnTotal;
        }
        else{
            float rx = x - sx, ry = y - sy;
            s = (rx * dx + ry * dy) / (length * length);
        }
        s = s < 0.0f ? 0.0f : (s > 1.0f ? 1.0f : s);
        fix.b_valid = true;
        fix.fraction = s;
        fix.plannedSeconds = timeAt(s);
        return fix;
    }

    //the target reached (see the file's comment)
    bool reached(float x, float y, float heading) const {
        if(!b_valid || isnan(x) || isnan(y)) return false;
        float ex = x - tx, ey = y - ty;
        float distance = hypotf(ex, ey);
        if(b_turnOnly){
            if(isnan(heading) || distance > passReach) return false;
            if(fabsf(wrap(th - heading)) <= alignedDegrees) return true;
            return fabsf(turnTotal) > 1e-3f && wrap(heading - sh) / turnTotal >= 1.0f;
        }
        if(distance <= arriveRadius) return true;
        return distance <= passReach && ex * dx + ey * dy > 0.0f;
    }


    //the rover already at a target: within arriveRadius and, unless its heading is blank, alignedDegrees
    static bool isThere(float x, float y, float heading, float mx, float my, float mh){
        if(hypotf(mx - x, my - y) > arriveRadius) return false;
        return isnan(mh) || fabsf(wrap(mh - heading)) <= alignedDegrees;
    }

    static float wrap(float degrees){
        if(isnan(degrees)) return degrees;
        degrees = fmodf(degrees + 180.0f, 360.0f);
        if(degrees < 0.0f) degrees += 360.0f;
        return degrees - 180.0f;
    }

    //the heading (deg, counter-clockwise, 0 along +y) of a direction (dx, dy): the map frame's convention
    static float headingOf(float dx, float dy){ return atan2f(-dx, dy) * 57.29578f; }

private:
    bool b_valid = false;
    bool b_turnOnly = false;
    float sx = 0, sy = 0, sh = 0;
    float tx = 0, ty = 0, th = NAN;
    float dx = 0, dy = 0, length = 0;
    float turnTotal = 0;
    float T = 0, ramp = 0, peak = 0, accel = 0;
};

//why a play was refused
enum class RunRefusal : uint8_t{ NONE, NO_MARK, ZERO, NO_TIME, NO_POSE, ALREADY_THERE };

//an arrival: the mark and its time against the duration
struct RunSplit{
    uint8_t mark = 0;               //1..9
    float delta = 0.0f;             //s, arrival - duration: late positive
};

//what the Tracking page shows of the run (ScreenModel), made by TargetRun::view
struct TrackingRunView{
    bool b_run = false;             //a run plays or is paused
    bool b_paused = false;
    bool b_arrived = false;         //the run ended at its target: the split, for arrivalShownMillis
    uint8_t target = 0;             //1..9
    float startX = NAN, startY = NAN;       //the plan's line: the pose at the play to the target
    float targetX = NAN, targetY = NAN;
    bool b_ghost = false;
    float ghostX = NAN, ghostY = NAN, ghostHeading = NAN;
    float delta = NAN;              //s, late positive (NaN: no reading yet)
    //[owner 2026-10-28] the turn tracked apart from the translation: the planned heading at the elapsed time minus the
    //rover's (deg, counter-clockwise positive: turn more to the left); NaN on a move without a turn or on a turn on the
    //spot (there the translation's pace is the turn's)
    float turnError = NAN;
    float timeLeft = NAN;           //s, the duration - elapsed
    RunSplit split;
    RunRefusal refusal = RunRefusal::NONE;  //a refused play, shown for a moment
    uint8_t refusalMark = 0;
};

class TargetRun{
public:
    enum class State : uint8_t{ IDLE, RUNNING, ARRIVED };
    enum Result : uint8_t{ NOTHING, STARTED, PAUSED, RESUMED, REFUSED };
    static constexpr uint32_t refusalShownMillis = 1500;
    static constexpr uint32_t arrivalShownMillis = 3000;

    explicit TargetRun(StopwatchControl& stopwatch) : watch(stopwatch) {}

    //the right click in drive mode (TrackingControl::PLAY_PAUSE): during a run pause / resume, timed at the press;
    //otherwise play: a plan from the pose now to the selected mark, the stopwatch restarted now (the pose's moment).
    //Refused (nothing changes but an arrival shown is acknowledged): no mark selected or an empty one, Z (no
    //duration), a mark without a duration (or 0.0 s), no pose, or the rover already there
    Result click(const MarkStore& marks, int8_t selected, bool b_pose, float x, float y, float heading,
                 uint32_t pressMillis, uint32_t now){
        if(state == State::RUNNING && watch.stopwatch().getState() == Stopwatch::State::IDLE) state = State::IDLE;
        if(state == State::RUNNING){
            return watch.playPause(pressMillis) == StopwatchControl::STOPPED ? PAUSED : RESUMED;
        }
        acknowledge();
        RunRefusal why = RunRefusal::NONE;
        const Mark& m = marks.get(selected < 0 ? 0 : uint8_t(selected));
        if(selected < 0 || !marks.isUsed(uint8_t(selected))) why = RunRefusal::NO_MARK;
        else if(selected == MarkStore::zero) why = RunRefusal::ZERO;
        else if(!m.b_time || m.timeTenths == 0) why = RunRefusal::NO_TIME;
        else if(!b_pose || isnan(x) || isnan(y) || isnan(heading)) why = RunRefusal::NO_POSE;
        else if(TargetPlan::isThere(x, y, heading, m.x(), m.y(), m.heading())) why = RunRefusal::ALREADY_THERE;
        if(why == RunRefusal::NONE && !plan.plan(x, y, heading, m.x(), m.y(), m.heading(), float(m.timeMillis()) * 0.001f)){
            why = RunRefusal::NO_TIME;
        }
        if(why != RunRefusal::NONE){
            refused = why;
            refusedMark = selected < 0 ? 0 : uint8_t(selected);
            refusedMillis = now;
            return REFUSED;
        }
        refused = RunRefusal::NONE;
        targetMark = uint8_t(selected);
        fix = PaceFix();
        delta = NAN;
        watch.restart(now);
        state = State::RUNNING;
        return STARTED;
    }

    //the left hold in drive mode: the run cancelled, the stopwatch reset (with no run: the stopwatch reset)
    void rewind(){
        state = State::IDLE;
        refused = RunRefusal::NONE;
        watch.rewind();
    }

    //the next action after an arrival (a play, a rewind, a mark selected): the split goes
    void acknowledge(){ if(state == State::ARRIVED) state = State::IDLE; }

    //every refresh with the pose in the display frame (b_pose false: none). True once, at the arrival. While paused
    //the reading stays as it was. The stopwatch reset elsewhere cancels the run; an arrival shown ends when the
    //stopwatch is reset or started again elsewhere
    bool update(bool b_pose, float x, float y, float heading, uint32_t now){
        Stopwatch::State w = watch.stopwatch().getState();
        if(state == State::RUNNING && w == Stopwatch::State::IDLE) state = State::IDLE;
        if(state == State::ARRIVED && w != Stopwatch::State::STOPPED) state = State::IDLE;
        if(state != State::RUNNING || w != Stopwatch::State::RUNNING || !b_pose) return false;
        float elapsed = seconds(now);
        fix = plan.locate(x, y, heading);
        delta = fix.b_valid ? elapsed - fix.plannedSeconds : NAN;
        turnError = NAN;
        if(plan.turns() && !plan.isTurnOnly() && !isnan(heading)){
            float gx, gy, gh;
            if(plan.ghostAt(elapsed, gx, gy, gh)) turnError = TargetPlan::wrap(gh - heading);
        }
        if(!plan.reached(x, y, heading)) return false;
        watch.stop(now);
        split.mark = targetMark;
        split.delta = float(watch.stopwatch().elapsed(now)) * 0.001f - plan.duration();
        state = State::ARRIVED;
        arrivedMillis = now;
        return true;
    }

    State getState() const { return state; }
    bool isRunning() const { return state == State::RUNNING; }
    bool isPaused() const { return state == State::RUNNING && !watch.stopwatch().isRunning(); }
    //the run's target (1..9), -1 without a run
    int8_t target() const { return state == State::RUNNING ? int8_t(targetMark) : int8_t(-1); }
    const TargetPlan& getPlan() const { return plan; }
    const PaceFix& getFix() const { return fix; }
    float getDelta() const { return delta; }
    const RunSplit& getSplit() const { return split; }
    RunRefusal refusal(uint32_t now) const {
        return refused != RunRefusal::NONE && now - refusedMillis < refusalShownMillis ? refused : RunRefusal::NONE;
    }
    float seconds(uint32_t now) const { return float(watch.stopwatch().elapsed(now)) * 0.001f; }

    //the page's view at now
    void view(TrackingRunView& v, uint32_t now) const {
        v = TrackingRunView();
        v.refusal = refusal(now);
        v.refusalMark = refusedMark;
        if(state == State::ARRIVED){
            v.b_arrived = now - arrivedMillis < arrivalShownMillis;
            v.target = targetMark;
            v.split = split;
            return;
        }
        if(state != State::RUNNING) return;
        v.b_run = true;
        v.b_paused = isPaused();
        v.target = targetMark;
        v.startX = plan.startX();
        v.startY = plan.startY();
        v.targetX = plan.targetX();
        v.targetY = plan.targetY();
        float elapsed = seconds(now);
        v.b_ghost = plan.ghostAt(elapsed, v.ghostX, v.ghostY, v.ghostHeading);
        v.delta = delta;
        v.turnError = turnError;
        v.timeLeft = plan.duration() - elapsed;
    }

private:
    StopwatchControl& watch;
    State state = State::IDLE;
    TargetPlan plan;
    uint8_t targetMark = 0;
    PaceFix fix;
    float delta = NAN;
    float turnError = NAN;
    RunSplit split;
    RunRefusal refused = RunRefusal::NONE;
    uint8_t refusedMark = 0;
    uint32_t refusedMillis = 0;
    uint32_t arrivedMillis = 0;
};

//The run's texts (the Tracking page's pills), host tested
namespace Pace{

    //the turn's own cue: shown beyond this many degrees of lag or lead
    static constexpr float turnBand = 5.0f;
    //"<turn glyph><degrees><degree ring>" for the corner pill, or "" within the band: the glyph turns the way to catch
    //up (RemoteScreens' rich text: 0x02 counter-clockwise, 0x04 clockwise, 0x01 the degree ring)
    inline void turn(char* out, size_t size, float turnError){
        if(isnan(turnError) || fabsf(turnError) <= turnBand){ out[0] = 0; return; }
        snprintf(out, size, "%c%d\x01", turnError > 0.0f ? '\x02' : '\x04', int(fabsf(turnError) + 0.5f));
    }


    static constexpr float onTimeBand = 0.3f;       //s: nothing shown within +-0.3 s
    static constexpr float blinkBeyond = 2.0f;      //s: the pill blinks past 2 s

    enum class Word : uint8_t{ ON_TIME, FASTER, SLOWER };

    //delta = elapsed - planned (s): late positive. At the band's edge still on time
    inline Word classify(float delta){
        if(delta > onTimeBand) return Word::FASTER;
        if(delta < -onTimeBand) return Word::SLOWER;
        return Word::ON_TIME;
    }
    inline bool blinks(float delta){ return fabsf(delta) > blinkBeyond; }

    //the pace pill: "FASTER" (late), "SLOWER" (early), nullptr on time or without a reading
    inline const char* word(float delta){
        if(isnan(delta)) return nullptr;
        Word w = classify(delta);
        return w == Word::FASTER ? "FASTER" : (w == Word::SLOWER ? "SLOWER" : nullptr);
    }

    //"+0.8s", "-0.5s", "+12s" from 10 s, "+99s" at most
    inline void seconds(char* out, size_t size, float delta){
        float a = fabsf(delta);
        const char* sign = delta < 0.0f ? "-" : "+";
        if(a < 9.95f) snprintf(out, size, "%s%.1fs", sign, a);
        else snprintf(out, size, "%s%ds", sign, a > 99.0f ? 99 : int(a + 0.5f));
    }

    //an arrival: "+0.4s" late, "-0.6s" early
    inline void split(char* out, size_t size, const RunSplit& s){ seconds(out, size, s.delta); }

    //the time left in s, rounded up as a countdown: "4.2" below 10 s, "12" whole seconds from there, "0.0" once over
    inline void timeLeft(char* out, size_t size, float t){
        if(isnan(t) || t < 0.0f) t = 0.0f;
        long tenths = long(ceilf(t * 10.0f - 1e-3f));
        if(tenths < 100) snprintf(out, size, "%ld.%ld", tenths / 10, tenths % 10);
        else snprintf(out, size, "%ld", long(ceilf(t - 1e-3f)));
    }

    //a refused play, for the pill: "SELECT A MARK", "ZERO HAS NO TIME" (spelt out: Z and 2 look alike in the 5 x 7
    //font), "3 HAS NO TIME", "NO POSITION", "ALREADY AT 3"
    inline void refusal(char* out, size_t size, RunRefusal why, uint8_t mark){
        char id = mark == 0 ? 'Z' : char('0' + mark);
        switch(why){
            case RunRefusal::NO_MARK: snprintf(out, size, "SELECT A MARK"); break;
            case RunRefusal::ZERO: snprintf(out, size, "ZERO HAS NO TIME"); break;
            case RunRefusal::NO_TIME: snprintf(out, size, "%c HAS NO TIME", id); break;
            case RunRefusal::NO_POSE: snprintf(out, size, "NO POSITION"); break;
            case RunRefusal::ALREADY_THERE: snprintf(out, size, "ALREADY AT %c", id); break;
            default: if(size) out[0] = 0; break;
        }
    }
};
