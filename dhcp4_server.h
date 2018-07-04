#pragma once

#include <glib.h>
#include <net/ethernet.h>
#include "dhcp4.h"

struct dhcp4_server_lease {
	guint8 mac[ETHER_ADDR_LEN];
	GSList* address;
	gint64 leasedat;
	gint32 leasetime;
};

struct dhcp4_server_cntx {
	unsigned ifidx;
	int packetsocket;
	guint packetsocketsource;
	guint8 mac[ETHER_ADDR_LEN];
	guint8 serverid[DHCP4_ADDRESS_LEN];
	guint8 subnetmask[DHCP4_ADDRESS_LEN];
	guint8 defaultgw[DHCP4_ADDRESS_LEN];
	GSList* pool;
	GSList* leases;
};

struct dhcp4_server_cntx* dhcp4_server_new(unsigned ifidx, const guint8* mac,
		const guint8* serverid, const guint8* poolstart, unsigned poollen,
		const guint8* subnetmask, const guint8* defaultgw);
void dhcp4_server_start(struct dhcp4_server_cntx* cntx);
void dhcp4_server_stop(struct dhcp4_server_cntx* cntx);
void dhcp4_server_free(struct dhcp4_server_cntx* cntx);
