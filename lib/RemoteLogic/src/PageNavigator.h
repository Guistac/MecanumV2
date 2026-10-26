#pragma once
#include <stdint.h>

enum class Page : uint8_t{ STATUS, LINK, RADIO, BATTERY, INPUTS, ABOUT, SETTINGS, COUNT };

//Remote pages: left pushbutton previous, right pushbutton next, wrapping. The settings page is in the cycle only while
//settings are available (speed switch at OFF). Any page returns to Status after 10 s without input [D-146 point 1]
class PageNavigator{
public:

    static constexpr uint32_t idleReturnMillis = 10000;

    //b_activity: any other operator input this loop (menu navigation) that keeps the page open
    void update(bool b_previous, bool b_next, bool b_settingsAvailable, bool b_activity, uint32_t nowMillis){
        uint8_t pageCount = b_settingsAvailable ? uint8_t(Page::COUNT) : uint8_t(Page::SETTINGS);
        if(page == Page::SETTINGS && !b_settingsAvailable) page = Page::STATUS;
        if(b_previous) page = Page((uint8_t(page) + pageCount - 1) % pageCount);
        if(b_next) page = Page((uint8_t(page) + 1) % pageCount);
        if(b_previous || b_next || b_activity) lastActivityMillis = nowMillis;
        else if(page != Page::STATUS && nowMillis - lastActivityMillis > idleReturnMillis) page = Page::STATUS;
    }

    Page getPage() const { return page; }
    void showStatus(){ page = Page::STATUS; }

private:
    Page page = Page::STATUS;
    uint32_t lastActivityMillis = 0;
};
