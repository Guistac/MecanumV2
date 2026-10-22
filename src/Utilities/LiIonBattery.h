#pragma once
#include <stdint.h>

//Rough state of charge of a Li-ion pack from its resting cell voltage.
//No current measurement: the value drops under load and recovers at standstill, read it as an indicator (about +-10% at rest).
namespace LiIonBattery{

    //Typical Li-ion open circuit voltage curve, per cell
    struct CurvePoint{ float cellVoltage; float percent; };
    static const CurvePoint curve[] = {
        {3.20, 0.0},
        {3.40, 5.0},
        {3.60, 15.0},
        {3.70, 30.0},
        {3.80, 50.0},
        {3.90, 65.0},
        {4.00, 80.0},
        {4.20, 100.0}
    };
    static const int curveSize = sizeof(curve) / sizeof(CurvePoint);

    //Below this the reading is not a pack (divider unplugged, board powered from USB only)
    static const float plausibleCellVoltageMin = 2.5;
    static const float lowCellVoltage = 3.5;
    static const float criticalCellVoltage = 3.3;

    inline bool isPlausible(float cellVoltage){ return cellVoltage >= plausibleCellVoltageMin; }

    inline int8_t percentFromCellVoltage(float cellVoltage){
        if(cellVoltage <= curve[0].cellVoltage) return 0;
        if(cellVoltage >= curve[curveSize - 1].cellVoltage) return 100;
        for(int i = 1; i < curveSize; i++){
            if(cellVoltage > curve[i].cellVoltage) continue;
            const CurvePoint& a = curve[i - 1];
            const CurvePoint& b = curve[i];
            float percent = a.percent + (cellVoltage - a.cellVoltage) * (b.percent - a.percent) / (b.cellVoltage - a.cellVoltage);
            return int8_t(percent + 0.5);
        }
        return 100;
    }

};
