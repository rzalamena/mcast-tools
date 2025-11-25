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

#include <err.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

void	 usage(void);
void	 ip_add_membership(int sock, const struct in_addr *ifaddr,
	    const struct in_addr *group);
void	 ip_block_source(int sock, const struct in_addr *ifaddr,
	    const struct in_addr *group, const struct in_addr *source);
void	 ip_add_source_membership(int sock, const struct in_addr *ifaddr,
	    const struct in_addr *group, const struct in_addr *source);

void
usage(void)
{
	extern const char *__progname;

	fprintf(stderr,
	    "%s: [-e] [-i interface_address] group [source1 source2 ...]\n"
	    "    -e: exclude mode\n"
	    "    -i: interface address\n",
	    __progname);

	exit(1);
}

int
main(int argc, char *argv[])
{
	int i, opt, sock;
	int is_include = 1;
	struct in_addr group, ifaddr, source;

	bzero(&ifaddr, sizeof(ifaddr));

	while ((opt = getopt(argc, argv, "ei:")) != -1) {
		switch (opt) {
		case 'e':
			is_include = 0;
			break;
		case 'i':
			if (inet_pton(AF_INET, optarg, &ifaddr) != 1)
				err(1, "invalid interface address %s", optarg);
			break;
		}
	}
	argc -= optind;
	argv += optind;
	if (argc < 1) {
		fprintf(stderr, "multicast group missing\n");
		usage();
	}
	if (is_include && argc < 2) {
		fprintf(stderr, "include mode requires at least one source\n");
		usage();
	}
	if (inet_pton(AF_INET, argv[0], &group) != 1)
		err(1, "invalid group %s", argv[0]);

	argc -= 1;
	argv += 1;
	sock = socket(AF_INET, SOCK_DGRAM, 0);
	if (sock == -1)
		err(1, "socket");

	if (is_include) {
		for (i = 0; i < argc; i++) {
			if (inet_pton(AF_INET, argv[i], &source) != 1)
				err(1, "invalid source %s", argv[i]);

			ip_add_source_membership(sock, &ifaddr, &group,
			    &source);
		}
	} else {
		ip_add_membership(sock, &ifaddr, &group);
		for (i = 0; i < argc; i++) {
			if (inet_pton(AF_INET, argv[i], &source) != 1)
				err(1, "invalid source %s", argv[i]);

			ip_block_source(sock, &ifaddr, &group, &source);
		}
	}

	poll(NULL, 0, -1);

	return 0;
}

void
ip_add_membership(int sock, const struct in_addr *ifaddr,
    const struct in_addr *group)
{
	struct ip_mreqn imr;

	bzero(&imr, sizeof(imr));
	imr.imr_multiaddr = *group;
	imr.imr_address = *ifaddr;
	if (setsockopt(sock, IPPROTO_IP, IP_ADD_MEMBERSHIP, &imr, sizeof(imr))
	    == -1)
		err(1, "setsockopt IP_ADD_MEMBERSHIP");
}

void
ip_block_source(int sock, const struct in_addr *ifaddr,
    const struct in_addr *group, const struct in_addr *source)
{
	struct ip_mreq_source imr;

	bzero(&imr, sizeof(imr));
	imr.imr_multiaddr = *group;
	imr.imr_interface = *ifaddr;
	imr.imr_sourceaddr = *source;
	if (setsockopt(sock, IPPROTO_IP, IP_BLOCK_SOURCE, &imr, sizeof(imr))
	    == -1)
		err(1, "setsockopt IP_BLOCK_SOURCE");
}

void
ip_add_source_membership(int sock, const struct in_addr *ifaddr,
    const struct in_addr *group, const struct in_addr *source)
{
	struct ip_mreq_source imr;

	bzero(&imr, sizeof(imr));
	imr.imr_multiaddr = *group;
	imr.imr_interface = *ifaddr;
	imr.imr_sourceaddr = *source;
	if (setsockopt(sock, IPPROTO_IP, IP_ADD_SOURCE_MEMBERSHIP, &imr,
	    sizeof(imr)) == -1)
		err(1, "setsockopt IP_ADD_SOURCE_MEMBERSHIP");
}
