#pragma once
#include <stdint.h>
#include <math.h>

//A small synth for the piezo (sound test): up to four voices mixed into 8-bit PWM duty samples, one per call of
//nextPair() at sampleRate (the firmware calls it from a timer interrupt and writes it to two PWM pins with an
//ultrasonic carrier, one per leg of the disc; the disc follows the average of their difference). The two duties are
//mirrored around the rest level, so the disc sees twice the swing of one pin. Square and pulse edges are smoothed
//(PolyBLEP): an edge falls between samples, and unsmoothed it would jump to the next one, a rough, distorted sound. Each note has a 3 ms attack, holds or decays exponentially, and
//releases in 5 ms. The output rests at 0 and ramps to mid-scale (the zero of the waveforms) in 20 ms before sound
//and back 0.3 s after it, so a single-ended disc does not thump (with both legs driven the ramp is the same on both,
//nothing across the disc).
class Synth{
public:

    //SAMPLE: a recorded clip (playClip), not a waveform
    enum class Wave : uint8_t{ SQUARE, PULSE25, PULSE12, SINE, TRIANGLE, NOISE, SAMPLE };

    static constexpr uint32_t sampleRate = 31250;
    //eight voices: the sound engine's layers use 0 to 4 (front 2 and 3, back 0, 1 and 4), a score (the startup sound) all eight
    static const uint8_t voiceCount = 8;
    static constexpr float defaultAttackSeconds = 0.003f;

    Synth(){
        for(int i = 0; i < 256; i++) sineTable[i] = sinf(float(i) * 6.2831853f / 256.0f);
    }

    //main thread (with the timer interrupt held off): a note on a voice, replacing what it played. level 0..1 (the
    //voices add up, the sum is clipped at 1); decaySeconds 0 holds the level for the length of the note, otherwise
    //it falls exponentially from the end of the attack (time constant). attackSeconds: a linear rise (a swell when
    //long); delaySeconds: the note waits this long first, so a whole piece can be handed over at once
    void play(uint8_t voice, Wave wave, float hertz, float level, float seconds, float decaySeconds = 0.0f,
              float attackSeconds = defaultAttackSeconds, float delaySeconds = 0.0f){
        if(voice >= voiceCount) return;
        Voice& v = voices[voice];
        v.wave = wave;
        v.increment = uint32_t(hertz * 4294967296.0f / float(sampleRate));
        v.level = level;
        v.decay = decaySeconds > 0.0f ? expf(-1.0f / (decaySeconds * float(sampleRate))) : 1.0f;
        v.samplesLeft = uint32_t(seconds * float(sampleRate));
        float attack = attackSeconds > defaultAttackSeconds ? attackSeconds : defaultAttackSeconds;
        v.attackStep = 1.0f / (attack * float(sampleRate));
        v.delaySamples = uint32_t(delaySeconds * float(sampleRate));
        v.b_active = true;
    }
    //main thread (with the timer interrupt held off): a recorded clip on a voice, 8-bit signed samples at their own
    //rate (read with linear steps, from flash on the Teensy), at a level; it ends with its last sample
    void playClip(uint8_t voice, const int8_t* samples, uint32_t count, uint32_t rate, float level){
        if(voice >= voiceCount || count == 0) return;
        Voice& v = voices[voice];
        v.wave = Wave::SAMPLE;
        v.clip = samples;
        v.clipCount = count;
        v.phase = 0;
        v.increment = uint32_t(float(rate) * 4096.0f / float(sampleRate));
        v.level = level;
        v.decay = 1.0f;
        v.samplesLeft = UINT32_MAX;
        v.attackStep = 1.0f / (defaultAttackSeconds * float(sampleRate));
        v.delaySamples = 0;
        v.b_active = true;
    }

    void stop(uint8_t voice){ if(voice < voiceCount) voices[voice].samplesLeft = 0; }
    void stopAll(){ for(uint8_t i = 0; i < voiceCount; i++) stop(i); }

    //no note sounding and the output back at rest: the firmware may stop the interrupt and the carrier
    bool isIdle() const {
        for(uint8_t i = 0; i < voiceCount; i++) if(voices[i].b_active) return false;
        return bias <= 0.0f;
    }

    struct Pair{ uint8_t a; uint8_t b; };

    //timer interrupt: one leg of the disc only (the other on ground)
    uint8_t nextSample(){ return nextPair().a; }

    //timer interrupt: the next duties of both legs, 0..255
    Pair nextPair(){
        bool b_pending = false;
        for(uint8_t i = 0; i < voiceCount; i++) if(voices[i].b_active) b_pending = true;
        //rest level: up to mid-scale before the first note (the notes wait), held 0.3 s after the last one so the
        //gaps of a phrase stay there, then down
        if(b_pending){
            holdSamples = holdLength;
            if(bias < 1.0f){
                bias += biasStep;
                if(bias > 1.0f) bias = 1.0f;
                uint8_t rest = uint8_t(bias * 127.5f + 0.5f);
                return {rest, rest};
            }
        }
        else if(holdSamples > 0) holdSamples--;
        else if(bias > 0.0f){
            bias -= biasStep;
            if(bias < 0.0f) bias = 0.0f;
        }

        float mix = 0.0f;
        for(uint8_t i = 0; i < voiceCount; i++){
            Voice& v = voices[i];
            if(!v.b_active) continue;
            if(v.delaySamples > 0){
                v.delaySamples--;
                continue;
            }
            //envelope: attack while the note lasts, then release; the level decays once the attack is done
            if(v.samplesLeft > 0){
                v.samplesLeft--;
                v.envelope += v.attackStep;
                if(v.envelope > 1.0f) v.envelope = 1.0f;
            }
            else{
                v.envelope -= releaseStep;
                if(v.envelope <= 0.0f){
                    v.envelope = 0.0f;
                    v.b_active = false;
                    continue;
                }
            }
            if(v.envelope >= 1.0f || v.samplesLeft == 0) v.level *= v.decay;
            uint32_t previous = v.phase;
            v.phase += v.increment;
            mix += wave(v, previous) * v.level * v.envelope;
        }
        if(mix > 1.0f) mix = 1.0f;
        if(mix < -1.0f) mix = -1.0f;
        float rest = bias * 127.5f;
        return {duty(rest + mix * 127.0f), duty(rest - mix * 127.0f)};
    }

private:

    struct Voice{
        Wave wave = Wave::SINE;
        uint32_t phase = 0;
        uint32_t increment = 0;
        float level = 0.0f;
        float decay = 1.0f;
        float envelope = 0.0f;
        uint32_t samplesLeft = 0;
        float noise = 0.0f;
        bool b_active = false;
        float attackStep = 0.0f;
        uint32_t delaySamples = 0;
        const int8_t* clip = nullptr;   //SAMPLE: the clip, its length; phase counts samples in 20.12 (1 M samples)
        uint32_t clipCount = 0;
    };

    static uint8_t duty(float value){
        if(value < 0.0f) value = 0.0f;
        if(value > 255.0f) value = 255.0f;
        return uint8_t(value + 0.5f);
    }

    //the correction near an edge at phase 0 (t and dt in periods): spreads the step over the samples around it
    static float polyBlep(float t, float dt){
        if(t < dt){
            float x = t / dt;
            return x + x - x * x - 1.0f;
        }
        if(t > 1.0f - dt){
            float x = (t - 1.0f) / dt;
            return x * x + x + x + 1.0f;
        }
        return 0.0f;
    }

    //high for width of the period, the rising edge at 0 and the falling one at width, both smoothed
    static float pulse(uint32_t phase, uint32_t increment, float width){
        float t = float(phase >> 8) / 16777216.0f;
        float dt = float(increment >> 8) / 16777216.0f;
        float value = t < width ? 1.0f : -1.0f;
        float fall = t - width;
        if(fall < 0.0f) fall += 1.0f;
        return value + polyBlep(t, dt) - polyBlep(fall, dt);
    }

    float wave(Voice& v, uint32_t previousPhase){
        uint32_t p = v.phase;
        switch(v.wave){
            case Wave::SQUARE: return pulse(p, v.increment, 0.5f);
            case Wave::PULSE25: return pulse(p, v.increment, 0.25f);
            case Wave::PULSE12: return pulse(p, v.increment, 0.125f);
            case Wave::SINE: return sineTable[p >> 24];
            case Wave::TRIANGLE:{
                float t = float(p >> 8) / 16777216.0f;     //0..1
                return t < 0.5f ? 4.0f * t - 1.0f : 3.0f - 4.0f * t;
            }
            case Wave::SAMPLE:{
                uint32_t index = previousPhase >> 12;
                if(index + 1 >= v.clipCount){
                    v.samplesLeft = 0;      //the clip is over: release
                    return 0.0f;
                }
                float t = float(previousPhase & 0xFFF) / 4096.0f;
                return (float(v.clip[index]) * (1.0f - t) + float(v.clip[index + 1]) * t) / 127.0f;
            }
            case Wave::NOISE:
                //a new random value each period: the pitch sets how bright the noise is
                if(p < previousPhase){
                    lfsr ^= lfsr << 13;
                    lfsr ^= lfsr >> 17;
                    lfsr ^= lfsr << 5;
                    v.noise = (lfsr & 1u) ? 1.0f : -1.0f;
                }
                return v.noise;
        }
        return 0.0f;
    }

    static constexpr float releaseStep = 1.0f / (0.005f * float(sampleRate));
    static constexpr float biasStep = 1.0f / (0.020f * float(sampleRate));

    float sineTable[256];
    Voice voices[voiceCount];
    static constexpr uint32_t holdLength = uint32_t(0.3f * float(sampleRate));
    float bias = 0.0f;
    uint32_t holdSamples = 0;
    uint32_t lfsr = 2463534242u;
};
