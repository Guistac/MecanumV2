#pragma once
#include <stdint.h>
#include <stdio.h>
#include <stddef.h>
#include <limits.h>
#include <stdint.h>

//The rehearsal stopwatch on the main screen [D-160]: display only, nothing of it is sent. Counts up, or down from a
//set time (whole seconds, 0:00 to 99:59, 0:00 = plain counting up) and on past zero into negative time until stopped by
//hand (nothing stops it automatically). RAM only: the set time is kept
//until the remote is switched off. Times in ms from the board's millis()
class Stopwatch{
public:

    enum class State : uint8_t{ IDLE, RUNNING, STOPPED };

    static constexpr uint32_t stepMillis = 1000;
    static constexpr uint32_t maxSetMillis = 99 * 60000 + 59000;     //99:59

    void start(uint32_t now){
        if(state == State::RUNNING) return;
        startMillis = now;
        state = State::RUNNING;
    }

    void stop(uint32_t now){
        if(state != State::RUNNING) return;
        accumulatedMillis += now - startMillis;
        state = State::STOPPED;
    }

    //to 0:00, or to the set countdown, and stopped
    void reset(){
        state = State::IDLE;
        accumulatedMillis = 0;
        b_timeUpDone = false;
    }

    //the countdown's start (clamped to 0..99:59, whole seconds); resets the timer. Returns the time set
    uint32_t setCountdown(int32_t millis){
        if(millis < 0) millis = 0;
        if(uint32_t(millis) > maxSetMillis) millis = int32_t(maxSetMillis);
        setMillis = uint32_t(millis) / stepMillis * stepMillis;
        reset();
        return setMillis;
    }

    //every loop: true once when a running countdown reaches zero (not again until reset)
    bool update(uint32_t now){
        if(state != State::RUNNING || setMillis == 0 || b_timeUpDone) return false;
        if(elapsed(now) < setMillis) return false;
        b_timeUpDone = true;
        return true;
    }

    State getState() const { return state; }
    bool isRunning() const { return state == State::RUNNING; }
    uint32_t getSetMillis() const { return setMillis; }
    bool isCountdown() const { return setMillis > 0; }

    uint32_t elapsed(uint32_t now) const {
        return accumulatedMillis + (state == State::RUNNING ? now - startMillis : 0);
    }
    bool isOvertime(uint32_t now) const { return isCountdown() && elapsed(now) >= setMillis; }
    uint32_t remaining(uint32_t now) const {
        uint32_t e = elapsed(now);
        return isCountdown() && e < setMillis ? setMillis - e : 0;
    }
    uint32_t overtime(uint32_t now) const {
        uint32_t e = elapsed(now);
        return isCountdown() && e > setMillis ? e - setMillis : 0;
    }

    //m:ss.t below 10 minutes, mm:ss from 10 minutes (tenths only with b_tenths), "-" in front for overtime (the
    //countdown run past zero: negative time)
    static void format(char* out, size_t size, uint32_t millis, bool b_tenths, bool b_negative = false){
        uint32_t tenths = millis / 100;
        uint32_t seconds = tenths / 10;
        uint32_t minutes = seconds / 60;
        const char* plus = b_negative ? "-" : "";
        if(b_tenths && minutes < 10) snprintf(out, size, "%s%lu:%02lu.%lu", plus, (unsigned long)minutes, (unsigned long)(seconds % 60), (unsigned long)(tenths % 10));
        else snprintf(out, size, "%s%lu:%02lu", plus, (unsigned long)minutes, (unsigned long)(seconds % 60));
    }

private:
    State state = State::IDLE;
    uint32_t startMillis = 0;
    uint32_t accumulatedMillis = 0;
    uint32_t setMillis = 0;
    bool b_timeUpDone = false;
};

//What the screen shows of the stopwatch (ScreenModel), made by StopwatchControl::view
struct StopwatchView{
    Stopwatch::State state = Stopwatch::State::IDLE;
    bool b_editing = false;         //the set time being edited: SET m:ss in a pill
    bool b_countdown = false;       //a set time: the bar across the screen shrinking with the time left
    bool b_overtime = false;        //a countdown past zero: negative time, -m:ss.t
    uint32_t setMillis = 0;
    uint32_t shownMillis = 0;       //count up: elapsed; countdown: remaining (rounded up to the tenth); overtime: past zero
    float remainingFraction = 1.0f; //countdown: the bar, 1 full .. 0 empty
};

//The stopwatch's controls, the decision logic kept off the board so it can be tested on the host [D-160]. Only while
//the board says they are active: the Status page shows, no menu, no games, no calibration, the sticks zeroed, no alarm
//and no sticks to release. Both stick buttons together do nothing: a press acts only after bothWindowMillis (or on
//its release, if sooner) without the other button, and a press while the other one is held is ignored. The time
//taken is the press's, so starting and stopping lose nothing to the wait.
//  right click: start / stop (stopped: start continues)
//  left click: reset (to 0:00 or the set countdown) and stop
//  edit mode: the left button held with the speed switch at OFF (the timer is stopped by the left press anyway):
//    right stick up / down +/- 1 s (first step on the push, repeats after 400 ms every 150 ms, every 80 ms after
//    1.5 s held, always 1 s), right / left +/- 1 min (repeats after 400 ms every 200 ms); a diagonal push counts as
//    its stronger axis. Right stick click clears to 0:00 (count up). 0:00 to 99:59: a step past either end stays
//    there. Letting go of the left button (or the switch leaving OFF) ends it; the timer then stands at the new time
class StopwatchControl{
public:

    static constexpr uint32_t bothWindowMillis = 50;
    static constexpr float stepThreshold = 0.6f;
    static constexpr float rearmThreshold = 0.3f;
    static constexpr uint32_t firstRepeatMillis = 400;
    static constexpr uint32_t secondsRepeatMillis = 150;
    static constexpr uint32_t secondsFastAfterMillis = 1500;
    static constexpr uint32_t secondsFastRepeatMillis = 80;
    static constexpr uint32_t minutesRepeatMillis = 200;
    static constexpr uint32_t minuteMillis = 60000;

    struct Inputs{
        bool b_active = false;      //the Status page with the controls allowed (see above)
        bool b_leftHeld = false;    //left stick button
        bool b_rightHeld = false;   //right stick button
        bool b_speedOff = false;    //speed switch at OFF
        float rightX = 0.0f;        //right stick, -1..1, up positive
        float rightY = 0.0f;
        uint32_t now = 0;
    };

    //update's result: one bit per thing that happened this loop (the board plays the sounds)
    enum Event : uint16_t{
        NONE = 0,
        STARTED = 1 << 0,
        STOPPED = 1 << 1,
        RESET = 1 << 2,
        EDIT_ENTERED = 1 << 3,
        EDIT_LEFT = 1 << 4,
        SET_CHANGED = 1 << 5,       //a step that changed the time
        SET_LIMIT = 1 << 6,         //a step at 99:59 or at 0:00 (nothing changed)
        SET_CLEARED = 1 << 7,       //cleared to 0:00: the right stick clicked, or stepped down to it
        TIME_UP = 1 << 8,           //a running countdown reached zero (once)
    };

    uint16_t update(const Inputs& in){
        uint16_t events = NONE;
        bool b_leftEdge = in.b_leftHeld && !b_leftWas;
        bool b_rightEdge = in.b_rightHeld && !b_rightWas;
        b_leftWas = in.b_leftHeld;
        b_rightWas = in.b_rightHeld;

        if(!in.b_active){
            //another page, the menu, the games...: nothing acts, edit mode ends silently, a held button stays spent
            b_leftPending = false;
            b_rightPending = false;
            b_leftArmed = false;
            b_editing = false;
            b_sticksBusy = false;
            if(watch.update(in.now)) events |= TIME_UP;
            return events;
        }

        //press edges: only with the other button up
        if(b_leftEdge && !in.b_rightHeld && !b_rightPending){ b_leftPending = true; leftPressMillis = in.now; }
        if(b_rightEdge && !in.b_leftHeld && !b_leftPending){ b_rightPending = true; rightPressMillis = in.now; }
        if(b_leftEdge && b_rightEdge){ b_leftPending = false; b_rightPending = false; }
        //the other one pressed within the window: both, nothing
        if(b_leftPending && in.b_rightHeld){ b_leftPending = false; b_rightPending = false; }
        if(b_rightPending && in.b_leftHeld){ b_leftPending = false; b_rightPending = false; }

        if(b_rightPending && (!in.b_rightHeld || in.now - rightPressMillis >= bothWindowMillis)){
            b_rightPending = false;
            if(watch.isRunning()){ watch.stop(rightPressMillis); events |= STOPPED; }
            else{ watch.start(rightPressMillis); events |= STARTED; }
        }
        if(b_leftPending && (!in.b_leftHeld || in.now - leftPressMillis >= bothWindowMillis)){
            b_leftPending = false;
            watch.reset();
            events |= RESET;
            b_leftArmed = in.b_leftHeld;
        }
        if(!in.b_leftHeld) b_leftArmed = false;

        //edit mode while the reset press is held at OFF
        bool b_canEdit = b_leftArmed && in.b_speedOff && !watch.isRunning();
        if(b_canEdit && !b_editing){
            b_editing = true;
            events |= EDIT_ENTERED;
            //a stick already out when editing starts does nothing until it has come back
            float y, x;
            dominant(in, y, x);
            seconds.start(y);
            minutes.start(x);
        }
        else if(!b_canEdit && b_editing){
            b_editing = false;
            events |= EDIT_LEFT;
        }
        if(b_editing){
            float y, x;
            dominant(in, y, x);
            int8_t secondStep = seconds.update(y, in.now, secondsRepeatMillis, secondsFastAfterMillis, secondsFastRepeatMillis);
            int8_t minuteStep = minutes.update(x, in.now, minutesRepeatMillis, UINT32_MAX, minutesRepeatMillis);
            int32_t step = secondStep * int32_t(Stopwatch::stepMillis) + minuteStep * int32_t(minuteMillis);
            if(secondStep != 0 || minuteStep != 0){
                uint32_t before = watch.getSetMillis();
                uint32_t after = watch.setCountdown(int32_t(before) + step);
                if(after == before) events |= SET_LIMIT;
                else if(after == 0) events |= SET_CLEARED;
                else events |= SET_CHANGED;
            }
            //the right stick's click clears (its press edge; the left button is held, so it is no start / stop)
            if(b_rightEdge){
                watch.setCountdown(0);
                events |= SET_CLEARED;
            }
        }
        //the grind and the OFF blink stay quiet during the edit hold only (the right stick has a job there)
        b_sticksBusy = b_editing;

        if(watch.update(in.now)) events |= TIME_UP;
        return events;
    }

    bool isEditing() const { return b_editing; }
    bool sticksBusy() const { return b_sticksBusy; }
    const Stopwatch& stopwatch() const { return watch; }

    StopwatchView view(uint32_t now) const {
        StopwatchView v;
        v.state = watch.getState();
        v.b_editing = b_editing;
        v.b_countdown = watch.isCountdown();
        v.b_overtime = watch.isOvertime(now);
        v.setMillis = watch.getSetMillis();
        if(!v.b_countdown) v.shownMillis = watch.elapsed(now);
        else if(v.b_overtime) v.shownMillis = watch.overtime(now);
        else{
            uint32_t r = watch.remaining(now);
            v.shownMillis = (r + 99) / 100 * 100;
            v.remainingFraction = float(r) / float(v.setMillis);
        }
        if(v.b_overtime) v.remainingFraction = 0.0f;
        return v;
    }

private:
    Stopwatch watch;
    bool b_leftWas = false;
    bool b_rightWas = false;
    bool b_leftPending = false;
    bool b_rightPending = false;
    uint32_t leftPressMillis = 0;
    uint32_t rightPressMillis = 0;
    bool b_leftArmed = false;       //the left button's press was taken (a reset): holding it may edit
    bool b_editing = false;
    bool b_sticksBusy = false;

    //one stick axis of the set time: a step on the push (60 %), re-armed under 30 %, repeated while held: the first
    //repeat after firstRepeatMillis, then every repeatMillis, every fastRepeatMillis once held fastAfterMillis
    struct Repeater{
        int8_t direction = 0;
        bool b_waitForRelease = false;
        uint32_t holdStartMillis = 0;
        uint32_t nextMillis = 0;

        void start(float value){
            direction = 0;
            b_waitForRelease = value >= rearmThreshold || value <= -rearmThreshold;
        }

        //returns -1, 0 or +1
        int8_t update(float value, uint32_t now, uint32_t repeatMillis, uint32_t fastAfterMillis, uint32_t fastRepeatMillis){
            if(b_waitForRelease){
                if(value < rearmThreshold && value > -rearmThreshold) b_waitForRelease = false;
                return 0;
            }
            if(direction != 0){
                bool b_released = direction > 0 ? value < rearmThreshold : value > -rearmThreshold;
                if(b_released){ direction = 0; return 0; }
                if(int32_t(now - nextMillis) < 0) return 0;
                nextMillis = now + (now - holdStartMillis >= fastAfterMillis ? fastRepeatMillis : repeatMillis);
                return direction;
            }
            if(value >= stepThreshold) direction = 1;
            else if(value <= -stepThreshold) direction = -1;
            else return 0;
            holdStartMillis = now;
            nextMillis = now + firstRepeatMillis;
            return direction;
        }
    };
    Repeater seconds;   //right stick up / down
    Repeater minutes;   //right stick right / left

    //a diagonal push counts as its stronger axis, the other one as centred
    static void dominant(const Inputs& in, float& y, float& x){
        float ay = in.rightY < 0 ? -in.rightY : in.rightY;
        float ax = in.rightX < 0 ? -in.rightX : in.rightX;
        y = ay >= ax ? in.rightY : 0.0f;
        x = ax > ay ? in.rightX : 0.0f;
    }
};
