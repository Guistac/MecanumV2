#pragma once
#include <Arduino.h>

class IODevice{
public:

    virtual void initialize() = 0;

    //returns true if inputs did update
    virtual bool updateInputs(){
        uint32_t now_micros = micros();
        if(now_micros - lastInputReading_micros < inputReadingInterval) return false;
        lastInputReading_micros = now_micros;
        return true;
    }

    static void setup(){
        analogReadResolution(10);
        analogWriteResolution(8);
    }

    static int getAnalogReadResolution(){
       return 1023;
    }

    static int getAnalogWriteResolution(){
        return 255;
    }

    void setInputUpdateFrequency(double freq){
        inputReadingInterval = 1000000.0 / freq;
    }

    double timeSinceLastInputChangeSeconds(){
        return double(millis() - lastInputChange_millis) / 1000.0;
    }

    uint32_t lastInputReading_micros = UINT32_MAX;
    uint32_t inputReadingInterval = 250;
    uint32_t lastInputChange_millis = 0;
};


class LED : public virtual IODevice{
public:

    LED(uint8_t outputPin_, bool isPwmCapable_){
        outputPin = outputPin_;
        isPwmCapable = isPwmCapable_;
    }

    virtual void initialize() override {
        pinMode(outputPin, OUTPUT);
        if(isPwmCapable) analogWrite(outputPin, 0);
        else digitalWrite(outputPin, LOW);
    }

    void setBrightness(float newBrightness){
        if(isPwmCapable){
            if(newBrightness >= 1.0) brightness = getAnalogWriteResolution();
            else if(newBrightness <= 0.0) brightness = 0;
            else brightness = map(newBrightness, 0.0, 1.0, 0, getAnalogWriteResolution());
            analogWrite(outputPin, brightness);
        }
        else{
            if (newBrightness > 0.5){
                brightness = 255;
                digitalWrite(outputPin, HIGH);
            }
            else{
                brightness = 0;
                digitalWrite(outputPin, LOW);
            }
        }
    }
    void turnOn(){
        brightness = 255;
        if(isPwmCapable) analogWrite(outputPin, getAnalogWriteResolution());
        else digitalWrite(outputPin, HIGH);
    }
    void turnOff(){
        brightness = 0;
        if(isPwmCapable) analogWrite(outputPin, 0);
        else digitalWrite(outputPin, LOW);
    }
    void set(bool isOn){
        if(isPwmCapable) {
            brightness = isOn ? 255 : 0;
            analogWrite(outputPin, isOn ? getAnalogWriteResolution() : 0);
        }
        else {
            digitalWrite(outputPin, isOn ? HIGH : LOW);
        }
    }

private:
    uint8_t outputPin;
    bool isPwmCapable;
    uint8_t brightness;

};



class PushButton : public virtual IODevice{
public:
    PushButton(uint8_t inputPin_, uint8_t inputPinConfiguration_, bool invert_){
        inputPin = inputPin_;
        invert = invert_;
        switch(inputPinConfiguration_){
            case INPUT:
            case INPUT_PULLDOWN:
            case INPUT_PULLUP:
                inputPinConfiguration = inputPinConfiguration_;
                break;
            default:
                inputPinConfiguration = INPUT;
                break;
        }
    }

    virtual void initialize() override {
        pinMode(inputPin, inputPinConfiguration);
        setInputUpdateFrequency(100.0);
    }

    virtual bool updateInputs() override {
        if(!IODevice::updateInputs()) return false;
        onUpdate();
        return true;
    };

    //Current State
    bool isPressed() { return buttonState; }

    //true for one call when button was pressed
    bool wasPushed(){
        if(stateChanged && buttonState){
            stateChanged = false;
            return true;
        }
        return false;
    }

    //Release Event
    bool wasReleased(){
        if(stateChanged && !buttonState){
            stateChanged = false;
            return true;
        }
        return false;
    }


protected:

    void onUpdate(){
        bool newButtonState = digitalRead(inputPin);
        if(inputPinConfiguration == INPUT_PULLUP) newButtonState = !newButtonState;
        if(invert) newButtonState = !newButtonState;

        //we don't clear the statechanged flag, we only can set it if there was change
        if(buttonState != newButtonState) {
            stateChanged = true;
            lastInputChange_millis = millis();
        }
        buttonState = newButtonState;
    }

private:
    uint8_t inputPin;
    uint8_t inputPinConfiguration;
    bool invert;
    bool buttonState;
    bool stateChanged = false;
};



class PushButtonWithLED : public PushButton, public LED{
public:

    PushButtonWithLED(uint8_t buttonPin_, uint8_t buttonPinConfiguration_, bool invertButton_, uint8_t ledPin_, bool isPwmCapable_) :
        PushButton(buttonPin_, buttonPinConfiguration_, invertButton_),
        LED(ledPin_, isPwmCapable_){}

    virtual void initialize() override {
        PushButton::initialize();
        LED::initialize();
    };

    virtual bool updateInputs() override {
        return PushButton::updateInputs();
    };
};

class EstopButton : public IODevice{
    public:

    EstopButton(uint8_t noPin_, uint8_t ncPin_, bool invert_){
        noPin = noPin_;
        ncPin = ncPin_;
        b_invert = invert_;
    }

    virtual void initialize() override {
        pinMode(noPin, INPUT_PULLUP);
        pinMode(ncPin, INPUT_PULLUP);
    }

    virtual bool updateInputs() override {
        if(!IODevice::updateInputs()) return false;
        bool newState = digitalRead(noPin) && !digitalRead(ncPin);
        if(b_invert) newState = !newState;
        if(newState != b_state){
            b_stateChanged = true;
            b_state = newState;
            lastInputChange_millis = millis();
        }
        //Serial.printf("%i %i %s\n", digitalRead(noPin), digitalRead(ncPin), b_state ? "Clear" : "Safe");
        return true;
    }

    bool isClear(){ return b_state; }

private:
    bool b_state;
    uint8_t noPin;
    uint8_t ncPin;
    bool b_invert;
    bool b_stateChanged = false;
};


class ToggleSwitch : public IODevice{
public:

    ToggleSwitch(uint8_t inputPin_, uint8_t inputPinConfiguration_, bool invert_){
        inputPin = inputPin_;
        invert = invert_;
        switch(inputPinConfiguration_){
            case INPUT:
            case INPUT_PULLDOWN:
            case INPUT_PULLUP:
                inputPinConfiguration = inputPinConfiguration_;
                break;
            default:
                inputPinConfiguration = INPUT;
                break;
        }
    }

    virtual void initialize() override {
        pinMode(inputPin, inputPinConfiguration);
        setInputUpdateFrequency(100.0);
    }

    virtual bool updateInputs() override {
        if(!IODevice::updateInputs()) return false;

        bool newSwitchState = digitalRead(inputPin);
        if(inputPinConfiguration == INPUT_PULLUP) newSwitchState = !newSwitchState;
        if(invert) newSwitchState = !newSwitchState;

        if(switchState != newSwitchState) {
            stateChanged = true;
            lastInputChange_millis = millis();
        }
        switchState = newSwitchState;

        /*
        if(newButtonState) readingCounter++;
        else readingCounter--;
        if(readingCounter > counterMax) readingCounter = counterMax;
        if(readingCounter < -counterMax) readingCounter = -counterMax;
        switchState = readingCounter > counterThreshold;
        */

        return true;
    }

    bool getSwitchState(){ return switchState; }

    bool wasSwitchedOn(){
        if(stateChanged && switchState){
            stateChanged = false;
            return true;
        }
        return false;
    }

    bool wasSwitchedOff(){
        if(stateChanged && !switchState){
            stateChanged = false;
            return true;
        }
        return false;
    }

private:
    uint8_t inputPin;
    uint8_t inputPinConfiguration;
    bool invert;
    bool switchState;
    bool stateChanged = false;

/*
    int8_t readingCounter = 0;
    uint8_t counterMax = 100;
    int8_t counterThreshold = 90;
*/
};



class ThreePositionToggleSwitch : public IODevice{
public:

    ThreePositionToggleSwitch(uint8_t inputPin1_, uint8_t inputPin2_, uint8_t inputPinConfiguration_){
        inputPin1 = inputPin1_;
        inputPin2 = inputPin2_;
        switch(inputPinConfiguration_){
            case INPUT:
            case INPUT_PULLDOWN:
            case INPUT_PULLUP:
                inputPinConfiguration = inputPinConfiguration_;
                break;
            default:
                inputPinConfiguration = INPUT;
                break;
        }
    }

    virtual void initialize() override {
        pinMode(inputPin1, inputPinConfiguration);
        pinMode(inputPin2, inputPinConfiguration);
        setInputUpdateFrequency(100.0);
    }

    virtual bool updateInputs() override {
        if(!IODevice::updateInputs()) return false;

        bool pin1Reading = digitalRead(inputPin1);
        bool pin2Reading = digitalRead(inputPin2);
        if(inputPinConfiguration == INPUT_PULLUP) {
            pin1Reading = !pin1Reading;
            pin2Reading = !pin2Reading;
        }
        uint8_t newState = readingsToState(pin1Reading, pin2Reading);

        if(newState != state) {
            b_stateChanged = true;
            lastInputChange_millis = millis();
        }
        state = newState;
       
        return true;
    }

    bool stateChanged(){
        if(b_stateChanged){
            b_stateChanged = false;
            return true;
        }
        return false;
    }
    uint8_t getState(){ return state; }

    bool wasSwitchedToPositive(){
        if(b_stateChanged && state == 2){
            b_stateChanged = false;
            return true;
        }
        return false;
    }
    bool wasSwitchedToNeutral(){
        if(b_stateChanged && state == 1){
            b_stateChanged = false;
            return true;
        }
        return false;
    }
    bool wasSwitchedToNegative(){
        if(b_stateChanged && state == 0){
            b_stateChanged = false;
            return true;
        }
        return false;
    }

private:

    uint8_t readingsToState(bool reading1, bool reading2){
        if(reading1 && reading2) return 3;
        else if(reading1) return 0;
        else if(reading2) return 2;
        else return 1;
    }

    uint8_t inputPin1;
    uint8_t inputPin2;
    uint8_t inputPinConfiguration;
    uint8_t state;
    bool b_stateChanged = false;
};



class Joystick2Axis : public virtual IODevice{
public:

    Joystick2Axis(uint8_t xAxisPin_, uint8_t yAxisPin_, bool invertX_, bool invertY_){
        xAxisPin = xAxisPin_;
        yAxisPin = yAxisPin_;
        invertX = invertX_;
        invertY = invertY_;
    }

    virtual void initialize() override {
        pinMode(xAxisPin, INPUT);
        pinMode(yAxisPin, INPUT);
        setInputUpdateFrequency(50.0);
        captureZero();
    }

    void captureZero(){
        zeroX = analogRead(xAxisPin);
        zeroY = analogRead(yAxisPin);
    }

    virtual bool updateInputs() override {
        if(!IODevice::updateInputs()) return false;

        int xReading = analogRead(xAxisPin);
        int yReading = analogRead(yAxisPin);
        xAxisValue = readingToValue(xReading, zeroX, invertX);
        yAxisValue = readingToValue(yReading, zeroY, invertY);

        if(previousXAxisValue != xAxisValue || previousYAxisValue != yAxisValue) {
            b_joystickMoved = true;
            lastInputChange_millis = millis();
        }
        previousXAxisValue = xAxisValue;
        previousYAxisValue = yAxisValue;

        return true;
    }

    float getXValue(){ return xAxisValue; }
    float getYValue(){ return yAxisValue; }
    bool moved(){
        if(b_joystickMoved){
            b_joystickMoved = false;
            return true;
        }
        return false;
    }

    bool isRawBelowThreshold(float threshold){
        if(threshold < 0.0) threshold = -threshold;
        float rawX = map(float(analogRead(xAxisPin)), 0, getAnalogReadResolution(), -1.0, 1.0);
        float rawY = map(float(analogRead(yAxisPin)), 0, getAnalogReadResolution(), -1.0, 1.0);
        Serial.printf("%.3f %.3f\n", rawX, rawY);
        delay(10);
        if(rawX > -threshold && rawX < threshold && rawY > -threshold && rawY < threshold) return true;
        return false;
    }

private:

    float readingToValue(uint16_t reading, int zeroOffset, bool b_invert){
        float normalized;
        if(reading >= zeroOffset) normalized = map(float(reading), zeroOffset, getAnalogReadResolution(), 0.0, 1.0);
        else normalized = map(float(reading), 0, zeroOffset, -1.0, 0.0);
        bool sign = normalized > 0.0;
        float absolute = sign ? normalized : -normalized;
        float output;
        if(absolute > maxLevel) output = 1.0;
        else if(absolute < minLevel) return 0.0;
        else {
            output = map(absolute, minLevel, maxLevel, 0.0, 1.0);
            if(output > 1.0) output = 1.0;
            else if(output < 0.0) output = 0.0;
        }
        if(!sign) output = -output;
        if(b_invert) output = -output;
        return output;
    }

    uint8_t xAxisPin;
    uint8_t yAxisPin;
    bool invertX;
    bool invertY;
    float xAxisValue;
    float yAxisValue;
    float previousXAxisValue;
    float previousYAxisValue;
    float minLevel = 0.01;
    float maxLevel = 0.99;
    int zeroX;
    int zeroY;
    uint32_t lastReadingMillis = UINT32_MAX;
    uint32_t readingDelayMillis = 20;
    bool b_joystickMoved = false;
};


class Joystick2AxisWithPushbutton : public Joystick2Axis, public PushButton{
public:

    Joystick2AxisWithPushbutton(uint8_t xAxisPin_, uint8_t yAxisPin_, bool invertX_, bool invertY_, uint8_t buttonInputPin_, uint8_t buttonInputPinConfiguration_, bool invertButton_) : 
    Joystick2Axis(xAxisPin_, yAxisPin_, invertX_, invertY_), 
    PushButton(buttonInputPin_, buttonInputPinConfiguration_, invertButton_){}

    virtual void initialize() override {
        PushButton::initialize();
        Joystick2Axis::initialize();
        //since we initialize the joystick parent class first, the update rate is set equal to the joystick rate
    }

    virtual bool updateInputs() override {
        if(!Joystick2Axis::updateInputs()) return false;
        PushButton::onUpdate();
        return true;
    }

};

class RotaryDipSwitch4Bit : public IODevice{
public:

    RotaryDipSwitch4Bit(uint8_t bit1pin_, uint8_t bit2pin_, uint8_t bit4pin_, uint8_t bit8pin_, uint8_t pinConfiguration_){
        bit1pin = bit1pin_;
        bit2pin = bit2pin_;
        bit4pin = bit4pin_;
        bit8pin = bit8pin_;
        pinConfiguration = pinConfiguration_;
    }

    virtual void initialize() override {
        pinMode(bit1pin, pinConfiguration);
        pinMode(bit2pin, pinConfiguration);
        pinMode(bit4pin, pinConfiguration);
        pinMode(bit8pin, pinConfiguration);
        setInputUpdateFrequency(10.0);
    }

    virtual bool updateInputs() override {
        if(!IODevice::updateInputs()) return false;
        uint8_t newPosition = readPosition();
        if(newPosition != position) {
            b_positionChanged = true;
            lastInputChange_millis = millis();
        }
        position = newPosition;
        return true;
    };

    bool positionChanged(){
        if(b_positionChanged){
            b_positionChanged = false;
            return true;
        }
        return false;
    }

    uint8_t getPosition(){ return position; }

private:

    uint8_t readPosition(){
        bool b1 = digitalRead(bit1pin);
        bool b2 = digitalRead(bit2pin);
        bool b4 = digitalRead(bit4pin);
        bool b8 = digitalRead(bit8pin);
        if(pinConfiguration == INPUT_PULLUP){
            b1 = !b1;
            b2 = !b2;
            b4 = !b4;
            b8 = !b8;
        }
        return (b8 << 3) | (b4 << 2) | (b2 << 1) | (b1 << 0);
    }

    uint8_t bit1pin;
    uint8_t bit2pin;
    uint8_t bit4pin;
    uint8_t bit8pin;
    uint8_t pinConfiguration;
    uint8_t position;
    uint8_t b_positionChanged = false;
};


class BatteryLevel : public IODevice{
public:

    BatteryLevel(uint8_t analogInputPin_, float voltageDividerFactor_, float minVoltage_, float maxVoltage_){
        inputPin = analogInputPin_;
        voltageDividerFactor = voltageDividerFactor_;
        minVoltage = minVoltage_;
        maxVoltage = maxVoltage_;
    }

    virtual void initialize() override {
        pinMode(inputPin, INPUT);
        voltage = readingToVoltage(analogRead(inputPin));
        level = voltageToLevel(voltage);
        setInputUpdateFrequency(10.0);
    }

    virtual bool updateInputs() override {
        if(!IODevice::updateInputs()) return false;

        float newVoltage = readingToVoltage(analogRead(inputPin));
        voltage = voltage * smoothingFilter + newVoltage * (1.f - smoothingFilter);
        level = voltageToLevel(voltage);

        return true;
    }

    float getLevel(){ return level; }
    float getVoltage(){ return voltage; }

    float readingToVoltage(uint16_t reading){
        return 3.3 * voltageDividerFactor * float(reading) / getAnalogReadResolution();
    }

    float voltageToLevel(float voltage){
        float output = map(voltage, minVoltage, maxVoltage, 0.0, 1.0);
        if(output <= 0.0) return 0.0;
        else if(output >= 1.0) return 1.0;
        return output;
    }

private:

    uint8_t inputPin;
    float voltageDividerFactor;
    float minVoltage;
    float maxVoltage;
    float voltage;
    float level;
    float smoothingFilter = 0.95;

};