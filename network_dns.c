#include <glib.h>
#include <teenynet/dhcp4_client.h>
#include <teenynet/ip4.h>
#include "network_dns.h"

#define RESOLVCONFPATH "/run/thingymcconfig/resolv.conf"

void network_dns_configure(struct dhcp4_client_lease* lease) {
	GString* resolvconfgstr = g_string_new(NULL);
	for (int i = 0; i < lease->numnameservers; i++) {
		guint8* nameserver = lease->nameservers[i];
		g_string_append_printf(resolvconfgstr, "nameserver "IP4_ADDRFMT"\n",
				IP4_ARGS(nameserver));
	}
	gsize resolvconfstrlen = resolvconfgstr->len;
	gchar* resolvconfstr = g_string_free(resolvconfgstr, FALSE);
	g_file_set_contents(RESOLVCONFPATH, resolvconfstr, resolvconfstrlen,
	NULL);
	g_free(resolvconfstr);
}
