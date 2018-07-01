#pragma once

#include <glib.h>
#include <gio/gio.h>
#include "dhcp4.h"

#define DHCP4_CLIENT_MAXNAMESERVERS	4

enum dhcp4_clientstate {
	DHCP4CS_IDLE, DHCP4CS_DISCOVERING, DHCP4CS_REQUESTING, DHCP4CS_CONFIGURED
};

struct dhcp4_client_lease {
	guint8 serverip[DHCP4_ADDRESS_LEN];
	guint8 leasedip[DHCP4_ADDRESS_LEN];
	guint8 subnetmask[DHCP4_ADDRESS_LEN];
	guint8 defaultgw[DHCP4_ADDRESS_LEN];
	guint8 nameservers[DHCP4_CLIENT_MAXNAMESERVERS][DHCP4_ADDRESS_LEN];
	guint32 leasetime;
};

struct dhcp4_client_cntx {
	enum dhcp4_clientstate state;
	unsigned ifidx;
	guint8 mac[6];

	GRand* rand;
	int rawsocket;
	guint32 xid;
	struct dhcp4_client_lease* pendinglease;
	struct dhcp4_client_lease* currentlease;
};

struct dhcp4_client_cntx* dhcp4_client_new(unsigned ifidx, const guint8* mac);
void dhcp4_client_start(struct dhcp4_client_cntx* cntx);
void dhcp4_client_stop(struct dhcp4_client_cntx* cntx);
