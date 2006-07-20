/*========================================================================
 * command.c  --  CLI command routines for Tsunami client.
 *
 * This contains routines for processing the commands of the Tsunami
 * file transfer CLI client.
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

#include <pthread.h>      /* for the pthreads library              */
#include <stdlib.h>       /* for *alloc() and free()               */
#include <string.h>       /* for standard string routines          */
#include <sys/socket.h>   /* for the BSD socket library            */
#include <sys/time.h>     /* for gettimeofday()                    */
#include <time.h>         /* for time()                            */
#include <unistd.h>       /* for standard Unix system calls        */

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
    char          *secret;

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
    secret = getpass("Password: ");
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
    u_char         *datagram;           /* the buffer for incoming blocks                 */
    struct timeval  repeat_time;        /* the time we last sent our retransmission list  */
    u_int32_t       this_block;         /* the block number for the block just received   */
    u_int16_t       this_type;          /* the block type for the block just received     */
    u_char          complete_flag = 0;  /* set to 1 when it's time to stop                */
    u_int32_t       iteration     = 0;  /* the number of iterations through the main loop */
    u_int64_t       delta;              /* generic holder of elapsed times                */
    u_int32_t       block;              /* generic holder of a block number               */
    ttp_transfer_t *xfer          = &(session->transfer);
    retransmit_t   *rexmit        = &(session->transfer.retransmit);
    int             status;
    pthread_t       disk_thread_id;

    /* make sure that we have a remote file name */
    if (command->count < 2)
	return warn("Invalid command syntax (use 'help get' for details)");

    /* make sure that we have an open session */
    if (session->server == NULL)
	return warn("Not connected to a Tsunami server");

    /* reinitialize the transfer data */
    memset(xfer, 0, sizeof(*xfer));

    /* store the remote filename */
    xfer->remote_filename = command->text[1];

    /* calculate the local filename */
    if (command->count >= 3)
	xfer->local_filename = command->text[2];
    else {
	xfer->local_filename = strrchr(command->text[1], '/');
	if (xfer->local_filename == NULL)
	    xfer->local_filename = command->text[1];
	else
	    ++(xfer->local_filename);
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

    /* allocate the received bitfield */
    xfer->received = (u_char *) calloc(xfer->block_count / 8 + 2, sizeof(u_char));
    if (xfer->received == NULL)
	error("Could not allocate received-data bitfield");

    /* allocate the ring buffer */
    xfer->ring_buffer = ring_create(session);

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

	/* reserve a datagram slot */
	datagram = ring_reserve(xfer->ring_buffer);

	/* try to receive a datagram */
	status = recvfrom(xfer->udp_fd, datagram, 6 + session->parameter->block_size, 0, (struct sockaddr *) &session->server_address, &session->server_address_length);
	if (status < 0) {
	    perror(NULL);
	    warn("UDP data transmission error");
	}

	/* confirm our slot reservation */
	if (ring_confirm(xfer->ring_buffer) < 0) {
	    warn("Error in accepting block");
	    goto abort;
	}

	/* retrieve the block number and block type */
	this_block = ntohl(*((u_int32_t *) datagram));
	this_type  = ntohs(*((u_int16_t *) (datagram + 4)));

	/* queue any retransmits we need */
	if (this_block > xfer->next_block)
	    for (block = xfer->next_block; block < this_block; ++block)
		if (ttp_request_retransmit(session, block) < 0) {
		    warn("Retransmission request failed");
		    goto abort;
		}

	/* if this is the last block */
	if ((this_block >= xfer->block_count) || (this_type == 'X')) {

	    /* prepare to stop if we're done */
	    if (xfer->blocks_left == 0) //rexmit->index_max == 0)
		complete_flag = 1;
	    else
		ttp_repeat_retransmit(session);
	}

	/* repeat our requests if it's time */
	if (!(iteration % 50)) {

	    /* if it's been at least a second */
	    if ((get_usec_since(&(xfer->stats.this_time)) > UPDATE_PERIOD) || (xfer->stats.total_blocks == 0)) {

		/* repeat our requests */
		if (ttp_repeat_retransmit(session) < 0) {
		    warn("Repeat of retransmission requests failed");
		    goto abort;
		}

		/* show our current statistics */
		ttp_update_stats(session);

		//gettimeofday(&repeat_time, NULL);
	    }
	}

	/* if this is an orignal, we expect to receive the successor to this block next */
	if (this_type == 'O') {
	    xfer->stats.total_blocks = this_block;
	    xfer->next_block         = this_block + 1;
	}
    }

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
    printf("The ANML web site can be found at:  http://www.anml.iu.edu/\n\n");

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
	  else if (!strcasecmp(command->text[1], "buffer"))     parameter->udp_buffer    = atol(command->text[2]);
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
         if(l>1 && (cpy[l-1])=='m') { 
            multiplier = 1000000; cpy[l-1]='\0';  
         } else if(l>1 && cpy[l-1]=='g') { 
            multiplier = 1000000000; cpy[l-1]='\0';   
         }
         parameter->target_rate   = multiplier * atol(cpy); 
   }
	  else if (!strcasecmp(command->text[1], "error"))      parameter->error_rate    = atof(command->text[2]) * 1000.0;
	  else if (!strcasecmp(command->text[1], "slowdown"))   parse_fraction(command->text[2], &parameter->slower_num, &parameter->slower_den);
	  else if (!strcasecmp(command->text[1], "speedup"))    parse_fraction(command->text[2], &parameter->faster_num, &parameter->faster_den);
	  else if (!strcasecmp(command->text[1], "history"))    parameter->history       = atoi(command->text[2]);
    }

    /* report on current values */
    if (do_all || !strcasecmp(command->text[1], "server"))     printf("server = %s\n",      parameter->server_name);
    if (do_all || !strcasecmp(command->text[1], "port"))       printf("port = %u\n",        parameter->server_port);
    if (do_all || !strcasecmp(command->text[1], "buffer"))     printf("buffer = %u\n",      parameter->udp_buffer);
    if (do_all || !strcasecmp(command->text[1], "verbose"))    printf("verbose = %s\n",     parameter->verbose_yn    ? "yes" : "no");
    if (do_all || !strcasecmp(command->text[1], "transcript")) printf("transcript = %s\n",  parameter->transcript_yn ? "yes" : "no");
    if (do_all || !strcasecmp(command->text[1], "ip"))         printf("ip = %s\n",          parameter->ipv6_yn       ? "v6"  : "v4");
    if (do_all || !strcasecmp(command->text[1], "output"))     printf("output = %s\n",      (parameter->output_mode == SCREEN_MODE) ? "screen" : "line");
    if (do_all || !strcasecmp(command->text[1], "rate"))       printf("rate = %u\n",        parameter->target_rate);
    if (do_all || !strcasecmp(command->text[1], "error"))      printf("error = %0.2f%%\n",  parameter->error_rate / 1000.0);
    if (do_all || !strcasecmp(command->text[1], "slowdown"))   printf("slowdown = %d/%d\n", parameter->slower_num, parameter->slower_den);
    if (do_all || !strcasecmp(command->text[1], "speedup"))    printf("speedup = %d/%d\n",  parameter->faster_num, parameter->faster_den);
    if (do_all || !strcasecmp(command->text[1], "history"))    printf("history = %d%%\n",   parameter->history);
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
 * Revision 1.1  2006/07/20 09:21:19  jwagnerhki
 * Initial revision
 *
 * Revision 1.2  2006/07/11 07:24:12  jwagnerhki
 * copied "set rate [nnn][m|g]" format from normal tsunami
 *
 * Revision 1.1  2006/07/10 12:35:11  jwagnerhki
 * added to trunk
 *
 */
