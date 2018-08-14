#include "buildconfig.h"
#include "network_wpasupplicant_priv.h"
#include "network_priv.h"
#include "utils.h"
#include "jsonbuilderutils.h"

struct _NetworkWpaSupplicant {
	GObject parent_instance;
	struct wpa_ctrl* wpa_ctrl;
	struct wpa_ctrl* wpa_event;
	GPid pid;
	gboolean connected;
	gchar* lasterror;
};

G_DEFINE_TYPE(NetworkWpaSupplicant, network_wpasupplicant, G_TYPE_OBJECT)

static guint supplicantsignal;
static GQuark detail_connected;
static GQuark detail_disconnected;

static void network_wpasupplicant_class_init(NetworkWpaSupplicantClass *klass) {
	supplicantsignal = g_signal_newv(NETWORK_WPASUPPLICANT_SIGNAL,
	NETWORK_TYPE_WPASUPPLICANT,
			G_SIGNAL_RUN_LAST | G_SIGNAL_NO_HOOKS | G_SIGNAL_NO_RECURSE
					| G_SIGNAL_DETAILED, NULL,
			NULL, NULL, NULL, G_TYPE_NONE, 0, NULL);
	detail_connected = g_quark_from_string(
	NETWORK_WPASUPPLICANT_DETAIL_CONNECTED);
	detail_disconnected = g_quark_from_string(
	NETWORK_WPASUPPLICANT_DETAIL_DISCONNECTED);
}

static void network_wpasupplicant_init(NetworkWpaSupplicant *self) {
}

#define ISOK(rsp) (strcmp(rsp, "OK") == 0)

static GPtrArray* scanresults = NULL;
static char* wpasupplicantsocketdir = "/tmp/thingy_sockets/";

static gchar* network_wpasupplicant_docommand(struct wpa_ctrl* wpa_ctrl,
		gsize* replylen, gboolean stripnewline, const char* command) {
	*replylen = 1024;
	char* reply = g_malloc0(*replylen + 1);
	if (wpa_ctrl_request(wpa_ctrl, command, strlen(command), reply, replylen,
	NULL) == 0) {
		if (stripnewline) {
			//kill the newline
			reply[*replylen - 1] = '\0';
			*replylen -= 1;
		}
#ifdef WSDEBUG
		g_message("command: %s, response: %s", command, reply);
#endif

		return reply;
	} else {
		g_free(reply);
		return NULL;
	}
}

static gchar* network_wpasupplicant_docommandf(struct wpa_ctrl* wpa_ctrl,
		gsize* replylen, gboolean stripnewline, const char* format, ...) {
	va_list fmtargs;
	va_start(fmtargs, format);
	GString* cmdstr = g_string_new(NULL);
	g_string_vprintf(cmdstr, format, fmtargs);
	gchar* cmd = g_string_free(cmdstr, FALSE);
	va_end(fmtargs);
	gchar* reply = network_wpasupplicant_docommand(wpa_ctrl, replylen,
			stripnewline, cmd);
	g_free(cmd);
	return reply;
}

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

static void network_wpasupplicant_freescanresult(gpointer data) {
	g_free(data);
}

static void network_wpasupplicant_getscanresults(struct wpa_ctrl* wpa_ctrl,
		const gchar* event) {
	if (scanresults != NULL)
		g_ptr_array_unref(scanresults);

	scanresults = g_ptr_array_new_with_free_func(
			network_wpasupplicant_freescanresult);

	size_t replylen;
	char* reply = network_wpasupplicant_docommand(wpa_ctrl, &replylen, FALSE,
			"SCAN_RESULTS");
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

#ifdef WSDEBUG
			g_message("bssid %s, frequency %d, rssi %d, flags %u, ssid %s",
					n->bssid, n->frequency, n->rssi, n->flags, n->ssid);
#endif
			g_match_info_next(matchinfo, NULL);
		}
		g_match_info_free(matchinfo);
		g_regex_unref(networkregex);
	}
}

static void network_wpasupplicant_eventhandler_connect(
		NetworkWpaSupplicant* supplicant, const gchar* event) {
	supplicant->connected = TRUE;
	g_signal_emit(supplicant, supplicantsignal, detail_connected);
}

static void network_wpasupplicant_eventhandler_disconnect(
		NetworkWpaSupplicant* supplicant, const gchar* event) {
	supplicant->connected = FALSE;
	g_signal_emit(supplicant, supplicantsignal, detail_disconnected);
}

static GHashTable* network_wpasupplicant_getkeyvalues(const gchar* event) {
	GHashTable* result = g_hash_table_new_full(g_str_hash, g_str_equal, g_free,
			g_free);

	GRegex* keyvalueregex = g_regex_new(NETWORK_WPASUPPLICANT_REGEX_KEYVALUE, 0,
			0, NULL);
	GMatchInfo* matchinfo;
	if (g_regex_match(keyvalueregex, event, 0, &matchinfo)) {
		do {
			gchar* key = g_match_info_fetch(matchinfo, 1);
			gchar* value = g_match_info_fetch(matchinfo, 2);
			if (key != NULL && value != NULL) {
				//g_message("kv: %s=%s", key, value);
				g_hash_table_insert(result, key, value);
			}
		} while (g_match_info_next(matchinfo, NULL));
	}
	g_match_info_free(matchinfo);

	return result;
}

static void network_wpasupplicant_eventhandler_ssiddisabled(
		NetworkWpaSupplicant* supplicant, const gchar* event) {
	//<3>CTRL-EVENT-SSID-TEMP-DISABLED id=0 ssid="ghettonet" auth_failures=2 duration=23 reason=WRONG_KEY
	g_message("here");
	GHashTable* keyvalues = network_wpasupplicant_getkeyvalues(event);
	gchar* reason = g_hash_table_lookup(keyvalues, "reason");
	if (supplicant->lasterror != NULL)
		g_free(supplicant->lasterror);
	supplicant->lasterror = g_strdup(reason);
	g_hash_table_unref(keyvalues);
}

static const struct wpaeventhandler_entry eventhandlers[] = { {
WPA_EVENT_SCAN_RESULTS, network_wpasupplicant_getscanresults }, {
WPA_EVENT_CONNECTED, network_wpasupplicant_eventhandler_connect }, {
WPA_EVENT_DISCONNECTED, network_wpasupplicant_eventhandler_disconnect }, {
WPA_EVENT_TEMP_DISABLED, network_wpasupplicant_eventhandler_ssiddisabled } };

static gboolean network_wpasupplicant_onevent(GIOChannel *source,
		GIOCondition condition, gpointer data) {
	NetworkWpaSupplicant* supplicant = data;
	size_t replylen = 1024;
	char* reply = g_malloc0(replylen + 1);
	wpa_ctrl_recv(supplicant->wpa_event, reply, &replylen);

#ifdef WSDEBUG
	g_message("event for wpa supplicant(%d): %s", supplicant->pid, reply);
#endif

	GRegex* regex = g_regex_new("^<([0-4])>([A-Z,-]* )", 0, 0, NULL);
	GMatchInfo* matchinfo;
	if (g_regex_match(regex, reply, 0, &matchinfo)) {
		char* level = g_match_info_fetch(matchinfo, 1);
		char* command = g_match_info_fetch(matchinfo, 2);

#ifdef WSDEBUG
		g_message("level: %s, command %s", level, command);
#endif

		for (int i = 0; i < G_N_ELEMENTS(eventhandlers); i++) {
			if (strcmp(command, eventhandlers[i].command) == 0) {
				eventhandlers[i].handler(supplicant, reply);
				break;
			}
		}

		g_free(level);
		g_free(command);

	}
	g_match_info_free(matchinfo);
	g_regex_unref(regex);

	g_free(reply);
	return TRUE;
}

void network_wpasupplicant_seties(NetworkWpaSupplicant* supplicant,
		const struct network_wpasupplicant_ie* ies, unsigned numies) {
	if (numies > 0) {
		GString* iedatastr = g_string_new("SET ap_vendor_elements ");
		for (int i = 0; i < numies; i++) {
			g_string_append_printf(iedatastr, "%02x%02x", (unsigned) ies->id,
					(unsigned) ies->payloadlen);
			const guint8* payload = ies->payload;
			for (int j = 0; j < ies->payloadlen; j++) {
				g_string_append_printf(iedatastr, "%02x",
						(unsigned) *payload++);
			}
			ies++;
		}
		gchar* iedatacmd = g_string_free(iedatastr, FALSE);
		gsize replylen;
		network_wpasupplicant_docommand(supplicant->wpa_ctrl, &replylen, TRUE,
				iedatacmd);
		g_free(iedatacmd);
	}
}

void network_wpasupplicant_scan(NetworkWpaSupplicant* supplicant) {
	size_t replylen;
	char* reply = network_wpasupplicant_docommand(supplicant->wpa_ctrl,
			&replylen,
			TRUE, "SCAN");
	if (reply != NULL) {
		g_free(reply);
	}
}

int network_wpasupplicant_addnetwork(NetworkWpaSupplicant* supplicant,
		const gchar* ssid, const gchar* psk, unsigned mode) {
	g_message("adding network %s with psk %s", ssid, psk);
	gsize respsz;
	gchar* resp = network_wpasupplicant_docommand(supplicant->wpa_ctrl, &respsz,
	TRUE, "ADD_NETWORK");
	guint64 networkid;
	if (resp != NULL) {
		if (!g_ascii_string_to_unsigned(resp, 10, 0, G_MAXUINT8, &networkid,
		NULL)) {
			g_message("failed to parse network id, command failed?");
			return -1;
		}
		g_free(resp);
	}

	resp = network_wpasupplicant_docommandf(supplicant->wpa_ctrl, &respsz, TRUE,
			"SET_NETWORK %u ssid \"%s\"", (unsigned) networkid, ssid);
	if (resp != NULL) {
		g_assert(ISOK(resp));
		g_free(resp);
	}

	resp = network_wpasupplicant_docommandf(supplicant->wpa_ctrl, &respsz, TRUE,
			"SET_NETWORK %u psk \"%s\"", (unsigned) networkid, psk);
	if (resp != NULL) {
		g_assert(ISOK(resp));
		g_free(resp);
	}

	resp = network_wpasupplicant_docommandf(supplicant->wpa_ctrl, &respsz, TRUE,
			"SET_NETWORK %u mode %d", (unsigned) networkid, mode);
	if (resp != NULL) {
		g_assert(ISOK(resp));
		g_free(resp);
	}
	return networkid;
}

void network_wpasupplicant_selectnetwork(NetworkWpaSupplicant* supplicant,
		int which) {
	gsize respsz;
	gchar* resp = network_wpasupplicant_docommandf(supplicant->wpa_ctrl,
			&respsz, TRUE, "SELECT_NETWORK %d", which);
	if (resp != NULL)
		g_free(resp);
}

NetworkWpaSupplicant* network_wpasupplicant_new(const char* interface) {
	NetworkWpaSupplicant* supplicant = g_object_new(NETWORK_TYPE_WPASUPPLICANT,
	NULL);

	gboolean ret = FALSE;
	g_message("starting wpa_supplicant for %s", interface);
	gchar* args[] = { WPASUPPLICANT_BINARYPATH, "-Dnl80211", "-i", interface,
			"-C", wpasupplicantsocketdir, "-qq", NULL };
	if (!g_spawn_async(NULL, args, NULL,
			G_SPAWN_STDOUT_TO_DEV_NULL | G_SPAWN_STDERR_TO_DEV_NULL, NULL,
			NULL, &supplicant->pid, NULL)) {
		g_message("failed to start wpa_supplicant");
		goto err_spawn;
	} else
		g_message("wpa_supplicant for %s started, pid %d", interface,
				supplicant->pid);

	g_usleep(2 * 1000000);

	GString* socketpathstr = g_string_new(NULL);
	g_string_printf(socketpathstr, "%s/%s", wpasupplicantsocketdir, interface);
	gchar* socketpath = g_string_free(socketpathstr, FALSE);

	supplicant->wpa_ctrl = wpa_ctrl_open(socketpath);
	if (supplicant->wpa_ctrl)
		g_message("wpa_supplicant control socket connected");
	else {
		g_message("failed to open wpa_supplicant control socket");
		goto err_openctrlsck;
	}

	supplicant->wpa_event = wpa_ctrl_open(socketpath);
	if (supplicant->wpa_event) {
		g_message("wpa_supplicant event socket connected");
		wpa_ctrl_attach(supplicant->wpa_event);
		int fd = wpa_ctrl_get_fd(supplicant->wpa_event);
		utils_addwatchforsocketfd(fd, G_IO_IN, network_wpasupplicant_onevent,
				supplicant);
	} else {
		g_message("failed to open wpa_supplicant event socket");
		goto err_openevntsck;
	}

	return supplicant;

	err_openevntsck:	//
	wpa_ctrl_close(supplicant->wpa_ctrl);
	out: //
	err_openctrlsck:	//
	g_free(socketpath);
	err_spawn:			//
	return NULL;
}

GPtrArray* network_wpasupplicant_getlastscanresults() {
	return scanresults;
}

void network_wpasupplicant_dumpstate(NetworkWpaSupplicant* supplicant,
		JsonBuilder* builder) {
	JSONBUILDER_START_OBJECT(builder, "supplicant");
	JSONBUILDER_ADD_BOOL(builder, "connected", supplicant->connected);
	if (supplicant->lasterror)
		JSONBUILDER_ADD_STRING(builder, "lasterror", supplicant->lasterror);
}

void network_wpasupplicant_ctrl_fill(NetworkWpaSupplicant* supplicant,
		struct tbus_fieldandbuff* field) {
	int supplicantstate = THINGYMCCONFIG_OK;
	if (supplicant->connected)
		supplicantstate = THINGYMCCONFIG_ACTIVE;
	struct tbus_fieldandbuff f = TBUS_STATEFIELD(
			THINGYMCCONFIG_FIELDTYPE_NETWORKSTATEUPDATE_SUPPLICANTSTATE,
			supplicantstate, 0);
	memcpy(field, &f, sizeof(*field));
}

void network_wpasupplicant_stop(NetworkWpaSupplicant* supplicant) {
	wpa_ctrl_close(supplicant->wpa_ctrl);
	wpa_ctrl_close(supplicant->wpa_event);
}
