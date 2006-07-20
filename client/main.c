/*========================================================================
 * main.c  --  Command-line interface for Tsunami client.
 *
 * This is the main module for the Tsunami file transfer CLI client.
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

#include <ctype.h>        /* for the isspace() routine             */
#include <stdlib.h>       /* for *alloc() and free()               */
#include <string.h>       /* for standard string routines          */
#include <unistd.h>       /* for standard Unix system calls        */

#include "tsunami.h"
#include "client.h"


/*------------------------------------------------------------------------
 * Function prototypes (module scope).
 *------------------------------------------------------------------------*/

void parse_command(command_t *command, char *buffer);


/*------------------------------------------------------------------------
 * MAIN PROGRAM
 *------------------------------------------------------------------------*/
int main(int argc, char *argv[])
{
    command_t        command;                           /* the current command being processed */
    u_char           command_text[MAX_COMMAND_LENGTH];  /* the raw text of the command         */
    ttp_session_t   *session = NULL;
    ttp_parameter_t  parameter;

    /* reset the client */
    memset(&parameter, 0, sizeof(parameter));
    reset_client(&parameter);

    /* while the command loop is still running */
    while (1) {

	/* present the prompt */
	fprintf(stdout, "tsunami> ");
	fflush(stdout);

	/* retrieve the user's command */
	if (fgets(command_text, MAX_COMMAND_LENGTH, stdin) == NULL)
	    error("Could not read command input");

	/* parse the command */
	parse_command(&command, command_text);

	/* make sure we have at least one word */
	if (command.count == 0)
	    continue;

	/* dispatch on the command type */
	     if (!strcasecmp(command.text[0], "close"))             command_close  (&command, session);
	else if (!strcasecmp(command.text[0], "connect")) session = command_connect(&command, &parameter);
	else if (!strcasecmp(command.text[0], "get"))               command_get    (&command, session);
	else if (!strcasecmp(command.text[0], "help"))              command_help   (&command, session);
	else if (!strcasecmp(command.text[0], "quit"))              command_quit   (&command, session);
	else if (!strcasecmp(command.text[0], "set"))               command_set    (&command, &parameter);
	else
	    fprintf(stderr, "Unrecognized command: '%s'.  Use 'HELP' for help.\n\n", command.text[0]);
    }

    /* if we're here, we shouldn't be */
    return 1;
}


/*------------------------------------------------------------------------
 * void parse_command(command_t *command, char *buffer);
 *
 * Given a buffer containing the text of a command, replaces the
 * whitespace with null terminators and fills the given command
 * structure with pointers to the words in the command.
 *------------------------------------------------------------------------*/
void parse_command(command_t *command, char *buffer)
{
    /* reset the count */
    command->count = 0;

    /* skip past initial whitespace */
    while (isspace(*buffer) && *buffer)
	++buffer;

    /* while we have command text left and not too many words */
    while ((command->count < MAX_COMMAND_WORDS) && *buffer) {

	/* save the start of the word */
	command->text[command->count++] = buffer;

	/* advance to the next whitespace (or the end) */
	while (*buffer && !isspace(*buffer))
	    ++buffer;

	/* convert the whitespace to terminators */
	while (*buffer && isspace(*buffer))
	    *(buffer++) = '\0';
    }
}


/*========================================================================
 * $Log: main.c,v $
 * Revision 1.1  2006/07/20 09:21:18  jwagnerhki
 * Initial revision
 *
 * Revision 1.1  2006/07/10 12:26:51  jwagnerhki
 * deleted unnecessary files
 *
 */
