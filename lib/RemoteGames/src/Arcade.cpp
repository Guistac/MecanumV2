#include "Arcade.h"
#include <stdio.h>

Arcade::Arcade(){
    games[0] = &pong;
    games[1] = &parking;
    games[2] = &cave;
    games[3] = &rocks;
    games[4] = &snake;
    games[5] = &blocks;
    for(uint8_t i = 0; i < gameCount; i++){
        b_hasBest[i] = false;
        best[i] = 0;
    }
}

Game& Arcade::game(uint8_t index){ return *games[index % gameCount]; }
const Game& Arcade::game(uint8_t index) const { return *games[index % gameCount]; }

//true once when both have been held for gestureMillis; armed again only after both buttons are released
bool Arcade::updateGesture(bool b_bothHeld, bool b_anyHeld, uint32_t now){
    if(!b_anyHeld) b_gestureArmed = true;
    if(!b_bothHeld || !b_gestureArmed){
        b_gestureHeld = false;
        return false;
    }
    if(!b_gestureHeld){
        b_gestureHeld = true;
        gestureStartMillis = now;
    }
    if(now - gestureStartMillis < gestureMillis) return false;
    b_gestureHeld = false;
    b_gestureArmed = false;
    return true;
}

bool Arcade::updateGate(bool b_allowed, bool b_bothSticksHeld, uint32_t now){
    if(!b_allowed){
        //held while not allowed (driving, another page): must be released before it counts
        b_gestureHeld = false;
        if(b_bothSticksHeld) b_gestureArmed = false;
        else b_gestureArmed = true;
        return false;
    }
    return updateGesture(b_bothSticksHeld, b_bothSticksHeld, now);
}

float Arcade::gateProgress(uint32_t now) const {
    if(!b_gestureHeld) return 0.0f;
    float progress = float(now - gestureStartMillis) / float(gestureMillis);
    return progress > 1.0f ? 1.0f : progress;
}

void Arcade::enter(uint32_t now){
    b_active = true;
    b_exitRequested = false;
    mode = Mode::PICKER;
    b_paused = false;
    b_clicksBlocked = true;
    b_pendingClick[0] = b_pendingClick[1] = false;
    b_previousHeld[0] = b_previousHeld[1] = true;
    b_gestureHeld = false;
    b_gestureArmed = false;
    lastTickMillis = now;
    lastInputMillis = now;
    seedCounter += now;
    startDemo();
}

void Arcade::leave(){
    b_active = false;
    b_exitRequested = false;
    b_gestureHeld = false;
    b_gestureArmed = false;      //the buttons may still be held: they must be released before the gate counts again
}

void Arcade::startDemo(){
    seedCounter = seedCounter * 1664525u + 1013904223u;
    game(selected).reset(seedCounter);
}

void Arcade::startGame(const bool b_held[2], uint32_t now){
    seedCounter = seedCounter * 1664525u + now;
    game(selected).reset(seedCounter);
    mode = Mode::PLAYING;
    b_paused = false;
    b_pendingClick[0] = b_pendingClick[1] = false;
    b_heldFromStart[0] = b_held[0];
    b_heldFromStart[1] = b_held[1];
    lastTickMillis = now;
}

void Arcade::recordScore(){
    const Game& g = game(selected);
    uint32_t value = g.score();
    bool b_better = !b_hasBest[selected] || (g.lowerScoreIsBetter() ? value < best[selected] : value > best[selected]);
    b_newBest = b_better && (value > 0 || g.lowerScoreIsBetter());
    if(b_newBest){
        best[selected] = value;
        b_hasBest[selected] = true;
    }
}

void Arcade::tick(const GameInput& input){
    Game& g = game(selected);
    if(mode == Mode::PICKER){
        GameInput demoInput;
        g.demo(demoInput);
        g.update(demoInput);
        if(g.isOver()) startDemo();
        return;
    }
    if(mode == Mode::PLAYING && !b_paused){
        g.update(input);
        if(g.isOver()){
            mode = Mode::OVER;
            overMillis = lastTickMillis;
            recordScore();
        }
    }
}

void Arcade::update(const GameInput& sticks, bool b_previousKey, bool b_nextKey, uint32_t now){
    if(!b_active) return;

    //clicks from the held state; none until both have been released after coming in
    bool b_click[2] = {false, false};
    for(int i = 0; i < 2; i++){
        b_click[i] = sticks.held[i] && !b_previousHeld[i];
        b_previousHeld[i] = sticks.held[i];
    }
    if(b_clicksBlocked){
        if(!sticks.held[0] && !sticks.held[1]) b_clicksBlocked = false;
        b_click[0] = b_click[1] = false;
    }

    bool b_input = b_previousKey || b_nextKey || sticks.held[0] || sticks.held[1];
    for(int i = 0; i < 4; i++) if(sticks.stick[i] > 0.2f || sticks.stick[i] < -0.2f) b_input = true;
    if(b_input) lastInputMillis = now;
    if(now - lastInputMillis > idleExitMillis) b_exitRequested = true;
    if(updateGesture(sticks.held[0] && sticks.held[1], sticks.held[0] || sticks.held[1], now)) b_exitRequested = true;
    //holding both to go out never starts or replays a game
    if(sticks.held[0] && sticks.held[1]) b_click[0] = b_click[1] = false;

    switch(mode){
        case Mode::PICKER:
            if(b_previousKey || b_nextKey){
                selected = (selected + (b_nextKey ? 1 : gameCount - 1)) % gameCount;
                startDemo();
            }
            else if(b_click[0] || b_click[1]){
                startGame(sticks.held, now);
                return;
            }
            break;
        case Mode::PLAYING:
            if(b_previousKey){
                mode = Mode::PICKER;
                startDemo();
            }
            else if(b_nextKey) b_paused = !b_paused;
            break;
        case Mode::OVER:
            if(b_previousKey || b_nextKey){
                mode = Mode::PICKER;
                startDemo();
            }
            else if((b_click[0] || b_click[1]) && now - overMillis > overLockMillis){
                startGame(sticks.held, now);
                return;
            }
            break;
    }

    //clicks wait for the next tick, so none is lost between two ticks
    for(int i = 0; i < 2; i++) if(b_click[i]) b_pendingClick[i] = true;

    uint8_t ticks = 0;
    const uint32_t tickMillis = uint32_t(Game::tickSeconds * 1000.0f);
    while(now - lastTickMillis >= tickMillis){
        if(ticks == maxTicksPerUpdate){
            lastTickMillis = now;
            break;
        }
        lastTickMillis += tickMillis;
        GameInput input = sticks;
        for(int i = 0; i < 2; i++){
            if(!sticks.held[i]) b_heldFromStart[i] = false;
            if(b_heldFromStart[i]) input.held[i] = false;
        }
        input.clicked[0] = b_pendingClick[0];
        input.clicked[1] = b_pendingClick[1];
        b_pendingClick[0] = b_pendingClick[1] = false;
        tick(input);
        ticks++;
    }
}

void Arcade::draw(Adafruit_GFX& out) const {
    const Game& g = game(selected);
    out.fillScreen(GameDraw::OFF);
    g.draw(out);
    char text[24];
    char value[12];
    switch(mode){
        case Mode::PICKER:
            //the name on top, the best of the session below, arrows for the pushbuttons
            GameDraw::pillCentered(out, 64, 0, g.name());
            GameDraw::pill(out, 0, 0, "<");
            GameDraw::pill(out, 128 - GameDraw::pillWidth(">"), 0, ">");
            if(b_hasBest[selected]){
                g.scoreText(value, sizeof(value), best[selected]);
                snprintf(text, sizeof(text), "BEST %s", value);
                GameDraw::pillCentered(out, 64, 23, text);
            }
            else GameDraw::pillCentered(out, 64, 23, "CLICK TO PLAY");
            break;
        case Mode::PLAYING:
            if(b_paused) GameDraw::pillCentered(out, 64, 12, "PAUSED");
            break;
        case Mode::OVER:{
            out.fillRect(16, 2, 96, 28, GameDraw::OFF);
            GameDraw::bigTextCentered(out, 64, 15, "GAME OVER");
            g.scoreText(value, sizeof(value), g.score());
            snprintf(text, sizeof(text), b_newBest ? "%s BEST!" : "%s", value);
            GameDraw::pillCentered(out, 64, 19, text, false);
            break;
        }
    }
}
