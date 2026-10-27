#include "Game.h"
#include <stdio.h>
#include <Fonts/FreeSansBold9pt7b.h>

void Game::scoreText(char* out, size_t size, uint32_t value) const {
    snprintf(out, size, "%lu", (unsigned long)value);
}

namespace GameDraw{

    void text(Adafruit_GFX& out, int16_t x, int16_t y, const char* s, uint16_t color){
        out.setFont(nullptr);
        out.setTextSize(1);
        out.setTextWrap(false);
        out.setTextColor(color);
        out.setCursor(x, y);
        out.print(s);
    }

    int16_t textWidth(const char* s){
        size_t length = strlen(s);
        return length ? int16_t(length * 6 - 1) : 0;    //5 px glyphs, 1 px between them
    }

    void textCentered(Adafruit_GFX& out, int16_t centreX, int16_t y, const char* s, uint16_t color){
        text(out, centreX - textWidth(s) / 2, y, s, color);
    }

    void textRight(Adafruit_GFX& out, int16_t rightX, int16_t y, const char* s, uint16_t color){
        text(out, rightX - textWidth(s) + 1, y, s, color);
    }

    int16_t pillWidth(const char* label){ return textWidth(label) + 4; }

    int16_t pill(Adafruit_GFX& out, int16_t x, int16_t y, const char* label, bool b_clear){
        int16_t w = pillWidth(label);
        if(b_clear) out.fillRect(x - 1, y - 1, w + 2, pillHeight + 2, OFF);
        out.fillRoundRect(x, y, w, pillHeight, 2, ON);
        text(out, x + 2, y + 1, label, OFF);
        return w;
    }

    void pillCentered(Adafruit_GFX& out, int16_t centreX, int16_t y, const char* label, bool b_clear){
        pill(out, centreX - pillWidth(label) / 2, y, label, b_clear);
    }

    void bigTextCentered(Adafruit_GFX& out, int16_t centreX, int16_t baselineY, const char* s, uint16_t color){
        out.setFont(&FreeSansBold9pt7b);
        out.setTextSize(1);
        out.setTextWrap(false);
        int16_t x1, y1;
        uint16_t w, h;
        out.getTextBounds(s, 0, baselineY, &x1, &y1, &w, &h);
        out.setTextColor(color);
        out.setCursor(centreX - int16_t(w) / 2 - x1, baselineY);
        out.print(s);
        out.setFont(nullptr);
    }
};
