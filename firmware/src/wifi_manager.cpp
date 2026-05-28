#include "wifi_manager.h"
#include <Arduino.h>
#include <WiFi.h>
#include <Preferences.h>

#define NVS_NS "clawdmeter"
#define RECONNECT_INTERVAL_MS 15000

static char ip_buf[20] = "0.0.0.0";

void wifi_manager_init(void) {
    Preferences prefs;
    prefs.begin(NVS_NS, true);
    String ssid = prefs.getString("ssid", "");
    String pass = prefs.getString("pass", "");
    prefs.end();

    if (ssid.length() == 0) {
        Serial.println("wifi: no credentials in NVS");
        return;
    }

    WiFi.mode(WIFI_STA);
    WiFi.setSleep(true);
    WiFi.setAutoReconnect(true);
    WiFi.begin(ssid.c_str(), pass.c_str());
    Serial.printf("wifi: connecting to \"%s\"...\n", ssid.c_str());
}

void wifi_manager_tick(void) {
    static uint32_t last_check = 0;
    uint32_t now = millis();
    if (now - last_check < RECONNECT_INTERVAL_MS) return;
    last_check = now;

    if (WiFi.status() == WL_CONNECTED) {
        strlcpy(ip_buf, WiFi.localIP().toString().c_str(), sizeof(ip_buf));
    } else {
        Serial.println("wifi: not connected, reconnecting...");
        WiFi.reconnect();
    }
}

bool wifi_is_connected(void) {
    return WiFi.status() == WL_CONNECTED;
}

const char* wifi_ip_str(void) {
    if (WiFi.status() == WL_CONNECTED) {
        strlcpy(ip_buf, WiFi.localIP().toString().c_str(), sizeof(ip_buf));
    }
    return ip_buf;
}

int wifi_rssi(void) {
    return WiFi.RSSI();
}
