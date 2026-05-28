#pragma once
#include <Arduino.h>

struct UsageData {
    float session_pct;
    int session_reset_secs;  // seconds until 5h window resets (-1 if unknown)
    float weekly_pct;
    int weekly_reset_secs;   // seconds until 7d window resets (-1 if unknown)
    char status[16];         // "allowed" or "limited"
    bool ok;                 // data fetch succeeded
    bool valid;              // false until first successful fetch
};
