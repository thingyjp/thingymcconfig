#include <sys/types.h>
#include <wpa_ctrl.h>
#include <glib.h>
#include <glib-object.h>
#include "network.h"

struct wpa_ctrl* wpa_ctrl = NULL;

static void network_wpasupplicant_getscanresults() {
	static const char* command = "SCAN_RESULTS";
	size_t replylen = 1024;
	char* reply = g_malloc0(replylen + 1);
	if (wpa_ctrl_request(wpa_ctrl, command, strlen(command), reply, &replylen,
	NULL) == 0) {
		g_message("sr: %s", reply);
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
	g_spawn_async(NULL, args, NULL, G_SPAWN_DEFAULT, NULL, NULL, NULL, NULL);

	g_usleep(2 * 1000000);

	GString* socketpathstr = g_string_new(NULL);
	g_string_printf(socketpathstr, "%s/%s", wpapath, interface);
	gchar* socketpath = g_string_free(socketpathstr, FALSE);

	wpa_ctrl = wpa_ctrl_open(socketpath);
	if (wpa_ctrl) {
		wpa_ctrl_attach(wpa_ctrl);
		int fd = wpa_ctrl_get_fd(wpa_ctrl);
		GIOChannel* channel = g_io_channel_unix_new(fd);
		g_io_add_watch(channel, G_IO_IN, network_wpasupplicant_onevent, NULL);
		g_message("wpa_supplicant running, control interface connected");
	} else
		g_message("failed to open wpa_supplicant control interface");

	g_free(socketpath);
}

int network_start() {
	network_wpasupplicant_start();
	return 0;
}

int network_stop() {
	return 0;
}
