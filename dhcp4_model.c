#include <string.h>
#include <arpa/inet.h>
#include <net/ethernet.h>
#include "buildconfig.h"
#include "dhcp4_model.h"

struct dhcp4_pktcntx* dhcp4_model_pkt_new() {
	struct dhcp4_pktcntx* pktcntx = g_malloc0(sizeof(*pktcntx));
	pktcntx->header = g_malloc0(sizeof(*pktcntx->header));
	return pktcntx;
}

static void dhcp4_model_pkt_walkoptions(guint8* options, gsize len,
		GSList** output) {
	guint8* option = options;

	while (option < options + len) {
		guint8 type = *option++;
		if (type == 0)
			continue;
		else if (type == 0xff)
			break;

		struct dhcp4_opt* opt = g_malloc0(sizeof(struct dhcp4_opt));
		opt->type = type;
		opt->len = *option++;
		opt->data = option;
		option += opt->len;

		*output = g_slist_append(*output, opt);
#ifdef D4MDEBUG
		g_message("option %d with %d bytes of data", (int ) opt->type,
				(int ) opt->len);
#endif
	}

}

struct dhcp4_pktcntx* dhcp4_model_pkt_parse(guint8* pkt, gsize len) {
	if (len
			< (sizeof(struct dhcp4_header) + DHCP4_SNAME_LEN + DHCP4_FILE_LEN
					+ sizeof(DHCP4_MAGIC) + 1)) {
#ifdef D4MDEBUG
		g_message("bad packet len");
#endif
		return NULL;
	}

	unsigned headerend = sizeof(struct dhcp4_header) + DHCP4_SNAME_LEN
			+ DHCP4_FILE_LEN;

	guint8* magic = pkt + headerend;
	if (memcmp(magic, DHCP4_MAGIC, sizeof(DHCP4_MAGIC)) != 0) {
#ifdef D4MDEBUG
		g_message("bad dhcp packet magic");
#endif
		return NULL;
	}

	struct dhcp4_pktcntx* pktcntx = g_malloc0(sizeof(struct dhcp4_pktcntx));
	pktcntx->header = (struct dhcp4_header*) pkt;

	int optionsoff = headerend + sizeof(DHCP4_MAGIC);
	dhcp4_model_pkt_walkoptions(pkt + optionsoff, len - optionsoff,
			&pktcntx->options);

	return pktcntx;
}

static gint dhcp4_model_findbytype(gconstpointer a, gconstpointer b) {
	const struct dhcp4_opt* opt = a;
	return opt->type - GPOINTER_TO_INT(b);
}

static struct dhcp4_opt* dhcp4_model_optbytype(struct dhcp4_pktcntx* pktcntx,
		guint8 type) {
	GSList* opt = g_slist_find_custom(pktcntx->options, GINT_TO_POINTER(type),
			dhcp4_model_findbytype);
	if (opt != NULL)
		return ((struct dhcp4_opt*) opt->data);
	else
		return NULL;
}

void dhcp4_model_pkt_set_dhcpmessagetype(struct dhcp4_pktcntx* pktcntx,
		guint8 type) {
	struct dhcp4_opt* opt = g_malloc(sizeof(*opt) + 1);
	opt->type = DHCP4_OPT_DHCPMESSAGETYPE;
	opt->len = 1;
	opt->data = ((guint8*) opt) + sizeof(*opt);
	*opt->data = type;
	pktcntx->options = g_slist_append(pktcntx->options, opt);
}

guint8 dhcp4_model_pkt_get_dhcpmessagetype(struct dhcp4_pktcntx* pktcntx) {
	struct dhcp4_opt* type = dhcp4_model_optbytype(pktcntx,
	DHCP4_OPT_DHCPMESSAGETYPE);
	if (type != NULL)
		return *type->data;
	return 0;
}

static void dhcp4_model_pkt_set_fourbyteopt(struct dhcp4_pktcntx* pktcntx,
		guint8 type, guint8* data) {
	struct dhcp4_opt* opt = g_malloc(sizeof(*opt) + DHCP4_ADDRESS_LEN);
	opt->type = type;
	opt->len = DHCP4_ADDRESS_LEN;
	opt->data = ((guint8*) opt) + sizeof(*opt);
	memcpy(opt->data, data, DHCP4_ADDRESS_LEN);
	pktcntx->options = g_slist_append(pktcntx->options, opt);
}

void dhcp4_model_pkt_set_requestedip(struct dhcp4_pktcntx* pktcntx, guint8* ip) {
	dhcp4_model_pkt_set_fourbyteopt(pktcntx, DHCP4_OPT_REQUESTEDIP, ip);
}

static gboolean dhcp4_model_pkt_get_fourbyteopt(struct dhcp4_pktcntx* pktcntx,
		guint8* result, guint8 type) {
	struct dhcp4_opt* opt = dhcp4_model_optbytype(pktcntx, type);
	if (opt != NULL) {
		if (opt->len != DHCP4_ADDRESS_LEN)
			return FALSE;
		memcpy(result, opt->data, DHCP4_ADDRESS_LEN);
		return TRUE;
	} else
		return FALSE;
}

void dhcp4_model_pkt_set_serverid(struct dhcp4_pktcntx* pktcntx, guint8* ip) {
	dhcp4_model_pkt_set_fourbyteopt(pktcntx, DHCP4_OPT_SERVERID, ip);
}

gboolean dhcp4_model_pkt_get_serverid(struct dhcp4_pktcntx* pktcntx,
		guint8* result) {
	return dhcp4_model_pkt_get_fourbyteopt(pktcntx, result, DHCP4_OPT_SERVERID);
}

void dhcp4_model_pkt_set_defaultgw(struct dhcp4_pktcntx* pktcntx, guint8* ip) {
	dhcp4_model_pkt_set_fourbyteopt(pktcntx, DHCP4_OPT_DEFAULTGATEWAY, ip);
}

gboolean dhcp4_model_pkt_get_defaultgw(struct dhcp4_pktcntx* pktcntx,
		guint8* result) {
	return dhcp4_model_pkt_get_fourbyteopt(pktcntx, result,
	DHCP4_OPT_DEFAULTGATEWAY);
}

void dhcp4_model_pkt_set_subnetmask(struct dhcp4_pktcntx* pktcntx,
		guint8* subnetmask) {
	dhcp4_model_pkt_set_fourbyteopt(pktcntx, DHCP4_OPT_SUBNETMASK, subnetmask);
}

gboolean dhcp4_model_pkt_get_subnetmask(struct dhcp4_pktcntx* pktcntx,
		guint8* result) {
	return dhcp4_model_pkt_get_fourbyteopt(pktcntx, result,
	DHCP4_OPT_SUBNETMASK);
}

void dhcp4_model_pkt_set_leasetime(struct dhcp4_pktcntx* pktcntx, guint32 time) {
	guint32 leasetime = htonl(time);
	dhcp4_model_pkt_set_fourbyteopt(pktcntx, DHCP4_OPT_LEASETIME,
			(guint8*) &leasetime);
}

gboolean dhcp4_model_pkt_get_leasetime(struct dhcp4_pktcntx* pktcntx,
		guint32* result) {
	guint32 leasetime;
	if (dhcp4_model_pkt_get_fourbyteopt(pktcntx, (guint8*) &leasetime,
	DHCP4_OPT_LEASETIME)) {
		*result = ntohl(leasetime);
		return TRUE;
	}
	return FALSE;
}

void dhcp4_model_pkt_set_domainnameservers(struct dhcp4_pktcntx* pktcntx,
		guint8* servers, int numservers) {
	guint8 nsserver[] = { 10, 0, 0, 1 };
	dhcp4_model_pkt_set_fourbyteopt(pktcntx, DHCP4_OPT_DOMAINNAMESERVER,
			nsserver);
}

gboolean dhcp4_model_pkt_get_domainnameservers(struct dhcp4_pktcntx* pktcntx,
		guint8* result, guint8* numresult) {
//TODO this is wrong!
	gboolean ret = dhcp4_model_pkt_get_fourbyteopt(pktcntx, result,
	DHCP4_OPT_DOMAINNAMESERVER);
	if (ret)
		*numresult = 1;
	return ret;
}

static void dhcp4_model_pkt_appendoption(gpointer data, gpointer userdata) {
	struct dhcp4_opt* opt = data;
	GByteArray* pktbuff = userdata;
	g_byte_array_append(pktbuff, &opt->type, sizeof(opt->type));
	g_byte_array_append(pktbuff, &opt->len, sizeof(opt->len));
	g_byte_array_append(pktbuff, opt->data, opt->len);
}

static void dhcp4_model_pkt_freeoption(gpointer data) {
	struct dhcp4_opt* opt = data;
	g_free(opt);
}

guint8* dhcp4_model_pkt_freetobytes(struct dhcp4_pktcntx* pktcntx, gsize* sz) {

	guint8* padding = g_malloc0(128);

	GByteArray* bytearray = g_byte_array_new();

	g_byte_array_append(bytearray, (guint8*) pktcntx->header,
			sizeof(*pktcntx->header));

	g_byte_array_append(bytearray, padding, DHCP4_SNAME_LEN);
	g_byte_array_append(bytearray, padding, DHCP4_FILE_LEN);

	g_byte_array_append(bytearray, DHCP4_MAGIC, sizeof(DHCP4_MAGIC));

	g_slist_foreach(pktcntx->options, dhcp4_model_pkt_appendoption, bytearray);

	const guint8 terminator[] = { 0xff };
	g_byte_array_append(bytearray, terminator, sizeof(terminator));

	g_free(pktcntx->header);
	g_slist_free_full(pktcntx->options, dhcp4_model_pkt_freeoption);
	g_free(pktcntx);

	*sz = bytearray->len;
	return g_byte_array_free(bytearray, FALSE);
}

void dhcp4_model_fillheader(gboolean reply, struct dhcp4_header* header,
		guint32 xid, guint8* yiaddr, guint8* siaddr, guint8* mac) {
	memset(header, 0, sizeof(*header));
	header->op = reply ? 2 : 1;
	header->htype = 1;
	header->hlen = 6;
	header->hops = 0;
	header->xid = xid;

	if (yiaddr != NULL)
		memcpy(header->yiaddr, yiaddr, sizeof(header->yiaddr));

	if (siaddr != NULL)
		memcpy(header->siaddr, siaddr, sizeof(header->siaddr));

	if (mac != NULL)
		memcpy(header->chaddr, mac, ETHER_ADDR_LEN);
}

