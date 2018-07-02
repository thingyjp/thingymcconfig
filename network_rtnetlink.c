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

void network_rtnetlink_clearipv4addr(int ifidx) {
	struct rtnl_addr* addr = rtnl_addr_alloc();
	rtnl_addr_set_ifindex(addr, ifidx);
	rtnl_addr_set_family(addr, AF_INET);
	rtnl_addr_delete(routesock, addr, 0);
	rtnl_addr_put(addr);
}

void network_rtnetlink_setipv4addr(int ifidx, const char* addrstr) {
	struct nl_addr* localaddr;
	nl_addr_parse(addrstr, AF_INET, &localaddr);

	struct rtnl_addr* addr = rtnl_addr_alloc();

	rtnl_addr_set_ifindex(addr, ifidx);
	rtnl_addr_set_local(addr, localaddr);

	int ret;
	if ((ret = rtnl_addr_add(routesock, addr, 0)) != 0)
		g_message("failed to set v4 addr; %d", ret);

	rtnl_addr_put(addr);
	nl_addr_put(localaddr);
}

void network_rtnetlink_setipv4defaultfw(int ifidx, const guint8* ip) {
	struct rtnl_route* route = rtnl_route_alloc();

	struct nl_addr* dstaddr;
	if (nl_addr_parse("0.0.0.0/0", AF_INET, &dstaddr) != 0) {
		g_message("failed to parse address");
		goto err_dstaddrparse;
	}

	rtnl_route_set_dst(route, dstaddr);
	rtnl_route_set_iif(route, ifidx);

	struct rtnl_nexthop* nh = rtnl_route_nh_alloc();
	struct nl_addr* gwaddr = nl_addr_build(AF_INET, ip, 4);
	rtnl_route_nh_set_gateway(nh, gwaddr);
	rtnl_route_add_nexthop(route, nh);
	rtnl_route_set_protocol(route, RTPROT_BOOT);

	int ret;
	if ((ret = rtnl_route_add(routesock, route, 0)) != 0)
		g_message("failed to set v4 default route; %d", ret);

	nl_addr_put(gwaddr);

	err_dstaddrparse: //
	nl_addr_put(dstaddr);
	rtnl_route_put(route);
}
