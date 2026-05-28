#include "config.h"
#include <Arduino.h>
#include <Preferences.h>
#include <WiFi.h>
#include <DNSServer.h>
#include <WebServer.h>
#include <lvgl.h>

#define NVS_NS "clawdmeter"

static char endpoint_buf[128] = {};

static const char SETUP_HTML[] PROGMEM = R"(<!DOCTYPE html>
<html>
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Clawdmeter Setup</title>
<style>
  body{font-family:sans-serif;max-width:420px;margin:40px auto;padding:0 20px;background:#111;color:#eee}
  h2{color:#D97706}
  label{display:block;margin:14px 0 4px;font-size:14px;color:#aaa}
  input{width:100%;padding:10px;background:#222;border:1px solid #444;color:#eee;border-radius:4px;box-sizing:border-box;font-size:15px}
  button{width:100%;padding:12px;margin-top:20px;background:#D97706;color:#000;border:none;border-radius:6px;font-size:16px;font-weight:bold;cursor:pointer}
</style>
</head>
<body>
<h2>Clawdmeter Setup</h2>
<form method="POST" action="/save">
  <label>WiFi SSID</label>
  <input name="ssid" type="text" required autocomplete="off">
  <label>WiFi Password</label>
  <input name="pass" type="password" autocomplete="off">
  <label>Usage endpoint URL</label>
  <input name="url" type="text" placeholder="http://192.168.x.x:1234/api/usage" required>
  <button type="submit">Save &amp; Reboot</button>
</form>
</body>
</html>)";

static const char SAVED_HTML[] PROGMEM = R"(<!DOCTYPE html>
<html>
<head><meta charset="utf-8"><title>Saved</title>
<style>body{font-family:sans-serif;max-width:420px;margin:40px auto;padding:0 20px;background:#111;color:#eee}
h2{color:#22c55e}</style></head>
<body><h2>Saved! Rebooting...</h2><p>Clawdmeter will now connect to your WiFi network.</p></body>
</html>)";

static DNSServer  dns;
static WebServer  web(80);

static void handle_root() {
    web.send_P(200, "text/html", SETUP_HTML);
}

static void handle_save() {
    String ssid = web.arg("ssid");
    String pass = web.arg("pass");
    String url  = web.arg("url");

    if (ssid.length() == 0 || url.length() == 0) {
        web.send(400, "text/plain", "Missing ssid or url");
        return;
    }

    Preferences prefs;
    prefs.begin(NVS_NS, false);
    prefs.putString("ssid", ssid);
    prefs.putString("pass", pass);
    prefs.putString("url",  url);
    prefs.end();

    web.send_P(200, "text/html", SAVED_HTML);
    delay(1500);
    ESP.restart();
}

static void run_portal(void) {
    Serial.println("config: no credentials — starting AP+captive portal");

    WiFi.mode(WIFI_AP);
    WiFi.softAP("Clawdmeter-Setup");
    delay(200);

    IPAddress ap_ip = WiFi.softAPIP();
    Serial.printf("config: AP IP %s — connect and browse to configure\n", ap_ip.toString().c_str());

    dns.start(53, "*", ap_ip);

    web.on("/save", HTTP_POST, handle_save);
    web.onNotFound(handle_root);
    web.begin();

    // Block until user saves config (handle_save reboots the device).
    while (true) {
        dns.processNextRequest();
        web.handleClient();
        lv_timer_handler();
        delay(5);
    }
}

void config_init(void) {
    Preferences prefs;
    prefs.begin(NVS_NS, true);  // read-only
    String ssid = prefs.getString("ssid", "");
    String url  = prefs.getString("url",  "");
    prefs.end();

    if (ssid.length() > 0 && url.length() > 0) {
        strlcpy(endpoint_buf, url.c_str(), sizeof(endpoint_buf));
        Serial.printf("config: provisioned, endpoint=%s\n", endpoint_buf);
        return;
    }

    run_portal();  // never returns
}

const char* config_get_endpoint(void) {
    return endpoint_buf;
}

// ---- STA config server ----

static void handle_sta_root() {
    // Dynamically inject current endpoint URL so the field is pre-filled.
    String page = F("<!DOCTYPE html>"
        "<html><head>"
        "<meta charset='utf-8'>"
        "<meta name='viewport' content='width=device-width,initial-scale=1'>"
        "<title>Clawdmeter Config</title>"
        "<style>"
        "body{font-family:sans-serif;max-width:460px;margin:40px auto;padding:0 20px;background:#111;color:#eee}"
        "h2{color:#D97706}h3{color:#aaa;font-weight:normal;margin-top:30px}"
        "label{display:block;margin:12px 0 4px;font-size:13px;color:#aaa}"
        "input{width:100%;padding:9px;background:#222;border:1px solid #444;color:#eee;"
              "border-radius:4px;box-sizing:border-box;font-size:14px}"
        ".btn{width:100%;padding:11px;margin-top:16px;border:none;border-radius:5px;"
             "font-size:15px;font-weight:bold;cursor:pointer}"
        ".btn-amber{background:#D97706;color:#000}"
        ".btn-red{background:#dc2626;color:#fff}"
        "hr{border:none;border-top:1px solid #333;margin:28px 0}"
        ".note{font-size:12px;color:#666;margin-top:6px}"
        "</style></head><body>"
        "<h2>Clawdmeter Config</h2>"

        "<h3>API endpoint</h3>"
        "<form method='POST' action='/save-url'>"
        "<label>Endpoint URL</label>"
        "<input name='url' type='text' value='");
    page += endpoint_buf;
    page += F("' required>"
        "<div class='note'>Takes effect immediately — no reboot needed.</div>"
        "<button class='btn btn-amber' type='submit'>Save endpoint</button>"
        "</form>"

        "<hr>"
        "<h3>WiFi credentials</h3>"
        "<form method='POST' action='/save-wifi'>"
        "<label>SSID</label>"
        "<input name='ssid' type='text' required autocomplete='off'>"
        "<label>Password</label>"
        "<input name='pass' type='password' autocomplete='off'>"
        "<div class='note'>Device will reboot and reconnect.</div>"
        "<button class='btn btn-red' type='submit'>Save WiFi &amp; reboot</button>"
        "</form>"
        "</body></html>");

    web.send(200, "text/html", page);
}

static void handle_sta_save_url() {
    String url = web.arg("url");
    if (url.length() == 0) {
        web.send(400, "text/plain", "Missing url");
        return;
    }

    Preferences prefs;
    prefs.begin(NVS_NS, false);
    prefs.putString("url", url);
    prefs.end();
    strlcpy(endpoint_buf, url.c_str(), sizeof(endpoint_buf));

    Serial.printf("config: endpoint updated to %s\n", endpoint_buf);

    String resp = F("<!DOCTYPE html><html><head><meta charset='utf-8'>"
        "<meta http-equiv='refresh' content='2;url=/'>"
        "<style>body{font-family:sans-serif;max-width:460px;margin:40px auto;padding:0 20px;"
        "background:#111;color:#eee}h2{color:#22c55e}</style></head>"
        "<body><h2>Saved!</h2><p>Endpoint updated. Redirecting...</p></body></html>");
    web.send(200, "text/html", resp);
}

static void handle_sta_save_wifi() {
    String ssid = web.arg("ssid");
    String pass = web.arg("pass");
    if (ssid.length() == 0) {
        web.send(400, "text/plain", "Missing ssid");
        return;
    }

    Preferences prefs;
    prefs.begin(NVS_NS, false);
    prefs.putString("ssid", ssid);
    prefs.putString("pass", pass);
    prefs.end();

    String resp = F("<!DOCTYPE html><html><head><meta charset='utf-8'>"
        "<style>body{font-family:sans-serif;max-width:460px;margin:40px auto;padding:0 20px;"
        "background:#111;color:#eee}h2{color:#22c55e}</style></head>"
        "<body><h2>Saved! Rebooting...</h2></body></html>");
    web.send(200, "text/html", resp);
    delay(1000);
    ESP.restart();
}

void config_server_init(void) {
    web.on("/",          HTTP_GET,  handle_sta_root);
    web.on("/save-url",  HTTP_POST, handle_sta_save_url);
    web.on("/save-wifi", HTTP_POST, handle_sta_save_wifi);
    web.onNotFound([]() { web.send(404, "text/plain", "Not found"); });
    web.begin();
    Serial.printf("config: server at http://%s/\n", WiFi.localIP().toString().c_str());
}

void config_server_tick(void) {
    web.handleClient();
}
