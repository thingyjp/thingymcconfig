#pragma once

#include <glib.h>
#include <net/ethernet.h>

struct network_interface {
	guint32 wiphy;
	gchar* ifname;
	gboolean ap;
	guint32 ifidx;
	guint8 mac[ETHER_ADDR_LEN];
};

void network_nl80211_createapvif(GHashTable* interfaces,
		struct network_interface* masterinterface);
gboolean network_nl80211_findphy(GHashTable* interfaces,
		const gchar* interfacename, guint32* wiphy);
GHashTable* network_nl80211_listwifiinterfaces(void);
gboolean network_nl80211_init(void);

gboolean network_nl80211_findapvif(gpointer key, gpointer value,
		gpointer user_data);
