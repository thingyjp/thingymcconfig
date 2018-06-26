#pragma once

#include <json-glib/json-glib.h>

#include "network_model.h"

struct network_config {
	char ssid[NETWORK_SSIDSTORAGELEN];
	char psk[NETWORK_PASSWORDSTORANGELEN];
};

struct network_status {
	char ssid[NETWORK_SSIDSTORAGELEN];
};

gboolean network_init(const char* interface, gboolean noap);
int network_waitforinterface(void);
int network_start(void);
int network_stop(void);
GPtrArray* network_scan(void);
void network_addnetwork(struct network_config* ntwkcfg);
struct network_config* network_parseconfig(JsonNode* root);

int network_startap(void);
int network_stopap(void);

void network_dumpstatus(JsonBuilder* builder);
