#include <glib.h>
#include <gio/gunixsocketaddress.h>

#include "include/thingymcconfig/client_glib.h"
#include "include/thingymcconfig/ctrl.h"
#include "tbus.h"

struct networkstate {
	unsigned supplicantstate;
	unsigned dhcp4state;
};

struct _ThingyMcConfigClient {
	GObject parent_instance;
	GSocketConnection* socketconnection;
	struct networkstate networkstate;
	const gchar* appname;
	int appindex;
};

G_DEFINE_TYPE(ThingyMcConfigClient, thingymcconfig_client, G_TYPE_OBJECT)

static guint signal_daemon;
static guint signal_networkstate;
static GQuark detail_daemon_connecting;
static GQuark detail_daemon_connected;
static GQuark detail_daemon_disconnected;
static GQuark detail_daemon_connectfailed;
static GQuark detail_networkstate_supplicant_connected;
static GQuark detail_networkstate_supplicant_disconnected;

static void thingymcconfig_client_fieldproc_appconfig(
		struct tbus_fieldandbuff* field, gpointer target, gpointer user_data) {
	g_message("processing app config field");
	unsigned* index = target;
	ThingyMcConfigClient* client = user_data;
	if (strcmp(field->buff, client->appname) == 0) {
		g_message("found our app index");
		*index = field->field.index.index;
	}
}

static void thingymcconfig_client_emitter_appconfig(gpointer target,
		gpointer user_data) {
	unsigned* index = target;
	if (*index != 0) {
		ThingyMcConfigClient* client = user_data;
		client->appindex = *index;
		g_signal_emit(client, signal_daemon, detail_daemon_connected);
	}
}

static void thingymcconfig_client_fieldproc_networkstate(
		struct tbus_fieldandbuff* field, gpointer target, gpointer user_data) {
	g_message("processing network state field");
	struct networkstate* newnetworkstate = target;
	switch (field->field.raw.type) {
	case THINGYMCCONFIG_FIELDTYPE_NETWORKSTATEUPDATE_SUPPLICANTSTATE:
		newnetworkstate->supplicantstate = field->field.stateanderror.state;
		break;
	}
}

static void thingymcconfig_client_emitter_networkstate(gpointer target,
		gpointer user_data) {
	ThingyMcConfigClient* client = user_data;
	g_message("emitting network state");
	struct networkstate* newnetworkstate = target;
	if (memcmp(&client->networkstate, newnetworkstate, sizeof(*newnetworkstate))
			== 0) {
		g_message("state unchanged");
		return;
	}

	if (newnetworkstate->supplicantstate
			!= client->networkstate.supplicantstate) {
		switch (newnetworkstate->supplicantstate) {
		case THINGYMCCONFIG_ACTIVE:
			g_signal_emit(client, signal_networkstate,
					detail_networkstate_supplicant_connected);
			break;
		default:
			g_signal_emit(client, signal_networkstate,
					detail_networkstate_supplicant_disconnected);
			break;
		}
	}

	memcpy(&client->networkstate, newnetworkstate,
			sizeof(client->networkstate));
}

static struct tbus_messageprocessor msgproc[] = {
		[THINGYMCCONFIG_MSGTYPE_CONFIG_APPS ] = { .allocsize = sizeof(unsigned),
				.fieldprocessor = thingymcconfig_client_fieldproc_appconfig,
				.emitter = thingymcconfig_client_emitter_appconfig },
		[THINGYMCCONFIG_MSGTYPE_EVENT_NETWORKSTATEUPDATE ] = { .allocsize =
				sizeof(struct networkstate), .fieldprocessor =
				thingymcconfig_client_fieldproc_networkstate, .emitter =
				thingymcconfig_client_emitter_networkstate } };

static gboolean thingymcconfig_client_socketcallback(GIOChannel *source,
		GIOCondition cond, gpointer data) {
	ThingyMcConfigClient* client = data;
	g_message("message on ctrl socket");
	GInputStream* is = g_io_stream_get_input_stream(
			G_IO_STREAM(client->socketconnection));
	gboolean ret = tbus_readmsg(is, msgproc, G_N_ELEMENTS(msgproc), client);
	if (!ret) {
		g_message("disconnected");
		g_object_unref(client->socketconnection);
		client->socketconnection = NULL;
		g_signal_emit(client, signal_daemon, detail_daemon_disconnected);
	}
	return ret;
}

static void thingymcconfig_client_class_init(ThingyMcConfigClientClass *klass) {
	signal_daemon = g_signal_newv(THINGYMCCONFIG_CLIENT_SIGNAL_DAEMON,
	THINGYMCCONFIG_TYPE_CLIENT,
			G_SIGNAL_RUN_LAST | G_SIGNAL_NO_HOOKS | G_SIGNAL_NO_RECURSE
					| G_SIGNAL_DETAILED, NULL,
			NULL, NULL, NULL, G_TYPE_NONE, 0, NULL);
	signal_networkstate = g_signal_newv(
	THINGYMCCONFIG_CLIENT_SIGNAL_NETWORKSTATE,
	THINGYMCCONFIG_TYPE_CLIENT,
			G_SIGNAL_RUN_LAST | G_SIGNAL_NO_HOOKS | G_SIGNAL_NO_RECURSE
					| G_SIGNAL_DETAILED, NULL,
			NULL, NULL, NULL, G_TYPE_NONE, 0, NULL);

	detail_daemon_connecting = g_quark_from_string(
	THINGYMCCONFIG_CLIENT_DETAIL_DAEMON_CONNECTING);
	detail_daemon_connected = g_quark_from_string(
	THINGYMCCONFIG_CLIENT_DETAIL_DAEMON_CONNECTED);
	detail_daemon_disconnected = g_quark_from_string(
	THINGYMCCONFIG_CLIENT_DETAIL_DAEMON_DISCONNECTED);
	detail_daemon_connectfailed = g_quark_from_string(
	THINGYMCCONFIG_CLIENT_DETAIL_DAEMON_CONNECTFAILED);

	detail_networkstate_supplicant_connected = g_quark_from_string(
	THINGYMCCONFIG_CLIENT_DETAIL_NETWORKSTATE_SUPPLICANTCONNECTED);
	detail_networkstate_supplicant_disconnected = g_quark_from_string(
	THINGYMCCONFIG_CLIENT_DETAIL_NETWORKSTATE_SUPPLICANTDISCONNECTED);
}

static void thingymcconfig_client_init(ThingyMcConfigClient *self) {
	self->appindex = -1;
}

ThingyMcConfigClient* thingymcconfig_client_new(const gchar* appname) {
	ThingyMcConfigClient* client = g_object_new(THINGYMCCONFIG_TYPE_CLIENT,
	NULL);
	client->appname = appname;
	return client;
}

void thingymcconfig_client_connect(ThingyMcConfigClient *client) {
	g_assert(client->socketconnection == NULL);

	GSocketConnection* socketconnection = NULL;

	g_signal_emit(client, signal_daemon, detail_daemon_connecting);

	GSocketAddress* socketaddress = g_unix_socket_address_new(
	THINGYMCCONFIG_CTRLSOCKPATH);

	GSocket* sock = g_socket_new(G_SOCKET_FAMILY_UNIX, G_SOCKET_TYPE_STREAM,
			G_SOCKET_PROTOCOL_DEFAULT, NULL);
	if (sock == NULL) {
		g_message("failed to create socket");
		goto err_sock;
	}

	GError* err = NULL;

	socketconnection = g_socket_connection_factory_create_connection(sock);
	if (!g_socket_connection_connect(socketconnection, socketaddress, NULL,
			&err)) {
		g_message("failed to connect control socket; %s", err->message);
		goto err_connect;
	}

	GIOChannel* channel = g_io_channel_unix_new(g_socket_get_fd(sock));
	guint source = g_io_add_watch(channel, G_IO_IN,
			thingymcconfig_client_socketcallback, client);
	g_io_channel_unref(channel);

	client->socketconnection = socketconnection;
	g_message("ctrl socket connected");

	err_sock: //
	err_connect: //

	if (!client->socketconnection)
		g_signal_emit(client, signal_daemon, detail_daemon_connectfailed);
}

void thingymcconfig_client_lazyconnect(ThingyMcConfigClient* client) {
	thingymcconfig_client_connect(client);
}

void thingymcconfig_client_sendconnectivitystate(ThingyMcConfigClient* client,
		gboolean connected) {
	g_assert(client->socketconnection);
	g_assert(client->appindex != -1);

	struct tbus_fieldandbuff fields[] =
			{
							TBUS_INDEXFIELD(THINGYMCCONFIG_FIELDTYPE_APPSTATEUPDATE_APPINDEX, client->appindex),
							TBUS_STATEFIELD(THINGYMCCONFIG_FIELDTYPE_APPSTATEUPDATE_CONNECTIVITY, connected ? THINGYMCCONFIG_OK : THINGYMCCONFIG_ERR, 0) };
	GOutputStream* os = g_io_stream_get_output_stream(
			G_IO_STREAM(client->socketconnection));
	tbus_writemsg(os, THINGYMCCONFIG_MSGTYPE_EVENT_APPSTATEUPDATE, fields,
			G_N_ELEMENTS(fields));
}

void thingymcconfig_client_sendappstate(ThingyMcConfigClient* client) {
	g_assert(client->socketconnection);
	g_assert(client->appindex != -1);

	struct tbus_fieldandbuff fields[] =
			{
							TBUS_INDEXFIELD(THINGYMCCONFIG_FIELDTYPE_APPSTATEUPDATE_APPINDEX, client->appindex),
							TBUS_STATEFIELD(THINGYMCCONFIG_FIELDTYPE_APPSTATEUPDATE_APPSTATE,THINGYMCCONFIG_OK, 0 ) };

	GOutputStream* os = g_io_stream_get_output_stream(
			G_IO_STREAM(client->socketconnection));
	tbus_writemsg(os, THINGYMCCONFIG_MSGTYPE_EVENT_APPSTATEUPDATE, fields,
			G_N_ELEMENTS(fields));
}

void thingymcconfig_client_free(ThingyMcConfigClient *client) {

}
