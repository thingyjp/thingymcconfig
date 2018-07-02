#include <glib.h>
#include "dhcp4.h"
#include "dhcp4_server.h"
#include "dhcp4_model.h"
#include "packetsocket.h"

struct dhcp4_server_cntx* dhcp4_server_new(unsigned ifidx, const guint8* mac) {
	struct dhcp4_server_cntx* cntx = g_malloc0(sizeof(*cntx));
	memcpy(cntx->mac, mac, sizeof(cntx->mac));
	return cntx;
}

static void dhcp4_server_processdhcppkt(struct dhcp4_server_cntx* cntx,
		struct dhcp4_pktcntx* pktcntx) {

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

void dhcp4_server_start(struct dhcp4_server_cntx* cntx) {
	cntx->packetsocket = packetsocket_createsocket_udp(cntx->ifidx, cntx->mac);
	GIOChannel* channel = g_io_channel_unix_new(cntx->packetsocket);
	cntx->packetsocketsource = g_io_add_watch(channel, G_IO_IN,
			dhcp4_server_packetsocketcallback, cntx);
	g_io_channel_unref(channel);
}

void dhcp4_server_stop(struct dhcp4_server_cntx* cntx) {
	//close(cntx->packetsocket);
}

void dhcp4_server_full(struct dhcp4_server_cntx* cntx) {

}
