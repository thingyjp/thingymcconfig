#pragma once

#include <glib.h>

gboolean network_rtnetlink_init(void);
void network_rtnetlink_setipv4addr(int ifidx, const char* addrstr);
