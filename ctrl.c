#include <glib.h>
#include <gio/gunixsocketaddress.h>
#include "ctrl.h"
#include "apps.h"
#include "network.h"
#include "utils.h"
#include "tbus.h"

static GPtrArray* clientconnections;
static GHashTable* clientappmapping;
static GSocketService* socketservice;

static void ctrl_disconnectapp(GSocketConnection* connection) {
	gpointer existingmapping = g_hash_table_lookup(clientappmapping,
			connection);
	if (existingmapping != NULL) {
		guint index = GPOINTER_TO_UINT(existingmapping);
		g_hash_table_remove(clientappmapping, connection);
		apps_onappdisconnected(index);
	}

	g_ptr_array_remove(clientconnections, connection);
	g_object_unref(connection);
}

static void ctrl_fieldproc_appstate(struct tbus_fieldandbuff* field,
		gpointer target, gpointer user_data) {
	g_message("processing app state field");
	struct apps_appstateupdate* appstate = target;
	switch (field->field.raw.type) {
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

static void ctrl_emitter_appstate(gpointer target, gpointer user_data) {
	struct apps_appstateupdate* appstate = target;
	GSocketConnection* connection = user_data;
	gpointer existingmapping = g_hash_table_lookup(clientappmapping,
			connection);
	if (existingmapping != NULL) {
		guint index = GPOINTER_TO_UINT(existingmapping);
		if (appstate->appindex != index) {
			g_message("app has mysteriously changed it's index");
		}
	} else {
		gpointer index = GUINT_TO_POINTER((guint ) appstate->appstate);
		g_hash_table_insert(clientappmapping, connection, index);
	}
	apps_onappstateupdate(appstate);
}

static struct tbus_messageprocessor msgproc[] = {
		[THINGYMCCONFIG_MSGTYPE_EVENT_APPSTATEUPDATE] = { .allocsize =
				sizeof(struct apps_appstateupdate), .fieldprocessor =
				ctrl_fieldproc_appstate, .emitter = ctrl_emitter_appstate } };

static gboolean ctrl_appincallback(GIOChannel *source, GIOCondition condition,
		gpointer data) {
	GSocketConnection* connection = data;
	g_message("incoming data from app");
	GInputStream* is = g_io_stream_get_input_stream(G_IO_STREAM(connection));
	gboolean ret = tbus_readmsg(is, msgproc, G_N_ELEMENTS(msgproc), connection);
	if (!ret)
		ctrl_disconnectapp(connection);
	return ret;
}

static gboolean ctrl_incomingcallback(GSocketService *service,
		GSocketConnection *connection, GObject *source_object,
		gpointer user_data) {
	g_message("incoming control socket connection");

	GOutputStream* os = g_io_stream_get_output_stream(G_IO_STREAM(connection));
	if (apps_ctrl_sendconfig(os) && network_ctrl_sendstate(os)) {
		g_object_ref(connection);
		g_ptr_array_add(clientconnections, connection);
		utils_addwatchforsocket(g_socket_connection_get_socket(connection),
				G_IO_IN, ctrl_appincallback, connection);
	}

	return TRUE;
}

void ctrl_init() {
	clientconnections = g_ptr_array_new();
	clientappmapping = g_hash_table_new(g_direct_hash, g_direct_equal);
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

static void ctrl_notifyclientofnetworkstatechange(gpointer data,
		gpointer user_data) {
	GSocketConnection* clientconnection = data;
	network_ctrl_sendstate(
			g_io_stream_get_output_stream(G_IO_STREAM(clientconnection)));
}

void ctrl_onnetworkstatechange() {
	g_ptr_array_foreach(clientconnections,
			ctrl_notifyclientofnetworkstatechange, NULL);
}

void ctrl_stop() {
	g_socket_service_stop(socketservice);
	GFile* sockfile = g_file_new_for_path(THINGYMCCONFIG_CTRLSOCKPATH);
	g_file_delete(sockfile, NULL, NULL);
	g_object_unref(sockfile);
}

