#pragma once

#define NETWORK_ERRMSG_FAILEDTOALLOCNLMSG "failed to allocate netlink message"

#define FLAG_ESS "ESS"
#define FLAG_WPS "WPS"
#define FLAG_WEP "WEP"
#define FLAG_WPA_PSK_CCMP "WPA-PSK-CCMP"
#define FLAG_WPA_PSK_CCMP_TKIP "WPA-PSK-CCMP+TKIP"
#define FLAG_WPA2_PSK_CCMP "WPA2-PSK-CCMP"
#define FLAG_WPA2_PSK_CCMP_TKIP "WPA2-PSK-CCMP+TKIP"

typedef enum {
	NF_ESS = 1, //
	NF_WPS = 1 << 1, //
	NF_WEP = 1 << 2, //
	NF_WPA_PSK_CCMP = 1 << 3, //
	NF_WPA_PSK_CCMP_TKIP = 1 << 4, //
	NF_WPA2_PSK_CCMP = 1 << 5, //
	NF_WPA2_PSK_CCMP_TKIP = 1 << 6
} network_flags;

struct network_interface {
	guint32 wiphy;
	gchar* ifname;
	gboolean ap;
	guint32 ifidx;
};
