#pragma once
#include <stdint.h>
#include <math.h>
#include "MarkStore.h"
#include "StickClick.h"
#include "StickRepeater.h"
#include <StickFlick.h>

//The Tracking page's controls (owner, 2026-10-28), the decision logic without the board, host tested. Display only:
//the stick buttons and the sticks go out on the uplink as before. Two modes by the speed switch:
//
//DRIVE MODE (SLOW / FAST: the sticks drive, so only the stick buttons act)
//  right click       PLAY a timed run to the selected mark, or during a run pause / resume it (TargetRun,
//                    TimedTarget.h: the plan, the refusals, the stopwatch)
//  left hold 1 s     during a run REWIND: the run cancelled, the stopwatch to 0:00 or its set countdown, stopped (as
//                    the left click resets the stopwatch on Status and Motion); between runs the previous stored mark
//  left click        zoom (the next step)
//  right hold 1 s    select the next stored mark (empty ones skipped, wrapping through Z); during a run the selection is
//                    the run's target and the hold does nothing; it works again as soon as the run ended
//EDIT MODE (OFF: the vehicle cannot move, the sticks are free)
//  left stick up / down       select the mark: none, Z, 1..9 (up: the one before, wrapping)
//  left stick left / right    the view: map -> mark list -> trip readout
//  left click        SNAP to the selected mark: "AT 3? HOLD L", confirmed only by HOLDING the left click 1 s (owner,
//                    2026-10-28: as hard to do by accident as an overwrite)
//  left hold 1 s     clear the trail (no question)
//  right click       STORE the pose here into the selected mark: "SAVE 3?" (an empty mark: a right click within 3 s
//                    confirms) or "OVERWRITE 3?" (blinking: confirmed only by HOLDING the right click 1 s, the press
//                    starting within 3 s). The stopwatch running: the mark takes its elapsed time as its duration
//                    (teach-in: how long the drive took since the stopwatch last started from 0:00)
//  right hold 1 s    the NUMBER EDITOR for the selected mark (not Z)
//  in the editor     left stick up / down: the field (x, y, h, t); left stick left: blank the field (h and t only);
//                    right stick up / down: +-1 cm / 1 deg / 0.1 s (repeats like the stopwatch's setter, faster
//                    held), right / left: +-1 m / 10 deg / 1 s; right click saves; left click leaves without saving;
//                    left hold 1 s asks "CLEAR 3?", a second 1 s hold clears the mark
//  a pushbutton, the other stick button, a stick flicked, or 3 s without the answer cancel a question; a pushbutton
//  in the editor leaves it without saving. A pushbutton that cancels is spent (no page change). Leaving edit mode
//  (the switch, the page) cancels everything without a sound.
//A click acts on its release (before 1 s), a hold once at 1 s; only a press that starts in the mode counts.

//what the screen shows (ScreenModel)
struct TrackingControlView{
    enum class Question : uint8_t{ NONE, AT, SAVE, OVERWRITE, CLEAR, CLEAR_ALL };
    enum class View : uint8_t{ MAP, LIST, TRIP };
    enum class Field : uint8_t{ X, Y, H, T };

    bool b_editMode = false;
    int8_t selected = -1;               //-1 none, 0 Z, 1..9
    View view = View::MAP;
    Question question = Question::NONE;
    uint8_t questionMark = 0;
    float holdProgress = 0.0f;          //the confirming hold (OVERWRITE, CLEAR), 0..1
    bool b_editing = false;
    uint8_t editMark = 0;               //the mark being edited (1..9)
    Mark draft;                         //the editor's values
    Field field = Field::X;
};

class TrackingControl{
public:
    using Question = TrackingControlView::Question;
    using View = TrackingControlView::View;
    using Field = TrackingControlView::Field;

    static constexpr uint32_t questionMillis = 3000;
    static constexpr uint32_t fineRepeatMillis = 150;
    static constexpr uint32_t fineFastAfterMillis = 1500;
    static constexpr uint32_t fineFastRepeatMillis = 80;
    static constexpr uint32_t coarseRepeatMillis = 200;
    static constexpr uint32_t coarseFastAfterMillis = 1500;     //held: faster, as the fine steps
    static constexpr uint32_t coarseFastRepeatMillis = 80;

    struct Inputs{
        bool b_active = false;          //the Tracking page with its controls allowed
        bool b_speedOff = false;        //edit mode
        float sticks[4] = {0, 0, 0, 0}; //left X, left Y, right X, right Y; y positive up
        bool b_leftHeld = false;
        bool b_rightHeld = false;
        bool b_pushbutton = false;      //either pushbutton pressed this loop (its edge)
        bool b_poseKnown = false;       //the pose in the display frame
        float x = NAN, y = NAN, heading = NAN;
        bool b_watchRunning = false;    //the stopwatch: its elapsed time for the teach-in
        uint32_t watchMillis = 0;
        bool b_run = false;             //a timed run plays or is paused (TargetRun)
        int8_t runTarget = -1;          //during a run: its target (1..9), -1 none
        uint32_t now = 0;
    };

    enum Event : uint32_t{
        NONE = 0,
        SELECTED = 1u << 0,
        VIEW_CHANGED = 1u << 1,
        ASKED = 1u << 2,
        SNAP = 1u << 3,                 //confirmed: snapMark() is the mark
        STORED = 1u << 4,               //a mark stored (confirmed)
        EDIT_OPENED = 1u << 5,
        EDIT_SAVED = 1u << 6,
        EDIT_LEFT = 1u << 7,            //left without saving
        MARK_CLEARED = 1u << 8,
        CANCELLED = 1u << 9,
        TIMED_OUT = 1u << 10,
        REFUSED = 1u << 11,             //nothing to act on (no pose, an empty mark, Z)
        STEP = 1u << 12,                //an editor value changed
        STEP_LIMIT = 1u << 13,          //a step at a limit (nothing changed)
        FIELD = 1u << 14,
        ZOOM = 1u << 15,
        CLEAR_TRAIL = 1u << 16,
        PLAY_PAUSE = 1u << 17,          //the right click in drive mode: play, or pause / resume at pressMillis()
        REWIND = 1u << 18,
        BUTTON_SPENT = 1u << 19,        //the pushbutton cancelled something: no page change
        MARKS_CHANGED = 1u << 20,       //the store changed: write it
        ALL_CLEARED = 1u << 21,         //every mark 1..9 cleared (confirmed in the list view)
    };

    explicit TrackingControl(MarkStore& store) : marks(store) {}

    uint32_t update(const Inputs& in){
        uint32_t events = NONE;
        uint8_t mode = !in.b_active ? 0 : (in.b_speedOff ? 2 : 1);
        if(mode != lastMode){
            //a press, a flick or a question never carries over into another mode
            left.spend();
            right.spend();
            flick.disarm();
            question = Question::NONE;
            b_editing = false;
            lastMode = mode;
        }
        StickClick::Result l = left.update(mode != 0, in.b_leftHeld, in.now);
        StickClick::Result r = right.update(mode != 0, in.b_rightHeld, in.now);
        float leftOnly[4] = {in.sticks[0], in.sticks[1], 0.0f, 0.0f};
        Flick f = mode == 2 ? flick.update(leftOnly) : Flick::NONE;
        if(mode == 1) events |= drive(in, l, r);
        else if(mode == 2) events |= b_editing ? editor(in, l, r, f) : edit(in, l, r, f);
        if(mode == 1 && in.b_run && in.runTarget >= 0) selected = in.runTarget;
        return events;
    }

    TrackingControlView view(uint32_t now) const {
        TrackingControlView v;
        v.b_editMode = lastMode == 2;
        v.selected = selected;
        v.view = shownView;
        v.question = question;
        v.questionMark = questionMark;
        if(question == Question::OVERWRITE) v.holdProgress = right.holdProgress(now);
        if(question == Question::CLEAR || question == Question::AT || question == Question::CLEAR_ALL) v.holdProgress = left.holdProgress(now);
        v.b_editing = b_editing;
        v.editMark = editMark;
        v.draft = draft;
        v.field = field;
        return v;
    }

    int8_t getSelected() const { return selected; }
    uint8_t snapMark() const { return snapTarget; }
    uint32_t pressMillis() const { return playPressMillis; }
    bool isEditing() const { return b_editing; }
    bool isAsking() const { return question != Question::NONE; }
    View getView() const { return shownView; }

private:
    MarkStore& marks;
    StickClick left, right;
    StickFlick flick;
    StickRepeater fine, coarse;
    uint8_t lastMode = 0;
    int8_t selected = -1;
    View shownView = View::MAP;
    Question question = Question::NONE;
    uint8_t questionMark = 0;
    uint32_t askedMillis = 0;
    bool b_editing = false;
    uint8_t editMark = 0;
    Mark draft;
    Field field = Field::X;
    uint8_t snapTarget = 0;
    uint32_t playPressMillis = 0;

    uint32_t drive(const Inputs& in, StickClick::Result l, StickClick::Result r){
        uint32_t events = NONE;
        if(r == StickClick::CLICK){ playPressMillis = right.pressedAt(); events |= PLAY_PAUSE; }
        //the left hold: during a run it cancels it (rewind); between runs it selects the previous stored mark, as the
        //right hold selects the next one (owner, 2026-10-28)
        if(l == StickClick::HOLD){
            if(in.b_run) events |= REWIND;
            else{ selected = stepStored(selected, -1); events |= SELECTED; }
        }
        if(l == StickClick::CLICK) events |= ZOOM;
        if(r == StickClick::HOLD && !in.b_run){
            selected = stepStored(selected, 1);
            events |= SELECTED;
        }
        return events;
    }

    //the next (+1) or previous (-1) stored mark from k, wrapping through Z (always stored); none (-1) counts as
    //before Z
    int8_t stepStored(int8_t k, int8_t direction) const {
        for(uint8_t i = 0; i < MarkStore::count; i++){
            if(k < 0) k = direction > 0 ? 0 : int8_t(MarkStore::count - 1);
            else k = int8_t((k + direction + MarkStore::count) % MarkStore::count);
            if(marks.isUsed(uint8_t(k))) return k;
        }
        return k;
    }

    void ask(Question q, uint8_t k, uint32_t now){
        question = q;
        questionMark = k;
        askedMillis = now;
    }

    uint32_t cancel(uint32_t reason){
        question = Question::NONE;
        return reason;
    }

    //a question pending: the 3 s, unless the confirming hold is under way (or completed this loop)
    bool timedOut(uint32_t now, StickClick::Result l, StickClick::Result r) const {
        bool b_holding = (question == Question::OVERWRITE && (right.isPressing() || r == StickClick::HOLD))
                      || ((question == Question::CLEAR || question == Question::AT || question == Question::CLEAR_ALL)
                          && (left.isPressing() || l == StickClick::HOLD));
        return !b_holding && now - askedMillis >= questionMillis;
    }

    uint32_t edit(const Inputs& in, StickClick::Result l, StickClick::Result r, Flick f){
        uint32_t events = NONE;
        if(question != Question::NONE){
            if(in.b_pushbutton) return cancel(CANCELLED | BUTTON_SPENT);
            if(f != Flick::NONE) return cancel(CANCELLED);
            if(timedOut(in.now, l, r)) return cancel(TIMED_OUT);
            if(question == Question::CLEAR_ALL){
                if(r == StickClick::PRESSED){ right.spend(); return cancel(CANCELLED); }
                if(l != StickClick::HOLD) return NONE;
                left.spend();
                question = Question::NONE;
                bool b_changed = false;
                for(uint8_t k = 1; k < MarkStore::count; k++) if(marks.clear(k)) b_changed = true;
                return ALL_CLEARED | (b_changed ? MARKS_CHANGED : NONE);
            }
            if(question == Question::AT){
                if(r == StickClick::PRESSED){ right.spend(); return cancel(CANCELLED); }
                if(l == StickClick::HOLD){
                    left.spend();
                    snapTarget = questionMark;
                    return cancel(SNAP);
                }
                return NONE;
            }
            //SAVE / OVERWRITE
            if(l == StickClick::PRESSED){ left.spend(); return cancel(CANCELLED); }
            bool b_confirmed = question == Question::SAVE ? (r == StickClick::CLICK || r == StickClick::HOLD) : r == StickClick::HOLD;
            if(!b_confirmed) return NONE;
            right.spend();
            uint8_t k = questionMark;
            question = Question::NONE;
            if(!in.b_poseKnown) return REFUSED;
            Mark m = Mark::fromPose(in.x, in.y, in.heading, in.b_watchRunning ? int32_t(in.watchMillis) : -1);
            if(marks.set(k, m)) events |= MARKS_CHANGED;
            return events | STORED;
        }
        if(f == Flick::UP || f == Flick::DOWN){
            //none, Z, 1..9 as 0..10
            int8_t i = int8_t(selected + 1);
            i = int8_t((i + (f == Flick::DOWN ? 1 : 10)) % 11);
            selected = int8_t(i - 1);
            return SELECTED;
        }
        if(f == Flick::LEFT || f == Flick::RIGHT){
            uint8_t v = (uint8_t(shownView) + (f == Flick::RIGHT ? 1 : 2)) % 3;
            shownView = View(v);
            return VIEW_CHANGED;
        }
        if(l == StickClick::CLICK){
            if(selected < 0 || !marks.isUsed(uint8_t(selected)) || !in.b_poseKnown) return REFUSED;
            ask(Question::AT, uint8_t(selected), in.now);
            return ASKED;
        }
        //the list view: the left hold asks to clear every mark (owner, 2026-10-28), confirmed by a second 1 s hold; on
        //the map it clears the trail
        if(l == StickClick::HOLD && shownView == View::LIST){
            ask(Question::CLEAR_ALL, 0, in.now);
            return ASKED;
        }
        if(l == StickClick::HOLD) return CLEAR_TRAIL;
        if(r == StickClick::CLICK){
            if(selected <= 0 || !in.b_poseKnown) return REFUSED;
            ask(marks.isUsed(uint8_t(selected)) ? Question::OVERWRITE : Question::SAVE, uint8_t(selected), in.now);
            return ASKED;
        }
        if(r == StickClick::HOLD){
            if(selected <= 0) return REFUSED;
            openEditor(in);
            return EDIT_OPENED;
        }
        return NONE;
    }

    void openEditor(const Inputs& in){
        b_editing = true;
        editMark = uint8_t(selected);
        field = Field::X;
        if(marks.isUsed(editMark)) draft = marks.get(editMark);
        else if(in.b_poseKnown) draft = Mark::fromPose(in.x, in.y, in.heading);
        else{
            draft = Mark();
            draft.b_used = true;
        }
        fine.start(in.sticks[3]);
        coarse.start(in.sticks[2]);
    }

    uint32_t editor(const Inputs& in, StickClick::Result l, StickClick::Result r, Flick f){
        if(question == Question::CLEAR){
            if(in.b_pushbutton) return cancel(CANCELLED | BUTTON_SPENT);
            if(f != Flick::NONE) return cancel(CANCELLED);
            if(timedOut(in.now, l, r)) return cancel(TIMED_OUT);
            if(r == StickClick::PRESSED){ right.spend(); return cancel(CANCELLED); }
            if(l != StickClick::HOLD) return NONE;
            question = Question::NONE;
            b_editing = false;
            uint32_t events = MARK_CLEARED;
            if(marks.clear(editMark)) events |= MARKS_CHANGED;
            return events;
        }
        if(in.b_pushbutton){
            b_editing = false;
            return EDIT_LEFT | BUTTON_SPENT;
        }
        if(r == StickClick::CLICK){
            b_editing = false;
            uint32_t events = EDIT_SAVED;
            if(marks.set(editMark, draft)) events |= MARKS_CHANGED;
            return events;
        }
        if(l == StickClick::CLICK){
            b_editing = false;
            return EDIT_LEFT;
        }
        if(l == StickClick::HOLD){
            if(!marks.isUsed(editMark)) return REFUSED;
            ask(Question::CLEAR, editMark, in.now);
            return ASKED;
        }
        if(f == Flick::UP || f == Flick::DOWN){
            field = Field((uint8_t(field) + (f == Flick::DOWN ? 1 : 3)) % 4);
            return FIELD;
        }
        if(f == Flick::LEFT){
            if(field == Field::H && draft.b_heading){ draft.b_heading = false; draft.headingTenths = 0; return STEP; }
            if(field == Field::T && draft.b_time){ draft.b_time = false; draft.timeTenths = 0; return STEP; }
            return STEP_LIMIT;
        }
        //the right stick: the stronger axis only
        float ry = in.sticks[3], rx = in.sticks[2];
        float ay = fabsf(ry), ax = fabsf(rx);
        int8_t fineStep = fine.update(ay >= ax ? ry : 0.0f, in.now, fineRepeatMillis, fineFastAfterMillis, fineFastRepeatMillis);
        int8_t coarseStep = coarse.update(ax > ay ? rx : 0.0f, in.now, coarseRepeatMillis, coarseFastAfterMillis, coarseFastRepeatMillis);
        if(fineStep == 0 && coarseStep == 0) return NONE;
        return step(fineStep, coarseStep) ? STEP : STEP_LIMIT;
    }

    //one step of the field: x, y 10 mm / 1000 mm; h 1 deg / 10 deg (wrapping); t 0.1 s / 1 s (0..99:59.9). A blank
    //heading or time starts from 0
    bool step(int8_t fineStep, int8_t coarseStep){
        switch(field){
            case Field::X:
            case Field::Y:{
                int32_t& v = field == Field::X ? draft.xMm : draft.yMm;
                int32_t next = Mark::clampMm(v + fineStep * 10 + coarseStep * 1000);
                if(next == v) return false;
                v = next;
                return true;
            }
            case Field::H:{
                if(!draft.b_heading){ draft.b_heading = true; draft.headingTenths = 0; }
                draft.headingTenths = Mark::wrapTenths(int32_t(draft.headingTenths) + fineStep * 10 + coarseStep * 100);
                return true;
            }
            case Field::T:{
                bool b_was = draft.b_time;
                if(!draft.b_time){ draft.b_time = true; draft.timeTenths = 0; }
                int32_t next = int32_t(draft.timeTenths) + fineStep + coarseStep * 10;
                if(next < 0) next = 0;
                if(next > Mark::maxTimeTenths) next = Mark::maxTimeTenths;
                bool b_changed = next != draft.timeTenths || !b_was;
                draft.timeTenths = uint16_t(next);
                return b_changed;
            }
        }
        return false;
    }
};
