#include <sys/types.h>
#include <netlink/socket.h>
#include <netlink/genl/genl.h>
#include <netlink/genl/ctrl.h>
#include <netlink/route/route.h>
#include <netlink/route/link.h>
#include <netlink/route/addr.h>
#include <linux/if.h>

#include "buildconfig.h"
#include "network.h"
#include "network_priv.h"
#include "network_nl80211.h"
#include "network_wpasupplicant.h"
#include "network_dhcp.h"

#define NUMBEROFINTERFACESWHENCONFIGURED 2

static const char* interfacename;
static char* apinterfacename;
static int apinterfaceindex;

static gboolean noapinterface;

struct wpa_ctrl* wpa_ctrl_sta = NULL;
struct wpa_ctrl* wpa_ctrl_ap = NULL;

static struct nl_sock* routesock;

static GPid stasupplicantpid, apsupplicantpid;

gboolean network_init(const char* interface, gboolean noap) {
	interfacename = interface;
	noapinterface = noap;

	if (!network_nl80211_init())
		goto err_nl80211init;

	routesock = nl_socket_alloc();
	nl_connect(routesock, NETLINK_ROUTE);

	network_wpasupplicant_init();

	return TRUE;

	err_nl80211init:			//
	return FALSE;
}

void network_cleanup() {

}

/*
 static void network_netlink_setlinkstate(struct network_interface* interface,
 gboolean up) {
 g_message("setting link state on %s", interface->ifname);
 struct nl_cache* cache;
 // how do you free this?
 int ret;
 if ((ret = rtnl_link_alloc_cache(routesock, AF_UNSPEC, &cache)) < 0) {
 g_message("failed to populate link cache -> %d", ret);
 goto out;
 }
 struct rtnl_link* link = rtnl_link_get(cache, interface->ifidx);
 if (link == NULL) {
 g_message("didn't find link");
 goto out;
 }

 struct rtnl_link* change = rtnl_link_alloc();
 rtnl_link_unset_flags(change, IFF_UP);

 rtnl_link_change(routesock, link, change, 0);

 rtnl_link_put(link);
 rtnl_link_put(change);
 out: return;
 }*/

static void network_netlink_setipv4addr(int ifidx, const char* addrstr) {
	struct nl_addr* localaddr;
	nl_addr_parse(addrstr, AF_INET, &localaddr);

	struct rtnl_addr* addr = rtnl_addr_alloc();

	rtnl_addr_set_ifindex(addr, ifidx);
	rtnl_addr_set_local(addr, localaddr);

	if (rtnl_addr_add(routesock, addr, 0) != 0)
		g_message("failed to set v4 addr");

	rtnl_addr_put(addr);
	nl_addr_put(localaddr);
}

/*static void network_createstainterface(GHashTable* interfaces,
 struct network_interface* masterinterface) {
 network_netlink_createvif(interfaces, masterinterface, IFNAMESUFFIX_STA,
 NL80211_IFTYPE_STATION);
 }*/

static gboolean network_filteroutotherphys_filter(gpointer key, gpointer value,
		gpointer user_data) {
	struct network_interface* interface = value;
	guint32 wiphy = *((guint32*) user_data);
	return wiphy != interface->wiphy;

}
static void network_filteroutoutherphys(GHashTable* interfaces, guint32 wiphy) {
	g_hash_table_foreach_remove(interfaces, network_filteroutotherphys_filter,
			&wiphy);
}

static gboolean network_setupinterfaces() {
	GHashTable* interfaces = network_nl80211_listwifiinterfaces();
	guint32 wiphy;
	gboolean ret = FALSE;

	if (network_nl80211_findphy(interfaces, interfacename, &wiphy)) {
		network_filteroutoutherphys(interfaces, wiphy);
		int remainingnetworks = g_hash_table_size(interfaces);
		if (remainingnetworks == 1) {
			g_message("ap interface is missing, will create");
			struct network_interface* masterinterface;
			masterinterface = g_hash_table_lookup(interfaces, interfacename);
			g_assert(masterinterface != NULL);

			/*
			 // create the station VIF
			 {
			 network_createstainterface(interfaces, masterinterface);
			 struct network_interface* stainterface = g_hash_table_find(
			 interfaces, network_findstainterface,
			 masterinterface->ifname);
			 g_assert(stainterface != NULL);
			 g_message("STA interface created -> %s", stainterface->ifname);
			 stainterfacename = stainterface->ifname;
			 }
			 */
			// create the ap VIF
			{
				if (!noapinterface) {
					network_nl80211_createapvif(interfaces, masterinterface);
					struct network_interface* apinterface = g_hash_table_find(
							interfaces, network_nl80211_findapvif,
							masterinterface->ifname);
					g_assert(apinterface != NULL);
					g_message("AP interface created -> %s",
							apinterface->ifname);
					apinterfacename = apinterface->ifname;
					apinterfaceindex = apinterface->ifidx;
				}
			}

			ret = TRUE;
		} else if (remainingnetworks == NUMBEROFINTERFACESWHENCONFIGURED) {
			struct network_interface* masterinterface;
			masterinterface = g_hash_table_lookup(interfaces, interfacename);
			g_assert(masterinterface != NULL);
			/*			struct network_interface* sta = g_hash_table_find(interfaces,
			 network_findstainterface, masterinterface->ifname);
			 g_assert(sta != NULL);
			 stainterfacename = sta->ifname;*/
			struct network_interface* ap = g_hash_table_find(interfaces,
					network_nl80211_findapvif, masterinterface->ifname);
			g_assert(ap != NULL);
			apinterfacename = ap->ifname;
			apinterfaceindex = ap->ifidx;

			g_message("reusing existing interface %s", apinterfacename);

			ret = TRUE;
		} else {
			g_message("FIXME: Add handling half configured situations");
		}
	}
	g_hash_table_unref(interfaces);

	return ret;
}

int network_start() {
	if (!network_setupinterfaces())
		return -1;
	network_wpasupplicant_start(&wpa_ctrl_sta, interfacename,
			&stasupplicantpid);
	network_dhcpclient_start(interfacename);
	return 0;
}

int network_waitforinterface() {
	//TODO this probably shouldn't poll
	while (TRUE) {
		GHashTable* interfaces = network_nl80211_listwifiinterfaces();
		if (g_hash_table_contains(interfaces, interfacename))
			break;
		g_hash_table_unref(interfaces);
		g_message("waiting for interface to appear");
		g_usleep(10 * 1000000);
	}
	return 0;
}

int network_stop() {
	network_dhcpclient_stop();
	network_wpasupplicant_stop(wpa_ctrl_sta, &stasupplicantpid);
	return 0;
}

int network_startap() {
	if (noapinterface)
		return 0;

	network_netlink_setipv4addr(apinterfaceindex, "10.0.0.1/30");
	if (!network_wpasupplicant_start(&wpa_ctrl_ap, apinterfacename,
			&apsupplicantpid))
		goto err_startsupp;

	network_wpasupplicant_addnetwork(wpa_ctrl_ap, "mythingy",
			"reallysecurepassword",
			WPASUPPLICANT_NETWORKMODE_AP);
	network_wpasupplicant_selectnetwork(wpa_ctrl_ap, 0);
	network_dhcpserver_start(apinterfacename);

	err_startsupp:			//
	return 0;
}

int network_stopap() {
	if (!noapinterface)
		return 0;
	return 0;
}

GPtrArray* network_scan() {
	network_wpasupplicant_scan(wpa_ctrl_sta);
	GPtrArray* scanresults = network_wpasupplicant_getlastscanresults();
	return scanresults;
}

void network_addnetwork(struct network_config* ntwkcfg) {
	network_wpasupplicant_addnetwork(wpa_ctrl_sta, ntwkcfg->ssid, ntwkcfg->psk,
	WPASUPPLICANT_NETWORKMODE_STA);
	gsize respsz;
	gchar* resp;
	network_wpasupplicant_selectnetwork(wpa_ctrl_sta, 0);
}

struct network_config* network_parseconfig(JsonNode* root) {
	if (json_node_get_node_type(root) == JSON_NODE_OBJECT) {
		JsonObject* rootobj = json_node_get_object(root);
		if (json_object_has_member(rootobj, "ssid")
				&& json_object_has_member(rootobj, "psk")) {
			struct network_config* ntwkcfg = g_malloc0(
					sizeof(struct network_config));
			const gchar* ssid = json_object_get_string_member(rootobj, "ssid");
			const gchar* psk = json_object_get_string_member(rootobj, "psk");
			strcpy(ntwkcfg->ssid, ssid);
			strcpy(ntwkcfg->psk, psk);
			return ntwkcfg;
		} else
			g_message("network config is missing required fields");
	} else
		g_message("root of network config should be an object");

	return NULL;
}
