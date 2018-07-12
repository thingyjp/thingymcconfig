#include <glib.h>
#include <gio/gunixsocketaddress.h>
#include "ctrl.h"

#define CTRLSOCKPATH "/tmp/thingymcconfig.socket"

static GSocketService* socketservice;

static gboolean ctrl_incomingcallback(GSocketService *service,
		GSocketConnection *connection, GObject *source_object,
		gpointer user_data) {
	g_message("incoming control socket connection");
	return FALSE;
}

void ctrl_init() {

}

void ctrl_start() {
	GSocketAddress* socketaddress = g_unix_socket_address_new(CTRLSOCKPATH);
	socketservice = g_socket_service_new();

	GError* err = NULL;
	if (!g_socket_listener_add_address((GSocketListener*) socketservice,
			socketaddress, G_SOCKET_TYPE_STREAM, G_SOCKET_PROTOCOL_DEFAULT,
			NULL, NULL, &err)) {
		g_message("failed to add listener; %s", err->message);
	}
	g_signal_connect(socketservice, "incoming",
			G_CALLBACK(ctrl_incomingcallback), NULL);
}

void ctrl_stop() {
	g_socket_service_stop(socketservice);
	GFile* sockfile = g_file_new_for_path(CTRLSOCKPATH);
	g_file_delete(sockfile, NULL, NULL);
	g_object_unref(sockfile);
}

