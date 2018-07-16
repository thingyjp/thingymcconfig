#include "dhcp4_client.h"
#include "dhcp4_model.h"
#include "dhcp4.h"
#include "packetsocket.h"
#include "ip4.h"

static void dhcp4_client_changestate(struct dhcp4_client_cntx* cntx,
		enum dhcp4_clientstate newstate);

struct dhcp4_client_cntx* dhcp4_client_new(unsigned ifidx, const guint8* mac) {
	struct dhcp4_client_cntx* cntx = g_malloc0(
			sizeof(struct dhcp4_client_cntx));
	cntx->ifidx = ifidx;
	memcpy(cntx->mac, mac, sizeof(cntx->mac));
	cntx->rand = g_rand_new();
	return cntx;
}

static void dhcp4_client_send_discover(struct dhcp4_client_cntx* cntx) {
	cntx->xid = g_rand_int(cntx->rand);
	struct dhcp4_pktcntx* pkt = dhcp4_model_pkt_new();
	dhcp4_model_fillheader(FALSE, pkt->header, cntx->xid, NULL, NULL,
			cntx->mac);
	dhcp4_model_pkt_set_dhcpmessagetype(pkt, DHCP4_DHCPMESSAGETYPE_DISCOVER);
	gsize pktsz;
	guint8* pktbytes = dhcp4_model_pkt_freetobytes(pkt, &pktsz);
	packetsocket_send_udp(cntx->rawsocket, cntx->ifidx, 0, DHCP4_PORT_CLIENT,
	DHCP4_PORT_SERVER, pktbytes, pktsz);
	g_free(pktbytes);
}

static gboolean dhcp4_client_discoverytimeout(gpointer data) {
	struct dhcp4_client_cntx* cntx = data;

	if (cntx->state != DHCP4CS_DISCOVERING)
		return FALSE;

	dhcp4_client_send_discover(cntx);
	return TRUE;
}

static void dhcp4_client_send_request(struct dhcp4_client_cntx* cntx) {
	struct dhcp4_pktcntx* pkt = dhcp4_model_pkt_new();
	dhcp4_model_fillheader(FALSE, pkt->header, cntx->xid, NULL, NULL,
			cntx->mac);
	dhcp4_model_pkt_set_dhcpmessagetype(pkt, DHCP4_DHCPMESSAGETYPE_REQUEST);
	dhcp4_model_pkt_set_serverid(pkt, cntx->pendinglease->serverip);
	dhcp4_model_pkt_set_requestedip(pkt, cntx->pendinglease->leasedip);
	gsize pktsz;
	guint8* pktbytes = dhcp4_model_pkt_freetobytes(pkt, &pktsz);
	packetsocket_send_udp(cntx->rawsocket, cntx->ifidx, 0, DHCP4_PORT_CLIENT,
	DHCP4_PORT_SERVER, pktbytes, pktsz);
	g_free(pktbytes);
}

static gboolean dhcp4_client_requesttimeout(gpointer data) {
	struct dhcp4_client_cntx* cntx = data;

	if (cntx->state != DHCP4CS_REQUESTING)
		return FALSE;

	g_message("timeout waiting for ack");
	dhcp4_client_changestate(cntx, DHCP4CS_DISCOVERING);
	return FALSE;
}

static gboolean dhcp4_client_leasetimeout(gpointer data) {
	struct dhcp4_client_cntx* cntx = data;

	if (cntx->state != DHCP4CS_CONFIGURED)
		return FALSE;

	dhcp4_client_changestate(cntx, DHCP4CS_DISCOVERING);
	return FALSE;
}

static void dhcp4_client_processdhcppkt(struct dhcp4_client_cntx* cntx,
		struct dhcp4_pktcntx* pktcntx) {
	if (pktcntx->header->xid == cntx->xid) {
		guint8 dhcpmessagetype = dhcp4_model_pkt_get_dhcpmessagetype(pktcntx);

		g_message("dhcp type %d", (int ) dhcpmessagetype);

		switch (cntx->state) {
		case DHCP4CS_DISCOVERING:
			switch (dhcpmessagetype) {
			case DHCP4_DHCPMESSAGETYPE_OFFER: {
				struct dhcp4_client_lease* lease = g_malloc0(sizeof(*lease));
				dhcp4_model_pkt_get_serverid(pktcntx, lease->serverip);
				memcpy(lease->leasedip, pktcntx->header->yiaddr,
						sizeof(lease->leasedip));
				dhcp4_model_pkt_get_subnetmask(pktcntx, lease->subnetmask);
				dhcp4_model_pkt_get_defaultgw(pktcntx, lease->defaultgw);
				dhcp4_model_pkt_get_leasetime(pktcntx, &lease->leasetime);
				dhcp4_model_pkt_get_domainnameservers(pktcntx,
						(guint8*) lease->nameservers, &lease->numnameservers);
				g_message(
						"have dhcp offer of "IP4_ADDRFMT"/"IP4_ADDRFMT" for %u seconds from "IP4_ADDRFMT,
						IP4_ARGS(lease->leasedip), IP4_ARGS(lease->subnetmask),
						lease->leasetime, IP4_ARGS(lease->serverip));
				cntx->pendinglease = lease;
				dhcp4_client_changestate(cntx, DHCP4CS_REQUESTING);
				break;
			}
			default:
				break;
			}
			break;
		case DHCP4CS_REQUESTING:
			switch (dhcpmessagetype) {
			case DHCP4_DHCPMESSAGETYPE_ACK:
				dhcp4_client_changestate(cntx, DHCP4CS_CONFIGURED);
				break;
			default:
				break;
			}
			break;
		default:
			break;
		}
	} else
		g_message("ignoring dhcp packet, different xid");
}

static gboolean dhcp4_client_rawsocketcallback(GIOChannel *source,
		GIOCondition condition, gpointer data) {
	struct dhcp4_client_cntx* cntx = data;
	guint8* udppkt = g_malloc(1024);
	gssize udppktlen = packetsocket_recv_udp(cntx->rawsocket, DHCP4_PORT_SERVER,
	DHCP4_PORT_CLIENT, udppkt, 1024);

	if (udppktlen > 0) {
		struct dhcp4_pktcntx* pktcntx = dhcp4_model_pkt_parse(udppkt,
				udppktlen);
		if (pktcntx != NULL) {
			dhcp4_client_processdhcppkt(cntx, pktcntx);
			g_free(pktcntx);
		}
	}
	g_free(udppkt);
	return TRUE;
}

static void dhcp4_client_changestate(struct dhcp4_client_cntx* cntx,
		enum dhcp4_clientstate newstate) {
	g_message("moving from state %d to %d", cntx->state, newstate);

	cntx->state = newstate;
	switch (cntx->state) {
	case DHCP4CS_DISCOVERING:
		cntx->rawsocket = packetsocket_createsocket_udp(cntx->ifidx, cntx->mac);
		g_assert(cntx->rawsocket);
		GIOChannel* channel = g_io_channel_unix_new(cntx->rawsocket);
		cntx->rawsocketeventsource = g_io_add_watch(channel, G_IO_IN,
				dhcp4_client_rawsocketcallback, cntx);
		g_io_channel_unref(channel);
		dhcp4_client_send_discover(cntx);
		g_timeout_add(10 * 1000, dhcp4_client_discoverytimeout, cntx);
		break;
	case DHCP4CS_REQUESTING:
		dhcp4_client_send_request(cntx);
		g_timeout_add(10 * 1000, dhcp4_client_requesttimeout, cntx);
		break;
	case DHCP4CS_CONFIGURED:
		g_source_remove(cntx->rawsocketeventsource);
		cntx->rawsocketeventsource = -1;
		close(cntx->rawsocket);
		cntx->rawsocket = -1;
		cntx->currentlease = cntx->pendinglease;
		cntx->pendinglease = NULL;
		_dhcp4_client_configureinterface(cntx->ifidx,
				cntx->currentlease->leasedip, cntx->currentlease->subnetmask,
				cntx->currentlease->defaultgw,
				(guint8*) cntx->currentlease->nameservers,
				cntx->currentlease->numnameservers);
		g_timeout_add(cntx->currentlease->leasetime * 1000,
				dhcp4_client_leasetimeout, cntx);
		break;
	default:
		break;
	}
}

void dhcp4_client_start(struct dhcp4_client_cntx* cntx) {
	_dhcp4_client_clearinterface(cntx->ifidx);
	dhcp4_client_changestate(cntx, DHCP4CS_DISCOVERING);
}

void dhcp4_client_stop(struct dhcp4_client_cntx* cntx) {

}
