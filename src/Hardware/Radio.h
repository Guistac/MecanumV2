#pragma once
#include <Arduino.h>
#include "RH_RF22.h"
#include "Utilities/Watchdog.h"

//RFM23BP radio with supervision:
//- initialization reports why it failed, retries with growing pauses and keeps the radio in shutdown in between
//- update() watches for lost interrupts, stuck transmissions and chip resets (brown-out),
//  and re-initializes the radio in the background when one of these happens
//- no call blocks for longer than one initialization attempt
class Radio{
public:

    enum class State : uint8_t{
        OFF,        //never initialized
        RUNNING,
        FAULT       //shut down, waiting for the next initialization attempt
    };

    struct Diagnostics{
        RH_RF22::InitError lastInitError = RH_RF22::INIT_OK;
        uint8_t lastDeviceType = 0;     //0x08 expected, 0xFF = MISO not driven, 0x00 = MISO stuck low
        uint32_t initAttempts = 0;
        uint32_t initFailures = 0;
        uint32_t faults = 0;            //runtime faults that forced a re-initialization
        uint32_t irqRecoveries = 0;     //times the nIRQ line was found stuck low
        uint32_t healthCheckFailures = 0;
        const char* lastFault = "none";
    };

private:
    RH_RF22* radio;
    uint8_t shutdownPin;
    uint8_t interruptPin;
    uint8_t channel = 0;
    double frequency = 0.0;
    int temperature = 0;

    double startFrequency = 869.4;
    double channelSpacing = 0.03;
    double frequencyCompensation = 0.015;

    //max transmit power, the RFM23BP draws ~550mA at 30dBm
    uint8_t txPower = RH_RF22_RF23BP_TXPOW_30DBM;

    //an 8 byte frame (4 header + 4 payload) takes ~13ms at 19.2kbps
    uint16_t txTimeoutMillis = 60;

    State state = State::OFF;
    Diagnostics diagnostics;
    Print* debugOutput = nullptr;

    //background retry pacing, gives the module time to cool down and the supply to settle
    const uint32_t minRetryDelayMillis = 100;
    const uint32_t maxRetryDelayMillis = 2000;
    uint32_t retryDelayMillis = 100;
    uint32_t nextRetryMillis = 0;

    uint32_t irqLowSinceMicros = 0;
    const uint32_t irqStuckMicros = 2000;

    uint32_t lastHealthCheckMillis = 0;
    const uint32_t healthCheckIntervalMillis = 250;

    uint16_t lastTxTimeouts = 0;

public:
    Radio(uint8_t shutdownPin_, uint8_t chipSelectPin_, uint8_t interruptPin_){
        radio = new RH_RF22(chipSelectPin_, interruptPin_);
        shutdownPin = shutdownPin_;
        interruptPin = interruptPin_;
        //keep the radio in shutdown until initialize(), level first so the pins never glitch low
        digitalWriteFast(shutdownPin, HIGH);
        pinMode(shutdownPin, OUTPUT);
        digitalWriteFast(chipSelectPin_, HIGH);
        pinMode(chipSelectPin_, OUTPUT);
    }

    void setDebugOutput(Print* output){ debugOutput = output; }

    //Blocking: tries up to maxTries times, with growing pauses in between.
    //If it fails, update() keeps retrying in the background.
    bool initialize(uint8_t maxTries = 1){
        retryDelayMillis = minRetryDelayMillis;
        for(int i = 0; i < maxTries; i++){
            if(attemptInitialization()) return true;
            if(i < maxTries - 1) Watchdog::delayFed(retryDelayMillis);
            increaseRetryDelay();
        }
        scheduleRetry();
        return false;
    }

    //call every loop iteration
    void update(){
        switch(state){
            case State::RUNNING:{
                pollInterruptLine();
                if(radio->consumeChipResetFlag()) { fault("chip reset (brown-out?)"); return; }
                if(radio->txTimeouts() != lastTxTimeouts) { fault("packet sent interrupt missing"); return; }
                if(radio->isTxStuck()) { fault("transmission never completed"); return; }
                if(millis() - lastHealthCheckMillis > healthCheckIntervalMillis){
                    lastHealthCheckMillis = millis();
                    if(!checkHealth()) { diagnostics.healthCheckFailures++; fault("register check failed"); return; }
                }
            }break;
            case State::FAULT:
            case State::OFF:
                if(int32_t(millis() - nextRetryMillis) >= 0){
                    if(!attemptInitialization()){
                        increaseRetryDelay();
                        scheduleRetry();
                    }
                }
                break;
        }
    }

    bool isRunning(){ return state == State::RUNNING; }
    State getState(){ return state; }
    const Diagnostics& getDiagnostics(){ return diagnostics; }

    const char* getInitErrorString(){
        switch(diagnostics.lastInitError){
            case RH_RF22::INIT_OK:                  return "ok";
            case RH_RF22::INIT_NO_INTERRUPT_PIN:    return "bad irq pin";
            case RH_RF22::INIT_CHIP_NOT_READY:      return "no response";
            case RH_RF22::INIT_BAD_DEVICE_TYPE:     return "bad device id";
            case RH_RF22::INIT_REGISTER_READBACK:   return "spi readback";
            case RH_RF22::INIT_TOO_MANY_DEVICES:    return "no irq vector";
        }
        return "?";
    }

    void updateTemperature(){
        if(!isRunning()) return;
        temperature = radio->temperatureRead() - 169;
    }

    int getTemperatureCelsius(){ return temperature; }

    void setChannel(uint8_t newChannel){
        if(newChannel > 15) newChannel = 15;
        if(newChannel != channel) applyChannel(newChannel);
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
        if(!isRunning()) return false;
        if(length < 4) return false;
        radio->setHeaderTo(buffer[0]);
        radio->setHeaderFrom(buffer[1]);
        radio->setHeaderId(buffer[2]);
        radio->setHeaderFlags(buffer[3], 255);
        bool b_success = radio->send(&buffer[4], length-4);
        return b_success;
    }

    bool receive(uint8_t* buffer, uint8_t* length){
        if(!isRunning()) return false;
        uint8_t internalBuffer[RH_RF22_MAX_MESSAGE_LEN];
        uint8_t internalLength = sizeof(internalBuffer);
        if(*length < 4) return false;
        if(radio->recv(internalBuffer, &internalLength)) {
            if(internalLength > *length - 4) internalLength = *length - 4;
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

private:

    bool attemptInitialization(){
        Watchdog::feed();
        diagnostics.initAttempts++;

        //power cycle the chip through SDN, a POR is started on the falling edge
        radio->detachRadioInterrupt();
        digitalWrite(shutdownPin, HIGH);
        Watchdog::delayFed(10);
        digitalWrite(shutdownPin, LOW);
        Watchdog::delayFed(20); //tPOR 16ms typ, init() keeps polling for the chip after this

        bool b_success = radio->init();
        diagnostics.lastInitError = radio->lastInitError();
        diagnostics.lastDeviceType = radio->deviceType();

        if(b_success){
            radio->setTxTimeout(txTimeoutMillis);
            b_success = radio->setModemConfig(RH_RF22::ModemConfigChoice::GFSK_Rb19_2Fd9_6);
        }
        if(b_success){
            radio->setTxPower(txPower);
            radio->setPromiscuous(true); //accept message with any TO address
            radio->setCADTimeout(0); //don't wait for channel activity detection
            applyChannel(channel, true);
            b_success = checkHealth();
            if(!b_success) diagnostics.lastInitError = RH_RF22::INIT_REGISTER_READBACK;
        }

        Watchdog::feed();

        if(!b_success){
            diagnostics.initFailures++;
            shutdown();
            state = State::FAULT;
            if(debugOutput) debugOutput->printf("Radio init failed (attempt %u): %s, device type 0x%02X\n",
                diagnostics.initAttempts, getInitErrorString(), diagnostics.lastDeviceType);
            return false;
        }

        lastTxTimeouts = radio->txTimeouts();
        irqLowSinceMicros = 0;
        lastHealthCheckMillis = millis();
        retryDelayMillis = minRetryDelayMillis;
        state = State::RUNNING;
        if(debugOutput) debugOutput->printf("Radio initialized (attempt %u)\n", diagnostics.initAttempts);
        return true;
    }

    //park the antenna control pins and put the chip in shutdown
    void shutdown(){
        radio->detachRadioInterrupt();
        radio->parkAntennaControl(); //no effect if the chip is not responding, harmless
        digitalWrite(shutdownPin, HIGH);
    }

    void fault(const char* reason){
        diagnostics.faults++;
        diagnostics.lastFault = reason;
        shutdown();
        state = State::FAULT;
        retryDelayMillis = minRetryDelayMillis;
        scheduleRetry();
        if(debugOutput) debugOutput->printf("Radio fault: %s, restarting radio\n", reason);
    }

    void scheduleRetry(){ nextRetryMillis = millis() + retryDelayMillis; }
    void increaseRetryDelay(){ retryDelayMillis = min(retryDelayMillis * 2, maxRetryDelayMillis); }

    //Registers that a brown-out or a lost SPI link would change.
    //In RX/TX operation GPIO_0 must be TX state and GPIO_1 RX state (antenna control).
    bool checkHealth(){
        if(radio->spiRead(RH_RF22_REG_00_DEVICE_TYPE) != diagnostics.lastDeviceType) return false;
        if(radio->spiRead(RH_RF22_REG_0B_GPIO_CONFIGURATION0) != RH_RF22_GPIO_TX_STATE) return false;
        if(radio->spiRead(RH_RF22_REG_0C_GPIO_CONFIGURATION1) != RH_RF22_GPIO_RX_STATE) return false;
        return true;
    }

    //nIRQ is edge triggered: if it ever stays low, no interrupt will come anymore.
    //A healthy line is only low for a few microseconds until the ISR reads the status registers.
    void pollInterruptLine(){
        if(digitalRead(interruptPin) == LOW){
            uint32_t now = micros();
            if(irqLowSinceMicros == 0) irqLowSinceMicros = now | 1;
            else if(now - irqLowSinceMicros > irqStuckMicros){
                diagnostics.irqRecoveries++;
                radio->serviceInterrupt();
                irqLowSinceMicros = 0;
            }
        }
        else irqLowSinceMicros = 0;
    }

    //while the radio is not running the channel is only stored, initialization applies it
    void applyChannel(uint8_t newChannel, bool b_force = false){
        channel = newChannel;
        frequency = startFrequency + channel * channelSpacing;
        if(isRunning() || b_force) radio->setFrequency(frequency, frequencyCompensation);
    }

};
