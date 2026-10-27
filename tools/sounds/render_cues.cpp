//Renders the remote's sounds to WAV files on the Mac [D-158]: cues, a game's music, or a game played by its demo
//player with its sound on, through the real SoundPlayer and Synth. Timer squares (SQUARE, DUET) are rendered as ideal
//squares, and as on the board they silence the synth while they sound. Mono, 16 bit, 31.25 kHz (the synth's rate);
//the signal is what the disc sees (the difference of its two legs). Built and run by tools/sounds/render_cues.py:
//
//    render_cues <out folder> list
//    render_cues <out folder> cue <name|all> [seconds]       remote_* (RemoteSounds), arcade_* (the arcade), kit_*
//    render_cues <out folder> music <game|all> [seconds]     the game's playMusic() alone (a new game, no ticks)
//    render_cues <out folder> game <game|all> <seconds> [seed] [mode]
//                                                            the game played by its demo with sound on (the picker's
//                                                            demo is silent: this bypasses it), the arcade's game over
//                                                            phrase, then the next game; mode 0..3 = SOUND ON, MUSIC
//                                                            OFF, FX OFF, MUTE
//
//Writes <out>/cue_<name>.wav, music_<game>.wav, game_<game>.wav and prints each one's length, peak and disc loudness
//(DiscMeter: the level through a model of the piezo disc, to compare music with effects).
#include <Arcade.h>
#include <ArcadeSounds.h>
#include <RemoteSounds.h>
#include <SoundKit.h>
#include <SoundPlayer.h>
#include <Synth.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <string>
#include <vector>

//the board's buzzer as RemoteSound drives it: squares and the synth share the pins, each stops the other; the synth
//runs (its interrupt) from a note until it is idle
class HostOutput : public SoundOutput{
public:
    void tone(uint16_t hertz) override {
        stopAll();
        if(hertz == 0) return;
        squareMode = 1;
        hertzA = hertz;
        phaseA = 0;
    }
    void duet(uint16_t a, uint16_t b) override {
        stopAll();
        squareMode = 2;
        hertzA = a;
        hertzB = b;
        phaseA = phaseB = 0;
    }
    void quietSquares() override { squareMode = 0; }
    void play(uint8_t voice, Synth::Wave wave, float hertz, float level, float seconds, float decaySeconds) override {
        squareMode = 0;
        synth.play(voice, wave, hertz, level, seconds, decaySeconds);
        b_synth = true;
    }
    void release(uint8_t voice) override { synth.stop(voice); }
    void playClip(uint8_t voice, const SoundClip& clip, float level) override {
        squareMode = 0;
        synth.playClip(voice, clip.samples, clip.count, clip.rate, level);
        b_synth = true;
    }
    void playScore(const SynthScoreNote* notes, uint8_t count) override {
        squareMode = 0;
        for(uint8_t i = 0; i < count && i < Synth::voiceCount; i++){
            const SynthScoreNote& n = notes[i];
            synth.play(i, n.wave, n.hertz, n.level, n.seconds, n.decaySeconds, n.attackSeconds, n.startSeconds);
        }
        b_synth = true;
    }

    //the next sample, -1..1
    float next(){
        const float dt = 1.0f / float(Synth::sampleRate);
        if(squareMode == 1){
            phaseA += hertzA * dt;
            phaseA -= float(int(phaseA));
            return phaseA < 0.5f ? 1.0f : -1.0f;
        }
        if(squareMode == 2){
            float a = 0.0f, b = 0.0f;
            if(hertzA){
                phaseA += hertzA * dt;
                phaseA -= float(int(phaseA));
                a = phaseA < 0.5f ? 1.0f : 0.0f;
            }
            if(hertzB){
                phaseB += hertzB * dt;
                phaseB -= float(int(phaseB));
                b = phaseB < 0.5f ? 1.0f : 0.0f;
            }
            return a - b;
        }
        if(!b_synth) return 0.0f;
        Synth::Pair p = synth.nextPair();
        if(synth.isIdle()) b_synth = false;
        return (float(p.a) - float(p.b)) / 255.0f;
    }

private:
    void stopAll(){
        squareMode = 0;
        synth.stopAll();
        b_synth = false;
    }
    Synth synth;
    bool b_synth = false;
    uint8_t squareMode = 0;     //0 none, 1 tone, 2 duet
    uint16_t hertzA = 0, hertzB = 0;
    float phaseA = 0.0f, phaseB = 0.0f;
};

//how loud a signal is on the remote's 12 mm piezo disc: it plays from about 1 kHz to 5 kHz and is nearly silent below
//400 Hz, so the level is taken after a model of it, a 2nd-order high-pass at 900 Hz (Butterworth) and a gentle
//1st-order low-pass at 6 kHz. Measured in 50 ms windows: "disc" is the RMS over the windows where something sounds
//(so a short effect and a long tune compare), "loudest" the loudest window; dB of full scale (a square at 1 = 0 dB)
struct DiscMeter{
    static constexpr uint32_t windowSamples = Synth::sampleRate / 20;
    float b0 = 0, b1 = 0, b2 = 0, a1 = 0, a2 = 0;      //the high-pass biquad (RBJ)
    float x1 = 0, x2 = 0, y1 = 0, y2 = 0;
    float lowAlpha = 0, low = 0;                         //the low-pass
    double windowDisc = 0.0, windowRaw = 0.0;
    uint32_t windowCount = 0;
    double activeDisc = 0.0;
    uint32_t activeWindows = 0;
    double loudestWindow = 0.0;

    DiscMeter(){
        const float pi = 3.14159265f;
        float w = 2.0f * pi * 900.0f / float(Synth::sampleRate);
        float alpha = sinf(w) / (2.0f * 0.7071f);
        float c = cosf(w);
        float a0 = 1.0f + alpha;
        b0 = (1.0f + c) / 2.0f / a0;
        b1 = -(1.0f + c) / a0;
        b2 = b0;
        a1 = -2.0f * c / a0;
        a2 = (1.0f - alpha) / a0;
        float rc = 1.0f / (2.0f * pi * 6000.0f);
        float dt = 1.0f / float(Synth::sampleRate);
        lowAlpha = dt / (rc + dt);
    }

    void add(float x){
        float y = b0 * x + b1 * x1 + b2 * x2 - a1 * y1 - a2 * y2;
        x2 = x1; x1 = x; y2 = y1; y1 = y;
        low += lowAlpha * (y - low);
        windowDisc += double(low) * low;
        windowRaw += double(x) * x;
        if(++windowCount < windowSamples) return;
        double raw = windowRaw / windowCount, disc = windowDisc / windowCount;
        //something sounds: the unfiltered signal above -60 dB
        if(raw > 1e-6){
            activeDisc += disc;
            activeWindows++;
            if(disc > loudestWindow) loudestWindow = disc;
        }
        windowDisc = windowRaw = 0.0;
        windowCount = 0;
    }

    static double decibels(double meanSquare){ return meanSquare > 1e-12 ? 10.0 * log10(meanSquare) : -120.0; }
    double disc() const { return activeWindows ? decibels(activeDisc / activeWindows) : -120.0; }
    double loudest() const { return decibels(loudestWindow); }
};

//the player, the output and the samples: run() advances the time a millisecond at a time
struct Render{
    SoundPlayer player;
    HostOutput out;
    std::vector<int16_t> samples;
    uint32_t now = 0;
    double owed = 0.0;
    float peak = 0.0f;
    DiscMeter meter;

    void run(uint32_t millis){
        for(uint32_t i = 0; i < millis; i++){
            player.update(now, out);
            owed += double(Synth::sampleRate) / 1000.0;
            while(owed >= 1.0){
                owed -= 1.0;
                float v = out.next();
                if(v > peak) peak = v;
                if(-v > peak) peak = -v;
                meter.add(v);
                samples.push_back(int16_t(v * 0.8f * 32767.0f));
            }
            now++;
        }
    }

    bool write(const std::string& path){
        FILE* f = fopen(path.c_str(), "wb");
        if(!f) return false;
        uint32_t rate = Synth::sampleRate;
        uint32_t dataBytes = uint32_t(samples.size() * 2);
        auto u32 = [&](uint32_t v){ fwrite(&v, 4, 1, f); };
        auto u16 = [&](uint16_t v){ fwrite(&v, 2, 1, f); };
        fwrite("RIFF", 1, 4, f);
        u32(36 + dataBytes);
        fwrite("WAVEfmt ", 1, 8, f);
        u32(16);
        u16(1);
        u16(1);
        u32(rate);
        u32(rate * 2);
        u16(2);
        u16(16);
        fwrite("data", 1, 4, f);
        u32(dataBytes);
        fwrite(samples.data(), 2, samples.size(), f);
        fclose(f);
        printf("%s: %.2f s, peak %.2f, disc %.1f dB (loudest 50 ms %.1f dB)\n", path.c_str(),
               double(samples.size()) / rate, double(peak), meter.disc(), meter.loudest());
        return true;
    }
};

//a bar of the kit's drums and a bass under it, built with the tracker: what the kit sounds like
static SoundCue kitGroove(){
    using namespace SoundKit;
    static NoteList<128> notes;
    //(the bass where the disc plays it: C4 and up, short, on a narrow pulse)
    static const uint8_t bass[] = {C4, HOLD, C4, G4, SoundKit::A3, HOLD, SoundKit::A3, E4, F4, HOLD, F4, C5, G4, HOLD, G4, D5};
    notes.clear();
    Tracker t(notes, 150);
    t.instrument(1, Synth::Wave::PULSE12, 40, 60);
    t.line(1, bass, sizeof(bass), 0, 1, 60).drums("K.HSK.HS", 0, 2);
    return t.cue(0);
}

struct NamedCue{ const char* name; SoundCue (*make)(); SoundLayer layer; };

//Blocks' tune with its drums (from level 3), as the game builds it
static SoundCue blocksWithDrums(){ static BlocksGame blocks; return blocks.tuneCue(true); }

//two of the games' effects, as they play them (references for the music's loudness)
static SoundCue fxSnakeEat(){
    static SoundNote n[1];
    n[0] = SoundKit::synthNote(0, SnakeGame::eatPitch(3), 45, 90, 25);
    SoundCue c; c.notes = n; c.noteCount = 1; c.priority = SoundPriority::GAME;
    return c;
}
static SoundCue fxBlocksRotate(){
    static SoundNote n[1];
    n[0] = SoundKit::synthNote(0, BlocksGame::rotateHertz, 30, 80, 15);
    SoundCue c; c.notes = n; c.noteCount = 1; c.priority = SoundPriority::GAME;
    return c;
}

static SoundCue kitKick(){ static SoundKit::NoteList<8> n; n.clear(); n.kick(0); return n.cue(); }
static SoundCue kitSnare(){ static SoundKit::NoteList<4> n; n.clear(); n.snare(0); return n.cue(); }
static SoundCue kitHat(){ static SoundKit::NoteList<4> n; n.clear(); n.hat(0); return n.cue(); }
static SoundCue kitSweepUp(){ static SoundKit::NoteList<32> n; n.clear(); n.sweep(0, 400, 2400, 250, 12); return n.cue(); }
static SoundCue kitSweepDown(){ static SoundKit::NoteList<32> n; n.clear(); n.sweep(0, 2400, 300, 400, 16, Synth::Wave::SQUARE, 80, 0, 80); return n.cue(); }
static SoundCue kitBurst(){ static SoundKit::NoteList<4> n; n.clear(); n.burst(0, 2500, 300, 100, 90); return n.cue(); }

#define REMOTE(NAME, SOUND) {"remote_" NAME, []{ return RemoteSounds::cue(RemoteSounds::Sound::SOUND); }, SoundLayer::FRONT}
static const NamedCue cues[] = {
    REMOTE("click", CLICK), REMOTE("value", VALUE), REMOTE("bump", BUMP), REMOTE("saved", SAVED),
    REMOTE("discarded", DISCARDED), REMOTE("press", PRESS), REMOTE("refused", REFUSED),
    REMOTE("enabling", STATUS_ENABLING), REMOTE("enabled", STATUS_ENABLED), REMOTE("disabling", STATUS_DISABLING),
    REMOTE("disabled", STATUS_DISABLED), REMOTE("estop", STATUS_ESTOP), REMOTE("estop_released", STATUS_ESTOP_RELEASED),
    REMOTE("link_found", STATUS_LINK_FOUND), REMOTE("warning", WARNING), REMOTE("speed_off", SPEED_OFF),
    REMOTE("speed_slow", SPEED_SLOW), REMOTE("speed_fast", SPEED_FAST), REMOTE("direction_set", DIRECTION_SET),
    REMOTE("direction_clear", DIRECTION_CLEAR), REMOTE("searching", SEARCHING),
    {"remote_arcade_loading", RemoteSounds::arcadeLoadingCue, SoundLayer::FRONT},
    {"remote_arcade_start", RemoteSounds::arcadeStartCue, SoundLayer::FRONT},
    {"arcade_pick", []{ return ArcadeSounds::pick(2); }, SoundLayer::FRONT},
    {"arcade_start", ArcadeSounds::start, SoundLayer::FRONT},
    {"arcade_pause", ArcadeSounds::pause, SoundLayer::FRONT},
    {"arcade_resume", ArcadeSounds::resume, SoundLayer::FRONT},
    {"arcade_back", ArcadeSounds::back, SoundLayer::FRONT},
    {"arcade_card_tick", ArcadeSounds::cardTick, SoundLayer::FRONT},
    {"arcade_mode_blip", ArcadeSounds::modeBlip, SoundLayer::FRONT},
    {"arcade_game_over", ArcadeSounds::gameOver, SoundLayer::BACK},
    {"arcade_fanfare", ArcadeSounds::fanfare, SoundLayer::BACK},
    {"arcade_letter_tick", ArcadeSounds::letterTick, SoundLayer::FRONT},
    {"arcade_cursor_tick", ArcadeSounds::cursorTick, SoundLayer::FRONT},
    {"arcade_name_saved", ArcadeSounds::nameSaved, SoundLayer::FRONT},
    {"arcade_cleared", ArcadeSounds::cleared, SoundLayer::FRONT},
    {"arcade_closing", ArcadeSounds::closing, SoundLayer::FRONT},
    {"arcade_picker_beat", ArcadeSounds::pickerBeat, SoundLayer::BACK},
    {"kit_kick", kitKick, SoundLayer::BACK},
    {"kit_snare", kitSnare, SoundLayer::BACK},
    {"kit_hat", kitHat, SoundLayer::BACK},
    {"kit_sweep_up", kitSweepUp, SoundLayer::FRONT},
    {"kit_sweep_down", kitSweepDown, SoundLayer::FRONT},
    {"kit_burst", kitBurst, SoundLayer::FRONT},
    {"kit_groove", kitGroove, SoundLayer::BACK},
    {"music_blocks_drums", blocksWithDrums, SoundLayer::BACK},
    {"fx_snake_eat", fxSnakeEat, SoundLayer::FRONT},
    {"fx_blocks_rotate", fxBlocksRotate, SoundLayer::FRONT},
};
#undef REMOTE

static std::string lower(std::string s){
    for(auto& c : s) c = char(tolower(c));
    return s;
}

//a cue alone: its length plus half a second of tail, or the given seconds (a looping cue: 8 s)
static void renderCue(const NamedCue& c, const std::string& outDir, float seconds){
    Render r;
    SoundCue cue = c.make();
    r.player.update(0, r.out);
    r.player.play(cue, c.layer);
    uint32_t millis = seconds > 0 ? uint32_t(seconds * 1000.0f) : cue.loopMillis ? 8000 : cue.lengthMillis() + 500;
    r.run(millis);
    r.write(outDir + "/cue_" + c.name + ".wav");
}

static void renderMusic(Game& game, const std::string& outDir, float seconds, uint32_t seed){
    Render r;
    r.player.update(0, r.out);
    game.setSound(&r.player);
    game.reset(seed);
    game.playMusic();
    r.run(uint32_t(seconds * 1000.0f));
    game.setSound(nullptr);
    r.write(outDir + "/music_" + lower(game.name()) + ".wav");
}

//as the Arcade plays it: tempo 100, the music, a tick every 20 ms with the demo's input; at game over the music stops,
//the arcade's phrase plays, 2 s later the next game
static void renderGame(Game& game, const std::string& outDir, float seconds, uint32_t seed, ArcadeSoundMode mode){
    Render r;
    ArcadeSink sink;
    sink.setOutput(&r.player);
    sink.setMode(mode);
    r.player.update(0, r.out);
    game.setSound(&sink);
    game.reset(seed);
    r.player.setTempo(SoundLayer::BACK, 100);
    game.playMusic();
    uint32_t total = uint32_t(seconds * 1000.0f);
    int games = 1;
    while(r.now < total){
        GameInput input;
        game.demo(input);
        game.update(input);
        r.run(20);
        if(game.isOver()){
            sink.stop(SoundLayer::BACK);
            r.player.setTempo(SoundLayer::BACK, 100);
            if(ArcadeSoundModes::allowsEffects(mode)) r.player.play(ArcadeSounds::gameOver(), SoundLayer::BACK);
            r.run(2000);
            game.reset(seed + uint32_t(++games));
            game.playMusic();
        }
    }
    game.setSound(nullptr);
    printf("%s: %d game(s), last score %lu\n", game.name(), games, (unsigned long)game.score());
    r.write(outDir + "/game_" + lower(game.name()) + ".wav");
}

int main(int argc, char** argv){
    if(argc < 3){
        fprintf(stderr, "render_cues <out folder> list | cue <name|all> [s] | music <game|all> [s] | game <game|all> <s> [seed] [mode]\n");
        return 2;
    }
    std::string outDir = argv[1];
    std::string what = argv[2];
    std::string which = argc > 3 ? argv[3] : "all";
    Arcade arcade;
    if(what == "list"){
        for(const NamedCue& c : cues) printf("cue %s\n", c.name);
        for(uint8_t i = 0; i < Arcade::gameCount; i++) printf("game %s\n", lower(arcade.game(i).name()).c_str());
        return 0;
    }
    if(what == "cue"){
        float seconds = argc > 4 ? float(atof(argv[4])) : 0.0f;
        bool b_found = false;
        for(const NamedCue& c : cues){
            if(which != "all" && which != c.name) continue;
            b_found = true;
            renderCue(c, outDir, seconds);
        }
        if(!b_found){
            fprintf(stderr, "no cue called %s (render_cues <out> list)\n", which.c_str());
            return 2;
        }
        return 0;
    }
    if(what == "music" || what == "game"){
        float seconds = argc > 4 ? float(atof(argv[4])) : (what == "music" ? 20.0f : 60.0f);
        uint32_t seed = argc > 5 ? uint32_t(strtoul(argv[5], nullptr, 10)) : 12345;
        uint8_t mode = argc > 6 ? uint8_t(atoi(argv[6])) : 0;
        bool b_found = false;
        for(uint8_t i = 0; i < Arcade::gameCount; i++){
            Game& game = arcade.game(i);
            if(which != "all" && strcasecmp(which.c_str(), game.name()) != 0) continue;
            b_found = true;
            if(what == "music") renderMusic(game, outDir, seconds, seed);
            else renderGame(game, outDir, seconds, seed, ArcadeSoundMode(mode % uint8_t(ArcadeSoundMode::COUNT)));
        }
        if(!b_found){
            fprintf(stderr, "no game called %s\n", which.c_str());
            return 2;
        }
        return 0;
    }
    fprintf(stderr, "unknown command %s\n", what.c_str());
    return 2;
}
