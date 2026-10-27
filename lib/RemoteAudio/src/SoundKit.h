#pragma once
#include <stdint.h>
#include <math.h>
#include "Sound.h"
#if defined(TEENSYDUINO)
#include <avr/pgmspace.h>
#endif
#ifndef PROGMEM
#define PROGMEM
#endif

//The sound kit [D-158]: small helpers every game uses, so the games sound like one set. Pitches by MIDI note,
//synth drums for the back layer's voice 2, sweeps, noise bursts, grains for continuous sounds, a list that collects
//notes, and a tracker that writes a tune as data (steps) instead of hand-computed milliseconds.
//
//How a game makes sound (rules: docs/arcade/README.md section 3)
//- Through Game::sound() only (silent in the picker's demo), priority SoundPriority::GAME (the default here).
//- Music on the back layer (looping, started in playMusic(), its speed by setTempo), effects on the front layer.
//- A timer square (Kind::SQUARE, DUET) silences every synth voice, the music too: effects are synth notes; squares
//  only for a crash, a death, a game over. Continuous sounds are grains (one short note per period, shorter than it).
//- Never the game's GameRandom: a SoundRandom of its own, so a game and its demo stay deterministic.
//
//    using namespace SoundKit;
//    NoteList<160> tune;                                     //a member of the game (the player keeps a pointer)
//    static const uint8_t melody[] PROGMEM = {E5, HOLD, B4, C5, D5, HOLD, C5, B4};
//    //build once (playMusic() runs on every start, resume and music back on; the list only grows)
//    if(tune.size() == 0){
//        Tracker t(tune, 125);                               //125 ms a step
//        t.instrument(0, Synth::Wave::PULSE25, 85).instrument(1, Synth::Wave::TRIANGLE, 60);
//        t.line(0, melody, sizeof(melody), 0, 1, 80).drums("K.H.S.H.", 0, 1);
//        music = t.cue(8);                                   //loops after 8 steps
//    }
//    void MyGame::playMusic(){ sound().play(music, SoundLayer::BACK); sound().setTempo(SoundLayer::BACK, 100 + 10 * level); }
//- Tempo: games set the back layer's only; the Arcade puts both back to 100 when a game ends.
//    NoteList<16> zap; zap.sweep(0, 2000, 400, 90, 6);       //in update(): sound().play(zap.cue());
//    if(motor.due(2)) motor.play(sound(), synthNote(0, hertz(A3) + speed * 4, 25, 60));
namespace SoundKit{

    //MIDI note numbers (A4 = 69 = 440 Hz); s for sharp. REST and HOLD are for Tracker::line
    enum : uint8_t{
        REST = 0,
        KICK = 1, SNARE = 2, HAT = 3,       //drums, as notes of a tracker step (voice 2 of the back layer)
        C2 = 36, Cs2 = 37, D2 = 38, Ds2 = 39, E2 = 40, F2 = 41, Fs2 = 42, G2 = 43, Gs2 = 44, A2 = 45, As2 = 46, B2 = 47,
        C3 = 48, Cs3 = 49, D3 = 50, Ds3 = 51, E3 = 52, F3 = 53, Fs3 = 54, G3 = 55, Gs3 = 56, A3 = 57, As3 = 58, B3 = 59,
        C4 = 60, Cs4 = 61, D4 = 62, Ds4 = 63, E4 = 64, F4 = 65, Fs4 = 66, G4 = 67, Gs4 = 68, A4 = 69, As4 = 70, B4 = 71,
        C5 = 72, Cs5 = 73, D5 = 74, Ds5 = 75, E5 = 76, F5 = 77, Fs5 = 78, G5 = 79, Gs5 = 80, A5 = 81, As5 = 82, B5 = 83,
        C6 = 84, Cs6 = 85, D6 = 86, Ds6 = 87, E6 = 88, F6 = 89, Fs6 = 90, G6 = 91, Gs6 = 92, A6 = 93, As6 = 94, B6 = 95,
        C7 = 96, Cs7 = 97, D7 = 98, Ds7 = 99, E7 = 100, F7 = 101, Fs7 = 102, G7 = 103, Gs7 = 104, A7 = 105, As7 = 106, B7 = 107,
        C8 = 108,
        HOLD = 255,
    };

    //a MIDI note's pitch in hertz, rounded
    inline uint16_t hertz(uint8_t midiNote){
        return uint16_t(440.0f * powf(2.0f, (float(midiNote) - 69.0f) / 12.0f) + 0.5f);
    }

    //the C major pentatonic from C (step 0) upward: C D E G A, then the next octave (the remote's interface scale)
    inline uint8_t pentatonic(uint8_t baseC, uint8_t step){
        static const uint8_t offsets[5] = {0, 2, 4, 7, 9};
        return uint8_t(baseC + 12 * (step / 5) + offsets[step % 5]);
    }

    //a synth note (front voice 0 unless given)
    inline SoundNote synthNote(uint16_t atMillis, uint16_t hertz, uint16_t millis, uint8_t levelPercent,
                               uint16_t decayMillis = 0, Synth::Wave wave = Synth::Wave::SQUARE, uint8_t voice = 0){
        return {atMillis, SoundNote::Kind::SYNTH, voice, wave, hertz, millis, levelPercent, decayMillis, 0};
    }

    //a timer square on both legs: the loudest, but it silences the synth (the music) while it sounds
    inline SoundNote squareNote(uint16_t atMillis, uint16_t hertz, uint16_t millis){
        return {atMillis, SoundNote::Kind::SQUARE, 0, Synth::Wave::SQUARE, hertz, millis, 0, 0, 0};
    }

    //a burst of noise (crash, explosion, shot): brightnessHertz is the NOISE pitch (how often it changes: 1000 dull,
    //6000 bright; keep it under about 8000, the disc is deaf above 5 kHz), decaying fast
    inline SoundNote noiseBurst(uint16_t atMillis, uint16_t brightnessHertz, uint16_t millis, uint8_t levelPercent = 90,
                                uint16_t decayMillis = 40, uint8_t voice = 0){
        return synthNote(atMillis, brightnessHertz, millis, levelPercent, decayMillis, Synth::Wave::NOISE, voice);
    }

    //the drums, synth notes on the back layer's voice 2, written for the disc (it plays from about 1 kHz to 5 kHz and
    //is nearly silent below 400 Hz: a real low kick would not be heard)
    //kick: a short thump of noise, then a square sweeping fast from 500 Hz to 150 Hz, six notes in 45 ms
    static const uint8_t kickNoteCount = 6;
    inline uint8_t kick(SoundNote* out, uint16_t atMillis, uint8_t levelPercent = 100){
        static const uint16_t pitches[kickNoteCount - 1] = {500, 360, 260, 190, 150};
        out[0] = noiseBurst(atMillis, 5000, 10, levelPercent, 0, 2);
        for(uint8_t i = 0; i < kickNoteCount - 1; i++){
            bool b_last = i == kickNoteCount - 2;
            out[i + 1] = synthNote(uint16_t(atMillis + 9 + i * 7), pitches[i], b_last ? 14 : 8, levelPercent,
                                   b_last ? 8 : 0, Synth::Wave::SQUARE, 2);
        }
        return kickNoteCount;
    }
    //snare: 90 ms of bright noise, fast decay
    inline SoundNote snare(uint16_t atMillis, uint8_t levelPercent = 90){
        return noiseBurst(atMillis, 7000, 90, levelPercent, 45, 2);
    }
    //hat: 18 ms of the brightest noise, quieter
    inline SoundNote hat(uint16_t atMillis, uint8_t levelPercent = 65){
        return noiseBurst(atMillis, 10000, 20, levelPercent, 10, 2);
    }

    //a sweep: a note gliding from one pitch to another as `steps` short notes on one voice over `millis` (even in
    //pitch, so a fall sounds as smooth as a rise): zaps, whistles, risers. Writes steps notes, returns the count
    static const uint8_t maxSweepSteps = 32;
    inline uint8_t sweep(SoundNote* out, uint16_t atMillis, uint16_t fromHertz, uint16_t toHertz, uint16_t millis,
                         uint8_t steps, Synth::Wave wave = Synth::Wave::SQUARE, uint8_t levelPercent = 80,
                         uint8_t voice = 0, uint16_t lastDecayMillis = 0){
        if(steps < 1) steps = 1;
        if(steps > maxSweepSteps) steps = maxSweepSteps;
        //a pitch from game state may reach 0 (a speed falling to rest): never a division by zero, never 0 Hz
        if(fromHertz < 20) fromHertz = 20;
        if(toHertz < 20) toHertz = 20;
        uint16_t stepMillis = uint16_t(millis / steps);
        if(stepMillis < 1) stepMillis = 1;
        float ratio = steps > 1 ? powf(float(toHertz) / float(fromHertz), 1.0f / float(steps - 1)) : 1.0f;
        float pitch = fromHertz;
        for(uint8_t i = 0; i < steps; i++){
            bool b_last = i == steps - 1;
            //each step a little longer than its slot, so the next one takes over with no gap
            out[i] = synthNote(uint16_t(atMillis + i * stepMillis), uint16_t(pitch + 0.5f), uint16_t(stepMillis + (b_last ? 0 : 2)),
                               levelPercent, b_last ? lastDecayMillis : 0, wave, voice);
            pitch *= ratio;
        }
        return steps;
    }

    //sound's own random numbers (xorshift32), never the game's GameRandom
    class SoundRandom{
    public:
        explicit SoundRandom(uint32_t seed = 2463534242u) : state(seed ? seed : 2463534242u){}
        uint32_t next(){
            state ^= state << 13;
            state ^= state >> 17;
            state ^= state << 5;
            return state;
        }
        //0..1
        float unit(){ return float(next() >> 8) / float(1u << 24); }
        //lowest..highest, both included
        int32_t between(int32_t lowest, int32_t highest){
            return lowest + int32_t(next() % uint32_t(highest - lowest + 1));
        }
    private:
        uint32_t state;
    };

    //a continuous sound (rotor, motor whine, hiss): one short note every period, made at run time from the game's
    //state, on the front layer. Call due() once per update() (a tick, 20 ms); a grain must be shorter than its
    //period so the front layer is free again for the next one. play() skips the grain while something else plays
    //in front (the game's own one-shot effect), unless b_yield is false
    class Grain{
    public:
        //true every periodTicks ticks (1: every tick, 50 a second)
        bool due(uint8_t periodTicks){
            if(++ticks < periodTicks) return false;
            ticks = 0;
            return true;
        }
        bool play(SoundSink& sink, const SoundNote& note, bool b_yield = true){
            if(b_yield && sink.isPlaying(SoundLayer::FRONT)) return false;
            return sink.playNote(note, SoundPriority::GAME, SoundLayer::FRONT);
        }
        //for a random pitch or level per grain
        SoundRandom random;
    private:
        uint8_t ticks = 0;
    };

    //collects notes into a buffer it does not own (NoteList holds one); sorted into time order by cue()
    class NoteWriter{
    public:
        NoteWriter(SoundNote* buffer, uint16_t capacity) : buffer(buffer), capacity(capacity){}
        NoteWriter(const NoteWriter&) = delete;
        NoteWriter& operator=(const NoteWriter&) = delete;

        void clear(){ count = 0; }
        uint16_t size() const { return count; }
        bool isFull() const { return count >= capacity; }
        const SoundNote& operator[](uint16_t i) const { return buffer[i]; }

        //a note (dropped when full)
        NoteWriter& add(const SoundNote& note){
            if(count < capacity) buffer[count++] = note;
            return *this;
        }
        NoteWriter& synth(uint16_t atMillis, uint16_t hertz, uint16_t millis, uint8_t levelPercent,
                          uint16_t decayMillis = 0, Synth::Wave wave = Synth::Wave::SQUARE, uint8_t voice = 0){
            return add(synthNote(atMillis, hertz, millis, levelPercent, decayMillis, wave, voice));
        }
        NoteWriter& square(uint16_t atMillis, uint16_t hertz, uint16_t millis){ return add(squareNote(atMillis, hertz, millis)); }
        NoteWriter& burst(uint16_t atMillis, uint16_t brightnessHertz, uint16_t millis, uint8_t levelPercent = 90,
                          uint16_t decayMillis = 40, uint8_t voice = 0){
            return add(noiseBurst(atMillis, brightnessHertz, millis, levelPercent, decayMillis, voice));
        }
        NoteWriter& kick(uint16_t atMillis, uint8_t levelPercent = 100){
            SoundNote notes[kickNoteCount];
            uint8_t n = SoundKit::kick(notes, atMillis, levelPercent);
            for(uint8_t i = 0; i < n; i++) add(notes[i]);
            return *this;
        }
        NoteWriter& snare(uint16_t atMillis, uint8_t levelPercent = 90){ return add(SoundKit::snare(atMillis, levelPercent)); }
        NoteWriter& hat(uint16_t atMillis, uint8_t levelPercent = 65){ return add(SoundKit::hat(atMillis, levelPercent)); }
        NoteWriter& sweep(uint16_t atMillis, uint16_t fromHertz, uint16_t toHertz, uint16_t millis, uint8_t steps,
                          Synth::Wave wave = Synth::Wave::SQUARE, uint8_t levelPercent = 80, uint8_t voice = 0,
                          uint16_t lastDecayMillis = 0){
            SoundNote notes[maxSweepSteps];
            uint8_t n = SoundKit::sweep(notes, atMillis, fromHertz, toHertz, millis, steps, wave, levelPercent, voice, lastDecayMillis);
            for(uint8_t i = 0; i < n; i++) add(notes[i]);
            return *this;
        }

        //time order (stable: notes at the same time keep the order they were added in)
        void sort(){
            for(uint16_t i = 1; i < count; i++){
                SoundNote n = buffer[i];
                uint16_t j = i;
                while(j > 0 && buffer[j - 1].atMillis > n.atMillis){
                    buffer[j] = buffer[j - 1];
                    j--;
                }
                buffer[j] = n;
            }
        }

        //sorts, then the cue over the notes (they stay in this buffer: do not change it while it plays)
        SoundCue cue(uint8_t priority = SoundPriority::GAME, uint16_t loopMillis = 0){
            sort();
            SoundCue c;
            c.notes = buffer;
            c.noteCount = count;
            c.priority = priority;
            c.loopMillis = loopMillis;
            return c;
        }

    private:
        SoundNote* buffer;
        uint16_t capacity;
        uint16_t count = 0;
    };

    //a NoteWriter with its own room for N notes
    template<uint16_t N>
    class NoteList : public NoteWriter{
    public:
        NoteList() : NoteWriter(storage, N){}
    private:
        SoundNote storage[N];
    };

    //a voice's sound in a tune: its waveform, how much of each note's length it sounds (gate), its decay
    struct Instrument{
        Synth::Wave wave = Synth::Wave::SQUARE;
        uint8_t gatePercent = 90;
        uint16_t decayMillis = 0;
    };

    //a note of a tune: from step `step`, on voice 0..2 (back layer: melody, bass, drums), MIDI note (or KICK, SNARE,
    //HAT), `length` steps long, at levelPercent
    struct Step{
        uint16_t step;
        uint8_t voice;
        uint8_t note;
        uint8_t length;
        uint8_t level;
    };

    //writes a tune into a NoteWriter from data: steps of stepMillis each. Build it once (the first playMusic()),
    //then play its cue; the tempo of the layer (setTempo) speeds it up without building it again
    class Tracker{
    public:
        Tracker(NoteWriter& out, uint16_t stepMillis) : out(out), stepLength(stepMillis){
            instruments[2].wave = Synth::Wave::NOISE;
        }

        Tracker& instrument(uint8_t voice, Synth::Wave wave, uint8_t gatePercent = 90, uint16_t decayMillis = 0){
            if(voice < 3) instruments[voice] = {wave, gatePercent, decayMillis};
            return *this;
        }

        uint16_t stepMillis() const { return stepLength; }
        uint16_t millisAt(uint16_t step) const { return uint16_t(uint32_t(step) * stepLength); }

        //one note; KICK, SNARE and HAT are the kit's drums (on voice 2 whatever voice is given), REST nothing
        Tracker& note(uint16_t step, uint8_t voice, uint8_t midiNote, uint8_t length = 1, uint8_t levelPercent = 80){
            uint16_t at = millisAt(step);
            switch(midiNote){
                case REST: case HOLD: return *this;
                case KICK: out.kick(at, levelPercent); return *this;
                case SNARE: out.snare(at, levelPercent); return *this;
                case HAT: out.hat(at, levelPercent); return *this;
                default: break;
            }
            const Instrument& in = instruments[voice < 3 ? voice : 2];
            uint32_t millis = uint32_t(length) * stepLength * in.gatePercent / 100;
            if(millis < 1) millis = 1;
            out.synth(at, hertz(midiNote), uint16_t(millis), levelPercent, in.decayMillis, in.wave, voice);
            return *this;
        }

        //a list of steps (static const ... PROGMEM), each moved by atStep: a pattern placed again further on
        Tracker& steps(const Step* list, uint16_t count, uint16_t atStep = 0){
            for(uint16_t i = 0; i < count; i++){
                const Step& s = list[i];
                note(uint16_t(s.step + atStep), s.voice, s.note, s.length, s.level);
            }
            return *this;
        }

        //a line on one voice, one entry every stepsPerNote steps from atStep: a MIDI note, REST, or HOLD (the note
        //before lasts one entry longer). The compact way to write a melody or a bass
        Tracker& line(uint8_t voice, const uint8_t* notes, uint16_t count, uint16_t atStep, uint8_t stepsPerNote = 1,
                      uint8_t levelPercent = 80){
            for(uint16_t i = 0; i < count; i++){
                uint8_t n = notes[i];
                if(n == HOLD || n == REST) continue;
                uint16_t entries = 1;
                while(i + entries < count && notes[i + entries] == HOLD) entries++;
                note(uint16_t(atStep + i * stepsPerNote), voice, n, uint8_t(entries * stepsPerNote), levelPercent);
            }
            return *this;
        }

        //drums from a pattern, one character a step: K kick, S snare, H hat, anything else nothing; the pattern
        //placed `repeats` times in a row from atStep
        Tracker& drums(const char* pattern, uint16_t atStep, uint8_t repeats = 1, uint8_t levelPercent = 100){
            uint16_t length = 0;
            while(pattern[length]) length++;
            for(uint8_t r = 0; r < repeats; r++){
                for(uint16_t i = 0; i < length; i++){
                    uint16_t step = uint16_t(atStep + r * length + i);
                    switch(pattern[i]){
                        case 'K': note(step, 2, KICK, 1, levelPercent); break;
                        case 'S': note(step, 2, SNARE, 1, uint8_t(levelPercent * 9 / 10)); break;
                        case 'H': note(step, 2, HAT, 1, uint8_t(levelPercent * 70 / 100)); break;
                        default: break;
                    }
                }
            }
            return *this;
        }

        //the tune as a cue: looping after loopSteps steps (0: plays once), sorted
        SoundCue cue(uint16_t loopSteps, uint8_t priority = SoundPriority::GAME){
            return out.cue(priority, uint16_t(uint32_t(loopSteps) * stepLength));
        }

    private:
        NoteWriter& out;
        uint16_t stepLength;
        Instrument instruments[3];
    };
};
