#ifndef INCLUDE_THINGYMCCONFIG_CLIENT_GLIB_H_
#define INCLUDE_THINGYMCCONFIG_CLIENT_GLIB_H_

#include <glib-object.h>

G_BEGIN_DECLS

#define THINGYMCCONFIG_TYPE_CLIENT thingymcconfig_client_get_type ()
G_DECLARE_FINAL_TYPE(ThingyMcConfigClient, thingymcconfig_client,
		THINGYMCCONFIG, CLIENT, GObject)

G_END_DECLS

#define THINGYMCCONFIG_CLIENT_SIGNAL_DAEMON                              "daemon"
#define THINGYMCCONFIG_CLIENT_DETAIL_DAEMON_CONNECTING                   "connecting"
#define THINGYMCCONFIG_CLIENT_DETAIL_DAEMON_CONNECTED                    "connected"
#define THINGYMCCONFIG_CLIENT_DETAIL_DAEMON_DISCONNECTED                 "disconnected"
#define THINGYMCCONFIG_CLIENT_DETAIL_DAEMON_CONNECTFAILED                "connectfailed"
#define THINGYMCCONFIG_CLIENT_SIGNAL_NETWORKSTATE                        "networkstate"
#define THINGYMCCONFIG_CLIENT_DETAIL_NETWORKSTATE_SUPPLICANTCONNECTED    "supplicantconnected"
#define THINGYMCCONFIG_CLIENT_DETAIL_NETWORKSTATE_SUPPLICANTDISCONNECTED "supplicantdisconnected"

ThingyMcConfigClient* thingymcconfig_client_new(const gchar* appname);
void thingymcconfig_client_connect(ThingyMcConfigClient *client);
void thingymcconfig_client_lazyconnect(ThingyMcConfigClient* client);
void thingymcconfig_client_sendconnectivitystate(ThingyMcConfigClient* client,
		gboolean connected);
void thingymcconfig_client_sendappstate(ThingyMcConfigClient* client);
void thingymcconfig_client_free(ThingyMcConfigClient* client);

#endif /* INCLUDE_THINGYMCCONFIG_CLIENT_GLIB_H_ */
