//Bench test firmware for the remote board: drives the RFM23BP SDN line on serial commands,
//so the supply current and the TX_ON / RX_ON levels can be measured in each radio state
//(docs/radio/05-bench-test-plan.md, T6b). Nothing else on the board is used.
//
//Commands (single characters on the USB serial, answered with one line):
//  '1'  SDN high: radio in shutdown
//  '0'  SDN low:  radio on (power-on reset, then GPIO power-on defaults)
//  'r'  read the device type register over SPI (0x08 expected when the radio is on)
//  's'  status line
//
//Pins (remote board): SDN 8, nIRQ 9, radio nSEL 10, SPI 11/12/13, OLED CS 24, OLED reset 25.

#include <Arduino.h>
#include <SPI.h>

#define RADIO_SHUTDOWN_PIN 8
#define RADIO_INTERRUPT_PIN 9
#define RADIO_SELECT_PIN 10
#define OLED_SELECT_PIN 24
#define OLED_RESET_PIN 25

static bool b_shutdown = false;

//radio held deselected and on, from a few ms after reset (before the core's 300 ms USB wait)
extern "C" void startup_middle_hook(void){
  digitalWriteFast(RADIO_SELECT_PIN, HIGH);
  pinMode(RADIO_SELECT_PIN, OUTPUT);
  digitalWriteFast(RADIO_SHUTDOWN_PIN, LOW);
  pinMode(RADIO_SHUTDOWN_PIN, OUTPUT);
}

uint8_t readRadioRegister(uint8_t address){
  SPI.beginTransaction(SPISettings(1000000, MSBFIRST, SPI_MODE0));
  digitalWriteFast(RADIO_SELECT_PIN, LOW);
  SPI.transfer(address & 0x7F);
  uint8_t value = SPI.transfer(0x00);
  digitalWriteFast(RADIO_SELECT_PIN, HIGH);
  SPI.endTransaction();
  return value;
}

void printStatus(){
  Serial.printf("status sdn=%d nirq=%d ms=%lu\n", b_shutdown ? 1 : 0, digitalReadFast(RADIO_INTERRUPT_PIN), millis());
}

void setup(){
  //display deselected and held in reset, so it draws the same current in every state
  digitalWriteFast(OLED_SELECT_PIN, HIGH);
  pinMode(OLED_SELECT_PIN, OUTPUT);
  digitalWriteFast(OLED_RESET_PIN, LOW);
  pinMode(OLED_RESET_PIN, OUTPUT);
  pinMode(RADIO_INTERRUPT_PIN, INPUT);
  SPI.begin();
  Serial.begin(115200);
  Serial.println("sdn_test ready");
}

void loop(){
  if(!Serial.available()) return;
  char command = Serial.read();
  switch(command){
    case '1':
      digitalWriteFast(RADIO_SHUTDOWN_PIN, HIGH);
      b_shutdown = true;
      printStatus();
      break;
    case '0':
      digitalWriteFast(RADIO_SHUTDOWN_PIN, LOW);
      b_shutdown = false;
      printStatus();
      break;
    case 'r':
      Serial.printf("device_type=0x%02X\n", readRadioRegister(0x00));
      break;
    case 's':
      printStatus();
      break;
    default:
      break;
  }
}
