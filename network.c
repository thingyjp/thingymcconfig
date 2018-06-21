#include <sys/types.h>
#include <wpa_ctrl.h>
#include <netlink/socket.h>
#include <netlink/genl/genl.h>
#include <netlink/genl/ctrl.h>
#include <netlink/route/route.h>
#include <netlink/route/link.h>
#include <linux/nl80211.h>
#include <linux/if.h>
#include "network.h"
#include "network_priv.h"
#include "buildconfig.h"

#define IFNAMESUFFIX_STA	"sta"
#define IFNAMESUFFIX_AP		"ap"

static const char* masterinterfacename;
static char* stainterfacename;
static char* apinterfacename;
static char* wpasupplicantsocketdir = "/tmp/thingy_sockets/";
static gboolean noapinterface;

struct wpa_ctrl* wpa_ctrl = NULL;
static GPtrArray* scanresults;

static struct nl_sock* nl80211sock;
static struct nl_sock* routesock;
static int nl80211id;

static GPid udhcpcpid, udhcpdpid, stasupplicantpid, apsupplicantpid;

#define MATCHBSSID "((?:[0-9a-f]{2}:{0,1}){6})"
#define MATCHFREQ "([1-9]{4})"
#define MATCHRSSI "(-[1-9]{2,3})"
#define FLAGPATTERN "[A-Z2\\-\\+]*"
#define MATCHFLAG "\\[("FLAGPATTERN")\\]"
#define MATCHFLAGS "((?:\\["FLAGPATTERN"\\]){1,})"
#define MATCHSSID "([a-zA-Z0-9\\-]*)"
#define SCANRESULTREGEX MATCHBSSID"\\s*"MATCHFREQ"\\s*"MATCHRSSI"\\s*"MATCHFLAGS"\\s*"MATCHSSID

static unsigned network_wpasupplicant_getscanresults_flags(const char* flags) {
	unsigned f = 0;
	GRegex* flagregex = g_regex_new(MATCHFLAG, 0, 0, NULL);
	GMatchInfo* flagsmatchinfo;
	g_regex_match(flagregex, flags, 0, &flagsmatchinfo);
	while (g_match_info_matches(flagsmatchinfo)) {
		char* flag = g_match_info_fetch(flagsmatchinfo, 1);
		if (strcmp(flag, FLAG_ESS) == 0)
			f |= NF_ESS;
		else if (strcmp(flag, FLAG_WPS) == 0)
			f |= NF_WPS;
		else if (strcmp(flag, FLAG_WEP) == 0)
			f |= NF_WEP;
		else if (strcmp(flag, FLAG_WPA_PSK_CCMP) == 0)
			f |= NF_WPA_PSK_CCMP;
		else if (strcmp(flag, FLAG_WPA_PSK_CCMP_TKIP) == 0)
			f |= NF_WPA_PSK_CCMP_TKIP;
		else if (strcmp(flag, FLAG_WPA2_PSK_CCMP) == 0)
			f |= NF_WPA2_PSK_CCMP;
		else if (strcmp(flag, FLAG_WPA2_PSK_CCMP_TKIP) == 0)
			f |= NF_WPA2_PSK_CCMP_TKIP;
		else
			g_message("unhandled flag: %s", flag);
		g_free(flag);
		g_match_info_next(flagsmatchinfo, NULL);
	}
	g_match_info_free(flagsmatchinfo);
	g_regex_unref(flagregex);
	return f;
}

static gchar* network_wpasupplicant_docommand(const char* command,
		gsize* replylen) {
	*replylen = 1024;
	char* reply = g_malloc0(*replylen + 1);
	if (wpa_ctrl_request(wpa_ctrl, command, strlen(command), reply, replylen,
	NULL) == 0) {
		g_message("command: %s, response: %s", command, reply);
		return reply;
	} else {
		g_free(reply);
		return NULL;
	}
}

static void network_wpasupplicant_scan() {
	size_t replylen;
	char* reply = network_wpasupplicant_docommand("SCAN", &replylen);
	if (reply != NULL) {
		g_free(reply);
	}
}

static void network_wpasupplicant_getscanresults() {
	size_t replylen;
	char* reply = network_wpasupplicant_docommand("SCAN_RESULTS", &replylen);
	if (reply != NULL) {
		//g_message("%s\n sr: %s", SCANRESULTREGEX, reply);
		GRegex* networkregex = g_regex_new(SCANRESULTREGEX, 0, 0,
		NULL);
		GMatchInfo* matchinfo;
		g_regex_match(networkregex, reply, 0, &matchinfo);
		while (g_match_info_matches(matchinfo)) {
			//char* line = g_match_info_fetch(matchinfo, 0);
			//g_message("l %s", line);

			char* bssid = g_match_info_fetch(matchinfo, 1);

			char* frequencystr = g_match_info_fetch(matchinfo, 2);
			int frequency = g_ascii_strtoll(frequencystr,
			NULL, 10);
			char* rssistr = g_match_info_fetch(matchinfo, 3);
			int rssi = g_ascii_strtoll(rssistr, NULL, 10);
			char* flags = g_match_info_fetch(matchinfo, 4);
			char* ssid = g_match_info_fetch(matchinfo, 5);

			struct network_scanresult* n = g_malloc0(
					sizeof(struct network_scanresult));
			strncpy(n->bssid, bssid, sizeof(n->bssid));
			n->frequency = frequency;
			n->rssi = rssi;
			n->flags = network_wpasupplicant_getscanresults_flags(flags);
			strncpy(n->ssid, ssid, sizeof(n->ssid));
			g_ptr_array_add(scanresults, n);

			g_free(bssid);
			g_free(frequencystr);
			g_free(rssistr);
			g_free(flags);
			g_free(ssid);

			g_message("bssid %s, frequency %d, rssi %d, flags %u, ssid %s",
					n->bssid, n->frequency, n->rssi, n->flags, n->ssid);

			g_match_info_next(matchinfo, NULL);
		}
		g_match_info_free(matchinfo);
		g_regex_unref(networkregex);
	}
}

static gboolean network_wpasupplicant_onevent(GIOChannel *source,
		GIOCondition condition, gpointer data) {
	g_message("have event from wpa_supplicant");
	size_t replylen = 1024;
	char* reply = g_malloc0(replylen + 1);
	wpa_ctrl_recv(wpa_ctrl, reply, &replylen);
	g_message("event: %s", reply);

	GRegex* regex = g_regex_new("^<([0-4])>([A-Z,-]* )", 0, 0, NULL);
	GMatchInfo* matchinfo;
	if (g_regex_match(regex, reply, 0, &matchinfo)) {
		char* level = g_match_info_fetch(matchinfo, 1);
		char* command = g_match_info_fetch(matchinfo, 2);
		g_message("level: %s, command %s", level, command);

		if (strcmp(command, WPA_EVENT_SCAN_RESULTS) == 0) {
			g_message("have scan results");
			network_wpasupplicant_getscanresults();
		}

		g_free(level);
		g_free(command);

	}
	g_match_info_free(matchinfo);
	g_regex_unref(regex);

	g_free(reply);
	return TRUE;
}

static void network_wpasupplicant_start(char* interface, GPid* pid) {
	g_message("starting wpa_supplicant for %s", interface);
	gchar* args[] = { "/sbin/wpa_supplicant", "-Dnl80211", "-i", interface,
			"-C", wpasupplicantsocketdir, "-qq", NULL };
	g_spawn_async(NULL, args, NULL, G_SPAWN_DEFAULT, NULL, NULL, pid,
	NULL);

	g_usleep(2 * 1000000);

	GString* socketpathstr = g_string_new(NULL);
	g_string_printf(socketpathstr, "%s/%s", wpasupplicantsocketdir, interface);
	gchar* socketpath = g_string_free(socketpathstr, FALSE);

	wpa_ctrl = wpa_ctrl_open(socketpath);
	if (wpa_ctrl) {
		wpa_ctrl_attach(wpa_ctrl);
		int fd = wpa_ctrl_get_fd(wpa_ctrl);
		GIOChannel* channel = g_io_channel_unix_new(fd);
		g_io_add_watch(channel, G_IO_IN, network_wpasupplicant_onevent,
		NULL);
		g_message("wpa_supplicant running, control interface connected");
	} else
		g_message("failed to open wpa_supplicant control interface");

	g_free(socketpath);
}

static void network_wpasupplicant_stop() {

}

void network_dhcpclient_start() {
	g_message("starting dhcp client for %s", stainterfacename);
	gchar* args[] = { "/bin/busybox", "udhcpc", "-i", stainterfacename, NULL };
	g_spawn_async(NULL, args, NULL, G_SPAWN_DEFAULT, NULL, NULL, &udhcpcpid,
	NULL);
}

void network_dhcpclient_stop() {
	g_spawn_close_pid(udhcpcpid);
}

void network_dhcpserver_start() {
	g_message("starting dhcp server for %s", stainterfacename);
	gchar* args[] = { "/bin/busybox", "udhcpd", "-i", stainterfacename, NULL };
	g_spawn_async(NULL, args, NULL, G_SPAWN_DEFAULT, NULL, NULL, &udhcpdpid,
	NULL);
}

void network_dhcpserver_stop() {
	g_spawn_close_pid(udhcpdpid);
}

static void network_interface_free(gpointer data) {
	struct network_interface* interface = data;
	g_free(interface);
}

static int network_netlink_interface_callback(struct nl_msg *msg, void *arg) {
#ifdef NLDEBUG
	nl_msg_dump(msg, stdout);
#endif

	struct genlmsghdr *genlhdr = nlmsg_data(nlmsg_hdr(msg));
	int attrlen = genlmsg_attrlen(genlhdr, 0);
	struct nlattr* attrs = genlmsg_attrdata(genlhdr, 0);

	struct network_interface* interface = g_malloc0(sizeof(*interface));
	struct nlattr *nla;
	int rem;
	nla_for_each_attr(nla, attrs, attrlen, rem)
	{
		switch (nla_type(nla)) {
		case NL80211_ATTR_IFNAME:
			interface->ifname = g_strdup(nla_get_string(nla));
			break;
		case NL80211_ATTR_WIPHY:
			interface->wiphy = nla_get_u32(nla);
			break;
		case NL80211_ATTR_IFTYPE: {
			uint32_t type = nla_get_u32(nla);
			interface->ap = type == NL80211_IFTYPE_AP;
		}
			break;
		case NL80211_ATTR_IFINDEX:
			interface->ifidx = nla_get_u32(nla);
			break;
		}
	}
	GHashTable* interfaces = arg;
	g_hash_table_insert(interfaces, interface->ifname, interface);

	return NL_SKIP;
}

static void network_netlink_sendmsgandfree(struct nl_msg* msg) {
	int ret;
	if ((ret = nl_send_sync(nl80211sock, msg)) < 0) {
		g_message("failed to send nl message -> %d", ret);
		nlmsg_free(msg);
	}
	//nl_recvmsgs_default(nlsock);

}

static GHashTable* network_netlink_listinterfaces() {
	struct nl_msg* msg = nlmsg_alloc();
	if (msg == NULL) {
		g_message(NETWORK_ERRMSG_FAILEDTOALLOCNLMSG);
		goto err_allocmsg;
	}

	GHashTable* interfaces = g_hash_table_new_full(g_str_hash, g_str_equal,
	NULL, network_interface_free);

	nl_socket_modify_cb(nl80211sock, NL_CB_VALID, NL_CB_CUSTOM,
			network_netlink_interface_callback, interfaces);
	genlmsg_put(msg, NL_AUTO_PORT, NL_AUTO_SEQ, nl80211id, 0, NLM_F_DUMP,
			NL80211_CMD_GET_INTERFACE, 0);

	network_netlink_sendmsgandfree(msg);

	return interfaces;

	err_allocmsg: //
	return NULL;
}

static gboolean network_netlink_init() {
	nl80211sock = nl_socket_alloc();
	if (nl80211sock == NULL) {
		g_message("failed to create nl80211 socket");
		goto err_sock;
	}
	if (genl_connect(nl80211sock)) {
		g_message("failed to connect nl80211 socket");
		goto err_connect;
	}

	nl80211id = genl_ctrl_resolve(nl80211sock, "nl80211");
	if (nl80211id == 0) {
		g_message("failed to resolve nl80211");
		goto err_resolve;
	}

	routesock = nl_socket_alloc();
	nl_connect(routesock, NETLINK_ROUTE);

	return TRUE;

	err_resolve: //
	err_connect: //
	nl_socket_free(nl80211sock);
	err_sock: //
	return FALSE;
}

void network_init(const char* interface, gboolean noap) {
	masterinterfacename = interface;
	noapinterface = noap;
	network_netlink_init();
	scanresults = g_ptr_array_new();
}

void network_cleanup() {

}

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
}

static gchar* network_generatevifname(const gchar* masterifname,
		const gchar* suffix) {
	GString* interfacenamestr = g_string_new(NULL);
	g_string_printf(interfacenamestr, "%s_%s", masterifname, suffix);
	gchar* interfacename = g_string_free(interfacenamestr, FALSE);
	return interfacename;
}

static void network_netlink_createvif(GHashTable* interfaces,
		struct network_interface* masterinterface, const gchar* suffix,
		guint32 type) {

	struct nl_msg* msg = nlmsg_alloc();
	if (msg == NULL) {
		g_message(NETWORK_ERRMSG_FAILEDTOALLOCNLMSG);
		goto err_allocmsg;
	}

	gchar* interfacename = network_generatevifname(masterinterface->ifname,
			suffix);

	g_message("creating a VIF called %s on %d", interfacename,
			masterinterface->wiphy);

	nl_socket_modify_cb(nl80211sock, NL_CB_VALID, NL_CB_CUSTOM,
			network_netlink_interface_callback, interfaces);
	genlmsg_put(msg, NL_AUTO_PORT, NL_AUTO_SEQ, nl80211id, 0, 0,
			NL80211_CMD_NEW_INTERFACE, 0);
	nla_put_string(msg, NL80211_ATTR_IFNAME, interfacename);
	nla_put_u32(msg, NL80211_ATTR_IFTYPE, type);
	nla_put_u32(msg, NL80211_ATTR_WIPHY, masterinterface->wiphy);

	network_netlink_sendmsgandfree(msg);

	g_free(interfacename);

	return;

	err_allocmsg: return;

}

static void network_createapinterface(GHashTable* interfaces,
		struct network_interface* masterinterface) {
	network_netlink_createvif(interfaces, masterinterface, IFNAMESUFFIX_AP,
			NL80211_IFTYPE_AP);
}

static void network_createstainterface(GHashTable* interfaces,
		struct network_interface* masterinterface) {
	network_netlink_createvif(interfaces, masterinterface, IFNAMESUFFIX_STA,
			NL80211_IFTYPE_STATION);
}

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

static gboolean network_findphy(GHashTable* interfaces,
		const gchar* interfacename, guint32* wiphy) {
	if (g_hash_table_contains(interfaces, interfacename)) {
		struct network_interface* interface = g_hash_table_lookup(interfaces,
				interfacename);
		*wiphy = interface->wiphy;
		return TRUE;
	}
	return FALSE;
}

static gboolean network_findapinterface(gpointer key, gpointer value,
		gpointer user_data) {
	gchar* masterinterfacename = user_data;
	struct network_interface* interface = value;
#ifdef DEBUG
	g_message("interface %s, is ap %d, %s", interface->ifname,
			(int )interface->ap, masterinterfacename);
#endif
	//return strcmp(masterinterfacename, interface->ifname) != 0 && interface->ap;

	gchar* apifname = network_generatevifname(masterinterfacename,
	IFNAMESUFFIX_AP);
	gboolean ret = strcmp(interface->ifname, apifname) == 0;
	g_free(apifname);
	return ret;
}

static gboolean network_findstainterface(gpointer key, gpointer value,
		gpointer user_data) {
	gchar* masterinterfacename = user_data;
	struct network_interface* interface = value;

	//return strcmp(masterinterfacename, interface->ifname) != 0 && !interface->ap;

	gchar* staifname = network_generatevifname(masterinterfacename,
	IFNAMESUFFIX_STA);
	gboolean ret = strcmp(interface->ifname, staifname) == 0;
	g_free(staifname);
	return ret;
}

static void network_setupinterfaces() {
	GHashTable* interfaces = network_netlink_listinterfaces();
	guint32 wiphy;
	if (network_findphy(interfaces, masterinterfacename, &wiphy)) {
		network_filteroutoutherphys(interfaces, wiphy);
		int remainingnetworks = g_hash_table_size(interfaces);
		if (remainingnetworks == 1) {
			network_netlink_setlinkstate(
					g_hash_table_lookup(interfaces, masterinterfacename),
					FALSE);

			g_message("interfaces missing.. will create");
			struct network_interface* masterinterface;
			masterinterface = g_hash_table_lookup(interfaces,
					masterinterfacename);
			g_assert(masterinterface != NULL);

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
			// create the ap VIF
			{
				if (!noapinterface) {
					network_createapinterface(interfaces, masterinterface);
					struct network_interface* apinterface = g_hash_table_find(
							interfaces, network_findapinterface,
							masterinterface->ifname);
					g_assert(apinterface != NULL);
					g_message("AP interface created -> %s",
							apinterface->ifname);
					apinterfacename = apinterface->ifname;
				}
			}
		} else if (remainingnetworks == 3) {
			struct network_interface* masterinterface;
			masterinterface = g_hash_table_lookup(interfaces,
					masterinterfacename);
			g_assert(masterinterface != NULL);
			struct network_interface* sta = g_hash_table_find(interfaces,
					network_findstainterface, masterinterface->ifname);
			g_assert(sta != NULL);
			stainterfacename = sta->ifname;
			struct network_interface* ap = g_hash_table_find(interfaces,
					network_findapinterface, masterinterface->ifname);
			g_assert(ap != NULL);
			apinterfacename = ap->ifname;

			g_message("reusing existing interfaces %s and %s", stainterfacename,
					apinterfacename);
		} else {
			g_message(
					"FIXME: Add handling for already configured, half configured situations");
			g_assert(FALSE);
		}
	}
	g_hash_table_unref(interfaces);
}

int network_start() {
	network_setupinterfaces();
	network_wpasupplicant_start(stainterfacename, &stasupplicantpid);
	network_dhcpclient_start();
	return 0;
}

int network_stop() {
	network_dhcpclient_stop();
	network_wpasupplicant_stop();
	return 0;
}

int network_startap() {
	if (!noapinterface)
		return 0;
	network_wpasupplicant_start(apinterfacename, &apsupplicantpid);
	return 0;
}

int network_stopap() {
	if (!noapinterface)
		return 0;
	return 0;
}

GPtrArray* network_scan() {
	network_wpasupplicant_scan();
	return scanresults;
}

void network_addnetwork(struct network_config* ntwkcfg) {
	g_message("adding network %s with psk %s", ntwkcfg->ssid, ntwkcfg->psk);
	gsize respsz;
	gchar* resp = network_wpasupplicant_docommand("ADD_NETWORK", &respsz);
	g_free(resp);

	GString* ssidcmdstr = g_string_new(NULL);
	g_string_printf(ssidcmdstr, "SET_NETWORK %d ssid \"%s\"", 0, ntwkcfg->ssid);
	gchar* ssidcmd = g_string_free(ssidcmdstr, FALSE);
	resp = network_wpasupplicant_docommand(ssidcmd, &respsz);
	g_free(ssidcmd);
	g_free(resp);

	GString* pskcmdstr = g_string_new(NULL);
	g_string_printf(pskcmdstr, "SET_NETWORK %d psk \"%s\"", 0, ntwkcfg->psk);
	gchar* pskcmd = g_string_free(pskcmdstr, FALSE);
	resp = network_wpasupplicant_docommand(pskcmd, &respsz);
	g_free(pskcmd);
	g_free(resp);

	GString* selectcmdstr = g_string_new(NULL);
	g_string_printf(selectcmdstr, "SELECT_NETWORK %d", 0);
	gchar* selectcmd = g_string_free(selectcmdstr, FALSE);
	resp = network_wpasupplicant_docommand(selectcmd, &respsz);
	g_free(selectcmd);
	g_free(resp);
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
