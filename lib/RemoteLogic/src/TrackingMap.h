#pragma once
#include <stdint.h>
#include <math.h>
#include <PageNavigator.h>

//The Tracking page's map, the logic without the drawing (host tested; RemoteScreens draws from TrackingView): a top-down
//map in the map frame with the rover fixed at the screen's centre, pointing up, the world turning round it.
//The map frame: the Motion page's zero when one is kept (PoseZero: x right of the vehicle at the zero, y its forward,
//heading counter-clockwise since), before any zero Stacato's world frame as received (x stage right, y upstage,
//heading counter-clockwise, 0 upstage). The same conventions in both: a vehicle at heading h faces (-sin h, cos h)
//and its right is (cos h, sin h). Display only: nothing of it is sent.
//  zoom: 3, 6, 12 or 24 px per metre (start 6), each zoom() steps 3 -> 6 -> 12 -> 24 -> 3, the scale gliding over
//  zoomMillis (eased, in equal ratios per moment, so the step in and the step back look alike)
//  the compass: the map frame's +y seen from the rover (the zero's forward, or upstage before a zero)
//  the steadied pose: the map is drawn at a pose that follows the real one only once it is more than deadBandPixels
//  away on the screen or deadBandDegrees turned, so standstill jitter does not flicker the grid and a slow creep
//  moves it in clean steps
//  trail: a sample of the pose every sampleMillis, the last trailCapacity (about 10 s) in a fixed ring, oldest first;
//  no pose: no sample; cleared by hand (TrackingControl: the left hold in edit mode) and by a new zero or snap (the
//  frame changed)

//what the screen needs, copied into the ScreenModel every frame (the renderer sets it directly)
struct TrackingView{
    static constexpr uint8_t trailCapacity = 50;
    float scale = 6.0f;                 //px per metre, as it glides between the zoom steps
    float compassHeading = NAN;         //deg, the rover's heading in the map frame (counter-clockwise): the compass; NaN none
    float mapX = NAN, mapY = NAN;       //the steadied pose the map is drawn at (follow), NaN: none steadied
    float mapHeading = NAN;
    uint8_t trailCount = 0;
    float trailX[trailCapacity] = {};   //map frame, m, oldest first
    float trailY[trailCapacity] = {};
};

class TrackingMap{
public:

    static constexpr uint8_t trailCapacity = TrackingView::trailCapacity;
    static constexpr uint32_t sampleMillis = 200;
    static constexpr uint8_t zoomStepCount = 4;
    static constexpr uint8_t startZoomIndex = 1;
    static constexpr uint32_t zoomMillis = 500;
    static constexpr float deadBandPixels = 0.75f;
    static constexpr float deadBandDegrees = 0.5f;
    //the rover's place on the screen (128 x 32): x 64, y 16
    static constexpr float centreX = 64.0f;
    static constexpr float centreY = 16.0f;

    static uint8_t zoomStep(uint8_t index){
        static const uint8_t steps[zoomStepCount] = {3, 6, 12, 24};
        return steps[index % zoomStepCount];
    }

    //the stick buttons on this page are TrackingControl's (the stopwatch's controls do not work there: isMainPage)
    static bool controlsPage(Page page){ return page == Page::TRACKING; }

    //the next zoom step (TrackingControl: the left click in drive mode)
    void zoom(uint32_t nowMillis = 0){
        //the glide starts from where the scale is now (a click during a glide turns it round smoothly)
        zoomFrom = scaleAt(nowMillis);
        zoomStartMillis = nowMillis;
        b_gliding = true;
        zoomIndex = uint8_t((zoomIndex + 1) % zoomStepCount);
    }

    //every refresh with the pose in the map frame (NaN: none) and whether a new zero was taken this refresh (the
    //frame changed: the trail is cleared first). A sample every sampleMillis while there is a pose
    void update(float x, float y, bool b_newZero, uint32_t nowMillis){
        if(b_newZero) clearTrail();
        if(isnan(x) || isnan(y)) return;
        if(count > 0 && nowMillis - lastSampleMillis < sampleMillis) return;
        lastSampleMillis = nowMillis;
        trailX[head] = x;
        trailY[head] = y;
        head = uint8_t((head + 1) % trailCapacity);
        if(count < trailCapacity) count++;
    }

    //every refresh with the pose in the map frame and the scale shown: the steadied pose follows (NaN: forgotten, the
    //next pose is taken at once)
    void follow(float x, float y, float heading, float scale){
        if(isnan(x) || isnan(y) || isnan(heading)){
            b_steady = false;
            return;
        }
        float dx = x - steadyX, dy = y - steadyY;
        float turned = fabsf(heading - steadyHeading);
        if(turned > 180.0f) turned = 360.0f - turned;
        if(!b_steady || sqrtf(dx * dx + dy * dy) * scale > deadBandPixels){
            steadyX = x;
            steadyY = y;
        }
        if(!b_steady || turned > deadBandDegrees) steadyHeading = heading;
        b_steady = true;
    }

    void clearTrail(){
        count = 0;
        head = 0;
    }

    uint8_t scale() const { return zoomStep(zoomIndex); }

    //the scale shown at nowMillis: the step's, or on the way there zoomMillis after the click (smoothstep, geometric)
    float scaleAt(uint32_t nowMillis) const {
        float to = float(scale());
        if(!b_gliding) return to;
        uint32_t elapsed = nowMillis - zoomStartMillis;
        if(elapsed >= zoomMillis) return to;
        float t = float(elapsed) / float(zoomMillis);
        float e = t * t * (3.0f - 2.0f * t);
        return zoomFrom * powf(to / zoomFrom, e);
    }
    uint8_t trailCount() const { return count; }

    //the i-th sample, oldest first
    void trailAt(uint8_t i, float& x, float& y) const {
        uint8_t index = uint8_t((head + trailCapacity - count + i) % trailCapacity);
        x = trailX[index];
        y = trailY[index];
    }

    //settled at the step (the renders), or as shown at nowMillis
    void view(TrackingView& v) const { view(v, zoomStartMillis + zoomMillis); }
    void view(TrackingView& v, uint32_t nowMillis) const {
        v.scale = scaleAt(nowMillis);
        v.mapX = b_steady ? steadyX : NAN;
        v.mapY = b_steady ? steadyY : NAN;
        v.mapHeading = b_steady ? steadyHeading : NAN;
        v.trailCount = count;
        for(uint8_t i = 0; i < count; i++) trailAt(i, v.trailX[i], v.trailY[i]);
    }

    //a point of the map frame seen from the rover at (roverX, roverY) with heading (deg, counter-clockwise): metres
    //to its right and ahead of it
    static void toRover(float x, float y, float roverX, float roverY, float heading, float& right, float& forward){
        float t = heading * 0.017453293f;
        float c = cosf(t), s = sinf(t);
        float dx = x - roverX, dy = y - roverY;
        right = dx * c + dy * s;
        forward = -dx * s + dy * c;
    }

    //the same on the screen: x 64 + right * scale, y 16 - forward * scale (y grows downwards)
    static void toScreen(float x, float y, float roverX, float roverY, float heading, float scale,
                         float& screenX, float& screenY){
        float right, forward;
        toRover(x, y, roverX, roverY, heading, right, forward);
        screenX = centreX + right * scale;
        screenY = centreY - forward * scale;
    }

private:
    uint8_t zoomIndex = startZoomIndex;
    bool b_gliding = false;
    float zoomFrom = 6.0f;
    uint32_t zoomStartMillis = 0;
    bool b_steady = false;
    float steadyX = 0.0f, steadyY = 0.0f, steadyHeading = 0.0f;
    float trailX[trailCapacity] = {};
    float trailY[trailCapacity] = {};
    uint8_t head = 0;               //where the next sample goes
    uint8_t count = 0;
    uint32_t lastSampleMillis = 0;
};
