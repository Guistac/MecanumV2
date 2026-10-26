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

    uint8_t chipSelectPin;
#ifdef RADIO_BENCH_DIAG
    uint8_t temperatureRaw = 0;

    //Polls the chip ready flag (interrupt status 2) directly over SPI (RadioHead may not have started
    //SPI yet). Returns the microseconds until it was set, or -1 after timeoutMicros. Reading the
    //status registers clears them, as RadioHead's own wait does.
    int32_t diagPollChipReady(uint32_t timeoutMicros){
        //before RadioHead's first init the select pin is not an output yet
        digitalWrite(chipSelectPin, HIGH);
        pinMode(chipSelectPin, OUTPUT);
        SPI.begin();
        uint32_t start = micros();
        while(micros() - start < timeoutMicros){
            SPI.beginTransaction(SPISettings(1000000, MSBFIRST, SPI_MODE0));
            digitalWrite(chipSelectPin, LOW);
            SPI.transfer(RH_RF22_REG_04_INTERRUPT_STATUS2 & 0x7F);
            uint8_t status2 = SPI.transfer(0x00);
            digitalWrite(chipSelectPin, HIGH);
            SPI.endTransaction();
            if(status2 != 0xFF && (status2 & RH_RF22_ICHIPRDY)) return int32_t(micros() - start);
            delayMicroseconds(100);
        }
        return -1;
    }
#endif

    double startFrequency = 869.4;
    double channelSpacing = 0.03;
    double frequencyCompensation = 0.015;

public:
    Radio(uint8_t shutdownPin_, uint8_t chipSelectPin_, uint8_t interruptPin_){
        radio = new RH_RF22(chipSelectPin_, interruptPin_);
        shutdownPin = shutdownPin_;
        chipSelectPin = chipSelectPin_;
        pinMode(shutdownPin, OUTPUT);
        digitalWrite(shutdownPin, LOW);
    }

    bool initialize(uint8_t maxTries = 1){
        int tries = 0;
        while(true){
            digitalWrite(shutdownPin, HIGH);
            delay(10);
            digitalWrite(shutdownPin, LOW);
#ifdef RADIO_BENCH_DIAG
            //same 10 ms as the original delay, but polling the chip ready flag meanwhile
            int32_t readyMicros = diagPollChipReady(10000);
#else
            delay(10);
#endif
            if(radio->init()) {
#ifdef RADIO_BENCH_DIAG
                Serial.printf("diag init_ok try=%i por_chip_ready_us=%li xtal_load=0x%02X xtal_por=0x%02X\n",
                    tries + 1, (long)readyMicros, radio->spiRead(RH_RF22_REG_09_OSCILLATOR_LOAD_CAPACITANCE),
                    radio->spiRead(RH_RF22_REG_62_CRYSTAL_OSCILLATOR_POR_CONTROL));
#endif
                break;
            }
            tries++;
#ifdef RADIO_BENCH_DIAG
            //bench diagnostics (radio-bench, docs/radio/05): what the chip answers after a failed init,
            //then an own software reset with up to 1 s for chip ready (crystal late or not starting at all?).
            //This extends failed attempts only.
            uint8_t status1 = radio->spiRead(RH_RF22_REG_03_INTERRUPT_STATUS1);
            uint8_t status2 = radio->spiRead(RH_RF22_REG_04_INTERRUPT_STATUS2);
            Serial.printf("diag init_fail try=%i device_type=0x%02X status1=0x%02X status2=0x%02X device_status=0x%02X "
                "xtal_load=0x%02X xtal_por=0x%02X por_chip_ready_us=%li\n",
                tries, radio->spiRead(RH_RF22_REG_00_DEVICE_TYPE), status1, status2,
                radio->spiRead(RH_RF22_REG_02_DEVICE_STATUS), radio->spiRead(RH_RF22_REG_09_OSCILLATOR_LOAD_CAPACITANCE),
                radio->spiRead(RH_RF22_REG_62_CRYSTAL_OSCILLATOR_POR_CONTROL), (long)readyMicros);
            radio->spiWrite(RH_RF22_REG_07_OPERATING_MODE1, RH_RF22_SWRES);
            Serial.printf("diag swres_chip_ready_us=%li (timeout 1000000)\n", (long)diagPollChipReady(1000000));
#endif
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

#ifdef RADIO_BENCH_DIAG
    //wider range for the bench: -64 to +192 C, 1 C per step (the default range saturates at +63.5 C)
    void updateTemperature(){
        temperatureRaw = radio->temperatureRead(RH_RF22_TSRANGE_M64_192C);
        temperature = int(temperatureRaw) - 64;
    }
    uint8_t getTemperatureRaw(){ return temperatureRaw; }
#else
    void updateTemperature(){
        temperature = radio->temperatureRead() - 169;
    }
#endif

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