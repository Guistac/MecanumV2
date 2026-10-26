#include "StatusView.h"
#include "RobotStates.h"

namespace Status{

    StatusView select(const StatusInputs& in){
        if(in.estop != EstopState::CLEAR){
            BigMessage message = in.estop == EstopState::WIRING ? BigMessage::ESTOP_WIRING : BigMessage::ESTOP;
            bool b_connected = in.b_linkUp && !in.b_radioFault;
            return {message, b_connected ? StatusLine::NORMAL : StatusLine::NOT_CONNECTED};
        }
        if(in.b_radioFault){
            return {BigMessage::RADIO_FAULT, in.radioFaultMillis > restartHintMillis ? StatusLine::RESTART_REMOTE : StatusLine::RADIO_RETRY};
        }
        if(!in.b_linkUp) return {BigMessage::NO_LINK, StatusLine::ESTOP_NOT_CONNECTED};

        StatusLine line = in.b_thankYou ? StatusLine::THANK_YOU : StatusLine::NORMAL;
        if(in.battery == BatteryState::CRITICAL) return {BigMessage::BATTERY_CRITICAL, line};
        switch(in.release){
            case StickGuard::Release::LEFT: return {BigMessage::RELEASE_LEFT, line};
            case StickGuard::Release::RIGHT: return {BigMessage::RELEASE_RIGHT, line};
            case StickGuard::Release::BOTH: return {BigMessage::RELEASE_BOTH, line};
            default: break;
        }
        switch(RobotState(in.reportedState)){
            case RobotState::DISABLING: return {BigMessage::DISABLING, line};
            case RobotState::DISABLED: return {BigMessage::DISABLED, line};
            case RobotState::ENABLING: return {BigMessage::ENABLING, line};
            case RobotState::ENABLED: return {BigMessage::ENABLED, line};
            case RobotState::EMERGENCY_STOPPING: return {BigMessage::STOPPING, line};
            case RobotState::EMERGENCY_STOPPED:
                if(in.sinceEstopReleaseMillis < extEstopDelayMillis) return {BigMessage::ESTOP, line};
                return {BigMessage::EXT_ESTOP, line};
            default: return {BigMessage::UNKNOWN, line};
        }
    }

    bool isAlarm(BigMessage message){
        switch(message){
            case BigMessage::ESTOP:
            case BigMessage::ESTOP_WIRING:
            case BigMessage::RADIO_FAULT:
            case BigMessage::NO_LINK:
            case BigMessage::BATTERY_CRITICAL:
            case BigMessage::EXT_ESTOP:
            case BigMessage::STOPPING:
                return true;
            default:
                return false;
        }
    }

    const char* text(BigMessage message){
        switch(message){
            case BigMessage::ESTOP: return "E-STOP";
            case BigMessage::ESTOP_WIRING: return "E-STOP WIRING";
            case BigMessage::RADIO_FAULT: return "RADIO FAULT";
            case BigMessage::NO_LINK: return "NO LINK";
            case BigMessage::BATTERY_CRITICAL: return "REMOTE BATT LOW";
            case BigMessage::RELEASE_LEFT: return "RELEASE L STICK";
            case BigMessage::RELEASE_RIGHT: return "RELEASE R STICK";
            case BigMessage::RELEASE_BOTH: return "RELEASE STICKS";
            case BigMessage::EXT_ESTOP: return "EXT E-STOP";
            case BigMessage::STOPPING: return "STOPPING";
            case BigMessage::DISABLING: return "Disabling";
            case BigMessage::DISABLED: return "Disabled";
            case BigMessage::ENABLING: return "Enabling";
            case BigMessage::ENABLED: return "Enabled";
            default: return "???";
        }
    }

    const char* text(StatusLine line){
        switch(line){
            case StatusLine::NOT_CONNECTED: return "NOT CONNECTED";
            case StatusLine::ESTOP_NOT_CONNECTED: return "E-STOP NOT CONNECTED";
            case StatusLine::RESTART_REMOTE: return "RESTART REMOTE";
            case StatusLine::THANK_YOU: return "THANK YOU";
            default: return "";
        }
    }

    const char* speedText(uint8_t speedState){
        switch(SpeedMode(speedState)){
            case SpeedMode::MEDIUM_SPEED: return "SLOW";
            case SpeedMode::HIGH_SPEED: return "FAST";
            default: return "OFF";
        }
    }

};
