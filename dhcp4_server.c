#include <glib.h>
#include <arpa/inet.h>
#include "dhcp4.h"
#include "dhcp4_server.h"
#include "dhcp4_model.h"
#include "packetsocket.h"
#include "ip4.h"

#define LEASETIME ((5 * 60) * 1000000)
#define EXPIRED (60 * 1000000)
#define MACFMT "%02x:%02x:%02x:%02x:%02x:%02x"
#define MACARG(a) (unsigned)a[0],(unsigned)a[1],(unsigned)a[2],(unsigned)a[3],(unsigned)a[4],(unsigned)a[5]

struct dhcp4_server_cntx* dhcp4_server_new(unsigned ifidx, const guint8* mac,
		const guint8* serverid, const guint8* poolstart, unsigned poollen,
		const guint8* subnetmask, const guint8* defaultgw) {
	struct dhcp4_server_cntx* cntx = g_malloc0(sizeof(*cntx));
	cntx->ifidx = ifidx;
	memcpy(cntx->mac, mac, sizeof(cntx->mac));

	memcpy(cntx->serverid, serverid, sizeof(cntx->serverid));

	guint32 poolstartaddr = ntohl(*((guint32*) poolstart));
	for (int i = 0; i < poollen; i++) {
		guint32 addr = htonl(poolstartaddr + i);
		cntx->pool = g_slist_append(cntx->pool, GUINT_TO_POINTER(addr));
	}
	cntx->pool = g_slist_reverse(cntx->pool);

	memcpy(cntx->subnetmask, subnetmask, sizeof(cntx->subnetmask));
	memcpy(cntx->defaultgw, defaultgw, sizeof(cntx->defaultgw));
	return cntx;
}

#define POOLADDRTOPTR(pooladdr) guint32 addr = GPOINTER_TO_UINT(pooladdr->data);\
								guint8* a = (guint8*)&addr

static struct dhcp4_server_lease* dhcp4_server_lease_new(
		struct dhcp4_server_cntx* cntx, guint8* mac) {

	GSList* pooladdr = g_slist_last(cntx->pool);
	if (pooladdr != NULL) {
		cntx->pool = g_slist_remove_link(cntx->pool, pooladdr);
		POOLADDRTOPTR(pooladdr);
		g_message("using "IP4_ADDRFMT" from pool", IP4_ARGS(a));
		struct dhcp4_server_lease* lease = g_malloc0(sizeof(*lease));
		lease->address = pooladdr;
		memcpy(lease->mac, mac, sizeof(lease->mac));

		lease->leasedat = g_get_monotonic_time();
		lease->leasetime = LEASETIME;

		return lease;
	}
	return NULL;
}

static gint64 dhcp4_server_lease_remaining(struct dhcp4_server_lease* lease) {
	gint64 leaseduration = g_get_monotonic_time() - lease->leasedat;
	gint64 leaseremaining = lease->leasetime - leaseduration;
	return leaseremaining;
}

static void dhcp4_server_lease_extend(struct dhcp4_server_lease* lease) {
	g_message("extending lease for "MACFMT, MACARG(lease->mac));
	lease->leasedat = g_get_monotonic_time();
	lease->leasetime = LEASETIME;
}

static void dhcp4_server_lease_free(struct dhcp4_server_lease* lease) {
	g_free(lease);
}

static void dhcp4_server_fillinleaseoptions(struct dhcp4_server_cntx* cntx,
		struct dhcp4_pktcntx* pkt, struct dhcp4_server_lease* lease) {
	guint32 leasetime = dhcp4_server_lease_remaining(lease) / 1000000;

	dhcp4_model_pkt_set_subnetmask(pkt, cntx->subnetmask);
	dhcp4_model_pkt_set_defaultgw(pkt, cntx->defaultgw);
	dhcp4_model_pkt_set_leasetime(pkt, leasetime);
	dhcp4_model_pkt_set_domainnameservers(pkt, NULL, 1);
	dhcp4_model_pkt_set_serverid(pkt, cntx->serverid);
}

static void dhcp4_server_send_offer(struct dhcp4_server_cntx* cntx, guint32 xid,
		struct dhcp4_server_lease* lease) {
	struct dhcp4_pktcntx* pkt = dhcp4_model_pkt_new();
	POOLADDRTOPTR(lease->address);
	dhcp4_model_fillheader(TRUE, pkt->header, xid, a, cntx->serverid,
			lease->mac);
	dhcp4_model_pkt_set_dhcpmessagetype(pkt, DHCP4_DHCPMESSAGETYPE_OFFER);
	dhcp4_server_fillinleaseoptions(cntx, pkt, lease);
	gsize pktsz;
	guint8* pktbytes = dhcp4_model_pkt_freetobytes(pkt, &pktsz);
	packetsocket_send_udp(cntx->packetsocket, cntx->ifidx,
			*((guint32*) cntx->serverid), DHCP4_PORT_SERVER,
			DHCP4_PORT_CLIENT, pktbytes, pktsz);
	g_free(pktbytes);
}

static void dhcp4_server_send_ack(struct dhcp4_server_cntx* cntx, guint32 xid,
		struct dhcp4_server_lease* lease) {
	struct dhcp4_pktcntx* pkt = dhcp4_model_pkt_new();
	POOLADDRTOPTR(lease->address);
	dhcp4_model_fillheader(TRUE, pkt->header, xid, a, cntx->serverid,
			lease->mac);
	dhcp4_model_pkt_set_dhcpmessagetype(pkt, DHCP4_DHCPMESSAGETYPE_ACK);
	dhcp4_server_fillinleaseoptions(cntx, pkt, lease);
	gsize pktsz;
	guint8* pktbytes = dhcp4_model_pkt_freetobytes(pkt, &pktsz);
	packetsocket_send_udp(cntx->packetsocket, cntx->ifidx,
			*((guint32*) cntx->serverid), DHCP4_PORT_SERVER,
			DHCP4_PORT_CLIENT, pktbytes, pktsz);
	g_free(pktbytes);
}

static gint dhcp4_server_leasebymac(gconstpointer a, gconstpointer b) {
	const struct dhcp4_server_lease* lease = a;
	const guint8* mac = b;
	return memcmp(lease->mac, mac, sizeof(lease->mac));
}

static struct dhcp4_server_lease* dhcp4_server_findleasebymac(GSList* leases,
		guint8* mac) {
	GSList* leaselink = g_slist_find_custom(leases, mac,
			dhcp4_server_leasebymac);
	struct dhcp4_server_lease* lease =
			leaselink != NULL ? leaselink->data : NULL;
	return lease;
}

static void dhcp4_server_processdhcppkt(struct dhcp4_server_cntx* cntx,
		struct dhcp4_pktcntx* pktcntx) {

	guint8 dhcpmessagetype = dhcp4_model_pkt_get_dhcpmessagetype(pktcntx);
	g_message("dhcp type %s", DHCP4_MESSAGETYPE_STRINGS[dhcpmessagetype]);

	switch (dhcpmessagetype) {
	case DHCP4_DHCPMESSAGETYPE_DISCOVER: {
		g_message("dhcp discover from "MACFMT, MACARG(pktcntx->header->chaddr));
		struct dhcp4_server_lease* lease = dhcp4_server_findleasebymac(
				cntx->leases, pktcntx->header->chaddr);
		if (lease == NULL) {
			g_message("creating new lease for "MACFMT,
					MACARG(pktcntx->header->chaddr));
			lease = dhcp4_server_lease_new(cntx, pktcntx->header->chaddr);
			cntx->leases = g_slist_append(cntx->leases, lease);
		} else {
			g_message("reusing existing lease for "MACFMT,
					MACARG(pktcntx->header->chaddr));
			dhcp4_server_lease_extend(lease);
		}
		dhcp4_server_send_offer(cntx, pktcntx->header->xid, lease);
	}
		break;
	case DHCP4_DHCPMESSAGETYPE_REQUEST: {
		g_message("dhcp request from "MACFMT, MACARG(pktcntx->header->chaddr));
		struct dhcp4_server_lease* lease = dhcp4_server_findleasebymac(
				cntx->leases, pktcntx->header->chaddr);
		if (lease != NULL) {
			dhcp4_server_lease_extend(lease);
			dhcp4_server_send_ack(cntx, pktcntx->header->xid, lease);
		}
	}
		break;
	}

}

static gboolean dhcp4_server_packetsocketcallback(GIOChannel *source,
		GIOCondition condition, gpointer data) {
	struct dhcp4_server_cntx* cntx = data;
	guint8* udppkt = g_malloc(1024);

	gssize udppktlen = packetsocket_recv_udp(cntx->packetsocket,
	DHCP4_PORT_CLIENT,
	DHCP4_PORT_SERVER, udppkt, 1024);

	if (udppktlen > 0) {
		struct dhcp4_pktcntx* pktcntx = dhcp4_model_pkt_parse(udppkt,
				udppktlen);
		if (pktcntx != NULL) {
			dhcp4_server_processdhcppkt(cntx, pktcntx);
			g_free(pktcntx);
		}
	}
	g_free(udppkt);
	return TRUE;
}

static void dhcp4_server_checklease(gpointer data, gpointer user_data) {
	struct dhcp4_server_lease* lease = data;
	struct dhcp4_server_cntx* cntx = user_data;
	gint64 remaining = dhcp4_server_lease_remaining(lease);
	g_message("checking lease for "MACFMT", %d seconds remaining",
			MACARG(lease->mac), (int )(remaining / 1000000));
	if (remaining < -EXPIRED) {
		g_message("removing expired lease");
		cntx->leases = g_slist_remove(cntx->leases, lease);
		cntx->pool = g_slist_concat(lease->address, cntx->pool);
		dhcp4_server_lease_free(lease);
	}
}

static gboolean dhcp4_server_timer(gpointer user_data) {
	struct dhcp4_server_cntx* cntx = user_data;
	g_slist_foreach(cntx->leases, dhcp4_server_checklease, cntx);
	return TRUE;
}

void dhcp4_server_start(struct dhcp4_server_cntx* cntx) {
	cntx->packetsocket = packetsocket_createsocket_udp(cntx->ifidx, cntx->mac);
	GIOChannel* channel = g_io_channel_unix_new(cntx->packetsocket);
	cntx->packetsocketsource = g_io_add_watch(channel, G_IO_IN,
			dhcp4_server_packetsocketcallback, cntx);
	g_io_channel_unref(channel);
	g_timeout_add(60 * 1000, dhcp4_server_timer, cntx);
}

void dhcp4_server_stop(struct dhcp4_server_cntx* cntx) {
	//close(cntx->packetsocket);
}

void dhcp4_server_free(struct dhcp4_server_cntx* cntx) {

}
