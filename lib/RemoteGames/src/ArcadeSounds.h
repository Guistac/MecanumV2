#pragma once
#include <stdint.h>
#include <Sound.h>

//The arcade's sound modes and its own sounds [D-157, D-158]. The mode is chosen in the arcade (a flick up in the
//picker or the pause screen) and kept in EEPROM; it applies through the ArcadeSink every game plays into. The
//remote's Sound setting at Off still silences everything: the board then gives the Arcade no sink at all.

enum class ArcadeSoundMode : uint8_t{ SOUND_ON, MUSIC_OFF, FX_OFF, MUTE, COUNT };

namespace ArcadeSoundModes{
    //SOUND ON, MUSIC OFF, FX OFF, MUTE
    const char* label(ArcadeSoundMode mode);
    //the picker's pill when the mode changed: SOUND: ON, SOUND: MUSIC OFF, SOUND: FX OFF, SOUND: MUTE
    const char* pillLabel(ArcadeSoundMode mode);
    inline bool allowsMusic(ArcadeSoundMode mode){ return mode == ArcadeSoundMode::SOUND_ON || mode == ArcadeSoundMode::FX_OFF; }
    inline bool allowsEffects(ArcadeSoundMode mode){ return mode == ArcadeSoundMode::SOUND_ON || mode == ArcadeSoundMode::MUSIC_OFF; }
    inline ArcadeSoundMode next(ArcadeSoundMode mode){ return ArcadeSoundMode((uint8_t(mode) + 1) % uint8_t(ArcadeSoundMode::COUNT)); }
    inline ArcadeSoundMode previous(ArcadeSoundMode mode){
        return ArcadeSoundMode((uint8_t(mode) + uint8_t(ArcadeSoundMode::COUNT) - 1) % uint8_t(ArcadeSoundMode::COUNT));
    }
};

//what a played game sounds into: passes its sounds on to the board's player unless the mode drops them (MUSIC OFF
//the back layer, FX OFF the front, MUTE both). No output (Sound setting Off): nothing plays. Stops and tempo
//always pass, so the layers are right when a mode lets them through again
class ArcadeSink : public SoundSink{
public:
    void setOutput(SoundSink* sink){ output = sink; }
    SoundSink* getOutput() const { return output; }
    void setMode(ArcadeSoundMode newMode){ mode = newMode; }
    ArcadeSoundMode getMode() const { return mode; }

    bool allows(SoundLayer layer) const {
        return layer == SoundLayer::BACK ? ArcadeSoundModes::allowsMusic(mode) : ArcadeSoundModes::allowsEffects(mode);
    }

    bool play(const SoundCue& cue, SoundLayer layer = SoundLayer::FRONT) override {
        return output && allows(layer) && output->play(cue, layer);
    }
    bool playNote(const SoundNote& note, uint8_t priority, SoundLayer layer = SoundLayer::FRONT) override {
        return output && allows(layer) && output->playNote(note, priority, layer);
    }
    //never cuts the remote's own sounds (a status chime, the ramp's loop): only what plays at games priority
    void stop(SoundLayer layer) override {
        if(output && output->playingPriority(layer) <= SoundPriority::GAME) output->stop(layer);
    }
    uint8_t playingPriority(SoundLayer layer) const override { return output ? output->playingPriority(layer) : 0; }
    void setTempo(SoundLayer layer, uint16_t percent) override { if(output) output->setTempo(layer, percent); }
    bool isPlaying(SoundLayer layer) const override { return output && output->isPlaying(layer); }

private:
    SoundSink* output = nullptr;
    ArcadeSoundMode mode = ArcadeSoundMode::SOUND_ON;
};

//the arcade's own cues, games priority. In the picker they are timer squares, the loudest (one cuts the picker's beat
//for its 40 ms, the beat goes on with its next note); with a game's music about to play (start, resume) synth notes
namespace ArcadeSounds{
    //back, looping: the picker's drum beat (kick, snare, hats and a bass blip on the root, 120 a minute), from the
    //arcade opening or a return to the picker until a game starts or the arcade closes; music (MUSIC OFF and MUTE
    //silence it)
    SoundCue pickerBeat();
    static const uint16_t pickerBeatStepMillis = 125;   //a sixteenth at 120 a minute
    static const uint16_t pickerBeatSteps = 32;         //two bars of 4/4
    //front: a pushbutton to another game, its pitch one step per game on the C pentatonic (C6 up)
    SoundCue pick(uint8_t gameIndex);
    //front: a game starts, a quick three-note rising jingle (synth)
    SoundCue start();
    //front: pause (two falling), resume (two rising, synth), back to the picker (two falling)
    SoundCue pause();
    SoundCue resume();
    SoundCue back();
    //front: a card opens or closes, a soft tick
    SoundCue cardTick();
    //front: the sound mode changed (only played when the new mode lets effects through)
    SoundCue modeBlip();
    //back, the first note at 400 ms (never cuts the game's own crash sound): a short falling phrase, or a fanfare
    //when the score entered the top 3
    SoundCue gameOver();
    SoundCue fanfare();
    //front: name entry, a letter changed, the cursor moved, the name saved (the fanfare's last bar)
    SoundCue letterTick();
    SoundCue cursorTick();
    SoundCue nameSaved();
    //front: a top 3 cleared, a falling phrase
    SoundCue cleared();
    //front: the arcade closes, the opening jingle (RemoteSounds::arcadeStartCue) reversed
    SoundCue closing();

    //the game over phrases wait this long, so the game's crash sound is heard first
    static const uint16_t gameOverDelayMillis = 400;
};
