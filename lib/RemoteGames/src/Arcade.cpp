#include "Arcade.h"
#include <stdio.h>

Arcade::Arcade(){
    games[0] = &pong;
    games[1] = &parking;
    games[2] = &cave;
    games[3] = &rocks;
    games[4] = &snake;
    games[5] = &blocks;
    games[6] = &plume;
    scores.load(nullptr);
}

void Arcade::setSound(SoundSink* sink){
    output = sink;
    gameSink.setOutput(sink);
}

void Arcade::setStore(ArcadeStore* store){
    scores.load(store);
    soundMode = ArcadeSoundMode(scores.getSoundMode());
    gameSink.setMode(soundMode);
}

uint8_t Arcade::slotOf(uint8_t index) const { return ArcadeScores::slotFor(game(index).name()); }

void Arcade::cue(const SoundCue& c, SoundLayer layer){
    if(output && ArcadeSoundModes::allowsEffects(soundMode)) output->play(c, layer);
}

void Arcade::startBeat(){
    if(!output) return;
    output->setTempo(SoundLayer::BACK, 100);
    if(ArcadeSoundModes::allowsMusic(soundMode)) output->play(ArcadeSounds::pickerBeat(), SoundLayer::BACK);
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
    card = Card::NONE;
    b_modePill = false;
    b_hintPill = true;
    hintStartMillis = now;
    b_namePending = false;
    overRank = ArcadeScores::noRank;
    b_clearArmed = false;
    b_clearHeld = false;
    flicks.disarm();
    lastTickMillis = now;
    lastInputMillis = now;
    lastNow = now;
    seedCounter += now;
    startDemo();
    startBeat();
}

void Arcade::leave(){
    if(b_active){
        //idle or gone in the middle of the name entry: the entry is saved as it stands, but by saveDeferred(), after
        //the loop's frame (an EEPROM write is a flash program: never between an E-stop and the frame that carries it)
        if(b_namePending){
            b_saveDeferred = true;
            b_saveDeferredAsEdited = mode == Mode::NAME;
        }
        game(selected).setSound(nullptr);
        if(output){
            output->stop(SoundLayer::BACK);
            output->setTempo(SoundLayer::BACK, 100);
            output->setTempo(SoundLayer::FRONT, 100);
        }
        cue(ArcadeSounds::closing());
    }
    b_active = false;
    b_exitRequested = false;
    b_gestureHeld = false;
    b_gestureArmed = false;      //the buttons may still be held: they must be released before the gate counts again
}

//the picker's game plays itself, silent
void Arcade::startDemo(){
    seedCounter = seedCounter * 1664525u + 1013904223u;
    game(selected).setSound(nullptr);
    game(selected).reset(seedCounter);
}

void Arcade::startGame(const bool b_held[2], uint32_t now){
    seedCounter = seedCounter * 1664525u + now;
    Game& g = game(selected);
    g.reset(seedCounter);
    mode = Mode::PLAYING;
    card = Card::NONE;
    b_modePill = false;
    b_hintPill = false;
    b_paused = false;
    b_namePending = false;
    overRank = ArcadeScores::noRank;
    b_pendingClick[0] = b_pendingClick[1] = false;
    b_heldFromStart[0] = b_held[0];
    b_heldFromStart[1] = b_held[1];
    lastTickMillis = now;
    //the jingle in front, the game's music behind at its written tempo
    if(output){
        output->stop(SoundLayer::BACK);
        output->setTempo(SoundLayer::BACK, 100);
        output->setTempo(SoundLayer::FRONT, 100);
    }
    cue(ArcadeSounds::start());
    g.setSound(&gameSink);
    g.playMusic();
}

void Arcade::toPicker(){
    gameSink.stop(SoundLayer::BACK);
    if(mode == Mode::PLAYING) gameSink.stop(SoundLayer::FRONT);
    //a game's tempo never outlives it (the picker's blips and the remote's alarms play as written)
    if(output){
        output->setTempo(SoundLayer::BACK, 100);
        output->setTempo(SoundLayer::FRONT, 100);
    }
    cue(ArcadeSounds::back());
    mode = Mode::PICKER;
    card = Card::NONE;
    b_paused = false;
    b_clearArmed = false;
    flicks.disarm();
    startDemo();
    startBeat();
}

//the game's own crash sound goes on in front; the music stops and the arcade's phrase follows behind from 400 ms
void Arcade::gameOver(uint32_t now){
    const Game& g = game(selected);
    mode = Mode::OVER;
    overMillis = now;
    overScore = g.score();
    overRank = scores.rankFor(slotOf(selected), overScore, g.lowerScoreIsBetter());
    b_namePending = overRank != ArcadeScores::noRank;
    gameSink.stop(SoundLayer::BACK);
    if(output){
        output->setTempo(SoundLayer::BACK, 100);
        output->setTempo(SoundLayer::FRONT, 100);
    }
    cue(b_namePending ? ArcadeSounds::fanfare() : ArcadeSounds::gameOver(), SoundLayer::BACK);
}

void Arcade::openName(uint32_t now){
    mode = Mode::NAME;
    memcpy(name, scores.getLastName(), sizeof(name));
    nameCursor = 0;
    letterHold = 0;
    flicks.disarm();
    overMillis = now;
}

//into the top 3: the name as edited, or the last name (a pushbutton); back on the game over screen with the rank
void Arcade::saveDeferred(){
    if(!b_saveDeferred) return;
    b_saveDeferred = false;
    const Game& g = game(selected);
    scores.insert(slotOf(selected), overScore, b_saveDeferredAsEdited ? name : scores.getLastName(), g.lowerScoreIsBetter());
    b_namePending = false;
}

void Arcade::saveName(bool b_asEdited, uint32_t now){
    const Game& g = game(selected);
    overRank = scores.insert(slotOf(selected), overScore, b_asEdited ? name : scores.getLastName(), g.lowerScoreIsBetter());
    b_namePending = false;
    mode = Mode::OVER;
    overMillis = now;
    cue(ArcadeSounds::nameSaved());
}

void Arcade::stepLetter(int8_t direction){
    const char* characters = nameCharacters();
    const int8_t count = int8_t(strlen(characters));
    const char* at = strchr(characters, name[nameCursor]);
    int8_t index = at && *at ? int8_t(at - characters) : 0;
    index = int8_t((index + direction + count) % count);
    name[nameCursor] = characters[index];
    cue(ArcadeSounds::letterTick());
}

void Arcade::openCard(Card newCard, uint32_t now){
    card = newCard;
    cardOpenMillis = now;
    b_modePill = false;
    if(newCard != Card::CLEAR) cue(ArcadeSounds::cardTick());
}

void Arcade::closeCard(){
    if(card != Card::CLEAR) cue(ArcadeSounds::cardTick());
    card = Card::NONE;
}

//kept in EEPROM; music switched back on while a game plays starts its tune again
void Arcade::changeSoundMode(ArcadeSoundMode newMode, uint32_t now){
    bool b_musicBefore = ArcadeSoundModes::allowsMusic(soundMode);
    soundMode = newMode;
    gameSink.setMode(newMode);
    scores.setSoundMode(uint8_t(newMode));
    modeChangeMillis = now;
    b_modePill = mode == Mode::PICKER;
    b_hintPill = false;
    if(output){
        if(!ArcadeSoundModes::allowsMusic(newMode)) output->stop(SoundLayer::BACK);
        if(!ArcadeSoundModes::allowsEffects(newMode)) output->stop(SoundLayer::FRONT);
    }
    if(ArcadeSoundModes::allowsMusic(newMode) && !b_musicBefore){
        if(mode == Mode::PLAYING && !b_paused) game(selected).playMusic();
        else if(mode == Mode::PICKER) startBeat();
    }
    cue(ArcadeSounds::modeBlip());
}

void Arcade::tick(const GameInput& input, uint32_t now){
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
        if(g.isOver()) gameOver(now);
    }
}

void Arcade::updatePicker(Flick flick, const bool b_click[2], const GameInput& sticks, bool b_previousKey, bool b_nextKey,
                          bool b_bothKeysHeld, uint32_t now){
    if(b_modePill && now - modeChangeMillis >= modePillMillis) b_modePill = false;
    if(b_hintPill && now - hintStartMillis >= hintPillMillis) b_hintPill = false;

    //both pushbuttons held 3 s: the clear question (once per hold)
    if(!b_bothKeysHeld){
        b_clearHeld = false;
        if(!b_previousKey && !b_nextKey) b_clearArmed = true;
    }
    else if(b_clearArmed && card != Card::CLEAR){
        if(!b_clearHeld){
            b_clearHeld = true;
            clearHoldStartMillis = now;
        }
        else if(now - clearHoldStartMillis >= clearHoldMillis){
            b_clearHeld = false;
            b_clearArmed = false;
            openCard(Card::CLEAR, now);
            return;
        }
    }

    if(card != Card::NONE){
        bool b_click_ = b_click[0] || b_click[1];
        if(card == Card::CLEAR){
            if(b_click_){
                scores.clear(slotOf(selected));
                card = Card::NONE;
                cue(ArcadeSounds::cleared());
            }
            else if(b_previousKey || b_nextKey || flick != Flick::NONE || now - cardOpenMillis >= clearAnswerMillis){
                card = Card::NONE;
            }
            return;
        }
        //the flick back closes it (help opened right, the top 3 down); the others do nothing
        Flick back = card == Card::HELP ? Flick::LEFT : Flick::UP;
        if(b_click_ || b_previousKey || b_nextKey || flick == back || now - cardOpenMillis >= cardMillis) closeCard();
        return;
    }

    if(b_previousKey != b_nextKey){
        selected = (selected + (b_nextKey ? 1 : gameCount - 1)) % gameCount;
        b_modePill = false;
        cue(ArcadeSounds::pick(selected));
        startDemo();
        return;
    }
    switch(flick){
        case Flick::UP: changeSoundMode(ArcadeSoundModes::next(soundMode), now); return;
        case Flick::DOWN: openCard(Card::BEST, now); return;
        case Flick::RIGHT: openCard(Card::HELP, now); return;
        default: break;
    }
    if(b_click[0] || b_click[1]) startGame(sticks.held, now);
}

void Arcade::updateName(Flick flick, const bool b_click[2], const GameInput& sticks, bool b_key, uint32_t now){
    if(b_click[0] || b_click[1]){
        saveName(true, now);
        return;
    }
    if(b_key){
        saveName(false, now);
        return;
    }
    switch(flick){
        case Flick::UP:
        case Flick::DOWN:
            letterHold = flick == Flick::UP ? 1 : -1;
            nextLetterMillis = now + letterRepeatDelayMillis;
            stepLetter(letterHold);
            return;
        case Flick::LEFT:
        case Flick::RIGHT:
            nameCursor = uint8_t((nameCursor + (flick == Flick::RIGHT ? 1 : ArcadeScores::nameLength - 1)) % ArcadeScores::nameLength);
            letterHold = 0;
            cue(ArcadeSounds::cursorTick());
            return;
        default: break;
    }
    //held up or down: the letter repeats
    float push = StickFlick::verticalPush(sticks.stick);
    bool b_stillHeld = letterHold > 0 ? push >= StickFlick::fire : push <= -StickFlick::fire;
    if(letterHold == 0 || !b_stillHeld){
        letterHold = 0;
        return;
    }
    if(int32_t(now - nextLetterMillis) >= 0){
        stepLetter(letterHold);
        nextLetterMillis += letterRepeatMillis;
    }
}

void Arcade::update(const GameInput& sticks, bool b_previousKey, bool b_nextKey, uint32_t now,
                    bool b_previousKeyHeld, bool b_nextKeyHeld){
    if(!b_active) return;
    lastNow = now;

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

    bool b_input = b_previousKey || b_nextKey || b_previousKeyHeld || b_nextKeyHeld || sticks.held[0] || sticks.held[1];
    for(int i = 0; i < 4; i++) if(sticks.stick[i] > 0.2f || sticks.stick[i] < -0.2f) b_input = true;
    if(b_input) lastInputMillis = now;
    if(now - lastInputMillis > idleExitMillis) b_exitRequested = true;
    if(updateGesture(sticks.held[0] && sticks.held[1], sticks.held[0] || sticks.held[1], now)) b_exitRequested = true;
    //holding both to go out never starts or replays a game
    if(sticks.held[0] && sticks.held[1]) b_click[0] = b_click[1] = false;

    //flicks are followed everywhere (so a stick still pushed from the game does not flick on the next screen) but
    //used only on the arcade's own screens
    Flick flick = flicks.update(sticks.stick);
    bool b_anyClick = b_click[0] || b_click[1];

    switch(mode){
        case Mode::PICKER:
            updatePicker(flick, b_click, sticks, b_previousKey, b_nextKey, b_previousKeyHeld && b_nextKeyHeld, now);
            if(mode == Mode::PLAYING) return;
            break;
        case Mode::PLAYING:
            if(b_previousKey){
                toPicker();
                break;
            }
            if(b_nextKey){
                b_paused = !b_paused;
                if(b_paused){
                    gameSink.stop(SoundLayer::BACK);
                    gameSink.stop(SoundLayer::FRONT);
                    cue(ArcadeSounds::pause());
                    flicks.disarm();
                }
                else{
                    cue(ArcadeSounds::resume());
                    game(selected).playMusic();
                }
            }
            else if(b_paused && (flick == Flick::UP || flick == Flick::DOWN)){
                changeSoundMode(flick == Flick::UP ? ArcadeSoundModes::next(soundMode) : ArcadeSoundModes::previous(soundMode), now);
            }
            break;
        case Mode::OVER:
            if(b_namePending){
                //the rank shows first; the name entry follows on its own, or at once on a click or a pushbutton
                bool b_early = (b_anyClick || b_previousKey || b_nextKey) && now - overMillis > overLockMillis;
                if(b_early || now - overMillis >= nameDelayMillis) openName(now);
                break;
            }
            if(b_previousKey || b_nextKey) toPicker();
            else if(b_anyClick && now - overMillis > overLockMillis){
                startGame(sticks.held, now);
                return;
            }
            break;
        case Mode::NAME:
            updateName(flick, b_click, sticks, b_previousKey || b_nextKey, now);
            break;
    }
    if(mode == Mode::OVER || mode == Mode::NAME){
        b_pendingClick[0] = b_pendingClick[1] = false;
        lastTickMillis = now;
        return;
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
        tick(input, lastTickMillis);
        ticks++;
        if(mode != Mode::PLAYING && mode != Mode::PICKER) break;
    }
}

//rank 0..2 as 1ST, 2ND, 3RD
static const char* rankText(uint8_t rank){
    static const char* const texts[] = {"1ST", "2ND", "3RD"};
    return rank < ArcadeScores::ranks ? texts[rank] : "";
}

//a name without its trailing spaces
static void trimmedName(char* out, const char* name){
    memcpy(out, name, ArcadeScores::nameLength + 1);
    for(int8_t i = ArcadeScores::nameLength - 1; i >= 0 && out[i] == ' '; i--) out[i] = 0;
}

void Arcade::draw(Adafruit_GFX& out) const {
    const Game& g = game(selected);
    out.fillScreen(GameDraw::OFF);
    char text[32];
    switch(mode){
        case Mode::PICKER:
            if(card == Card::HELP) drawHelp(out, nullptr, nullptr, "<> GAME ^SOUND vBEST");
            else if(card == Card::BEST) drawBest(out);
            else{
                g.draw(out);
                drawPicker(out);
            }
            break;
        case Mode::PLAYING:
            if(b_paused){
                //the mode, and that a flick up or down changes it
                snprintf(text, sizeof(text), "^v %s", ArcadeSoundModes::label(soundMode));
                drawHelp(out, "PAUSED", text, nullptr);
            }
            else g.draw(out);
            break;
        case Mode::OVER:{
            g.draw(out);
            //the game's over text (its score, or more), then the rank; the dark box grows with a long pill
            char over[16];
            g.overText(over, sizeof(over), overScore);
            if(overRank != ArcadeScores::noRank) snprintf(text, sizeof(text), "%s %s", over, rankText(overRank));
            else snprintf(text, sizeof(text), "%s", over);
            int16_t boxWidth = GameDraw::pillWidth(text) + 4;
            if(boxWidth < 96) boxWidth = 96;
            if(boxWidth > 128) boxWidth = 128;
            out.fillRect(64 - boxWidth / 2, 2, boxWidth, 28, GameDraw::OFF);
            GameDraw::bigTextCentered(out, 64, 15, "GAME OVER");
            GameDraw::pillCentered(out, 64, 19, text, false);
            break;
        }
        case Mode::NAME:
            drawName(out);
            break;
    }
}

//over the game playing itself: its name on top between the pushbuttons' arrows, the record holder (or the new
//sound mode, or the clear question) in the bottom pill, the sound mode's icon in the corner
void Arcade::drawPicker(Adafruit_GFX& out) const {
    const Game& g = game(selected);
    GameDraw::pillCentered(out, 64, 0, g.name());
    GameDraw::pill(out, 0, 0, "<");
    GameDraw::pill(out, 128 - GameDraw::pillWidth(">"), 0, ">");
    char text[32];
    char value[12];
    const ArcadeScores::Entry& best = scores.entry(slotOf(selected), 0);
    if(card == Card::CLEAR) GameDraw::pillCentered(out, 64, 23, "CLEAR TOP 3?");
    else if(b_modePill) GameDraw::pillCentered(out, 64, 23, ArcadeSoundModes::pillLabel(soundMode));
    else if(b_hintPill) GameDraw::pillCentered(out, 64, 23, hintText());
    else if(best.b_used){
        char who[ArcadeScores::nameLength + 1];
        trimmedName(who, best.name);
        g.scoreText(value, sizeof(value), best.score);
        snprintf(text, sizeof(text), "%s %s", who, value);
        GameDraw::pillCentered(out, 64, 23, text);
    }
    else GameDraw::pillCentered(out, 64, 23, "CLICK TO PLAY");
    //a dark margin first, so the icon and its arrow never touch the game behind them (the widest pill, SOUND: MUSIC
    //OFF, starts at x 15, its own dark edge at 14)
    out.fillRect(0, 23, 14, 9, GameDraw::OFF);
    drawSoundIcon(out, 0, 25);
}

//four lines of the 5 x 7 font, 8 px apart: a first line (centred, or left with a right-aligned part), the game's
//help lines, and a last line
void Arcade::drawHelp(Adafruit_GFX& out, const char* firstLine, const char* firstLineRight, const char* lastLine) const {
    const char* help[3] = {nullptr, nullptr, nullptr};
    game(selected).helpLines(help);
    const char* lines[4] = {nullptr, nullptr, nullptr, nullptr};
    uint8_t count = 0;
    if(firstLine) lines[count++] = firstLine;
    for(uint8_t i = 0; i < 3 && count < 4; i++) if(help[i]) lines[count++] = help[i];
    if(lastLine){
        if(count > 3) count = 3;
        lines[count++] = lastLine;
    }
    for(uint8_t i = 0; i < count; i++){
        int16_t y = int16_t(i * 8);
        if(i == 0 && firstLine && firstLineRight){
            GameDraw::text(out, 0, y, firstLine);
            GameDraw::textRight(out, 127, y, firstLineRight);
        }
        else GameDraw::textCentered(out, 64, y, lines[i]);
    }
}

//the game's name, then its top 3: rank, name, score (empty ranks as -)
void Arcade::drawBest(Adafruit_GFX& out) const {
    const Game& g = game(selected);
    GameDraw::textCentered(out, 64, 0, g.name());
    uint8_t slot = slotOf(selected);
    char text[16];
    char value[12];
    for(uint8_t r = 0; r < ArcadeScores::ranks; r++){
        int16_t y = int16_t(8 + r * 8);
        const ArcadeScores::Entry& e = scores.entry(slot, r);
        if(!e.b_used){
            snprintf(text, sizeof(text), "%u -", unsigned(r + 1));
            GameDraw::text(out, 4, y, text);
            continue;
        }
        snprintf(text, sizeof(text), "%u %s", unsigned(r + 1), e.name);
        GameDraw::text(out, 4, y, text);
        g.scoreText(value, sizeof(value), e.score);
        GameDraw::textRight(out, 123, y, value);
    }
}

//NEW BEST! (or TOP 3), the rank and the score; the four characters, the edited one in a pill; how to use it
void Arcade::drawName(Adafruit_GFX& out) const {
    const Game& g = game(selected);
    char value[12];
    char text[40];
    g.scoreText(value, sizeof(value), overScore);
    snprintf(text, sizeof(text), "%s %s %s", overRank == 0 ? "NEW BEST!" : "TOP 3", rankText(overRank), value);
    if(GameDraw::textWidth(text) > 128) snprintf(text, sizeof(text), "%s %s", rankText(overRank), value);
    GameDraw::textCentered(out, 64, 0, text);
    const int16_t pitch = 12;
    int16_t left = 64 - (pitch * (ArcadeScores::nameLength - 1) + 5) / 2;
    for(uint8_t i = 0; i < ArcadeScores::nameLength; i++){
        int16_t x = int16_t(left + i * pitch);
        char letter[2] = {name[i] == ' ' ? '_' : name[i], 0};
        if(i == nameCursor){
            if(name[i] == ' ') letter[0] = ' ';
            GameDraw::pill(out, x - 2, 11, letter, false);
        }
        else GameDraw::text(out, x, 12, letter);
    }
    GameDraw::textCentered(out, 64, 25, "UP/DN LETTER CLICK OK");
}

//7 x 7: a speaker with its waves (SOUND ON), a note and an X (MUSIC OFF), a note (FX OFF), a speaker and an X (MUTE)
void Arcade::drawSoundIcon(Adafruit_GFX& out, int16_t x, int16_t y) const {
    //one byte a row, bit 7 the left column
    static const uint8_t icons[4][7] = {
        {0x00, 0x22, 0xEA, 0xEA, 0xEA, 0x22, 0x00},     //the speaker (box and cone), two waves
        {0x30, 0x28, 0x20, 0x2A, 0xE4, 0xEA, 0x00},     //a note and an X
        {0x18, 0x14, 0x12, 0x10, 0x70, 0xF0, 0x60},     //an eighth note
        {0x00, 0x20, 0xEA, 0xE4, 0xEA, 0x20, 0x00},     //the speaker and an X
    };
    //5 x 7, a column apart: an up arrow, the flick that changes the mode
    static const uint8_t arrow[7] = {0x20, 0x70, 0xA8, 0x20, 0x20, 0x20, 0x20};
    const uint8_t* icon = icons[uint8_t(soundMode) % 4];
    for(int16_t row = 0; row < 7; row++){
        for(int16_t column = 0; column < 7; column++){
            if(icon[row] & (0x80 >> column)) out.drawPixel(x + column, y + row, GameDraw::ON);
        }
        for(int16_t column = 0; column < 5; column++){
            if(arrow[row] & (0x80 >> column)) out.drawPixel(x + 8 + column, y + row, GameDraw::ON);
        }
    }
}
