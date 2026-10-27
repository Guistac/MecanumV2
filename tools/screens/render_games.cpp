//Records the remote's games playing themselves (their attract mode) and the Arcade's own screens, as raw 128 x 32
//frames at 50 Hz. Built and run by tools/screens/render_games.py on the Mac; the firmware draws the same code.
//    render_games <out folder> <game name or "all" or "arcade"> <seconds> [seed]
#include <Adafruit_GFX.h>
#include <Arcade.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>

static void writeFrame(FILE* f, GFXcanvas1& canvas){
    fwrite(canvas.getBuffer(), 1, 128 * 32 / 8, f);
}

static void recordGame(Game& game, const std::string& path, float seconds, uint32_t seed){
    FILE* f = fopen(path.c_str(), "wb");
    GFXcanvas1 canvas(128, 32);
    game.reset(seed);
    int ticks = int(seconds / Game::tickSeconds);
    int games = 1;
    for(int t = 0; t < ticks; t++){
        GameInput input;
        game.demo(input);
        game.update(input);
        canvas.fillScreen(0);
        game.draw(canvas);
        writeFrame(f, canvas);
        if(game.isOver()){
            //hold the last frame half a second, then the next game
            for(int i = 0; i < 25; i++) writeFrame(f, canvas);
            game.reset(seed + uint32_t(++games));
        }
    }
    fclose(f);
    printf("%s: %d ticks, %d game(s), last score %lu\n", game.name(), ticks, games, (unsigned long)game.score());
}

//the Arcade as the operator sees it, scripted: the picker, its help and top 3 cards, the sound modes, the next
//games, Cave played (paused, resumed, let fall), the game over and the name entry, the top 3 card with the new entry,
//the clear question; the rest of the time the picker. Reacts to the Arcade's mode, so it follows the game's length
static void recordArcade(const std::string& path, float seconds){
    FILE* f = fopen(path.c_str(), "wb");
    GFXcanvas1 canvas(128, 32);
    RamArcadeStore store;
    Arcade arcade;
    arcade.setStore(&store);
    uint32_t now = 1000;
    arcade.enter(now);
    int frames = int(seconds / Game::tickSeconds);
    Arcade::Mode lastMode = arcade.getMode();
    int modeFrame = 0;          //frames since the mode changed
    int phase = 0;              //0 before the game, 1 after it
    for(int i = 0; i < frames; i++){
        now += 20;
        GameInput sticks;
        bool b_previous = false, b_next = false, b_previousHeld = false, b_nextHeld = false;
        Arcade::Mode mode = arcade.getMode();
        if(mode != lastMode){
            static const char* const names[] = {"picker", "playing", "over", "name"};
            printf("frame %d: %s\n", i, names[uint8_t(mode)]);
            lastMode = mode;
            modeFrame = 0;
        }
        int t = modeFrame++;
        auto flick = [&](int at, float x, float y){
            if(t >= at && t < at + 3){
                sticks.stick[0] = x;
                sticks.stick[1] = y;
            }
        };
        switch(mode){
            case Arcade::Mode::PICKER:
                if(phase == 0){
                    flick(50, 1, 0);                        //help card
                    if(t == 150) b_next = true;             //closes it (the same game)
                    flick(180, 0, -1);                      //top 3 card (empty)
                    flick(260, 0, 1);                       //closes it
                    for(int k = 0; k < 4; k++) flick(300 + 40 * k, 0, 1);  //the sound modes round
                    if(t == 480 || t == 500) b_next = true; //to Cave
                    if(t == 540 || t == 541) sticks.held[0] = true;     //play
                }
                else{
                    flick(40, 0, -1);                       //the top 3 card with the new entry
                    flick(140, 0, 1);
                    if(t >= 180 && t < 340) b_previousHeld = b_nextHeld = true;     //the clear question after 3 s
                    if(t == 180) b_previous = true;
                    if(t == 181) b_next = true;
                    if(t == 360 || t == 361) sticks.held[1] = true;     //cleared
                }
                break;
            case Arcade::Mode::PLAYING:
                if(t == 30 || t == 80) b_next = true;       //pause, resume
                flick(50, 0, 1);                            //the sound mode on the pause screen
                if(t == 120 || t == 121) sticks.held[0] = true;     //one click of thrust, then it falls
                break;
            case Arcade::Mode::OVER:
                if(phase == 1 && t == 100) b_previous = true;   //after the name: back to the picker
                break;
            case Arcade::Mode::NAME:
                phase = 1;
                flick(20, 0, 1);                            //B
                if(t >= 50 && t < 100){                     //held up: C, D, ...
                    sticks.stick[1] = 1;
                }
                flick(120, 1, 0);                           //the next letter
                flick(150, 0, -1);                          //back to Z... from A
                if(t == 200 || t == 201) sticks.held[0] = true;     //saved
                break;
        }
        arcade.update(sticks, b_previous, b_next, now, b_previousHeld, b_nextHeld);
        canvas.fillScreen(0);
        arcade.draw(canvas);
        writeFrame(f, canvas);
    }
    fclose(f);
}

int main(int argc, char** argv){
    if(argc < 4){
        fprintf(stderr, "render_games <out folder> <game name|all|arcade> <seconds> [seed]\n");
        return 2;
    }
    std::string outDir = argv[1];
    std::string which = argv[2];
    float seconds = float(atof(argv[3]));
    uint32_t seed = argc > 4 ? uint32_t(strtoul(argv[4], nullptr, 10)) : 12345;
    if(which == "arcade"){
        recordArcade(outDir + "/arcade.frames", seconds);
        return 0;
    }
    Arcade arcade;
    bool b_found = false;
    for(uint8_t i = 0; i < Arcade::gameCount; i++){
        Game& game = arcade.game(i);
        std::string name = game.name();
        if(which != "all" && strcasecmp(which.c_str(), name.c_str()) != 0) continue;
        b_found = true;
        for(auto& c : name) c = char(tolower(c));
        recordGame(game, outDir + "/game_" + name + ".frames", seconds, seed);
    }
    if(!b_found){
        fprintf(stderr, "no game called %s\n", which.c_str());
        return 2;
    }
    return 0;
}
