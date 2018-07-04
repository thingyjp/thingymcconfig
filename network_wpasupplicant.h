#pragma once

#include <json-glib/json-glib.h>
#include <wpa_ctrl.h>
#include "network_model.h"

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

void network_wpasupplicant_init(void);
gboolean network_wpasupplicant_start(struct wpa_ctrl** wpa_ctrl,
		struct wpa_ctrl** wpa_event, const char* interface, GPid* pid);
void network_wpasupplicant_seties(struct wpa_ctrl* wpa_ctrl,
		const struct network_wpasupplicant_ie* ies, unsigned numies);
void network_wpasupplicant_scan(struct wpa_ctrl* wpa_ctrl);
int network_wpasupplicant_addnetwork(struct wpa_ctrl* wpa_ctrl,
		const gchar* ssid, const gchar* psk, unsigned mode);
void network_wpasupplicant_selectnetwork(struct wpa_ctrl* wpa_ctrl, int which);
GPtrArray* network_wpasupplicant_getlastscanresults(void);
void network_wpasupplicant_stop(struct wpa_ctrl* wpa_ctrl, GPid* pid);

void network_wpasupplicant_dumpstatus(JsonBuilder* builder);
