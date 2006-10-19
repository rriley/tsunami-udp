/* Copyright (C) 2001--2002 Ari Mujunen, Ari.Mujunen@hut.fi

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software Foundation,
   Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.  */


/* Ensure 64-bit file support regardless of '-D_LARGEFILE64_SOURCE=1'
   in Makefile. */
#define _LARGEFILE64_SOURCE 1  /* Large File Support (LFS) '*64()' functions. */ 
#define _FILE_OFFSET_BITS 64  /* Automatic '*()' --> '*64()' replacement. */ 

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#include <sys/types.h>  /* open() */
#include <sys/stat.h>  /* open() */
#include <fcntl.h>  /* open() */

#include <sys/time.h>  /* gettimeofday() */
#include <unistd.h>  /* gettimeofday(), usleep() */

#include <string.h>  /* strstr() */

#include <sys/ipc.h>  /* for shared memory */
#include <sys/shm.h>

#include <sys/ioctl.h>
#include "vsib_ioctl.h"

#include "server.h"



/* Upcoming wrshm.h: */
#define fourCharLong(a,b,c,d) ( ((long)a)<<24 | ((long)b)<<16 | ((long)c)<<8 | d )

/* Default values for mode */
int vsib_mode = 2;                     /* 2: low 16 bit mode */
int vsib_mode_gigabit = 0;             /* 0: no gigabit mode */
int vsib_mode_embed_1pps_markers = 1;  /* 1: embed markers */
int vsib_mode_skip_samples = 0;        /* 0: do not skip samples */

typedef struct sSh {
  int relSeekBlocks;
} tSh, *ptSh;


  int readMode;
  int vsib_started = 0;
  int usleeps;

  /* Shared memory unique identifier (key) and the returned reference id. */
  key_t shKey;
  int shId = -1;
  ptSh sh;


/* Depending on readMode either standard input or output. */
int vsib_fileno;

/* A protected (error-checking) ioctl() for VSIB driver. */
static void
vsib_ioctl(
  unsigned int mode,
  unsigned long arg
) {
  if (ioctl(vsib_fileno,
            mode,
            arg)
  ) {
    char *which;
    char err[255];
    
    if (vsib_fileno == STDOUT_FILENO) {
      which = "rd";
    } else {
      which = "wr";
    }
    snprintf(err, sizeof(err), "%s: ioctl(vsib_fileno, 0x%04x,...)", which, mode);
    perror(err);
    fprintf(stderr, "%s: standard I/O is not an VSIB board\n", which);
    exit(EXIT_FAILURE);
  }
}  /* vsib_ioctl */



double
tim(void) {
  struct timeval tv;
  double t;

  assert( gettimeofday(&tv, NULL) == 0 );
  t = (double)tv.tv_sec + (double)tv.tv_usec/1000000.0;
  return t;
}  /* tim */

void
start_vsib(ttp_session_t *session)
{
  ttp_transfer_t  *xfer  = &session->transfer;
 
  /* Find out the file number */
 
  vsib_fileno = fileno(xfer->vsib);
 


  /* Create and initialize 'wr<-->control' shared memory. */
  shKey = fourCharLong('v','s','i','b');
  assert( (shId = shmget(shKey, sizeof(tSh), IPC_CREAT | 0777)) != -1 );
  assert( (sh = (ptSh)shmat(shId, NULL, 0)) != (void *)-1 );
  sh->relSeekBlocks = 0;

  vsib_ioctl(VSIB_SET_MODE,
             (VSIB_MODE_MODE(vsib_mode)
              | VSIB_MODE_RUN
              | (vsib_mode_gigabit ? VSIB_MODE_GIGABIT : 0)
              | (vsib_mode_embed_1pps_markers ? VSIB_MODE_EMBED_1PPS_MARKERS : 0)
              | (vsib_mode_skip_samples & 0x0000ffff))
            );
}  /* start_VSIB */



void read_vsib_block(char *memblk, int blksize)

  {
      size_t nread;

    /* Write (or read) one block. */
    /* xxx: need to add '-1' error checks to VSIB read()/write() calls */

      /* Read a block from VSIB; if not enough, sleep a little. */
      nread = read (vsib_fileno, memblk, blksize);
      while (nread < blksize) {
        /* Not one full block in buffer, wait. */
        usleep(1000);  /* a small amount, probably ends up to be 10--20msec */
	/*        usleeps++; */
        nread += read (vsib_fileno, memblk+nread, blksize-nread);
      }  /* while not at least one full block in VSIB DMA ring buffer */

  }  /* read_vsib_block */



void stop_vsib(ttp_session_t *session)

  {

  /* Stop the board, first DMA, and when the last descriptor */
  /* has been transferred, then write stop to board command register. */
  vsib_ioctl(VSIB_DELAYED_STOP_DMA, 0);
  {
    unsigned long b;

    vsib_ioctl(VSIB_IS_DMA_DONE, (unsigned long)&b);
    while (!b) {
      fprintf(stderr, "Waiting for last DMA descriptor (sl=%d)\n",
              usleeps);
      usleep(100000);
      /*      usleeps++; */
      vsib_ioctl(VSIB_IS_DMA_DONE, (unsigned long)&b);
    }
  }



  vsib_ioctl(VSIB_SET_MODE, VSIB_MODE_STOP);


  /* Remove shared memory to mark that 'wr/rd' is no more running. */
  if ((shId != -1) && (sh != (ptSh)-1) && (sh != NULL)) {
    //assert( shmctl(shId, IPC_RMID, NULL) == 0 );
    //assert( shmdt(sh) == 0 );
    if( shmctl(shId, IPC_RMID, NULL) != 0 ) {
       fprintf(stderr, "Shared memory mark remove shmctl() returned non-0\n");
    } else {
       if( shmdt(sh) != 0 ) {
          fprintf(stderr, "Shared memory mark remove shmdt() returned non-0\n");
       }
    }
  }  // if shared memory was allocated

		    /*  return(); */
}  /* main */

