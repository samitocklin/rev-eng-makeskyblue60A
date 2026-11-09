// Wi-Fi station: credentials in NVS, auto-reconnect, off until configured
#pragma once
#include <stdbool.h>

void net_init(void);                       // starts Wi-Fi only if credentials are stored
bool net_connected(void);
const char *net_ip(void);
const char *net_ssid(void);
int net_rssi(void);
void net_set_credentials(const char *ssid, const char *pass);  // save + (re)connect
void net_disable(void);                    // save empty + stop
