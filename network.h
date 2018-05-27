#pragma once

#include <json-glib/json-glib.h>

#define NETWORK_SSIDSTORAGELEN 33
#define NETWORK_PASSWORDSTORANGELEN 65

struct network_scanresult {
	char bssid[18];
	int frequency;
	int rssi;
	char ssid[NETWORK_SSIDSTORAGELEN];
	unsigned flags;
};

struct network_config {
	char ssid[NETWORK_SSIDSTORAGELEN];
	char psk[NETWORK_PASSWORDSTORANGELEN];
};

struct network_status {
	char ssid[NETWORK_SSIDSTORAGELEN];
};

void network_init(void);
int network_start(void);
int network_stop(void);
GPtrArray* network_scan(void);
void network_addnetwork(struct network_config* ntwkcfg);
struct network_config* network_parseconfig(JsonNode* root);

int network_startap(void);
int network_stopap(void);
