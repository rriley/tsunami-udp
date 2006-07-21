/*========================================================================
 * config.c  --  Tuneable parameters for Tsunami client.
 *
 * This contains tuneable parameters for the Tsunami file transfer client.
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

#include <stdlib.h>  /* for malloc(), free(), etc. */
#include <string.h>  /* for memset()               */

#include "client.h"


/*------------------------------------------------------------------------
 * Global constants.
 *------------------------------------------------------------------------*/

const u_int32_t  DEFAULT_BLOCK_SIZE    = 32768;        /* default size of a single file block          */
const int        DEFAULT_TABLE_SIZE    = 2048;         /* initial size of the retransmission table     */
const char      *DEFAULT_SERVER_NAME   = "localhost";  /* default name of the remote server            */
const u_int16_t  DEFAULT_SERVER_PORT   = TS_TCP_PORT;  /* default TCP port of the remote server        */
const u_int16_t  DEFAULT_CLIENT_PORT   = TS_UDP_PORT;  /* default UDP port of the client               */
const u_int32_t  DEFAULT_UDP_BUFFER    = 20000000;     /* default size of the UDP receive buffer       */
const u_char     DEFAULT_VERBOSE_YN    = 1;            /* the default verbosity setting                */
const u_char     DEFAULT_TRANSCRIPT_YN = 0;            /* the default transcript setting               */
const u_char     DEFAULT_IPV6_YN       = 0;            /* the default IPv6 setting                     */
const u_char     DEFAULT_OUTPUT_MODE   = LINE_MODE;    /* the default output mode (SCREEN or LINE)     */
const u_int32_t  DEFAULT_TARGET_RATE   = 650000000;    /* the default target rate (in bps)             */
const u_int32_t  DEFAULT_ERROR_RATE    = 7500;         /* the default threshhold error rate (% x 1000) */
const u_int16_t  DEFAULT_SLOWER_NUM    = 25;           /* default numerator in the slowdown factor     */
const u_int16_t  DEFAULT_SLOWER_DEN    = 24;           /* default denominator in the slowdown factor   */
const u_int16_t  DEFAULT_FASTER_NUM    = 5;            /* default numerator in the speedup factor      */
const u_int16_t  DEFAULT_FASTER_DEN    = 6;            /* default denominator in the speedup factor    */
const u_int16_t  DEFAULT_HISTORY       = 25;           /* default percentage of history in rates       */
const u_char     DEFAULT_NO_RETRANSMIT = 0;            /* on default use retransmission                */

const int        MAX_COMMAND_LENGTH    = 1024;         /* maximum length of a single command           */


/*------------------------------------------------------------------------
 * void reset_client(ttp_parameter_t *parameter);
 *
 * Resets the set of default parameters to their default values.
 *------------------------------------------------------------------------*/
void reset_client(ttp_parameter_t *parameter)
{
    /* free the previous hostname if necessary */
    if (parameter->server_name != NULL)
	free(parameter->server_name);

    /* zero out the memory structure */
    memset(parameter, 0, sizeof(*parameter));

    /* fill the fields with their defaults */
    parameter->block_size    = DEFAULT_BLOCK_SIZE;
    parameter->server_name   = strdup(DEFAULT_SERVER_NAME);
    parameter->server_port   = DEFAULT_SERVER_PORT;
    parameter->client_port   = DEFAULT_CLIENT_PORT;
    parameter->udp_buffer    = DEFAULT_UDP_BUFFER;
    parameter->verbose_yn    = DEFAULT_VERBOSE_YN;
    parameter->transcript_yn = DEFAULT_TRANSCRIPT_YN;
    parameter->ipv6_yn       = DEFAULT_IPV6_YN;
    parameter->output_mode   = DEFAULT_OUTPUT_MODE;
    parameter->target_rate   = DEFAULT_TARGET_RATE;
    parameter->error_rate    = DEFAULT_ERROR_RATE;
    parameter->slower_num    = DEFAULT_SLOWER_NUM;
    parameter->slower_den    = DEFAULT_SLOWER_DEN;
    parameter->faster_num    = DEFAULT_FASTER_NUM;
    parameter->faster_den    = DEFAULT_FASTER_DEN;
    parameter->history       = DEFAULT_HISTORY;
    parameter->no_retransmit = DEFAULT_NO_RETRANSMIT;

    /* make sure the strdup() worked */
    if (parameter->server_name == NULL)
      error("Could not reset default server name");
}


/*========================================================================
 * $Log: config.c,v $
 * Revision 1.2  2006/07/21 07:55:35  jwagnerhki
 * new UDP port define
 *
 * Revision 1.1.1.1  2006/07/20 09:21:19  jwagnerhki
 * reimport
 *
 * Revision 1.1  2006/07/10 12:35:11  jwagnerhki
 * added to trunk
 *
 */
