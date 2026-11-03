#include "RemoteScreens.h"
#include <CreepView.h>
#include <Fonts/FreeSansBold9pt7b.h>

//Remote screens [D-147]: 128 x 32, three rows. Top (y 0..7): link and batteries. Middle (y 8..22): the one thing that
//matters now. Bottom (y 23..31): the speed switch on the left and the frame switch on the right, each above its switch,
//the stopwatch between them [D-160]. The alive sign is a gap travelling round the state dot (main screen) or the active
//page dot (pages) every few frames.

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
        //the reply indicator, as the first firmware drew it: a triangle turning a quarter with every reply received
        //(the font's four arrow heads), 1 px right of the bars
        if(m.b_linkUp){
            static const char* const turn[4] = {"\x10", "\x1E", "\x11", "\x1F"};
            text(g, pixelFont, x + signalWidth + 1, y, turn[m.rxCounter % 4]);
        }
    }

    //the alive sign: a one pixel gap travelling round a dot's edge every few frames (the screen is still being drawn).
    //One per screen: on the main screen in the top row after the channel, the active page dot on the pages.
    static int aliveStep(const ScreenModel& m){ return (m.frame / 3) % 8; }

    //the active page dot (also the main screen's alive dot), 3 x 3, with or without the travelling gap
    static void activeDot(Adafruit_GFX& g, int16_t x, int16_t y, const ScreenModel& m, bool b_alive){
        static const int8_t ring[8][2] = {{0, 0}, {1, 0}, {2, 0}, {2, 1}, {2, 2}, {1, 2}, {0, 2}, {0, 1}};
        g.fillRect(x, y, 3, 3, ON);
        if(b_alive) g.drawPixel(x + ring[aliveStep(m)][0], y + ring[aliveStep(m)][1], OFF);
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
    //four arrow heads round a cross: the Motion page [D-161]
    static const uint8_t motionBitmap[] = { 0x10, 0x38, 0x92, 0xFE, 0x92, 0x38, 0x10 };
    //a stick's top, up and pressed (smaller): the About page's sign that the games can be opened
    static const uint8_t stickUpBitmap[] = { 0x38, 0x7C, 0xFE, 0xFE, 0xFE, 0x7C, 0x38 };
    static const uint8_t stickDownBitmap[] = { 0x00, 0x38, 0x7C, 0x7C, 0x7C, 0x38, 0x00 };

    //——— the vehicle state as shown [D-163] ———

    //the reply's byte 12 (Stacato's MAV_STATE and armed flag) in capitals, for the Motion title and the Status word
    //where it says more than the 3 bit state; UNKNOWN: the 3 bit state's text; nullptr: none
    static const char* reportedStateText(uint8_t reportedState){
        switch(RobotState(reportedState)){
            case RobotState::DISABLING: return "DISABLING";
            case RobotState::DISABLED: return "DISABLED";
            case RobotState::ENABLING: return "ENABLING";
            case RobotState::ENABLED: return "ENABLED";
            case RobotState::EMERGENCY_STOPPING: return "STOPPING";
            case RobotState::EMERGENCY_STOPPED: return "E-STOP";
            default: return nullptr;
        }
    }

    const char* vehicleStateText(Reply::VehicleState state, uint8_t reportedState){
        switch(state){
            case Reply::VehicleState::NO_LINK: return "NO LINK";
            case Reply::VehicleState::NO_STACATO: return "NO STACATO";
            case Reply::VehicleState::BUS_START: return "BUS START";
            case Reply::VehicleState::BUS_OFF: return "BUS OFF";
            case Reply::VehicleState::VEHICLE_OFF: return "VEHICLE OFF";
            case Reply::VehicleState::DISABLED: return "DISABLED";
            case Reply::VehicleState::ENABLING: return "ENABLING";
            case Reply::VehicleState::ENABLED: return "ENABLED";
            case Reply::VehicleState::DISABLING: return "DISABLING";
            case Reply::VehicleState::E_STOPPING: return "E-STOPPING";
            case Reply::VehicleState::E_STOP: return "E-STOP";
            case Reply::VehicleState::AXIS_FAULT: return "AXIS FAULT";
            default: return reportedStateText(reportedState);
        }
    }

    //the state as shown: NO_LINK whenever the link is down, whatever the view still holds
    static Reply::VehicleState shownVehicleState(const ScreenModel& m){
        return m.b_linkUp ? m.motion.vehicleState : Reply::VehicleState::NO_LINK;
    }

    //——— the status screen ———

    static void drawTopRow(Adafruit_GFX& g, const ScreenModel& m){
        signalIcon(g, 0, 0, m);
        char channel[8];
        if(m.b_direct) snprintf(channel, sizeof(channel), m.b_dropout ? "DROP" : "USB");
        else if(m.b_usbWaiting && blink(m.now, 1000, 500)) snprintf(channel, sizeof(channel), "USB");
        else if(m.b_radioOff) snprintf(channel, sizeof(channel), "RF OFF");
        else snprintf(channel, sizeof(channel), "CH%X", m.channel);
        //after the signal bars and the reply triangle (5 px), 2 px clear of it; then the alive dot
        int16_t channelX = signalWidth + 1 + 5 + 2;
        text(g, pixelFont, channelX, 0, channel);
        activeDot(g, channelX + textWidth(g, pixelFont, channel) + 3, 2, m, true);

        //the vehicle's pack, then the remote's own battery (blinking when low)
        char voltage[8];
        if(m.b_linkUp) snprintf(voltage, sizeof(voltage), "%.1fV", m.robotVoltage);
        else snprintf(voltage, sizeof(voltage), "--.-V");
        textRight(g, pixelFont, 111, 0, voltage);
        if(m.remoteBattery == BatteryState::OK || blink(m.now, 1000, 600)) batteryIcon(g, 114, 0, m.remotePercent);
    }

    //[D-163] The message as shown: the 3 bit state's E-stop ramp is also Stacato's disabling ramp (CRITICAL with no
    //E-stop latched): shown as Disabling, a state, not as the STOPPING alarm. Every decision keeps m.status
    static BigMessage shownMessage(const ScreenModel& m){
        if(m.status.message == BigMessage::STOPPING && shownVehicleState(m) == Reply::VehicleState::DISABLING) return BigMessage::DISABLING;
        return m.status.message;
    }

    //the vehicle states that say more than the 3 bit state, in the state word's place [D-163]
    static bool isFinerState(Reply::VehicleState state){
        switch(state){
            case Reply::VehicleState::NO_STACATO:
            case Reply::VehicleState::BUS_START:
            case Reply::VehicleState::BUS_OFF:
            case Reply::VehicleState::VEHICLE_OFF:
            case Reply::VehicleState::AXIS_FAULT:
                return true;
            default:
                return false;
        }
    }

    //in the big font in title case like the other state words (in capitals NO STACATO and VEHICLE OFF do not fit)
    static const char* finerStateText(Reply::VehicleState state){
        switch(state){
            case Reply::VehicleState::NO_STACATO: return "No Stacato";
            case Reply::VehicleState::BUS_START: return "Bus start";
            case Reply::VehicleState::BUS_OFF: return "Bus off";
            case Reply::VehicleState::VEHICLE_OFF: return "Vehicle off";
            case Reply::VehicleState::AXIS_FAULT: return "Axis fault";
            default: return "Unknown";
        }
    }

    static const char* stateText(BigMessage message){
        switch(message){
            case BigMessage::ENABLED: return "Enabled";
            case BigMessage::ENABLING: return "Enabling";
            case BigMessage::DISABLED: return "Disabled";
            case BigMessage::DISABLING: return "Disabling";
            case BigMessage::RADIO_OFF: return "Radio off";
            default: return "Unknown";
        }
    }

    //a state from byte 12 when the 3 bit state is none of the four
    static BigMessage messageFromVehicleState(Reply::VehicleState state){
        switch(state){
            case Reply::VehicleState::DISABLED: return BigMessage::DISABLED;
            case Reply::VehicleState::ENABLING: return BigMessage::ENABLING;
            case Reply::VehicleState::ENABLED: return BigMessage::ENABLED;
            case Reply::VehicleState::DISABLING: return BigMessage::DISABLING;
            default: return BigMessage::UNKNOWN;
        }
    }

    static const char* alarmText(const ScreenModel& m, BigMessage message){
        switch(message){
            case BigMessage::ESTOP: return "E-STOP";
            case BigMessage::EXT_ESTOP:
                //[D-163] Stacato's EMERGENCY with no E-stop latched: a drive fault, not an E-stop
                return shownVehicleState(m) == Reply::VehicleState::AXIS_FAULT ? "AXIS FAULT" : "E-STOP";
            case BigMessage::ESTOP_WIRING: return "WIRING";
            case BigMessage::RADIO_FAULT: return "RADIO";
            case BigMessage::NO_LINK: return "NO LINK";
            case BigMessage::BATTERY_CRITICAL: return "BATTERY";
            case BigMessage::STOPPING:
                return shownVehicleState(m) == Reply::VehicleState::E_STOPPING ? "E-STOPPING" : "STOPPING";
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
            case BigMessage::EXT_ESTOP:
                return shownVehicleState(m) == Reply::VehicleState::AXIS_FAULT ? "a drive is faulted" : "not from this remote";
            case BigMessage::BATTERY_CRITICAL: return "charge the remote";
            case BigMessage::STOPPING: return "e-stop ramp";
            case BigMessage::ESTOP: return "on this remote";
            default: return nullptr;
        }
    }

    static void drawMiddle(Adafruit_GFX& g, const ScreenModel& m){
        BigMessage message = shownMessage(m);

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
            //an E-stop's banner pulses, own or not, in step with the LEDs (Leds::estopFlash)
            if(message == BigMessage::ESTOP) b_filled = blink(m.now, 800, 500);
            if(message == BigMessage::EXT_ESTOP && shownVehicleState(m) != Reply::VehicleState::AXIS_FAULT) b_filled = blink(m.now, 800, 500);
            if(message == BigMessage::STOPPING) b_filled = blink(m.now, 300, 150);
            uint16_t ink = b_filled ? OFF : ON;
            if(b_filled) g.fillRoundRect(0, middleTop, 127, middleHeight, 3, ON);
            const char* word = alarmText(m, message);
            bool b_axisFault = message == BigMessage::EXT_ESTOP && shownVehicleState(m) == Reply::VehicleState::AXIS_FAULT;
            bool b_estop = (message == BigMessage::ESTOP || message == BigMessage::EXT_ESTOP
                        || message == BigMessage::ESTOP_WIRING || message == BigMessage::STOPPING) && !b_axisFault;
            //the icon on the left and the word centred right of it (x 16..122); a word too wide for that (E-STOPPING)
            //says what it is by itself: centred on the screen, without the icon
            int16_t wordWidth = textWidth(g, bigFont, word);
            if(wordWidth <= 107){
                if(b_estop) estopIcon(g, 4, middleTop + 3, ink);
                else g.drawBitmap(4, middleTop + 4, warningBitmap, 9, 8, ink);
                int16_t x = 70 - wordWidth / 2;
                if(x < 16) x = 16;
                text(g, bigFont, x, middleTop + 13, word, ink);
            }
            else textCentered(g, bigFont, 63, middleTop + 13, word, ink);
            return;
        }

        //a normal state: the word alone; the vehicle's finer state where it says more [D-163]
        Reply::VehicleState vehicle = shownVehicleState(m);
        bool b_finer = isFinerState(vehicle);
        if(message == BigMessage::UNKNOWN && !b_finer) message = messageFromVehicleState(vehicle);
        const char* word = b_finer ? finerStateText(vehicle) : stateText(message);
        int16_t wordWidth = textWidth(g, bigFont, word);
        //the word alone, centred (the alive dot is in the top row)
        int16_t x = (127 - wordWidth) / 2;
        //a descender (Enabling, Disabling) reaches the bottom row: keep it left of the frame indicator
        if(strchr(word, 'g')){
            const char* frame = m.b_worldFrame ? "WORLD" : "BODY";
            int16_t frameLeft = 125 - textWidth(g, pixelFont, frame) - (m.b_worldFrame ? 3 : 1);
            int16_t limit = frameLeft - 2 - wordWidth;
            if(x > limit) x = limit;
            if(x < 0) x = 0;
        }
        text(g, bigFont, x, middleTop + 13, word);
    }

    //——— the stopwatch [D-160] ———

    //where an element w px wide goes: centred under the screen's middle, kept inside left..right
    static int16_t placeCentred(int16_t w, int16_t left, int16_t right){
        int16_t x = 63 - w / 2;
        if(x + w - 1 > right) x = right - w + 1;
        if(x < left) x = left;
        return x;
    }

    //the edit mode's stick mark, 5 px wide in a 7 px text line (top at y): up and down triangles (seconds) and a
    //dot each side (minutes)
    static const int16_t stickMarkWidth = 5;
    static void stickMarks(Adafruit_GFX& g, int16_t x, int16_t y, uint16_t color){
        g.drawPixel(x + 2, y, color);
        g.drawFastHLine(x + 1, y + 1, 3, color);
        g.drawPixel(x, y + 3, color);
        g.drawPixel(x + 4, y + 3, color);
        g.drawFastHLine(x + 1, y + 5, 3, color);
        g.drawPixel(x + 2, y + 6, color);
    }

    //Bottom centre, between the speed and frame labels (left..right, already 1 px clear of them):
    //  editing: a pill "SET m:ss" with the stick mark (up / down seconds, left / right minutes), steady
    //  counting up or idle: plain digits, m:ss.t below 10 minutes, mm:ss from 10 (0:00.0, 2:13.4, 12:05)
    //  countdown: plain digits too, the time left (the bar across the screen shows how much, countdownBar)
    //  overtime (run past zero, it keeps running): negative time, -0:05, -1:12.3, the full pill and plain text taking
    //    turns every 0.5 s; stopped in overtime: the full pill, steady, the time frozen
    //  stopped otherwise: the digits blink slowly (0.8 s on, 0.4 s off)
    static void drawStopwatch(Adafruit_GFX& g, const ScreenModel& m, int16_t left, int16_t right){
        const StopwatchView& v = m.stopwatch;
        char digits[12];
        if(v.b_editing){
            char label[16];
            Stopwatch::format(digits, sizeof(digits), v.setMillis, false);
            snprintf(label, sizeof(label), "SET %s", digits);
            int16_t tw = textWidth(g, pixelFont, label);
            int16_t w = 2 + tw + 2 + stickMarkWidth + 2;
            int16_t x = placeCentred(w, left, right);
            g.fillRoundRect(x, bottomTop, w, pillHeight, 2, ON);
            text(g, pixelFont, x + 2, bottomTop + 1, label, OFF);
            stickMarks(g, x + 2 + tw + 2, bottomTop + 1, OFF);
            return;
        }
        bool b_digits = v.state != Stopwatch::State::STOPPED || blink(m.now, 1200, 800);
        if(!v.b_overtime){
            //counting up, or down (the bar across the screen shows the countdown, drawStatus)
            Stopwatch::format(digits, sizeof(digits), v.shownMillis, true);
            int16_t x = placeCentred(textWidth(g, pixelFont, digits), left, right);
            if(b_digits) text(g, pixelFont, x, bottomTop + 1, digits);
            return;
        }
        //past zero: negative time in a pill, blinking while it runs, steady when stopped
        Stopwatch::format(digits, sizeof(digits), v.shownMillis, true, true);
        int16_t tw = textWidth(g, pixelFont, digits);
        //a fixed width (the widest -m:ss.t) so the pill does not jump as the digits change; shrunk to fit
        int16_t w = textWidth(g, pixelFont, "-0:00.0") + 4;
        if(w < tw + 4) w = tw + 4;
        if(w > right - left + 1) w = right - left + 1;
        int16_t x = placeCentred(w, left, right);
        int16_t tx = x + (w - tw) / 2;
        if(v.state == Stopwatch::State::STOPPED || blink(m.now, 1000, 500)){
            g.fillRoundRect(x, bottomTop, w, pillHeight, 2, ON);
            text(g, pixelFont, tx, bottomTop + 1, digits, OFF);
        }
        else text(g, pixelFont, tx, bottomTop + 1, digits);
    }

    //[D-207] creep as the receiver echoes it, and only while the switch is at middle here (the echo lags a frame when
    //the switch leaves it)
    static bool isCreepShown(const ScreenModel& m){ return CreepView::isShown(m.b_creep, m.b_linkUp, m.speedState); }

    static void drawBottomRow(Adafruit_GFX& g, const ScreenModel& m){
        //during an alarm the row explains it (no room for a gap between the banner and pills): the switches matter less
        //than why nothing moves
        char buffer[24];
        bool b_alarm = Status::isAlarm(shownMessage(m));
        const char* note = b_alarm ? alarmNote(m, buffer, sizeof(buffer)) : nullptr;
        if(b_alarm){
            if(note){
                if(m.status.line == StatusLine::RESTART_REMOTE && !blink(m.now, 1000, 700)) return;
                textCentered(g, pixelFont, 63, 24, note);
                return;
            }
            //no explanation needed (NO LINK): the switches as plain text, pills would touch the banner
            //a stick pushed at OFF: the speed text blinks (no pill here, it would touch the banner)
            const char* speed = isCreepShown(m) ? "CREEP" : Status::speedText(m.speedState);
            if(!m.b_sticksOffWarning || blink(m.now, 250, 125)) text(g, pixelFont, 2, 24, speed);
            textRight(g, pixelFont, 123, 24, m.b_worldFrame ? "WORLD" : "BODY");
            return;
        }

        //speed switch, above it on the left: OFF as plain text (sticks do nothing), SLOW and FAST in a pill; creep
        //[D-207 point 5] CREEP flashing at 1 Hz, 0.5 s in the pill, 0.5 s plain text in the same place, so the word is
        //always readable and never taken for the steady SLOW
        SpeedMode speed = SpeedMode(m.speedState);
        bool b_creep = isCreepShown(m);
        if(b_creep){
            if(CreepView::isPillFilled(m.now)) pill(g, 0, bottomTop, "CREEP");
            else text(g, pixelFont, 2, bottomTop + 1, "CREEP");
        }
        else if(speed == SpeedMode::MEDIUM_SPEED) pill(g, 0, bottomTop, "SLOW");
        else if(speed == SpeedMode::HIGH_SPEED) pill(g, 0, bottomTop, "FAST");
        //a stick pushed at OFF (the grind sounds): OFF blinks negative, in a pill
        else if(m.b_sticksOffWarning && blink(m.now, 250, 125)) pill(g, 0, bottomTop, "OFF");
        else text(g, pixelFont, 2, bottomTop + 1, "OFF");

        //frame switch, above it on the right: BODY (the default) plain, WORLD in a pill
        if(m.b_worldFrame) pillRight(g, 125, bottomTop, "WORLD");
        else textRight(g, pixelFont, 123, bottomTop + 1, "BODY");

        //the free span between them, 1 px clear of each
        int16_t speedRight = b_creep ? pillWidth(g, "CREEP") - 1
                           : speed == SpeedMode::MEDIUM_SPEED || speed == SpeedMode::HIGH_SPEED ? pillWidth(g, "SLOW") - 1
                           : (m.b_sticksOffWarning ? pillWidth(g, "OFF") - 1 : 2 + textWidth(g, pixelFont, "OFF") - 1);
        int16_t frameLeft = m.b_worldFrame ? 125 - pillWidth(g, "WORLD") + 1 : 123 - textWidth(g, pixelFont, "BODY") + 1;
        int16_t left = speedRight + 2;
        int16_t right = frameLeft - 2;

        //between them: thank you or a marker (one at a time, taking turns every second) while they last, otherwise
        //the stopwatch [D-160] (the Status page has no page dots)
        const char* markers[4];
        int count = 0;
        if(m.b_rebooted) markers[count++] = "REBOOT";
        if(m.b_recovery) markers[count++] = "RST";
        if(m.b_hot) markers[count++] = "HOT";
        if(m.b_safeMode) markers[count++] = "SAFE";
        //thank you between hearts (character 3 of the built-in font, as the first firmware drew them): spaced when it
        //fits between the switch labels, tight when a pill leaves less room
        if(m.status.line == StatusLine::THANK_YOU){
            static const char* thanks[4] = {"\x03 thank you \x03", "\x03thank you\x03", "\x03 thanks \x03", "\x03thanks\x03"};
            const char* label = thanks[3];
            for(int i = 0; i < 4; i++){
                if(textWidth(g, pixelFont, thanks[i]) <= right - left + 1){
                    label = thanks[i];
                    break;
                }
            }
            textCentered(g, pixelFont, (left + right) / 2, bottomTop + 1, label);
            return;
        }
        if(count > 0){
            const char* marker = markers[(m.now / 1000) % count];
            pill(g, 63 - pillWidth(g, marker) / 2, bottomTop, marker);
        }
        else drawStopwatch(g, m, left, right);
    }

    //The countdown's bar [D-160]: row 8, the full width, lit from the left for the time left (remaining / set), so
    //it shrinks from right to left; full and blinking past zero (steady when stopped there). Row 7 keeps it clear of
    //the top row; the state word starts at row 9, so the bar leaves a notch over each letter top (the letter's
    //columns and one either side): nothing touches and nothing else moves. Hidden during an alarm, like the digits
    static const int16_t countdownBarRow = 8;

    static bool countdownBarShown(const ScreenModel& m){
        return m.stopwatch.b_countdown && !Status::isAlarm(shownMessage(m));
    }

    static void countdownBar(Adafruit_GFX& g, const ScreenModel& m, const GFXcanvas1* middle){
        const StopwatchView& v = m.stopwatch;
        int16_t length = 128;
        if(v.b_overtime){
            if(v.state != Stopwatch::State::STOPPED && !blink(m.now, 1000, 500)) return;
        }
        else{
            float fraction = v.remainingFraction < 0.0f ? 0.0f : (v.remainingFraction > 1.0f ? 1.0f : v.remainingFraction);
            length = int16_t(fraction * 128.0f + 0.5f);
        }
        //the bar's end (its last 2 px, the playhead) ignores the notches: always visible, even over a letter (owner,
        //2026-10-27: the end was lost in a notch); there it may touch a letter top, the one exception to the gaps
        const int16_t headFrom = v.b_overtime ? 128 : length - 2;
        for(int16_t x = 0; x < length; x++){
            bool b_notch = false;
            for(int16_t dx = -1; dx <= 1 && middle && x < headFrom; dx++){
                if(x + dx >= 0 && x + dx < 128 && middle->getPixel(x + dx, countdownBarRow + 1)) b_notch = true;
            }
            if(!b_notch) g.drawPixel(x, countdownBarRow, ON);
        }
    }

    void drawStatus(Adafruit_GFX& g, const ScreenModel& m){
        drawTopRow(g, m);
        if(countdownBarShown(m)){
            //the middle drawn aside first: the bar needs to know where the word's letter tops are
            GFXcanvas1 middle(128, 32);
            middle.fillScreen(0);
            drawMiddle(middle, m);
            for(int16_t y = middleTop; y < bottomTop + pillHeight; y++){
                for(int16_t x = 0; x < 128; x++) if(middle.getPixel(x, y)) g.drawPixel(x, y, ON);
            }
            countdownBar(g, m, &middle);
        }
        else drawMiddle(g, m);
        drawBottomRow(g, m);
    }

    //——— info pages ———

    //the title line: icon and title, page dots on the right (plain: a filled bar would need 9 rows and leave 2 lines)
    const char* pageTitle(Page page){
        switch(page){
            case Page::MOTION: return "Motion";
            case Page::MOTORS: return "Motors";
            case Page::TRACKING: return "Tracking";
            case Page::LINK: return "Link";
            case Page::RADIO: return "Radio";
            case Page::BATTERY: return "Battery";
            case Page::INPUTS: return "Inputs";
            case Page::ABOUT: return "About";
            case Page::SETTINGS: return "Settings";
            default: return "";
        }
    }

    //title: instead of the page's own (the Motion page shows the vehicle state there)
    static void drawPageHeader(Adafruit_GFX& g, const ScreenModel& m, const uint8_t* icon, const char* title = nullptr){
        g.drawBitmap(0, 0, icon, 7, 7, ON);
        text(g, pixelFont, 10, 0, m.titleOverride ? m.titleOverride : (title ? title : pageTitle(m.page)));
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

    //——— the Motion page [D-161, D-163] ———

    //A second main page next to Status: no return to Status after 10 s, the stopwatch's controls work here too, and it
    //shows the same timer [D-160]. Three lines in rows 9..31, one per axis, its label in the middle: the body velocity
    //left of it, the pose right of it. The wheels have their own page (Motors).
    //  title line (rows 0..6, no page dots, as on Status): the motion icon, the vehicle state in capitals (small: a
    //  descender would touch the bar's row), the alive dot after it, and the timer right-aligned in plain digits (m:ss.t,
    //  mm:ss; blinking slowly when stopped; past zero the negative time, blinking with the bar, steady when stopped)
    //  row 8: the countdown bar, drawn by the Status page's code (notches over the content's tops in row 9)
    //  labels X, Y, R (x 61..65): the axis of both values on the line
    //  left, right-aligned at x 54: the body velocity, radio axes: X+ right, Y+ forward in m/s with 3 decimals, R+
    //  clockwise in deg/s with 1
    //  right, right-aligned at x 110: the pose in Stacato's world frame: x stage right, y upstage in m with 3 decimals,
    //  the heading in deg with 1 and a degree ring after it (counter-clockwise, 0 upstage); -- without a value
    //  no units but "/s" after each velocity (it tells velocity from position), "m" after x and y and the heading's
    //  degree; title: the page's name (the vehicle state is the Status page's); no plus signs:
    //  a minus only
    //  zeroed (the frame switch turned to WORLD, PoseZero): the pose is the movement since, in the vehicle's frame at
    //  that moment (y forward, x right)
    //  no motion data (no link, nothing fresh from Stacato): the page says so on the text lines (without a link the
    //  title says NO LINK already), title and bar stay
    //  editing the countdown: in place of the timer, SET m:ss and the stick marks in plain text; the content stays, the
    //  bar is hidden
    static const int16_t motionContentTop = 9;
    static const int16_t axisLabelX = 61;
    static const int16_t velocityValueRight = 54;
    static const int16_t poseValueRight = 110;

    //the font's degree sign is a 4 x 4 blob that reads as a dot: a 3 x 3 ring (its corners off) at the line's top
    static void degreeRing(Adafruit_GFX& g, int16_t x, int16_t top){
        g.drawPixel(x + 1, top, ON);
        g.drawPixel(x, top + 1, ON);
        g.drawPixel(x + 2, top + 1, ON);
        g.drawPixel(x + 1, top + 2, ON);
    }

    //a value as printed: a minus only, none on zero
    static void plainValue(char* buffer, size_t size, float value, int decimals){
        float half = 0.5f * powf(10.0f, float(-decimals));
        if(fabsf(value) < half) value = 0.0f;
        snprintf(buffer, size, "%.*f", decimals, value);
    }

    //the timer in the title line, plain, right-aligned at x 126 (no pill: a 9 px pill would not fit above the content);
    //editing in place: SET and the time being set, then the stick marks, steady, the content staying in view
    static void titleTimer(Adafruit_GFX& g, const ScreenModel& m){
        const StopwatchView& v = m.stopwatch;
        char digits[12];
        if(v.b_editing){
            char label[16];
            Stopwatch::format(digits, sizeof(digits), v.setMillis, false);
            snprintf(label, sizeof(label), "SET %s", digits);
            int16_t marksX = 126 - stickMarkWidth + 1;
            textRight(g, pixelFont, marksX - 2, 0, label);
            stickMarks(g, marksX, 0, ON);
            return;
        }
        bool b_shown;
        if(v.b_overtime){
            Stopwatch::format(digits, sizeof(digits), v.shownMillis, true, true);
            //the same phase as the bar (countdownBar)
            b_shown = v.state == Stopwatch::State::STOPPED || blink(m.now, 1000, 500);
        }
        else{
            Stopwatch::format(digits, sizeof(digits), v.shownMillis, true);
            b_shown = v.state != Stopwatch::State::STOPPED || blink(m.now, 1200, 800);
        }
        if(b_shown) textRight(g, pixelFont, 126, 0, digits);
    }

    static void drawMotionContent(Adafruit_GFX& g, const ScreenModel& m){
        const char* reason = nullptr;
        switch(m.motionAvailability){
            case Reply::MotionAvailability::NO_LINK: reason = "no link"; break;
            case Reply::MotionAvailability::NO_DATA: reason = "none from Stacato"; break;
            case Reply::MotionAvailability::LIVE: break;
        }
        if(reason){
            textCentered(g, pixelFont, 63, motionContentTop, "no motion data");
            //without a link the title says so already
            if(m.motionAvailability != Reply::MotionAvailability::NO_LINK) textCentered(g, pixelFont, 63, motionContentTop + 8, reason);
            return;
        }
        const Reply::MotionView& v = m.motion;
        char value[12];
        const char* labels[3] = {"X", "Y", "R"};
        //the velocity: m/s with 3 decimals below 10 m/s, deg/s with one below 1000
        float degrees = v.vr * 57.29578f;
        const float velocity[3] = {v.vx, v.vy, degrees};
        //the pose: m with 3 decimals (mm) below 100 m, the heading in deg with one
        const float pose[3] = {v.poseX, v.poseY, v.heading};
        for(int i = 0; i < 3; i++){
            int16_t top = motionContentTop + i * 8;
            text(g, pixelFont, axisLabelX, top, labels[i]);
            if(i < 2) plainValue(value, sizeof(value), velocity[i], fabsf(velocity[i]) < 9.9995f ? 3 : 2);
            else plainValue(value, sizeof(value), velocity[i], fabsf(velocity[i]) < 999.95f ? 1 : 0);
            //"/s" after the velocity says what it is, without a full unit
            textRight(g, pixelFont, velocityValueRight - textWidth(g, pixelFont, "/s") - 1, top, value);
            textRight(g, pixelFont, velocityValueRight, top, "/s");
            if(isnan(pose[i])) snprintf(value, sizeof(value), "--");
            //m with 3 decimals (mm) below 10 m, 2 below 100 m, then 1: room for the unit
            else if(i < 2) plainValue(value, sizeof(value), pose[i], fabsf(pose[i]) < 9.9995f ? 3 : (fabsf(pose[i]) < 99.995f ? 2 : 1));
            else plainValue(value, sizeof(value), pose[i], 1);
            textRight(g, pixelFont, poseValueRight, top, value);
            if(i == 2 && !isnan(pose[i])) degreeRing(g, poseValueRight + 2, top);
            //the position's unit after the value (owner, 2026-10-28), where the heading has its degree
            if(i < 2 && !isnan(pose[i])) text(g, pixelFont, poseValueRight + 2, top, "m");
        }
    }

    static void drawMotionPage(Adafruit_GFX& g, const ScreenModel& m){
        //the title line: icon, the page's name (owner, 2026-10-28: the state is the Status page's), the alive dot (no
        //page dots to carry it), the timer
        g.drawBitmap(0, 0, motionBitmap, 7, 7, ON);
        const char* title = m.titleOverride ? m.titleOverride : pageTitle(Page::MOTION);
        text(g, pixelFont, 10, 0, title);
        activeDot(g, 10 + textWidth(g, pixelFont, title) + 3, 2, m, true);
        titleTimer(g, m);
        //the countdown bar as on Status: the content drawn aside first, the bar notched over its tops
        if(countdownBarShown(m) && !m.stopwatch.b_editing){
            GFXcanvas1 content(128, 32);
            content.fillScreen(0);
            drawMotionContent(content, m);
            for(int16_t y = motionContentTop; y < 32; y++){
                for(int16_t x = 0; x < 128; x++) if(content.getPixel(x, y)) g.drawPixel(x, y, ON);
            }
            countdownBar(g, m, &content);
        }
        else drawMotionContent(g, m);
    }

    //——— the Motors page [D-163] ———

    //Permanent like the main pages (no return to Status), but the stopwatch's controls do not work here and it shows
    //no timer, no title and no page dots: the four wheels seen from above, the vehicle's front up. Each wheel is a
    //small mecanum wheel, 7 x 15 (a filled rounded body with dark diagonal rollers, FL and RR rising to the right, FR
    //and RL falling, as on the vehicle) whose rollers scroll with the wheel velocity: up while it rolls forward, faster
    //the faster it turns (wheelPhase). Beside each wheel, on its side of the screen (x 0..48 left, 78..126 right):
    //  a text line: its state next to the wheel (ON enabled, RDY ready, OFF neither, -- not yet known; FLT and STO in
    //  plain text instead, taking turns every second when both) and the velocity in % of the drive's limit outside
    //  under it two sliders from a centre tick, 24 px each way = 100 %, right = forward on both sides, with a tick at
    //  each end: the velocity solid (3 px), under it the torque in % of rated dotted (3 px); no bar without a value
    //The alive dot sits in the vehicle's centre.
    static const int16_t wheelWidth = 7;
    static const int16_t wheelHeight = 15;
    static const int16_t wheelPeriod = 4;
    static const int16_t sliderHalf = 23;      //the bar's length at 100 %; the end ticks one further out
    //front rows 0..14 (text lines 0 and 8), rear rows 17..31 (text lines 17 and 25): the 2 rows between keep the front
    //torque line's descender (trq) clear of the rear state line
    static const int16_t wheelTop[4] = {0, 0, 17, 17};
    static const bool wheelLeft[4] = {true, false, true, false};
    //FL and RR rollers rise to the right (/), FR and RL fall (\), seen from above (Stacato's Mecanum README)
    static const int8_t rollerSlant[4] = {1, -1, -1, 1};

    static void wheelIcon(Adafruit_GFX& g, int16_t x, int16_t y, int8_t slant, float phase){
        g.fillRoundRect(x, y, wheelWidth, wheelHeight, 2, ON);
        //the rollers: dark diagonals inside the rim, moved up by the phase (pixels along the wheel)
        int32_t shift = int32_t(floorf(phase));
        for(int16_t dy = 1; dy < wheelHeight - 1; dy++){
            for(int16_t dx = 1; dx < wheelWidth - 1; dx++){
                int32_t index = dx * slant + dy + shift;
                if(((index % wheelPeriod) + wheelPeriod) % wheelPeriod == 0) g.drawPixel(x + dx, y + dy, OFF);
            }
        }
    }

    float advanceWheelPhase(float phase, float velocity, uint32_t elapsedMillis){
        if(isnan(velocity)) return phase;
        if(elapsedMillis > 100) elapsedMillis = 100;
        phase += velocity * wheelPixelsPerSecond * float(elapsedMillis) / 1000.0f;
        //kept small: the pattern repeats every period
        phase = fmodf(phase, float(wheelPeriod * 1000));
        return phase;
    }

    static const char* wheelStateText(const ScreenModel& m, uint8_t wheel){
        const Reply::MotionView& v = m.motion;
        if(!m.b_linkUp || !v.b_flagsKnown) return "--";
        bool b_fault = Reply::wheelFlag(v.wheelFlags, wheel, Reply::FAULT);
        bool b_sto = Reply::wheelFlag(v.wheelFlags, wheel, Reply::STO);
        if(b_fault && b_sto) return (m.now / 1000) % 2 == 0 ? "FLT" : "STO";
        if(b_fault) return "FLT";
        if(b_sto) return "STO";
        if(Reply::wheelFlag(v.wheelFlags, wheel, Reply::ENABLED)) return "ON";
        if(Reply::wheelFlag(v.wheelFlags, wheel, Reply::READY)) return "RDY";
        return "OFF";
    }

    //a slider's bar from the centre, -1..1 (clamped), none for NaN; dotted: a checker
    static void sliderBar(Adafruit_GFX& g, int16_t centre, int16_t top, float value, bool b_dotted){
        if(isnan(value)) return;
        if(value > 1.0f) value = 1.0f;
        if(value < -1.0f) value = -1.0f;
        int16_t length = int16_t(lroundf(fabsf(value) * float(sliderHalf)));
        if(length == 0) return;
        int16_t left = value > 0.0f ? centre + 1 : centre - length;
        for(int16_t y = top; y < top + 3; y++){
            for(int16_t x = left; x < left + length; x++) if(!b_dotted || (x + y) % 2 == 0) g.drawPixel(x, y, ON);
        }
    }

    static void percentText(char* buffer, size_t size, float value, bool b_known){
        if(!b_known || isnan(value)) snprintf(buffer, size, "--");
        else{
            int percent = int(lroundf(value * 100.0f));
            if(percent > 100) percent = 100;
            if(percent < -100) percent = -100;
            if(percent == 0) snprintf(buffer, size, "0%%");
            else snprintf(buffer, size, "%+d%%", percent);
        }
    }

    static void drawMotorsPage(Adafruit_GFX& g, const ScreenModel& m){
        const Reply::MotionView& v = m.motion;
        bool b_live = m.b_linkUp;
        for(uint8_t wheel = 0; wheel < 4; wheel++){
            int16_t top = wheelTop[wheel];
            bool b_left = wheelLeft[wheel];
            char velocity[8];
            percentText(velocity, sizeof(velocity), v.wheelVelocity[wheel], b_live);
            const char* state = wheelStateText(m, wheel);
            //the wheels close together in the middle like the vehicle, the text outside: the state and trq beside the
            //wheel, the values right-aligned on the outer side (the left ones at x 28, the right ones at the edge)
            int16_t iconX = b_left ? 52 : 68;
            wheelIcon(g, iconX, top, rollerSlant[wheel], m.wheelPhase[wheel]);
            if(b_left){
                textRight(g, pixelFont, 48, top, state);
                textRight(g, pixelFont, 28, top, velocity);
            }
            else{
                text(g, pixelFont, 78, top, state);
                textRight(g, pixelFont, 126, top, velocity);
            }
            //the two sliders, rows top+8..top+14: a centre tick and end ticks over both, velocity then torque
            int16_t centre = b_left ? 24 : 102;
            int16_t sliderTop = top + 8;
            g.drawFastVLine(centre, sliderTop, 7, ON);
            g.drawFastVLine(centre - sliderHalf - 1, sliderTop, 7, ON);
            g.drawFastVLine(centre + sliderHalf + 1, sliderTop, 7, ON);
            if(b_live) sliderBar(g, centre, sliderTop, v.wheelVelocity[wheel], false);
            if(b_live && v.b_torqueKnown) sliderBar(g, centre, sliderTop + 4, v.wheelTorque[wheel], true);
        }
        //the alive sign in the vehicle's centre, between the wheel rows
        activeDot(g, 62, 14, m, true);
    }

    //——— the Tracking page ———

    //Permanent like Motion and Motors (no return to Status), the stopwatch's controls do not work here, no title, no
    //page dots: a top-down map round the vehicle, which stays at the screen's centre (x 64, y 16) as a filled 5 x 7
    //arrowhead pointing up, its forward; the map moves and turns round it (TrackingMap: the frame, the transform).
    //  the grid in metres, turning with the heading: a solid line every 5 m, from 4 px per m a dot on every metre
    //  crossing (drawGrid); drawn at the steadied pose (a dead band, TrackingMap::follow), so odometry jitter and slow
    //  creep move it in clean 1 px steps; the zoom steps 3, 6, 12 and 24 px per m, gliding between them over half a
    //  second (TrackingView::scale)
    //  the frame's origin (the zero, or the stage origin before one) a small x turning with the map, on a dark disc
    //  the trail: the last ~10 s as 2 x 2 dots at least 3 px apart, each on a dark 1 px halo, so they read over the
    //  grid
    //  bottom left on a dark margin: the scale, a bar of 1 m (5 m below 4 px per m) with end ticks and "1m" / "5m"
    //  top left: the alive dot
    //  top right: the compass, a needle on a dark disc pointing to the map frame's +y as seen from the rover (the
    //  forward of the zero when one is kept, else the stage's upstage), its tip a filled dot; none without a heading
    //  no pose or no motion data: no map, one centred plain line in the top row ("no link", "no motion data", "no
    //  position"), the rover still drawn
    //A dark margin of 1 px round the rover, the label, the delta, the alive dot and the line keeps the grid off them.
    static const int16_t roverLeft = 62;       //the arrowhead, x 62..66, rows 13..19: its centre column at x 64
    static const int16_t roverTop = 13;
    static const uint8_t roverBitmap[] = { 0x20, 0x20, 0x70, 0x70, 0xF8, 0xF8, 0xD8 };
    static const int16_t trackingDotX = 0;     //the alive dot, x 0..2, rows 0..2
    static const int16_t compassX = 121;       //the compass's centre, its disc radius 6: x 115..127, rows 0..12
    static const int16_t compassY = 6;
    static const int16_t scaleTextTop = 24;    //the scale label's text, rows 24..30

    static int16_t screenPixel(float v){ return int16_t(floorf(v + 0.5f)); }

    //A grid that stays calm on a 1 bit screen while it turns and creeps: the 5 m lines drawn as lines between their
    //screen end points (one staircase that keeps its shape as it moves), the metres only as a dot on each crossing
    //(a turned lattice of dots reads cleanly where dotted lines shimmer); from 4 px per m, below it the 5 m lines only
    static void drawGrid(Adafruit_GFX& g, float roverX, float roverY, float heading, float scale){
        //every line that can reach the screen: half its diagonal (66 px) round the rover
        float reach = 68.0f / scale;
        int32_t firstX = int32_t(floorf(roverX - reach)), lastX = int32_t(ceilf(roverX + reach));
        int32_t firstY = int32_t(floorf(roverY - reach)), lastY = int32_t(ceilf(roverY + reach));
        //the 5 m lines: x = k along y, y = k along x
        for(int32_t k = firstX; k <= lastX; k++){
            if(k % 5 != 0) continue;
            float x0, y0, x1, y1;
            TrackingMap::toScreen(float(k), roverY - reach, roverX, roverY, heading, scale, x0, y0);
            TrackingMap::toScreen(float(k), roverY + reach, roverX, roverY, heading, scale, x1, y1);
            g.drawLine(screenPixel(x0), screenPixel(y0), screenPixel(x1), screenPixel(y1), ON);
        }
        for(int32_t k = firstY; k <= lastY; k++){
            if(k % 5 != 0) continue;
            float x0, y0, x1, y1;
            TrackingMap::toScreen(roverX - reach, float(k), roverX, roverY, heading, scale, x0, y0);
            TrackingMap::toScreen(roverX + reach, float(k), roverX, roverY, heading, scale, x1, y1);
            g.drawLine(screenPixel(x0), screenPixel(y0), screenPixel(x1), screenPixel(y1), ON);
        }
        //the metre crossings off the 5 m lines
        if(scale < 4.0f) return;
        for(int32_t i = firstX; i <= lastX; i++){
            for(int32_t j = firstY; j <= lastY; j++){
                if(i % 5 == 0 || j % 5 == 0) continue;
                float sx, sy;
                TrackingMap::toScreen(float(i), float(j), roverX, roverY, heading, scale, sx, sy);
                int16_t px = screenPixel(sx), py = screenPixel(sy);
                if(px >= 0 && px < 128 && py >= 0 && py < 32) g.drawPixel(px, py, ON);
            }
        }
    }

    //the origin: an x of 2 px arms along the map's diagonals, on a dark disc of radius 3 (the 5 m lines cross there)
    static void originMark(Adafruit_GFX& g, float roverX, float roverY, float heading, float scale){
        float ox, oy;
        TrackingMap::toScreen(0.0f, 0.0f, roverX, roverY, heading, scale, ox, oy);
        int16_t cx = screenPixel(ox), cy = screenPixel(oy);
        if(cx < -4 || cx > 131 || cy < -4 || cy > 35) return;
        g.fillCircle(cx, cy, 3, OFF);
        //the map's diagonals on the screen: its x and y axes turned by the heading
        float t = heading * 0.017453293f;
        float ax = cosf(t), ay = sinf(t);           //the map's x axis on the screen (y down)
        float bx = sinf(t), by = -cosf(t);          //the map's y axis
        g.drawPixel(cx, cy, ON);
        for(int i = 1; i <= 2; i++){
            float d = float(i) * 0.70710678f;
            for(int sign = -1; sign <= 1; sign += 2){
                float s = float(sign) * d;
                g.drawPixel(screenPixel(ox + s * (ax + bx)), screenPixel(oy + s * (ay + by)), ON);
                g.drawPixel(screenPixel(ox + s * (ax - bx)), screenPixel(oy + s * (ay - by)), ON);
            }
        }
    }

    //the trail from the newest sample back: a 2 x 2 dot wherever it is at least trailDotSpacing px (either way) from
    //the last dot drawn (the samples lie closer than a pixel at walking speed), each on a dark 1 px halo, so it
    //reads as dots apart from the 1 px grid
    static const int16_t trailDotSpacing = 3;

    static void drawTrail(Adafruit_GFX& g, const TrackingView& v, float roverX, float roverY, float heading, float scale){
        int16_t px[TrackingView::trailCapacity], py[TrackingView::trailCapacity];
        uint8_t dots = 0;
        //where the rover is: the first dot keeps its spacing from it too
        int16_t lastX = int16_t(TrackingMap::centreX), lastY = int16_t(TrackingMap::centreY);
        for(int i = int(v.trailCount) - 1; i >= 0; i--){
            float sx, sy;
            TrackingMap::toScreen(v.trailX[i], v.trailY[i], roverX, roverY, heading, scale, sx, sy);
            //the dot's top left pixel, the dot centred on the sample
            int16_t x = int16_t(floorf(sx)), y = int16_t(floorf(sy));
            int16_t dx = int16_t(abs(x - lastX)), dy = int16_t(abs(y - lastY));
            if(dx < trailDotSpacing && dy < trailDotSpacing) continue;
            px[dots] = x;
            py[dots] = y;
            dots++;
            lastX = x;
            lastY = y;
        }
        for(uint8_t i = 0; i < dots; i++) g.fillRect(px[i] - 1, py[i] - 1, 4, 4, OFF);
        for(uint8_t i = 0; i < dots; i++) g.fillRect(px[i], py[i], 2, 2, ON);
    }

    //the scale bottom left: a bar of 1 m (5 m at 2 px per m) on row 30 with ticks at both ends (rows 28..30), the
    //text after it, on its own dark margin
    static void scaleLabel(Adafruit_GFX& g, float scale){
        int metres = scale >= 4.0f ? 1 : 5;
        int16_t length = int16_t(lroundf(float(metres) * scale));      //the bar spans length + 1 px, tick to tick
        const char* label = metres == 1 ? "1m" : "5m";
        int16_t textX = length + 3;
        int16_t right = textX + textWidth(g, pixelFont, label) - 1;
        g.fillRect(0, scaleTextTop - 1, right + 2, 32 - (scaleTextTop - 1), OFF);
        g.drawFastHLine(0, 30, length + 1, ON);
        g.drawFastVLine(0, 28, 3, ON);
        g.drawFastVLine(length, 28, 3, ON);
        text(g, pixelFont, textX, scaleTextTop, label);
    }

    //the map frame's +y seen from a rover at heading h (counter-clockwise): (sin h, -cos h) on the screen (y down)
    static void compass(Adafruit_GFX& g, float compassHeading){
        if(isnan(compassHeading)) return;
        g.fillCircle(compassX, compassY, 6, OFF);
        float t = compassHeading * 0.017453293f;
        float dx = sinf(t), dy = -cosf(t);
        int16_t tipX = screenPixel(float(compassX) + 4.0f * dx), tipY = screenPixel(float(compassY) + 4.0f * dy);
        int16_t tailX = screenPixel(float(compassX) - 3.0f * dx), tailY = screenPixel(float(compassY) - 3.0f * dy);
        g.drawLine(tailX, tailY, tipX, tipY, ON);
        g.fillCircle(tipX, tipY, 1, ON);
    }

    //——— the Tracking page's marks, timed targets and views (owner, 2026-10-28) ———

    //Small text with a few own glyphs in the 5 x 7 cell, so widths stay 6 px a character: \x01 a degree ring (the
    //font's degree is a blob), \x02 turn counter-clockwise, \x04 turn clockwise. Drawn in either colour (in a pill:
    //OFF)
    //a half circle over the top with the arrowhead at the end it turns to (left: counter-clockwise)
    static const uint8_t turnLeftBitmap[] = { 0x00, 0x70, 0x88, 0x88, 0xE8, 0x48, 0x00 };
    static const uint8_t turnRightBitmap[] = { 0x00, 0x70, 0x88, 0x88, 0xB8, 0x90, 0x00 };

    static int16_t richWidth(const char* s){ return int16_t(strlen(s) * 6 - 1); }

    //'\x05' '\x06' in rich text (two cells, 11 px): an arrow pointing at richArrowDegrees (counter-clockwise from the
    //screen's up) in the 7 x 7 square at the pair's centre: a shaft through the centre and a head of two 2 px strokes
    static float richArrowDegrees = 0.0f;
    static void richArrow(Adafruit_GFX& g, int16_t x, int16_t y, uint16_t color){
        float t = richArrowDegrees * 0.017453293f;
        float dx = -sinf(t), dy = -cosf(t);         //on the screen, y down
        float cx = float(x) + 5.0f, cy = float(y) + 3.0f;
        float tipX = cx + 3.0f * dx, tipY = cy + 3.0f * dy;
        g.drawLine(screenPixel(cx - 3.0f * dx), screenPixel(cy - 3.0f * dy), screenPixel(tipX), screenPixel(tipY), color);
        //the head: strokes from the tip back at 40 degrees either side of the shaft
        for(int side = -1; side <= 1; side += 2){
            float a = t + float(side) * 0.70f;
            float bx = sinf(a), by = cosf(a);       //backwards along the turned shaft
            g.drawLine(screenPixel(tipX), screenPixel(tipY), screenPixel(tipX + 2.4f * bx), screenPixel(tipY + 2.4f * by), color);
        }
    }

    static void richText(Adafruit_GFX& g, int16_t x, int16_t y, const char* s, uint16_t color){
        char one[2] = {0, 0};
        for(const char* c = s; *c; c++, x += 6){
            if(*c == '\x01'){
                g.drawPixel(x + 1, y, color);
                g.drawPixel(x, y + 1, color);
                g.drawPixel(x + 2, y + 1, color);
                g.drawPixel(x + 1, y + 2, color);
            }
            else if(*c == '\x02') g.drawBitmap(x, y, turnLeftBitmap, 5, 7, color);
            else if(*c == '\x04') g.drawBitmap(x, y, turnRightBitmap, 5, 7, color);
            else if(*c == '\x05') richArrow(g, x, y, color);
            else if(*c == '\x06'){}
            else{
                one[0] = *c;
                text(g, pixelFont, x, y, one, color);
            }
        }
    }

    //a filled pill with rich text at x (top y); returns its width
    static int16_t richPill(Adafruit_GFX& g, int16_t x, int16_t y, const char* s){
        int16_t w = richWidth(s) + 4;
        g.fillRoundRect(x, y, w, pillHeight, 2, ON);
        richText(g, x + 2, y + 1, s, OFF);
        return w;
    }

    //a mark's place on the screen
    static void markScreen(const Mark& mk, float roverX, float roverY, float heading, float scale, int16_t& sx, int16_t& sy){
        float fx, fy;
        TrackingMap::toScreen(mk.x(), mk.y(), roverX, roverY, heading, scale, fx, fy);
        sx = screenPixel(fx);
        sy = screenPixel(fy);
    }

    //a heading of the map frame (deg) as a screen direction seen from the rover at roverHeading (y down)
    static void screenDirection(float headingDeg, float roverHeading, float& dx, float& dy){
        float t = (headingDeg - roverHeading) * 0.017453293f;
        dx = -sinf(t);
        dy = -cosf(t);
    }

    //the marks: their number in plain digits on a dark 1 px margin, centred on the place; the selected one negative in
    //a 9 x 9 pill (Z: the letter), with a 2 x 2 dot 8 px out along its heading (none without one). Z only while it is
    //selected: otherwise the origin's x is the zero
    static void drawMarks(Adafruit_GFX& g, const ScreenModel& m, float roverX, float roverY, float heading, float scale){
        int8_t selected = m.trackingControl.selected;
        //"none" selected clears the map of marks (owner, 2026-10-28); a run's target is still drawn
        if(selected < 0 && !m.run.b_run) return;
        for(int pass = 0; pass < 2; pass++){
            for(uint8_t k = 0; k < MarkStore::count; k++){
                const Mark& mk = m.marks.get(k);
                bool b_selected = int8_t(k) == selected;
                if(!mk.b_used || (pass == 0) == b_selected) continue;
                if(k == MarkStore::zero && !b_selected) continue;
                int16_t sx, sy;
                markScreen(mk, roverX, roverY, heading, scale, sx, sy);
                if(sx < -12 || sx > 140 || sy < -12 || sy > 44) continue;
                char label[2] = {char(k == 0 ? 'Z' : '0' + k), 0};
                int16_t w = b_selected ? 9 : 5, h = b_selected ? 9 : 7;
                int16_t x = sx - w / 2, y = sy - h / 2;
                if(b_selected && mk.b_heading){
                    float dx, dy;
                    screenDirection(mk.heading(), heading, dx, dy);
                    int16_t px = screenPixel(float(sx) + 8.0f * dx - 0.5f), py = screenPixel(float(sy) + 8.0f * dy - 0.5f);
                    g.fillRect(px - 1, py - 1, 4, 4, OFF);
                    g.fillRect(px, py, 2, 2, ON);
                }
                g.fillRect(x - 1, y - 1, w + 2, h + 2, OFF);
                if(b_selected){
                    g.fillRoundRect(x, y, w, h, 2, ON);
                    text(g, pixelFont, x + 2, y + 1, label, OFF);
                }
                else text(g, pixelFont, x, y, label);
            }
        }
    }

    //the run's line: dotted (every other pixel along it) from the pose at the play to the target, over the grid
    static void dottedLine(Adafruit_GFX& g, int16_t x0, int16_t y0, int16_t x1, int16_t y1){
        int16_t dx = int16_t(abs(x1 - x0)), dy = int16_t(-abs(y1 - y0));
        int16_t sx = x0 < x1 ? 1 : -1, sy = y0 < y1 ? 1 : -1;
        int16_t err = int16_t(dx + dy);
        for(int i = 0; i < 1200; i++){
            if(i % 2 == 0 && x0 >= 0 && x0 < 128 && y0 >= 0 && y0 < 32) g.drawPixel(x0, y0, ON);
            if(x0 == x1 && y0 == y1) break;
            int16_t e2 = int16_t(2 * err);
            if(e2 >= dy){ err = int16_t(err + dy); x0 = int16_t(x0 + sx); }
            if(e2 <= dx){ err = int16_t(err + dx); y0 = int16_t(y0 + sy); }
        }
    }

    static void drawRunLine(Adafruit_GFX& g, const TrackingRunView& r, float roverX, float roverY, float heading, float scale){
        if(!r.b_run || isnan(r.startX) || isnan(r.targetX)) return;
        float x0, y0, x1, y1;
        TrackingMap::toScreen(r.startX, r.startY, roverX, roverY, heading, scale, x0, y0);
        TrackingMap::toScreen(r.targetX, r.targetY, roverX, roverY, heading, scale, x1, y1);
        //clipped coarsely: the far ends kept within a few screens
        auto clip = [](float v, float lo, float hi){ return v < lo ? lo : (v > hi ? hi : v); };
        dottedLine(g, screenPixel(clip(x0, -400, 528)), screenPixel(clip(y0, -400, 432)),
                   screenPixel(clip(x1, -400, 528)), screenPixel(clip(y1, -400, 432)));
    }

    //the ghost: where the plan is now (TargetPlan::ghostAt), a 1 px ring of radius 6 (the rover's arrowhead fits inside
    //with a pixel to spare) with a 2 px tick outside its rim along the planned heading, on a dark margin of 1 px round
    //the ring and the tick only (so the dotted line to the target runs up to the ring). On time the arrowhead sits in
    //the ring pointing at the tick: the ghost ahead of the rover means slow, behind it fast
    static const int16_t ghostRadius = 6;
    static void drawGhost(Adafruit_GFX& g, const TrackingRunView& r, float roverX, float roverY, float heading, float scale){
        if(!r.b_ghost) return;
        float fx, fy;
        TrackingMap::toScreen(r.ghostX, r.ghostY, roverX, roverY, heading, scale, fx, fy);
        int16_t cx = screenPixel(fx), cy = screenPixel(fy);
        if(cx < -10 || cx > 137 || cy < -10 || cy > 41) return;
        //the margin: the ring's band (radius 5 to 7) dark, then the tick's pixels each with a dark 3 x 3 round it
        for(int16_t rr = ghostRadius - 1; rr <= ghostRadius + 1; rr++) g.drawCircle(cx, cy, rr, OFF);
        float dx = 0.0f, dy = 0.0f;
        bool b_tick = !isnan(r.ghostHeading);
        if(b_tick){
            screenDirection(r.ghostHeading, heading, dx, dy);
            for(int d = ghostRadius + 1; d <= ghostRadius + 2; d++)
                g.fillRect(screenPixel(fx + d * dx) - 1, screenPixel(fy + d * dy) - 1, 3, 3, OFF);
        }
        g.drawCircle(cx, cy, ghostRadius, ON);
        if(!b_tick) return;
        for(int d = ghostRadius + 1; d <= ghostRadius + 2; d++) g.drawPixel(screenPixel(fx + d * dx), screenPixel(fy + d * dy), ON);
    }

    //the drive-frame arrow, bottom right on a dark disc (x 118..127, rows 22..31): only at WORLD, which way the stick
    //forward pushes the vehicle, Stacato's upstage seen from Stacato's own heading (not the display correction)
    static const int16_t driveArrowX = 123;
    static const int16_t driveArrowY = 27;
    static const int16_t driveArrowLeft = 118;
    static void driveArrow(Adafruit_GFX& g, float driveHeading){
        g.fillCircle(driveArrowX, driveArrowY, 5, OFF);
        float t = driveHeading * 0.017453293f;
        float dx = sinf(t), dy = -cosf(t);
        float tipX = driveArrowX + 3.6f * dx, tipY = driveArrowY + 3.6f * dy;
        float baseX = driveArrowX + 0.6f * dx, baseY = driveArrowY + 0.6f * dy;
        float tailX = driveArrowX - 3.4f * dx, tailY = driveArrowY - 3.4f * dy;
        g.drawLine(screenPixel(tailX), screenPixel(tailY), screenPixel(baseX), screenPixel(baseY), ON);
        g.fillTriangle(screenPixel(tipX), screenPixel(tipY),
                       screenPixel(baseX - 2.2f * dy), screenPixel(baseY + 2.2f * dx),
                       screenPixel(baseX + 2.2f * dy), screenPixel(baseY - 2.2f * dx), ON);
    }

    //the selected mark's pill, bottom centre between the scale label and the corner: its number, the distance and the
    //bearing from the rover (an arrow pointing at it, as seen from the rover facing up, and the angle off its forward)
    //and the turn to its
    //heading; shortened to fit
    static void markPill(Adafruit_GFX& g, const ScreenModel& m, int16_t left, int16_t right){
        int8_t k = m.trackingControl.selected;
        if(k < 0) return;
        //an empty slot selected (to store into it): say so, so the selection is never invisible
        if(!m.marks.isUsed(uint8_t(k))){
            char empty[12];
            snprintf(empty, sizeof(empty), "%c empty", k == 0 ? 'Z' : char('0' + k));
            int16_t w = pillWidth(g, empty);
            if(w <= right - left + 1){
                int16_t x = placeCentred(w, left, right);
                g.fillRect(x - 1, bottomTop - 1, w + 2, 32 - bottomTop + 1, OFF);
                pill(g, x, bottomTop, empty);
            }
            return;
        }
        const Mark& mk = m.marks.get(uint8_t(k));
        const Reply::MotionView& v = m.motion;
        float dx = mk.x() - v.poseX, dy = mk.y() - v.poseY;
        float distance = hypotf(dx, dy);
        float bearing = TargetPlan::wrap(TargetPlan::headingOf(dx, dy) - v.heading);
        float turn = mk.b_heading ? TargetPlan::wrap(mk.heading() - v.heading) : NAN;
        char id = k == 0 ? 'Z' : char('0' + k);
        char d[10], b[10], t[10];
        snprintf(d, sizeof(d), distance < 9.995f ? "%.2fm" : "%.1fm", distance > 99.0f ? 99.0f : distance);
        b[0] = 0;
        richArrowDegrees = bearing;
        if(distance > TargetPlan::arriveRadius) snprintf(b, sizeof(b), "\x05\x06%d\x01", int(fabsf(bearing) + 0.5f));
        t[0] = 0;
        if(!isnan(turn)) snprintf(t, sizeof(t), "%c%d\x01", turn >= 0.0f ? '\x02' : '\x04', int(fabsf(turn) + 0.5f));
        char label[40];
        int16_t room = right - left + 1;
        //shortened until it fits: the full form, the parts closer, the distance in decimetres, then without the turn,
        //then the distance alone
        char dShort[10];
        snprintf(dShort, sizeof(dShort), "%.1fm", distance > 99.0f ? 99.0f : distance);
        const char* forms[5] = {"%c %s %s %s", "%c %s %s%s", "%c %s %s%s", "%c %s %s", "%c %s"};
        for(int i = 0; i < 5; i++){
            snprintf(label, sizeof(label), forms[i], id, i == 2 ? dShort : d, i < 4 ? b : "", i < 3 ? t : "");
            //trim a trailing space left by an empty part
            size_t n = strlen(label);
            while(n > 0 && label[n - 1] == ' ') label[--n] = 0;
            if(richWidth(label) + 4 <= room) break;
        }
        int16_t w = richWidth(label) + 4;
        if(w > room) return;
        richPill(g, placeCentred(w, left, right), bottomTop, label);
    }

    //the run's bottom row (owner, 2026-10-28: the operator drives by eye, the screen only says whether to hurry): the
    //time left in plain digits bottom left ("4.2", whole seconds from 10 s); nothing more within +-0.3 s; outside it a
    //small filled pill FASTER (late) or SLOWER (early), centred between the time left and the corner, taking turns
    //with plain text every 0.5 s past 2 s; PAUSED in plain text in the pill's place while paused. After the arrival
    //only the split "+0.4s" in a filled pill, for 3 s. The turn apart, in the right corner (Pace::turn). Each on a dark
    //1 px margin
    static void runBottom(Adafruit_GFX& g, const ScreenModel& m, int16_t right){
        const TrackingRunView& r = m.run;
        int16_t left = 0;
        if(r.b_arrived){
            char split[12];
            Pace::split(split, sizeof(split), r.split);
            int16_t w = pillWidth(g, split);
            int16_t x = placeCentred(w, 0, right);
            g.fillRect(x - 1, bottomTop - 1, w + 2, 32 - bottomTop + 1, OFF);
            pill(g, x, bottomTop, split);
            return;
        }
        if(!r.b_run) return;
        if(!isnan(r.timeLeft)){
            char t[12];
            Pace::timeLeft(t, sizeof(t), r.timeLeft);
            int16_t w = textWidth(g, pixelFont, t);
            g.fillRect(0, bottomTop, w + 1, 32 - bottomTop, OFF);
            text(g, pixelFont, 0, bottomTop + 1, t);
            left = int16_t(w + 3);
        }
        //the turn, tracked apart (owner, 2026-10-28): a filled pill in the right corner with the way to turn and the lag,
        //e.g. "<turn>12°", beyond 5 deg; the translation's pill then centres in what is left
        char turn[12];
        Pace::turn(turn, sizeof(turn), r.b_paused ? NAN : r.turnError);
        if(turn[0]){
            int16_t tw = richWidth(turn) + 4;
            int16_t tx = right - tw + 1;
            g.fillRect(tx - 1, bottomTop - 1, tw + 2, 32 - bottomTop + 1, OFF);
            richPill(g, tx, bottomTop, turn);
            right = int16_t(tx - 5);       //the pills 4 px apart, the strip between them dark (no grid between pills)
            g.fillRect(right + 1, bottomTop - 1, tx - right - 1, 32 - bottomTop + 1, OFF);
        }
        const char* word = r.b_paused ? "PAUSED" : Pace::word(r.delta);
        if(!word) return;
        bool b_filled = !r.b_paused && (!Pace::blinks(r.delta) || blink(m.now, 1000, 500));
        int16_t w = pillWidth(g, word);
        int16_t x = placeCentred(w, left, right);
        g.fillRect(x - 1, bottomTop - 1, w + 2, 32 - bottomTop + 1, OFF);
        if(b_filled) pill(g, x, bottomTop, word);
        else text(g, pixelFont, x + 2, bottomTop + 1, word);
    }

    //a refused play, for a moment at the top centre in a pill over a dark margin (the compass hidden): "3 HAS NO TIME",
    //"SELECT A MARK", "ZERO HAS NO TIME", "NO POSITION", "ALREADY AT 3"
    static void refusalPill(Adafruit_GFX& g, const ScreenModel& m){
        char label[24];
        Pace::refusal(label, sizeof(label), m.run.refusal, m.run.refusalMark);
        if(!label[0]) return;
        int16_t w = pillWidth(g, label);
        int16_t x = placeCentred(w, 4, 127);
        g.fillRect(x - 1, 0, w + 2, 10, OFF);
        pill(g, x, 0, label);
    }

    //a pill that fills from the left with a hold's progress: the filled part negative, the rest plain text
    static void progressPill(Adafruit_GFX& g, int16_t x, int16_t y, const char* label, float progress){
        int16_t w = pillWidth(g, label);
        GFXcanvas1 filled(w, pillHeight), plain(w, pillHeight);
        filled.fillScreen(0);
        plain.fillScreen(0);
        filled.fillRoundRect(0, 0, w, pillHeight, 2, ON);
        text(filled, pixelFont, 2, 1, label, OFF);
        text(plain, pixelFont, 2, 1, label, ON);
        int16_t split = int16_t(float(w) * progress + 0.5f);
        for(int16_t cx = 0; cx < w; cx++){
            for(int16_t cy = 0; cy < pillHeight; cy++){
                bool b_on = cx < split ? filled.getPixel(cx, cy) : plain.getPixel(cx, cy);
                if(b_on) g.drawPixel(x + cx, y + cy, ON);
            }
        }
    }

    //a question at the top centre in a pill over a dark margin: "AT 3? HOLD L", "SAVE 3? R", "OVERWRITE 3? HOLD R" (blinking
    //until the hold starts), "CLEAR 3? HOLD L" (the same); while held the pill fills from the left with the hold
    static void questionPill(Adafruit_GFX& g, const ScreenModel& m){
        const TrackingControlView& c = m.trackingControl;
        using Q = TrackingControlView::Question;
        if(c.question == Q::NONE) return;
        char id = c.questionMark == 0 ? 'Z' : char('0' + c.questionMark);
        char label[24];
        switch(c.question){
            case Q::AT: snprintf(label, sizeof(label), "AT %c? HOLD L", id); break;
            case Q::SAVE: snprintf(label, sizeof(label), "SAVE %c? R", id); break;
            case Q::OVERWRITE: snprintf(label, sizeof(label), "OVERWRITE %c? HOLD R", id); break;
            case Q::CLEAR: snprintf(label, sizeof(label), "CLEAR %c? HOLD L", id); break;
            case Q::CLEAR_ALL: snprintf(label, sizeof(label), "CLEAR ALL? HOLD L"); break;
            default: return;
        }
        bool b_hold = c.question == Q::OVERWRITE || c.question == Q::CLEAR || c.question == Q::AT || c.question == Q::CLEAR_ALL;
        int16_t w = pillWidth(g, label);
        int16_t x = placeCentred(w, 4, 127);
        g.fillRect(x - 1, 0, w + 2, 10, OFF);
        if(b_hold && c.holdProgress > 0.0f) progressPill(g, x, 0, label, c.holdProgress);
        else if(!b_hold || blink(m.now, 500, 300)) pill(g, x, 0, label);
        else text(g, pixelFont, x + 2, 1, label);
    }

    //a mark's values for the list and the editor: x, y in m, h in deg, t in s
    static void mmText(char* out, size_t size, int32_t mm, int decimals){
        plainValue(out, size, float(mm) * 0.001f, decimals);
    }

    static void timeText(char* out, size_t size, const Mark& mk, bool b_long){
        if(!mk.b_time){ snprintf(out, size, "--"); return; }
        if(b_long){ Stopwatch::format(out, size, mk.timeMillis(), true); return; }
        //short: seconds with tenths below 100 s, m:ss from there
        if(mk.timeTenths < 1000) snprintf(out, size, "%u.%u", unsigned(mk.timeTenths / 10), unsigned(mk.timeTenths % 10));
        else Stopwatch::format(out, size, mk.timeMillis(), false);
    }

    //a value of the list: m with 1 decimal, whole metres from 10 m (at most 4 characters)
    static void listMetres(char* out, size_t size, int32_t mm){
        if(abs(mm) < 9950) plainValue(out, size, float(mm) * 0.001f, 1);
        else snprintf(out, size, "%d", int(lroundf(float(mm) * 0.001f)));
    }

    //the mark list (edit mode, left stick right from the map): three lines, the selected mark's in a pill (rows 0..8,
    //11..19, 22..30), a window round it; right-aligned columns with 3 px between them at their widest: the number,
    //x and y in m (1 decimal, whole metres from 10 m; the editor has the mm), the heading in whole deg, the time in s
    //with tenths (m:ss from 100 s); -- blank, an empty mark "--" only. The alive dot top right
    static void drawMarkList(Adafruit_GFX& g, const ScreenModel& m){
        int8_t selected = m.trackingControl.selected < 0 ? 0 : m.trackingControl.selected;
        int8_t first = int8_t(selected - 1);
        if(first < 0) first = 0;
        if(first > MarkStore::count - 3) first = MarkStore::count - 3;
        for(int8_t line = 0; line < 3; line++){
            uint8_t k = uint8_t(first + line);
            const Mark& mk = m.marks.get(k);
            int16_t top = int16_t(line * 11);
            bool b_selected = int8_t(k) == m.trackingControl.selected;
            uint16_t ink = b_selected ? OFF : ON;
            if(b_selected) g.fillRoundRect(0, top, 123, pillHeight, 2, ON);
            char id[2] = {char(k == 0 ? 'Z' : '0' + k), 0};
            text(g, pixelFont, 2, top + 1, id, ink);
            if(!mk.b_used){
                text(g, pixelFont, 16, top + 1, "--", ink);
                continue;
            }
            char v[12];
            listMetres(v, sizeof(v), mk.xMm);
            textRight(g, pixelFont, 32, top + 1, v, ink);
            listMetres(v, sizeof(v), mk.yMm);
            textRight(g, pixelFont, 58, top + 1, v, ink);
            if(mk.b_heading){
                snprintf(v, sizeof(v), "%d", int(lroundf(mk.heading())));
                textRight(g, pixelFont, 86, top + 1, v, ink);
                richText(g, 88, top + 1, "\x01", ink);
            }
            else textRight(g, pixelFont, 86, top + 1, "--", ink);
            timeText(v, sizeof(v), mk, false);
            textRight(g, pixelFont, 120, top + 1, v, ink);
        }
        activeDot(g, 125, 0, m, true);
    }

    //the trip readout (edit mode, left stick left from the map): since the last zero or snap, plain text in capitals
    //(no descenders between the lines)
    static void drawTrip(Adafruit_GFX& g, const ScreenModel& m){
        const TripView& t = m.trip;
        char a[24], b[24];
        snprintf(a, sizeof(a), t.distance < 999.5f ? "DIST %.2fm" : "DIST %.0fm", t.distance);
        snprintf(b, sizeof(b), "TURN %d\x01", int(t.turned + 0.5f));
        text(g, pixelFont, 0, 0, a);
        richText(g, 126 - richWidth(b) + 1, 0, b, ON);
        snprintf(a, sizeof(a), "TOP %.2f/s", t.top);
        char time[12];
        Stopwatch::format(time, sizeof(time), t.movingMillis, false);
        snprintf(b, sizeof(b), "MOVE %s", time);
        text(g, pixelFont, 0, 9, a);
        textRight(g, pixelFont, 126, 9, b);
        if(t.since == 0) snprintf(a, sizeof(a), "SINCE ZERO");
        else if(t.since <= 9) snprintf(a, sizeof(a), "SINCE SNAP %u", unsigned(t.since));
        else snprintf(a, sizeof(a), "SINCE POWER ON");
        text(g, pixelFont, 0, 18, a);
        activeDot(g, 124, 28, m, true);
    }

    //the number editor (edit mode, right hold): the title "MARK 3" with the keys, the four fields in two rows of
    //pills (rows 10..18, 21..29), the one being changed filled; CLEAR 3? in the title row while asked
    static void drawEditor(Adafruit_GFX& g, const ScreenModel& m){
        const TrackingControlView& c = m.trackingControl;
        const Mark& d = c.draft;
        if(c.question == TrackingControlView::Question::CLEAR) questionPill(g, m);
        else{
            char title[12];
            snprintf(title, sizeof(title), "MARK %u", unsigned(c.editMark));
            text(g, pixelFont, 0, 0, title);
            textRight(g, pixelFont, 118, 0, "R SAVE L BACK");
            activeDot(g, 124, 2, m, true);
        }
        char x[14], y[14], h[14], t[14], v[12];
        mmText(v, sizeof(v), d.xMm, 3);
        snprintf(x, sizeof(x), "x %s", v);
        mmText(v, sizeof(v), d.yMm, 3);
        snprintf(y, sizeof(y), "y %s", v);
        if(d.b_heading){
            plainValue(v, sizeof(v), d.heading(), 1);
            snprintf(h, sizeof(h), "h %s\x01", v);
        }
        else snprintf(h, sizeof(h), "h --");
        timeText(v, sizeof(v), d, true);
        snprintf(t, sizeof(t), "t %s", v);
        const char* fields[4] = {x, y, h, t};
        for(int i = 0; i < 4; i++){
            int16_t fx = i % 2 == 0 ? 0 : 64, top = i < 2 ? 10 : 21;
            if(uint8_t(c.field) == i) richPill(g, fx, top, fields[i]);
            else richText(g, fx + 2, top + 1, fields[i], ON);
        }
    }

    static void drawTrackingPage(Adafruit_GFX& g, const ScreenModel& m){
        const TrackingControlView& c = m.trackingControl;
        if(c.b_editMode && c.b_editing){
            drawEditor(g, m);
            return;
        }
        if(c.b_editMode && c.view != TrackingControlView::View::MAP){
            if(c.view == TrackingControlView::View::LIST) drawMarkList(g, m);
            else drawTrip(g, m);
            //a question shows over the view
            if(c.question != TrackingControlView::Question::NONE){
                g.fillRect(0, 0, 128, 11, OFF);
                questionPill(g, m);
            }
            return;
        }
        const Reply::MotionView& v = m.motion;
        const char* reason = nullptr;
        switch(m.motionAvailability){
            case Reply::MotionAvailability::NO_LINK: reason = "no link"; break;
            case Reply::MotionAvailability::NO_DATA: reason = "no motion data"; break;
            case Reply::MotionAvailability::LIVE:
                if(isnan(v.poseX) || isnan(v.poseY) || isnan(v.heading)) reason = "no position";
                break;
        }
        float scale = m.tracking.scale;
        //a run, or its arrival for 3 s: the bottom row is the run's (no scale label, no mark pill, no compass)
        bool b_run = (m.run.b_run || m.run.b_arrived) && !reason;
        bool b_question = c.question != TrackingControlView::Question::NONE;
        if(!reason){
            //the map at the steadied pose (TrackingMap::follow: a dead band against a shimmering grid), the pose itself
            //where none was steadied (the renders)
            const TrackingView& t = m.tracking;
            bool b_steady = !isnan(t.mapX) && !isnan(t.mapY) && !isnan(t.mapHeading);
            float x = b_steady ? t.mapX : v.poseX, y = b_steady ? t.mapY : v.poseY;
            float heading = b_steady ? t.mapHeading : v.heading;
            drawGrid(g, x, y, heading, scale);
            originMark(g, x, y, heading, scale);
            if(b_run) drawRunLine(g, m.run, x, y, heading, scale);
            drawTrail(g, m.tracking, x, y, heading, scale);
            drawMarks(g, m, x, y, heading, scale);
            if(b_run && m.run.b_run) drawGhost(g, m.run, x, y, heading, scale);
            //the bottom row: the scale (not during a run), the pace or the selected mark, the drive arrow
            bool b_arrow = !isnan(m.driveHeading);
            int16_t right = b_arrow ? driveArrowLeft - 2 : 127;
            if(b_run) runBottom(g, m, right);
            else{
                scaleLabel(g, m.tracking.scale);
                int metres = scale >= 4.0f ? 1 : 5;
                int16_t labelRight = int16_t(lroundf(float(metres) * scale)) + 3 + textWidth(g, pixelFont, metres == 1 ? "1m" : "5m") - 1;
                markPill(g, m, labelRight + 3, right);
            }
            if(b_arrow) driveArrow(g, m.driveHeading);
        }
        else{
            int16_t w = textWidth(g, pixelFont, reason);
            int16_t x = 64 - w / 2;
            g.fillRect(x - 1, 0, w + 2, 8, OFF);
            text(g, pixelFont, x, 0, reason);
        }
        //the rover, the compass (not during a run, nor while a pill takes the top) and the alive dot over their dark
        //margins
        g.fillRect(roverLeft - 1, roverTop - 1, 7, 9, OFF);
        g.drawBitmap(roverLeft, roverTop, roverBitmap, 5, 7, ON);
        if(b_question) questionPill(g, m);
        else if(b_run){}            //no top line during a run: the ghost is never hidden
        else if(m.run.refusal != RunRefusal::NONE) refusalPill(g, m);
        else compass(g, m.tracking.compassHeading);
        g.fillRect(trackingDotX, 0, 4, 4, OFF);
        activeDot(g, trackingDotX, 0, m, true);
    }

    //——— the Link page ———

    //The last 60 s on the left (x 0..59, rows 9..31, one column a second, the newest on the right, filled from the
    //right): the loss as bars from the bottom (100 % the full height, at least 1 px for any loss), the signal (the
    //weaker side, -110 dBm at the bottom to -40 dBm at the top) as a line, dark where it crosses a bar; none without
    //a link. On the right (x 64..126) the numbers now: what the remote and the receiver hear, the loss and the delay
    static const int16_t historyTop = 9;
    static const int16_t historyBottom = 31;
    static int16_t signalRow(int8_t dbm){
        float v = (float(dbm) + 110.0f) / 70.0f;
        if(v < 0.0f) v = 0.0f;
        if(v > 1.0f) v = 1.0f;
        return int16_t(historyBottom - lroundf(v * float(historyBottom - historyTop)));
    }

    static void drawLinkPage(Adafruit_GFX& g, const ScreenModel& m){
        drawPageHeader(g, m, antennaBitmap);
        const LinkHistoryView& h = m.linkHistory;
        int16_t span = historyBottom - historyTop + 1;
        int16_t barTop[LinkHistoryView::capacity];
        for(uint8_t i = 0; i < h.count; i++){
            int16_t x = int16_t(LinkHistoryView::capacity - h.count + i);
            int16_t height = h.loss[i] == 0 ? 0 : int16_t(1 + lroundf(float(h.loss[i]) / 100.0f * float(span - 1)));
            barTop[i] = int16_t(historyBottom - height + 1);
            if(height > 0) g.drawFastVLine(x, barTop[i], height, ON);
        }
        int16_t lastX = -1, lastY = 0;
        for(uint8_t i = 0; i < h.count; i++){
            int16_t x = int16_t(LinkHistoryView::capacity - h.count + i);
            if(h.dbm[i] == LinkHistoryView::noSignal){ lastX = -1; continue; }
            int16_t y = signalRow(h.dbm[i]);
            //the line joins the samples column by column (a step drawn as a vertical run)
            int16_t from = lastX >= 0 ? lastY : y;
            int16_t y0 = from < y ? from : y, y1 = from < y ? y : from;
            for(int16_t yy = y0; yy <= y1; yy++){
                bool b_inBar = yy >= barTop[i] && h.loss[i] > 0;
                g.drawPixel(x, yy, b_inBar ? OFF : ON);
            }
            lastX = x;
            lastY = y;
        }
        char value[16];
        text(g, pixelFont, 64, 9, "remote");
        if(m.b_linkUp) snprintf(value, sizeof(value), "%d", m.downDbm); else snprintf(value, sizeof(value), "--");
        textRight(g, pixelFont, 126, 9, value);
        text(g, pixelFont, 64, 17, "recv");
        if(m.b_linkUp) snprintf(value, sizeof(value), "%d", m.upDbm); else snprintf(value, sizeof(value), "--");
        textRight(g, pixelFont, 126, 17, value);
        if(m.b_linkUp){
            snprintf(value, sizeof(value), "%u%%", m.loss);
            text(g, pixelFont, 64, 25, value);
            snprintf(value, sizeof(value), "%.0fms", m.rttMs);
            textRight(g, pixelFont, 126, 25, value);
        }
        else text(g, pixelFont, 64, 25, "lost --");
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
        //the games can be opened: both sticks, pressing every 0.8 s, in the header between the title and the dots
        if(m.b_arcadeReady){
            const uint8_t* stick = m.now % 800 < 300 ? stickDownBitmap : stickUpBitmap;
            g.drawBitmap(46, 0, stick, 7, 7, ON);
            g.drawBitmap(55, 0, stick, 7, 7, ON);
            text(g, pixelFont, 65, 0, "PLAY");
        }
    }

    //both sticks held on the About page: the games load, a block bar filling with the gesture [D-157]
    static void drawArcadeLoading(Adafruit_GFX& g, const ScreenModel& m){
        const char* title = "GAME CONSOLE";
        int16_t width = textWidth(g, pixelFont, title) + 6;
        g.fillRoundRect(64 - width / 2, 0, width, 9, 2, ON);
        textCentered(g, pixelFont, 64, 1, title, OFF);
        //14 blocks of 7 x 9 px with 1 px gaps; the ones still to come a dot on the track; the newest one blinks
        const int blocks = 14;
        float filled = m.arcadeLoading * blocks;
        for(int i = 0; i < blocks; i++){
            int16_t x = 8 + i * 8;
            if(i + 1 <= int(filled)) g.fillRect(x, 11, 7, 9, ON);
            else if(i == int(filled) && m.now % 160 < 80) g.fillRect(x, 11, 7, 9, ON);
            else g.drawPixel(x + 3, 15, ON);
        }
        //the dots after the word walk
        char loading[12];
        uint8_t dots = uint8_t((m.now / 200) % 4);
        snprintf(loading, sizeof(loading), "LOADING%.*s", dots, "...");
        text(g, pixelFont, 64 - textWidth(g, pixelFont, "LOADING...") / 2, 24, loading);
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
            case Page::MOTION: drawMotionPage(g, m); break;
            case Page::MOTORS: drawMotorsPage(g, m); break;
            case Page::TRACKING: drawTrackingPage(g, m); break;
            case Page::LINK: drawLinkPage(g, m); break;
            case Page::RADIO: drawRadioPage(g, m); break;
            case Page::BATTERY: drawBatteryPage(g, m); break;
            case Page::INPUTS: drawInputsPage(g, m); break;
            case Page::ABOUT:
                if(m.arcadeLoading > 0.0f) drawArcadeLoading(g, m);
                else drawAboutPage(g, m);
                break;
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
                    if(m.pending.isRadioOff()) snprintf(value, sizeof(value), "Off");
                    else snprintf(value, sizeof(value), "%X", m.pending.channel);
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
                case SettingsMenu::Item::SOUND:
                    name = "Sound";
                    snprintf(value, sizeof(value), "%s", RemoteSounds::levelLabel(m.pending.sound));
                    b_editable = true;
                    b_changed = m.pending.sound != m.settings.sound;
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
