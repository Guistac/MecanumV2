//The arcade's sound modes and its own cues [D-157, D-158]
#include "ArcadeSounds.h"
#include <SoundKit.h>

namespace ArcadeSoundModes{
    const char* label(ArcadeSoundMode mode){
        static const char* const labels[] = {"SOUND ON", "MUSIC OFF", "FX OFF", "MUTE"};
        return labels[uint8_t(mode) < uint8_t(ArcadeSoundMode::COUNT) ? uint8_t(mode) : 0];
    }
    const char* pillLabel(ArcadeSoundMode mode){
        static const char* const labels[] = {"SOUND: ON", "SOUND: MUSIC OFF", "SOUND: FX OFF", "SOUND: MUTE"};
        return labels[uint8_t(mode) < uint8_t(ArcadeSoundMode::COUNT) ? uint8_t(mode) : 0];
    }
};

namespace ArcadeSounds{

    namespace{
        using W = Synth::Wave;
        const SoundNote::Kind SQ = SoundNote::Kind::SQUARE, SY = SoundNote::Kind::SYNTH;

        //C6 D6 E6 G6 A6 C7 D7 E7: one per game
        const SoundNote pickNotes[] = {
            {0, SQ, 0, W::SQUARE, 1047, 40, 0, 0},
            {0, SQ, 0, W::SQUARE, 1175, 40, 0, 0},
            {0, SQ, 0, W::SQUARE, 1319, 40, 0, 0},
            {0, SQ, 0, W::SQUARE, 1568, 40, 0, 0},
            {0, SQ, 0, W::SQUARE, 1760, 40, 0, 0},
            {0, SQ, 0, W::SQUARE, 2093, 40, 0, 0},
            {0, SQ, 0, W::SQUARE, 2349, 40, 0, 0},
            {0, SQ, 0, W::SQUARE, 2637, 40, 0, 0},
        };
        //C6 E6 G6, the last one ringing
        const SoundNote startNotes[] = {
            {0, SY, 0, W::SQUARE, 1047, 55, 100, 0},
            {60, SY, 0, W::SQUARE, 1319, 55, 100, 0},
            {120, SY, 0, W::SQUARE, 1568, 160, 100, 80},
        };
        const SoundNote pauseNotes[] = {
            {0, SQ, 0, W::SQUARE, 1568, 60, 0, 0},
            {75, SQ, 0, W::SQUARE, 1047, 90, 0, 0},
        };
        const SoundNote resumeNotes[] = {
            {0, SY, 0, W::SQUARE, 1047, 60, 100, 0},
            {75, SY, 0, W::SQUARE, 1568, 90, 100, 40},
        };
        const SoundNote backNotes[] = {
            {0, SQ, 0, W::SQUARE, 1319, 50, 0, 0},
            {60, SQ, 0, W::SQUARE, 880, 80, 0, 0},
        };
        const SoundNote cardTickNotes[] = {{0, SY, 0, W::SQUARE, 3520, 15, 80, 8}};
        const SoundNote modeBlipNotes[] = {{0, SQ, 0, W::SQUARE, 2093, 40, 0, 0}};
        //G5 E5 C5 G4, the last one longer, falling away
        const SoundNote gameOverNotes[] = {
            {400, SY, 0, W::SQUARE, 784, 110, 95, 0},
            {530, SY, 0, W::SQUARE, 659, 110, 95, 0},
            {660, SY, 0, W::SQUARE, 523, 110, 95, 0},
            {790, SY, 0, W::SQUARE, 392, 400, 95, 160},
        };
        //C5 E5 G5 C6 quick, then the last bar: G5, C6 held, an octave below under it
        const SoundNote fanfareNotes[] = {
            {400, SY, 0, W::SQUARE, 523, 70, 100, 0},
            {480, SY, 0, W::SQUARE, 659, 70, 100, 0},
            {560, SY, 0, W::SQUARE, 784, 70, 100, 0},
            {640, SY, 0, W::SQUARE, 1047, 150, 100, 0},
            {840, SY, 0, W::SQUARE, 784, 90, 100, 0},
            {940, SY, 0, W::SQUARE, 1047, 420, 100, 200},
            {940, SY, 1, W::PULSE25, 523, 420, 80, 200},
        };
        //the fanfare's last bar, on its own
        const SoundNote nameSavedNotes[] = {
            {0, SY, 0, W::SQUARE, 1568, 90, 100, 0},
            {100, SY, 0, W::SQUARE, 2093, 300, 100, 150},
            {100, SY, 1, W::PULSE25, 1047, 300, 80, 150},
        };
        const SoundNote letterTickNotes[] = {{0, SQ, 0, W::SQUARE, 3136, 20, 0, 0}};
        const SoundNote cursorTickNotes[] = {{0, SY, 0, W::SQUARE, 2637, 12, 70, 6}};
        //C7 A6 G6 E6 D6 C6, quick, the last one longer
        const SoundNote clearedNotes[] = {
            {0, SQ, 0, W::SQUARE, 2093, 45, 0, 0},
            {55, SQ, 0, W::SQUARE, 1760, 45, 0, 0},
            {110, SQ, 0, W::SQUARE, 1568, 45, 0, 0},
            {165, SQ, 0, W::SQUARE, 1319, 45, 0, 0},
            {220, SQ, 0, W::SQUARE, 1175, 45, 0, 0},
            {275, SQ, 0, W::SQUARE, 1047, 160, 0, 0},
        };
        //RemoteSounds' arcade start jingle backwards: C8 G7 E7 C7 G6
        const SoundNote closingNotes[] = {
            {0, SQ, 0, W::SQUARE, 4186, 60, 0, 0},
            {70, SQ, 0, W::SQUARE, 3136, 60, 0, 0},
            {140, SQ, 0, W::SQUARE, 2637, 60, 0, 0},
            {210, SQ, 0, W::SQUARE, 2093, 60, 0, 0},
            {280, SQ, 0, W::SQUARE, 1568, 220, 0, 0},
        };
    }

    //two bars: kick on 1 and 3 (a push before the second bar's 4), snare on 2 and 4, hats on the off-beats; a short
    //blip on the root under each kick (C5, G4 in the second bar's second half: where the disc plays them). Built
    //once: the player keeps a pointer to it
    SoundCue pickerBeat(){
        using namespace SoundKit;
        static NoteList<64> notes;
        static SoundCue cue;
        if(notes.size() == 0){
            static const Step bass[] = {
                {0, 1, C5, 2, 60}, {8, 1, C5, 2, 55}, {16, 1, C5, 2, 60}, {24, 1, G4, 2, 55}, {26, 1, G4, 2, 50},
            };
            Tracker t(notes, pickerBeatStepMillis);
            t.instrument(1, Synth::Wave::PULSE12, 50, 60);
            t.drums("K.H.S.H.K.H.S.H.", 0, 1, 95).drums("K.H.S.H.K.HKS.HH", 16, 1, 95);
            t.steps(bass, sizeof(bass) / sizeof(bass[0]));
            cue = t.cue(pickerBeatSteps);
        }
        return cue;
    }

    SoundCue pick(uint8_t gameIndex){
        const uint8_t count = sizeof(pickNotes) / sizeof(SoundNote);
        SoundCue c;
        c.notes = &pickNotes[gameIndex % count];
        c.noteCount = 1;
        c.priority = SoundPriority::GAME;
        return c;
    }
    SoundCue start(){ return soundCue(startNotes, SoundPriority::GAME); }
    SoundCue pause(){ return soundCue(pauseNotes, SoundPriority::GAME); }
    SoundCue resume(){ return soundCue(resumeNotes, SoundPriority::GAME); }
    SoundCue back(){ return soundCue(backNotes, SoundPriority::GAME); }
    SoundCue cardTick(){ return soundCue(cardTickNotes, SoundPriority::GAME); }
    SoundCue modeBlip(){ return soundCue(modeBlipNotes, SoundPriority::GAME); }
    SoundCue gameOver(){ return soundCue(gameOverNotes, SoundPriority::GAME); }
    SoundCue fanfare(){ return soundCue(fanfareNotes, SoundPriority::GAME); }
    SoundCue letterTick(){ return soundCue(letterTickNotes, SoundPriority::GAME); }
    SoundCue cursorTick(){ return soundCue(cursorTickNotes, SoundPriority::GAME); }
    SoundCue nameSaved(){ return soundCue(nameSavedNotes, SoundPriority::GAME); }
    SoundCue cleared(){ return soundCue(clearedNotes, SoundPriority::GAME); }
    SoundCue closing(){ return soundCue(closingNotes, SoundPriority::GAME); }
};
