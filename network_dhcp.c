#include "buildconfig.h"
#include "network_dhcp.h"

static GPid dhcpcpid, dhcpdpid;

void network_dhcpclient_start(const gchar* interfacename) {
	g_message("starting dhcp client for %s", interfacename);
	gchar* args[] = { DHCPC_BINARYPATH, "-d", interfacename, NULL };
	if (!g_spawn_async(NULL, args, NULL,
			G_SPAWN_STDOUT_TO_DEV_NULL | G_SPAWN_STDERR_TO_DEV_NULL, NULL, NULL,
			&dhcpcpid, NULL)) {
		g_message("failed to start dhcp client for %s", interfacename);
	}
}

void network_dhcpclient_stop() {
	g_spawn_close_pid(dhcpcpid);
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
