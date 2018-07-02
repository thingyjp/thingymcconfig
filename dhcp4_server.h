#pragma once

#include <net/ethernet.h>

struct dhcp4_server_cntx {
	unsigned ifidx;
	int packetsocket;
	guint packetsocketsource;
	guint8 mac[ETHER_ADDR_LEN];
};

struct dhcp4_server_cntx* dhcp4_server_new(unsigned ifidx, const guint8* mac);
void dhcp4_server_start(struct dhcp4_server_cntx* cntx);
void dhcp4_server_stop(struct dhcp4_server_cntx* cntx);
void dhcp4_server_full(struct dhcp4_server_cntx* cntx);
