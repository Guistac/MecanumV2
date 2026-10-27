#include "RemoteTransitions.h"
#include <string.h>

namespace RemoteTransitions{

    float easeOut(float t){
        if(t <= 0.0f) return 0.0f;
        if(t >= 1.0f) return 1.0f;
        float u = 1.0f - t;
        return 1.0f - u * u * u;
    }

    //two frames side by side, shifted by `shift` towards the old one's exit
    static bool slide(GFXcanvas1& from, GFXcanvas1& to, int16_t x, int16_t y, int16_t shift, bool b_forward){
        if(b_forward) return x < 128 - shift ? from.getPixel(x + shift, y) : to.getPixel(x - (128 - shift), y);
        return x >= shift ? from.getPixel(x - shift, y) : to.getPixel(x + (128 - shift), y);
    }

    //4 x 4 ordered dither: which pixels have switched to the new frame at this progress
    static bool dithered(int16_t x, int16_t y, float progress){
        static const uint8_t bayer[4][4] = {{0, 8, 2, 10}, {12, 4, 14, 6}, {3, 11, 1, 9}, {15, 7, 13, 5}};
        return (bayer[y & 3][x & 3] + 0.5f) / 16.0f < progress;
    }

    void splitFlap(char* out, size_t size, const char* from, const char* to, float progress){
        size_t fromLength = strlen(from);
        size_t toLength = strlen(to);
        size_t length = fromLength > toLength ? fromLength : toLength;
        if(length >= size) length = size - 1;
        for(size_t i = 0; i < length; i++){
            //each letter settles a little after the one before it, flipping through the alphabet until then
            float settle = 0.35f + 0.08f * i;
            char target = i < toLength ? to[i] : ' ';
            if(progress >= settle || settle >= 1.0f && progress >= 0.99f) out[i] = target;
            else if(progress < 0.05f) out[i] = i < fromLength ? from[i] : ' ';
            else out[i] = char('A' + (int(progress * 40.0f) + i * 7) % 26);
        }
        out[length] = 0;
        //trailing blanks of a shorter new title
        while(length > 0 && out[length - 1] == ' ') out[--length] = 0;
    }

    int16_t pageDotsLeft(uint8_t pageCount){ return 125 - pageCount * 4 - 1; }

    void blend(GFXcanvas1& out, GFXcanvas1& from, GFXcanvas1& to, Header header, Content content,
               float progress, bool b_forward, int16_t drawnFromX){
        int16_t shift = int16_t(progress * 128.0f + 0.5f);
        for(int16_t y = 0; y < 32; y++){
            bool b_header = y < headerRows;
            for(int16_t x = 0; x < 128; x++){
                bool pixel;
                if(b_header && x >= drawnFromX) pixel = to.getPixel(x, y);
                else if(b_header){
                    switch(header){
                        case Header::ROLL: {
                            int16_t roll = int16_t(progress * headerRows + 0.5f);
                            //only the header rows of each frame: the old one moves up, the new one follows from below
                            int16_t fromY = y + roll;
                            int16_t toY = y + roll - headerRows;
                            pixel = (fromY < headerRows && from.getPixel(x, fromY)) || (toY >= 0 && to.getPixel(x, toY));
                            break;
                        }
                        case Header::LAG: {
                            float late = progress * progress;
                            pixel = slide(from, to, x, y, int16_t(late * 128.0f + 0.5f), b_forward);
                            break;
                        }
                        case Header::DRAWN:
                            pixel = to.getPixel(x, y);
                            break;
                        default:
                            pixel = slide(from, to, x, y, shift, b_forward);
                            break;
                    }
                }
                else if(content == Content::DISSOLVE) pixel = dithered(x, y, progress) ? to.getPixel(x, y) : from.getPixel(x, y);
                else pixel = slide(from, to, x, y, shift, b_forward);
                out.drawPixel(x, y, pixel ? 1 : 0);
            }
        }
    }

};
