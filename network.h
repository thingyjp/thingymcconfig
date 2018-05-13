#pragma once

#include <glib.h>

struct network_scanresult {
	char bssid[18];
	int frequency;
	int rssi;
	char ssid[33];
	unsigned flags;
};

void network_init(void);
int network_start(void);
int network_stop(void);
GPtrArray* network_scan(void);
