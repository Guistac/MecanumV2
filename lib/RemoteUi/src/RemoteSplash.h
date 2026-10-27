#pragma once
#include <Adafruit_GFX.h>

//The remote's startup animation: the screen fills white from the left while the BERUPO letters fly in one after the
//other with the motion of the Game Boy Color start screen (large from below, over their place, down, sideways onto
//it, two hops); a shine crosses the word, then the screen clears from the left. Black letters on
//white. Both sticks pressed at power on: the former "Becker Poels" startup instead. Shared by the firmware and the
//Mac renderer.
namespace RemoteSplash{

    enum class Style{ BERUPO, BECKER_POELS };

    static constexpr float fillInSeconds = 0.5f;        //the white fill, left to right (as the former animation)
    static constexpr float fadeOutSeconds = 0.25f;       //cleared again from the left

    //when the screen starts to clear: Becker Poels right after the fill, as it always did
    constexpr float fadeOutStart(Style style){ return style == Style::BERUPO ? 1.85f : fillInSeconds; }
    constexpr float duration(Style style){ return fadeOutStart(style) + fadeOutSeconds; }

    //the whole frame at this time since the start (0..duration)
    void draw(Adafruit_GFX& out, float seconds, Style style = Style::BERUPO);

    float easeInOutCubic(float t);
};
