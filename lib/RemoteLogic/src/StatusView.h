#pragma once
#include <stdint.h>
#include "EstopContacts.h"
#include "BatteryMonitor.h"
#include "StickGuard.h"
#include "ReplyFrame.h"

//What the remote's main screen says on its big line and whether line 3 is replaced, highest priority first
//[D-147 point 3]: own E-stop (or its wiring), radio fault, no link, critical battery, sticks to release, then the
//state reported by the receiver
enum class BigMessage : uint8_t{
    ESTOP, ESTOP_WIRING, RADIO_FAULT, NO_LINK, BATTERY_CRITICAL,
    RELEASE_LEFT, RELEASE_RIGHT, RELEASE_BOTH,
    EXT_ESTOP, STOPPING, DISABLING, DISABLED, ENABLING, ENABLED, UNKNOWN,
    //[D-163] Stacato reports its fieldbus starting or off, or the vehicle offline, or no Stacato runs: not an alarm, the
    //Status page names the finer state (Bus start, Bus off, Vehicle off, No Stacato). The 3 bit state still drives
    //every decision
    NOT_RUNNING,
    //[owner 2026-10-28] the channel setting is Off: nothing is sent, not an alarm
    RADIO_OFF
};

enum class StatusLine : uint8_t{ NORMAL, NOT_CONNECTED, ESTOP_NOT_CONNECTED, RADIO_RETRY, RESTART_REMOTE, THANK_YOU };

struct StatusInputs{
    EstopState estop = EstopState::CLEAR;
    bool b_radioFault = false;
    uint32_t radioFaultMillis = 0;          //how long the radio has been failing
    bool b_radioOff = false;                //the channel setting is Off (and no USB link)
    bool b_linkUp = false;
    BatteryState battery = BatteryState::OK;
    StickGuard::Release release = StickGuard::Release::NONE;
    uint8_t reportedState = 0;              //RobotState from the receiver's reply
    //[D-163] Stacato's finer state: a disable ramp also arrives as the 3 bit E-stopping, this tells it apart
    Reply::VehicleState vehicle = Reply::VehicleState::UNKNOWN;
    uint32_t sinceEstopReleaseMillis = UINT32_MAX;  //since the own E-stop was last released
    bool b_thankYou = false;
};

struct StatusView{
    BigMessage message;
    StatusLine line;
};

namespace Status{

    //Stacato still reports E-stopped for a moment after the own E-stop is released (heartbeat delay)
    static constexpr uint32_t extEstopDelayMillis = 500;
    static constexpr uint32_t restartHintMillis = 30000;

    StatusView select(const StatusInputs& inputs);

    //never dimmed while shown [D-147 point 5]
    bool isAlarm(BigMessage message);
    //[owner 2026-10-28] the radio fault and no link: a radio reset clears them by itself, so they take the operator
    //out of the games only once they have lasted gamesLinkGraceMillis (the speed switch is at OFF while the games
    //run and they send no sticks); every other alarm at once
    bool isLinkAlarm(BigMessage message);
    static constexpr uint32_t gamesLinkGraceMillis = 5000;

    const char* text(BigMessage message);
    const char* text(StatusLine line);

    //speed switch as Stacato applies it (D-93): LOW = sticks off, MEDIUM = slow, HIGH = fast; both bits read as LOW
    const char* speedText(uint8_t speedState);
};
