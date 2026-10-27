#pragma once
#include "Sound.h"

//Plays cues on two layers (SoundLayer) into a SoundOutput. Main thread: parts call play() whenever, the firmware
//calls update() every loop; nothing blocks. Front uses synth voices 2 and 3, back 0, 1 and 4 (melody, bass, drums).
//Each layer has a tempo (setTempo): its cue time runs at that rate, kept as a position so a change never jumps.
class SoundPlayer : public SoundSink{
public:

    static const uint16_t minTempo = 10;
    static const uint16_t maxTempo = 1000;

    bool play(const SoundCue& cue, SoundLayer layer = SoundLayer::FRONT) override {
        if(cue.noteCount == 0) return false;
        Layer& l = layers[uint8_t(layer)];
        if(l.isPlaying() && cue.priority < l.cue.priority) return false;
        l.start(cue, lastNow);
        return true;
    }

    bool playNote(const SoundNote& note, uint8_t priority, SoundLayer layer = SoundLayer::FRONT) override {
        Layer& l = layers[uint8_t(layer)];
        if(l.isPlaying() && priority < l.cue.priority) return false;
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

    void setTempo(SoundLayer layer, uint16_t percent) override {
        if(percent < minTempo) percent = minTempo;
        if(percent > maxTempo) percent = maxTempo;
        layers[uint8_t(layer)].tempo = percent;
    }
    uint16_t getTempo(SoundLayer layer) const { return layers[uint8_t(layer)].tempo; }

    bool isPlaying(SoundLayer layer) const override { return layers[uint8_t(layer)].isPlaying(); }
    uint8_t playingPriority(SoundLayer layer) const override {
        const Layer& l = layers[uint8_t(layer)];
        return l.isPlaying() ? l.cue.priority : 0;
    }

    //every loop, with the time that play() calls in between count from
    void update(uint32_t now, SoundOutput& out){
        lastNow = now;
        for(uint8_t i = 0; i < uint8_t(SoundLayer::COUNT); i++) layers[i].update(now, out, SoundLayer(i));
    }

    //the synth voice a note of a layer plays on: front 2, 3 (its voice 2 on 3); back 0, 1, 4
    static uint8_t synthVoice(SoundLayer layer, uint8_t voice){
        if(layer == SoundLayer::FRONT) return voice == 0 ? 2 : 3;
        return voice == 0 ? 0 : voice == 1 ? 1 : 4;
    }

private:

    struct Layer{
        SoundCue cue;
        SoundNote ownNote = {};
        uint32_t position = 0;          //the cue's time in hundredths of a millisecond (it runs at the tempo)
        uint32_t lastUpdateMillis = 0;
        uint32_t length = 0;
        uint16_t nextNote = 0;
        uint16_t tempo = 100;
        bool b_active = false;
        bool b_restart = false;
        bool b_stop = false;
        bool b_square = false;
        uint32_t squareEnd = 0;

        bool isPlaying() const {
            if(!b_active || b_stop) return false;
            return cue.loopMillis > 0 || position / 100 < length;
        }

        void start(const SoundCue& newCue, uint32_t now){
            cue = newCue;
            length = cue.lengthMillis();
            position = 0;
            lastUpdateMillis = now;
            nextNote = 0;
            b_active = true;
            b_restart = true;
            b_stop = false;
        }

        void silence(SoundOutput& out, SoundLayer layer){
            if(b_square){
                out.quietSquares();
                b_square = false;
            }
            out.release(synthVoice(layer, 0));
            out.release(synthVoice(layer, 1));
            if(layer == SoundLayer::BACK) out.release(synthVoice(layer, 2));
        }

        void update(uint32_t now, SoundOutput& out, SoundLayer layer){
            if(b_stop){
                silence(out, layer);
                b_active = false;
                b_stop = false;
                return;
            }
            if(b_restart){
                silence(out, layer);
                b_restart = false;
            }
            if(!b_active) return;
            position += (now - lastUpdateMillis) * tempo;
            lastUpdateMillis = now;
            uint32_t elapsed = position / 100;
            //note lengths and decays follow the tempo: milliseconds * 100 / tempo, in seconds
            const float secondsPerMilli = 0.1f / float(tempo);
            while(nextNote < cue.noteCount && elapsed >= cue.notes[nextNote].atMillis){
                const SoundNote& n = cue.notes[nextNote++];
                if(n.kind == SoundNote::Kind::SQUARE || n.kind == SoundNote::Kind::DUET){
                    if(n.kind == SoundNote::Kind::SQUARE) out.tone(n.hertz);
                    else out.duet(n.hertz, n.hertzB);
                    b_square = true;
                    squareEnd = uint32_t(n.atMillis) + n.millis;
                }
                else{
                    out.play(synthVoice(layer, n.voice), n.wave, n.hertz, n.levelPercent / 100.0f,
                             n.millis * secondsPerMilli, n.decayMillis * secondsPerMilli);
                }
            }
            if(b_square && elapsed >= squareEnd){
                out.quietSquares();
                b_square = false;
            }
            if(nextNote >= cue.noteCount && !b_square){
                if(cue.loopMillis > 0 && elapsed >= cue.loopMillis){
                    position -= uint32_t(cue.loopMillis) * 100;
                    nextNote = 0;
                }
                else if(cue.loopMillis == 0 && elapsed >= length) b_active = false;
            }
        }
    };

    Layer layers[uint8_t(SoundLayer::COUNT)];
    uint32_t lastNow = 0;
};
