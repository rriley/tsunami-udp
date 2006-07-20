/*========================================================================
 * server.h  --  Global header for Tsunami server.
 *
 * This contains global definitions for the Tsunami file transfer client.
 *
 * Written by Mark Meiss (mmeiss@indiana.edu).
 * Copyright © 2002 The Trustees of Indiana University.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1) All redistributions of source code must retain the above
 *    copyright notice, the list of authors in the original source
 *    code, this list of conditions and the disclaimer listed in this
 *    license;
 *
 * 2) All redistributions in binary form must reproduce the above
 *    copyright notice, this list of conditions and the disclaimer
 *    listed in this license in the documentation and/or other
 *    materials provided with the distribution;
 *
 * 3) Any documentation included with all redistributions must include
 *    the following acknowledgement:
 *
 *      "This product includes software developed by Indiana
 *      University`s Advanced Network Management Lab. For further
 *      information, contact Steven Wallace at 812-855-0960."
 *
 *    Alternatively, this acknowledgment may appear in the software
 *    itself, and wherever such third-party acknowledgments normally
 *    appear.
 *
 * 4) The name "tsunami" shall not be used to endorse or promote
 *    products derived from this software without prior written
 *    permission from Indiana University.  For written permission,
 *    please contact Steven Wallace at 812-855-0960.
 *
 * 5) Products derived from this software may not be called "tsunami",
 *    nor may "tsunami" appear in their name, without prior written
 *    permission of Indiana University.
 *
 * Indiana University provides no reassurances that the source code
 * provided does not infringe the patent or any other intellectual
 * property rights of any other entity.  Indiana University disclaims
 * any liability to any recipient for claims brought by any other
 * entity based on infringement of intellectual property rights or
 * otherwise.
 *
 * LICENSEE UNDERSTANDS THAT SOFTWARE IS PROVIDED "AS IS" FOR WHICH
 * NO WARRANTIES AS TO CAPABILITIES OR ACCURACY ARE MADE. INDIANA
 * UNIVERSITY GIVES NO WARRANTIES AND MAKES NO REPRESENTATION THAT
 * SOFTWARE IS FREE OF INFRINGEMENT OF THIRD PARTY PATENT, COPYRIGHT,
 * OR OTHER PROPRIETARY RIGHTS.  INDIANA UNIVERSITY MAKES NO
 * WARRANTIES THAT SOFTWARE IS FREE FROM "BUGS", "VIRUSES", "TROJAN
 * HORSES", "TRAP DOORS", "WORMS", OR OTHER HARMFUL CODE.  LICENSEE
 * ASSUMES THE ENTIRE RISK AS TO THE PERFORMANCE OF SOFTWARE AND/OR
 * ASSOCIATED MATERIALS, AND TO THE PERFORMANCE AND VALIDITY OF
 * INFORMATION GENERATED USING SOFTWARE.
 *========================================================================*/

#ifndef __SERVER_H
#define __SERVER_H

#include <netinet/in.h>  /* for struct sockaddr_in, etc.                 */
#include <stdio.h>       /* for NULL, FILE *, etc.                       */
#include <sys/types.h>   /* for various system data types                */

#include "tsunami.h"     /* for Tsunami function prototypes and the like */


/*------------------------------------------------------------------------
 * Global constants.
 *------------------------------------------------------------------------*/

extern const u_int32_t  DEFAULT_BLOCK_SIZE;     /* default size of a single file block     */
extern const u_char    *DEFAULT_SECRET;         /* default shared secret                   */
extern const u_int16_t  DEFAULT_TCP_PORT;       /* default TCP port to listen on           */
extern const u_int32_t  DEFAULT_UDP_BUFFER;     /* default size of the UDP transmit buffer */
extern const u_char     DEFAULT_VERBOSE_YN;     /* the default verbosity setting           */
extern const u_char     DEFAULT_TRANSCRIPT_YN;  /* the default transcript setting          */
extern const u_char     DEFAULT_IPV6_YN;        /* the default IPv6 setting                */
extern const u_char     DEFAULT_NO_RETRANSMIT;  /* server-side setting, on default use retransmission */

#define MAX_FILENAME_LENGTH  1024               /* maximum length of a requested filename  */
#define RINGBUF_BLOCKS  1                       /* Size of ring buffer (disabled now) */


/*------------------------------------------------------------------------
 * Data structures.
 *------------------------------------------------------------------------*/

/* Tsunami transfer protocol parameters */
typedef struct {
    time_t              epoch;          /* the Unix epoch used to identify this run   */
    u_char              verbose_yn;     /* verbose mode (0=no, 1=yes)                 */
    u_char              transcript_yn;  /* transcript mode (0=no, 1=yes)              */
    u_char              ipv6_yn;        /* IPv6 mode (0=no, 1=yes)                    */
    u_int16_t           tcp_port;       /* TCP port number for listening on           */
    u_int32_t           udp_buffer;     /* size of the UDP send buffer in bytes       */
    const u_char       *secret;         /* the shared secret for users to prove       */
    u_int32_t           block_size;     /* the size of each block (in bytes)          */
    u_int64_t           file_size;      /* the total file size (in bytes)             */
    u_int32_t           block_count;    /* the total number of blocks in the file     */
    u_int32_t           target_rate;    /* the transfer rate that we're targetting    */
    u_int32_t           error_rate;     /* the threshhold error rate (in % x 1000)    */
    u_int32_t           ipd_time;       /* the inter-packet delay in usec             */
    u_int16_t           slower_num;     /* the numerator of the increase-IPD factor   */
    u_int16_t           slower_den;     /* the denominator of the increase-IPD factor */
    u_int16_t           faster_num;     /* the numerator of the decrease-IPD factor   */
    u_int16_t           faster_den;     /* the denominator of the decrease-IPD factor */
    u_char              no_retransmit;  /* for testing, actual retransmission can be disabled */
    char                *ringbuf;       /* Pointer to ring buffer start               */
    u_int16_t           fileout;        /* Do we store the data to file?              */
} ttp_parameter_t;

/* state of a transfer */
typedef struct {
    ttp_parameter_t    *parameter;    /* the TTP protocol parameters                */
    char               *filename;     /* the path to the file                       */
    FILE               *file;         /* the open file that we're transmitting      */
    FILE               *vsib;         /* the vsib file number                       */
    FILE               *transcript;   /* the open transcript file for statistics    */
    int                 udp_fd;       /* the file descriptor of our UDP socket      */
    struct sockaddr    *udp_address;  /* the destination for our file data          */
    socklen_t           udp_length;   /* the length of the UDP socket address       */
    u_int32_t           ipd_current;  /* the inter-packet delay currently in usec   */
    u_int32_t           block;        /* the current block that we're up to         */
} ttp_transfer_t;

/* state of a Tsunami session as a whole */
typedef struct {
    ttp_parameter_t    *parameter;    /* the TTP protocol parameters                */
    ttp_transfer_t      transfer;     /* the current transfer in progress, if any   */
    int                 client_fd;    /* the connection to the remote client        */
} ttp_session_t;


/*------------------------------------------------------------------------
 * Function prototypes.
 *------------------------------------------------------------------------*/

/* config.c */
void reset_server         (ttp_parameter_t *parameter);

/* io.c */
int  build_datagram       (ttp_session_t *session, u_int32_t block_index, u_int16_t block_type, u_char *datagram);

/* vsibctl.c */
void start_vsib (ttp_session_t *sessi);
void stop_vsib (ttp_session_t *session);
void read_vsib_block(char *memblk, int blksize);

/* log.c */
/* void log                  (FILE *log_file, const char *format, ...); */

/* network.c */
int  create_tcp_socket    (ttp_parameter_t *parameter);
int  create_udp_socket    (ttp_parameter_t *parameter);

/* protocol.c */
int  ttp_accept_retransmit(ttp_session_t *session, retransmission_t *retransmission, u_char *datagram);
int  ttp_authenticate     (ttp_session_t *session, const u_char *secret);
int  ttp_negotiate        (ttp_session_t *session);
int  ttp_open_port        (ttp_session_t *session);
int  ttp_open_transfer    (ttp_session_t *session);

/* transcript.c */
void xscript_close        (ttp_session_t *session, u_int64_t delta);
void xscript_data_log     (ttp_session_t *session, const char *logline);
void xscript_data_start   (ttp_session_t *session, const struct timeval *epoch);
void xscript_data_stop    (ttp_session_t *session, const struct timeval *epoch);
void xscript_open         (ttp_session_t *session);

#endif /* __SERVER_H */


/*========================================================================
 * $Log: server.h,v $
 * Revision 1.2  2006/07/20 12:23:45  jwagnerhki
 * header file merge
 *
 * Revision 1.1.1.1  2006/07/20 09:21:20  jwagnerhki
 * reimport
 *
 * Revision 1.1  2006/07/10 12:37:21  jwagnerhki
 * added to trunk
 *
 */


