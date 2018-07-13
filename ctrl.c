#include <glib.h>
#include <gio/gunixsocketaddress.h>
#include "ctrl.h"
#include "include/thingymcconfig/ctrl.h"

#define CTRLSOCKPATH "/tmp/thingymcconfig.socket"

static GSocketService* socketservice;

static void ctrl_send_networkstate(GSocketConnection* connection) {
	GByteArray* pktbuff = g_byte_array_new();

	struct thingymcconfig_ctrl_field fields[] = { { .type =
	THINGYMCCONFIG_FIELDTYPE_NETWORKSTATEUPDATE_SUPPLICANTSTATE }, { .type =
	THINGYMCCONFIG_FIELDTYPE_NETWORKSTATEUPDATE_DHCPSTATE } };

	struct thingymcconfig_ctrl_msgheader msghdr = { .type =
	THINGYMCCONFIG_MSGTYPE_EVENT_NETWORKSTATEUPDATE, .numfields = G_N_ELEMENTS(
			fields) + 1 };

	g_byte_array_append(pktbuff, (void*) &msghdr, sizeof(msghdr));
	g_byte_array_append(pktbuff, (void*) fields, sizeof(fields));
	g_byte_array_append(pktbuff, (void*) &thingymcconfig_terminator,
			sizeof(thingymcconfig_terminator));

	GOutputStream* os = g_io_stream_get_output_stream(G_IO_STREAM(connection));
	g_output_stream_write(os, pktbuff->data, pktbuff->len, NULL, NULL);
	g_byte_array_free(pktbuff, TRUE);
}

static gboolean ctrl_incomingcallback(GSocketService *service,
		GSocketConnection *connection, GObject *source_object,
		gpointer user_data) {
	g_message("incoming control socket connection");

	ctrl_send_networkstate(connection);

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

