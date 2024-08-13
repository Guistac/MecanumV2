#pragma once
#include <Arduino.h>

struct Vec2f{
    Vec2f(){}
    Vec2f(float x_, float y_) : x(x_), y(y_){}
    float x = 0.0;
    float y = 0.0;
};