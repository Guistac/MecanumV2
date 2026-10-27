#include "RemoteSounds.h"
#include "RobotStates.h"

namespace RemoteSounds{

    namespace{
        using W = Synth::Wave;
        const SoundNote::Kind SQ = SoundNote::Kind::SQUARE, SY = SoundNote::Kind::SYNTH, DU = SoundNote::Kind::DUET;

        //interface: short timer squares on both legs (the loudest; the synth's clicks were too weak on the disc,
        //2026-10-27), 40 ms and high so they carry; the refused buzz stays a synth chord at full level
        const SoundNote click[] = {{0, SQ, 0, W::SQUARE, 2637, 40, 0, 0}};
        const SoundNote value[] = {{0, SQ, 0, W::SQUARE, 3136, 40, 0, 0}};
        const SoundNote bump[] = {{0, SQ, 0, W::SQUARE, 330, 60, 0, 0}};
        const SoundNote saved[] = {
            {0, SQ, 0, W::SQUARE, 1568, 70, 0, 0},
            {90, SQ, 0, W::SQUARE, 2093, 140, 0, 0},
        };
        const SoundNote discarded[] = {
            {0, SQ, 0, W::SQUARE, 2093, 70, 0, 0},
            {90, SQ, 0, W::SQUARE, 1568, 140, 0, 0},
        };
        const SoundNote press[] = {{0, SQ, 0, W::SQUARE, 2093, 40, 0, 0}};
        //the toggle switches, over their own mechanical click: 80 ms, the pitch tells the position
        const SoundNote speedOff[] = {{0, SQ, 0, W::SQUARE, 659, 80, 0, 0}};
        const SoundNote speedSlow[] = {{0, SQ, 0, W::SQUARE, 1319, 80, 0, 0}};
        const SoundNote speedFast[] = {{0, SQ, 0, W::SQUARE, 2637, 80, 0, 0}};
        const SoundNote directionSet[] = {
            {0, SQ, 0, W::SQUARE, 1568, 50, 0, 0},
            {60, SQ, 0, W::SQUARE, 2093, 50, 0, 0},
        };
        const SoundNote directionClear[] = {
            {0, SQ, 0, W::SQUARE, 2093, 50, 0, 0},
            {60, SQ, 0, W::SQUARE, 1568, 50, 0, 0},
        };
        //pages: C6 D6 E6 G6 A6 C7 D7 (a pentatonic run, Status lowest)
        const SoundNote pageNotes[] = {
            {0, SQ, 0, W::SQUARE, 1047, 40, 0, 0},
            {0, SQ, 0, W::SQUARE, 1175, 40, 0, 0},
            {0, SQ, 0, W::SQUARE, 1319, 40, 0, 0},
            {0, SQ, 0, W::SQUARE, 1568, 40, 0, 0},
            {0, SQ, 0, W::SQUARE, 1760, 40, 0, 0},
            {0, SQ, 0, W::SQUARE, 2093, 40, 0, 0},
            {0, SQ, 0, W::SQUARE, 2349, 40, 0, 0},
        };

        //the arcade's way in: eight major chords climbing C5 to C6, 262 ms each, every chord arpeggiated twice
        //(root, third, fifth, 43 ms a note), the last note held: it lasts past the gesture (Arcade::gestureMillis,
        //2 s) and the start jingle cuts it when the arcade opens, so the tune never ends before the bar
        SoundNote loadingNotes[48];
        bool loadingBuilt = false;
        void buildLoading(){
            static const uint16_t roots[8] = {523, 587, 659, 698, 784, 880, 988, 1047};
            for(uint8_t chord = 0; chord < 8; chord++){
                float root = roots[chord];
                uint16_t chordNotes[3] = {uint16_t(root), uint16_t(root * 1.2599f + 0.5f), uint16_t(root * 1.4983f + 0.5f)};
                for(uint8_t i = 0; i < 6; i++){
                    SoundNote& n = loadingNotes[chord * 6 + i];
                    n = {uint16_t(chord * 262 + i * 43), SQ, 0, W::SQUARE, chordNotes[i % 3], 40, 0, 0};
                }
            }
            loadingNotes[47].millis = 600;
            loadingBuilt = true;
        }
        const SoundNote startNotes[] = {
            {0, SQ, 0, W::SQUARE, 1568, 60, 0, 0},
            {70, SQ, 0, W::SQUARE, 2093, 60, 0, 0},
            {140, SQ, 0, W::SQUARE, 2637, 60, 0, 0},
            {210, SQ, 0, W::SQUARE, 3136, 60, 0, 0},
            {290, SQ, 0, W::SQUARE, 4186, 220, 0, 0},
        };

        //in time with the NO LINK LEDs: left flash at 0, right flash at 100 ms (LedPattern.h); a falling interval, low
        //and off the interface scale, and three fading echoes of both notes, so it reads as searching, not as a click
        const SoundNote searching[] = {
            {0, SQ, 0, W::SQUARE, 988, 60, 0, 0},
            {100, SQ, 0, W::SQUARE, 740, 70, 0, 0},
            {300, SY, 0, W::SQUARE, 988, 70, 70, 50},
            {400, SY, 0, W::SQUARE, 740, 80, 70, 50},
            {600, SY, 0, W::SQUARE, 988, 70, 35, 50},
            {700, SY, 0, W::SQUARE, 740, 80, 35, 50},
            {900, SY, 0, W::SQUARE, 988, 70, 15, 50},
            {1000, SY, 0, W::SQUARE, 740, 80, 15, 50},
        };
        //two low squares a little apart: a rough buzz
        const SoundNote refused[] = {
            {0, SY, 0, W::SQUARE, 196, 180, 100, 0},
            {0, SY, 1, W::SQUARE, 208, 180, 100, 0},
        };

        //status: off the interface's C pentatonic, several notes, the last one echoed by the synth's square at 70 and
        //35 % (a triangle at 40 % was too faint to hear on the disc, 2026-10-27): it reads as a tail, not a new sound
        const SoundNote statusEnabling[] = {
            {0, SQ, 0, W::SQUARE, 740, 40, 0, 0},
            {50, SQ, 0, W::SQUARE, 988, 40, 0, 0},
        };
        const SoundNote statusEnabled[] = {
            {0, SQ, 0, W::SQUARE, 988, 60, 0, 0},
            {70, SQ, 0, W::SQUARE, 1245, 60, 0, 0},
            {140, SQ, 0, W::SQUARE, 1480, 60, 0, 0},
            {210, SQ, 0, W::SQUARE, 1976, 120, 0, 0},
            {400, SY, 0, W::SQUARE, 1976, 100, 70, 60},
            {580, SY, 0, W::SQUARE, 1976, 100, 35, 60},
        };
        const SoundNote statusDisabling[] = {
            {0, SQ, 0, W::SQUARE, 988, 40, 0, 0},
            {50, SQ, 0, W::SQUARE, 740, 40, 0, 0},
        };
        const SoundNote statusDisabled[] = {
            {0, SQ, 0, W::SQUARE, 1976, 60, 0, 0},
            {70, SQ, 0, W::SQUARE, 1480, 60, 0, 0},
            {140, SQ, 0, W::SQUARE, 988, 60, 0, 0},
            {210, SQ, 0, W::SQUARE, 494, 160, 0, 0},
            {440, SY, 0, W::SQUARE, 494, 120, 70, 80},
            {640, SY, 0, W::SQUARE, 494, 120, 35, 80},
        };
        //a noise burst, then a fast alternation of tritones (30 ms a note) stepping down: deliberately harsh, and
        //squares on both legs so it is the loudest sound the remote makes (the low beating duets were too quiet)
        SoundNote estopNotes[1 + 3 * 8];
        bool estopBuilt = false;
        void buildEstop(){
            estopNotes[0] = {0, SY, 0, W::NOISE, 6000, 60, 100, 0};
            static const uint16_t pairs[3][2] = {{1568, 1109}, {1397, 988}, {1245, 880}};
            for(uint8_t step = 0; step < 3; step++){
                for(uint8_t i = 0; i < 8; i++){
                    estopNotes[1 + step * 8 + i] = {uint16_t(70 + step * 250 + i * 30), SQ, 0, W::SQUARE, pairs[step][i % 2], 30, 0, 0};
                }
            }
            estopNotes[24].millis = 120;    //the last one rings a little
            estopBuilt = true;
        }
        const SoundNote statusEstopReleased[] = {
            {0, SQ, 0, W::SQUARE, 1397, 90, 0, 0},
            {110, SQ, 0, W::SQUARE, 1865, 160, 0, 0},
            {330, SY, 0, W::SQUARE, 1865, 120, 70, 80},
            {530, SY, 0, W::SQUARE, 1865, 120, 35, 80},
        };
        const SoundNote statusLinkFound[] = {
            {0, SQ, 0, W::SQUARE, 988, 50, 0, 0},
            {100, SQ, 0, W::SQUARE, 1480, 80, 0, 0},
            {260, SY, 0, W::SQUARE, 1480, 100, 70, 60},
            {440, SY, 0, W::SQUARE, 1480, 100, 35, 60},
        };
        //three rough beeps: the two legs a semitone apart (F6 and F sharp 6), urgent but not the E-stop's growl
        const SoundNote warning[] = {
            {0, DU, 0, W::SQUARE, 1397, 130, 0, 0, 1480},
            {220, DU, 0, W::SQUARE, 1397, 130, 0, 0, 1480},
            {440, DU, 0, W::SQUARE, 1397, 130, 0, 0, 1480},
        };

        #define NOTES(LIST) LIST, uint8_t(sizeof(LIST) / sizeof(SoundNote))
        struct Entry{ const SoundNote* list; uint8_t count; };
        const Entry table[] = {
            {nullptr, 0},           //NONE
            {NOTES(click)},
            {NOTES(value)},
            {NOTES(bump)},
            {NOTES(saved)},
            {NOTES(discarded)},
            {NOTES(press)},
            {NOTES(refused)},
            {NOTES(statusEnabling)},
            {NOTES(statusEnabled)},
            {NOTES(statusDisabling)},
            {NOTES(statusDisabled)},
            {estopNotes, uint8_t(sizeof(estopNotes) / sizeof(SoundNote))},
            {NOTES(statusEstopReleased)},
            {NOTES(statusLinkFound)},
            {NOTES(warning)},
            {NOTES(speedOff)},
            {NOTES(speedSlow)},
            {NOTES(speedFast)},
            {NOTES(directionSet)},
            {NOTES(directionClear)},
            {NOTES(searching)},
        };
        #undef NOTES
        static_assert(sizeof(table) / sizeof(Entry) == size_t(Sound::COUNT), "one entry per sound");

        //an E-stop shown: own, external, or the vehicle's emergency stop under way
        bool isEstop(BigMessage m){
            return m == BigMessage::ESTOP || m == BigMessage::EXT_ESTOP || m == BigMessage::STOPPING;
        }

        //states the receiver reported: the link was up
        bool isReported(BigMessage m){
            switch(m){
                case BigMessage::EXT_ESTOP:
                case BigMessage::STOPPING:
                case BigMessage::DISABLING:
                case BigMessage::DISABLED:
                case BigMessage::ENABLING:
                case BigMessage::ENABLED: return true;
                default: return false;
            }
        }
    }

    Sound forSpeedChange(uint8_t fromState, uint8_t toState){
        //LOW_SPEED and NO_SPEED both read as OFF (StatusView::speedText)
        auto position = [](uint8_t state){
            if(state == uint8_t(SpeedMode::MEDIUM_SPEED)) return 1;
            if(state == uint8_t(SpeedMode::HIGH_SPEED)) return 2;
            return 0;
        };
        int from = position(fromState), to = position(toState);
        if(from == to) return Sound::NONE;
        return to == 0 ? Sound::SPEED_OFF : to == 1 ? Sound::SPEED_SLOW : Sound::SPEED_FAST;
    }

    SoundNote grindGrain(float deflection, uint32_t& seed){
        if(deflection < 0.0f) deflection = 0.0f;
        if(deflection > 1.0f) deflection = 1.0f;
        seed ^= seed << 13;
        seed ^= seed >> 17;
        seed ^= seed << 5;
        float random = float(seed & 0xFFFF) / 65535.0f;
        float lowest = 200.0f + 300.0f * deflection;
        float spread = 200.0f + 900.0f * deflection;
        SoundNote n = {0, SoundNote::Kind::SQUARE, 0, Synth::Wave::SQUARE, uint16_t(lowest + random * spread), 12, 0, 0};
        return n;
    }

    SoundCue pageCue(uint8_t page){
        const uint8_t count = sizeof(pageNotes) / sizeof(SoundNote);
        SoundCue c;
        c.notes = &pageNotes[page < count ? page : count - 1];
        c.noteCount = 1;
        c.priority = SoundPriority::INTERFACE;
        return c;
    }

    SoundCue arcadeLoadingCue(){
        if(!loadingBuilt) buildLoading();
        return soundCue(loadingNotes, SoundPriority::GAME);
    }

    SoundCue arcadeStartCue(){ return soundCue(startNotes, SoundPriority::GAME); }

    const char* levelLabel(uint8_t level){
        static const char* labels[levelCount] = {"All", "Alarms", "Off"};
        return labels[level < levelCount ? level : defaultLevel];
    }

    bool isAlarm(Sound sound){
        return sound >= Sound::STATUS_ENABLING && sound <= Sound::WARNING;
    }

    bool gamesAllowed(uint8_t level){ return Level(level) != Level::OFF; }

    bool isAllowed(Sound sound, uint8_t level){
        switch(Level(level)){
            case Level::ALL: return true;
            case Level::ALARMS: return isAlarm(sound);
            default: return false;
        }
    }

    Sound forStatusChange(BigMessage from, BigMessage to){
        if(from == to || from == BigMessage::UNKNOWN) return Sound::NONE;
        if(isEstop(to) && !isEstop(from)) return Sound::STATUS_ESTOP;
        //released: the next state the receiver reports (own E-stop goes over EXT_ESTOP for a moment)
        if(isEstop(from) && !isEstop(to) && isReported(to)) return Sound::STATUS_ESTOP_RELEASED;
        if(from == BigMessage::NO_LINK && isReported(to)) return Sound::STATUS_LINK_FOUND;
        if(to == BigMessage::ENABLING) return Sound::STATUS_ENABLING;
        if(to == BigMessage::ENABLED) return Sound::STATUS_ENABLED;
        if(to == BigMessage::DISABLING) return Sound::STATUS_DISABLING;
        if(to == BigMessage::DISABLED && isReported(from)) return Sound::STATUS_DISABLED;
        if(to == BigMessage::NO_LINK && isReported(from)) return Sound::WARNING;
        if(to == BigMessage::RADIO_FAULT || to == BigMessage::ESTOP_WIRING || to == BigMessage::BATTERY_CRITICAL) return Sound::WARNING;
        return Sound::NONE;
    }

    bool refusesEnable(BigMessage message){
        switch(message){
            case BigMessage::ESTOP:
            case BigMessage::ESTOP_WIRING:
            case BigMessage::RADIO_FAULT:
            case BigMessage::NO_LINK:
            case BigMessage::BATTERY_CRITICAL:
            case BigMessage::RELEASE_LEFT:
            case BigMessage::RELEASE_RIGHT:
            case BigMessage::RELEASE_BOTH:
            case BigMessage::EXT_ESTOP: return true;
            default: return false;
        }
    }

    SoundCue cue(Sound sound){
        if(!estopBuilt) buildEstop();
        const Entry& e = table[uint8_t(sound) < uint8_t(Sound::COUNT) ? uint8_t(sound) : 0];
        SoundCue c;
        c.notes = e.list;
        c.noteCount = e.count;
        c.priority = isAlarm(sound) ? SoundPriority::ALARM : SoundPriority::INTERFACE;
        return c;
    }
}
