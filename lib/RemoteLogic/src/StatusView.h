#pragma once
#include <stdint.h>
#include "EstopContacts.h"
#include "BatteryMonitor.h"
#include "StickGuard.h"

//What the remote's main screen says on its big line and whether line 3 is replaced, highest priority first
//[D-147 point 3]: own E-stop (or its wiring), radio fault, no link, critical battery, sticks to release, then the
//state reported by the receiver
enum class BigMessage : uint8_t{
    ESTOP, ESTOP_WIRING, RADIO_FAULT, NO_LINK, BATTERY_CRITICAL,
    RELEASE_LEFT, RELEASE_RIGHT, RELEASE_BOTH,
    EXT_ESTOP, STOPPING, DISABLING, DISABLED, ENABLING, ENABLED, UNKNOWN
};

enum class StatusLine : uint8_t{ NORMAL, NOT_CONNECTED, ESTOP_NOT_CONNECTED, RADIO_RETRY, RESTART_REMOTE, THANK_YOU };

struct StatusInputs{
    EstopState estop = EstopState::CLEAR;
    bool b_radioFault = false;
    uint32_t radioFaultMillis = 0;          //how long the radio has been failing
    bool b_linkUp = false;
    BatteryState battery = BatteryState::OK;
    StickGuard::Release release = StickGuard::Release::NONE;
    uint8_t reportedState = 0;              //RobotState from the receiver's reply
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

    const char* text(BigMessage message);
    const char* text(StatusLine line);

    //speed switch as Stacato applies it (D-93): LOW = sticks off, MEDIUM = slow, HIGH = fast; both bits read as LOW
    const char* speedText(uint8_t speedState);
};
