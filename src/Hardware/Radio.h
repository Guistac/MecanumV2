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

    uint8_t interruptPin;
    uint16_t recoveries = 0;
    uint16_t handledChipResets = 0;
    uint8_t expectedBandSelect = 0;     //frequency band register as written by setChannel()
    uint32_t lastConfigCheckMillis = 0;
    uint32_t lastRecoveryMillis = 0;
    bool b_initialized = false;
    uint16_t initFailures = 0;          //failed initializations in a row
    uint32_t faultStartMillis = 0;

    double startFrequency = 869.4;
    double channelSpacing = 0.03;
    double frequencyCompensation = 0.015;

public:
    Radio(uint8_t shutdownPin_, uint8_t chipSelectPin_, uint8_t interruptPin_){
        radio = new RH_RF22(chipSelectPin_, interruptPin_);
        shutdownPin = shutdownPin_;
        interruptPin = interruptPin_;
        pinMode(shutdownPin, OUTPUT);
        digitalWrite(shutdownPin, LOW);
    }

    //false: the chip did not answer, update() tries again every 2 s
    bool initialize(uint8_t maxTries = 1){
        b_initialized = false;
        bool b_success = initializeChip(maxTries);
        if(b_success) initFailures = 0;
        else{
            if(initFailures == 0) faultStartMillis = millis();
            if(initFailures < UINT16_MAX) initFailures++;
            lastRecoveryMillis = millis();
        }
        return b_success;
    }

    bool isInitialized(){ return b_initialized; }
    uint16_t getInitFailures(){ return initFailures; }
    //how long the radio has been failing to initialize, 0 while it works
    uint32_t getFaultMillis(){ return b_initialized ? 0 : millis() - faultStartMillis; }
    uint16_t getTxTimeouts(){ return radio->txTimeouts(); }
    uint16_t getChipResets(){ return radio->chipResets(); }

private:

    bool initializeChip(uint8_t maxTries){
        int tries = 0;
        //on a re-init RadioHead's interrupt handler is still attached: it would run on the power-on and
        //chip ready interrupts and clear the chip ready flag before init() reads it. init() attaches it again.
        detachInterrupt(digitalPinToInterrupt(interruptPin));
        while(true){
            digitalWrite(shutdownPin, HIGH);
            delay(10);
            digitalWrite(shutdownPin, LOW);
            delay(10);
            if(radio->init()) {
                break;
            }
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
        b_initialized = true;
        //the chip forgot its frequency: set it again, the current channel on a re-init, else channel 0
        uint8_t restoreChannel = channel > 15 ? 0 : channel;
        channel = -1;
        setChannel(restoreChannel);
        handledChipResets = radio->chipResets();
        
        return true;
    }

public:

    void updateTemperature(){
        if(!b_initialized) return;
        temperature = radio->temperatureRead() - 169;
    }

    int getTemperatureCelsius(){ return temperature; }

    void setChannel(uint8_t newChannel){
        if(newChannel > 15) newChannel = 15;
        //chip not running: only remember the channel, initialize() sets it
        if(!b_initialized){
            channel = newChannel;
            return;
        }
        if(newChannel != channel){
            double newFrequency = startFrequency + newChannel * channelSpacing;
            radio->setFrequency(newFrequency, frequencyCompensation);
            expectedBandSelect = radio->spiRead(RH_RF22_REG_75_FREQUENCY_BAND_SELECT);
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
        if(length < 4 || !b_initialized) return false;
        radio->setHeaderTo(buffer[0]);
        radio->setHeaderFrom(buffer[1]);
        radio->setHeaderId(buffer[2]);
        radio->setHeaderFlags(buffer[3], 255);
        uint16_t timeoutsBefore = radio->txTimeouts();
        bool b_success = radio->send(&buffer[4], length-4);
        //the previous packet never finished (chip reset or brown-out while sending, docs/radio/05):
        //start the radio over instead of waiting forever
        if(radio->txTimeouts() != timeoutsBefore) recover("tx timeout");
        return b_success;
    }

    //main loop: re-init after a chip reset (brown-out, SDN glitch), which leaves the chip on its
    //default frequency and modem settings. The robot only sends in reply, so without this a reset
    //chip would never hear the remote again and no send could time out. Detected by the chip's
    //power-on reset interrupt, and once a second by the frequency band register (a few us).
    //Also once a second: nIRQ is edge triggered, so if it is low and stays low (a flag pending when the
    //interrupt was attached, a lost edge) no interrupt will ever come again, though the chip is fine.
    void update(){
        //the chip never came up or a re-init failed: try again every 2 s
        if(!b_initialized){
            if(millis() - lastRecoveryMillis >= 2000){
                bool b_started = initialize(1);
                Serial.printf("radio init retry %s (%i failed)\n", b_started ? "ok" : "failed", initFailures);
            }
            return;
        }
        //a re-init blocks for about 140 ms: while a chip stays dead, retry every 2 s, not every loop
        if(recoveries > 0 && millis() - lastRecoveryMillis < 2000) return;
        if(radio->chipResets() != handledChipResets){
            recover("chip reset interrupt");
            return;
        }
        if(millis() - lastConfigCheckMillis >= 1000){
            lastConfigCheckMillis = millis();
            if(radio->spiRead(RH_RF22_REG_75_FREQUENCY_BAND_SELECT) != expectedBandSelect){
                recover("configuration lost");
                return;
            }
            if(digitalReadFast(interruptPin) == LOW){
                delayMicroseconds(200);     //the handler clears a fresh interrupt within a few us
                if(digitalReadFast(interruptPin) == LOW) recover("interrupt line stuck low");
            }
        }
    }

    void recover(const char* reason){
        recoveries++;
        lastRecoveryMillis = millis();
        bool b_restarted = initialize(1);
        Serial.printf("radio %s, re-init %s (recovery %i)\n", reason, b_restarted ? "ok" : "failed", recoveries);
    }

    //number of re-inits after a send timeout or a chip reset
    uint16_t getRecoveries(){ return recoveries; }


    bool receive(uint8_t* buffer, uint8_t* length){
        if(!b_initialized) return false;
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