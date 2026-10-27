#include "RemoteScreens.h"
#include <Fonts/FreeSansBold9pt7b.h>

//Remote screens [D-146]: 128 x 32, three rows. Top (y 0..7): link and batteries. Middle (y 8..22): the one thing that
//matters now. Bottom (y 23..31): the speed switch on the left and the frame switch on the right, each above its switch,
//page dots between them. The active page dot is also the alive sign: a gap travels round it with every few frames.

namespace RemoteUi{

    static const uint16_t ON = 1;
    static const uint16_t OFF = 0;

    //the bold proportional font only for the state and the alarm words; everything else in the 5 x 7 pixel font
    static const GFXfont* bigFont = &FreeSansBold9pt7b;
    static const GFXfont* pixelFont = nullptr;

    //Rules (owner): negative text sits in a filled pill or bar with 1 px of fill all round, also against the screen
    //edge; nothing touches (1 px gap); no outline-only shapes; text lines every 8 px (the 5 x 7 font's descenders use
    //an 8th row). Main screen rows: top line 0..6, middle 8..22 (13 px capitals with 1 px around), bottom 23..31
    //(9 px pills); a full-width alarm banner leaves no gap above the pills, so the bottom row is then plain text.
    static const int16_t middleTop = 8;
    static const int16_t middleHeight = 15;
    static const int16_t bottomTop = 23;
    static const int16_t pillHeight = 9;

    //——— helpers ———

    static int16_t textWidth(Adafruit_GFX& g, const GFXfont* font, const char* s){
        g.setFont(font);
        g.setTextSize(1);
        int16_t x1, y1;
        uint16_t w, h;
        g.getTextBounds(s, 0, 20, &x1, &y1, &w, &h);
        if(!font) return int16_t(strlen(s) * 6 - 1);    //5 px glyphs, 1 px between them
        return int16_t(w + x1);
    }

    //font: baseline at y; the pixel font: top at y
    static void text(Adafruit_GFX& g, const GFXfont* font, int16_t x, int16_t y, const char* s, uint16_t color = ON){
        g.setFont(font);
        g.setTextSize(1);
        g.setTextColor(color);
        g.setCursor(x, y);
        g.print(s);
    }

    static void textCentered(Adafruit_GFX& g, const GFXfont* font, int16_t centreX, int16_t y, const char* s, uint16_t color = ON){
        text(g, font, centreX - textWidth(g, font, s) / 2, y, s, color);
    }

    static void textRight(Adafruit_GFX& g, const GFXfont* font, int16_t rightX, int16_t y, const char* s, uint16_t color = ON){
        text(g, font, rightX - textWidth(g, font, s) + 1, y, s, color);
    }

    //a filled pill, 9 px high, the text with 1 px above and below and 2 px at the sides; returns its width
    static int16_t pillWidth(Adafruit_GFX& g, const char* label){ return textWidth(g, pixelFont, label) + 4; }

    static int16_t pill(Adafruit_GFX& g, int16_t x, int16_t y, const char* label){
        int16_t w = pillWidth(g, label);
        g.fillRoundRect(x, y, w, pillHeight, 2, ON);
        text(g, pixelFont, x + 2, y + 1, label, OFF);
        return w;
    }

    static int16_t pillRight(Adafruit_GFX& g, int16_t rightX, int16_t y, const char* label){
        int16_t w = pillWidth(g, label);
        return pill(g, rightX - w + 1, y, label);
    }

    static bool blink(uint32_t now, uint32_t period, uint32_t onTime){ return now % period < onTime; }

    //——— icons ———

    //a 16 px wedge rising from 1 to 7 px, filled column by column with the signal (16 steps); the rest a dotted
    //baseline. No link: a blinking cross. One pixel right of it flips with every reply received.
    static const int16_t signalWidth = 16;

    static void signalIcon(Adafruit_GFX& g, int16_t x, int16_t y, const ScreenModel& m){
        int filled = 0;
        if(m.b_linkUp){
            filled = int(m.signal * signalWidth + 0.5f);
            if(filled < 1) filled = 1;
            if(filled > signalWidth) filled = signalWidth;
        }
        for(int i = 0; i < signalWidth; i++){
            int16_t height = 1 + (i * 6 + 7) / 15;
            if(i < filled) g.drawFastVLine(x + i, y + 7 - height, height, ON);
            else if(i % 2 == 0) g.drawPixel(x + i, y + 6, ON);
        }
        if(!m.b_linkUp && blink(m.now, 1000, 600)){
            g.drawLine(x + 6, y, x + 10, y + 4, ON);
            g.drawLine(x + 10, y, x + 6, y + 4, ON);
        }
        if(m.b_linkUp && (m.rxCounter & 1)) g.drawPixel(x + signalWidth + 1, y, ON);
    }

    //the alive sign: a one pixel gap travelling round a dot's edge every few frames (the screen is still being drawn).
    //One per screen: the state dot on the main screen, the active page dot on the pages.
    static int aliveStep(const ScreenModel& m){ return (m.frame / 3) % 8; }

    //the active page dot, 3 x 3, with or without the travelling gap
    static void activeDot(Adafruit_GFX& g, int16_t x, int16_t y, const ScreenModel& m, bool b_alive){
        static const int8_t ring[8][2] = {{0, 0}, {1, 0}, {2, 0}, {2, 1}, {2, 2}, {1, 2}, {0, 2}, {0, 1}};
        g.fillRect(x, y, 3, 3, ON);
        if(b_alive) g.drawPixel(x + ring[aliveStep(m)][0], y + ring[aliveStep(m)][1], OFF);
    }

    //the gap on the rim of a radius 3 circle (the state dot)
    static void circleGap(Adafruit_GFX& g, int16_t cx, int16_t cy, const ScreenModel& m){
        static const int8_t rim[8][2] = {{0, -3}, {2, -2}, {3, 0}, {2, 2}, {0, 3}, {-2, 2}, {-3, 0}, {-2, -2}};
        g.drawPixel(cx + rim[aliveStep(m)][0], cy + rim[aliveStep(m)][1], OFF);
    }

    static void batteryIcon(Adafruit_GFX& g, int16_t x, int16_t y, uint8_t percent){
        g.drawRect(x, y, 11, 7, ON);
        g.drawFastVLine(x + 11, y + 2, 3, ON);
        int16_t fill = (int16_t(percent) * 9 + 50) / 100;
        if(fill > 0) g.fillRect(x + 1, y + 1, fill, 5, ON);
    }

    //a triangle with an exclamation mark (9 x 8)
    static const uint8_t warningBitmap[] = {
        0x08, 0x00, 0x1C, 0x00, 0x14, 0x00, 0x36, 0x00, 0x77, 0x00, 0x7F, 0x00, 0xF7, 0x80, 0xFF, 0x80
    };

    //the E-stop: a circle with a bar (9 x 9)
    static void estopIcon(Adafruit_GFX& g, int16_t x, int16_t y, uint16_t color){
        g.drawCircle(x + 4, y + 4, 4, color);
        g.drawCircle(x + 4, y + 4, 3, color);
        g.drawLine(x + 2, y + 2, x + 6, y + 6, color);
    }

    //7 x 7 icons for the page titles
    static const uint8_t stickBitmap[] = { 0x38, 0x38, 0x10, 0x10, 0x7C, 0xFE, 0xFE };
    static const uint8_t gearBitmap[] = { 0x28, 0x7C, 0xC6, 0x44, 0xC6, 0x7C, 0x28 };
    static const uint8_t antennaBitmap[] = { 0x44, 0x92, 0xBA, 0x54, 0x10, 0x10, 0x38 };
    static const uint8_t chipBitmap[] = { 0x54, 0xFE, 0x44, 0xC6, 0x44, 0xFE, 0x54 };
    static const uint8_t batteryBitmap[] = { 0x38, 0x7C, 0x44, 0x7C, 0x44, 0x7C, 0x7C };
    static const uint8_t infoBitmap[] = { 0x7C, 0xEE, 0xFE, 0xEE, 0xEE, 0xEE, 0x7C };

    //——— the status screen ———

    static void drawTopRow(Adafruit_GFX& g, const ScreenModel& m){
        signalIcon(g, 0, 0, m);
        char channel[8];
        if(m.b_direct) snprintf(channel, sizeof(channel), m.b_dropout ? "DROP" : "USB");
        else snprintf(channel, sizeof(channel), "CH%X", m.channel);
        text(g, pixelFont, signalWidth + 4, 0, channel);

        //the vehicle's pack, then the remote's own battery (blinking when low)
        char voltage[8];
        if(m.b_linkUp) snprintf(voltage, sizeof(voltage), "%.1fV", m.robotVoltage);
        else snprintf(voltage, sizeof(voltage), "--.-V");
        textRight(g, pixelFont, 111, 0, voltage);
        if(m.remoteBattery == BatteryState::OK || blink(m.now, 1000, 600)) batteryIcon(g, 114, 0, m.remotePercent);
    }

    static const char* stateText(BigMessage message){
        switch(message){
            case BigMessage::ENABLED: return "Enabled";
            case BigMessage::ENABLING: return "Enabling";
            case BigMessage::DISABLED: return "Disabled";
            case BigMessage::DISABLING: return "Disabling";
            default: return "Unknown";
        }
    }

    static const char* alarmText(BigMessage message){
        switch(message){
            case BigMessage::ESTOP:
            case BigMessage::EXT_ESTOP: return "E-STOP";
            case BigMessage::ESTOP_WIRING: return "WIRING";
            case BigMessage::RADIO_FAULT: return "RADIO";
            case BigMessage::NO_LINK: return "NO LINK";
            case BigMessage::BATTERY_CRITICAL: return "BATTERY";
            case BigMessage::STOPPING: return "STOPPING";
            default: return "";
        }
    }

    //what a one-word alarm means, under it
    static const char* alarmNote(const ScreenModel& m, char* buffer, size_t size){
        switch(m.status.line){
            case StatusLine::NOT_CONNECTED: return "e-stop not sent";
            case StatusLine::ESTOP_NOT_CONNECTED: return nullptr;   //owner: NO LINK says it all
            case StatusLine::RADIO_RETRY: snprintf(buffer, size, "radio fault, retry %u", m.radioFailures); return buffer;
            case StatusLine::RESTART_REMOTE: return "restart the remote";
            default: break;
        }
        switch(m.status.message){
            case BigMessage::ESTOP_WIRING: return "e-stop contact fault";
            case BigMessage::EXT_ESTOP: return "not from this remote";
            case BigMessage::BATTERY_CRITICAL: return "charge the remote";
            case BigMessage::STOPPING: return "e-stop ramp";
            case BigMessage::ESTOP: return "on this remote";
            default: return nullptr;
        }
    }

    static void drawMiddle(Adafruit_GFX& g, const ScreenModel& m){
        BigMessage message = m.status.message;

        //sticks to release: the stick icon and a sentence
        if(message == BigMessage::RELEASE_LEFT || message == BigMessage::RELEASE_RIGHT || message == BigMessage::RELEASE_BOTH){
            const char* prompt = message == BigMessage::RELEASE_LEFT ? "Release left stick" :
                                 (message == BigMessage::RELEASE_RIGHT ? "Release right stick" : "Release both sticks");
            int16_t w = textWidth(g, pixelFont, prompt) + 10;
            int16_t x = (127 - w) / 2;
            g.drawBitmap(x, 12, stickBitmap, 7, 7, ON);
            text(g, pixelFont, x + 10, 12, prompt);
            return;
        }

        //alarms: a filled banner with negative text; the own E-stop and the ramp pulse between banner and plain text
        if(Status::isAlarm(message)){
            bool b_filled = true;
            if(message == BigMessage::ESTOP) b_filled = blink(m.now, 800, 500);
            if(message == BigMessage::STOPPING) b_filled = blink(m.now, 300, 150);
            uint16_t ink = b_filled ? OFF : ON;
            if(b_filled) g.fillRoundRect(0, middleTop, 127, middleHeight, 3, ON);
            bool b_estop = message == BigMessage::ESTOP || message == BigMessage::EXT_ESTOP
                        || message == BigMessage::ESTOP_WIRING || message == BigMessage::STOPPING;
            if(b_estop) estopIcon(g, 4, middleTop + 3, ink);
            else g.drawBitmap(4, middleTop + 4, warningBitmap, 9, 8, ink);
            textCentered(g, bigFont, 70, middleTop + 13, alarmText(message), ink);
            return;
        }

        //a normal state: a dot (filled when enabled, pulsing while it changes) and the word
        const char* word = stateText(message);
        int16_t wordWidth = textWidth(g, bigFont, word);
        int16_t x = (127 - wordWidth - 10) / 2;
        //a descender (Enabling, Disabling) reaches the bottom row: keep it left of the frame indicator
        if(strchr(word, 'g')){
            const char* frame = m.b_worldFrame ? "WORLD" : "BODY";
            int16_t frameLeft = 125 - textWidth(g, pixelFont, frame) - (m.b_worldFrame ? 3 : 1);
            int16_t limit = frameLeft - 2 - wordWidth - 10;
            if(x > limit) x = limit;
            if(x < 0) x = 0;
        }
        int16_t cy = middleTop + 7;
        switch(message){
            case BigMessage::ENABLED: g.fillCircle(x + 3, cy, 3, ON); break;
            case BigMessage::DISABLED: g.drawCircle(x + 3, cy, 3, ON); break;
            case BigMessage::ENABLING:
            case BigMessage::DISABLING:
                g.drawCircle(x + 3, cy, 3, ON);
                if(blink(m.now, 400, 200)) g.fillCircle(x + 3, cy, 1, ON);
                break;
            default:
                g.drawCircle(x + 3, cy, 3, ON);
                break;
        }
        circleGap(g, x + 3, cy, m);
        text(g, bigFont, x + 10, middleTop + 13, word);
    }

    static void drawBottomRow(Adafruit_GFX& g, const ScreenModel& m){
        //during an alarm the row explains it (no room for a gap between the banner and pills): the switches matter less
        //than why nothing moves
        char buffer[24];
        const char* note = Status::isAlarm(m.status.message) ? alarmNote(m, buffer, sizeof(buffer)) : nullptr;
        if(Status::isAlarm(m.status.message)){
            if(note){
                if(m.status.line == StatusLine::RESTART_REMOTE && !blink(m.now, 1000, 700)) return;
                textCentered(g, pixelFont, 63, 24, note);
                return;
            }
            //no explanation needed (NO LINK): the switches as plain text, pills would touch the banner
            const char* speed = Status::speedText(m.speedState);
            text(g, pixelFont, 2, 24, speed);
            textRight(g, pixelFont, 123, 24, m.b_worldFrame ? "WORLD" : "BODY");
            return;
        }

        //speed switch, above it on the left: OFF as plain text (sticks do nothing), SLOW and FAST in a pill
        SpeedMode speed = SpeedMode(m.speedState);
        if(speed == SpeedMode::MEDIUM_SPEED) pill(g, 0, bottomTop, "SLOW");
        else if(speed == SpeedMode::HIGH_SPEED) pill(g, 0, bottomTop, "FAST");
        else text(g, pixelFont, 2, bottomTop + 1, "OFF");

        //frame switch, above it on the right: BODY (the default) plain, WORLD in a pill
        if(m.b_worldFrame) pillRight(g, 125, bottomTop, "WORLD");
        else textRight(g, pixelFont, 123, bottomTop + 1, "BODY");

        //between them: thank you, a marker (one at a time, taking turns every second), or which page this is
        const char* markers[4];
        int count = 0;
        if(m.b_rebooted) markers[count++] = "REBOOT";
        if(m.b_recovery) markers[count++] = "RST";
        if(m.b_hot) markers[count++] = "HOT";
        if(m.b_safeMode) markers[count++] = "SAFE";
        if(m.status.line == StatusLine::THANK_YOU) textCentered(g, pixelFont, 63, bottomTop + 1, "thank you");
        else if(count > 0){
            const char* marker = markers[(m.now / 1000) % count];
            pill(g, 63 - pillWidth(g, marker) / 2, bottomTop, marker);
        }
        else{
            int16_t x = 63 - (m.pageCount * 4) / 2;
            for(int i = 0; i < m.pageCount; i++){
                //the state dot above carries the alive sign, except while sticks are to be released (no state dot)
                bool b_stateDot = m.status.message != BigMessage::RELEASE_LEFT && m.status.message != BigMessage::RELEASE_RIGHT
                               && m.status.message != BigMessage::RELEASE_BOTH;
                if(i == int(m.page)) activeDot(g, x + i * 4, bottomTop + 3, m, !b_stateDot);
                else g.drawPixel(x + i * 4 + 1, bottomTop + 4, ON);
            }
        }
    }

    void drawStatus(Adafruit_GFX& g, const ScreenModel& m){
        drawTopRow(g, m);
        drawMiddle(g, m);
        drawBottomRow(g, m);
    }

    //——— info pages ———

    //the title line: icon and title, page dots on the right (plain: a filled bar would need 9 rows and leave 2 lines)
    const char* pageTitle(Page page){
        switch(page){
            case Page::LINK: return "Link";
            case Page::RADIO: return "Radio";
            case Page::BATTERY: return "Battery";
            case Page::INPUTS: return "Inputs";
            case Page::ABOUT: return "About";
            case Page::SETTINGS: return "Settings";
            default: return "";
        }
    }

    static void drawPageHeader(Adafruit_GFX& g, const ScreenModel& m, const uint8_t* icon){
        g.drawBitmap(0, 0, icon, 7, 7, ON);
        text(g, pixelFont, 10, 0, m.titleOverride ? m.titleOverride : pageTitle(m.page));
        int16_t x = 125 - m.pageCount * 4;
        float active = m.pageDotPosition >= 0.0f ? m.pageDotPosition : float(m.page);
        for(int i = 0; i < m.pageCount; i++) g.drawPixel(x + i * 4 + 1, 3, ON);
        activeDot(g, x + int16_t(active * 4.0f + 0.5f), 2, m, true);
    }

    //lines 1..3 under the title: label left, value right
    static void row(Adafruit_GFX& g, int index, const char* label, const char* value){
        int16_t top = 8 + index * 8;
        text(g, pixelFont, 0, top, label);
        textRight(g, pixelFont, 125, top, value);
    }

    static void drawLinkPage(Adafruit_GFX& g, const ScreenModel& m){
        drawPageHeader(g, m, antennaBitmap);
        char value[24];
        //how strongly each side hears the other (the radio keeps its last reading: only meaningful while replies arrive)
        if(m.b_linkUp) snprintf(value, sizeof(value), "%ddBm", m.downDbm);
        else snprintf(value, sizeof(value), "--");
        row(g, 0, "remote hears", value);
        if(m.b_linkUp) snprintf(value, sizeof(value), "%ddBm", m.upDbm);
        else snprintf(value, sizeof(value), "--");
        row(g, 1, "receiver hears", value);
        //frames without a reply in the last second, and how long a reply takes
        if(m.b_linkUp){
            snprintf(value, sizeof(value), "lost %u%%  delay %.0fms", m.loss, m.rttMs);
            text(g, pixelFont, 0, 24, value);
        }
        else row(g, 2, "lost", "--");
    }

    static void drawRadioPage(Adafruit_GFX& g, const ScreenModel& m){
        drawPageHeader(g, m, chipBitmap);
        char value[24];
        if(m.b_radioUp) snprintf(value, sizeof(value), "%d C", m.temperature);
        else snprintf(value, sizeof(value), "fault #%u", m.radioFailures);
        row(g, 0, m.b_radioUp ? "temperature" : "state", value);
        snprintf(value, sizeof(value), "%u", m.recoveries);
        row(g, 1, "recoveries", value);
        snprintf(value, sizeof(value), "%u / %u", m.txTimeouts, m.chipResets);
        row(g, 2, "tx t/o, resets", value);
    }

    static void drawBatteryPage(Adafruit_GFX& g, const ScreenModel& m){
        drawPageHeader(g, m, batteryBitmap);
        char value[24];
        //the charge (from the resting voltage, which rides through transmit sag) and its state, then the cell right now
        const char* state = m.remoteBattery == BatteryState::OK ? "ok" : (m.remoteBattery == BatteryState::LOW_CHARGE ? "low" : "empty");
        snprintf(value, sizeof(value), "%u%% %s", m.remotePercent, state);
        row(g, 0, "remote", value);
        snprintf(value, sizeof(value), "%.2fV", m.remoteVoltage);
        row(g, 1, "  cell", value);
        if(m.b_linkUp) snprintf(value, sizeof(value), "%.1fV", m.robotVoltage);
        else snprintf(value, sizeof(value), "--");
        row(g, 2, "vehicle", value);
    }

    //the original pixel input test: sticks as bars, the nine digital inputs as pixels up or down
    static void inputPixels(Adafruit_GFX& g, const ScreenModel& m, int16_t x, int16_t y, uint16_t ink){
        uint16_t paper = ink == ON ? OFF : ON;
        auto analog = [&](float value, int16_t ax, int16_t ay, int16_t w, bool flip){
            int16_t xPos = int16_t(roundf(ax + (value + 1.0f) * w / 2.0f));
            int16_t yL = flip ? ay : ay + 1;
            int16_t yD = flip ? ay + 1 : ay;
            g.drawLine(ax, yL, ax + w, yL, ink);
            g.drawPixel(xPos, yD, ink);
            if(value == 0.0f || value == -1.0f || value == 1.0f) g.drawPixel(xPos, yL, paper);
        };
        int16_t dx = x + 12;
        int16_t dy = y + 2;
        g.drawPixel(dx + 0, dy + (m.b_stickPressed[0] ? 1 : 0), ink);
        g.drawPixel(dx + 1, dy + (m.b_pushbutton[0] ? 1 : 0), ink);
        g.drawPixel(dx + 2, dy + (m.b_disableButton ? 1 : 0), ink);
        switch(m.speedState){
            case 0: g.drawPixel(dx + 3, dy + 1, ink); break;
            case 1: g.drawPixel(dx + 3, dy, ink); break;
            case 2: g.drawPixel(dx + 3, dy - 1, ink); break;
        }
        g.drawPixel(dx + 4, dy + (m.estop == EstopState::CLEAR ? 0 : 1), ink);
        g.drawPixel(dx + 5, dy + (m.b_worldFrame ? 0 : 1), ink);
        g.drawPixel(dx + 6, dy + (m.b_enableButton ? 1 : 0), ink);
        g.drawPixel(dx + 7, dy + (m.b_pushbutton[1] ? 1 : 0), ink);
        g.drawPixel(dx + 8, dy + (m.b_stickPressed[1] ? 1 : 0), ink);
        analog(m.sticks[0], x, y, 10, false);
        analog(m.sticks[1], x, y + 3, 10, true);
        analog(m.sticks[2], x + 22, y, 10, false);
        analog(m.sticks[3], x + 22, y + 3, 10, true);
    }

    static void drawInputsPage(Adafruit_GFX& g, const ScreenModel& m){
        drawPageHeader(g, m, stickBitmap);
        inputPixels(g, m, 52, 0, ON);
        char value[24];
        snprintf(value, sizeof(value), "%+.2f %+.2f", m.sticks[0], m.sticks[1]);
        row(g, 0, m.b_stickPressed[0] ? "L *" : "L", value);
        snprintf(value, sizeof(value), "%+.2f %+.2f", m.sticks[2], m.sticks[3]);
        row(g, 1, m.b_stickPressed[1] ? "R *" : "R", value);
        //the buttons held right now, and the E-stop
        char held[24] = {0};
        if(m.b_pushbutton[0]) strcat(held, "< ");
        if(m.b_disableButton) strcat(held, "DIS ");
        if(m.b_enableButton) strcat(held, "EN ");
        if(m.b_pushbutton[1]) strcat(held, "> ");
        text(g, pixelFont, 0, 24, held[0] ? held : "-");
        //the E-stop's state and its two raw contacts (1 = high)
        char estop[24];
        const char* state = m.estop == EstopState::CLEAR ? "ok" : (m.estop == EstopState::WIRING ? "WIRING" : "E-STOP");
        snprintf(estop, sizeof(estop), "%s NO%d NC%d", state, m.b_estopNoHigh ? 1 : 0, m.b_estopNcHigh ? 1 : 0);
        textRight(g, pixelFont, 125, 24, estop);
    }

    static void drawAboutPage(Adafruit_GFX& g, const ScreenModel& m){
        drawPageHeader(g, m, infoBitmap);
        char value[24];
        row(g, 0, "firmware", m.version);
        uint32_t s = m.uptimeSeconds;
        snprintf(value, sizeof(value), "%lu:%02lu:%02lu", (unsigned long)(s / 3600), (unsigned long)((s / 60) % 60), (unsigned long)(s % 60));
        row(g, 1, "uptime", value);
        //why it last started, and how many watchdog or crash resets since it was switched on (only if any)
        const char* cause = m.resetCause == ResetCause::WATCHDOG ? "watchdog" : (m.resetCause == ResetCause::CRASH ? "crash" : "power on");
        if(m.b_safeMode) snprintf(value, sizeof(value), "SAFE x%u", m.resetCount);
        else if(m.resetCount > 0) snprintf(value, sizeof(value), "%s x%u", cause, m.resetCount);
        else snprintf(value, sizeof(value), "%s", cause);
        row(g, 2, "last boot", value);
    }

    static void drawSettingsPage(Adafruit_GFX& g, const ScreenModel& m){
        drawPageHeader(g, m, gearBitmap);
        char value[24];
        snprintf(value, sizeof(value), "%X", m.settings.channel);
        row(g, 0, "channel", value);
        snprintf(value, sizeof(value), "%u/4 %s", m.settings.contrast + 1, RemoteSettings::dimmingLabel(m.settings.dimming));
        row(g, 1, "screen", value);
        //the way in: the stick to the right
        textRight(g, pixelFont, 125, 24, "change: stick >");
    }

    void drawPage(Adafruit_GFX& g, const ScreenModel& m){
        switch(m.page){
            case Page::LINK: drawLinkPage(g, m); break;
            case Page::RADIO: drawRadioPage(g, m); break;
            case Page::BATTERY: drawBatteryPage(g, m); break;
            case Page::INPUTS: drawInputsPage(g, m); break;
            case Page::ABOUT: drawAboutPage(g, m); break;
            case Page::SETTINGS: drawSettingsPage(g, m); break;
            default: drawStatus(g, m); break;
        }
    }

    //——— settings menu: four rows on the 8 px text grid, the selected one a filled bar ———

    void drawMenu(Adafruit_GFX& g, const ScreenModel& m){
        int selected = int(m.menuItem);
        int itemCount = int(SettingsMenu::Item::COUNT);
        //three rows: a 9 px bar and its 1 px gap per row, 4 would not leave the gaps
        const int rows = 3;
        int first = selected - 1;
        if(first > itemCount - rows) first = itemCount - rows;
        if(first < 0) first = 0;

        for(int r = 0; r < rows; r++){
            int item = first + r;
            int16_t y = 1 + r * 10;
            bool b_selected = item == selected;
            uint16_t ink = b_selected ? OFF : ON;
            if(b_selected) g.fillRoundRect(0, y - 1, 123, 9, 2, ON);

            const char* name = "";
            char value[16] = {0};
            bool b_editable = false;
            bool b_changed = false;
            switch(SettingsMenu::Item(item)){
                case SettingsMenu::Item::CHANNEL:
                    name = "Channel";
                    snprintf(value, sizeof(value), "%X", m.pending.channel);
                    b_editable = true;
                    b_changed = m.pending.channel != m.settings.channel;
                    break;
                case SettingsMenu::Item::CONTRAST:
                    name = "Contrast";
                    snprintf(value, sizeof(value), "%u/4", m.pending.contrast + 1);
                    b_editable = true;
                    b_changed = m.pending.contrast != m.settings.contrast;
                    break;
                case SettingsMenu::Item::DIMMING:
                    name = "Dimming";
                    snprintf(value, sizeof(value), "%s", RemoteSettings::dimmingLabel(m.pending.dimming));
                    b_editable = true;
                    b_changed = m.pending.dimming != m.settings.dimming;
                    break;
                case SettingsMenu::Item::STICKS:
                    name = "Sticks";
                    snprintf(value, sizeof(value), "calibrate >");
                    break;
                case SettingsMenu::Item::SAVE:
                    name = m.pending != m.settings ? "Save" : "Save (none)";
                    if(b_selected && m.pending != m.settings) snprintf(value, sizeof(value), "hold >");
                    break;
                case SettingsMenu::Item::EXIT:
                    name = "Exit";
                    snprintf(value, sizeof(value), "<");
                    break;
                default: break;
            }
            text(g, pixelFont, 2, y, name, ink);
            if(b_changed) g.fillRect(2 + textWidth(g, pixelFont, name) + 2, y + 2, 2, 3, ink);
            //a short confirmation replaces the selected row's value for a moment
            if(b_selected && m.note) snprintf(value, sizeof(value), "%s", m.note);
            if(value[0]){
                if(b_selected && b_editable && !m.note){
                    char arrows[20];
                    snprintf(arrows, sizeof(arrows), "< %s >", value);
                    textRight(g, pixelFont, 120, y, arrows, ink);
                }
                else textRight(g, pixelFont, 120, y, value, ink);
            }
            //save: a bar filling beside the word while the stick is held right
            if(SettingsMenu::Item(item) == SettingsMenu::Item::SAVE && b_selected && m.pending != m.settings){
                g.drawRect(30, y + 1, 50, 5, OFF);
                g.fillRect(30, y + 1, int16_t(50 * m.saveProgress), 5, OFF);
            }
        }
        //where in the list: a thin bar on the right edge
        int16_t barTop = first * 32 / itemCount;
        int16_t barHeight = rows * 32 / itemCount;
        g.drawFastVLine(125, 0, 32, ON);
        g.fillRect(124, barTop, 3, barHeight, ON);
    }

    //——— stick calibration [H15] ———

    //a stick's circle: the live position, and the extremes recorded so far (or saved)
    static void stickCircle(Adafruit_GFX& g, int16_t cx, uint16_t rawX, uint16_t rawY,
                            const AxisCalibration& x, const AxisCalibration& y, bool b_extremes, bool b_live){
        const int16_t cy = 15;
        const int16_t r = 14;
        const float half = AxisCalibration::fullScale / 2.0f;
        g.drawCircle(cx, cy, r, ON);
        float centreX = b_extremes ? x.centre : half;
        float centreY = b_extremes ? y.centre : half;
        if(b_extremes){
            int16_t left = cx + int16_t((x.low - centreX) * r / half);
            int16_t right = cx + int16_t((x.high - centreX) * r / half);
            int16_t top = cy + int16_t((y.low - centreY) * r / half);
            int16_t bottom = cy + int16_t((y.high - centreY) * r / half);
            g.drawRect(left, top, right - left + 1, bottom - top + 1, ON);
            g.drawFastHLine(cx - 2, cy, 5, ON);
            g.drawFastVLine(cx, cy - 2, 5, ON);
        }
        if(b_live){
            int16_t px = cx + int16_t((rawX - centreX) * r / half);
            int16_t py = cy + int16_t((rawY - centreY) * r / half);
            g.fillCircle(px, py, 1, ON);
        }
    }

    void drawCalibration(Adafruit_GFX& g, const ScreenModel& m){
        const StickCalibration& c = m.calibration;
        stickCircle(g, 15, m.raw[0], m.raw[1], c.axes[0], c.axes[1], m.b_circling, true);
        stickCircle(g, 111, m.raw[2], m.raw[3], c.axes[2], c.axes[3], m.b_circling, true);
        //two lines of instruction, the step on the left above the way out (the save pill keeps clear on the right)
        if(!m.b_circling){
            textCentered(g, pixelFont, 63, 0, "Release");
            textCentered(g, pixelFont, 63, 8, "both");
        }
        else if(m.b_keepCircling){
            textCentered(g, pixelFont, 63, 0, "Circle");
            textCentered(g, pixelFont, 63, 8, "further");
        }
        else{
            textCentered(g, pixelFont, 63, 0, "Circle");
            textCentered(g, pixelFont, 63, 8, "both");
        }
        text(g, pixelFont, 32, 16, m.b_circling ? "2/2" : "1/2");
        //the ways out, on the pushbuttons below
        text(g, pixelFont, 32, 24, "<esc");
        if(m.b_circling && m.b_calibrationComplete) pillRight(g, 94, bottomTop, "SAVE>");
    }

    //after saving: each stick's measured box in its circle, its worst side as a share of the half scale and the
    //offset of its rest position
    void drawCalibrationResult(Adafruit_GFX& g, const ScreenModel& m){
        const StickCalibration& c = m.calibration;
        stickCircle(g, 15, 0, 0, c.axes[0], c.axes[1], true, false);
        stickCircle(g, 111, 0, 0, c.axes[2], c.axes[3], true, false);
        auto reach = [](const AxisCalibration& a){
            int low = a.centre - a.low;
            int high = a.high - a.centre;
            return (low < high ? low : high) * 100 / 512;
        };
        auto offset = [](const AxisCalibration& a){ return (int(a.centre) - 512) * 100 / 512; };
        auto stickLine = [&](char* line, size_t size, const AxisCalibration& x, const AxisCalibration& y){
            int r = reach(x) < reach(y) ? reach(x) : reach(y);
            int o = abs(offset(x)) > abs(offset(y)) ? offset(x) : offset(y);
            snprintf(line, size, "%d%% %+d", r, o);
        };
        textCentered(g, pixelFont, 63, 0, "Saved");
        char line[16];
        stickLine(line, sizeof(line), c.axes[0], c.axes[1]);
        text(g, pixelFont, 34, 8, line);
        stickLine(line, sizeof(line), c.axes[2], c.axes[3]);
        textRight(g, pixelFont, 93, 16, line);
        //under the empty left half of the line above
        pill(g, 33, bottomTop, "OK");
    }

    void draw(Adafruit_GFX& g, const ScreenModel& m){
        if(m.b_calibrationResult) drawCalibrationResult(g, m);
        else if(m.b_menuOpen && m.b_calibrating) drawCalibration(g, m);
        else if(m.b_menuOpen) drawMenu(g, m);
        else drawPage(g, m);
        g.setFont(nullptr);
    }

};
