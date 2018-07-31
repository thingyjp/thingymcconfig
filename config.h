#pragma once

#include "network_model.h"

struct config {
	struct network_config* ntwkcfg;
};

void config_init(const gchar* configpath);
void config_onnetworkconfigured(struct network_config* config);
const struct config* config_getconfig(void);
