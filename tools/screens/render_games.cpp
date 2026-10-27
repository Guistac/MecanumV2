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

//the Arcade as the operator sees it: picker, next game, play (the demo player as input), pause, game over
static void recordArcade(const std::string& path, float seconds){
    FILE* f = fopen(path.c_str(), "wb");
    GFXcanvas1 canvas(128, 32);
    Arcade arcade;
    uint32_t now = 1000;
    arcade.enter(now);
    int frames = int(seconds / Game::tickSeconds);
    for(int i = 0; i < frames; i++){
        now += 20;
        GameInput sticks;
        bool b_previous = false, b_next = false;
        if(i == 75 || i == 150) b_next = true;
        if(arcade.getMode() == Arcade::Mode::PLAYING) arcade.game(arcade.getSelected()).demo(sticks);
        if(i == 225 || i == 226) sticks.held[0] = true;     //a click: play
        arcade.update(sticks, b_previous, b_next, now);
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
