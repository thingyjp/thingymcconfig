#include <sys/socket.h>
#include <linux/if_packet.h>
#include <arpa/inet.h>
#include <net/ethernet.h>
#include <netinet/ip.h>
#include <netinet/udp.h>
#include <errno.h>
#include "buildconfig.h"
#include "packetsocket.h"

struct __attribute__((__packed__)) udppseudohdr {
	guint32 sa;
	guint32 da;
	guint8 pad;
	guint8 proto;
	guint16 len;
};

static unsigned long packetsocket_ipcsum_next(unsigned long sum, guint16 *data,
		gsize len) {
	for (int i = 0; i < len; i += 2) {
		//TODO add option to pad
		guint16 v = *data++;
		sum += v;
	}
	return sum;
}

static unsigned short packetsocket_ipcsum_finalise(unsigned long sum) {
	unsigned long carry = sum >> 16;
	while (carry != 0) {
		sum = (sum & 0xffff) + carry;
		carry = sum >> 16;
	}
	return ~sum;
}

int packetsocket_createsocket_udp(int ifindex, const guint8* mac) {
	int sock = socket(PF_PACKET, SOCK_DGRAM | SOCK_NONBLOCK, htons(ETH_P_IP));

	struct sockaddr_ll addr;
	memset(&addr, 0, sizeof(addr));
	addr.sll_family = AF_PACKET;
	addr.sll_ifindex = ifindex;
	//addr.sll_protocol = htons(ETH_P_IP);
	memcpy(addr.sll_addr, mac, ETHER_ADDR_LEN);

#ifndef PSNOBIND
	if (bind(sock, (struct sockaddr*) &addr, sizeof(addr)) == -1) {
		g_message("failed to bind socket; %d", errno);
	}
#endif

	int recvbuffsz = 32 * 1024;
	setsockopt(socket, SOL_SOCKET, SO_RCVBUF, &recvbuffsz, sizeof(recvbuffsz));

	g_message("created raw socket %d", sock);
	return sock;
}

void packetsocket_send_udp(int rawsock, int ifindex, guint16 srcprt,
		guint16 dstprt, guint8* payload, gsize payloadlen) {

#ifdef PSDEBUG
	g_message("sending raw %d byte udp packet from %d to %d using interface %d",
			payloadlen, (int ) srcprt, (int ) dstprt, (int ) ifindex);
#endif

	const guint8 broadcast[] = { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };

	struct sockaddr_ll addr;
	memset(&addr, 0, sizeof(addr));
	addr.sll_family = AF_PACKET;
	addr.sll_ifindex = ifindex;
	addr.sll_halen = ETHER_ADDR_LEN;
	addr.sll_protocol = htons(ETH_P_IP);
	memcpy(addr.sll_addr, broadcast, ETHER_ADDR_LEN);

	struct udphdr udphdr;
	memset(&udphdr, 0, sizeof(udphdr));
	udphdr.source = htons(srcprt);
	udphdr.dest = htons(dstprt);
	udphdr.len = htons(sizeof(udphdr) + payloadlen);

	struct iphdr iphdr;
	memset(&iphdr, 0, sizeof(iphdr));
	iphdr.version = 4;
	iphdr.ihl = 5;
	iphdr.ttl = 0xff;
	iphdr.protocol = IPPROTO_UDP;
	iphdr.tot_len = htons(sizeof(udphdr) + sizeof(iphdr) + payloadlen);
	iphdr.daddr = 0xffffffff;
	unsigned long checksum = 0;
	checksum = packetsocket_ipcsum_next(checksum, (guint16*) &iphdr,
			sizeof(iphdr));
	iphdr.check = packetsocket_ipcsum_finalise(checksum);

	unsigned long udpchecksum = 0;
	struct udppseudohdr pseudohdr = { .sa = iphdr.saddr, .da = iphdr.daddr,
			.pad = 0, .proto = iphdr.protocol, .len = udphdr.len };
	udpchecksum = packetsocket_ipcsum_next(udpchecksum, (guint16*) &pseudohdr,
			sizeof(pseudohdr));
	udpchecksum = packetsocket_ipcsum_next(udpchecksum, (guint16*) &udphdr,
			sizeof(udphdr));
	udpchecksum = packetsocket_ipcsum_next(udpchecksum, (guint16*) payload,
			payloadlen);
	udphdr.check = packetsocket_ipcsum_finalise(udpchecksum);

	struct iovec parts[3];
	parts[0].iov_base = &iphdr;
	parts[0].iov_len = sizeof(iphdr);
	parts[1].iov_base = &udphdr;
	parts[1].iov_len = sizeof(udphdr);
	parts[2].iov_base = payload;
	parts[2].iov_len = payloadlen;

	struct msghdr msg;
	memset(&msg, 0, sizeof(msg));
	msg.msg_name = &addr;
	msg.msg_namelen = sizeof(addr);
	msg.msg_iov = parts;
	msg.msg_iovlen = G_N_ELEMENTS(parts);

	if (sendmsg(rawsock, &msg, 0) == -1)
		g_message("failed to send raw udp packet: %d", errno);
}

gssize packetsocket_recv_udp(int fd, int srcport, int destport, guint8* buff,
		gsize buffsz) {
	guint8 rawbuff[1024];
	int read = recv(fd, rawbuff, sizeof(rawbuff), 0);
	if (read <= 0) {
		switch (read) {
		case 0:
#ifdef PSDEBUG
			g_message("0");
#endif
			break;
		case 1:
#ifdef PSDEBUG
			g_message("error while recv'ing from socket %d; %d", fd, errno);
#endif
			break;
		}
		return -1;
	}

	struct iphdr* iphdr = (struct iphdr*) rawbuff;
	struct udphdr* udphdr = (struct udphdr*) (rawbuff + sizeof(*iphdr));

	if (read < (sizeof(*iphdr) + sizeof(*udphdr))) {
#ifdef PSDEBUG
		g_message("packet too short to contain ip + udp header");
#endif
		return -1;
	}

	guint8* payload = rawbuff + sizeof(*iphdr) + sizeof(*udphdr);

	if (iphdr->version != 4 || iphdr->protocol != IPPROTO_UDP) {
#ifdef PSDEBUG
		g_message("not an IPv4 UDP packet; version %d, proto %d",
				(int ) iphdr->version, (int ) iphdr->protocol);
#endif
		return -1;
	}

	int payloadsize = ntohs(udphdr->len) - sizeof(*udphdr);
	int remainingpkt = read - (sizeof(*iphdr) + sizeof(*udphdr));
	if (payloadsize > remainingpkt) {
#ifdef PSDEBUG
		g_message("truncated payload; udp header says %d but only have %d left",
				payloadsize, remainingpkt);
#endif
		return -1;
	}

	int sport = ntohs(udphdr->source);
	if (srcport != -1 && sport != srcport) {
#ifdef PSDEBUG
		g_message("ignoring udp packet from source port %d", sport);
#endif
		return -1;
	}

	int dport = ntohs(udphdr->dest);
	if (destport != -1 && dport != destport) {
#ifdef PSDEBUG
		g_message("ignoring udp packet to dest port %d", dport);
#endif
		return -1;
	}

#ifdef PSDEBUG
	g_message("read %d from socket, srcport %d, dstport %d", read, sport,
			dport);
#endif

	memcpy(buff, payload, payloadsize);
	return payloadsize;
}
