#pragma once
#include <stdint.h>
#include "LiIonBattery.h"

enum class BatteryState{ OK, LOW_CHARGE, CRITICAL };

//Remote cell warnings from the resting (slow filtered) cell voltage: low under 3.5 V, critical under 3.3 V, each left
//again 50 mV above its threshold so transmit sag does not make them flicker. Warnings only [D-147 point 4]
class BatteryMonitor{
public:

    static constexpr float hysteresis = 0.05f;

    BatteryState update(float restingCellVoltage){
        float low = LiIonBattery::lowCellVoltage;
        float critical = LiIonBattery::criticalCellVoltage;
        switch(state){
            case BatteryState::OK:
                if(restingCellVoltage < critical) state = BatteryState::CRITICAL;
                else if(restingCellVoltage < low) state = BatteryState::LOW_CHARGE;
                break;
            case BatteryState::LOW_CHARGE:
                if(restingCellVoltage < critical) state = BatteryState::CRITICAL;
                else if(restingCellVoltage > low + hysteresis) state = BatteryState::OK;
                break;
            case BatteryState::CRITICAL:
                if(restingCellVoltage > low + hysteresis) state = BatteryState::OK;
                else if(restingCellVoltage > critical + hysteresis) state = BatteryState::LOW_CHARGE;
                break;
        }
        return state;
    }

    BatteryState getState() const { return state; }

private:
    BatteryState state = BatteryState::OK;
};
