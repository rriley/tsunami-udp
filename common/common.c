/*========================================================================
 * common.c  --  Shared routines between the Tsunami client and server.
 *
 * This module contains routines of use to both the Tsunami client and
 * the Tsunami server.
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

#include <fcntl.h>       /* for the open() constant definitions   */
#include <netinet/in.h>  /* for htons(), et al.                   */
#include <string.h>      /* for standard string handling routines */
#include <time.h>        /* for time-handling functions           */
#include <unistd.h>      /* for standard Unix system calls        */

#include "md5.h"         /* for MD5 message digest support        */
#include "tsunami.h"     /* for Tsunami function prototypes, etc. */


/*------------------------------------------------------------------------
 * Definitions of global constants.
 *------------------------------------------------------------------------*/

const u_int32_t PROTOCOL_REVISION  = 0x20021202;

const u_int16_t REQUEST_RETRANSMIT = 0;
const u_int16_t REQUEST_RESTART    = 1;
const u_int16_t REQUEST_STOP       = 2;
const u_int16_t REQUEST_ERROR_RATE = 3;


/*------------------------------------------------------------------------
 * int get_random_data(u_char *buffer, size_t bytes);
 *
 * Attempts to read the given number of bytes of random data from
 * /dev/random and into the given buffer.  Returns 0 on success and -1
 * on failure.
 *------------------------------------------------------------------------*/
int get_random_data(u_char *buffer, size_t bytes)
{
    int random_fd;

    /* try to open /dev/random */
    if ((random_fd = open("/dev/random", O_RDONLY)) < 0)
	return -1;

    /* obtain the appropriate amount of data */
    if (read(random_fd, buffer, bytes) < 0)
	return -1;

    /* close /dev/random and return */
    return (close(random_fd) < 0) ? -1 : 0;
}


/*------------------------------------------------------------------------
 * u_int64_t get_usec_since(struct timeval *old_time);
 *
 * Returns the number of microseconds that have elapsed between the
 * given time and the time of this call.
 *------------------------------------------------------------------------*/
u_int64_t get_usec_since(struct timeval *old_time)
{
    struct timeval now;
    u_int64_t      result = 0;

    /* get the current time */
    gettimeofday(&now, NULL);

    /* return the elapsed time */
    while (now.tv_sec > old_time->tv_sec) {
	result += 1000000;
	--now.tv_sec;
    }
    return result + (now.tv_usec - old_time->tv_usec);

    /*------------------------------------------------------------
     * We used to calculate it like this, but the above is
     * usually a bit faster in the general case.  Note that we
     * have BIG problems if old_time is in the future, however...
     *------------------------------------------------------------*/
    /* return 1000000LL * (now.tv_sec - old_time->tv_sec) + now.tv_usec - old_time->tv_usec; */
}


/*------------------------------------------------------------------------
 * u_int64_t htonll(u_int64_t value);
 *
 * Converts the given 64-bit value in host byte order to network byte
 * order and returns it.
 *------------------------------------------------------------------------*/
u_int64_t htonll(u_int64_t value)
{
    static int necessary = -1;

    /* if we don't know if this is necessary, find out */
    if (necessary == -1)
	necessary = (5 != htons(5));

    /* perform the conversion if necessary */
    if (necessary)
	return (((u_int64_t) htonl(value & 0x00000000ffffffffLL)) << 32) | ((u_int64_t) htonl(value >> 32));
    else
	return value;
}


/*------------------------------------------------------------------------
 * char *make_transcript_filename(char *buffer, time_t epoch,
 *                                const char *extension);
 *
 * Prepares a filename for a Tsunami transcript.  The buffer must be
 * large enough to hold a string of the form "YYYY-MM-DD-HH-MM-SS",
 * plus the provided extension.  A pointer to the buffer is returned.
 *------------------------------------------------------------------------*/
char *make_transcript_filename(char *buffer, time_t epoch, const char *extension)
{
    struct tm gmt;

    /* build the time structure */
    gmtime_r(&epoch, &gmt);

    /* construct the filename */
    sprintf(buffer, "%04d-%02d-%02d-%02d-%02d-%02d.%s",
	    gmt.tm_year + 1900, gmt.tm_mon + 1, gmt.tm_mday,
	    gmt.tm_hour, gmt.tm_min, gmt.tm_sec, extension);
    return buffer;
}


/*------------------------------------------------------------------------
 * u_int64_t ntohll(u_int64_t value);
 *
 * Converts the given 64-bit value in network byte order to host byte
 * order and returns it.
 *------------------------------------------------------------------------*/
u_int64_t ntohll(u_int64_t value)
{
    return htonll(value);
}


/*------------------------------------------------------------------------
 * u_char *prepare_proof(u_char *buffer, size_t bytes,
 *                       const u_char *secret, u_char *digest);
 *
 * Prepares an MD5 hash as proof that we know the same shared secret as
 * another system.  The null-terminated secret stored in [secret] is
 * repeatedly XORed over the data stored in [buffer] (which is of
 * length [bytes]).  The MD5 hash of the resulting buffer is then
 * stored in [digest].  The pointer to the digest is returned.
 *------------------------------------------------------------------------*/
u_char *prepare_proof(u_char *buffer, size_t bytes, const u_char *secret, u_char *digest)
{
    u_int32_t secret_length;  /* the length of the shared secret */
    u_int32_t offset;         /* iterator for the buffer         */

    /* get the length of the secret */
    secret_length = strlen(secret);

    /* prepare the buffer for the digest */
    for (offset = 0; offset < bytes; ++offset)
	buffer[offset] ^= secret[offset % secret_length];

    /* run MD5 and return the results */
    md5_digest(buffer, bytes, digest);
    return digest;
}


/*------------------------------------------------------------------------
 * int read_line(int fd, char *buffer, size_t buffer_length);
 *
 * Reads a newline-terminated line from the given file descriptor and
 * returns it, sans the newline character.  No buffering is done.
 * Returns 0 on success and a negative value on error.
 *------------------------------------------------------------------------*/
int read_line(int fd, char *buffer, size_t buffer_length)
{
    int buffer_offset = 0;

    /* read in the full line */
    do {
	if (read(fd, buffer + buffer_offset, 1) <= 0)
	    return warn("Could not read complete line of input");
    } while ((buffer[buffer_offset++] != '\n') && (buffer_offset < buffer_length));

    /* terminate the string and return */
    buffer[buffer_offset - 1] = '\0';
    return 0;
}


/*------------------------------------------------------------------------
 * void usleep_that_works(u_int64_t usec);
 *
 * Sleeps for the given amount of microseconds, with better accuracy
 * than that offered by the standard usleep() routine.  We do a real
 * sleep until we get into within 10,000 microseconds, then busy-wait
 * for the rest of the time.
 *------------------------------------------------------------------------*/
void usleep_that_works(u_int64_t usec)
{
    u_int64_t      sleep_time = (usec / 10000) * 10000;  /* the amount of time to sleep */
    struct timeval delay, now;

    /* get the current time */
    gettimeofday(&now, NULL);

    /* do the basic sleep */
    delay.tv_sec  = sleep_time / 1000000;
    delay.tv_usec = sleep_time % 1000000;
    select(0, NULL, NULL, NULL, &delay);

    /* and spin for the rest of the time */
    while (get_usec_since(&now) < usec);
}


/*========================================================================
 * $Log: common.c,v $
 * Revision 1.1  2006/07/20 09:21:19  jwagnerhki
 * Initial revision
 *
 * Revision 1.1  2006/07/10 12:27:29  jwagnerhki
 * added to trunk
 *
 */
