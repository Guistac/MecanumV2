#pragma once
#include <stdint.h>

//The Link page's history (owner, 2026-10-28): the last 60 s, one sample a second in a fixed ring (no allocation):
//the loss (% of frames without a reply in the last second) and the signal (dBm, the weaker of what the remote and
//the receiver hear; none without a link).
struct LinkHistoryView{
    static constexpr uint8_t capacity = 60;
    uint8_t count = 0;
    uint8_t loss[capacity] = {};            //oldest first, 0..100
    int8_t dbm[capacity] = {};              //oldest first; noSignal: none
    static constexpr int8_t noSignal = 0;
};

class LinkHistory{
public:
    static constexpr uint8_t capacity = LinkHistoryView::capacity;
    static constexpr uint32_t sampleMillis = 1000;

    //every refresh; a sample every second (the first at once). dbm: the weaker side, b_link false: no signal
    bool update(uint32_t now, uint8_t loss, int16_t dbm, bool b_link){
        if(count > 0 && now - lastMillis < sampleMillis) return false;
        //a late refresh keeps the 1 s grid
        lastMillis = count > 0 && now - lastMillis < 2 * sampleMillis ? lastMillis + sampleMillis : now;
        lossRing[head] = loss > 100 ? 100 : loss;
        int16_t d = dbm < -127 ? -127 : (dbm > -1 ? -1 : dbm);
        dbmRing[head] = b_link ? int8_t(d) : LinkHistoryView::noSignal;
        head = uint8_t((head + 1) % capacity);
        if(count < capacity) count++;
        return true;
    }

    uint8_t size() const { return count; }

    void view(LinkHistoryView& v) const {
        v.count = count;
        for(uint8_t i = 0; i < count; i++){
            uint8_t index = uint8_t((head + capacity - count + i) % capacity);
            v.loss[i] = lossRing[index];
            v.dbm[i] = dbmRing[index];
        }
    }

private:
    uint8_t lossRing[capacity] = {};
    int8_t dbmRing[capacity] = {};
    uint8_t head = 0;
    uint8_t count = 0;
    uint32_t lastMillis = 0;
};
