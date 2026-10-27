#pragma once
#include <stdint.h>
#include "RemoteSettings.h"

//Remote settings menu, open only while the speed switch is at OFF, navigated with the sticks: up/down selects the
//item, left/right changes its value. Values stay pending until Save is held to the right for 1 s; Exit, or leaving
//OFF, discards them [D-147 point 2]. Sticks starts the joystick calibration, which saves on its own [H15]
class SettingsMenu{
public:

    enum class Item : uint8_t{ CHANNEL, CONTRAST, DIMMING, SOUND, STICKS, SAVE, EXIT, COUNT };
    enum class Event{ NONE, SAVED, CLOSED, CALIBRATE };

    static constexpr uint32_t saveHoldMillis = 1000;

    void open(const RemoteSettings& saved){
        b_open = true;
        pending = saved;
        item = Item::CHANNEL;
    }
    void close(){ b_open = false; }
    bool isOpen() const { return b_open; }

    //itemStep +1 = next item (down), valueStep +1 = right, heldRightMillis from the value axis
    Event update(int8_t itemStep, int8_t valueStep, uint32_t heldRightMillis){
        if(!b_open) return Event::NONE;

        int8_t newItem = int8_t(item) + itemStep;
        if(newItem < 0) newItem = 0;
        if(newItem >= int8_t(Item::COUNT)) newItem = int8_t(Item::COUNT) - 1;
        if(Item(newItem) != item){
            item = Item(newItem);
            return Event::NONE;
        }

        switch(item){
            case Item::CHANNEL:
                pending.channel = (pending.channel + RemoteSettings::channelCount + valueStep) % RemoteSettings::channelCount;
                break;
            case Item::CONTRAST:
                pending.contrast = stepClamped(pending.contrast, valueStep, RemoteSettings::contrastLevels);
                break;
            case Item::DIMMING:
                pending.dimming = stepClamped(pending.dimming, valueStep, RemoteSettings::dimmingOptions);
                break;
            case Item::SOUND:
                pending.sound = stepClamped(pending.sound, valueStep, RemoteSounds::levelCount);
                break;
            case Item::STICKS:
                if(valueStep > 0) return Event::CALIBRATE;
                break;
            case Item::SAVE:
                if(heldRightMillis >= saveHoldMillis){
                    b_open = false;
                    return Event::SAVED;
                }
                break;
            case Item::EXIT:
                if(valueStep > 0){
                    b_open = false;
                    return Event::CLOSED;
                }
                break;
            default: break;
        }
        return Event::NONE;
    }

    Item getItem() const { return item; }
    const RemoteSettings& getPending() const { return pending; }


    //0..1 while Save is held
    float saveProgress(uint32_t heldRightMillis) const {
        if(item != Item::SAVE) return 0.0f;
        float progress = float(heldRightMillis) / float(saveHoldMillis);
        return progress > 1.0f ? 1.0f : progress;
    }

private:

    static uint8_t stepClamped(uint8_t value, int8_t step, uint8_t count){
        int16_t stepped = int16_t(value) + step;
        if(stepped < 0) return 0;
        if(stepped >= count) return count - 1;
        return uint8_t(stepped);
    }

    bool b_open = false;
    Item item = Item::CHANNEL;
    RemoteSettings pending;
};
