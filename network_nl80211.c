#include <linux/if.h>
#include <linux/nl80211.h>
#include <netlink/socket.h>
#include <netlink/genl/genl.h>
#include <netlink/genl/ctrl.h>
#include "network_nl80211.h"
#include "network_priv.h"

#define IFNAMESUFFIX_STA	"sta"
#define IFNAMESUFFIX_AP		"ap"

static struct nl_sock* nl80211sock;
static int nl80211id;

static void network_nl80211_sendmsgandfree(struct nl_msg* msg) {
	int ret;
	if ((ret = nl_send_sync(nl80211sock, msg)) < 0)
		g_message("failed to send nl message -> %d", ret);
}

static int network_netlink_interface_callback(struct nl_msg *msg, void *arg) {
#ifdef NLDEBUG
	nl_msg_dump(msg, stdout);
#endif

	struct genlmsghdr *genlhdr = nlmsg_data(nlmsg_hdr(msg));
	int attrlen = genlmsg_attrlen(genlhdr, 0);
	struct nlattr* attrs = genlmsg_attrdata(genlhdr, 0);

	struct network_interface* interface = g_malloc0(sizeof(*interface));
	struct nlattr *nla;
	int rem;
	nla_for_each_attr(nla, attrs, attrlen, rem)
	{
		switch (nla_type(nla)) {
		case NL80211_ATTR_IFNAME:
			interface->ifname = g_strdup(nla_get_string(nla));
			break;
		case NL80211_ATTR_WIPHY:
			interface->wiphy = nla_get_u32(nla);
			break;
		case NL80211_ATTR_IFTYPE: {
			uint32_t type = nla_get_u32(nla);
			interface->ap = type == NL80211_IFTYPE_AP;
		}
			break;
		case NL80211_ATTR_IFINDEX:
			interface->ifidx = nla_get_u32(nla);
			break;
		}
	}
	GHashTable* interfaces = arg;
	g_hash_table_insert(interfaces, interface->ifname, interface);

	return NL_OK;
}

static gchar* network_nl80211_generatevifname(const gchar* masterifname,
		const gchar* suffix) {
	GString* interfacenamestr = g_string_new(NULL);

	g_string_append_len(interfacenamestr, masterifname,
			MIN(strlen(masterifname), IFNAMSIZ - (1 + strlen(suffix) + 1)));
	g_string_append_c_inline(interfacenamestr, '_');
	g_string_append(interfacenamestr, suffix);

	gchar* interfacename = g_string_free(interfacenamestr, FALSE);
	return interfacename;
}

static void network_netlink_createvif(GHashTable* interfaces,
		struct network_interface* masterinterface, const gchar* suffix,
		guint32 type) {

	struct nl_msg* msg = nlmsg_alloc();
	if (msg == NULL) {
		g_message(NETWORK_ERRMSG_FAILEDTOALLOCNLMSG);
		goto err_allocmsg;
	}

	gchar* interfacename = network_nl80211_generatevifname(
			masterinterface->ifname, suffix);

	g_message("creating a VIF called %s on %d", interfacename,
			masterinterface->wiphy);

	nl_socket_modify_cb(nl80211sock, NL_CB_VALID, NL_CB_CUSTOM,
			network_netlink_interface_callback, interfaces);
	genlmsg_put(msg, NL_AUTO_PORT, NL_AUTO_SEQ, nl80211id, 0, 0,
			NL80211_CMD_NEW_INTERFACE, 0);
	nla_put_string(msg, NL80211_ATTR_IFNAME, interfacename);
	nla_put_u32(msg, NL80211_ATTR_IFTYPE, type);
	nla_put_u32(msg, NL80211_ATTR_WIPHY, masterinterface->wiphy);

	network_nl80211_sendmsgandfree(msg);
	nl_recvmsgs_default(nl80211sock);

	g_free(interfacename);

	return;

	err_allocmsg: return;
}

void network_nl80211_createapvif(GHashTable* interfaces,
		struct network_interface* masterinterface) {
	network_netlink_createvif(interfaces, masterinterface, IFNAMESUFFIX_AP,
			NL80211_IFTYPE_AP);
}

gboolean network_nl80211_findphy(GHashTable* interfaces,
		const gchar* interfacename, guint32* wiphy) {
	if (g_hash_table_contains(interfaces, interfacename)) {
		struct network_interface* interface = g_hash_table_lookup(interfaces,
				interfacename);
		*wiphy = interface->wiphy;
		return TRUE;
	}
	return FALSE;
}

static void network_interface_free(gpointer data) {
	struct network_interface* interface = data;
	g_free(interface);
}

GHashTable* network_nl80211_listwifiinterfaces() {
	struct nl_msg* msg = nlmsg_alloc();
	if (msg == NULL) {
		g_message(NETWORK_ERRMSG_FAILEDTOALLOCNLMSG);
		goto err_allocmsg;
	}

	GHashTable* interfaces = g_hash_table_new_full(g_str_hash, g_str_equal,
	NULL, network_interface_free);

	nl_socket_modify_cb(nl80211sock, NL_CB_VALID, NL_CB_CUSTOM,
			network_netlink_interface_callback, interfaces);
	genlmsg_put(msg, NL_AUTO_PORT, NL_AUTO_SEQ, nl80211id, 0, NLM_F_DUMP,
			NL80211_CMD_GET_INTERFACE, 0);

	network_nl80211_sendmsgandfree(msg);

	return interfaces;

	err_allocmsg: //
	return NULL;
}

gboolean network_nl80211_findapvif(gpointer key, gpointer value,
		gpointer user_data) {
	gchar* masterinterfacename = user_data;
	struct network_interface* interface = value;
#ifdef DEBUG
	g_message("interface %s, is ap %d, %s", interface->ifname,
			(int )interface->ap, masterinterfacename);
#endif
	//return strcmp(masterinterfacename, interface->ifname) != 0 && interface->ap;

	gchar* apifname = network_nl80211_generatevifname(masterinterfacename,
	IFNAMESUFFIX_AP);
	gboolean ret = strcmp(interface->ifname, apifname) == 0;
	g_free(apifname);
	return ret;
}

gboolean network_nl80211_init() {
	nl80211sock = nl_socket_alloc();
	if (nl80211sock == NULL) {
		g_message("failed to create nl80211 socket");
		goto err_sock;
	}
	if (genl_connect(nl80211sock)) {
		g_message("failed to connect nl80211 socket");
		goto err_connect;
	}

	nl80211id = genl_ctrl_resolve(nl80211sock, "nl80211");
	if (nl80211id == 0) {
		g_message("failed to resolve nl80211");
		goto err_resolve;
	}

	return TRUE;

	err_resolve: //
	err_connect: //
	nl_socket_free(nl80211sock);
	err_sock: //
	return FALSE;
}
