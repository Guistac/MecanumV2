#pragma once
#include <Arduino.h>
#include "RH_RF22.h"

class Radio{
private:
    RH_RF22* radio;
    uint8_t shutdownPin;
    uint8_t channel = -1;
    double frequency = 0.0;
    int temperature;

    double startFrequency = 869.4;
    double channelSpacing = 0.03;
    double frequencyCompensation = 0.015;

public:
    Radio(uint8_t shutdownPin_, uint8_t chipSelectPin_, uint8_t interruptPin_){
        radio = new RH_RF22(chipSelectPin_, interruptPin_);
        shutdownPin = shutdownPin_;
        pinMode(shutdownPin, OUTPUT);
        digitalWrite(shutdownPin, LOW);
    }

    bool initialize(uint8_t maxTries = 1){
        int tries = 0;
        while(true){
            digitalWrite(shutdownPin, HIGH);
            delay(10);
            digitalWrite(shutdownPin, LOW);
            delay(10);
            if(radio->init()) break;
            tries++;
            if(tries >= maxTries) return false;
            else Serial.printf("Failed radio init, trying again (%i/%i)\n", tries, maxTries);
        }
        //after init, the radio is not in Rx Mode
        //we need to set this mode immediately else we'll overheat
        radio->setModeRx();

        if(!radio->setModemConfig(RH_RF22::ModemConfigChoice::GFSK_Rb19_2Fd9_6)) return false;

        radio->setTxPower(RH_RF22_RF23BP_TXPOW_30DBM); //set max transmit power
        radio->setPromiscuous(true); //accept message with any TO address
        radio->setCADTimeout(0); //don't wait for channel activity detection
        setChannel(0);
        
        return true;
    }

    void updateTemperature(){
        temperature = radio->temperatureRead() - 169;
    }

    int getTemperatureCelsius(){ return temperature; }

    void setChannel(uint8_t newChannel){
        if(newChannel > 15) newChannel = 15;
        if(newChannel != channel){
            double newFrequency = startFrequency + newChannel * channelSpacing;
            radio->setFrequency(newFrequency, frequencyCompensation);
            channel = newChannel;
            frequency = newFrequency;
        }
    }

    void setNextChannel(){
        if(channel >= 15) setChannel(0);
        else setChannel(channel+1);
    }
    void setPreviousChannel(){
        if(channel == 0) setChannel(15);
        else setChannel(channel-1);
    }
    uint8_t getChannel(){ return channel; }
    double getFrequency(){ return frequency; }

    bool send(uint8_t* buffer, size_t length){
        if(length < 4) return false;
        radio->setHeaderTo(buffer[0]);
        radio->setHeaderFrom(buffer[1]);
        radio->setHeaderId(buffer[2]);
        radio->setHeaderFlags(buffer[3], 255);
        bool b_success = radio->send(&buffer[4], length-4);
        return b_success;
    }

    bool receive(uint8_t* buffer, uint8_t* length){
        uint8_t internalBuffer[255];
        uint8_t internalLength = 255;
        if(radio->recv(internalBuffer, &internalLength)) {
            buffer[0] = radio->headerTo();
            buffer[1] = radio->headerFrom();
            buffer[2] = radio->headerId();
            buffer[3] = radio->headerFlags();
            memcpy(buffer+4, internalBuffer, internalLength);
            *length = internalLength + 4;
            return true;
        }
        return false;
    }

    int16_t getSignalStrength(){
        return radio->lastRssi();
    }


};