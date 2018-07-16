#include <glib.h>
#include <gio/gunixsocketaddress.h>
#include "ctrl.h"
#include "apps.h"
#include "include/thingymcconfig/ctrl.h"
#include "utils.h"

static GPtrArray* clientconnections;
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

static void ctrl_disconnectapp(GSocketConnection* connection) {
	g_ptr_array_remove(clientconnections, connection);
	g_object_unref(connection);
}

static gboolean ctrl_readfields(GInputStream* is,
		struct thingymcconfig_ctrl_msgheader* msghdr,
		void (*fieldcallback)(struct thingymcconfig_ctrl_field* field,
				gpointer user_data), gpointer user_data) {

	gboolean ret = FALSE;

	int fields = 0;
	gboolean terminated = FALSE;
	for (int f = 0; f < msghdr->numfields; f++) {
		struct thingymcconfig_ctrl_field field;
		if (g_input_stream_read(is, &field, sizeof(field), NULL, NULL)
				!= sizeof(field))
			goto out;

		fields++;
		if (field.type == THINGYMCCONFIG_FIELDTYPE_TERMINATOR) {
			terminated = TRUE;
			break;
		}

		g_message("have field; type: %d, v0: %d, v1: %d, v2: %d",
				(int ) field.type, (int ) field.v0, (int ) field.v1,
				(int ) field.v2);

		if (fieldcallback != NULL)
			fieldcallback(&field, user_data);
	}

	if (fields != msghdr->numfields || !terminated) {
		g_message("client is out of sync");
		goto out;
	}

	ret = TRUE;

	out: //
	return ret;
}

static void ctrl_appincallback_appstatefieldcallback(
		struct thingymcconfig_ctrl_field* field, gpointer user_data) {
	struct apps_appstateupdate* appstate = user_data;
	switch (field->type) {
	case THINGYMCCONFIG_FIELDTYPE_APPSTATEUPDATE_APPINDEX:
		appstate->appindex =
				((struct thingymcconfig_ctrl_field_index*) field)->index;
		break;
	case THINGYMCCONFIG_FIELDTYPE_APPSTATEUPDATE_APPSTATE: {
		struct thingymcconfig_ctrl_field_stateanderror* f =
				(struct thingymcconfig_ctrl_field_stateanderror*) field;
		appstate->appstate = f->state;
		appstate->apperror = f->error;
	}
		break;
	case THINGYMCCONFIG_FIELDTYPE_APPSTATEUPDATE_CONNECTIVITY: {
		struct thingymcconfig_ctrl_field_stateanderror* f =
				(struct thingymcconfig_ctrl_field_stateanderror*) field;
		appstate->connectivitystate = f->state;
		appstate->connectivityerror = f->error;
	}
		break;
	}
}

static gboolean ctrl_appincallback(GIOChannel *source, GIOCondition condition,
		gpointer data) {
	GSocketConnection* connection = data;

	g_message("incoming data from app");

	GInputStream* is = g_io_stream_get_input_stream(G_IO_STREAM(connection));
	struct thingymcconfig_ctrl_msgheader msghdr;
	if (g_input_stream_read(is, &msghdr, sizeof(msghdr), NULL, NULL)
			!= sizeof(msghdr))
		goto err;

	if (msghdr.type == THINGYMCCONFIG_MSGTYPE_EVENT_APPSTATEUPDATE) {
		struct apps_appstateupdate appstate;
		memset(&appstate, 0, sizeof(appstate));
		if (ctrl_readfields(is, &msghdr,
				ctrl_appincallback_appstatefieldcallback, &appstate)) {
			if (!apps_onappstateupdate(&appstate))
				goto err;
		} else
			goto err;
	} else if (!ctrl_readfields(is, &msghdr, NULL, NULL))
		goto err;

	return TRUE;
	err: //
	ctrl_disconnectapp(connection);
	return FALSE;
}

static gboolean ctrl_incomingcallback(GSocketService *service,
		GSocketConnection *connection, GObject *source_object,
		gpointer user_data) {
	g_message("incoming control socket connection");

	g_object_ref(connection);
	g_ptr_array_add(clientconnections, connection);

	utils_addwatchforsocket(g_socket_connection_get_socket(connection), G_IO_IN,
			ctrl_appincallback, connection);

	ctrl_send_networkstate(connection);
	return TRUE;
}

void ctrl_init() {
	clientconnections = g_ptr_array_new();
}

void ctrl_start() {
	GSocketAddress* socketaddress = g_unix_socket_address_new(
	THINGYMCCONFIG_CTRLSOCKPATH);
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
	GFile* sockfile = g_file_new_for_path(THINGYMCCONFIG_CTRLSOCKPATH);
	g_file_delete(sockfile, NULL, NULL);
	g_object_unref(sockfile);
}

