#include <netlink/route/route.h>
#include <netlink/route/link.h>
#include <netlink/route/addr.h>
#include "network_rtnetlink.h"

static struct nl_sock* routesock;

/*
 static void network_netlink_setlinkstate(struct network_interface* interface,
 gboolean up) {
 g_message("setting link state on %s", interface->ifname);
 struct nl_cache* cache;
 // how do you free this?
 int ret;
 if ((ret = rtnl_link_alloc_cache(routesock, AF_UNSPEC, &cache)) < 0) {
 g_message("failed to populate link cache -> %d", ret);
 goto out;
 }
 struct rtnl_link* link = rtnl_link_get(cache, interface->ifidx);
 if (link == NULL) {
 g_message("didn't find link");
 goto out;
 }

 struct rtnl_link* change = rtnl_link_alloc();
 rtnl_link_unset_flags(change, IFF_UP);

 rtnl_link_change(routesock, link, change, 0);

 rtnl_link_put(link);
 rtnl_link_put(change);
 out: return;
 }*/

gboolean network_rtnetlink_init() {
	routesock = nl_socket_alloc();
	nl_connect(routesock, NETLINK_ROUTE);
	return TRUE;
}

void network_rtnetlink_setipv4addr(int ifidx, const char* addrstr) {
	struct nl_addr* localaddr;
	nl_addr_parse(addrstr, AF_INET, &localaddr);

	struct rtnl_addr* addr = rtnl_addr_alloc();

	rtnl_addr_set_ifindex(addr, ifidx);
	rtnl_addr_set_local(addr, localaddr);

	if (rtnl_addr_add(routesock, addr, 0) != 0)
		g_message("failed to set v4 addr");

	rtnl_addr_put(addr);
	nl_addr_put(localaddr);
}
