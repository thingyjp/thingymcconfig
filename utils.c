#include "utils.h"

guint utils_addwatchforsocket(GSocket* sock, GIOCondition cond,
		GIOFunc callback, gpointer user_data) {
	return utils_addwatchforsocketfd(g_socket_get_fd(sock), cond, callback,
			user_data);
}

guint utils_addwatchforsocketfd(int fd, GIOCondition cond, GIOFunc callback,
		gpointer user_data) {
	GIOChannel* channel = g_io_channel_unix_new(fd);
	guint source = g_io_add_watch(channel, cond, callback, user_data);
	g_io_channel_unref(channel);
	return source;
}

void thingymcconfig_utils_hexdump(guint8* payload, gsize len) {
	for (gsize i = 0; i < len; i += 8) {
		GString* hexstr = g_string_new(NULL);
		GString* asciistr = g_string_new(NULL);
		for (int j = 0; j < 8; j++) {
			if (i + j < len) {
				g_string_append_printf(hexstr, "%02x ", (unsigned) *payload);
				g_string_append_printf(asciistr, "%c",
				g_ascii_isgraph(*payload) ? *payload : ' ');
			} else {
				g_string_append_printf(hexstr, "   ");
				g_string_append_printf(asciistr, " ");
			}
			payload++;
		}
		gchar* hs = g_string_free(hexstr, FALSE);
		gchar* as = g_string_free(asciistr, FALSE);
		g_message("%08x %s[%s]", (unsigned ) i, hs, as);
		g_free(hs);
		g_free(as);
	}
}
