#pragma once
#include <stdint.h>
#include <math.h>

//The trip readout (the Tracking page's trip view, owner 2026-10-28): since the last zero or snap, the distance driven,
//the turn (all turning added up, either way), the top speed and the time moving. Display only. Odometry jitter at
//standstill must not add up: the distance grows only once the pose is more than stepMetres from the last point
//counted (then by the whole step), the turn once more than stepDegrees from the last heading counted. Moving: the
//body speed above movingSpeed or the turn rate above movingTurnRate; a frame's time counted up to maxFrameMillis
//(a gap in the replies adds nothing).
class TripMeter{
public:
    static constexpr float stepMetres = 0.02f;
    static constexpr float stepDegrees = 1.0f;
    static constexpr float movingSpeed = 0.02f;         //m/s
    static constexpr float movingTurnRate = 2.0f;       //deg/s
    static constexpr uint32_t maxFrameMillis = 200;

    //every refresh: the pose in the display frame (NaN: none), the body speed (m/s) and turn rate (deg/s, NaN: none)
    void update(float x, float y, float heading, float speed, float turnRate, uint32_t now){
        if(b_timed){
            uint32_t dt = now - lastMillis;
            bool b_moving = (!isnan(speed) && speed > movingSpeed) || (!isnan(turnRate) && fabsf(turnRate) > movingTurnRate);
            if(b_moving && dt <= maxFrameMillis) movingMs += dt;
        }
        b_timed = true;
        lastMillis = now;
        if(!isnan(speed) && speed > top) top = speed;
        if(!isnan(x) && !isnan(y)){
            if(!b_anchor){
                ax = x;
                ay = y;
                b_anchor = true;
            }
            float d = hypotf(x - ax, y - ay);
            if(d > stepMetres){
                metres += d;
                ax = x;
                ay = y;
            }
        }
        if(!isnan(heading)){
            if(!b_headingAnchor){
                ah = heading;
                b_headingAnchor = true;
            }
            float d = fabsf(heading - ah);
            if(d > 180.0f) d = 360.0f - d;
            if(d > stepDegrees){
                degrees += d;
                ah = heading;
            }
        }
    }

    //a new zero or snap (the frame changed: the next pose is the new start)
    void reset(){
        metres = 0.0f;
        degrees = 0.0f;
        top = 0.0f;
        movingMs = 0;
        b_anchor = false;
        b_headingAnchor = false;
    }

    float distance() const { return metres; }
    float turned() const { return degrees; }
    float topSpeed() const { return top; }
    uint32_t movingMillis() const { return movingMs; }

private:
    float metres = 0.0f, degrees = 0.0f, top = 0.0f;
    uint32_t movingMs = 0;
    bool b_anchor = false, b_headingAnchor = false, b_timed = false;
    float ax = 0.0f, ay = 0.0f, ah = 0.0f;
    uint32_t lastMillis = 0;
};
