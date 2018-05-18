#pragma once

#include <glib.h>

#define NETWORK_SSIDSTORAGELEN 33

struct network_scanresult {
	char bssid[18];
	int frequency;
	int rssi;
	char ssid[NETWORK_SSIDSTORAGELEN];
	unsigned flags;
};

struct network_config {
	char ssid[NETWORK_SSIDSTORAGELEN];
	char password[65];
};

struct network_status {
	char ssid[NETWORK_SSIDSTORAGELEN];
};

void network_init(void);
int network_start(void);
int network_stop(void);
GPtrArray* network_scan(void);
