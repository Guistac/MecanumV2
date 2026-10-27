#pragma once
#include <stdint.h>

//A flick of a stick, for the arcade's own screens [D-157]: one stick (either) passing 0.6 along one axis from below
//0.3, the dominant axis deciding the direction. That stick re-arms once both its axes are back under 0.3. Held on,
//nothing more comes (the name entry repeats a held letter itself).
enum class Flick : uint8_t{ NONE, UP, DOWN, LEFT, RIGHT };

class StickFlick{
public:
    static constexpr float fire = 0.6f;
    static constexpr float rest = 0.3f;

    //stick: left X, left Y, right X, right Y (GameInput::stick: y positive up). The left stick first if both flick
    Flick update(const float stick[4]){
        Flick result = Flick::NONE;
        for(uint8_t s = 0; s < 2; s++){
            float x = stick[s * 2], y = stick[s * 2 + 1];
            float ax = x < 0 ? -x : x, ay = y < 0 ? -y : y;
            if(ax < rest && ay < rest){
                b_armed[s] = true;
                continue;
            }
            if(!b_armed[s] || (ax < fire && ay < fire)) continue;
            b_armed[s] = false;
            if(result != Flick::NONE) continue;
            if(ay >= ax) result = y > 0 ? Flick::UP : Flick::DOWN;
            else result = x > 0 ? Flick::RIGHT : Flick::LEFT;
        }
        return result;
    }

    //both must come back under 0.3 before the next flick (coming into a screen with a stick pushed)
    void disarm(){ b_armed[0] = b_armed[1] = false; }

    //how far a stick is pushed along y, the larger of the two (the name entry's held letter)
    static float verticalPush(const float stick[4]){
        float y = stick[1];
        if((stick[3] < 0 ? -stick[3] : stick[3]) > (y < 0 ? -y : y)) y = stick[3];
        return y;
    }

private:
    bool b_armed[2] = {false, false};
};
