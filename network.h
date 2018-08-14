#pragma once

#include <glib.h>

#include "network_model.h"

struct network_status {
	char ssid[NETWORK_SSIDSTORAGELEN];
};

gboolean network_init(const char* interface, gboolean noap);
int network_waitforinterface(void);
gboolean network_start(void);
int network_stop(void);
GPtrArray* network_scan(void);
gboolean network_configure(struct network_config* ntwkcfg);
int network_startap(const gchar* nameprefix);
int network_stopap(void);
void network_dumpstatus(JsonBuilder* builder);
gboolean network_ctrl_sendstate(GOutputStream* os);
