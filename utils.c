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

gchar* utils_jsonbuildertostring(JsonBuilder* jsonbuilder, gsize* jsonlen) {
	JsonNode* responseroot = json_builder_get_root(jsonbuilder);
	JsonGenerator* generator = json_generator_new();
	json_generator_set_root(generator, responseroot);

	gchar* json = json_generator_to_data(generator, jsonlen);
	json_node_free(responseroot);
	g_object_unref(generator);
	g_object_unref(jsonbuilder);
	return json;
}
