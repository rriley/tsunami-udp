/*========================================================================
 * md5.c  --  MD5 message digest routines.
 *
 * This is a from-scratch implementation of the MD5 Message-Digest
 * Algorithm.  This code is NOT based on the reference implementation
 * found in RFC 1321 and is thus not subject to the licensing
 * restrictions of RSA Data Security, Inc.
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

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>

#include "md5.h"


/*------------------------------------------------------------------------
 * Rotate an unsigned integer left some number of bits.
 *------------------------------------------------------------------------*/
#define rotate(X, bits) (((X) << (bits)) | ((X) >> (32 - (bits))))


/*------------------------------------------------------------------------
 * The package-wide T array.
 *------------------------------------------------------------------------*/

u_int32_t T[64] = { 0xd76aa478, 0xe8c7b756, 0x242070db, 0xc1bdceee,    /* 01 - 04 */
		    0xf57c0faf, 0x4787c62a, 0xa8304613, 0xfd469501,    /* 05 - 08 */
		    0x698098d8, 0x8b44f7af, 0xffff5bb1, 0x895cd7be,    /* 09 - 12 */
		    0x6b901122, 0xfd987193, 0xa679438e, 0x49b40821,    /* 13 - 16 */

		    0xf61e2562, 0xc040b340, 0x265e5a51, 0xe9b6c7aa,    /* 17 - 20 */
		    0xd62f105d, 0x02441453, 0xd8a1e681, 0xe7d3fbc8,    /* 21 - 24 */
		    0x21e1cde6, 0xc33707d6, 0xf4d50d87, 0x455a14ed,    /* 25 - 28 */
		    0xa9e3e905, 0xfcefa3f8, 0x676f02d9, 0x8d2a4c8a,    /* 29 - 32 */

		    0xfffa3942, 0x8771f681, 0x6d9d6122, 0xfde5380c,    /* 33 - 36 */
		    0xa4beea44, 0x4bdecfa9, 0xf6bb4b60, 0xbebfbc70,    /* 37 - 40 */
		    0x289b7ec6, 0xeaa127fa, 0xd4ef3085, 0x04881d05,    /* 41 - 44 */
		    0xd9d4d039, 0xe6db99e5, 0x1fa27cf8, 0xc4ac5665,    /* 45 - 48 */

		    0xf4292244, 0x432aff97, 0xab9423a7, 0xfc93a039,    /* 49 - 52 */
		    0x655b59c3, 0x8f0ccc92, 0xffeff47d, 0x85845dd1,    /* 53 - 56 */
		    0x6fa87e4f, 0xfe2ce6e0, 0xa3014314, 0x4e0811a1,    /* 57 - 60 */
		    0xf7537e82, 0xbd3af235, 0x2ad7d2bb, 0xeb86d391 };  /* 61 - 64 */

u_char pad[64] = { 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };

u_char md5_table[64][2] = { { 0, 7}, { 1,12}, { 2,17}, { 3,22}, { 4, 7}, { 5,12}, { 6,17}, { 7,22},
	 		    { 8, 7}, { 9,12}, {10,17}, {11,22}, {12, 7}, {13,12}, {14,17}, {15,22},
			    { 1, 5}, { 6, 9}, {11,14}, { 0,20}, { 5, 5}, {10, 9}, {15,14}, { 4,20},
			    { 9, 5}, {14, 9}, { 3,14}, { 8,20}, {13, 5}, { 2, 9}, { 7,14}, {12,20},
			    { 5, 4}, { 8,11}, {11,16}, {14,23}, { 1, 4}, { 4,11}, { 7,16}, {10,23},
			    {13, 4}, { 0,11}, { 3,16}, { 6,23}, { 9, 4}, {12,11}, {15,16}, { 2,23},
			    { 0, 6}, { 7,10}, {14,15}, { 5,21}, {12, 6}, { 3,10}, {10,15}, { 1,21},
			    { 8, 6}, {15,10}, { 6,15}, {13,21}, { 4, 6}, {11,10}, { 2,15}, { 9,21} };

u_int32_t md5_F(u_int32_t x, u_int32_t y, u_int32_t z) { return (x & y) | (~x & z); }
u_int32_t md5_G(u_int32_t x, u_int32_t y, u_int32_t z) { return (x & z) | (y & ~z); }
u_int32_t md5_H(u_int32_t x, u_int32_t y, u_int32_t z) { return x ^ y ^ z;          }
u_int32_t md5_I(u_int32_t x, u_int32_t y, u_int32_t z) { return y ^ (x | ~z);       }

u_int32_t (*md5_dispatch[4])(u_int32_t, u_int32_t, u_int32_t) = { md5_F, md5_G, md5_H, md5_I };


/*------------------------------------------------------------------------
 * void md5_digest(u_char *buffer, size_t size, u_char *digest);
 *
 * Given a message of the given size, computes the MD5 digest of the
 * messages and stores it in the given pre-allocated 16-byte buffer.
 *
 * Note that this code assumes a little-endian machine and a message
 * size of at most 2**29!
 *------------------------------------------------------------------------*/
void md5_digest(u_char *buffer, size_t size, u_char *digest)
{
    u_int32_t X[16], state[4], tempState[4], func, sum;
    int       i, blocks, j;

    /* initialize the state array */
    state[0] = 0x67452301;  state[1] = 0xefcdab89;
    state[2] = 0x98badcfe;  state[3] = 0x10325476;

    /* calculate the number of blocks */
    blocks = size / 64;
    if ((size % 64) > 56) ++blocks;

    /* for each block */
    for (i = 0; i <= blocks; ++i) {

	/* if it's not the last block or next-to-last block */
	if (i < blocks - 1) {

	    /* copy the block into X */
	    memcpy((u_char *) X, buffer + (64 * i), 64);

	/* if it's the next-to-last block */
	} else if (i == blocks - 1) {

	    /* copy most or all of the block into X */
	    if ((64 * (i + 1)) > size) {
		memcpy((u_char *) X, buffer + (64 * i), size % 64);
		memcpy(((u_char *) X) + (size % 64), pad, 64 - (size % 64));
	    } else {
		memcpy((u_char *) X, buffer + (64 * i), 64);
	    }

	/* otherwise, copy just the needed bytes */
	} else {

	    /* copy part of the message and the padding block into X */
	    if ((64 * i) > size) {
		memset((u_char *) X, 0x00, 64);
	    } else {
		memcpy((u_char *) X, buffer + (64 * i), size % 64);
		memcpy(((u_char *) X) + (size % 64), pad, 64 - (size % 64));
	    }

	    /* put the message size into X */
	    X[14] = size * 8;
	    X[15] = 0x00000000;
	}

	/* save the current state */
	memcpy((u_char *) tempState, (u_char *) state, 16);

	/* do the computation */
	for (j = 0; j < 64; ++j) {
	    func = md5_dispatch[j/16](state[(4 - j%4 + 1) % 4], state[(4 - j%4 + 2) % 4], state[(4 - j%4 + 3) % 4]);
	    sum  = state[(4-j%4)%4] + func + X[md5_table[j][0]] + T[j];
	    state[(4-j%4)%4] = state[(4-j%4+1)%4] + rotate(sum, md5_table[j][1]);
	}

	/* do the necessary additions */
	for (j = 0; j < 4; ++j)
	    state[j] += tempState[j];
    }

    /* copy the state into the digest */
    memcpy(digest, (u_char *) state, 16);
}


/*------------------------------------------------------------------------
 * void md5_fprint_digest(FILE *file, u_char *digest);
 *
 * Prints the given digest on the given open file handle.
 *------------------------------------------------------------------------*/
void md5_fprint_digest(FILE *file, u_char *digest)
{
    int i;

    /* print the bytes one-by-one */
    for (i = 0; i < 16; ++i)
	fprintf(file, "%02x", digest[i]);
}


/*------------------------------------------------------------------------
 * void md5_sprint_digest(char *buffer, u_char *digest);
 *
 * Prints the given digest in hex form into the given string buffer,
 * which must be at least 33 characters in size.
 *------------------------------------------------------------------------*/
void md5_sprint_digest(char *buffer, u_char *digest)
{
    /* print the digest */
    sprintf(buffer, "%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x",
	    digest[0], digest[1], digest[2],  digest[3],  digest[4],  digest[5],  digest[6],  digest[7],
	    digest[8], digest[9], digest[10], digest[11], digest[12], digest[13], digest[14], digest[15]);
}


/*========================================================================
 * $Log: md5.c,v $
 * Revision 1.1  2006/07/20 09:21:19  jwagnerhki
 * Initial revision
 *
 * Revision 1.1  2006/07/10 12:27:29  jwagnerhki
 * added to trunk
 *
 */
