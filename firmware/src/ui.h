#pragma once
#include "data.h"

enum screen_t {
    SCREEN_SPLASH,
    SCREEN_USAGE,
    SCREEN_WIFI,
    SCREEN_COUNT,
};

void ui_init(void);
void ui_update(const UsageData* data);
void ui_tick_anim(void);
void ui_show_screen(screen_t screen);
void ui_cycle_screen(void);
void ui_toggle_splash(void);
screen_t ui_get_current_screen(void);
void ui_update_wifi_status(bool connected, const char* ip, bool api_ok, bool api_ever);
void ui_update_battery(int percent, bool charging);
void ui_update_time(const char* time_str);
