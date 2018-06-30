#include <sys/socket.h>
#include <linux/if_packet.h>
#include <arpa/inet.h>
#include <net/ethernet.h>
#include <netinet/ip.h>
#include <netinet/udp.h>
#include <errno.h>
#include "packetsocket.h"

static unsigned short packetsocket_ipcsum(guint16 *data, gsize len) {
	unsigned long sum = 0;

	for (int i = 0; i < len; i += 2)
		sum += ntohs(*data++);

	unsigned long carry = sum >> 16;
	while (carry != 0) {
		sum = (sum & 0xffff) + carry;
		carry = sum >> 16;
	}

	return htons(~sum);
}

int packetsocket_createsocket_udp(int ifindex, const guint8* mac) {
	int sock = socket(PF_PACKET, SOCK_DGRAM, htons(ETH_P_IP));

	struct sockaddr_ll addr;
	memset(&addr, 0, sizeof(addr));
	addr.sll_family = AF_PACKET;
	addr.sll_ifindex = ifindex;
	addr.sll_halen = ETHER_ADDR_LEN;
	addr.sll_protocol = htons(ETH_P_IP);
	memcpy(addr.sll_addr, mac, ETHER_ADDR_LEN);

	if (bind(sock, (struct sockaddr*) &addr, sizeof(addr)) == -1) {
		g_message("failed to bind socket; %d", errno);
	}

	g_message("created raw socket %d", sock);
	return sock;
}

void packetsocket_send_udp(int rawsock, int ifindex, guint16 srcprt,
		guint16 dstprt, guint8* payload, gsize payloadlen) {

	g_message("sending raw %d byte udp packet from %d to %d using interface %d",
			payloadlen, (int ) srcprt, (int ) dstprt, (int ) ifindex);

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
	iphdr.check = packetsocket_ipcsum((guint16*) &iphdr, sizeof(iphdr));

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

int packetsocket_recv_udp(int fd, int srcport, int destport, guint8* buff,
		gsize buffsz) {
	guint8 rawbuff[1024];
	int read = recv(fd, rawbuff, sizeof(rawbuff), 0);
	if (read == -1) {
		g_message("error while recv'ing from socket %d; %d", fd, errno);
		return -1;
	}

	g_message("read %d from socket", read);

	struct iphdr* iphdr = (struct iphdr*) rawbuff;
	struct udphdr* udphdr = (struct udphdr*) (rawbuff + sizeof(*iphdr));
	guint8* payload = rawbuff + sizeof(*iphdr) + sizeof(*udphdr);

	if (iphdr->version != 4 || iphdr->protocol != IPPROTO_UDP) {
		g_message("not an IPv4 UDP packet");
		return -1;
	}

	if (srcport != -1 && ntohs(udphdr->source) != srcport) {
		return -1;
	}

	if (destport != -1 && ntohs(udphdr->dest) != destport) {
		return -1;
	}

	g_message("have UDP packet");
	int payloadsize = ntohs(udphdr->len) - sizeof(*udphdr);
	memcpy(buff, payload, payloadsize);
	return payloadsize;
}
