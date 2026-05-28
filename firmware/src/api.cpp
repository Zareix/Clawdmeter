#include "api.h"
#include "config.h"
#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

static bool          last_ok       = false;
static unsigned long last_fetch_ms = 0;

void api_init(void) {
    Serial.printf("api: endpoint = %s\n", config_get_endpoint());
}

bool api_fetch(UsageData* out) {
    const char* url = config_get_endpoint();
    if (!url || url[0] == '\0') {
        Serial.println("api: ERROR no endpoint configured");
        last_ok = false;
        last_fetch_ms = millis();
        return false;
    }

    if (WiFi.status() != WL_CONNECTED) {
        // Don't update last_ok / last_fetch_ms — WiFi not ready is not a fetch failure
        Serial.println("api: skip fetch, WiFi not connected");
        return false;
    }

    Serial.printf("api: fetching %s\n", url);

    HTTPClient http;
    http.begin(url);
    http.setTimeout(5000);
    int code = http.GET();

    if (code <= 0) {
        // code <= 0 means connection-level error (timeout, refused, DNS fail…)
        Serial.printf("api: ERROR connection failed (HTTPClient error %d: %s)\n",
                      code, http.errorToString(code).c_str());
        http.end();
        last_ok = false;
        last_fetch_ms = millis();
        return false;
    }

    if (code != 200) {
        String body = http.getString();
        Serial.printf("api: ERROR HTTP %d — body: %s\n", code, body.substring(0, 120).c_str());
        http.end();
        last_ok = false;
        last_fetch_ms = millis();
        return false;
    }

    String body = http.getString();
    http.end();

    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, body);
    if (err) {
        Serial.printf("api: ERROR JSON parse failed (%s) — body: %s\n",
                      err.c_str(), body.substring(0, 120).c_str());
        last_ok = false;
        last_fetch_ms = millis();
        return false;
    }

    out->session_pct        = doc["usagePercent5h"] | 0.0f;
    out->session_reset_secs = doc["resetIn5h"]      | -1;
    out->weekly_pct         = doc["usagePercent7d"] | 0.0f;
    out->weekly_reset_secs  = doc["resetIn7d"]      | -1;
    strlcpy(out->status, doc["status"] | "unknown", sizeof(out->status));
    out->ok    = doc["ok"] | false;
    out->valid = true;

    last_ok = true;
    last_fetch_ms = millis();
    Serial.printf("api: OK s=%.1f%% w=%.1f%% status=%s\n",
                  out->session_pct, out->weekly_pct, out->status);
    return true;
}

bool api_last_ok(void) {
    return last_ok;
}

unsigned long api_last_fetch_ms(void) {
    return last_fetch_ms;
}
