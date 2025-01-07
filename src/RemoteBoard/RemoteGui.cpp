#include "RemoteBoard.h"
#include "Utilities/RobotStates.h"

static float easeInOutCubic(float in){
    return in < 0.5 ? (4 * pow(in,3)) : (1 - pow(-2 * in + 2, 3) / 2);
};

static uint8_t frameCounter = 0;


void RemoteBoard::drawSignalBar(int x, int y, int w, int h){
    display.drawLine(x,y,x,y+h,WHITE);
    display.drawLine(x+w,y,x+w,y+h,WHITE);
    int barWidth = map(radio.getSignalStrength(), -120, 4, 0, w);
    if(robotProxy.b_connected) display.fillRect(x,2,barWidth,3,WHITE);
    else{
        float o = sin(millis()*0.005);
        int halfW = w * 0.5;
        int center = x + halfW;
        int offset = o * halfW;
        int pointWidth = (1.0 - (o < 0.0 ? -o : o)) * 50.0;
        display.fillRect(center + 1 + offset - pointWidth*0.5, 2, pointWidth, 3, WHITE);
    }
}

void RemoteBoard::refreshGui(){
    display.clearDisplay();

    frameCounter++;
    if(frameCounter == 32) frameCounter = 0;
    if(frameCounter >= 16) display.drawPixel(127, 31 - (31 - frameCounter), WHITE);
    else display.drawPixel(127, 31 - frameCounter, WHITE);

    display.setCursor(0,0);
    display.setTextSize(1);
    
    display.printf("Ch#%X", radio.getChannel());
    if(b_editingRadioChannel) {
        display.drawLine(0, 8, 22, 8, (millis() % 200) > 100 ? WHITE : BLACK);
    }
    else if(b_channelSelectorButtonsPressed) {
        double t = min(leftPushbutton.timeSinceLastInputChangeSeconds(), rightPushbutton.timeSinceLastInputChangeSeconds());
        double n = t/frequencyEditPressDuration;
        display.drawLine(0,8,22*n,8,WHITE);
    }



    display.setCursor(26,0);
    switch(robotProxy.rxCounter % 4){
        case 0: display.print("\x10"); break;
        case 1: display.print("\x1E"); break;
        case 2: display.print("\x11"); break;
        case 3: display.print("\x1F"); break;
    }

    drawSignalBar(34, 0, 93, 6);

    display.setTextColor(WHITE);
    display.setTextSize(2);
    display.setCursor(0,9);

    if(robotProxy.b_connected){
        switch(robotProxy.status){
            case uint8_t(RobotState::DISABLING):
                display.setCursor(12,9);
                display.print("Disabling");
                break;
            case uint8_t(RobotState::DISABLED):
                display.setCursor(18,9);
                display.print("Disabled");
                break;
            case uint8_t(RobotState::ENABLING):
                display.setCursor(18,9);
                display.print("Enabling");
                break;
            case uint8_t(RobotState::ENABLED):
                display.setCursor(22,9);
                display.print("Enabled");
                break;
            case uint8_t(RobotState::EMERGENCY_STOPPING):
                if(millis() % 100 > 50){
                    display.setCursor(36,9);
                    display.print("ESTOP");
                }
                break;
            case uint8_t(RobotState::EMERGENCY_STOPPED):
                if(millis() % 500 > 250){
                    display.setCursor(36,9);
                    display.print("ESTOP");
                }
                break;
            default:
                display.setCursor(36,9);
                display.print("???");
                break;
        }
    }
    else{
        display.setCursor(22,9);
        display.print("Offline");
    }
    
    display.setTextSize(1);
    display.setCursor(0,25);
    display.printf("T:%.2fV", batteryLevel.getVoltage());
    display.setCursor(48, 25);
    if(robotProxy.b_connected) display.printf("R:%.1fV", robotProxy.batteryVoltage);
    else display.printf("R:xx.xV");

    inputDeviceTestGui(95,26);

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
        display.drawPixel(x+4, y + (estopButton.isClear() ? 0 : 1), WHITE);
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