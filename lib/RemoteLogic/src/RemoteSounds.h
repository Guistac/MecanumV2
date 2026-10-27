#pragma once
#include <stdint.h>
#include <Sound.h>
#include "StatusView.h"

//The remote's own sounds [D-158], played through the shared sound engine (lib/RemoteAudio). Each plays once. The
//Sound setting picks what plays: All (navigation, presses, status, warnings), Alarms only (status changes and
//warnings, the default) or Off (nothing, not even the startup tune). Status changes follow the state the receiver
//reports, never the button press. Two families that cannot be mistaken for each other: interface sounds are one
//short plain square on the C pentatonic (C D E G A); status sounds and warnings are several notes off that scale
//(B major, F sharp), with a fading synth echo, or rough two-leg buzzes (DUET). Status and warnings have alarm
//priority, never cut off by an interface sound.
namespace RemoteSounds{

    //the Sound setting
    enum class Level : uint8_t{ ALL, ALARMS, OFF };
    static const uint8_t levelCount = 3;
    static const uint8_t defaultLevel = uint8_t(Level::ALARMS);
    const char* levelLabel(uint8_t level);

    enum class Sound : uint8_t{
        NONE,
        CLICK,          //page change, menu item moved, menu opened
        VALUE,          //a menu value changed
        BUMP,           //the end of a list, a value at its limit
        SAVED,          //settings or calibration saved: two rising notes
        DISCARDED,      //menu left without saving: two falling notes
        PRESS,          //enable or disable pressed
        REFUSED,        //enable pressed while it cannot work (E-stop, no link, sticks to release): a low buzz
        STATUS_ENABLING,        //status: a quick rising pair ("arming")
        STATUS_ENABLED,         //a bright B major arpeggio with an echo
        STATUS_DISABLING,       //a quick falling pair
        STATUS_DISABLED,        //falling fifths to a low B, an echo
        STATUS_ESTOP,           //an E-stop engaged (own, external or the vehicle stopping): noise, then low
                                //beating buzzes stepping down, deliberately harsh
        STATUS_ESTOP_RELEASED,  //a calm rise, an echo
        STATUS_LINK_FOUND,      //the receiver answers again: two pings rising, an echo
        WARNING,                //link lost, radio fault, E-stop wiring, battery: three rough two-leg beeps
        SPEED_OFF,      //the speed switch to OFF (sticks off): low; to slow: middle; to fast: high
        SPEED_SLOW,
        SPEED_FAST,
        DIRECTION_SET,  //the direction switch on: two rising notes; off: two falling
        DIRECTION_CLEAR,
        SEARCHING,      //NO LINK, every 2 s with the LEDs: a low ping with the left LED flash, a higher one with the
                        //right, then three fading echoes
        COUNT
    };

    bool isAlarm(Sound sound);
    bool isAllowed(Sound sound, uint8_t level);
    //the games play unless the setting is Off
    bool gamesAllowed(uint8_t level);

    //the sound's cue for the engine (NONE: an empty cue)
    SoundCue cue(Sound sound);

    //the sound for a change of the main screen's big message, or NONE. Nothing from UNKNOWN (power on); link lost
    //only after a state the receiver reported (not while starting without one)
    Sound forStatusChange(BigMessage from, BigMessage to);

    //enable cannot work while this shows
    bool refusesEnable(BigMessage message);

    //the speed switch's sound for its new state (SpeedMode), or NONE between the two OFF readings
    Sound forSpeedChange(uint8_t fromState, uint8_t toState);

    //a stick out of centre while the speed switch is at OFF (it does nothing): a continuous grind for as long as it
    //is out, one grain every grindPeriodMillis: a 12 ms square at a random pitch, higher and wider the further the
    //stick is pushed (deflection 0..1), then a gap. Not tonal on purpose: it must not read as a beep
    static const uint32_t grindPeriodMillis = 18;
    SoundNote grindGrain(float deflection, uint32_t& seed);

    //a page change: each page its own note, rising from Status (an interface sound, like CLICK)
    SoundCue pageCue(uint8_t page);

    //the way into the games (About page, both sticks held): a rising chiptune over the gesture's length, then a
    //start jingle when the arcade opens. Games priority; they play unless the setting is Off
    SoundCue arcadeLoadingCue();
    SoundCue arcadeStartCue();

    //NO LINK: the searching ping starts with the LED period (Leds::pattern), searchPeriodMillis
    static const uint32_t searchPeriodMillis = 2000;
};
