#pragma once
#include <Sound.h>
#include "RemoteSplash.h"

//The BERUPO startup sound [D-158]: the owner's recording (tools/sounds/startup.m4a) taken apart by
//tools/sounds/partials.py: a B flat major 9 sharp 11 chord, then a bell, C7 with C8 just after it. Played one note
//at a time (each ends as the next starts, so they never add up and nothing clips), in time with the animation: the
//chord as a rising arpeggio, one note as each letter appears (50 ms after, as it turns solid), its top note ringing
//out with the chord's measured decay until the shine starts; then, with the shine, the bell's two rising notes, the
//top one ringing out with the bell's decay. Synth squares at full level: loud for the disc (harmonics in its loud band), with real fades. Tried before,
//2026-10-27: samples (faint, noisy), all partials together as sines (faint) or clipped squares (muddy), a gated
//timer-square arpeggio (chopped), only the two strongest partials (too bare).
namespace StartupScore{

    static const uint8_t noteCount = 8;

    //fills notes (noteCount) from the splash's timing
    inline void build(SynthScoreNote* notes){
        static const float chord[6] = {468.0f, 700.0f, 883.0f, 1050.0f, 1324.0f, 1572.0f};    //B flat 4 .. G6
        const float shine = RemoteSplash::shineStart();
        for(uint8_t i = 0; i < 6; i++){
            float start = RemoteSplash::letterStart(i) + RemoteSplash::arpeggioDelaySeconds;
            bool b_top = i == 5;
            float length = b_top ? shine - start : RemoteSplash::letterStart(i + 1) - RemoteSplash::letterStart(i);
            notes[i] = {start, Synth::Wave::SQUARE, chord[i], 1.0f, 0.003f, length, b_top ? 0.480f : 0.0f};
        }
        notes[6] = {shine, Synth::Wave::SQUARE, 2089.0f, 1.0f, 0.003f, 0.081f, 0.0f};             //C7
        notes[7] = {shine + 0.081f, Synth::Wave::SQUARE, 4177.0f, 1.0f, 0.003f, 0.946f, 0.246f};  //C8, rings out
    }
};
