/**
 * @file
 * TCP internal implementations (do not use in application code)
 */

/*
 * Copyright (c) 2001-2004 Swedish Institute of Computer Science.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT
 * SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT
 * OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY
 * OF SUCH DAMAGE.
 *
 * This file is part of the lwIP TCP/IP stack.
 *
 * Author: Adam Dunkels <adam@sics.se>
 *
 */
#ifndef LWIP_HDR_TCP_PRIV_H
#define LWIP_HDR_TCP_PRIV_H

#include "lwip/opt.h"

#if LWIP_TCP /* don't build if not configured for use in lwipopts.h */

#include "lwip/tcp.h"
#include "lwip/mem.h"
#include "lwip/pbuf.h"
#include "lwip/ip.h"
#include "lwip/icmp.h"
#include "lwip/err.h"
#include "lwip/ip6.h"
#include "lwip/ip6_addr.h"
#include "lwip/prot/tcp.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Functions for interfacing with TCP: */

/* Lower layer interface to TCP: */
void             tcp_init    (void);  /* Initialize this module. */
void             tcp_tmr     (void);  /* Must be called every
                                         TCP_TMR_INTERVAL
                                         ms. (Typically 250 ms). */
/* It is also possible to call these two functions at the right
   intervals (instead of calling tcp_tmr()). */
void             tcp_slowtmr (void);
void             tcp_fasttmr (void);

/* Call this from a netif driver (watch out for threading issues!) that has
   returned a memory error on transmit and now has free buffers to send more.
   This iterates all active pcbs that had an error and tries to call
   tcp_output, so use this with care as it might slow down the system. */
void             tcp_txnow   (void);

/* Only used by IP to pass a TCP segment to TCP: */
void             tcp_input   (struct pbuf *p, struct netif *inp);
/* Used within the TCP code only: */
struct tcp_pcb * tcp_alloc   (u8_t prio);
void             tcp_free    (struct tcp_pcb *pcb);
void             tcp_abandon (struct tcp_pcb *pcb, int reset);
err_t            tcp_send_empty_ack(struct tcp_pcb *pcb);
err_t            tcp_rexmit  (struct tcp_pcb *pcb);
err_t            tcp_rexmit_rto_prepare(struct tcp_pcb *pcb);
void             tcp_rexmit_rto_commit(struct tcp_pcb *pcb);
void             tcp_rexmit_rto  (struct tcp_pcb *pcb);
void             tcp_rexmit_fast (struct tcp_pcb *pcb);
u32_t            tcp_update_rcv_ann_wnd(struct tcp_pcb *pcb);
err_t            tcp_process_refused_data(struct tcp_pcb *pcb);

#define TCP_SEQ_LT(a,b)     ((s32_t)((u32_t)(a) - (u32_t)(b)) < 0)
#define TCP_SEQ_LEQ(a,b)    ((s32_t)((u32_t)(a) - (u32_t)(b)) <= 0)
#define TCP_SEQ_GT(a,b)     ((s32_t)((u32_t)(a) - (u32_t)(b)) > 0)
#define TCP_SEQ_GEQ(a,b)    ((s32_t)((u32_t)(a) - (u32_t)(b)) >= 0)
/* is b<=a<=c? */
#define TCP_SEQ_BETWEEN(a,b,c) (TCP_SEQ_GEQ(a,b) && TCP_SEQ_LEQ(a,c))

/* Minshall's variant of the Nagle send check. */
#define tcp_nagle_minshall_check(tpcb) (TCP_SEQ_GT(tpcb->snd_sml, tpcb->lastack) && \
                                        TCP_SEQ_LEQ(tpcb->snd_sml, tpcb->snd_nxt))

/**
 * This is the Nagle algorithm: try to combine user data to send as few TCP
 * segments as possible. Only send if
 * - no previously transmitted data on the connection remains unacknowledged or
 * - the TF_NODELAY flag is set (nagle algorithm turned off for this pcb) or
 * - the only unsent segment is at least pcb->mss bytes long (or there is more
 *   than one unsent segment - with lwIP, this can happen although unsent->len < mss)
 * - or if we are in fast-retransmit (TF_INFR)
 * - With Minshall's variant: all sent small packets are ACKed.
 */
#define tcp_do_output_nagle(tpcb) ((((tpcb)->unacked == NULL) || \
                            ((tpcb)->flags & (TF_NODELAY | TF_INFR)) || \
                            (((tpcb)->unsent != NULL) && (((tpcb)->unsent->next != NULL) || \
                              ((tpcb)->unsent->len >= (tpcb)->mss) || !tcp_nagle_minshall_check(tpcb))) \
                            ) ? 1 : 0)

#define tcp_output_nagle(tpcb) (tcp_do_output_nagle(tpcb) ? tcp_output(tpcb) : ERR_OK)

/*
  100 value looks awkward,
  A fast timer every 100 milliseconds (yet beats delay ack duration of 200msec of linux.)
  Slow timer every 200 milliseconds,
*/
#ifndef TCP_TMR_INTERVAL
#define TCP_TMR_INTERVAL       100  /* The TCP timer interval in milliseconds. */
#endif /* TCP_TMR_INTERVAL */

#ifndef TCP_FAST_INTERVAL
#define TCP_FAST_INTERVAL      TCP_TMR_INTERVAL /* the fine grained timeout in milliseconds */
#endif /* TCP_FAST_INTERVAL */

#ifndef TCP_SLOW_INTERVAL

#ifndef TCP_SLOW_INTERVAL_PERIOD
#define TCP_SLOW_INTERVAL_PERIOD      2 /* the fine grained timeout in milliseconds */
#endif /* TCP_SLOW_INTERVAL_PERIOD */
/* the coarse grained timeout in milliseconds */
#define TCP_SLOW_INTERVAL      ((TCP_SLOW_INTERVAL_PERIOD) * (TCP_TMR_INTERVAL))
#endif /* TCP_SLOW_INTERVAL */

/**
 * TCP_INITIAL_RTO_DURATION: Minimum RTO default duration has been reduced to 1 second from 3 seconds, RFC6298
 */
#ifndef TCP_INITIAL_RTO_DURATION
#define TCP_INITIAL_RTO_DURATION 1000
#endif

/* RTO druation of 400ms ensures that RTO expiry will be always between 200ms-400ms */
#ifndef TCP_MIN_RTO_DURATION
#define TCP_MIN_RTO_DURATION 400
#endif

#ifndef TCP_RTO_DURATION_AFTER_SYN_RTX
#define TCP_RTO_DURATION_AFTER_SYN_RTX 3000
#endif

/**
 * TCP_MAX_RTO_DURATION A maximum value MAY be placed on RTO provided it is at least 60 seconds.
 */
#ifndef TCP_MAX_RTO_DURATION
#define TCP_MAX_RTO_DURATION 60000
#endif

#ifndef TCP_MIN_RTO_TICKS
#define TCP_MIN_RTO_TICKS (TCP_MIN_RTO_DURATION / TCP_SLOW_INTERVAL)
#endif

#ifndef TCP_MAX_RTO_TICKS
#define TCP_MAX_RTO_TICKS (TCP_MAX_RTO_DURATION / TCP_SLOW_INTERVAL)
#endif

#ifndef TCP_RTO_TICKS_AFTER_SYN_RTX
#define TCP_RTO_TICKS_AFTER_SYN_RTX (TCP_RTO_DURATION_AFTER_SYN_RTX / TCP_SLOW_INTERVAL)
#endif

#if DRIVER_STATUS_CHECK
/* will round off to the higher value in case it is not multiples of TCP_SLOW_INTERVAL */
#define DRIVER_WAKEUP_COUNT ((DRIVER_WAKEUP_INTERVAL + (TCP_SLOW_INTERVAL - 1)) / TCP_SLOW_INTERVAL)
#endif

#define TCP_FIN_WAIT_TIMEOUT 20000 /* milliseconds */
#define TCP_SYN_RCVD_TIMEOUT 20000 /* milliseconds */

#define TCP_OOSEQ_TIMEOUT        6U /* x RTO */

#ifndef TCP_MSL
#define TCP_MSL 60000UL /* The maximum segment lifetime in milliseconds */
#endif

/* Keepalive values, compliant with RFC 1122. Don't change this unless you know what you're doing */
#ifndef  TCP_KEEPIDLE_DEFAULT
#define  TCP_KEEPIDLE_DEFAULT     7200000UL /* Default KEEPALIVE timer in milliseconds */
#endif

#ifndef  TCP_KEEPINTVL_DEFAULT
#define  TCP_KEEPINTVL_DEFAULT    75000UL   /* Default Time between KEEPALIVE probes in milliseconds */
#endif

#ifndef  TCP_KEEPCNT_DEFAULT
#define  TCP_KEEPCNT_DEFAULT      9U        /* Default Counter for KEEPALIVE probes */
#endif

#define  TCP_MAXIDLE              TCP_KEEPCNT_DEFAULT * TCP_KEEPINTVL_DEFAULT  /* Maximum KEEPALIVE probe time */

#define TCP_TCPLEN(seg) ((seg)->len + (((TCPH_FLAGS((seg)->tcphdr) & (TCP_FIN | TCP_SYN)) != 0) ? 1U : 0U))

/** Flags used on input processing, not on pcb->flags
*/
#define TF_RESET     (u8_t)0x08U   /* Connection was reset. */
#define TF_CLOSED    (u8_t)0x10U   /* Connection was successfully closed. */
#define TF_GOT_FIN   (u8_t)0x20U   /* Connection was closed by the remote end. */


#if LWIP_EVENT_API

#define TCP_EVENT_ACCEPT(lpcb,pcb,arg,err,ret) ret = lwip_tcp_event(arg, (pcb),\
                LWIP_EVENT_ACCEPT, NULL, 0, err)
#define TCP_EVENT_SENT(pcb,space,ret) ret = lwip_tcp_event((pcb)->callback_arg, (pcb),\
                   LWIP_EVENT_SENT, NULL, space, ERR_OK)
#define TCP_EVENT_RECV(pcb,p,err,ret) ret = lwip_tcp_event((pcb)->callback_arg, (pcb),\
                LWIP_EVENT_RECV, (p), 0, (err))
#define TCP_EVENT_CLOSED(pcb,ret) ret = lwip_tcp_event((pcb)->callback_arg, (pcb),\
                LWIP_EVENT_RECV, NULL, 0, ERR_OK)
#define TCP_EVENT_CONNECTED(pcb,err,ret) ret = lwip_tcp_event((pcb)->callback_arg, (pcb),\
                LWIP_EVENT_CONNECTED, NULL, 0, (err))
#define TCP_EVENT_POLL(pcb,ret)       do { if ((pcb)->state != SYN_RCVD) {                          \
                ret = lwip_tcp_event((pcb)->callback_arg, (pcb), LWIP_EVENT_POLL, NULL, 0, ERR_OK); \
                } else {                                                                            \
                ret = ERR_ARG; } } while(0)
/* For event API, last state SYN_RCVD must be excluded here: the application
   has not seen this pcb, yet! */
#define TCP_EVENT_ERR(last_state,errf,arg,err)  do { if (last_state != SYN_RCVD) {                \
                lwip_tcp_event((arg), NULL, LWIP_EVENT_ERR, NULL, 0, (err)); } } while(0)

#else /* LWIP_EVENT_API */

#define TCP_EVENT_ACCEPT(lpcb,pcb,arg,err,ret)                 \
  do {                                                         \
    if((lpcb)->accept != NULL)                                 \
      (ret) = (lpcb)->accept((arg),(pcb),(err));               \
    else (ret) = ERR_ARG;                                      \
  } while (0)

#define TCP_EVENT_SENT(pcb,space,ret)                          \
  do {                                                         \
    if((pcb)->sent != NULL)                                    \
      (ret) = (pcb)->sent((pcb)->callback_arg,(pcb),(space));  \
    else (ret) = ERR_OK;                                       \
  } while (0)

#define TCP_EVENT_RECV(pcb,p,err,ret)                          \
  do {                                                         \
    if((pcb)->recv != NULL) {                                  \
      (ret) = (pcb)->recv((pcb)->callback_arg,(pcb),(p),(err));\
    } else {                                                   \
      (ret) = tcp_recv_null(NULL, (pcb), (p), (err));          \
    }                                                          \
  } while (0)

#define TCP_EVENT_CLOSED(pcb,ret)                                \
  do {                                                           \
    if(((pcb)->recv != NULL)) {                                  \
      (ret) = (pcb)->recv((pcb)->callback_arg,(pcb),NULL,ERR_OK);\
    } else {                                                     \
      (ret) = ERR_OK;                                            \
    }                                                            \
  } while (0)

#define TCP_EVENT_CONNECTED(pcb,err,ret)                         \
  do {                                                           \
    if((pcb)->connected != NULL)                                 \
      (ret) = (pcb)->connected((pcb)->callback_arg,(pcb),(err)); \
    else (ret) = ERR_OK;                                         \
  } while (0)

#define TCP_EVENT_POLL(pcb,ret)                                \
  do {                                                         \
    if((pcb)->poll != NULL)                                    \
      (ret) = (pcb)->poll((pcb)->callback_arg,(pcb));          \
    else (ret) = ERR_OK;                                       \
  } while (0)

#define TCP_EVENT_ERR(last_state,errf,arg,err)                 \
  do {                                                         \
    LWIP_UNUSED_ARG(last_state);                               \
    if((errf) != NULL)                                         \
      (errf)((arg),(err));                                     \
  } while (0)

#endif /* LWIP_EVENT_API */

/** Enabled extra-check for TCP_OVERSIZE if LWIP_DEBUG is enabled */
#if TCP_OVERSIZE && defined(LWIP_DEBUG)
#define TCP_OVERSIZE_DBGCHECK 1
#else
#define TCP_OVERSIZE_DBGCHECK 0
#endif

/** Don't generate checksum on copy if CHECKSUM_GEN_TCP is disabled */
#define TCP_CHECKSUM_ON_COPY  (LWIP_CHECKSUM_ON_COPY && CHECKSUM_GEN_TCP)

/* This structure represents a TCP segment on the unsent, unacked and ooseq queues */
struct tcp_seg {
  struct tcp_seg *next;    /* used when putting segments on a queue */
  struct pbuf *p;          /* buffer containing data + TCP header */
  u16_t len;               /* the TCP length of this segment */
#if TCP_OVERSIZE_DBGCHECK
  u16_t oversize_left;     /* Extra bytes available at the end of the last
                              pbuf in unsent (used for asserting vs.
                              tcp_pcb.unsent_oversize only) */
#endif /* TCP_OVERSIZE_DBGCHECK */
#if TCP_CHECKSUM_ON_COPY
  u16_t chksum;
  u8_t  chksum_swapped;
#endif /* TCP_CHECKSUM_ON_COPY */
  u8_t  flags;
#define TF_SEG_OPTS_MSS         (u8_t)0x01U /* Include MSS option (only used in SYN segments) */
#define TF_SEG_OPTS_TS          (u8_t)0x02U /* Include timestamp option. */
#define TF_SEG_DATA_CHECKSUMMED (u8_t)0x04U /* ALL data (not the header) is
                                               checksummed into 'chksum' */
#define TF_SEG_OPTS_WND_SCALE   (u8_t)0x08U /* Include WND SCALE option (only used in SYN segments) */

  /* Adding for SACK */
#define TF_SEG_OPTS_SACK        (u8_t)0x10U /* Include SACK option */
#define TF_SEG_OPTS_SACK_PERMITTED        (u8_t)0x20U /* Include SACK Permitted option */
#define TF_SEG_OPTS_SACK_OPTIONS          (u8_t)0x40U /* Include SACK Options */

  /* Adding for SACK */
#if LWIP_SACK

  u32_t  state;         /* This will be used to maintain SACK scoreboard as per RFC 6675 */
#define TF_SEG_NONE            0x0000U
#define TF_SEG_SACKED          0x0001U /* Segment Sacked */
#define TF_SEG_RETRANSMITTED   0x0002U /* Retransmitted as part of SACK based loss recovery alg */

  u32_t order; /* order of the segment in the ooseq q */
#if LWIP_SACK_PERF_OPT
  u32_t pkt_trans_seq_cntr;
#endif
#endif /* LWIP_SACK */

#if DRIVER_STATUS_CHECK
  u32_t seg_type;
#endif
  struct tcp_hdr *tcphdr;  /* the TCP header */
};

#define LWIP_TCP_OPT_EOL        0
#define LWIP_TCP_OPT_NOP        1
#define LWIP_TCP_OPT_MSS        2
#define LWIP_TCP_OPT_WS         3
#define LWIP_TCP_OPT_TS         8

/* Adding for SACK */
#define LWIP_TCP_OPT_SACK_PERMITTED   4
#define LWIP_TCP_OPT_SACK             5

#define LWIP_TCP_OPT_LEN_MSS    4
#if LWIP_TCP_TIMESTAMPS
#define LWIP_TCP_OPT_LEN_TS     10
#define LWIP_TCP_OPT_LEN_TS_OUT 12 /* aligned for output (includes NOP padding) */
#else
#define LWIP_TCP_OPT_LEN_TS_OUT 0
#endif
#if LWIP_WND_SCALE
#define LWIP_TCP_OPT_LEN_WS     3
#define LWIP_TCP_OPT_LEN_WS_OUT 4 /* aligned for output (includes NOP padding) */
#else
#define LWIP_TCP_OPT_LEN_WS_OUT 0
#endif

#if LWIP_SACK
#define LWIP_TCP_OPT_LEN_SACK_PERMITTED     2
#define LWIP_TCP_OPT_LEN_SACK_PERMITTED_OUT 4 /* aligned for output (includes NOP padding) */
#define LWIP_TCP_OPT_SACK_MIN_LEN 10
#else
#define LWIP_TCP_OPT_LEN_SACK_PERMITTED_OUT 0
#endif

#define LWIP_TCP_OPT_LENGTH(flags) \
  ((flags) & TF_SEG_OPTS_MSS       ? LWIP_TCP_OPT_LEN_MSS           : 0) + \
  ((flags) & TF_SEG_OPTS_TS        ? LWIP_TCP_OPT_LEN_TS_OUT        : 0) + \
  ((flags) & TF_SEG_OPTS_WND_SCALE ? LWIP_TCP_OPT_LEN_WS_OUT        : 0) + \
  ((flags) & TF_SEG_OPTS_SACK_PERMITTED ? LWIP_TCP_OPT_LEN_SACK_PERMITTED_OUT : 0)

/* 4 -> padding + kind + length. 8 -> each SACK block */
#define LWIP_TCP_SACK_OPT_LENGTH(__cnt)    ((__cnt) > 0 ? (u8_t)(4 + ((__cnt) * 8)) : 0)

/** This returns a TCP header option for MSS in an u32_t */
#define TCP_BUILD_MSS_OPTION(mss) lwip_htonl(0x02040000 | ((mss) & 0xFFFF))

#if LWIP_WND_SCALE
#define TCPWNDSIZE_F       U32_F
#define TCPWND_MAX         0xFFFFFFFFU
#define TCPWND_CHECK16(x)  LWIP_ASSERT("window size > 0xFFFF", (x) <= 0xFFFF)
#define TCPWND_MIN16(x)    ((u16_t)LWIP_MIN((x), 0xFFFF))
#else /* LWIP_WND_SCALE */
#define TCPWNDSIZE_F       U16_F
#define TCPWND_MAX         0xFFFFU
#define TCPWND_CHECK16(x)
#define TCPWND_MIN16(x)    x
#endif /* LWIP_WND_SCALE */

/* Global variables: */
extern struct tcp_pcb *tcp_input_pcb;
extern u32_t tcp_ticks;
extern u8_t tcp_active_pcbs_changed;

/* The TCP PCB lists. */
union tcp_listen_pcbs_t { /* List of all TCP PCBs in LISTEN state. */
  struct tcp_pcb_listen *listen_pcbs;
  struct tcp_pcb *pcbs;
};
extern struct tcp_pcb *tcp_bound_pcbs;
extern union tcp_listen_pcbs_t tcp_listen_pcbs;
extern struct tcp_pcb *tcp_active_pcbs;  /* List of all TCP PCBs that are in a
              state in which they accept or send
              data. */
extern struct tcp_pcb *tcp_tw_pcbs;      /* List of all TCP PCBs in TIME-WAIT. */

#define NUM_TCP_PCB_LISTS_NO_TIME_WAIT  3
#define NUM_TCP_PCB_LISTS               4
extern struct tcp_pcb ** const tcp_pcb_lists[NUM_TCP_PCB_LISTS];

/* Axioms about the above lists:
   1) Every TCP PCB that is not CLOSED is in one of the lists.
   2) A PCB is only in one of the lists.
   3) All PCBs in the tcp_listen_pcbs list is in LISTEN state.
   4) All PCBs in the tcp_tw_pcbs list is in TIME-WAIT state.
*/
/* Define two macros, TCP_REG and TCP_RMV that registers a TCP PCB
   with a PCB list or removes a PCB from a list, respectively. */
#ifndef TCP_DEBUG_PCB_LISTS
#define TCP_DEBUG_PCB_LISTS 0
#endif
#if TCP_DEBUG_PCB_LISTS
#define TCP_REG(pcbs, npcb) do {\
                            struct tcp_pcb *tcp_tmp_pcb; \
                            LWIP_DEBUGF(TCP_DEBUG, ("TCP_REG %p local port %"U16_F"\n", (void *)(npcb), (npcb)->local_port)); \
                            for (tcp_tmp_pcb = *(pcbs); \
          tcp_tmp_pcb != NULL; \
        tcp_tmp_pcb = tcp_tmp_pcb->next) { \
                                LWIP_ASSERT("TCP_REG: already registered\n", tcp_tmp_pcb != (npcb)); \
                            } \
                            LWIP_ASSERT("TCP_REG: pcb->state != CLOSED", ((pcbs) == &tcp_bound_pcbs) || ((npcb)->state != CLOSED)); \
                            (npcb)->next = *(pcbs); \
                            LWIP_ASSERT("TCP_REG: npcb->next != npcb", (npcb)->next != (npcb)); \
                            *(pcbs) = (npcb); \
                            LWIP_ASSERT("TCP_REG: tcp_pcbs sane", tcp_pcbs_sane()); \
              tcp_timer_needed(); \
                            } while(0)
#define TCP_RMV(pcbs, npcb) do { \
                            struct tcp_pcb *tcp_tmp_pcb; \
                            LWIP_ASSERT("TCP_RMV: pcbs != NULL", *(pcbs) != NULL); \
                            LWIP_DEBUGF(TCP_DEBUG, ("TCP_RMV: removing %p from %p\n", (void *)(npcb), (void *)(*(pcbs)))); \
                            if(*(pcbs) == (npcb)) { \
                               *(pcbs) = (*pcbs)->next; \
                            } else for (tcp_tmp_pcb = *(pcbs); tcp_tmp_pcb != NULL; tcp_tmp_pcb = tcp_tmp_pcb->next) { \
                               if(tcp_tmp_pcb->next == (npcb)) { \
                                  tcp_tmp_pcb->next = (npcb)->next; \
                                  break; \
                               } \
                            } \
                            (npcb)->next = NULL; \
                            LWIP_ASSERT("TCP_RMV: tcp_pcbs sane", tcp_pcbs_sane()); \
                            LWIP_DEBUGF(TCP_DEBUG, ("TCP_RMV: removed %p from %p\n", (void *)(npcb), (void *)(*(pcbs)))); \
                            } while(0)

#else /* LWIP_DEBUG */

#define TCP_REG(pcbs, npcb)                        \
  do {                                             \
    (npcb)->next = *pcbs;                          \
    *(pcbs) = (npcb);                              \
    tcp_timer_needed();                            \
  } while (0)

#if !LWIP_SMALL_SIZE
#define TCP_RMV(pcbs, npcb)                        \
  do {                                             \
    if(*(pcbs) == (npcb)) {                        \
      (*(pcbs)) = (*pcbs)->next;                   \
    }                                              \
    else {                                         \
      struct tcp_pcb *tcp_tmp_pcb;                 \
      for (tcp_tmp_pcb = *pcbs;                    \
          tcp_tmp_pcb != NULL;                     \
          tcp_tmp_pcb = tcp_tmp_pcb->next) {       \
        if(tcp_tmp_pcb->next == (npcb)) {          \
          tcp_tmp_pcb->next = (npcb)->next;        \
          break;                                   \
        }                                          \
      }                                            \
    }                                              \
    (npcb)->next = NULL;                           \
  } while (0)
#else
void tcp_remove(struct tcp_pcb **pcbs, struct tcp_pcb *npcb);
#define TCP_RMV(pcbs, npcb) tcp_remove(pcbs, npcb)
#endif

#endif /* LWIP_DEBUG */

#define TCP_REG_ACTIVE(npcb)                       \
  do {                                             \
    TCP_REG(&tcp_active_pcbs, npcb);               \
    tcp_active_pcbs_changed = 1;                   \
  } while (0)

#define TCP_RMV_ACTIVE(npcb)                       \
  do {                                             \
    TCP_RMV(&tcp_active_pcbs, npcb);               \
    tcp_active_pcbs_changed = 1;                   \
  } while (0)

#define TCP_PCB_REMOVE_ACTIVE(pcb)                 \
  do {                                             \
    tcp_pcb_remove(&tcp_active_pcbs, pcb);         \
    tcp_active_pcbs_changed = 1;                   \
  } while (0)


/* Internal functions: */
struct tcp_pcb *tcp_pcb_copy(struct tcp_pcb *pcb);
void tcp_pcb_purge(struct tcp_pcb *pcb);
void tcp_pcb_remove(struct tcp_pcb **pcblist, struct tcp_pcb *pcb);

#if LWIP_SACK_PERF_OPT
void tcp_fr_segs_free(struct tcp_sack_fast_rxmited *seg);
#endif

void tcp_segs_free(struct tcp_seg *seg);
void tcp_seg_free(struct tcp_seg *seg);
struct tcp_seg *tcp_seg_copy(struct tcp_seg *seg);

#define tcp_ack(pcb)                               \
  do {                                             \
    if((pcb)->flags & TF_ACK_DELAY) {              \
      tcp_clear_flags(pcb, TF_ACK_DELAY);          \
      tcp_ack_now(pcb);                            \
    }                                              \
    else {                                         \
      tcp_set_flags(pcb, TF_ACK_DELAY);            \
    }                                              \
  } while (0)

#define tcp_ack_now(pcb)                           \
  tcp_set_flags(pcb, TF_ACK_NOW)

err_t tcp_send_fin(struct tcp_pcb *pcb);
err_t tcp_enqueue_flags(struct tcp_pcb *pcb, u8_t flags);

void tcp_rexmit_seg(struct tcp_pcb *pcb, struct tcp_seg *seg);

void tcp_rst(struct tcp_pcb *pcb, u32_t seqno, u32_t ackno,
       const ip_addr_t *local_ip, const ip_addr_t *remote_ip,
       u16_t local_port, u16_t remote_port);

u32_t tcp_next_iss(struct tcp_pcb *pcb);

err_t tcp_keepalive(struct tcp_pcb *pcb);
err_t tcp_split_unsent_seg(struct tcp_pcb *pcb, u16_t split);
err_t tcp_zero_window_probe(struct tcp_pcb *pcb);
void  tcp_trigger_input_pcb_close(void);

#if TCP_CALCULATE_EFF_SEND_MSS
u16_t tcp_eff_send_mss_netif(u16_t sendmss, struct netif *outif,
                             const ip_addr_t *dest);
#define tcp_eff_send_mss(sendmss, pcb, dest) \
    tcp_eff_send_mss_netif(sendmss, ip_route_pcb(dest, pcb), dest)
#endif /* TCP_CALCULATE_EFF_SEND_MSS */

#if LWIP_CALLBACK_API
err_t tcp_recv_null(void *arg, struct tcp_pcb *pcb, struct pbuf *p, err_t err);
#endif /* LWIP_CALLBACK_API */

#if TCP_DEBUG || TCP_INPUT_DEBUG || TCP_OUTPUT_DEBUG
void tcp_debug_print(struct tcp_hdr *tcphdr);
void tcp_debug_print_flags(u8_t flags);
void tcp_debug_print_state(enum tcp_state s);
void tcp_debug_print_pcbs(void);
s16_t tcp_pcbs_sane(void);
#else
#  define tcp_debug_print(tcphdr)
#  define tcp_debug_print_flags(flags)
#  define tcp_debug_print_state(s)
#  define tcp_debug_print_pcbs()
#  define tcp_pcbs_sane() 1
#endif /* TCP_DEBUG */

/** External function (implemented in timers.c), called when TCP detects
 * that a timer is needed (i.e. active- or time-wait-pcb found). */
void tcp_timer_needed(void);

void tcp_netif_ip_addr_changed(const ip_addr_t* old_addr, const ip_addr_t* new_addr);

#if TCP_QUEUE_OOSEQ
void tcp_free_ooseq(struct tcp_pcb *pcb);
#endif

#if LWIP_TCP_PCB_NUM_EXT_ARGS
err_t tcp_ext_arg_invoke_callbacks_passive_open(struct tcp_pcb_listen *lpcb, struct tcp_pcb *cpcb);
#endif

#if LWIP_TCP && LWIP_IPV6
void  tcp_unlock_accept(const ip6_addr_t *ipaddr);
#endif

#ifdef __cplusplus
}
#endif

#endif /* LWIP_TCP */

#endif /* LWIP_HDR_TCP_PRIV_H */
