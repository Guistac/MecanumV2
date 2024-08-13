#include "RemoteBoard.h"

static float easeInOutCubic(float in){
    return in < 0.5 ? (4 * pow(in,3)) : (1 - pow(-2 * in + 2, 3) / 2);
};

void RemoteBoard::refreshGui(){
    display.clearDisplay();

    int x = map(radio.getSignalStrength(), -120, 4, 0, 127);
    if(robotProxy.b_connected) display.fillRect(0,0,x,4,WHITE);
    display.drawLine(0,0,0,5,WHITE);
    display.drawLine(127,0,127,5,WHITE);

    display.setTextColor(WHITE);
    display.setTextSize(2);
    display.setCursor(0,7);
    display.printf("Ch%X", radio.getChannel());
    if(robotProxy.b_connected) {
        display.setCursor(36,7);
        switch(robotProxy.rxCounter % 4){
            /*
            case 0: display.print("\x07"); break;
            case 1: display.print("\x04"); break;
            case 2: display.print("\x0F"); break;
            case 3: display.print("\x04"); break;
            */
            case 0: display.print("\x10"); break;
            case 1: display.print("\x1E"); break;
            case 2: display.print("\x11"); break;
            case 3: display.print("\x1F"); break;
        }
        display.setCursor(50,7);
        display.printf("%i", radio.getSignalStrength());
    }
    else {
        display.setCursor(36,7);
        uint32_t t = millis() % 400;
        const char* txt;
        if(t > 300) txt = "...";
        else if(t > 200) txt = ".. ";
        else if(t > 100) txt = ".  ";
        else txt = "   ";
        display.print(txt);
    }
    if(b_editingRadioChannel) display.drawLine(0, 22, 33, 22, millis()%200 > 100 ? WHITE : BLACK);
    else if(b_channelSelectorButtonsPressed) {
        double t = min(leftPushbutton.timeSinceLastInputChangeSeconds(), rightPushbutton.timeSinceLastInputChangeSeconds());
        double n = t/frequencyEditPressDuration;
        display.drawLine(0,22,33*n,22,WHITE);
    }
    
    display.setTextSize(1);
    display.setCursor(0,25);
    display.printf("B:%.2fV T:%iC", batteryLevel.getVoltage(), radio.getTemperatureCelsius());

    inputDeviceTestGui(88,26);

    if(!displayRevealTimer.hasExpired()){
        int w = 128;
        int x = easeInOutCubic(displayRevealTimer.getProgress()) * w;
        display.fillRect(x,0,w-x,32,BLACK);
    }

    display.display();
}

void RemoteBoard::inputDeviceTestGui(int x, int y){
    auto drawDigitalInputState = [this](int x, int y){
        display.drawPixel(x+0, y + (leftJoystick.isPressed() ? 1 : 0), WHITE);
        display.drawPixel(x+1, y + (leftPushbutton.isPressed() ? 1 : 0), WHITE);
        display.drawPixel(x+2, y + (leftLedButton.isPressed() ? 1 : 0), WHITE);
        switch(speedToggle.getState()){
            case 0: display.drawPixel(x+3, y+1, WHITE); break;
            case 1: display.drawPixel(x+3, y, WHITE); break;
            case 2: display.drawPixel(x+3, y-1, WHITE); break;
        }
        display.drawPixel(x+4, y, WHITE);
        display.drawPixel(x+5, y + (directionToggle.getSwitchState() ? 0 : 1), WHITE);
        display.drawPixel(x+6, y + (rightLedButton.isPressed() ? 1 : 0), WHITE);
        display.drawPixel(x+7, y + (rightPushbutton.isPressed() ? 1 : 0), WHITE);
        display.drawPixel(x+8, y + (rightJoystick.isPressed() ? 1 : 0), WHITE);
    };
    auto drawAnalogValue = [this](float value, int x, int y, int w, bool flip){
        int xPos = round(map(value, -1.0, 1.0, float(x), float(x+w)));
        int yL = flip ? y : y+1;
        int yD = flip ? y+1 : y;
        display.drawLine(x,yL,x+w,yL,WHITE);
        display.drawPixel(xPos,yD,WHITE);
        if(value == 0.0) display.drawPixel(xPos,yL,BLACK);
        else if(value == -1.0) display.drawPixel(xPos,yL,BLACK);
        else if(value == 1.0) display.drawPixel(xPos,yL,BLACK);
    };
    drawAnalogValue(leftJoystick.getXValue(), x, y, 10, false);
    drawAnalogValue(leftJoystick.getYValue(), x, y+3, 10, true);
    drawDigitalInputState(x+12,y+2);
    drawAnalogValue(rightJoystick.getXValue(), x+22, y, 10, false);
    drawAnalogValue(rightJoystick.getYValue(), x+22, y+3, 10, true);
}

void RemoteBoard::startupAnimation(){

    FrequencyTimer frameDrawEvent(200.0);

    auto showStartupGraphic = [this](){
        display.setTextColor(BLACK);
        display.setTextSize(3);
        display.setCursor(22,-1);
        display.print("B");
        display.setTextSize(2);
        display.print("ecker");
        display.setCursor(42,15);
        display.setTextSize(3);
        display.print("P");
        display.setTextSize(2);
        display.print("oels");
    };

    LengthTimer screenFillTimer(.5);
    while(!screenFillTimer.hasExpired()){
        float ll;
        float rl;
        if(screenFillTimer.getProgress() < 0.5){
            ll = constrain(map(screenFillTimer.getProgress(), 0.0, 0.25, 1.0, 0.0), 0.0, 1.0);
            rl = constrain(map(screenFillTimer.getProgress(), 0.25, 0.5, 1.0, 0.0), 0.0, 1.0);
        }else{
            ll = constrain(map(screenFillTimer.getProgress(), 0.5, 0.75, 0.0, 1.0), 0.0, 1.0);
            rl = constrain(map(screenFillTimer.getProgress(), 0.75, 1.0, 0.0, 1.0), 0.0, 1.0);
        }
        leftLedButton.setBrightness(ll);
        rightLedButton.setBrightness(rl);
        if(frameDrawEvent.shouldTrigger()){
            display.clearDisplay();
            int w = easeInOutCubic(screenFillTimer.getProgress()) * 127;
            display.fillRect(0,0,w,31,WHITE);
            showStartupGraphic();
            display.display();
        }
    }

    LengthTimer screenClearTimer(.25);
    while(!screenClearTimer.hasExpired()){
        float ll = constrain(map(screenClearTimer.getProgress(), 0.0, 0.5, 1.0, 0.0), 0.0, 1.0);
        float rl = constrain(map(screenClearTimer.getProgress(), 0.5, 1.0, 1.0, 0.0), 0.0, 1.0);
        leftLedButton.setBrightness(ll);
        rightLedButton.setBrightness(rl);
        if(frameDrawEvent.shouldTrigger()){
            display.clearDisplay();
            int w = 127 - easeInOutCubic(screenClearTimer.getProgress()) * 127;
            display.fillRect(127-w,0,w,31, WHITE);
            showStartupGraphic();
            display.display();
        }
    }
}

void RemoteBoard::radioInitFailedScreen(){
    display.setTextSize(1);
    display.setTextColor(WHITE);
    display.setCursor(5,8);
    display.print("radio init failed...");
    display.setCursor(5,16);
    display.print("please reboot remote");
    display.display();
    while(true){
        leftLedButton.turnOff();
        rightLedButton.turnOn();
        delay(500);
        leftLedButton.turnOn();
        rightLedButton.turnOff();
        delay(500);
    }
}

void RemoteBoard::releaseJoysticksScreen(bool released){
    display.clearDisplay();
    if(!released){
        display.setTextSize(1);
        display.setTextColor(WHITE);
        display.setCursor(2,12);
        const char* dots = (millis()%300 < 100) ? "." : ((millis()%300 < 200) ? ".." : "...");
        display.printf("Release joysticks %s", dots);
    }
    else{

        display.setTextColor(WHITE);
        display.setTextSize(1);
        display.setCursor(0,0);
        display.printf("\x3\x3\x3\x3\x3\x3\x3\x3\x3\x3\x3\x3\x3\x3\x3\x3\x3\x3\x3\x3\x3");
        display.setCursor(0,24);
        display.printf("\x3\x3\x3\x3\x3\x3\x3\x3\x3\x3\x3\x3\x3\x3\x3\x3\x3\x3\x3\x3\x3");
        display.setTextSize(2);
        display.setCursor(4,9);
        display.printf("Thank You!");
    }
    display.display();
}