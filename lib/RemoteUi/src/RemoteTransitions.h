#pragma once
#include <stdint.h>
#include <stddef.h>
#include <Adafruit_GFX.h>

//How one screen gives way to the next [D-147]: the top line (rows 0..7: the page header, or the main screen's top
//line) and the content below it (rows 8..31) can move differently. Shared by the firmware and the Mac renderer.
namespace RemoteTransitions{

    enum class Header{
        SLIDE,      //moves with the content
        ROLL,       //rolls vertically: the old one up and out, the new one up from below
        LAG,        //slides sideways but later than the content, catching up at the end
        DRAWN,      //the new frame's own header as drawn (the caller animates it in the model)
    };

    enum class Content{
        SLIDE,      //sideways, in the direction of the page change
        DISSOLVE,   //in place, pixel by pixel (ordered dither)
    };

    static const int16_t headerRows = 8;

    //progress 0..1 (already eased); forward = to the next page (the new one comes in from the right).
    //drawnFromX: header pixels from this x on are the new frame's as drawn (the page dots, gliding in the model)
    void blend(GFXcanvas1& out, GFXcanvas1& from, GFXcanvas1& to, Header header, Content content,
               float progress, bool b_forward, int16_t drawnFromX = 128);

    //where the page dots start in a page header (x of the first one), for drawnFromX
    int16_t pageDotsLeft(uint8_t pageCount);

    float easeOut(float t);

    //split-flap title: the letters flip through the alphabet and settle one after the other on the new title
    void splitFlap(char* out, size_t size, const char* from, const char* to, float progress);
};
