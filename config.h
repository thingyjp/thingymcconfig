#pragma once

#include "network_model.h"

struct config {
	struct network_config* ntwkcfg;
};

void config_init(void);

void config_onnetworkconfigured(struct network_config* config);
