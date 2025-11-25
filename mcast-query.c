/*
 * Copyright (c) 2025 Rafael Zalamena <rzalamena@gmail.com>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <arpa/inet.h>
#include <netinet/igmp.h>
#include <netinet/ip.h>

#include <err.h>
#include <errno.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "igmp.h"

void		 usage(void);
void		 ip_add_membership(int, struct in_addr *, struct in_addr *);
void		 mcast_parse(uint8_t *, size_t);
void		 mcast_parse_query(uint8_t *, size_t);
void		 mcast_parse_report(uint8_t *, size_t);
void		 mcast_send_query(int);
uint16_t	 in_cksum(void *, size_t);

void
usage(void)
{
	extern const char	*__progname;

	fprintf(stderr, "%s: [-i interface_address]\n",
	    __progname);

	exit(1);
}

int
main(int argc, char *argv[])
{
	int			 n, opt, ready, sock;
	struct pollfd		 pfd;
	struct in_addr		 group, ifaddr;
	uint8_t			 msg[1516];

	while ((opt = getopt(argc, argv, "i:")) != -1) {
		switch (opt) {
		case 'i':
			if (inet_pton(AF_INET, optarg, &ifaddr) != 1)
				err(1, "invalid interface address %s", optarg);
			break;
		}
	}

	sock = socket(AF_INET, SOCK_RAW, IPPROTO_IGMP);
	if (sock == -1)
		err(1, "socket");

	inet_pton(AF_INET, "224.0.0.22", &group);
	ip_add_membership(sock, &ifaddr, &group);

	pfd.fd = sock;
	pfd.events = POLLIN;

	mcast_send_query(sock);

	do {
		ready = poll(&pfd, 1, -1);
		if (ready == -1)
			break;
		if (ready == 0)
			continue;

		if (pfd.revents & POLLIN) {
			n = recv(pfd.fd, msg, sizeof(msg), 0);
			if (n == -1) {
				if (errno == EAGAIN || errno == EWOULDBLOCK ||
				    errno == EINTR)
					continue;

				err(1, "recv");
			}
			if (n == 0)
				errx(1, "recv: eof");

			mcast_parse(msg, n);
		}
	} while (1);

	return 0;
}

void
ip_add_membership(int sock, struct in_addr *ifaddr, struct in_addr *group)
{
	struct ip_mreqn		 imr;

	bzero(&imr, sizeof(imr));
	imr.imr_multiaddr = *group;
	imr.imr_address = *ifaddr;
	if (setsockopt(sock, IPPROTO_IP, IP_ADD_MEMBERSHIP, &imr, sizeof(imr))
	    == -1)
		err(1, "setsockopt IP_ADD_MEMBERSHIP");
}

void
mcast_parse(uint8_t *data, size_t data_len)
{
	struct ip	*ip;
	struct igmp	*igmp;
	char		 address[INET_ADDRSTRLEN];

	if (data_len < sizeof(struct ip))
		return;

	ip = (struct ip *)data;
	data += ip->ip_hl << 2;
	data_len -= ip->ip_hl << 2;

	inet_ntop(AF_INET, &ip->ip_src, address, sizeof(address));
	printf("\n%s", address);
	inet_ntop(AF_INET, &ip->ip_dst, address, sizeof(address));
	printf(" -> %s\n", address);

	igmp = (struct igmp *)data;
	switch (igmp->igmp_type) {
	case IGMP_MEMBERSHIP_QUERY:
		if (data_len >= sizeof(struct igmpv3_query)) {
			mcast_parse_query(data, data_len);
			break;
		}

		inet_ntop(AF_INET, &igmp->igmp_group, address, sizeof(address));
		printf("IGMPv1 or IGMPv2 Query: group %s\n", address);
		break;
	case IGMP_V1_MEMBERSHIP_REPORT:
	case IGMP_V2_MEMBERSHIP_REPORT:
		if (igmp->igmp_type == IGMP_V1_MEMBERSHIP_REPORT)
			printf("IGMPv1 Report ");
		else
			printf("IGMPv2 Report ");

		inet_ntop(AF_INET, &igmp->igmp_group, address, sizeof(address));
		printf("code:%d group:%s\n", igmp->igmp_code, address);
		break;
	case IGMP_V3_MEMBERSHIP_REPORT:
		mcast_parse_report(data, data_len);
		break;
	}
}

void
mcast_parse_query(uint8_t *data, size_t data_len)
{
	struct igmpv3_query *iq;
	int i;
	char address[INET_ADDRSTRLEN];

	if (data_len < sizeof(struct igmpv3_query))
		return;

	iq = (struct igmpv3_query *)data;
	inet_ntop(AF_INET, &iq->iq_group, address, sizeof(address));
	printf("IGMPv3 Query mrc:%d group:%s flags:0x%02x qqic:%d nsrc:%d\n",
	    iq->iq_mrc, address, iq->iq_flags, iq->iq_qqic, ntohs(iq->iq_nsrc));

	data += sizeof(struct igmpv3_query);
	for (i = 0; i < ntohs(iq->iq_nsrc); i++) {
		inet_ntop(AF_INET, &data, address, sizeof(address));
		printf("  source %s\n", address);

		data += sizeof(struct in_addr);
	}
}

void
mcast_parse_report(uint8_t *data, size_t data_len)
{
	struct igmp_record	*gr;
	struct igmpv3_report	*ir;
	int			 nrecs, nsrcs;
	size_t			 rectotal;
	char			 address[INET_ADDRSTRLEN];

	if (data_len < sizeof(struct igmpv3_report))
		return;

	ir = (struct igmpv3_report *)data;
	printf("IGMPv3 Report nrecs:%d\n", ntohs(ir->ir_nrec));

	data += sizeof(struct igmpv3_report);
	data_len -= sizeof(struct igmpv3_report);

	for (nrecs = ntohs(ir->ir_nrec); nrecs > 0; nrecs--) {
		if (data_len < sizeof(struct igmp_record))
			return;

		gr = (struct igmp_record *)data;
		inet_ntop(AF_INET, &gr->gr_group, address, sizeof(address));
		printf("  %s ", address);
		switch (gr->gr_type) {
		case IGMP_RECORD_IS_INCLUDE:
			printf("is_in ");
			break;
		case IGMP_RECORD_IS_EXCLUDE:
			printf("is_ex ");
			break;
		case IGMP_RECORD_TO_INCLUDE:
			printf("to_in ");
			break;
		case IGMP_RECORD_TO_EXCLUDE:
			printf("to_ex ");
			break;
		case IGMP_RECORD_ALLOW_SOURCE:
			printf("allow ");
			break;
		case IGMP_RECORD_BLOCK_SOURCE:
			printf("block ");
			break;
		}
		printf("nsrc:%d\n", ntohs(gr->gr_nsrc));

		data += sizeof(struct igmp_record);
		data_len -= sizeof(struct igmp_record);

		rectotal = ntohs(gr->gr_nsrc) * sizeof(uint32_t);
		if (data_len < rectotal)
			return;

		for (nsrcs = ntohs(gr->gr_nsrc); nsrcs > 0; nsrcs--) {
			inet_ntop(AF_INET, data, address, sizeof(address));
			printf("    %s\n", address);

			data += sizeof(uint32_t);
			data_len -= sizeof(uint32_t);
		}
	}
}

uint16_t
in_cksum(void *data, size_t data_len)
{
	uint16_t *wordp = data;
	uint32_t sum = 0;

	while (data_len > 0) {
		if (data_len == 1) {
			sum += *(uint8_t *)wordp;
			break;
		}

		sum += *wordp;
		data_len -= 2;
		wordp++;
	}

	while (sum >> 16)
		sum = (sum >> 16) + (sum & 0xffff);

	return (~sum);
}

void
mcast_send_query(int sock)
{
	struct igmpv3_query	 iq;
	struct msghdr		 msg;
	struct sockaddr_in	 sin;
	struct iovec		 iov[1];

	iq.iq_type = IGMP_MEMBERSHIP_QUERY;
	iq.iq_mrc = 100;
	iq.iq_group.s_addr = 0;
	iq.iq_flags = 2;
	iq.iq_qqic = 125;
	iq.iq_nsrc = 0;
	iq.iq_cksum = in_cksum(&iq, sizeof(iq));

	iov[0].iov_base = &iq;
	iov[0].iov_len = sizeof(iq);

	sin.sin_family = AF_INET;
	sin.sin_addr.s_addr = htonl(0xe0000001);

	bzero(&msg, sizeof(msg));
	msg.msg_name = &sin;
	msg.msg_namelen = sizeof(sin);
	msg.msg_iov = iov;
	msg.msg_iovlen = 1;
	if (sendmsg(sock, &msg, 0) <= 0)
		err(1, "sendmsg");
}
