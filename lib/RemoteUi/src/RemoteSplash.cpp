#include "RemoteSplash.h"
#include "BerupoLogo.h"
#include <math.h>

namespace RemoteSplash{

    static const uint16_t ON = 1;      //lit: the white background
    static const uint16_t OFF = 0;     //dark: the letters

    //the letter motion follows the Game Boy Color start screen: each letter comes in large from below left, rises
    //past its place, drops shrinking, landing closer to the word's centre, slides sideways onto its place and hops
    //twice there. Lengths in letter heights. The landing, slide and hops are measured on the start screen recorded at
    //30 ms a frame: slide 7 frames, hops 9 and 6 frames, 0.34 and 0.1 of the letter height.
    static constexpr float firstLetterStart = 0.05f;
    static constexpr float letterStagger = 0.075f;
    static constexpr float flightSeconds = 0.45f;       //from appearing to landing
    static constexpr float slideSeconds = 0.21f;        //from landing onto its place, speeding up
    static constexpr float faintSeconds = 0.05f;        //appears dithered (half ink) first
    static constexpr float startBelow = 1.5f;           //the letter's centre below its place when it appears
    static constexpr float apexAbove = 0.9f;            //the highest point of the arc above its place
    static constexpr float startLeft = 1.5f;            //left of its landing place when it appears
    static constexpr float startScale = 3.0f;           //size when it appears, 1 at rest
    static constexpr float landingSpacing = 0.7f;       //landing distance from the word's centre, of the final one
    static constexpr float hopSeconds[2] = {0.27f, 0.18f};
    static constexpr float hopHeight[2] = {0.34f, 0.1f};
    //after the last letter has appeared (and the startup sound's last arpeggio note, arpeggioDelaySeconds later), a
    //pause, then the shine and with it the sound's bell (StartupScore.h): the pause from that note is 1.65 x what it
    //was when the shine came as the last letter's first hop ended (0.93 s; 1.5 x, then 10 % more, 2026-10-27)
    static constexpr float lastLetterStart = firstLetterStart + (BerupoLogo::letterCount - 1) * letterStagger;
    static constexpr float shineBegins = lastLetterStart + arpeggioDelaySeconds + 1.65f * (flightSeconds + slideSeconds + 0.27f);
    static constexpr float shineSeconds = 0.35f;
    static constexpr float shineWidth = 10.0f;          //px, a band leaning right (x + y / 2) that draws the letters half-tone

    float easeInOutCubic(float t){
        return t < 0.5f ? 4.0f * t * t * t : 1.0f - powf(-2.0f * t + 2.0f, 3.0f) / 2.0f;
    }

    static float clamp01(float t){
        return t < 0.0f ? 0.0f : (t > 1.0f ? 1.0f : t);
    }

    static bool bitAt(const BerupoLogo::Letter& letter, int16_t u, int16_t v){
        if(u < 0 || v < 0 || u >= letter.width || v >= letter.height) return false;
        int16_t stride = (letter.width + 7) / 8;
        return letter.bits[v * stride + u / 8] & (0x80 >> (u % 8));
    }

    //one letter, centred at (cx, cy) in screen px, scaled by sx and sy; a pixel is ink when at least half of its
    //4 x 4 samples are. shine: x + y / 2 of the shine band's left edge, the band and b_faint draw every other pixel
    static void drawLetter(Adafruit_GFX& out, const BerupoLogo::Letter& letter, float cx, float cy, float sx, float sy,
                           float shine, bool b_faint){
        const float s = float(BerupoLogo::supersample);
        float halfWidth = letter.width / s / 2.0f * sx;
        float halfHeight = letter.height / s / 2.0f * sy;
        int16_t x0 = int16_t(floorf(cx - halfWidth));
        int16_t x1 = int16_t(ceilf(cx + halfWidth));
        int16_t y0 = int16_t(floorf(cy - halfHeight));
        int16_t y1 = int16_t(ceilf(cy + halfHeight));
        if(y0 < 0) y0 = 0;
        if(y1 > 32) y1 = 32;
        if(x0 < 0) x0 = 0;
        if(x1 > 128) x1 = 128;
        for(int16_t y = y0; y < y1; y++){
            for(int16_t x = x0; x < x1; x++){
                int ink = 0;
                for(int j = 0; j < 4; j++){
                    float v = ((y + (j + 0.5f) / 4.0f - cy) / sy) * s + letter.height / 2.0f;
                    for(int i = 0; i < 4; i++){
                        float u = ((x + (i + 0.5f) / 4.0f - cx) / sx) * s + letter.width / 2.0f;
                        if(bitAt(letter, int16_t(floorf(u)), int16_t(floorf(v)))) ink++;
                    }
                }
                if(ink < 8) continue;
                float band = x + y / 2.0f - shine;
                if((b_faint || (band >= 0.0f && band < shineWidth)) && ((x + y) & 1)) continue;
                out.drawPixel(x, y, OFF);
            }
        }
    }

    //the former startup graphic, unchanged: B and P in size 3, the rest in size 2 of the built-in font
    static void drawBeckerPoels(Adafruit_GFX& out){
        out.setFont(nullptr);
        out.setTextColor(OFF);
        out.setTextSize(3);
        out.setCursor(22, -1);
        out.print("B");
        out.setTextSize(2);
        out.print("ecker");
        out.setCursor(42, 15);
        out.setTextSize(3);
        out.print("P");
        out.setTextSize(2);
        out.print("oels");
        out.setTextSize(1);
        out.setTextColor(ON);
    }

    void draw(Adafruit_GFX& out, float seconds, Style style){
        const float fadeOutStart = RemoteSplash::fadeOutStart(style);
        out.fillScreen(OFF);
        if(seconds < fadeOutStart){
            int16_t width = int16_t(easeInOutCubic(clamp01(seconds / fillInSeconds)) * 128.0f);
            out.fillRect(0, 0, width, 32, ON);
        }else{
            int16_t cleared = int16_t(easeInOutCubic(clamp01((seconds - fadeOutStart) / fadeOutSeconds)) * 128.0f);
            out.fillRect(cleared, 0, 128 - cleared, 32, ON);
        }
        if(style == Style::BECKER_POELS){
            drawBeckerPoels(out);
            return;
        }

        //the band runs from left of the word to past its right end
        float shine = -100.0f;
        if(seconds >= shineBegins && seconds < shineBegins + shineSeconds){
            shine = 0.0f + easeInOutCubic((seconds - shineBegins) / shineSeconds) * (128.0f + 16.0f + shineWidth) - 16.0f - shineWidth;
        }

        const float s = float(BerupoLogo::supersample);
        const BerupoLogo::Letter& first = BerupoLogo::letters[0];
        const BerupoLogo::Letter& last = BerupoLogo::letters[BerupoLogo::letterCount - 1];
        float wordCentre = (first.x + last.x + last.width) / s / 2.0f;
        for(int16_t i = 0; i < BerupoLogo::letterCount; i++){
            const BerupoLogo::Letter& letter = BerupoLogo::letters[i];
            float age = seconds - firstLetterStart - i * letterStagger;
            if(age <= 0.0f) continue;
            float height = letter.height / s;
            float restX = (letter.x + letter.width / 2.0f) / s;
            float restY = (letter.y + letter.height / 2.0f) / s;
            float landingX = wordCentre + (restX - wordCentre) * landingSpacing;

            float cx, cy, scale = 1.0f;
            if(age < flightSeconds){
                //fast at first, then a parabola in q: from below through the apex (at q = 0.6) down onto the place
                float p = age / flightSeconds;
                float q = powf(p, 0.6f);
                //f(q) = startBelow (1 - q) - k q (1 - q), with k so that the lowest value (the apex) is -apexAbove
                float k = startBelow + 2.0f * apexAbove + 2.0f * sqrtf(apexAbove * (startBelow + apexAbove));
                cy = restY + height * (startBelow * (1.0f - q) - k * q * (1.0f - q));
                cx = landingX - height * startLeft * (1.0f - q);
                scale = 1.0f + (startScale - 1.0f) * powf(1.0f - p, 2.0f);
            }else if(age < flightSeconds + slideSeconds){
                float p = (age - flightSeconds) / slideSeconds;
                cx = landingX + (restX - landingX) * powf(p, 1.3f);
                cy = restY;
            }else{
                //in place: two hops, each back at the rest position when it ends, then still
                cx = restX;
                cy = restY;
                float hopAge = age - flightSeconds - slideSeconds;
                for(int hop = 0; hop < 2; hop++){
                    if(hopAge < hopSeconds[hop]){
                        //clamped: a hop age a rounding below 0 would make powf return NaN and hide the letter
                        cy = restY - height * hopHeight[hop] * powf(sinf(clamp01(hopAge / hopSeconds[hop]) * 3.14159265f), 1.4f);
                        break;
                    }
                    hopAge -= hopSeconds[hop];
                }
            }
            drawLetter(out, letter, cx, cy, scale, scale, shine, age < faintSeconds);
        }
    }

    float letterStart(uint8_t letter){ return firstLetterStart + letter * letterStagger; }
    float shineStart(){ return shineBegins; }

    //Becker Poels: a rising arpeggio across the fill (BERUPO plays the startup clip instead, RemoteGui)
    static const Note beckerPoelsTune[] = {
        {0.0f, 262, 90},
        {0.12f, 330, 90},
        {0.24f, 392, 90},
        {0.36f, 523, 250},
    };

    const Note* tune(Style style, uint8_t& count){
        if(style == Style::BECKER_POELS){
            count = sizeof(beckerPoelsTune) / sizeof(Note);
            return beckerPoelsTune;
        }
        count = 0;
        return nullptr;
    }

};
