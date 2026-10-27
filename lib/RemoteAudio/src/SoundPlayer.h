#pragma once
#include "Sound.h"

//Plays cues on two layers (SoundLayer) into a SoundOutput. Main thread: parts call play() whenever, the firmware
//calls update() every loop; nothing blocks. Front uses synth voices 2 and 3, back 0 and 1.
class SoundPlayer : public SoundSink{
public:

    bool play(const SoundCue& cue, SoundLayer layer = SoundLayer::FRONT) override {
        if(cue.noteCount == 0) return false;
        Layer& l = layers[uint8_t(layer)];
        if(l.isPlaying(lastNow) && cue.priority < l.cue.priority) return false;
        l.start(cue, lastNow);
        return true;
    }

    bool playNote(const SoundNote& note, uint8_t priority, SoundLayer layer = SoundLayer::FRONT) override {
        Layer& l = layers[uint8_t(layer)];
        if(l.isPlaying(lastNow) && priority < l.cue.priority) return false;
        l.ownNote = note;
        l.ownNote.atMillis = 0;
        SoundCue cue;
        cue.notes = &l.ownNote;
        cue.noteCount = 1;
        cue.priority = priority;
        return play(cue, layer);
    }

    void stop(SoundLayer layer) override {
        Layer& l = layers[uint8_t(layer)];
        if(l.b_active) l.b_stop = true;
    }

    bool isPlaying(SoundLayer layer) const { return layers[uint8_t(layer)].isPlaying(lastNow); }
    uint8_t playingPriority(SoundLayer layer) const {
        const Layer& l = layers[uint8_t(layer)];
        return l.isPlaying(lastNow) ? l.cue.priority : 0;
    }

    //every loop, with the time that play() calls in between count from
    void update(uint32_t now, SoundOutput& out){
        lastNow = now;
        for(uint8_t i = 0; i < uint8_t(SoundLayer::COUNT); i++) layers[i].update(now, out, uint8_t(i == 0 ? 2 : 0));
    }

private:

    struct Layer{
        SoundCue cue;
        SoundNote ownNote = {};
        uint32_t startMillis = 0;
        uint32_t length = 0;
        uint8_t nextNote = 0;
        bool b_active = false;
        bool b_restart = false;
        bool b_stop = false;
        bool b_square = false;
        uint32_t squareEnd = 0;

        bool isPlaying(uint32_t now) const {
            if(!b_active || b_stop) return false;
            return cue.loopMillis > 0 || now - startMillis < length;
        }

        void start(const SoundCue& newCue, uint32_t now){
            cue = newCue;
            length = cue.lengthMillis();
            startMillis = now;
            nextNote = 0;
            b_active = true;
            b_restart = true;
            b_stop = false;
        }

        void silence(SoundOutput& out, uint8_t voiceBase){
            if(b_square){
                out.quietSquares();
                b_square = false;
            }
            out.release(voiceBase);
            out.release(voiceBase + 1);
        }

        void update(uint32_t now, SoundOutput& out, uint8_t voiceBase){
            if(b_stop){
                silence(out, voiceBase);
                b_active = false;
                b_stop = false;
                return;
            }
            if(b_restart){
                silence(out, voiceBase);
                b_restart = false;
            }
            if(!b_active) return;
            uint32_t elapsed = now - startMillis;
            while(nextNote < cue.noteCount && elapsed >= cue.notes[nextNote].atMillis){
                const SoundNote& n = cue.notes[nextNote++];
                if(n.kind == SoundNote::Kind::SQUARE || n.kind == SoundNote::Kind::DUET){
                    if(n.kind == SoundNote::Kind::SQUARE) out.tone(n.hertz);
                    else out.duet(n.hertz, n.hertzB);
                    b_square = true;
                    squareEnd = uint32_t(n.atMillis) + n.millis;
                }
                else{
                    out.play(uint8_t(voiceBase + (n.voice & 1)), n.wave, n.hertz, n.levelPercent / 100.0f,
                             n.millis / 1000.0f, n.decayMillis / 1000.0f);
                }
            }
            if(b_square && elapsed >= squareEnd){
                out.quietSquares();
                b_square = false;
            }
            if(nextNote >= cue.noteCount && !b_square){
                if(cue.loopMillis > 0 && elapsed >= cue.loopMillis){
                    startMillis += cue.loopMillis;
                    nextNote = 0;
                }
                else if(cue.loopMillis == 0 && elapsed >= length) b_active = false;
            }
        }
    };

    Layer layers[uint8_t(SoundLayer::COUNT)];
    uint32_t lastNow = 0;
};
