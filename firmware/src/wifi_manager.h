#pragma once

void wifi_manager_init(void);
void wifi_manager_tick(void);
bool wifi_is_connected(void);
const char* wifi_ip_str(void);
int wifi_rssi(void);
