#pragma once
#include <Arduino.h>

namespace Safety{

    //the offset has to be smaller than the modulo to contain the safety number in the uint16_t range
    const uint16_t safetyModulo = 333;
    const uint16_t safetyOffset = 33;

    uint16_t generateSafeNumber(){
        //SAFE STATE
        //is represented as any number that is not divisible by the safety key after having the offset added
        //most numbers represent this case (safetyModulo - 1 / safetyModulo)
        uint16_t output = random(safetyModulo, UINT16_MAX - safetyModulo);
        if(output % safetyModulo == 0) output += random(1, safetyModulo - 1);
        output -= safetyOffset;
        return output;
    }

    uint16_t generateClearNumber(){
        //CLEAR STATE
        //define that as a number that is divisible by the safety key after having the offset added
        //not many numbers represent this case (1 / safetyModulo)
        uint16_t output = random(safetyModulo, UINT16_MAX - safetyModulo);
        output -= output % safetyModulo;
        output -= safetyOffset;
        return output;
    }

    bool isNumberClear(uint16_t input){
        //Safety Data Format
        //result = (safetyNumber + offset) % divider
        //result == 0 -> CLEAR state
        //result != 0 -> SAFE state
        if(input == UINT16_MAX) return false;
        if(input < safetyModulo - safetyOffset) return false;
        uint16_t result = (input + safetyOffset) % safetyModulo;
        return result == 0;
    }


};