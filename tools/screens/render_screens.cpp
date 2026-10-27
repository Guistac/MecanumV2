//Draws the remote's screens for a set of example states into a 128 x 32 canvas and writes each as a PBM picture.
//Built and run by tools/screens/render.py on the Mac; the firmware draws the same code on the OLED.
#include <Adafruit_GFX.h>
#include <RemoteScreens.h>
#include <RemoteTransitions.h>
#include <stdio.h>
#include <string>
#include <vector>

struct Example{
    std::string name;
    ScreenModel model;
};

static ScreenModel base(){
    ScreenModel m;
    m.now = 100;
    m.channel = 3;
    m.b_linkUp = true;
    m.signal = 0.7f;
    m.rxCounter = 1;
    m.loss = 2;
    m.downDbm = -62;
    m.upDbm = -58;
    m.rttMs = 8.1f;
    m.robotVoltage = 56.3f;
    m.remotePercent = 80;
    m.remoteRestingVoltage = 3.98f;
    m.remoteVoltage = 3.95f;
    m.status = {BigMessage::ENABLED, StatusLine::NORMAL};
    m.speedState = 1;
    m.version = "7d818fd";
    m.uptimeSeconds = 5025;
    m.temperature = 34;
    m.radioFailures = 0;
    return m;
}

static void writePbm(GFXcanvas1& canvas, const std::string& path){
    FILE* f = fopen(path.c_str(), "w");
    fprintf(f, "P1\n128 32\n");
    for(int y = 0; y < 32; y++){
        for(int x = 0; x < 128; x++) fprintf(f, "%d ", canvas.getPixel(x, y) ? 1 : 0);
        fprintf(f, "\n");
    }
    fclose(f);
}

int main(int argc, char** argv){
    std::string outDir = argc > 1 ? argv[1] : ".";
    std::vector<Example> examples;
    auto add = [&](const char* name, ScreenModel m){ examples.push_back({name, m}); };

    { ScreenModel m = base(); add("01_enabled_slow", m); }
    { ScreenModel m = base(); m.status.message = BigMessage::DISABLED; m.speedState = 0; add("02_disabled_off", m); }
    { ScreenModel m = base(); m.status.message = BigMessage::ENABLING; m.speedState = 2; m.b_worldFrame = true; add("03_enabling_fast_world", m); }
    { ScreenModel m = base(); m.status = {BigMessage::ESTOP, StatusLine::NORMAL}; add("04_estop", m); }
    { ScreenModel m = base(); m.status = {BigMessage::ESTOP, StatusLine::NORMAL}; m.now = 600; add("05_estop_pulse", m); }
    { ScreenModel m = base(); m.b_linkUp = false; m.status = {BigMessage::NO_LINK, StatusLine::ESTOP_NOT_CONNECTED}; add("06_no_link", m); }
    { ScreenModel m = base(); m.b_linkUp = false; m.status = {BigMessage::RADIO_FAULT, StatusLine::RADIO_RETRY}; m.radioFailures = 3; add("07_radio_fault", m); }
    { ScreenModel m = base(); m.status = {BigMessage::ESTOP_WIRING, StatusLine::NORMAL}; add("08_estop_wiring", m); }
    { ScreenModel m = base(); m.status = {BigMessage::EXT_ESTOP, StatusLine::NORMAL}; m.b_recovery = true; add("09_ext_estop_rst", m); }
    { ScreenModel m = base(); m.status = {BigMessage::RELEASE_LEFT, StatusLine::NORMAL}; add("10_release_left", m); }
    { ScreenModel m = base(); m.status = {BigMessage::DISABLED, StatusLine::THANK_YOU}; m.speedState = 0; add("11_thank_you", m); }
    { ScreenModel m = base(); m.status = {BigMessage::BATTERY_CRITICAL, StatusLine::NORMAL}; m.remotePercent = 3; m.remoteBattery = BatteryState::CRITICAL; add("12_battery_critical", m); }
    { ScreenModel m = base(); m.b_hot = true; m.b_safeMode = true; m.status.message = BigMessage::DISABLED; add("13_hot_safe", m); }
    { ScreenModel m = base(); m.b_direct = true; m.status.message = BigMessage::STOPPING; add("14_direct_stopping", m); }
    { ScreenModel m = base(); m.status.message = BigMessage::DISABLING; m.b_rebooted = true; add("15_disabling_rebooted", m); }
    { ScreenModel m = base(); m.b_direct = true; m.b_dropout = true; m.b_linkUp = false; m.status = {BigMessage::NO_LINK, StatusLine::ESTOP_NOT_CONNECTED}; add("16_direct_dropout", m); }
    { ScreenModel m = base(); m.status.message = BigMessage::ENABLED; m.remotePercent = 12; m.remoteBattery = BatteryState::LOW_CHARGE; m.speedState = 2; m.b_worldFrame = true; add("17_enabled_battery_low", m); }
    { ScreenModel m = base(); m.status = {BigMessage::RELEASE_BOTH, StatusLine::NORMAL}; m.speedState = 0; add("18_release_both", m); }
    { ScreenModel m = base(); m.status = {BigMessage::ESTOP, StatusLine::NOT_CONNECTED}; m.b_linkUp = false; add("19_estop_not_connected", m); }
    { ScreenModel m = base(); m.page = Page::LINK; add("20_page_link", m); }
    { ScreenModel m = base(); m.page = Page::LINK; m.b_linkUp = false; m.downDbm = -103; add("20b_page_link_none", m); }
    { ScreenModel m = base(); m.page = Page::RADIO; m.b_radioUp = true; m.recoveries = 2; m.txTimeouts = 1; add("21b_page_radio_up", m); }
    { ScreenModel m = base(); m.page = Page::ABOUT; m.b_safeMode = true; m.resetCount = 3; m.resetCause = ResetCause::WATCHDOG; add("24b_page_about_safe", m); }
    { ScreenModel m = base(); m.page = Page::RADIO; m.recoveries = 1; add("21_page_radio", m); }
    { ScreenModel m = base(); m.page = Page::BATTERY; add("22_page_battery", m); }
    { ScreenModel m = base(); m.page = Page::INPUTS; m.sticks[0] = 0.5f; m.sticks[1] = -0.2f; m.b_enableButton = true; add("23_page_inputs", m); }
    { ScreenModel m = base(); m.page = Page::ABOUT; add("24_page_about", m); }
    { ScreenModel m = base(); m.page = Page::SETTINGS; m.pageCount = 7; m.speedState = 0; add("25_page_settings", m); }
    { ScreenModel m = base(); m.b_menuOpen = true; m.speedState = 0; m.pending.channel = 5; add("30_menu_channel", m); }
    { ScreenModel m = base(); m.b_menuOpen = true; m.speedState = 0; m.menuItem = SettingsMenu::Item::STICKS; add("31_menu_sticks", m); }
    { ScreenModel m = base(); m.b_menuOpen = true; m.speedState = 0; m.menuItem = SettingsMenu::Item::SAVE; m.pending.contrast = 2; m.saveProgress = 0.6f; add("32_menu_save", m); }
    { ScreenModel m = base(); m.b_menuOpen = true; m.speedState = 0; m.menuItem = SettingsMenu::Item::DIMMING; m.pending.dimming = 1; add("33_menu_dimming", m); }
    { ScreenModel m = base(); m.b_menuOpen = true; m.speedState = 0; m.menuItem = SettingsMenu::Item::EXIT; m.pending.contrast = 3; add("34_menu_exit", m); }
    { ScreenModel m = base(); m.b_menuOpen = true; m.speedState = 0; m.menuItem = SettingsMenu::Item::STICKS; m.note = "Saved"; add("35_menu_saved_note", m); }
    { ScreenModel m = base(); m.b_menuOpen = true; m.b_calibrating = true; m.raw[0] = 700; m.raw[1] = 400; add("40_calibration_release", m); }
    {
        ScreenModel m = base(); m.b_menuOpen = true; m.b_calibrating = true; m.b_circling = true; m.b_keepCircling = true;
        for(int i = 0; i < 4; i++){ m.calibration.axes[i] = {300, 510, 760}; }
        m.raw[2] = 700; m.raw[3] = 520;
        add("40b_calibration_further", m);
    }
    {
        ScreenModel m = base(); m.b_menuOpen = true; m.b_calibrating = true; m.b_circling = true; m.b_calibrationComplete = true;
        for(int i = 0; i < 4; i++){ m.calibration.axes[i] = {60, 510, 990}; }
        m.calibration.axes[1] = {120, 520, 870};
        m.raw[0] = 900; m.raw[1] = 300; m.raw[2] = 510; m.raw[3] = 990;
        add("41_calibration_circle", m);
    }
    {
        ScreenModel m = base(); m.b_calibrationResult = true;
        for(int i = 0; i < 4; i++){ m.calibration.axes[i] = {60, 510, 990}; }
        m.calibration.axes[1] = {120, 540, 870};
        add("42_calibration_result", m);
    }

    for(auto& example : examples){
        GFXcanvas1 canvas(128, 32);
        canvas.fillScreen(0);
        RemoteUi::draw(canvas, example.model);
        writePbm(canvas, outDir + "/" + example.name + ".pbm");
    }
    //transitions: one frame per 20 ms (the display's 50 Hz) over the 160 ms of a page change, forward then back
    //(t_<idea>_<f|b><frame>.pbm)
    struct Idea{
        const char* name;
        RemoteTransitions::Header header;
        RemoteTransitions::Content content;
        int headerDrawing;      //0: none, 1: split-flap title, 2: title swaps halfway and the dot glides, 3: only the dot glides
        bool b_fromStatus;
    };
    const Idea ideas[] = {
        {"0_now_all_slides", RemoteTransitions::Header::SLIDE, RemoteTransitions::Content::SLIDE, 0, false},
        {"A_header_rolls", RemoteTransitions::Header::ROLL, RemoteTransitions::Content::SLIDE, 0, false},
        {"A2_header_rolls_from_main", RemoteTransitions::Header::ROLL, RemoteTransitions::Content::SLIDE, 0, true},
        {"B_header_slides_content_dissolves", RemoteTransitions::Header::SLIDE, RemoteTransitions::Content::DISSOLVE, 0, false},
        {"C_header_lags", RemoteTransitions::Header::LAG, RemoteTransitions::Content::SLIDE, 0, false},
        {"D_split_flap_title", RemoteTransitions::Header::DRAWN, RemoteTransitions::Content::SLIDE, 1, false},
        {"E_header_still_dot_glides", RemoteTransitions::Header::DRAWN, RemoteTransitions::Content::SLIDE, 2, false},
        {"AE_header_rolls_dot_glides", RemoteTransitions::Header::ROLL, RemoteTransitions::Content::SLIDE, 3, false},
        {"AE2_from_main", RemoteTransitions::Header::ROLL, RemoteTransitions::Content::SLIDE, 3, true},
    };
    const int frames = 9;
    for(const Idea& idea : ideas) for(int direction = 0; direction < 2; direction++){
        bool b_forward = direction == 0;
        ScreenModel first = base();
        first.page = idea.b_fromStatus ? Page::STATUS : Page::LINK;
        ScreenModel second = base();
        second.page = idea.b_fromStatus ? Page::LINK : Page::RADIO;
        ScreenModel fromModel = b_forward ? first : second;
        ScreenModel toModel = b_forward ? second : first;
        GFXcanvas1 from(128, 32), to(128, 32), header(128, 32), out(128, 32);
        from.fillScreen(0);
        RemoteUi::draw(from, fromModel);
        for(int frame = 0; frame < frames; frame++){
            float progress = RemoteTransitions::easeOut(frame / float(frames - 1));
            to.fillScreen(0);
            RemoteUi::draw(to, toModel);
            if(idea.headerDrawing){
                //the header drawn for this frame, over the new frame's top rows
                ScreenModel headerModel = toModel;
                char title[16];
                if(idea.headerDrawing == 1){
                    RemoteTransitions::splitFlap(title, sizeof(title), RemoteUi::pageTitle(fromModel.page), RemoteUi::pageTitle(toModel.page), progress);
                    headerModel.titleOverride = title;
                }
                else{
                    if(idea.headerDrawing == 2 && progress < 0.5f) headerModel.page = fromModel.page;
                    if(fromModel.page != Page::STATUS)
                        headerModel.pageDotPosition = float(fromModel.page) + (float(toModel.page) - float(fromModel.page)) * progress;
                }
                header.fillScreen(0);
                RemoteUi::draw(header, headerModel);
                for(int y = 0; y < RemoteTransitions::headerRows; y++){
                    for(int x = 0; x < 128; x++) to.drawPixel(x, y, header.getPixel(x, y));
                }
            }
            //A + E: the dots glide in the drawn frame while the rest of the header rolls (from the main screen, which has
            //no dots, everything rolls)
            int16_t drawnFromX = 128;
            if(idea.headerDrawing == 3 && fromModel.page != Page::STATUS && toModel.page != Page::STATUS)
                drawnFromX = RemoteTransitions::pageDotsLeft(toModel.pageCount);
            RemoteTransitions::blend(out, from, to, idea.header, idea.content, progress, b_forward, drawnFromX);
            char name[64];
            snprintf(name, sizeof(name), "/t_%s_%c%d.pbm", idea.name, b_forward ? 'f' : 'b', frame);
            writePbm(out, outDir + name);
        }
    }

    //the alive sign: 24 frames each of the main screen (state dot) and a page (page dot), a GIF at 50 Hz each
    //(g_<name>_<frame>.pbm)
    for(int frame = 0; frame < 24; frame++){
        for(int page = 0; page < 3; page++){
            ScreenModel m = base();
            m.frame = frame;
            m.rxCounter = frame / 2;
            if(page == 1) m.status.message = BigMessage::DISABLED;
            if(page == 2) m.page = Page::BATTERY;
            GFXcanvas1 canvas(128, 32);
            canvas.fillScreen(0);
            RemoteUi::draw(canvas, m);
            char name[64];
            snprintf(name, sizeof(name), "/g_alive_%s_%d.pbm", page == 0 ? "enabled" : (page == 1 ? "disabled" : "page"), frame);
            writePbm(canvas, outDir + name);
        }
    }

    printf("%zu screens\n", examples.size());
    return 0;
}
