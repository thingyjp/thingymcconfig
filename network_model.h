#pragma once

#include <json-glib/json-glib.h>

#define NETWORK_SSIDSTORAGELEN 33
#define NETWORK_PASSWORDSTORANGELEN 65

typedef enum {
	NF_ESS = 1, //
	NF_WPS = 1 << 1, //
	NF_WEP = 1 << 2, //
	NF_WPA_PSK_CCMP = 1 << 3, //
	NF_WPA_PSK_CCMP_TKIP = 1 << 4, //
	NF_WPA2_PSK_CCMP = 1 << 5, //
	NF_WPA2_PSK_CCMP_TKIP = 1 << 6
} network_flags;

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

struct network_config* network_model_config_deserialise(JsonNode* root);
void network_model_config_serialise(struct network_config* config,
		JsonBuilder* jsonbuilder);
