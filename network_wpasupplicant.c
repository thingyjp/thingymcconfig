#include "buildconfig.h"
#include "network_wpasupplicant.h"

static GPtrArray* scanresults;
static char* wpasupplicantsocketdir = "/tmp/thingy_sockets/";

static gboolean network_wpasupplicant_onevent(GIOChannel *source,
		GIOCondition condition, gpointer data) {
	struct wpa_ctrl* wpa_ctrl = data;
	size_t replylen = 1024;
	char* reply = g_malloc0(replylen + 1);
	wpa_ctrl_recv(wpa_ctrl, reply, &replylen);

#ifdef WSDEBUG
	g_message("event for wpa supplicant: %s", reply);
#endif

	GRegex* regex = g_regex_new("^<([0-4])>([A-Z,-]* )", 0, 0, NULL);
	GMatchInfo* matchinfo;
	if (g_regex_match(regex, reply, 0, &matchinfo)) {
		char* level = g_match_info_fetch(matchinfo, 1);
		char* command = g_match_info_fetch(matchinfo, 2);

#ifdef WSDEBUG
		g_message("level: %s, command %s", level, command);
#endif

		if (strcmp(command, WPA_EVENT_SCAN_RESULTS) == 0) {
			g_message("have scan results");
			network_wpasupplicant_getscanresults(wpa_ctrl);
		} else if (strcmp(command, AP_STA_CONNECTED) == 0) {

		} else if (strcmp(command, AP_STA_DISCONNECTED) == 0) {

		}

		g_free(level);
		g_free(command);

	}
	g_match_info_free(matchinfo);
	g_regex_unref(regex);

	g_free(reply);
	return TRUE;
}

static gchar* network_wpasupplicant_docommand(struct wpa_ctrl* wpa_ctrl,
		const char* command, gsize* replylen) {
	*replylen = 1024;
	char* reply = g_malloc0(*replylen + 1);
	if (wpa_ctrl_request(wpa_ctrl, command, strlen(command), reply, replylen,
	NULL) == 0) {
#ifdef WSDEBUG
		g_message("command: %s, response: %s", command, reply);
#endif
		return reply;
	} else {
		g_free(reply);
		return NULL;
	}
}

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

void network_wpasupplicant_scan(struct wpa_ctrl* wpa_ctrl) {
	size_t replylen;
	char* reply = network_wpasupplicant_docommand(wpa_ctrl, "SCAN", &replylen);
	if (reply != NULL) {
		g_free(reply);
	}
}

void network_wpasupplicant_getscanresults(struct wpa_ctrl* wpa_ctrl) {
	size_t replylen;
	char* reply = network_wpasupplicant_docommand(wpa_ctrl, "SCAN_RESULTS",
			&replylen);
	if (reply != NULL) {
		//g_message("%s\n sr: %s", SCANRESULTREGEX, reply);
		GRegex* networkregex = g_regex_new(SCANRESULTREGEX, 0, 0, NULL);
		GMatchInfo* matchinfo;
		g_regex_match(networkregex, reply, 0, &matchinfo);
		while (g_match_info_matches(matchinfo)) {
			//char* line = g_match_info_fetch(matchinfo, 0);
			//g_message("l %s", line);

			char* bssid = g_match_info_fetch(matchinfo, 1);

			char* frequencystr = g_match_info_fetch(matchinfo, 2);
			int frequency = g_ascii_strtoll(frequencystr, NULL, 10);
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

void network_wpasupplicant_addnetwork(struct wpa_ctrl* wpa_ctrl,
		const gchar* ssid, const gchar* psk, unsigned mode) {
	g_message("adding network %s with psk %s", ssid, psk);
	gsize respsz;
	gchar* resp = network_wpasupplicant_docommand(wpa_ctrl, "ADD_NETWORK",
			&respsz);
	g_free(resp);

	{
		GString* ssidcmdstr = g_string_new(NULL);
		g_string_printf(ssidcmdstr, "SET_NETWORK %d ssid \"%s\"", 0, ssid);
		gchar* ssidcmd = g_string_free(ssidcmdstr, FALSE);
		resp = network_wpasupplicant_docommand(wpa_ctrl, ssidcmd, &respsz);
		g_free(ssidcmd);
		g_free(resp);
	}

	{
		GString* pskcmdstr = g_string_new(NULL);
		g_string_printf(pskcmdstr, "SET_NETWORK %d psk \"%s\"", 0, psk);
		gchar* pskcmd = g_string_free(pskcmdstr, FALSE);
		resp = network_wpasupplicant_docommand(wpa_ctrl, pskcmd, &respsz);
		g_free(pskcmd);
		g_free(resp);
	}

	{
		GString* modecmdstr = g_string_new(NULL);
		g_string_printf(modecmdstr, "SET_NETWORK %d mode %d", 0, mode);
		gchar* modecmd = g_string_free(modecmdstr, FALSE);
		resp = network_wpasupplicant_docommand(wpa_ctrl, modecmd, &respsz);
		g_free(modecmd);
		g_free(resp);
	}
}

void network_wpasupplicant_selectnetwork(struct wpa_ctrl* wpa_ctrl, int which) {
	gsize respsz;
	GString* selectcmdstr = g_string_new(NULL);
	g_string_printf(selectcmdstr, "SELECT_NETWORK %d", 0);
	gchar* selectcmd = g_string_free(selectcmdstr, FALSE);
	gchar* resp = network_wpasupplicant_docommand(wpa_ctrl, selectcmd, &respsz);
	g_free(selectcmd);
	g_free(resp);
}

void network_wpasupplicant_init() {
	scanresults = g_ptr_array_new();
}

gboolean network_wpasupplicant_start(struct wpa_ctrl** wpa_ctrl,
		const char* interface, GPid* pid) {
	gboolean ret = FALSE;
	g_message("starting wpa_supplicant for %s", interface);
	gchar* args[] = { WPASUPPLICANT_BINARYPATH, "-Dnl80211", "-i", interface,
			"-C", wpasupplicantsocketdir, "-qq", NULL };
	if (!g_spawn_async(NULL, args, NULL,
			G_SPAWN_STDOUT_TO_DEV_NULL | G_SPAWN_STDERR_TO_DEV_NULL, NULL, NULL,
			pid, NULL)) {
		g_message("failed to start wpa_supplicant");
		goto err_spawn;
	}

	g_usleep(2 * 1000000);

	GString* socketpathstr = g_string_new(NULL);
	g_string_printf(socketpathstr, "%s/%s", wpasupplicantsocketdir, interface);
	gchar* socketpath = g_string_free(socketpathstr, FALSE);

	*wpa_ctrl = wpa_ctrl_open(socketpath);
	if (*wpa_ctrl) {
		wpa_ctrl_attach(*wpa_ctrl);
		int fd = wpa_ctrl_get_fd(*wpa_ctrl);
		GIOChannel* channel = g_io_channel_unix_new(fd);
		g_io_add_watch(channel, G_IO_IN, network_wpasupplicant_onevent,
				*wpa_ctrl);
		g_message("wpa_supplicant running, control interface connected");
	} else {
		g_message("failed to open wpa_supplicant control interface");
		goto err_openctrlsck;
	}

	ret = TRUE;

	err_openctrlsck:	//
	g_free(socketpath);
	err_spawn:			//
	return ret;
}

void network_wpasupplicant_stop(struct wpa_ctrl* wpa_ctrl, GPid* pid) {

}
