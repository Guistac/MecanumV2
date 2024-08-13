#pragma once
#include <Arduino.h>
#include "Adafruit_SSD1305.h"

class Display : public Adafruit_SSD1305{

public:

    Display(uint8_t dcPin_, uint8_t resetPin_, uint8_t cdPin_) : Adafruit_SSD1305(128, 32, &SPI, dcPin_, resetPin_, cdPin_, 7000000UL){

    }

    bool initialize(){
        if(!begin()) {
            Serial.println("Failed to init display");
            return false;
        }
        //display.setContrast(255);
        clearDisplay();
        setTextSize(1);
        setTextColor(WHITE);
        display();
        return true;
    }

};