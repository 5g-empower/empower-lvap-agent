/*
 * =====================================================================================
 *
 *       Filename:  gtp.h
 *
 *    Description:  GPRS Tunnelling Protocol version 1 header.
 *
 *        Created:  11/16/2017 10:13:46 AM
 *       Compiler:  gcc
 *
 *         Author:  Tejas Subramanya
 *        Company:  FBK CREATE-NET
 *
 * =====================================================================================
 */

#ifndef GTP_H
#define GTP_H

/*
 * Definitions of GTP headers based on 3GPP standards
 */

#define GTP_VERSION 0x20       /* Version Number */
#define GTP_PROTOCOL_TYPE 0x10 /* Protocol Type */
#define GTP_EHP 0x04           /* Extension Header Present */
#define GTP_SNP 0x02           /* Sequence Number Present */
#define GTP_NPDUNP 0x01        /* N-PDU Number Present */

#define GPDU_TYPE 255
#define GTP_HEADER_LEN 8

struct click_gtp {
  uint8_t flags; /* As seen above */
  uint8_t message_type; /* GTP message type */
  uint16_t message_length; /* Message length */
  uint32_t teid; /* Tunnel Endpoint ID */
};

#endif
