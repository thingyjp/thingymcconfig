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
