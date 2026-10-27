#pragma once
#include <stdint.h>

//Remote: holds the transmitted sticks at neutral while the settings menu is open, and after it closes or after any
//boot until both sticks have been released to their rest position [D-147 points 2 and 7]
class StickGuard{
public:

    enum class Release{ NONE, LEFT, RIGHT, BOTH };

    //every loop: b_leftCentred / b_rightCentred when the stick reads near its rest position
    void update(bool b_menuOpen, bool b_leftCentred, bool b_rightCentred){
        b_menu = b_menuOpen;
        b_left = b_leftCentred;
        b_right = b_rightCentred;
        if(b_menuOpen) b_waitForCentre = true;
        else if(b_waitForCentre && b_leftCentred && b_rightCentred){
            b_waitForCentre = false;
            b_released = true;
        }
    }

    bool holdNeutral() const { return b_waitForCentre; }

    //true once each time the hold ends (at boot: capture the joystick zero then)
    bool wasReleased(){
        bool released = b_released;
        b_released = false;
        return released;
    }

    //which stick the operator has to release, NONE while the menu is open or nothing is held
    Release releaseNeeded() const {
        if(!b_waitForCentre || b_menu) return Release::NONE;
        if(!b_left && !b_right) return Release::BOTH;
        if(!b_left) return Release::LEFT;
        if(!b_right) return Release::RIGHT;
        return Release::NONE;
    }

private:
    bool b_waitForCentre = true;
    bool b_released = false;
    bool b_menu = false;
    bool b_left = false;
    bool b_right = false;
};
