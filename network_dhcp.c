#include "buildconfig.h"
#include "network_dhcp.h"
#include "dhcp4_client.h"

static struct dhcp4_client_cntx* dhcp4clientcntx;
static GPid dhcpdpid;

void network_dhcpclient_start(unsigned ifidx, const gchar* interfacename) {
	g_message("starting dhcp4 client for %s", interfacename);
	guint8 mac[] = { 0x08, 0x6a, 0x0a, 0x97, 0x63, 0x9c };
	dhcp4clientcntx = dhcp4_client_new(ifidx, mac);
	dhcp4_client_start(dhcp4clientcntx);
}

void network_dhcpclient_stop() {

}

void network_dhcpserver_start(const gchar* interfacename) {
	g_message("starting dhcp server for %s", interfacename);
	gchar* args[] = { DHCPD_BINARYPATH, "-f", interfacename, NULL };
	if (!g_spawn_async(NULL, args, NULL,
			G_SPAWN_STDOUT_TO_DEV_NULL | G_SPAWN_STDERR_TO_DEV_NULL, NULL, NULL,
			&dhcpdpid, NULL)) {
		g_message("failed to start dhcp server for %s", interfacename);
	}
}

void network_dhcpserver_stop() {
	g_spawn_close_pid(dhcpdpid);
}

void network_dhcp_dumpstatus(JsonBuilder* builder) {

}
