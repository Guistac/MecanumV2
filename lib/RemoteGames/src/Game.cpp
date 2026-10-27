#include "Game.h"
#include <stdio.h>
#include <Fonts/FreeSansBold9pt7b.h>

//plays nothing and holds no state: one for every game without a sink
static SilentSink silentSink;

SoundSink& Game::sound() const { return soundSink ? *soundSink : silentSink; }

void Game::scoreText(char* out, size_t size, uint32_t value) const {
    snprintf(out, size, "%lu", (unsigned long)value);
}

namespace{

    //draws on another Adafruit_GFX a quarter turn: x (along the text) goes down the screen from y, y (the letters'
    //top to bottom) goes towards screen left from topX. Made on the stack for one string, nothing allocated
    class SidewaysTarget : public Adafruit_GFX{
    public:
        SidewaysTarget(Adafruit_GFX& target, int16_t topX, int16_t y, GameDraw::LitQuery lit, const void* context)
            : Adafruit_GFX(1024, 1024), target(target), topX(topX), startY(y), lit(lit), context(context){}
        void drawPixel(int16_t x, int16_t y, uint16_t color) override {
            int16_t screenX = int16_t(topX - y);
            int16_t screenY = int16_t(startY + x);
            if(lit) color = lit(context, screenX, screenY) ? GameDraw::OFF : GameDraw::ON;
            target.drawPixel(screenX, screenY, color);
        }
    private:
        Adafruit_GFX& target;
        int16_t topX, startY;
        GameDraw::LitQuery lit;
        const void* context;
    };

    void sideways(Adafruit_GFX& out, int16_t topX, int16_t y, const char* s, uint16_t color, GameDraw::LitQuery lit,
                  const void* context){
        SidewaysTarget target(out, topX, y, lit, context);
        GameDraw::text(target, 0, 0, s, color);
    }
};

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

    void textSideways(Adafruit_GFX& out, int16_t topX, int16_t y, const char* s, uint16_t color){
        sideways(out, topX, y, s, color, nullptr, nullptr);
    }

    void textSidewaysCentered(Adafruit_GFX& out, int16_t topX, int16_t centreY, const char* s, uint16_t color){
        sideways(out, topX, int16_t(centreY - textWidth(s) / 2), s, color, nullptr, nullptr);
    }

    void textSidewaysXor(Adafruit_GFX& out, int16_t topX, int16_t y, const char* s, LitQuery lit, const void* context){
        sideways(out, topX, y, s, ON, lit, context);
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
