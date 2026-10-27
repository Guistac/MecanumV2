#pragma once
#include "Sound.h"

//Host tests [D-158]: a SoundSink that plays nothing and remembers what it was asked to play, per layer: how many
//cues and notes, the last ones (a cue's first note), the tempo, the stop calls. A game test plays with it and checks
//its key sounds fire (and that its demo, without a sink, is silent). isPlaying() is what the test sets
//(setPlaying), false by default.
class RecordingSink : public SoundSink{
public:

    static const uint8_t keep = 16;     //the last plays kept per layer

    //one play: a cue (its notes stay where the game keeps them) or a single note (copied)
    struct Play{
        SoundCue cue;
        SoundNote note = {};            //a note: the note; a cue: its first note
        bool b_note = false;
        uint8_t priority = 0;
        uint32_t order = 0;             //counts every play on both layers, from 1
    };

    bool play(const SoundCue& cue, SoundLayer layer = SoundLayer::FRONT) override {
        Play p;
        p.cue = cue;
        if(cue.noteCount > 0) p.note = cue.notes[0];
        p.priority = cue.priority;
        record(p, layer);
        return true;
    }

    bool playNote(const SoundNote& note, uint8_t priority, SoundLayer layer = SoundLayer::FRONT) override {
        Play p;
        p.note = note;
        p.b_note = true;
        p.priority = priority;
        record(p, layer);
        return true;
    }

    void stop(SoundLayer layer) override { stops[uint8_t(layer)]++; }
    void setTempo(SoundLayer layer, uint16_t percent) override {
        tempo[uint8_t(layer)] = percent;
        tempoCalls[uint8_t(layer)]++;
    }
    bool isPlaying(SoundLayer layer) const override { return b_playing[uint8_t(layer)]; }

    void setPlaying(SoundLayer layer, bool b){ b_playing[uint8_t(layer)] = b; }

    //everything forgotten (the tempo back to 100)
    void clear(){ *this = RecordingSink(); }

    //plays on a layer since the last clear(), cues and notes
    uint32_t count(SoundLayer layer) const { return counts[uint8_t(layer)]; }
    uint32_t cueCount(SoundLayer layer) const { return cues[uint8_t(layer)]; }
    uint32_t noteCount(SoundLayer layer) const { return notes[uint8_t(layer)]; }
    uint32_t total() const { return counts[0] + counts[1]; }
    uint32_t stopCount(SoundLayer layer) const { return stops[uint8_t(layer)]; }
    uint16_t getTempo(SoundLayer layer) const { return tempo[uint8_t(layer)]; }
    uint32_t tempoCount(SoundLayer layer) const { return tempoCalls[uint8_t(layer)]; }

    //the play `back` plays ago on a layer (0: the last one); nullptr when there is none kept
    const Play* last(SoundLayer layer, uint8_t back = 0) const {
        uint8_t l = uint8_t(layer);
        uint32_t n = counts[l];
        if(back >= keep || back >= n) return nullptr;
        return &plays[l][(n - 1 - back) % keep];
    }

    //the pitch of the last play's (first) note on a layer, 0 when nothing played
    uint16_t lastHertz(SoundLayer layer) const {
        const Play* p = last(layer);
        return p ? p->note.hertz : 0;
    }

    //one of the kept plays on a layer has a note at this pitch (a cue: any of its notes)
    bool heard(SoundLayer layer, uint16_t hertz) const {
        uint8_t l = uint8_t(layer);
        uint32_t n = counts[l] < keep ? counts[l] : keep;
        for(uint32_t i = 0; i < n; i++){
            const Play& p = plays[l][i];
            if(p.b_note){
                if(p.note.hertz == hertz) return true;
            }
            else for(uint16_t k = 0; k < p.cue.noteCount; k++) if(p.cue.notes[k].hertz == hertz) return true;
        }
        return false;
    }

private:
    void record(const Play& p, SoundLayer layer){
        uint8_t l = uint8_t(layer);
        Play& slot = plays[l][counts[l] % keep];
        slot = p;
        slot.order = ++order;
        counts[l]++;
        if(p.b_note) notes[l]++;
        else cues[l]++;
    }

    Play plays[2][keep];
    uint32_t counts[2] = {0, 0};
    uint32_t cues[2] = {0, 0};
    uint32_t notes[2] = {0, 0};
    uint32_t stops[2] = {0, 0};
    uint16_t tempo[2] = {100, 100};
    uint32_t tempoCalls[2] = {0, 0};
    bool b_playing[2] = {false, false};
    uint32_t order = 0;
};
