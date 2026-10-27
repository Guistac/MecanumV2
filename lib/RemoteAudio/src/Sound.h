#pragma once
#include <stdint.h>
#include "Synth.h"

//The remote's sound engine, shared by every part that makes sound (the interface sounds, the games, ...) [D-158].
//Nothing here knows the board: a part describes its sounds as cues (lists of notes) and hands them to a SoundSink;
//the SoundPlayer turns them into calls on a SoundOutput, which the firmware implements on the piezo.
//
//  part (menu, status, a game) --cue--> SoundSink (SoundPlayer) --notes--> SoundOutput (the board's buzzer)

//one note of a cue
struct SoundNote{
    enum class Kind : uint8_t{
        SQUARE,     //a timer square wave on both legs of the disc: the loudest, one at a time, silences the synth
        SYNTH,      //a synth voice: any waveform, a level and a decay, up to two at once per layer
        DUET,       //a square on each leg at its own pitch (hertz, hertzB): two notes at once, or a rough beating
                    //buzz when they are close; as loud as SQUARE, silences the synth
    };
    uint16_t atMillis;          //from the start of the cue
    Kind kind;
    uint8_t voice;              //synth: 0 or 1 within the layer
    Synth::Wave wave;           //synth
    uint16_t hertz;
    uint16_t millis;
    uint8_t levelPercent;       //synth
    uint16_t decayMillis;       //synth, 0 holds the level
    uint16_t hertzB;            //duet: the other leg's pitch (last, so lists without duets can leave it out)
};

//a sound: its notes in time order, how much it matters, whether it repeats
struct SoundCue{
    const SoundNote* notes = nullptr;
    uint8_t noteCount = 0;
    uint8_t priority = 0;       //a cue cuts off a playing one of the same or a lower priority, never a higher one
    uint16_t loopMillis = 0;    //0 plays once; otherwise starts again after this long (music)

    //how long it sounds: the end of its last note
    uint32_t lengthMillis() const {
        uint32_t end = 0;
        for(uint8_t i = 0; i < noteCount; i++){
            uint32_t noteEnd = uint32_t(notes[i].atMillis) + notes[i].millis;
            if(noteEnd > end) end = noteEnd;
        }
        return end;
    }
};

//a cue from a static note list: SoundCue c = soundCue(myNotes, Priority::GAME);
template<uint8_t N>
SoundCue soundCue(const SoundNote (&notes)[N], uint8_t priority, uint16_t loopMillis = 0){
    SoundCue cue;
    cue.notes = notes;
    cue.noteCount = N;
    cue.priority = priority;
    cue.loopMillis = loopMillis;
    return cue;
}

namespace SoundPriority{
    static const uint8_t GAME = 1;
    static const uint8_t INTERFACE = 2;
    static const uint8_t ALARM = 3;
};

//two layers play at once: effects in front (interface sounds, alarms, a game's effects), music behind (a game's
//tune, looping). Each has its own two synth voices; a square note in either silences the synth while it sounds
enum class SoundLayer : uint8_t{ FRONT, BACK, COUNT };

//what a part plays its sounds into (games get one of these; they never see the player or the board)
class SoundSink{
public:
    //true when the cue plays; false when a more important one is playing on that layer
    virtual bool play(const SoundCue& cue, SoundLayer layer = SoundLayer::FRONT) = 0;
    //a single note made up at run time (a pitch from the game state): kept by the sink, no list needed
    virtual bool playNote(const SoundNote& note, uint8_t priority, SoundLayer layer = SoundLayer::FRONT) = 0;
    virtual void stop(SoundLayer layer) = 0;
};

//a recorded sound: 8-bit signed samples (in flash on the Teensy: PROGMEM), made from an audio file by
//tools/sounds/clip.py, which also fits it to the disc (high-pass, presence boost, compression)
struct SoundClip{
    const int8_t* samples;
    uint32_t count;
    uint32_t rate;
};

//a note of a piece the synth plays on its own: all notes are handed over at once, each on its own voice (up to
//Synth::voiceCount), and each waits for its start. For a sound that must go on while the firmware is busy, the
//startup sound (it plays on after the animation)
struct SynthScoreNote{
    float startSeconds;
    Synth::Wave wave;
    float hertz;
    float level;
    float attackSeconds;        //to the peak
    float seconds;              //from the start to the release (attack included)
    float decaySeconds;         //time constant after the peak
};

//the board's buzzer: square waves and the synth share the pins, so each stops the other
class SoundOutput{
public:
    virtual void tone(uint16_t hertz) = 0;      //a square wave on both legs in opposite phase, until quietSquares()
    //a square on each leg on its own: two notes at once; 0 holds that leg low
    virtual void duet(uint16_t hertzA, uint16_t hertzB) = 0;
    virtual void quietSquares() = 0;
    virtual void play(uint8_t voice, Synth::Wave wave, float hertz, float level, float seconds, float decaySeconds) = 0;
    virtual void release(uint8_t voice) = 0;    //that synth voice fades out (5 ms)
    virtual void playClip(uint8_t voice, const SoundClip& clip, float level) = 0;
    virtual void playScore(const SynthScoreNote* notes, uint8_t count) = 0;
    void quiet(){
        quietSquares();
        for(uint8_t v = 0; v < Synth::voiceCount; v++) release(v);
    }
};
