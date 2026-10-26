
#include "RobotBoard.h"

#include "Utilities/SafetyNumber.h"

#include "common/mavlink.h"
#include "MavLink.h"

ServoMotor frontLeftServo(
    FRONTLEFT_PULSE_PIN,
    FRONTLEFT_DIRECTION_PIN,
    FRONTLEFT_ENABLE_PIN,
    FRONTLEFT_ALARM_PIN,
    FRONTLEFT_PENDING_PIN);
ServoMotor frontRightServo(
    FRONTRIGHT_PULSE_PIN,
    FRONTRIGHT_DIRECTION_PIN,
    FRONTRIGHT_ENABLE_PIN,
    FRONTRIGHT_ALARM_PIN,
    FRONTRIGHT_PENDING_PIN);
ServoMotor backLeftServo(
    BACKLEFT_PULSE_PIN,
    BACKLEFT_DIRECTION_PIN,
    BACKLEFT_ENABLE_PIN,
    BACKLEFT_ALARM_PIN,
    BACKLEFT_PENDING_PIN);
ServoMotor backRightServo(
    BACKRIGHT_PULSE_PIN,
    BACKRIGHT_DIRECTION_PIN,
    BACKRIGHT_ENABLE_PIN,
    BACKRIGHT_ALARM_PIN,
    BACKRIGHT_PENDING_PIN);
std::vector<ServoMotor*> motors = {
  &frontLeftServo,
  &frontRightServo,
  &backLeftServo,
  &backRightServo
};

void enableAllMotors(){
  for(auto motor : motors) motor->enable();
}

void disableAllMotors(){
  for(auto motor : motors) motor->disable();
}

bool areAllMotorsEnabled(){
  for(auto motor : motors) {
    if(!motor->isEnabled()) return false;
  }
  return true;
}

bool hasAnyMotorFault(){
  for(auto motor : motors) {
    if(motor->hasAlarm()) return true;
  }
  return false;
}

void updateAllMotors(){
  for(auto motor : motors) motor->update();
}

void RobotBoard::initialize(){

  mavlink = new Mavlink();
  mavlink->setSources(&remoteProxy, &batteryReading, configuration.batteryCellCount);

  batteryReading.setVoltageDividerFactor(configuration.batteryVoltageDividerFactor);
  for(auto device : ioDevices) device->initialize();

  frontLeftServo.initialize([]{   frontLeftServo.isr(); });
  frontRightServo.initialize([]{  frontRightServo.isr(); });
  backLeftServo.initialize([]{    backLeftServo.isr(); });
  backRightServo.initialize([]{   backRightServo.isr(); });
  for(auto motor : motors) motor->disable();

  frontLeftServo.wheelPosition_mm =  configuration.frontLeft_wheelPosition_mm;
  backLeftServo.wheelPosition_mm =   configuration.backLeft_wheelPosition_mm;
  frontRightServo.wheelPosition_mm = configuration.frontRight_wheelPosition_mm;
  backRightServo.wheelPosition_mm =  configuration.backRight_wheelPosition_mm;
  frontLeftServo.wheelFrictionVector_mmPerRev =   configuration.frontLeft_wheelFrictionVector_mmprev;
  backLeftServo.wheelFrictionVector_mmPerRev =    configuration.backLeft_wheelFrictionVector_mmprev;
  frontRightServo.wheelFrictionVector_mmPerRev =  configuration.frontRight_wheelFrictionVector_mmprev;
  backRightServo.wheelFrictionVector_mmPerRev =   configuration.backRight_wheelFrictionVector_mmprev;

  if(!radio.initialize(100)) {
    if(DEBUG_PRINTS) Serial.println("Failed Radio Configuration");
    return;
  }
  if(DEBUG_PRINTS) Serial.println("Configured Radio");

  if(!motionControl.configure(configuration)){
    if(DEBUG_PRINTS) Serial.println("Failed Motion Control Configuration");
    return;
  }
  if(DEBUG_PRINTS) Serial.println("Configured Motion Control");

  startupAnimation();
  delay(100);
}

void RobotBoard::startupAnimation(){
  LengthTimer startupTimer(1.0);

  while(!startupTimer.hasExpired()){
    int ledCount = leds.size();
    float ts = 1.0 / ledCount;
    float ol = 4.0;
    ts /= 1.0 + ts * ol;

    float t = startupTimer.getProgress();
    for(int i = 0; i < ledCount; i++){
      float lt = i * ts;
      float mlt = lt + ts * (1.0 + ol);
      if(t >= lt && t <= mlt) {
        float lp = map(t, lt, mlt, 0.0, 1.0);
        float br;
        if(lp < 0.5) br = map(lp, 0.0, 0.5, 0.0, 1.0);
        else br = map(lp, 0.5, 1.0, 1.0, 0.0);
        leds[i]->setBrightness(br);
      }
      else leds[i]->turnOff();
    }
    delay(10);
  }
  for(auto led : leds) led->turnOff();
}

FrequencyTimer timer_HEARTBEAT(5.0);
FrequencyTimer timer_POSITION_TARGET_LOCAL_NED(50.0);
mavlink_message_t msg;
uint8_t buffer[MAVLINK_MAX_PACKET_LEN];
bool msgrcv = false;
LengthTimer heartbeatTimeout(0.5);
bool b_mavConnected = false;

void RobotBoard::update(){
  updateIO();
  //re-init the radio after a chip reset, else the link stays down until a power cycle (docs/radio/05 T9)
  radio.update();
  receiveData();
  updateRobotLogic();
  updateAllMotors();
  motionControl.updateMotionControl();

  mavlink->update();
  statusLED.set(mavlink->b_online);
}

void RobotBoard::receiveData(){
  uint8_t incomingBuffer[20];
  uint8_t length = 20;
  if(radio.receive(incomingBuffer, &length)){
    if(!decodeRemoteFrame(incomingBuffer, length, remoteProxy)) return;

    remoteProxy.lastReceiveTimeMillis = millis();
    if(!remoteProxy.b_connected){
      if(DEBUG_PRINTS) Serial.printf("Remote Connected! (%i)\n", millis());
      remoteProxy.b_connected = true;
    }

    if(DEBUG_PRINTS && false){
      Serial.printf("%.2f %.2f %.2f %.2f\n",
        remoteProxy.leftJoystickX,
        remoteProxy.leftJoystickY,
        remoteProxy.rightJoystickX,
        remoteProxy.rightJoystickY);
      Serial.printf("dis%i ena%i ss%i dir%i lb%i rb%i lj%i rj%i count%i\n",
        remoteProxy.b_shouldDisable,
        remoteProxy.b_shouldEnable,
        remoteProxy.speedSelection,
        remoteProxy.b_directionToggle,
        remoteProxy.b_leftButton,
        remoteProxy.b_rightButton,
        remoteProxy.b_leftJoystick,
        remoteProxy.b_rightJoystick,
        remoteProxy.txCounter);
    }

    sendData();
  }
  else if(remoteProxy.b_connected && millis() - remoteProxy.lastReceiveTimeMillis > 500) {
    remoteProxy.b_connected = false;
    if(DEBUG_PRINTS) Serial.printf("Remote Disconnected... (%i)\n", millis());
  }
}

void RobotBoard::sendData(){
  uint8_t outBuffer[8];

  uint8_t header = 0x7;
  uint16_t statusword = header & 0x7; //3 bit header
  if(mavlink->b_online) statusword |= (uint8_t(mavlink->stacatoState) & 0x7) << 3;
  else statusword |= (uint8_t(state) & 0x7) << 3;
  statusword |= (uint8_t(motionControl.speedMode) & 0x3) << 6;
  if(motionControl.directionMode) statusword |= 0x100;
  if(function1Button.isPressed()) statusword |= 0x200;
  //two bytes free 0b00001100 00000000
  statusword |= (remoteProxy.txCounter & 0xF) << 12;

  uint8_t motorStatusWord = 0x0;
  if(frontLeftServo.hasAlarm())     motorStatusWord |= 0x1;
  if(backLeftServo.hasAlarm())      motorStatusWord |= 0x2;
  if(frontRightServo.hasAlarm())    motorStatusWord |= 0x4;
  if(backRightServo.hasAlarm())     motorStatusWord |= 0x8;
  if(frontLeftServo.isEnabled())    motorStatusWord |= 0x10;
  if(backLeftServo.isEnabled())     motorStatusWord |= 0x20;
  if(frontRightServo.isEnabled())   motorStatusWord |= 0x40;
  if(backRightServo.isEnabled())    motorStatusWord |= 0x80;

  outBuffer[0] = statusword & 0xFF;
  outBuffer[1] = (statusword >> 8) & 0xFF;
  outBuffer[2] = motorStatusWord;
  outBuffer[3] = map(motionControl.getXVelocityNormalized(), -1.0, 1.0, -127, 127);
  outBuffer[4] = map(motionControl.getYVelocityNormalized(), -1.0, 1.0, -127, 127);
  outBuffer[5] = map(motionControl.getRVelocityNormalized(), -1.0, 1.0, -127, 127);
  outBuffer[6] = radio.getSignalStrength() + 150;
  //45..70V on one byte for the handheld display; clamped so an out of range voltage can't wrap
  outBuffer[7] = uint8_t(constrain(map(batteryReading.getVoltage(), 45.0, 70.0, 0.0, 255.0), 0.0, 255.0));

  radio.send(outBuffer, 8);
}

void RobotBoard::updateIO(){
  for(auto device : ioDevices) device->updateInputs();

  if(channelSelector.positionChanged()) radio.setChannel(channelSelector.getPosition());

  //blink status led
  static FrequencyTimer ledTimer(100.0);
  //if(ledTimer.shouldTrigger()) statusLED.setBrightness(map(sin(float(millis()) * 0.01), -1.0, 1.0, 0.0, 1.0));

  functionLED.set(function2Button.isPressed());


  //refresh motor leds
  double channelShowTime = 2.0;
  if(channelSelector.timeSinceLastInputChangeSeconds() < channelShowTime){
    uint8_t pos = channelSelector.getPosition();
    float br = map(channelShowTime - channelSelector.timeSinceLastInputChangeSeconds(), 0, channelShowTime, 0, 1.0);
    if(pos != 0){
      brLED.setBrightness((pos & 0x8) ? br : 0);
      blLED.setBrightness((pos & 0x4) ? br : 0);
      frLED.setBrightness((pos & 0x2) ? br : 0);
      flLED.setBrightness((pos & 0x1) ? br : 0);
    }
    else{
      uint32_t period = 200;
      uint32_t t = millis() % period;
      brLED.setBrightness((t > 0 && t < period / 4) ? br : 0);
      blLED.setBrightness((t > period / 4 && t < period / 2) ? br : 0);
      frLED.setBrightness((t > period / 2 && t < 3 * period / 4) ? br : 0);
      flLED.setBrightness((t > 3 * period / 4 && t < period) ? br : 0);
    }
  }
  else{
    auto setMotorLed = [](ServoMotor* motor, LED* led){
      if(motor->isEnabled()) led->turnOn();
      else if(motor->isEnabling()) led->set(millis() % 200 > 100 ? true : false);
      else led->turnOff();
    };
    setMotorLed(&backRightServo, &brLED);
    setMotorLed(&backLeftServo, &blLED);
    setMotorLed(&frontRightServo, &frLED);
    setMotorLed(&frontLeftServo, &flLED);
  }

  //refresh remote led
  if(remoteProxy.b_connected) remoteLED.setBrightness(remoteProxy.txCounter % 2 == 0 ? 1.0 : 0.2);
  else remoteLED.turnOff();
}

void RobotBoard::updateRobotLogic(){

  if(!remoteProxy.b_safetyClear){
      if(state != RobotState::EMERGENCY_STOPPED && state != RobotState::EMERGENCY_STOPPING){
          motionControl.requestEmergencyDeceleration();
          state = RobotState::EMERGENCY_STOPPING;
          if(DEBUG_PRINTS) Serial.println("——— Emergency Stop Requested.");
      }
  }

  if(remoteProxy.speedSelection != motionControl.speedMode) motionControl.setSpeedMode(remoteProxy.speedSelection);

  motionControl.setSpeedMode(remoteProxy.speedSelection);

  switch(state){
      case RobotState::ENABLING:{
          uint32_t timeSinceEnableRequestMillis = millis() - enableRequestMillis;
          bool b_timeout = timeSinceEnableRequestMillis > 2000;
          if(areAllMotorsEnabled() && !hasAnyMotorFault() && timeSinceEnableRequestMillis > 1500){
              state = RobotState::ENABLED;
              if(DEBUG_PRINTS){
                Serial.printf("——— Robot Enabled. (took %ims)\n", timeSinceEnableRequestMillis);
                Serial.printf("——— FL:%i FR:%i BL:%i BR:%i\n", frontLeftServo.isEnabled(), frontRightServo.isEnabled(), backLeftServo.isEnabled(), backRightServo.isEnabled());
              }
          }
          else if(remoteProxy.b_shouldDisable || !remoteProxy.b_connected || timeSinceEnableRequestMillis > 2000){
              disableAllMotors();
              state = RobotState::DISABLED;
              if(DEBUG_PRINTS){
                if(b_timeout)                         Serial.println("——— Enable Request Timed Out.");
                else if(!remoteProxy.b_connected)     Serial.println("——— Enable Request Aborted : remote was disconnected.");
                else if(remoteProxy.b_shouldDisable)  Serial.println("——— Cancelled Enable Request.");
              }
          }
      }break;
      case RobotState::ENABLED:{
          if(hasAnyMotorFault() || !areAllMotorsEnabled() || remoteProxy.b_shouldDisable || !remoteProxy.b_connected){
              motionControl.setControlTargetsNormalized(0.0,0.0,0.0);
              state = RobotState::DISABLING;
              if(DEBUG_PRINTS){
                if(hasAnyMotorFault())                Serial.println("——— Disabling Robot : Not all servos were alarm free.");
                else if(!areAllMotorsEnabled()){
                  Serial.println("——— Disabling Robot : Not all servos were enabled.");
                  Serial.printf("——— FL:%i FR:%i BL:%i BR:%i\n", frontLeftServo.isEnabled(), frontRightServo.isEnabled(), backLeftServo.isEnabled(), backRightServo.isEnabled());
                }
                else if(!remoteProxy.b_connected)     Serial.println("——— Disabling Robot : Remote was disconnected.");
                else if(remoteProxy.b_shouldDisable)  Serial.println("——— Disabling Robot by user request.");
              }
          }
          else {
              float xCommand = remoteProxy.leftJoystickX;
              float yCommand = remoteProxy.leftJoystickY;
              float rCommand = remoteProxy.rightJoystickX;
              if(!remoteProxy.b_directionToggle){
                xCommand = -xCommand;
                yCommand = -yCommand;
              }
              motionControl.setControlTargetsNormalized(xCommand, yCommand, rCommand);
          }
      }break;
      case RobotState::DISABLING:{
        motionControl.setControlTargetsNormalized(0.0,0.0,0.0);
          if(motionControl.isStopped()){
              disableAllMotors();
              state = RobotState::DISABLED;
              if(DEBUG_PRINTS) Serial.println("——— Robot Disabled.");
          }
      }break;
      case RobotState::DISABLED:{
          motionControl.reset();
          if(remoteProxy.b_shouldEnable) {
              enableAllMotors();
              enableRequestMillis = millis();
              state = RobotState::ENABLING;
              if(DEBUG_PRINTS) Serial.println("——— Enabling Robot ...");
          }
      }break;
      case RobotState::EMERGENCY_STOPPING:{
          motionControl.requestEmergencyDeceleration();
          if(motionControl.isStopped()){
              disableAllMotors();
              state = RobotState::EMERGENCY_STOPPED;
              if(DEBUG_PRINTS) Serial.println("——— Emergency Stopped.");
          }
      }break;
      case RobotState::EMERGENCY_STOPPED:{
          motionControl.reset();
          disableAllMotors();
          if(remoteProxy.b_safetyClear) {
              state = RobotState::DISABLED;
              if(DEBUG_PRINTS) Serial.println("——— Emergency Stop Released.");
          }
      }break;
  }
}

void RobotBoard::pinTest(){
  uint8_t outputPins[13] = {
    FRONTLEFT_ENABLE_PIN,
    FRONTLEFT_PULSE_PIN,
    FRONTLEFT_DIRECTION_PIN,
    FRONTRIGHT_ENABLE_PIN,
    FRONTRIGHT_PULSE_PIN,
    FRONTRIGHT_DIRECTION_PIN,
    BACKLEFT_ENABLE_PIN,
    BACKLEFT_PULSE_PIN,
    BACKLEFT_DIRECTION_PIN,
    BACKRIGHT_ENABLE_PIN,
    BACKRIGHT_PULSE_PIN,
    BACKRIGHT_DIRECTION_PIN,
    13
  };

  uint8_t digitalInputPins[6] = {
    ROBOT_CHANNELSELECT_1_PIN,
    ROBOT_CHANNELSELECT_2_PIN,
    ROBOT_CHANNELSELECT_4_PIN,
    ROBOT_CHANNELSELECT_8_PIN,
    ROBOT_FUNCTION1BUTTON_PIN,
    ROBOT_FUNCTION2BUTTON_PIN
  };

  uint8_t pwmPins[7] = {
    1,
    2,
    3,
    4,
    5,
    28,
    29
  };

 for(int i = 0; i < 7; i++) pinMode(pwmPins[i], OUTPUT);
 for(int i = 0; i < 6; i++) pinMode(digitalInputPins[i], INPUT_PULLUP);
 for(int i = 0; i < 13; i++) pinMode(outputPins[i], OUTPUT);
  while(true){
    for(int i = 0; i < 7; i++) analogWrite(pwmPins[i], (sin(millis() * 0.01 + 4.0 * i * 3.14 / 7.0) + 1.0) * 128.0);
    for(int i = 0; i < 13; i++) digitalWrite(outputPins[i], millis() % 1000 > 500);
    for(int i = 0; i < 6; i++) Serial.printf("%i ", digitalRead(digitalInputPins[i]));
    Serial.println();
    delay(10);
  }

}
