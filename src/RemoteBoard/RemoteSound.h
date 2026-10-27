#pragma once
#include <Arduino.h>
#include <IntervalTimer.h>
#include <Sound.h>

//The board's end of the sound engine (lib/RemoteAudio, SoundOutput) [D-158]: the piezo disc between two pins (A, B;
//220 ohm in series), two ways:
//- square waves: a timer interrupt toggles both legs in opposite phase (the disc sees +3.3 V and -3.3 V, twice the
//  swing of one leg on ground), or each leg at its own pitch (two notes at once, the disc plays their difference)
//- the Synth as a PWM DAC: both legs carry a 62.5 kHz carrier whose duties a 31.25 kHz timer interrupt sets, mirrored
//  around the rest level; the disc follows the average of their difference
//Only one at a time: each stops the other. The carriers and the interrupts run only while sounding; otherwise both
//pins rest low. One instance (the interrupts have no argument). Each edge is a current spike into the disc (a
//capacitor) through the resistor, up to 6.6 V / 220 ohm: the carrier stays moderate so the average stays a few mA.
//Main thread only (the interrupts are internal).
class RemoteSound : public SoundOutput{
public:

    static constexpr float carrierHertz = 62500.0f;

    RemoteSound(uint8_t pinA, uint8_t pinB) : pinA(pinA), pinB(pinB){ instance = this; }

    void tone(uint16_t hertz) override {
        stopAll();
        if(hertz == 0) return;
        b_levelA = true;
        b_levelB = false;
        digitalWrite(pinA, HIGH);
        digitalWrite(pinB, LOW);
        b_squareA = timerA.begin(toggleBoth, halfPeriodMicros(hertz));
    }

    //a square tone for this long, then quiet (update() ends it)
    void tone(uint16_t hertz, uint16_t millis){
        tone(hertz);
        toneEndMillis = ::millis() + millis;
        b_toneTimed = true;
    }

    void duet(uint16_t hertzA, uint16_t hertzB) override {
        stopAll();
        b_levelA = false;
        b_levelB = false;
        if(hertzA) b_squareA = timerA.begin(toggleA, halfPeriodMicros(hertzA));
        if(hertzB) b_squareB = timerB.begin(toggleB, halfPeriodMicros(hertzB));
    }

    void quietSquares() override { stopSquares(); }

    void release(uint8_t voice) override {
        noInterrupts();
        synth.stop(voice);
        interrupts();
    }

    void play(uint8_t voice, Synth::Wave wave, float hertz, float level, float seconds, float decaySeconds) override {
        stopSquares();
        noInterrupts();
        synth.play(voice, wave, hertz, level, seconds, decaySeconds);
        interrupts();
        startSynth();
    }

    void playClip(uint8_t voice, const SoundClip& clip, float level) override {
        stopSquares();
        noInterrupts();
        synth.playClip(voice, clip.samples, clip.count, clip.rate, level);
        interrupts();
        startSynth();
    }

    void playScore(const SynthScoreNote* notes, uint8_t count) override {
        stopSquares();
        noInterrupts();
        for(uint8_t i = 0; i < count && i < Synth::voiceCount; i++){
            const SynthScoreNote& n = notes[i];
            synth.play(i, n.wave, n.hertz, n.level, n.seconds, n.decaySeconds, n.attackSeconds, n.startSeconds);
        }
        interrupts();
        startSynth();
    }

    //every loop: ends a timed tone; once the synth is done, stops its interrupt and the carriers
    void update(){
        if(b_toneTimed && int32_t(::millis() - toneEndMillis) >= 0) stopSquares();
        if(!b_synth) return;
        noInterrupts();
        bool b_idle = synth.isIdle();
        interrupts();
        if(b_idle) stopSynth();
    }

private:

    void startSynth(){
        if(b_synth) return;
        analogWriteFrequency(pinA, carrierHertz);
        analogWriteFrequency(pinB, carrierHertz);
        analogWrite(pinA, 0);
        analogWrite(pinB, 0);
        b_synth = timerSynth.begin(sample, 1000000.0f / float(Synth::sampleRate));
    }

    static float halfPeriodMicros(uint16_t hertz){ return 500000.0f / float(hertz); }

    void restLow(){
        pinMode(pinA, OUTPUT);
        pinMode(pinB, OUTPUT);
        digitalWrite(pinA, LOW);
        digitalWrite(pinB, LOW);
    }

    void stopSquares(){
        b_toneTimed = false;
        if(!b_squareA && !b_squareB) return;
        timerA.end();
        timerB.end();
        b_squareA = b_squareB = false;
        restLow();
    }

    void stopSynth(){
        if(!b_synth) return;
        timerSynth.end();
        noInterrupts();
        synth.stopAll();
        interrupts();
        b_synth = false;
        restLow();
    }

    void stopAll(){
        stopSquares();
        stopSynth();
        restLow();
    }

    static void toggleBoth(){
        RemoteSound& s = *instance;
        s.b_levelA = !s.b_levelA;
        digitalWrite(s.pinA, s.b_levelA);
        digitalWrite(s.pinB, !s.b_levelA);
    }
    static void toggleA(){
        RemoteSound& s = *instance;
        s.b_levelA = !s.b_levelA;
        digitalWrite(s.pinA, s.b_levelA);
    }
    static void toggleB(){
        RemoteSound& s = *instance;
        s.b_levelB = !s.b_levelB;
        digitalWrite(s.pinB, s.b_levelB);
    }
    static void sample(){
        RemoteSound& s = *instance;
        Synth::Pair duty = s.synth.nextPair();
        analogWrite(s.pinA, duty.a);
        analogWrite(s.pinB, duty.b);
    }

    static RemoteSound* instance;
    uint8_t pinA;
    uint8_t pinB;
    Synth synth;
    IntervalTimer timerA;
    IntervalTimer timerB;
    IntervalTimer timerSynth;
    volatile bool b_levelA = false;
    volatile bool b_levelB = false;
    bool b_squareA = false;
    bool b_squareB = false;
    bool b_synth = false;
    bool b_toneTimed = false;
    uint32_t toneEndMillis = 0;
};
