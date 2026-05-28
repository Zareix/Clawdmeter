#pragma once

// Check NVS for stored WiFi + endpoint credentials.
// If missing: start AP "Clawdmeter-Setup" + captive portal HTTP server and block
// forever (device reboots after user saves config via browser form).
// Must be called after LVGL + ui_init() so lv_timer_handler() can run in the portal loop.
// Returns normally only when credentials already exist in NVS.
void config_init(void);

const char* config_get_endpoint(void);

// Start config web server on port 80 in STA mode.
// Call after WiFi is up. Serves config panel at http://<device-ip>/
void config_server_init(void);

// Must be called every loop iteration to process HTTP requests.
void config_server_tick(void);
