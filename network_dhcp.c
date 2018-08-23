#include <nlglue/rtnetlink.h>
#include <teenynet/dhcp4_client.h>
#include <teenynet/dhcp4_server.h>
#include <teenynet/ip4.h>
#include "buildconfig.h"
#include "network_dhcp.h"
#include "jsonbuilderutils.h"

static Dhcp4Client* dhcp4client = NULL;
static struct dhcp4_server_cntx* dhcp4servercntx;

static void network_dhcpclient_supplicantconnected(void) {
	dhcp4_client_resume(dhcp4client);
}

static void network_dhcpclient_supplicantdisconnected(void) {
	dhcp4_client_pause(dhcp4client);
}

static void network_dhcpclient_lease(Dhcp4Client* client,
		struct dhcp4_client_lease* lease, gpointer user_data) {
	if (lease != NULL) {
		unsigned ifidx = dhcp4_client_getifindx(dhcp4client);
		guint32 nm = *((guint32*) lease->subnetmask);
		int nmbits = 0;
		for (int i = 0; i < sizeof(nm) * 8; i++)
			nmbits += (nm >> i) & 1;

		GString* addrgstr = g_string_new(NULL);
		g_string_append_printf(addrgstr, IP4_ADDRFMT"/%d",
				IP4_ARGS(lease->leasedip), nmbits);
		gchar* addrstr = g_string_free(addrgstr, FALSE);
		network_rtnetlink_setipv4addr(ifidx, addrstr);
		g_free(addrstr);

		network_rtnetlink_setipv4defaultfw(ifidx, lease->defaultgw);

		GString* resolvconfgstr = g_string_new(NULL);
		for (int i = 0; i < lease->numnameservers; i++) {
			guint8* nameserver = lease->nameservers[i];
			g_string_append_printf(resolvconfgstr, "nameserver "IP4_ADDRFMT,
					IP4_ARGS(nameserver));
		}
		gsize resolvconfstrlen = resolvconfgstr->len;
		gchar* resolvconfstr = g_string_free(resolvconfgstr, FALSE);
		g_file_set_contents("/etc/resolv.conf", resolvconfstr, resolvconfstrlen,
		NULL);
		g_free(resolvconfstr);
	} else
		network_rtnetlink_clearipv4addr(dhcp4_client_getifindx(dhcp4client));
}

void network_dhcpclient_start(NetworkWpaSupplicant* supplicant, unsigned ifidx,
		const gchar* interfacename, const guint8* interfacemac) {
	g_message("starting dhcp4 client for %s", interfacename);
	network_rtnetlink_clearipv4addr(ifidx);
	dhcp4client = dhcp4_client_new(ifidx, interfacemac);
	dhcp4_client_start(dhcp4client);
	g_signal_connect(supplicant,
			NETWORK_WPASUPPLICANT_SIGNAL "::" NETWORK_WPASUPPLICANT_DETAIL_CONNECTED,
			network_dhcpclient_supplicantconnected, NULL);
	g_signal_connect(supplicant,
			NETWORK_WPASUPPLICANT_SIGNAL "::" NETWORK_WPASUPPLICANT_DETAIL_DISCONNECTED,
			network_dhcpclient_supplicantdisconnected, NULL);
	g_signal_connect(dhcp4client, DHCP4_CLIENT_SIGNAL_LEASE,
			network_dhcpclient_lease, NULL);
}

void network_dhcpclient_stop() {
	dhcp4_client_stop(dhcp4client);
}

void network_dhcpserver_start(unsigned ifidx, const gchar* interfacename,
		const guint8* interfacemac) {
	guint8 serverid[] = { 10, 0, 0, 1 };
	guint8 poolstart[] = { 10, 0, 0, 2 };
	unsigned poollen = 5;
	guint8 subnetmask[] = { 255, 255, 255, 248 };
	guint8 defaultgw[] = { 10, 0, 0, 1 };
	g_message("starting dhcp server for %s", interfacename);
	dhcp4servercntx = dhcp4_server_new(ifidx, interfacemac, serverid, poolstart,
			poollen, subnetmask, defaultgw);
	dhcp4_server_start(dhcp4servercntx);
}

void network_dhcpserver_stop() {
	dhcp4_server_stop(dhcp4servercntx);
}

static gchar* dhcpcstatestrs[] = { [DHCP4CS_IDLE] = "idle", [DHCP4CS_DISCOVERING
		] = "discovering", [DHCP4CS_REQUESTING ] = "requesting",
		[DHCP4CS_CONFIGURED ] = "configured" };

static void network_dhcp_dumpstatus_addip4addr(JsonBuilder* builder,
		guint8* addr) {
	GString* strstr = g_string_new(NULL);
	g_string_printf(strstr, IP4_ADDRFMT, IP4_ARGS(addr));
	gchar* str = g_string_free(strstr, FALSE);
	json_builder_add_string_value(builder, str);
	g_free(str);
}

void network_dhcp_dumpstatus(JsonBuilder* builder) {
	if (dhcp4client != NULL) {
		JSONBUILDER_START_OBJECT(builder, "dhcp4");
		JSONBUILDER_ADD_STRING(builder, "state",
				dhcpcstatestrs[dhcp4_client_getstate(dhcp4client)]);

		struct dhcp4_client_lease* lease = dhcp4_client_getlease(dhcp4client);
		if (lease != NULL) {
			JSONBUILDER_START_OBJECT(builder, "lease");
			json_builder_set_member_name(builder, "ip");
			network_dhcp_dumpstatus_addip4addr(builder, lease->leasedip);
			json_builder_set_member_name(builder, "subnetmask");
			network_dhcp_dumpstatus_addip4addr(builder, lease->subnetmask);
			json_builder_set_member_name(builder, "defaultgw");
			network_dhcp_dumpstatus_addip4addr(builder, lease->defaultgw);
			JSONBUILDER_START_ARRAY(builder, "nameservers");
			for (int i = 0; i < lease->numnameservers; i++) {
				network_dhcp_dumpstatus_addip4addr(builder,
						lease->nameservers[i]);
			}
			json_builder_end_array(builder);
			json_builder_end_object(builder);
		}
		json_builder_end_object(builder);
	}
}
