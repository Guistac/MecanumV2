#pragma once
//Host tests only (env:native): the rules every remote game keeps, checked by each game's own test folder
//(test/test_game_<name>): TEST_GAME_CONTRACT(SomeGame) inside a Unity test.
#include <unity.h>
#include <chrono>
#include <Game.h>
#include "HostGfx.h"

namespace GameContract{

    inline uint32_t frameHash(const GFXcanvas1& canvas){
        const uint8_t* buffer = const_cast<GFXcanvas1&>(canvas).getBuffer();
        uint32_t hash = 2166136261u;
        for(int i = 0; i < 128 * 32 / 8; i++) hash = (hash ^ buffer[i]) * 16777619u;
        return hash;
    }

    //a tick with its drawing slower than this counts as slow (the Mac is about 5x faster than the Teensy)
    static const double slowTickMicros = 2000.0;

    //plays the demo for some seconds, returns a hash of every frame; counts the games that ended and the slow ticks
    inline uint32_t playDemo(Game& game, uint32_t seed, float seconds, int& gamesOver, int& slowTicks){
        GFXcanvas1 canvas(128, 32);
        game.reset(seed);
        uint32_t hash = 0;
        int ticks = int(seconds / Game::tickSeconds);
        for(int t = 0; t < ticks; t++){
            GameInput input;
            game.demo(input);
            auto start = std::chrono::steady_clock::now();
            game.update(input);
            canvas.fillScreen(0);
            game.draw(canvas);
            double micros = std::chrono::duration<double, std::micro>(std::chrono::steady_clock::now() - start).count();
            if(micros > slowTickMicros) slowTicks++;
            hash = hash * 31u + frameHash(canvas);
            if(game.isOver()){
                gamesOver++;
                game.reset(seed + uint32_t(gamesOver));
            }
        }
        return hash;
    }

    //1. the same seed plays the same (deterministic), another seed plays differently
    //2. the demo player loses (or finishes) within 3 minutes, so the picker's attract mode keeps changing
    //3. a tick with its drawing stays well inside the remote's loop: under 2 ms on the Mac for all but 0.5 % of the
    //   ticks (a loaded Mac stalls a single tick now and then; a slow game is slow on many)
    //4. the screen is never left blank for a whole game, the name fits the picker
    template <typename G>
    void check(){
        G a, b, c;
        int overA = 0, overB = 0, overC = 0;
        int slow = 0;
        uint32_t hashA = playDemo(a, 1234, 60.0f, overA, slow);
        uint32_t hashB = playDemo(b, 1234, 60.0f, overB, slow);
        uint32_t hashC = playDemo(c, 98765, 60.0f, overC, slow);
        TEST_ASSERT_EQUAL_UINT32_MESSAGE(hashA, hashB, "same seed, same game");
        TEST_ASSERT_TRUE_MESSAGE(hashA != hashC, "another seed, another game");

        G d;
        int over = 0;
        playDemo(d, 777, 180.0f, over, slow);
        TEST_ASSERT_TRUE_MESSAGE(over > 0, "the demo player ends a game within 3 minutes");
        //3 x 60 s and 180 s of ticks
        const int ticks = int(360.0f / Game::tickSeconds);
        TEST_ASSERT_TRUE_MESSAGE(slow * 200 < ticks, "a tick with its drawing takes less than 2 ms on the Mac (99.5 %)");

        G e;
        TEST_ASSERT_TRUE_MESSAGE(strlen(e.name()) > 0 && strlen(e.name()) <= 10, "name of 1 to 10 characters");
        e.reset(5);
        GFXcanvas1 canvas(128, 32);
        canvas.fillScreen(0);
        e.draw(canvas);
        bool b_lit = false;
        for(int y = 0; y < 32 && !b_lit; y++) for(int x = 0; x < 128; x++) if(canvas.getPixel(x, y)){ b_lit = true; break; }
        TEST_ASSERT_TRUE_MESSAGE(b_lit, "the first frame shows something");
        TEST_ASSERT_FALSE_MESSAGE(e.isOver(), "a new game is not over");
    }
};
