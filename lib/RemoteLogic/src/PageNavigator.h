#pragma once
#include <stdint.h>

//Motion right after Status: the vehicle's velocities and pose [D-161, D-163]; Motors right after Motion: the four
//wheels [D-163]
enum class Page : uint8_t{ STATUS, MOTION, MOTORS, LINK, RADIO, BATTERY, INPUTS, ABOUT, SETTINGS, COUNT };

//The two main pages, Status and Motion: the stopwatch's controls work on them and they show its timer [D-160]
inline bool isMainPage(Page page){ return page == Page::STATUS || page == Page::MOTION; }

//The pages that stay open without input and have no page dots: the main pages and Motors [D-163] (the stopwatch's
//controls do not work on Motors: isMainPage)
inline bool staysOpen(Page page){ return isMainPage(page) || page == Page::MOTORS; }

//Remote pages: left pushbutton previous, right pushbutton next, wrapping. The settings page is in the cycle only while
//settings are available (speed switch at OFF). Any page but those that stay open (Status, Motion, Motors) returns to
//Status after 10 s without input [D-147 point 1]
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
        else if(!staysOpen(page) && nowMillis - lastActivityMillis > idleReturnMillis) page = Page::STATUS;
    }

    Page getPage() const { return page; }
    void showStatus(){ page = Page::STATUS; }

private:
    Page page = Page::STATUS;
    uint32_t lastActivityMillis = 0;
};
