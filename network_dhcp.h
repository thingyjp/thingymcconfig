#pragma once

#include <json-glib/json-glib.h>

void network_dhcpclient_start(unsigned ifidx, const gchar* interfacename,
		const guint8* interfacemac);
void network_dhcpclient_stop(void);
void network_dhcpserver_start(const gchar* interfacename);
void network_dhcpserver_stop(void);
void network_dhcp_dumpstatus(JsonBuilder* builder);
