#pragma once
#include <Arduino.h>

class FrequencyTimer{
public:

    FrequencyTimer(){}

    FrequencyTimer(double frequency){
        setFrequency(frequency);
    }

    bool shouldTrigger(){
        uint32_t now = micros();
        if(now - lastEventTime > eventInterval){
            lastEventTime = now;
            return true;
        }
        return false;
    }

    void setFrequency(double frequency){
        eventInterval = 1000000.0 / frequency;
    }

    void forceTrigger(){ lastEventTime -= eventInterval; }

private:
    uint32_t lastEventTime = 0;
    uint32_t eventInterval = 0;
};

class LengthTimer{
public:

    LengthTimer(){}

    LengthTimer(double seconds){
        setLength(seconds);
        start();
    }

    void setLength(double seconds){ lengthMillis = seconds * 1000.0; }
    void start(){ startMillis = millis(); }
    float getProgress(){ return float(millis() - startMillis) / float(lengthMillis); }
    bool hasExpired(){ return millis() - startMillis > lengthMillis; }

private:
    uint32_t startMillis = 0;
    uint32_t lengthMillis = 0;
};