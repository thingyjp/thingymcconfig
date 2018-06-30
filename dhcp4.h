#pragma once

static const guint8 DHCP4_MAGIC[] = { 0x63, 0x82, 0x53, 0x63 };

#define DHCP4_OPT_SUBNETMASK			1
#define DHCP4_OPT_DEFAULTGATEWAY		3
#define DHCP4_OPT_REQUESTEDIP			50
#define DHCP4_OPT_LEASETIME				51
#define DHCP4_OPT_DHCPMESSAGETYPE		53
#define DHCP4_OPT_SERVERID				54

#define DHCP4_DHCPMESSAGETYPE_DISCOVER	1
#define DHCP4_DHCPMESSAGETYPE_OFFER		2
#define DHCP4_DHCPMESSAGETYPE_REQUEST	3
#define DHCP4_DHCPMESSAGETYPE_ACK		5
#define DCHP4_DHCPMESSAGETYPE_NAK		6

#define DHCP4_PORT_SERVER	67
#define DHCP4_PORT_CLIENT	68

#define DHCP4_ADDRESS_LEN	4
