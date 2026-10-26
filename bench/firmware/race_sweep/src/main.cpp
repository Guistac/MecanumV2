//Bench test firmware for the remote board (docs/radio/05-bench-test-plan.md, T7a):
//induces RadioHead's chip-ready race at room temperature by shifting the phase of the polls.
//
//Hypothesis (T6e): interrupt status 2 of the RFM23BP is clear-on-read. RadioHead's init polls it
//back to back (one read every 17.4 us). When chip ready is set after the chip has taken the
//register value of a read but before that read clears the flags, the flag is lost and init times out.
//
//Each trial repeats RadioHead's sequence with its own SPI functions: software reset #1, 1 ms,
//device type read, software reset #2, then (after an added delay of d ns) either
//  method "poll":  RadioHead's loop, reads of status 2 back to back until chip ready (5 ms timeout)
//  method "fixed": wait 2 ms, then read status 2 once (the flag is latched by then)
//nIRQ is recorded independently by a pin interrupt. Sweeps d from 0 to 20 us in 250 ns steps.
//
//Serial commands: 's' runs one sweep of both methods, prints one line per delay and "done".

#include <Arduino.h>
#include <SPI.h>
#include <RH_RF22.h>

#define RADIO_SHUTDOWN_PIN 8
#define RADIO_INTERRUPT_PIN 9
#define RADIO_SELECT_PIN 10
#define OLED_SELECT_PIN 24
#define OLED_RESET_PIN 25

#define TRIALS 20
#define STEP_NS 250
#define MAX_DELAY_NS 20000
#define POLL_TIMEOUT_MS 5

RH_RF22 radio(RADIO_SELECT_PIN, RADIO_INTERRUPT_PIN);

volatile bool irqSeen = false;
volatile uint32_t irqCycles = 0;

void onRadioInterrupt(){
  if(!irqSeen){
    irqCycles = ARM_DWT_CYCCNT;
    irqSeen = true;
  }
}

static inline uint32_t nsToCycles(uint32_t ns){ return uint32_t((uint64_t)ns * F_CPU_ACTUAL / 1000000000ULL); }
static inline float cyclesToUs(uint32_t cycles){ return float(cycles) * 1e6f / float(F_CPU_ACTUAL); }

static inline void waitCycles(uint32_t cycles){
  uint32_t start = ARM_DWT_CYCCNT;
  while(ARM_DWT_CYCCNT - start < cycles);
}

struct Trial{
  bool ok;
  bool irq;
  float readyUs;      //poll: time of the read that saw chip ready; fixed: unused
  float irqUs;        //nIRQ falling edge after reset #2
  uint16_t reads;
};

//RadioHead's reset sequence up to software reset #2 (RH_RF22::init and RH_RF22::reset)
uint32_t resetSequence(){
  radio.spiRead(RH_RF22_REG_03_INTERRUPT_STATUS1);
  radio.spiRead(RH_RF22_REG_04_INTERRUPT_STATUS2);
  radio.spiWrite(RH_RF22_REG_07_OPERATING_MODE1, RH_RF22_SWRES);
  delay(1);
  radio.spiRead(RH_RF22_REG_00_DEVICE_TYPE);
  noInterrupts();
  irqSeen = false;
  interrupts();
  radio.spiWrite(RH_RF22_REG_07_OPERATING_MODE1, RH_RF22_SWRES);
  return ARM_DWT_CYCCNT;
}

Trial trialPoll(uint32_t delayCycles){
  Trial t = {false, false, -1, -1, 0};
  uint32_t t0 = resetSequence();
  waitCycles(delayCycles);
  //same loop as RH_RF22::init()
  uint32_t startMillis = millis();
  while(true){
    t.reads++;
    if(radio.spiRead(RH_RF22_REG_04_INTERRUPT_STATUS2) & RH_RF22_ICHIPRDY){
      t.ok = true;
      t.readyUs = cyclesToUs(ARM_DWT_CYCCNT - t0);
      break;
    }
    if(millis() - startMillis > POLL_TIMEOUT_MS) break;
  }
  t.irq = irqSeen;
  if(t.irq) t.irqUs = cyclesToUs(irqCycles - t0);
  return t;
}

Trial trialFixed(uint32_t delayCycles){
  Trial t = {false, false, -1, -1, 1};
  uint32_t t0 = resetSequence();
  waitCycles(delayCycles);
  delay(2);
  t.ok = radio.spiRead(RH_RF22_REG_04_INTERRUPT_STATUS2) & RH_RF22_ICHIPRDY;
  t.irq = irqSeen;
  if(t.irq) t.irqUs = cyclesToUs(irqCycles - t0);
  return t;
}

void sweep(){
  uint32_t sweepStart = millis();
  for(int method = 0; method < 2; method++){
    for(uint32_t ns = 0; ns <= MAX_DELAY_NS; ns += STEP_NS){
      uint32_t cycles = nsToCycles(ns);
      int fails = 0, irqs = 0;
      float readySum = 0, irqSum = 0, readsSum = 0;
      int readyCount = 0;
      for(int i = 0; i < TRIALS; i++){
        Trial t = method == 0 ? trialPoll(cycles) : trialFixed(cycles);
        if(!t.ok) fails++;
        if(t.irq){ irqs++; irqSum += t.irqUs; }
        if(t.ok && t.readyUs >= 0){ readySum += t.readyUs; readyCount++; }
        readsSum += t.reads;
      }
      Serial.printf("sweep method=%s delay_ns=%lu trials=%d fails=%d irq=%d irq_us=%.2f ready_us=%.2f reads=%.1f\n",
        method == 0 ? "poll" : "fixed", ns, TRIALS, fails, irqs,
        irqs ? irqSum / irqs : -1.0f, readyCount ? readySum / readyCount : -1.0f, readsSum / TRIALS);
    }
  }
  Serial.printf("done ms=%lu\n", millis() - sweepStart);
}

void setup(){
  //radio on and deselected from a few ms after reset is not needed here: the sweep resets it itself
  digitalWriteFast(OLED_SELECT_PIN, HIGH);
  pinMode(OLED_SELECT_PIN, OUTPUT);
  digitalWriteFast(OLED_RESET_PIN, LOW);
  pinMode(OLED_RESET_PIN, OUTPUT);
  digitalWriteFast(RADIO_SHUTDOWN_PIN, LOW);
  pinMode(RADIO_SHUTDOWN_PIN, OUTPUT);
  Serial.begin(115200);
  //SPI setup only (begin, select pin, settings), not RH_RF22::init(): its interrupt handler would
  //compete with ours for the nIRQ pin
  radio.RHSPIDriver::init();
  pinMode(RADIO_INTERRUPT_PIN, INPUT);
  attachInterrupt(digitalPinToInterrupt(RADIO_INTERRUPT_PIN), onRadioInterrupt, FALLING);
  Serial.printf("race_sweep ready, device type 0x%02X, F_CPU %lu\n", radio.spiRead(RH_RF22_REG_00_DEVICE_TYPE), F_CPU_ACTUAL);
}

void loop(){
  if(Serial.available() && Serial.read() == 's') sweep();
}
