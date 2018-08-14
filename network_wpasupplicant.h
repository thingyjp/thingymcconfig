#pragma once

#include <json-glib/json-glib.h>
#include <wpa_ctrl.h>
#include "network_model.h"
#include "tbus.h"

G_BEGIN_DECLS

#define NETWORK_TYPE_WPASUPPLICANT network_wpasupplicant_get_type ()
G_DECLARE_FINAL_TYPE(NetworkWpaSupplicant, network_wpasupplicant, NETWORK,
		WPASUPPLICANT, GObject)

G_END_DECLS

#define WPASUPPLICANT_NETWORKMODE_STA	0
#define WPASUPPLICANT_NETWORKMODE_AP	2

#define FLAG_ESS "ESS"
#define FLAG_WPS "WPS"
#define FLAG_WEP "WEP"
#define FLAG_WPA_PSK_CCMP "WPA-PSK-CCMP"
#define FLAG_WPA_PSK_CCMP_TKIP "WPA-PSK-CCMP+TKIP"
#define FLAG_WPA2_PSK_CCMP "WPA2-PSK-CCMP"
#define FLAG_WPA2_PSK_CCMP_TKIP "WPA2-PSK-CCMP+TKIP"

struct network_wpasupplicant_ie {
	guint8 id;
	const guint8* payload;
	guint8 payloadlen;
};

#define NETWORK_WPASUPPLICANT_SIGNAL              "wpasupplicant"
#define NETWORK_WPASUPPLICANT_DETAIL_CONNECTED    "connected"
#define NETWORK_WPASUPPLICANT_DETAIL_DISCONNECTED "disconnected"

NetworkWpaSupplicant* network_wpasupplicant_new(const char* interface);
void network_wpasupplicant_seties(NetworkWpaSupplicant* supplicant,
		const struct network_wpasupplicant_ie* ies, unsigned numies);
void network_wpasupplicant_scan(NetworkWpaSupplicant* supplicant);
int network_wpasupplicant_addnetwork(NetworkWpaSupplicant* supplicant,
		const gchar* ssid, const gchar* psk, unsigned mode);
void network_wpasupplicant_selectnetwork(NetworkWpaSupplicant* supplicant,
		int which);
GPtrArray* network_wpasupplicant_getlastscanresults(void);
void network_wpasupplicant_dumpstate(NetworkWpaSupplicant* supplicant,
		JsonBuilder* builder);
void network_wpasupplicant_ctrl_fill(NetworkWpaSupplicant* supplicant,
		struct tbus_fieldandbuff* field);
void network_wpasupplicant_stop(NetworkWpaSupplicant* supplicant);
