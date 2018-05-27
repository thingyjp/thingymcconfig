#pragma once

struct config {
	const gchar* interface;
	gboolean provisioned;
};

void config_init(void);
