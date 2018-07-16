#include <sys/types.h>
#include <netlink/socket.h>
#include <netlink/genl/genl.h>
#include <netlink/genl/ctrl.h>
#include <linux/if.h>

#include "buildconfig.h"
#include "network.h"
#include "network_priv.h"
#include "network_nl80211.h"
#include "network_rtnetlink.h"
#include "network_wpasupplicant.h"
#include "network_dhcp.h"
#include "config.h"
#include "jsonbuilderutils.h"

#define NUMBEROFINTERFACESWHENCONFIGURED 2

static const char* interfacename;
struct network_interface *stainterface, *apinterface;

static char* apinterfacename;

static gboolean noapinterface;

struct wpa_ctrl* wpa_ctrl_sta = NULL;
struct wpa_ctrl* wpa_event_sta = NULL;
struct wpa_ctrl* wpa_ctrl_ap = NULL;
struct wpa_ctrl* wpa_event_ap = NULL;

static GPid stasupplicantpid, apsupplicantpid;

enum NETWORK_CONFIGURATION_STATE {
	NTWKST_UNCONFIGURED, NTWKST_INPROGRESS, NTWKST_CONFIGURED
};

static enum NETWORK_CONFIGURATION_STATE configurationstate = NTWKST_UNCONFIGURED;
static guint timeoutsource;
static struct network_config* networkbeingconfigured;

gboolean network_init(const char* interface, gboolean noap) {
	interfacename = interface;
	noapinterface = noap;

	if (!network_nl80211_init())
		goto err_nl80211init;

	if (!network_rtnetlink_init())
		goto err_rtnetlinkinit;

	network_wpasupplicant_init();
	return TRUE;

	err_rtnetlinkinit:			//
	err_nl80211init:			//
	return FALSE;
}

void network_cleanup() {

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

	struct network_interface* sta;
	struct network_interface* ap;

	if (network_nl80211_findphy(interfaces, interfacename, &wiphy)) {
		network_filteroutoutherphys(interfaces, wiphy);
		int remainingnetworks = g_hash_table_size(interfaces);
		if (remainingnetworks == 1) {
			g_message("ap interface is missing, will create");
			sta = g_hash_table_lookup(interfaces, interfacename);
			g_assert(sta != NULL);
			// create the ap VIF
			{
				if (!noapinterface) {
					network_nl80211_createapvif(interfaces, sta);
					ap = g_hash_table_find(interfaces,
							network_nl80211_findapvif, sta->ifname);
					g_assert(ap != NULL);
					g_message("AP interface created -> %s", ap->ifname);
					apinterfacename = ap->ifname;
				}
			}

			ret = TRUE;
		} else if (remainingnetworks == NUMBEROFINTERFACESWHENCONFIGURED) {
			sta = g_hash_table_lookup(interfaces, interfacename);
			g_assert(sta != NULL);
			ap = g_hash_table_find(interfaces, network_nl80211_findapvif,
					sta->ifname);
			g_assert(ap != NULL);
			apinterfacename = ap->ifname;
			g_message("reusing existing interface %s", apinterfacename);
			ret = TRUE;
		} else {
			g_message("FIXME: Add handling half configured situations");
		}
	}

	if (ret) {
		stainterface = g_malloc(sizeof(*stainterface));
		memcpy(stainterface, sta, sizeof(*stainterface));
		apinterface = g_malloc(sizeof(*apinterface));
		memcpy(apinterface, ap, sizeof(*apinterface));
	}

	g_hash_table_unref(interfaces);

	return ret;
}

int network_start() {
	if (!network_setupinterfaces())
		return -1;
	network_wpasupplicant_start(&wpa_ctrl_sta, &wpa_event_sta, interfacename,
			&stasupplicantpid);
	network_dhcpclient_start(stainterface->ifidx, interfacename,
			stainterface->mac);

	const struct config* cfg = config_getconfig();
	if (cfg->ntwkcfg != NULL) {
		int networkid = network_wpasupplicant_addnetwork(wpa_ctrl_sta,
				cfg->ntwkcfg->ssid, cfg->ntwkcfg->psk,
				WPASUPPLICANT_NETWORKMODE_STA);
		network_wpasupplicant_selectnetwork(wpa_ctrl_sta, networkid);
		configurationstate = NTWKST_CONFIGURED;
	}

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

static const gchar thingyidstr[] = "thingymcconfig:0";
static const struct network_wpasupplicant_ie ies[] = { { .id = 0xDD, .payload =
		(const guint8*) thingyidstr, .payloadlen = sizeof(thingyidstr) - 1 } };

int network_startap(const gchar* nameprefix) {
	if (noapinterface)
		return 0;

	GString* namestr = g_string_new(nameprefix);
	g_string_append_printf(namestr, "_%02x%02x%02x", apinterface->mac[3],
			apinterface->mac[4], apinterface->mac[5]);
	gchar* name = g_string_free(namestr, FALSE);

	network_rtnetlink_clearipv4addr(apinterface->ifidx);
	network_rtnetlink_setipv4addr(apinterface->ifidx, "10.0.0.1/29");
	if (!network_wpasupplicant_start(&wpa_ctrl_ap, &wpa_event_ap,
			apinterfacename, &apsupplicantpid))
		goto err_startsupp;

	network_wpasupplicant_seties(wpa_ctrl_ap, ies, G_N_ELEMENTS(ies));
	network_wpasupplicant_addnetwork(wpa_ctrl_ap, name, "reallysecurepassword",
	WPASUPPLICANT_NETWORKMODE_AP);
	network_wpasupplicant_selectnetwork(wpa_ctrl_ap, 0);
	network_dhcpserver_start(apinterface->ifidx, apinterfacename,
			apinterface->mac);

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

static gboolean network_configure_timeout(gpointer data) {
	g_message("config timeout");
	return FALSE;
}

gboolean network_configure(struct network_config* ntwkcfg) {
	if (configurationstate != NTWKST_UNCONFIGURED)
		return FALSE;

	configurationstate = NTWKST_INPROGRESS;

	networkbeingconfigured = ntwkcfg;

	int networkid = network_wpasupplicant_addnetwork(wpa_ctrl_sta,
			ntwkcfg->ssid, ntwkcfg->psk,
			WPASUPPLICANT_NETWORKMODE_STA);
	network_wpasupplicant_selectnetwork(wpa_ctrl_sta, networkid);

	timeoutsource = g_timeout_add(60 * 1000, network_configure_timeout,
	NULL);

	return TRUE;
}

static const gchar* configstatestrings[] = { [NTWKST_UNCONFIGURED
		] = "unconfigured", [NTWKST_INPROGRESS] = "inprogress",
		[NTWKST_CONFIGURED] = "configured" };

void network_dumpstatus(JsonBuilder* builder) {
	JSONBUILDER_START_OBJECT(builder, "network");
	JSONBUILDER_ADD_STRING(builder, "config_state",
			configstatestrings[configurationstate]);
	network_wpasupplicant_dumpstatus(builder);
	network_dhcp_dumpstatus(builder);
	json_builder_end_object(builder);
}

static void network_checkconfigurationstate() {
	if (configurationstate == NTWKST_INPROGRESS) {
		g_source_remove(timeoutsource);
		configurationstate = NTWKST_CONFIGURED;
		config_onnetworkconfigured(networkbeingconfigured);
		g_free(networkbeingconfigured);
		g_message("configuration complete");
	}
}

void network_onsupplicantstatechange(gboolean connected) {
	network_checkconfigurationstate();
}
