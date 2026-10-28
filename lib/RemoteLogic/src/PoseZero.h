#pragma once
#include <math.h>

//The Motion page's own zero for the pose [D-163]: the frame switch turned from BODY to WORLD keeps the pose of that
//moment, and from then on the page shows the movement since then in the vehicle's frame at that moment: y forward
//from the mark, x to its right, the heading turned since (counter-clockwise positive, wrapped to -180..180). Display
//only: Stacato's pose and the stick frame are untouched. Kept until the next zero or a restart; the switch already at
//WORLD at power on is not an edge. An edge while no pose is known zeroes nothing.
//The display correction (owner, 2026-10-28): the zero is one case of a correction, displayed = R(-a) * (p - o) and
//heading - a, with (o, a) the zero's pose. snap() sets it from a mark instead: "I am physically on mark k", so the
//pose now reads exactly as the mark's x, y and heading (a mark without a heading snaps the place only and keeps the
//heading shown). Snapping to Z, (0, 0, 0), is a zero. The marks are kept in the display frame and do not move on a
//snap: snapping cancels the drift since the last zero or snap. The next zero (the switch) replaces a snap.
class PoseZero{
public:

    //every refresh, with the switch and the pose as received (world frame: x stage right, y upstage, m; heading deg,
    //counter-clockwise, 0 upstage; NaN: none). True when a new zero was taken (the Tracking page clears its trail)
    bool update(bool b_world, float x, float y, float heading){
        bool b_edge = b_known && b_world && !b_wasWorld;
        b_known = true;
        b_wasWorld = b_world;
        if(!b_edge || isnan(x) || isnan(y) || isnan(heading)) return false;
        zeroX = x;
        zeroY = y;
        zeroHeading = heading;
        b_active = true;
        return true;
    }

    bool isActive() const { return b_active; }

    //the pose as received (NaN: none) is mark (mx, my) with heading mh (NaN: keep the heading shown). False, nothing
    //changed, without a pose
    bool snap(float x, float y, float heading, float mx, float my, float mh){
        if(isnan(x) || isnan(y) || isnan(heading) || isnan(mx) || isnan(my)) return false;
        float a = isnan(mh) ? (b_active ? zeroHeading : 0.0f) : wrap(heading - mh);
        float t = a * 0.017453293f;
        float c = cosf(t), s = sinf(t);
        //p = o + R(a) * m
        zeroX = x - (c * mx - s * my);
        zeroY = y - (s * mx + c * my);
        zeroHeading = a;
        b_active = true;
        return true;
    }

    //the pose relative to the zero, in place (NaN stays NaN); unchanged while no zero is kept
    void apply(float& x, float& y, float& heading) const {
        if(!b_active) return;
        float t = zeroHeading * 0.017453293f;
        float c = cosf(t), s = sinf(t);
        float dx = x - zeroX, dy = y - zeroY;
        //the vehicle at the zero faces (-sin t, cos t) in the world, its right is (cos t, sin t)
        x = dx * c + dy * s;
        y = -dx * s + dy * c;
        heading = wrap(heading - zeroHeading);
    }

    static float wrap(float degrees){
        if(isnan(degrees)) return degrees;
        degrees = fmodf(degrees + 180.0f, 360.0f);
        if(degrees < 0.0f) degrees += 360.0f;
        return degrees - 180.0f;
    }

private:
    bool b_known = false;           //the first reading only sets the switch's state
    bool b_wasWorld = false;
    bool b_active = false;
    float zeroX = 0.0f, zeroY = 0.0f, zeroHeading = 0.0f;
};
