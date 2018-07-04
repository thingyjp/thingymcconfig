#pragma once

#include <glib.h>

int packetsocket_createsocket_udp(int ifindex, const guint8* mac);
void packetsocket_send_udp(int rawsock, int ifindex, guint32 srcaddr,
		guint16 srcprt, guint16 dstprt, guint8* payload, gsize payloadlen);
gssize packetsocket_recv_udp(int fd, int srcport, int destport, guint8* buff,
		gsize buffsz);
