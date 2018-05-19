#include <sys/types.h>
#include <wpa_ctrl.h>
#include "network.h"

struct wpa_ctrl* wpa_ctrl = NULL;
static GPtrArray* scanresults;

#define MATCHBSSID "((?:[0-9a-f]{2}:{0,1}){6})"
#define MATCHFREQ "([1-9]{4})"
#define MATCHRSSI "(-[1-9]{2,3})"
#define FLAGPATTERN "[A-Z2\\-\\+]*"
#define MATCHFLAG "\\[("FLAGPATTERN")\\]"
#define MATCHFLAGS "((?:\\["FLAGPATTERN"\\]){1,})"
#define MATCHSSID "([a-zA-Z0-9\\-]*)"
#define SCANRESULTREGEX MATCHBSSID"\\s*"MATCHFREQ"\\s*"MATCHRSSI"\\s*"MATCHFLAGS"\\s*"MATCHSSID

typedef enum {
	NF_ESS = 1, //
	NF_WPS = 1 << 1, //
	NF_WEP = 1 << 2, //
	NF_WPA_PSK_CCMP = 1 << 3, //
	NF_WPA_PSK_CCMP_TKIP = 1 << 4, //
	NF_WPA2_PSK_CCMP = 1 << 5, //
	NF_WPA2_PSK_CCMP_TKIP = 1 << 6
} network_flags;

#define FLAG_ESS "ESS"
#define FLAG_WPS "WPS"
#define FLAG_WEP "WEP"
#define FLAG_WPA_PSK_CCMP "WPA-PSK-CCMP"
#define FLAG_WPA_PSK_CCMP_TKIP "WPA-PSK-CCMP+TKIP"
#define FLAG_WPA2_PSK_CCMP "WPA2-PSK-CCMP"
#define FLAG_WPA2_PSK_CCMP_TKIP "WPA2-PSK-CCMP+TKIP"

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
		g_message("%s\n sr: %s", SCANRESULTREGEX, reply);
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

static void network_wpasupplicant_start() {
	char* interface = "wlxdcfb02a63a12";
	char* wpapath = "/tmp/omnomnom";
	g_message("starting wpa_supplicant for %s", interface);
	gchar* args[] = { "/sbin/wpa_supplicant", "-Dnl80211", "-i", interface,
			"-C", wpapath, NULL };
	g_spawn_async(NULL, args, NULL, G_SPAWN_DEFAULT, NULL, NULL, NULL,
	NULL);

	g_usleep(2 * 1000000);

	GString* socketpathstr = g_string_new(NULL);
	g_string_printf(socketpathstr, "%s/%s", wpapath, interface);
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

void network_dhcpclient_start() {
	char* interface = "wlxdcfb02a63a12";
	g_message("starting dhcpclient for %s", interface);
	gchar* args[] = { "/bin/busybox", "udhcpc", "-i", interface, NULL };
	g_spawn_async(NULL, args, NULL, G_SPAWN_DEFAULT, NULL, NULL, NULL,
	NULL);
}

void network_dhcpclient_stop() {

}

void network_init() {
	scanresults = g_ptr_array_new();
}

int network_start() {
	network_wpasupplicant_start();
	network_dhcpclient_start();
	return 0;
}

int network_stop() {
	return 0;
}

GPtrArray* network_scan() {
	network_wpasupplicant_scan();
	return scanresults;
}

void network_addnetwork(struct network_config* ntwkcfg) {
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
