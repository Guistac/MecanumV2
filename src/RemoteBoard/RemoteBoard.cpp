#include "RemoteBoard.h"
#include "Utilities/SafetyNumber.h"
#include <EEPROM.h>


bool RemoteBoard::areJoysticksReleased(){
    float joystickReleasedThreshold = 0.1;
    return leftJoystick.isRawBelowThreshold(joystickReleasedThreshold) && rightJoystick.isRawBelowThreshold(joystickReleasedThreshold);
}

void RemoteBoard::saveRadioChannel(uint8_t channel){
    if(channel == savedRadioChannel) return;
    EEPROM.write(0x0, channel);
    savedRadioChannel = channel;
}

uint8_t RemoteBoard::loadRadioChannel(){
    savedRadioChannel = EEPROM.read(0x0);
    return savedRadioChannel;
}



void RemoteBoard::initialize(){

    //configure io devices, turn on leds to signal success
    for(auto ioDevice : ioDevices) ioDevice->initialize();
    leftLedButton.turnOn();
    rightLedButton.turnOn();

    //configure display
    display.initialize();
    delay(150); //before this the display won't show anything

    //show a nice startup animation
    startupAnimation();

    //now that we waited a bit for power to stabilize, we can start the radio
    //this fails if we do it too soon after powerup
    if(!radio.initialize(10)) radioInitFailedScreen();
    //load the radio channel setting saved on eeprom
    radio.setChannel(loadRadioChannel());

    //check if the joysticks are released to zero them out
    bool showJoystickReleaseThankYou = false;
    if(!areJoysticksReleased()){
        while(!areJoysticksReleased()){
            if(guiRefreshEvent.shouldTrigger()) releaseJoysticksScreen(false);
        }
        showJoystickReleaseThankYou = true;
    }
    leftJoystick.captureZero();
    rightJoystick.captureZero();
    if(showJoystickReleaseThankYou){
        releaseJoysticksScreen(true);
        delay(500);
    }

    displayRevealTimer.setLength(.4);
    displayRevealTimer.start();
}


void RemoteBoard::update(){
    //update all input devices
    for(auto ioDevice : ioDevices) ioDevice->updateInputs();



    if(!b_editingRadioChannel && leftPushbutton.isPressed() && rightPushbutton.isPressed()){
        b_channelSelectorButtonsPressed = true;
        if(leftPushbutton.timeSinceLastInputChangeSeconds() > frequencyEditPressDuration &&
        rightPushbutton.timeSinceLastInputChangeSeconds() > frequencyEditPressDuration){
            b_editingRadioChannel = true;
        }
    }
    else if(b_editingRadioChannel){
        if(!leftPushbutton.isPressed() || !rightPushbutton.isPressed()) b_channelSelectorButtonsPressed = false;
        if(leftPushbutton.wasPushed()) {
            radio.setPreviousChannel();
            radioSendEvent.forceTrigger();
        }
        else if(rightPushbutton.wasPushed()){
            radio.setNextChannel();
            radioSendEvent.forceTrigger();
        }
        if(!b_channelSelectorButtonsPressed && leftPushbutton.isPressed() && rightPushbutton.isPressed()){
            b_channelSelectorButtonsPressed = true;
            b_editingRadioChannel = false;
            saveRadioChannel(radio.getChannel());
        }
    }else b_channelSelectorButtonsPressed = false;

    //Update LEDs
    if(robotProxy.functionButtonPressed){
        leftLedButton.set(millis() % 100 < 50);
        rightLedButton.set(millis() % 100 > 50);
    }else{
        leftLedButton.set(leftLedButton.isPressed());
        rightLedButton.set(rightLedButton.isPressed());
    }

    //display refresh
    if(guiRefreshEvent.shouldTrigger()) refreshGui();

    //radio temperature
    if(temperatureReadingEvent.shouldTrigger()) radio.updateTemperature();

    //radio send and receive
    sendData();
    receiveData();
}


void RemoteBoard::sendData(){
    //radio send
    if(radioSendEvent.shouldTrigger()){

        uint8_t header = 0x7; //header is 0b111
        bool b_shouldDisable = leftLedButton.isPressed();
        bool b_shouldEnable = rightLedButton.isPressed();
        uint8_t speedSelection = speedToggle.getState();
        bool b_directionToggle = directionToggle.getSwitchState();
        bool b_leftButton = leftPushbutton.isPressed();
        bool b_rightButton = rightPushbutton.isPressed();
        bool b_leftJoystickPressed = leftJoystick.isPressed();
        bool b_rightJoystickPressed = rightJoystick.isPressed();
        robotProxy.txCounter++;

        uint16_t controlWord = header & 0x7; //3 bit header
        if(b_shouldDisable)         controlWord |= 0x8;
        if(b_shouldEnable)          controlWord |= 0x10;
        controlWord |= (speedSelection & 0x3) << 5;
        if(b_directionToggle)       controlWord |= 0x80;
        if(b_leftButton)            controlWord |= 0x100;
        if(b_rightButton)           controlWord |= 0x200;
        if(b_leftJoystickPressed)   controlWord |= 0x400;
        if(b_rightJoystickPressed)  controlWord |= 0x800;
        controlWord |= (robotProxy.txCounter & 0xF) << 12;

        uint16_t safetyNumber = Safety::generateClearNumber();
        //else safetyNumber = Safety::generateSafeNumber();

        uint8_t outgoingFrame[8];
        outgoingFrame[0] = controlWord & 0xFF;
        outgoingFrame[1] = (controlWord >> 8) & 0xFF;
        outgoingFrame[2] = map(leftJoystick.getXValue(), -1.0, 1.0, 0, 255);
        outgoingFrame[3] = map(leftJoystick.getYValue(), -1.0, 1.0, 0, 255);
        outgoingFrame[4] = map(rightJoystick.getXValue(), -1.0, 1.0, 0, 255);
        outgoingFrame[5] = map(rightJoystick.getYValue(), -1.0, 1.0, 0, 255);
        outgoingFrame[6] = safetyNumber;
        outgoingFrame[7] = safetyNumber >> 8;

        radio.send(outgoingFrame, 8);
        robotProxy.lastSendTimeMicros = micros();
        
    }
}

void RemoteBoard::receiveData(){
    //radio listen
    uint8_t incomingFrame[128];
    uint8_t length = 128;
    if(radio.receive(incomingFrame, &length)){
        robotProxy.rxCounter++;
        if(length != 8) return;

        uint16_t statusword = incomingFrame[0] | incomingFrame[1] << 8;

        uint8_t header = statusword & 0x7;
        if(header != 0x7) return;
        robotProxy.b_connected = true;
        radioSendEvent.setFrequency(cyclicSendFrequency);
        robotProxy.lastReceiveTime = millis();

        robotProxy.status = (statusword >> 3) & 0x7;
        robotProxy.speedModeDisplay = (statusword >> 6) & 0x3;
        robotProxy.directionModeDisplay = statusword & 0x100;
        robotProxy.functionButtonPressed = statusword & 0x200;
        //two bytes free 0b00001100 00000000
        uint8_t mirroredTxCounter = (statusword >> 12) & 0xF;

        uint8_t motorStatusWord = incomingFrame[2];
        robotProxy.frontLeft_alarm =       motorStatusWord & 0x1;
        robotProxy.backLeft_alarm =        motorStatusWord & 0x2;
        robotProxy.frontRight_alarm =      motorStatusWord & 0x4;
        robotProxy.backRight_alarm =       motorStatusWord & 0x8;
        robotProxy.frontLeft_enabled =     motorStatusWord & 0x10;
        robotProxy.backLeft_enabled =      motorStatusWord & 0x20;
        robotProxy.frontRight_enabled =    motorStatusWord & 0x40;
        robotProxy.backRight_enabled =     motorStatusWord & 0x80;
    
        int8_t xVelocity_i8 = incomingFrame[3];
        int8_t yVelocity_i8 = incomingFrame[4];
        int8_t rVelocity_i8 = incomingFrame[5];
        robotProxy.xVelocity = map(float(xVelocity_i8), -127.0, 127.0, -1.0, 1.0);
        robotProxy.yVelocity = map(float(yVelocity_i8), -127.0, 127.0, -1.0, 1.0);
        robotProxy.rVelocity = map(float(rVelocity_i8), -127.0, 127.0, -1.0, 1.0);

        robotProxy.robotRxSignalStrength = incomingFrame[6] - 150;
        robotProxy.batteryVoltage = incomingFrame[7];

        if((robotProxy.txCounter & 0xF) == mirroredTxCounter){
            uint32_t sendToReceiveTimeMicros = micros() - robotProxy.lastSendTimeMicros;
            robotProxy.lastMessageRoundTripTimeMillis = double(sendToReceiveTimeMicros) / 1000.0;
            //Serial.printf("msg#%i round trip: %.1fms\n", robotProxy.txCounter, robotProxy.lastMessageRoundTripTimeMillis);
        }
    }

    if(millis() - robotProxy.lastReceiveTime > 500) {
        robotProxy.b_connected = false;
        radioSendEvent.setFrequency(pingSendFrequency);
    }
}

