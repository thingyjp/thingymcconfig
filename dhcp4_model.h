#pragma once

#include <glib.h>
#include "dhcp4.h"

struct dhcp4_opt {
	guint8 type;
	guint8 len;
	guint8* data;
};

struct __attribute__((packed)) dhcp4_header {
	guint8 op;
	guint8 htype;
	guint8 hlen;
	guint8 hops;
	guint32 xid;
	guint16 secs;
	guint16 flags;
	guint32 ciaddr;
	guint8 yiaddr[DHCP4_ADDRESS_LEN];
	guint8 siaddr[DHCP4_ADDRESS_LEN];
	guint32 giaddr;
	guint8 chaddr[16];
};

struct dhcp4_pktcntx {
	struct dhcp4_header* header;
	GSList* options;
};

struct dhcp4_pktcntx* dhcp4_model_pkt_new(void);
struct dhcp4_pktcntx* dhcp4_model_pkt_parse(guint8* pkt, gsize len);
void dhcp4_model_pkt_set_dhcpmessagetype(struct dhcp4_pktcntx* pktcntx,
		guint8 type);
guint8 dhcp4_model_pkt_get_dhcpmessagetype(struct dhcp4_pktcntx* pktcntx);
void dhcp4_model_pkt_set_requestedip(struct dhcp4_pktcntx* pktcntx, guint8* ip);
void dhcp4_model_pkt_set_subnetmask(struct dhcp4_pktcntx* pktcntx,
		guint8* subnetmask);
void dhcp4_model_pkt_add_router(struct dhcp4_pktcntx* pktcntx);
void dhcp4_model_pkt_add_dnsserver(struct dhcp4_pktcntx* pktcntx);

void dhcp4_model_pkt_set_serverid(struct dhcp4_pktcntx* pktcntx, guint8* ip);
gboolean dhcp4_model_pkt_get_serverid(struct dhcp4_pktcntx* pktcntx,
		guint8* result);
void dhcp4_model_pkt_set_defaultgw(struct dhcp4_pktcntx* pktcntx, guint8* ip);
gboolean dhcp4_model_pkt_get_defaultgw(struct dhcp4_pktcntx* pktcntx,
		guint8* result);
gboolean dhcp4_model_pkt_get_subnetmask(struct dhcp4_pktcntx* pktcntx,
		guint8* result);
void dhcp4_model_pkt_set_leasetime(struct dhcp4_pktcntx* pktcntx, guint32 time);
gboolean dhcp4_model_pkt_get_leasetime(struct dhcp4_pktcntx* pktcntx,
		guint32* result);
void dhcp4_model_pkt_set_domainnameservers(struct dhcp4_pktcntx* pktcntx,
		guint8* servers, int numservers);
gboolean dhcp4_model_pkt_get_domainnameservers(struct dhcp4_pktcntx* pktcntx,
		guint8* result, guint8* numresult);

guint8* dhcp4_model_pkt_freetobytes(struct dhcp4_pktcntx* pktcntx, gsize* sz);

void dhcp4_model_fillheader(gboolean reply, struct dhcp4_header* header,
		guint32 xid, guint8* yiaddr, guint8* siaddr, guint8* mac);
