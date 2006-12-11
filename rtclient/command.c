/*========================================================================
 * command.c  --  CLI command routines for Tsunami client.
 *
 * This contains routines for processing the commands of the Tsunami
 * file transfer CLI client.
 *
 * Written by Mark Meiss (mmeiss@indiana.edu).
 * Copyright (C) 2002 The Trustees of Indiana University.
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
 * NO WARRANTIES AS TO CAPABILITIES OR ACCURACY ARE MADE. INDIANA
 * UNIVERSITY GIVES NO WARRANTIES AND MAKES NO REPRESENTATION THAT
 * SOFTWARE IS FREE OF INFRINGEMENT OF THIRD PARTY PATENT, COPYRIGHT,
 * OR OTHER PROPRIETARY RIGHTS.  INDIANA UNIVERSITY MAKES NO
 * WARRANTIES THAT SOFTWARE IS FREE FROM "BUGS", "VIRUSES", "TROJAN
 * HORSES", "TRAP DOORS", "WORMS", OR OTHER HARMFUL CODE.  LICENSEE
 * ASSUMES THE ENTIRE RISK AS TO THE PERFORMANCE OF SOFTWARE AND/OR
 * ASSOCIATED MATERIALS, AND TO THE PERFORMANCE AND VALIDITY OF
 * INFORMATION GENERATED USING SOFTWARE.
 *========================================================================*/

#include <pthread.h>      /* for the pthreads library              */
#include <stdlib.h>       /* for *alloc() and free()               */
#include <string.h>       /* for standard string routines          */
#include <sys/socket.h>   /* for the BSD socket library            */
#include <sys/time.h>     /* for gettimeofday()                    */
#include <time.h>         /* for time()                            */
#include <unistd.h>       /* for standard Unix system calls        */
#include <ctype.h>        /* for toupper() etc                     */

#include "client.h"


/*------------------------------------------------------------------------
 * Prototypes for module-scope routines.
 *------------------------------------------------------------------------*/

void *disk_thread   (void *arg);
int   parse_fraction(const char *fraction, u_int16_t *num, u_int16_t *den);


/*------------------------------------------------------------------------
 * int command_close(command_t *command, ttp_session_t *session);
 *
 * Closes the given open Tsunami control session if it's active, thus
 * making it invalid for further use.  Returns 0 on success and non-zero
 * on error.
 *------------------------------------------------------------------------*/
int command_close(command_t *command, ttp_session_t *session)
{
    /* make sure we have an open connection */
    if (session->server == NULL)
	return warn("Tsunami session was not active");

    /* otherwise, go ahead and close it */
    fclose(session->server);
    session->server = NULL;
    if (session->parameter->verbose_yn)
	printf("Connection closed.\n\n");
    return 0;
}


/*------------------------------------------------------------------------
 * ttp_session_t *command_connect(command_t *command,
 *                                ttp_parameter_t *parameter);
 *
 * Opens a new Tsunami control session to the server specified in the
 * command or in the given set of default parameters.  This involves
 * prompting the user to enter the shared secret.  On success, we return
 * a pointer to the new TTP session object.  On failure, we return NULL.
 *
 * Note that the default host and port stored in the parameter object
 * are updated if they were specified in the command itself.
 *------------------------------------------------------------------------*/
ttp_session_t *command_connect(command_t *command, ttp_parameter_t *parameter)
{
    int            server_fd;
    ttp_session_t *session;
    #define PASS_KEY "kitten"
    char           secret[] = PASS_KEY;

    /* if we were given a new host, store that information */
    if (command->count > 1) {
	if (parameter->server_name != NULL)
	    free(parameter->server_name);
	parameter->server_name = strdup(command->text[1]);
	if (parameter->server_name == NULL) {
	    warn("Could not update server name");
	    return NULL;
	}
    }

    /* if we were given a port, store that information */
    if (command->count > 2)
	parameter->server_port = atoi(command->text[2]);

    /* allocate a new session */
    session = (ttp_session_t *) calloc(1, sizeof(ttp_session_t));
    if (session == NULL)
	error("Could not allocate session object");
    session->parameter = parameter;

    /* obtain our client socket */
    server_fd = create_tcp_socket(session, parameter->server_name, parameter->server_port);
    if (server_fd < 0) {
	sprintf(g_error, "Could not connect to %s:%d.", parameter->server_name, parameter->server_port);
	warn(g_error);
	return NULL;
    }

    /* convert our server connection into a stream */
    session->server = fdopen(server_fd, "w+");
    if (session->server == NULL) {
	warn("Could not convert control channel into a stream");
	close(server_fd);
	free(session);
	return NULL;
    }

    /* negotiate the connection parameters */
    if (ttp_negotiate(session) < 0) {
	warn("Protocol negotiation failed");
	fclose(session->server);
	free(session);
	return NULL;
    }

    /* get the shared secret from the user */
    /*    secret = getpass("Password: "); */
    strcpy(secret, PASS_KEY); /* write back the passwd erased by possible earlier ttp_auth's() */
    if (secret == NULL)
	error("Could not read shared secret");

    /* authenticate to the server */
    if (ttp_authenticate(session, secret) < 0) {
	warn("Authentication failed");
	fclose(session->server);
	free(session);
	return NULL;
    }

    /* we succeeded */
    if (session->parameter->verbose_yn)
	printf("Connected.\n\n");
    return session;
}


/*------------------------------------------------------------------------
 * int command_get(command_t *command, ttp_session_t *session);
 *
 * Tries to initiate a file transfer for the remote file given in the
 * command.  If the user did not supply a local filename, we derive it
 * from the remote filename.  Returns 0 on a successful transfer and
 * nonzero on an error condition.
 *------------------------------------------------------------------------*/
int command_get(command_t *command, ttp_session_t *session)
{
    u_char         *datagram;           /* the buffer (in ring) for incoming blocks       */
    u_char         *local_datagram;     /* the local temp space for incoming block        */
    struct timeval  repeat_time;        /* the time we last sent our retransmission list  */
    u_int32_t       this_block;         /* the block number for the block just received   */
    u_int16_t       this_type;          /* the block type for the block just received     */
    u_char          complete_flag = 0;  /* set to 1 when it's time to stop                */
    u_int32_t       iteration     = 0;  /* the number of iterations through the main loop */
    u_int64_t       delta;              /* generic holder of elapsed times                */
    u_int32_t       block;              /* generic holder of a block number               */
    u_int64_t       udp_errors;         /* receive errors reported by OS protocol stack   */
    ttp_transfer_t *xfer          = &(session->transfer);
    retransmit_t   *rexmit        = &(session->transfer.retransmit);
    int             status;
    pthread_t       disk_thread_id;

    /* The following variables will be used only in multiple file transfer
     * session they are used to recieve the file names and other parameters
     */
    int             multimode = 0;
    char           *file_names;
    char          **all_fileNames = NULL;
    int             f_counter, f_total;

    /* this struct wil hold the RTT time */
    struct timeval ping_s, ping_e;
    long wait_u_sec = 1;

    /* make sure that we have a remote file name */
    if (command->count < 2)
	return warn("Invalid command syntax (use 'help get' for details)");

    /* make sure that we have an open session */
    if (session == NULL || session->server == NULL)
	return warn("Not connected to a Tsunami server");

    /* reinitialize the transfer data */
    memset(xfer, 0, sizeof(*xfer));

    /* if the client asking for multiple files to be transfered */
    f_counter = 0; 
    f_total = 0;
    if(!strcmp("*",command->text[1])) {
       int   bytes_read = 0, totalSize = 0, totalNumber = 0;
       char  file_size[10];
       char  file_number[10];

       multimode = 1;
       printf("Requesting all available files\n");

       /* Try to calculate the RTT from client to server */
       gettimeofday(&(ping_s), NULL);
       status = fprintf(session->server, "%s\n", command->text[1]);

       status = fread(file_size, sizeof(char), 10, session->server);
       gettimeofday(&(ping_e),NULL);

       status = fread(file_number, sizeof(char), 10, session->server);

       fprintf(session->server, "%s", "got size");

       if ((status <= 0) || fflush(session->server))
          return warn("Could not request file");

       /* see if the request was successful */
       if (status < 1)
          return warn("Could not read response to file request");

       /* total sent data size */
       sscanf(file_size, "%d", &totalSize);
       /* total number of file */
       sscanf(file_size, "%d", &totalNumber);

       printf("\nGot file size: %d\n",totalSize);
       if (totalSize<=0) {
          return warn("Server advertised no files to get");
       }
       if((file_names=malloc(totalSize*sizeof(char)))==NULL)
          error("Could not allocate memory\n");

       fread(file_names, sizeof(char), totalSize, session->server);
       fprintf(session->server,"%s", "got list");

       if((all_fileNames=calloc(totalNumber, sizeof(char*)))==NULL)
          error("could not allocate memory\n");

       /* this while loop separates the file names */
       while(bytes_read<totalSize) {
           if((all_fileNames[f_counter]=malloc((strlen(file_names+bytes_read)+1)*sizeof(char)))==NULL) {
              error("could not allocate memory\n");
           } else {
              strcpy(all_fileNames[f_counter],file_names+bytes_read);
           }

           bytes_read += strlen(all_fileNames[f_counter++])+1;
           printf("%s ", all_fileNames[f_counter -1]);
       }
       f_total = f_counter;
       f_counter = 0;

       printf("file size %d. Getting %d files.\n", bytes_read, f_total);
       /*free(file_names);*/
    }

    /* calculate and convert RTT to u_sec */
    wait_u_sec = (ping_e.tv_sec - ping_s.tv_sec)*1000000+(ping_e.tv_usec-ping_s.tv_usec);
    /* add a 10% safety margin */
    wait_u_sec = wait_u_sec + ((int)(wait_u_sec* 0.1));

    do /*---loop for single and multi file request---*/
    {

    /* reset some vars for current run */
    complete_flag = 0;
    iteration = 0;
    udp_errors = get_udp_in_errors();

    /* store the remote filename */
    if(!multimode)
       xfer->remote_filename = command->text[1];
    else
       xfer->remote_filename = all_fileNames[f_counter];

    /* calculate the local filename */
    if(!multimode) {
       if (command->count >= 3) {
          xfer->local_filename = command->text[2];
       } else {
          xfer->local_filename = strrchr(command->text[1], '/');
          if (xfer->local_filename == NULL)
             xfer->local_filename = command->text[1];
          else
             ++(xfer->local_filename);
       }
    } else {
       xfer->local_filename = all_fileNames[f_counter];
       printf("GET *: now requesting file '%s'\n", xfer->local_filename);
    }

    /* negotiate the file request with the server */
    if (ttp_open_transfer(session, xfer->remote_filename, xfer->local_filename) < 0)
	return warn("File transfer request failed");

    /* create the UDP data socket */
    if (ttp_open_port(session) < 0)
	return warn("Creation of data socket failed");

    /* allocate the retransmission table */
    rexmit->table = (u_int32_t *) calloc(DEFAULT_TABLE_SIZE, sizeof(u_int32_t));
    if (rexmit->table == NULL)
	error("Could not allocate retransmission table");

    //rexmit->rx_table = (u_int32_t *) calloc(xfer->block_count+1, sizeof(u_int32_t));
    //if (rexmit->rx_table == NULL)
    //    error("Could not allocate retransmission table");

    /* allocate the received bitfield */
    xfer->received = (u_char *) calloc(xfer->block_count / 8 + 2, sizeof(u_char));
    if (xfer->received == NULL)
	error("Could not allocate received-data bitfield");

    /* allocate the ring buffer */
    xfer->ring_buffer = ring_create(session);

    /* allocate the faster local buffer */
    local_datagram = (u_char *) calloc(6 + session->parameter->block_size, sizeof(u_char));
    if (local_datagram == NULL)
        error("Could not allocate fast local datagram buffer in command_get()");

    /* start up the disk I/O thread */
    status = pthread_create(&disk_thread_id, NULL, disk_thread, session);
    if (status != 0)
	error("Could not create I/O thread");

    /* Finish initializing the retransmission object */
    rexmit->table_size = DEFAULT_TABLE_SIZE;
    rexmit->index_max  = 0;

    /* we start by expecting block #1 */
    xfer->next_block = 1;

   /*---------------------------
   * START TIMING
   *---------------------------*/

   memset(&xfer->stats, 0, sizeof(xfer->stats));
   gettimeofday(&repeat_time,              NULL);
   gettimeofday(&(xfer->stats.start_time), NULL);
   gettimeofday(&(xfer->stats.this_time),  NULL);
   if (session->parameter->transcript_yn)
      xscript_data_start(session, &(xfer->stats.start_time));

   /* until we break out of the transfer */
   while (!complete_flag) {
   
      /* we got here again */
      ++iteration;
      
      /* try to receive a datagram */
      status = recvfrom(xfer->udp_fd, local_datagram, 6 + session->parameter->block_size, 0, 
                        (struct sockaddr *) &session->server_address, &session->server_address_length);
      if (status < 0) {
          warn("UDP data transmission error");
          printf("Apparently frozen transfer, trying to do retransmit request\n");
          if (ttp_repeat_retransmit(session) < 0) {  /* repeat our requests */
             warn("Repeat of retransmission requests failed");
             goto abort;
          }
      }

      /* retrieve the block number and block type */
      this_block = ntohl(*((u_int32_t *) local_datagram));
      this_type  = ntohs(*((u_int16_t *) (local_datagram + 4)));

      if (!(session->transfer.received[this_block / 8] & (1 << (this_block % 8))) || this_type == TS_BLOCK_TERMINATE) 
      {
   
          /* reserve a datagram slot */
          datagram = ring_reserve(xfer->ring_buffer);
    
          /* copy data from local buffer into ring buffer */
          memcpy(datagram, local_datagram, 6 + session->parameter->block_size);
        
          /* confirm our slot reservation */
          if (ring_confirm(xfer->ring_buffer) < 0) {
              warn("Error in accepting block");
              goto abort;
          }
    
          /* queue any retransmits we need */
          if ((this_block > xfer->next_block) && (session->parameter->no_retransmit == 0)) {
              for (block = xfer->next_block; block < this_block; ++block) {
                  if (ttp_request_retransmit(session, block) < 0) {
                      warn("Retransmission request failed");
                      goto abort;
                  }
              }
          }
   
          /* if this is the last block */
          if ((this_block >= xfer->block_count) || (this_type == TS_BLOCK_TERMINATE)) {
   
            /* prepare to stop if we're done */
            if (xfer->blocks_left == 0 || session->parameter->no_retransmit == 1) //rexmit->index_max == 0)
              complete_flag = 1;
            else
              ttp_repeat_retransmit(session);
          }

          /* if this is an orignal, we expect to receive the successor to this block next */
          if (this_type == TS_BLOCK_ORIGINAL) {
              xfer->stats.total_blocks = this_block;
              xfer->next_block         = this_block + 1;
          }

      }//if(not a duplicate block)
   
      /* repeat our requests if it's time */
      if (!(iteration % 50)) {
   
          /* if it's been at least a second */
          /* TODO: ensure equal sample spacing! several UPDATE_PERIOD's may fit into 50 iterations */
          if ((get_usec_since(&(xfer->stats.this_time)) > UPDATE_PERIOD) || (xfer->stats.total_blocks == 0)) {
   
            /* repeat our retransmission requests */
            if (0 == session->parameter->no_retransmit) {
                if (ttp_repeat_retransmit(session) < 0) {
                    warn("Repeat of retransmission requests failed");
                    goto abort;
                }
            }
         
            /* show our current statistics */
            ttp_update_stats(session);
         
            //gettimeofday(&repeat_time, NULL);
         }
      }
   
   } /* Transfer of the file completes here*/

    /* add a stop block to the ring buffer */
    datagram = ring_reserve(xfer->ring_buffer);
    *((u_int32_t *) datagram) = 0;
    if (ring_confirm(xfer->ring_buffer) < 0)
	warn("Error in terminating disk thread");

    /* wait for the disk thread to die */
    if (pthread_join(disk_thread_id, NULL) < 0)
	warn("Disk thread terminated with error");

    /*---------------------------
     * STOP TIMING
     *---------------------------*/

    /* tell the server to quit transmitting */
    if (ttp_request_stop(session) < 0) {
	warn("Could not request end of transfer");
	goto abort;
    }

    /* display the final results */
    delta = get_usec_since(&(xfer->stats.start_time));
    printf("Mbits of data transmitted = %0.2f\n", xfer->file_size * 8.0 / (1024.0 * 1024.0));
    printf("Duration in seconds       = %0.2f\n", delta / 1000000.0);
    printf("THROUGHPUT (Mbps)         = %0.2f\n", xfer->file_size * 8.0 / delta);
    printf("PC UDP packet rx errors   = %lld\n",  get_udp_in_errors() - udp_errors);
    printf("\n");

    /* update the transcript */
    if (session->parameter->transcript_yn) {
	gettimeofday(&repeat_time, NULL);
	xscript_data_stop(session, &repeat_time);
	xscript_close(session, delta);
    }

    /* close our open files */
    close(xfer->udp_fd);
    fclose(xfer->file);    xfer->file     = NULL;

    /* deallocate memory */
    ring_destroy(xfer->ring_buffer);
    free(rexmit->table);   rexmit->table  = NULL;
    free(xfer->received);  xfer->received = NULL;
    free(local_datagram);  local_datagram = NULL;

    } while(++f_counter<f_total);

    /* we succeeded */
    return 0;

 abort:
    fprintf(stderr, "Transfer not successful.  (WARNING: You may need to reconnect.)\n\n");
    close(xfer->udp_fd);
    ring_destroy(xfer->ring_buffer);
    if (xfer->file     != NULL) { fclose(xfer->file);    xfer->file     = NULL; }
    if (rexmit->table  != NULL) { free(rexmit->table);   rexmit->table  = NULL; }
    if (xfer->received != NULL) { free(xfer->received);  xfer->received = NULL; }
    return -1;
}


/*------------------------------------------------------------------------
 * int command_help(command_t *command, ttp_session_t *session);
 *
 * Offers help on either the list of available commands or a particular
 * command.  Returns 0 on success and nonzero on failure, which is not
 * possible, but it normalizes the API.
 *------------------------------------------------------------------------*/
int command_help(command_t *command, ttp_session_t *session)
{
    /* if no command was supplied */
    if (command->count < 2) {
	printf("Help is available for the following commands:\n\n");
	printf("    close    connect    get    help    quit    set\n\n");
	printf("Use 'help <command>' for help on an individual command.\n\n");

    /* handle the CLOSE command */
    } else if (!strcasecmp(command->text[1], "close")) {
	printf("Usage: close\n\n");
	printf("Closes the current connection to a remote Tsunami server.\n\n");

    /* handle the CONNECT command */
    } else if (!strcasecmp(command->text[1], "connect")) {
	printf("Usage: connect\n");
	printf("       connect <remote-host>\n");
	printf("       connect <remote-host> <remote-port>\n\n");
	printf("Opens a connection to a remote Tsunami server.  If the host and port\n");
	printf("are not specified, default values are used.  (Use the 'set' command to\n");
	printf("modify these values.)\n\n");
	printf("After connecting, you will be prompted to enter a shared secret for\n");
	printf("authentication.\n\n");

    /* handle the GET command */
    } else if (!strcasecmp(command->text[1], "get")) {
	printf("Usage: get <remote-file>\n");
	printf("       get <remote-file> <local-file>\n\n");
	printf("Attempts to retrieve the remote file with the given name using the\n");
	printf("Tsunami file transfer protocol.  If the local filename is not\n");
	printf("specified, the final part of the remote filename (after the last path\n");
	printf("separator) will be used.\n\n");

    /* handle the HELP command */
    } else if (!strcasecmp(command->text[1], "help")) {
	printf("Come on.  You know what that command does.\n\n");

    /* handle the QUIT command */
    } else if (!strcasecmp(command->text[1], "quit")) {
	printf("Usage: quit\n\n");
	printf("Closes any open connection to a remote Tsunami server and exits the\n");
	printf("Tsunami client.\n\n");

    /* handle the SET command */
    } else if (!strcasecmp(command->text[1], "set")) {
	printf("Usage: set\n");
	printf("       set <field>\n");
	printf("       set <field> <value>\n\n");
	printf("Sets one of the defaults to the given value.  If the value is omitted,\n");
	printf("the current value of the field is returned.  If the field is also\n");
	printf("omitted, the current values of all defaults are returned.\n\n");

    /* apologize for our ignorance */
    } else {
	printf("'%s' is not a recognized command.\n", command->text[1]);
	printf("Use 'help' for a list of commands.\n\n");
    }

    /* we succeeded */
    return 0;
}


/*------------------------------------------------------------------------
 * int command_quit(command_t *command, ttp_session_t *session);
 *
 * Closes the open connection (if there is one) and aborts the operation
 * of the Tsunami client.  For API uniformity, we pretend to return
 * something, but we don't.
 *------------------------------------------------------------------------*/
int command_quit(command_t *command, ttp_session_t *session)
{
    /* close the connection if there is one */
    if (session && (session->server != NULL))
	fclose(session->server);

    /* wave good-bye */
    printf("Thank you for using Tsunami.\n");
    printf("The ANML web site can be found at:    http://www.anml.iu.edu/\n");
    printf("The SourceForge site can be found at: http://tsunami-udp.sf.net/\n\n");

    /* and quit */
    exit(1);
    return 0;
}


/*------------------------------------------------------------------------
 * int command_set(command_t *command, ttp_parameter_t *parameter);
 *
 * Sets a particular parameter to the given value, or simply reports
 * on the current value of one or more fields.  Returns 0 on success
 * and nonzero on failure.
 *------------------------------------------------------------------------*/
int command_set(command_t *command, ttp_parameter_t *parameter)
{
    int do_all = (command->count == 1);

    /* handle actual set operations first */
    if (command->count == 3) {
	if (!strcasecmp(command->text[1], "server")) {
	    if (parameter->server_name != NULL)
		free(parameter->server_name);
	    parameter->server_name = strdup(command->text[2]);
	    if (parameter->server_name == NULL)
		error("Could not update server name");
	} else if (!strcasecmp(command->text[1], "port"))       parameter->server_port   = atoi(command->text[2]);
	  else if (!strcasecmp(command->text[1], "udpport"))    parameter->client_port   = atoi(command->text[2]);
	  else if (!strcasecmp(command->text[1], "buffer"))     parameter->udp_buffer    = atol(command->text[2]);
	  else if (!strcasecmp(command->text[1], "blocksize"))     parameter->block_size    = atol(command->text[2]);
	  else if (!strcasecmp(command->text[1], "verbose"))    parameter->verbose_yn    = (strcmp(command->text[2], "yes") == 0);
	  else if (!strcasecmp(command->text[1], "transcript")) parameter->transcript_yn = (strcmp(command->text[2], "yes") == 0);
	  else if (!strcasecmp(command->text[1], "ip"))         parameter->ipv6_yn       = (strcmp(command->text[2], "v6")  == 0);
	  else if (!strcasecmp(command->text[1], "output"))     parameter->output_mode   = (strcmp(command->text[2], "screen") ? LINE_MODE : SCREEN_MODE);
	  else if (!strcasecmp(command->text[1], "rate"))       { 
         long multiplier = 1;
         char *cmd = command->text[2];
         char cpy[256];
         int l = strlen(cmd);
         strcpy(cpy, cmd);
         if(l>1 && (toupper(cpy[l-1]))=='M') { 
            multiplier = 1000000; cpy[l-1]='\0';  
         } else if(l>1 && toupper(cpy[l-1])=='G') { 
            multiplier = 1000000000; cpy[l-1]='\0';   
         }
         parameter->target_rate   = multiplier * atol(cpy); 
   }
	  else if (!strcasecmp(command->text[1], "error"))        parameter->error_rate    = atof(command->text[2]) * 1000.0;
	  else if (!strcasecmp(command->text[1], "slowdown"))     parse_fraction(command->text[2], &parameter->slower_num, &parameter->slower_den);
	  else if (!strcasecmp(command->text[1], "speedup"))      parse_fraction(command->text[2], &parameter->faster_num, &parameter->faster_den);
	  else if (!strcasecmp(command->text[1], "history"))      parameter->history       = atoi(command->text[2]);
	  else if (!strcasecmp(command->text[1], "noretransmit")) parameter->no_retransmit = (strcmp(command->text[2], "yes") == 0);
    }

    /* report on current values */
    if (do_all || !strcasecmp(command->text[1], "server"))     printf("server = %s\n",      parameter->server_name);
    if (do_all || !strcasecmp(command->text[1], "port"))       printf("port = %u\n",        parameter->server_port);
    if (do_all || !strcasecmp(command->text[1], "udpport"))    printf("udpport = %u\n",     parameter->client_port);
    if (do_all || !strcasecmp(command->text[1], "buffer"))     printf("buffer = %u\n",      parameter->udp_buffer);
    if (do_all || !strcasecmp(command->text[1], "blocksize"))  printf("blocksize = %u\n",   parameter->block_size);
    if (do_all || !strcasecmp(command->text[1], "verbose"))    printf("verbose = %s\n",     parameter->verbose_yn    ? "yes" : "no");
    if (do_all || !strcasecmp(command->text[1], "transcript")) printf("transcript = %s\n",  parameter->transcript_yn ? "yes" : "no");
    if (do_all || !strcasecmp(command->text[1], "ip"))         printf("ip = %s\n",          parameter->ipv6_yn       ? "v6"  : "v4");
    if (do_all || !strcasecmp(command->text[1], "output"))     printf("output = %s\n",      (parameter->output_mode == SCREEN_MODE) ? "screen" : "line");
    if (do_all || !strcasecmp(command->text[1], "rate"))       printf("rate = %u\n",        parameter->target_rate);
    if (do_all || !strcasecmp(command->text[1], "error"))      printf("error = %0.2f%%\n",  parameter->error_rate / 1000.0);
    if (do_all || !strcasecmp(command->text[1], "slowdown"))   printf("slowdown = %d/%d\n", parameter->slower_num, parameter->slower_den);
    if (do_all || !strcasecmp(command->text[1], "speedup"))    printf("speedup = %d/%d\n",  parameter->faster_num, parameter->faster_den);
    if (do_all || !strcasecmp(command->text[1], "history"))    printf("history = %d%%\n",   parameter->history);
    if (do_all || !strcasecmp(command->text[1], "noretransmit")) printf("noretransmit = %s\n", (1==parameter->no_retransmit) ? "yes" : "no");
    printf("\n");

    /* we succeeded */
    return 0;
}


/*------------------------------------------------------------------------
 * void *disk_thread(void *arg);
 *
 * This is the thread that takes care of saved received blocks to disk.
 * It runs until the network thread sends it a datagram with a block
 * number of 0.  The return value has no meaning.
 *------------------------------------------------------------------------*/
void *disk_thread(void *arg)
{
    ttp_session_t *session = (ttp_session_t *) arg;
    u_char        *datagram;
    int            status;
    u_int32_t      block_index;
    u_int16_t      block_type;

    /* while the world is turning */
    while (1) {

	/* get another block */
	datagram    = ring_peek(session->transfer.ring_buffer);
	block_index = ntohl(*((u_int32_t *) datagram));
	block_type  = ntohs(*((u_int16_t *) (datagram + 4)));

	/* quit if we got the mythical 0 block */
	if (block_index == 0) {
	    printf("!!!!\n");
	    return NULL;
	}

	/* save it to disk */
	status = accept_block(session, block_index, datagram + 6);
	if (status < 0) {
	    warn("Block accept failed");
	    return NULL;
	}

	/* pop the block */
	ring_pop(session->transfer.ring_buffer);
    }
}


/*------------------------------------------------------------------------
 * int parse_fraction(const char *fraction,
 *                    u_int16_t *num, u_int16_t *den);
 *
 * Given a string in the form "nnn/ddd", saves the numerator and
 * denominator in the given 16-bit locations.  Returns 0 on success and
 * nonzero on error.
 *------------------------------------------------------------------------*/
int parse_fraction(const char *fraction, u_int16_t *num, u_int16_t *den)
{
    const char *slash;

    /* get the location of the '/' */
    slash = strchr(fraction, '/');
    if (slash == NULL)
	return warn("Value is not a fraction");

    /* store the two parts of the value */
    *num = atoi(fraction);
    *den = atoi(slash + 1);

    /* we succeeded */
    return 0;
}


/*========================================================================
 * $Log: command.c,v $
 * Revision 1.8  2006/12/11 11:11:56  jwagnerhki
 * show operating system UDP rx error stats in summary
 *
 * Revision 1.7  2006/12/05 15:24:50  jwagnerhki
 * now noretransmit code in client only, merged rt client code
 *
 * Revision 1.6  2006/11/10 11:34:45  jwagnerhki
 * merge from normal client, indentation, transmit termination fix
 *
 * Revision 1.10  2006/11/10 11:32:42  jwagnerhki
 * indentation, transmit termination fix
 *
 * Revision 1.9  2006/11/06 07:42:44  jwagnerhki
 * changed defaults, added 144MiB bigbufsize mention
 *
 * Revision 1.8  2006/10/28 19:29:15  jwagnerhki
 * jamil GET* merge, insertionsort disabled by default again
 *
 * Revision 1.7  2006/10/28 17:00:12  jwagnerhki
 * block type defines
 *
 * Revision 1.6  2006/10/24 21:21:36  jwagnerhki
 * fixed client loosing password
 *
 * Revision 1.5  2006/10/11 08:36:50  jwagnerhki
 * added URL to SF project page for the byebye msg
 *
 * Revision 1.4  2006/09/07 13:56:57  jwagnerhki
 * udp socket reusable, udp port selectable in client
 *
 * Revision 1.3  2006/08/08 06:04:28  jwagnerhki
 * included ctype.h
 *
 * Revision 1.2  2006/07/21 07:48:56  jwagnerhki
 * set rate now also with mM gG
 *
 * Revision 1.1.1.1  2006/07/20 09:21:18  jwagnerhki
 * reimport
 *
 * Revision 1.1  2006/07/10 12:26:51  jwagnerhki
 * deleted unnecessary files
 *
 */
