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

    //when the screen starts to clear: Becker Poels right after the fill, as it always did; BERUPO once the startup
    //sound's bell (in time with the shine, StartupScore.h) has rung out
    constexpr float fadeOutStart(Style style){ return style == Style::BERUPO ? 2.96f : fillInSeconds; }
    constexpr float duration(Style style){ return fadeOutStart(style) + fadeOutSeconds; }

    //the startup sound's arpeggio comes this much after each letter appears (as it turns solid)
    static constexpr float arpeggioDelaySeconds = 0.05f;

    //BERUPO's moments, for the startup sound: when each letter appears (0..5), when the shine starts across the word
    float letterStart(uint8_t letter);
    float shineStart();

    //the whole frame at this time since the start (0..duration)
    void draw(Adafruit_GFX& out, float seconds, Style style = Style::BERUPO);

    //the startup tune, in time with draw(): each note from its start (seconds), played as square waves on the buzzer
    struct Note{ float start; uint16_t hertz; uint16_t millis; };
    const Note* tune(Style style, uint8_t& count);

    float easeInOutCubic(float t);
};
