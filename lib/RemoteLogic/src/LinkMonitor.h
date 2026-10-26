#pragma once
#include <stdint.h>

//Remote link quality [D-146 point 3]: replies lost over the last full second, and the link bar from the weaker of
//the two RSSIs (the remote's reading of the receiver, the receiver's reading of the remote)
class LinkMonitor{
public:

    static constexpr uint32_t windowMillis = 1000;
    static constexpr int16_t barMinDbm = -100;
    static constexpr int16_t barMaxDbm = -40;

    void onSent(){ sent++; }
    void onReply(){ replies++; }

    void update(uint32_t nowMillis){
        if(nowMillis - windowStartMillis < windowMillis) return;
        if(sent == 0) loss = 100;
        else loss = replies >= sent ? 0 : uint8_t(100 - (100 * replies) / sent);
        sent = 0;
        replies = 0;
        windowStartMillis = nowMillis;
    }

    uint8_t lossPercent() const { return loss; }

    static int16_t weaker(int16_t rssiA, int16_t rssiB){ return rssiA < rssiB ? rssiA : rssiB; }

    //0..1 over -100..-40 dBm
    static float barFraction(int16_t rssiDbm){
        float fraction = float(rssiDbm - barMinDbm) / float(barMaxDbm - barMinDbm);
        if(fraction < 0.0f) return 0.0f;
        if(fraction > 1.0f) return 1.0f;
        return fraction;
    }

private:
    uint32_t windowStartMillis = 0;
    uint16_t sent = 0;
    uint16_t replies = 0;
    uint8_t loss = 100;
};
