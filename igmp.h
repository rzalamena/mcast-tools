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

#ifndef IGMP_H
#define IGMP_H

struct igmpv3_query {
	uint8_t		 iq_type;
	uint8_t		 iq_mrc;	/* maximum response code */
	uint16_t	 iq_cksum;
	struct in_addr	 iq_group;
	uint8_t		 iq_flags;
	uint8_t		 iq_qqic;	/* querier's query interval code */
	uint16_t	 iq_nsrc;	/* number of sources */
	/* struct in_addr iq_srcs[] */
};

#define IGMP_V3_MEMBERSHIP_REPORT 0x22

struct igmpv3_report {
	uint8_t		 ir_type;
	uint8_t		 ir_rsv1;
	uint16_t	 ir_cksum;
	uint16_t	 ir_rsv2;
	uint16_t	 ir_nrec;	/* number of records */
	/* struct igmp_grouprec ir_recs[]; */
};


/* IGMP group record types that can show up in a IGMPv3 Report. */

/* Current-State records */
#define IGMP_RECORD_IS_INCLUDE   1
#define IGMP_RECORD_IS_EXCLUDE   2
/* Filter-Mode-Change records */
#define IGMP_RECORD_TO_INCLUDE   3
#define IGMP_RECORD_TO_EXCLUDE   4
/* Source-List-Change records */
#define IGMP_RECORD_ALLOW_SOURCE 5
#define IGMP_RECORD_BLOCK_SOURCE 6

struct igmp_record {
	uint8_t		 gr_type;
	uint8_t		 gr_auxlen;	/* auxiliary data length */
	uint16_t	 gr_nsrc;	/* number of sources */
	struct in_addr	 gr_group;
	/* struct in_addr gr_srcs[]; */
	/* uint8_t gr_aux[]; */
};

#endif /* IGMP_H */
