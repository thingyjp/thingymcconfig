#pragma once

#include <net/ethernet.h>
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
	guint8 numnameservers;
	guint32 leasetime;
};

struct dhcp4_client_cntx {
	enum dhcp4_clientstate state;
	unsigned ifidx;
	guint8 mac[ETHER_ADDR_LEN];

	GRand* rand;
	int rawsocket;
	guint rawsocketeventsource;

	guint32 xid;
	struct dhcp4_client_lease* pendinglease;
	struct dhcp4_client_lease* currentlease;

	gboolean paused;
};

struct dhcp4_client_cntx* dhcp4_client_new(unsigned ifidx, const guint8* mac);
void dhcp4_client_start(struct dhcp4_client_cntx* cntx);
void dhcp4_client_pause(struct dhcp4_client_cntx* ctnx);
void dhcp4_client_resume(struct dhcp4_client_cntx* cntx);
void dhcp4_client_stop(struct dhcp4_client_cntx* cntx);

/* The intention here is to not make this stuff too dependent on the rest
 * of the code so if it turns out to be useful somewhere else in the future
 * it's easy to split out. That said here are some external functions that
 * need to be provided elsewhere
 */

void _dhcp4_client_clearinterface(unsigned ifidx);

void _dhcp4_client_configureinterface(unsigned ifidx, const guint8* address,
		const guint8* netmask, const guint8* gateway, const guint8* nameservers,
		const guint8 numnameservers);
