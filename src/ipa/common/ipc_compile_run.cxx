/*
   Copyright (C) 2010 PathScale Inc. All Rights Reserved.
*/
/*
 * Copyright (C) 2007 Pathscale, LLC.  All Rights Reserved.
 */

/*
 * Copyright (C) 2008-2009 Advanced Micro Devices, Inc.  All Rights Reserved.
 */

/*
 *  Copyright (C) 2006, 2007. QLogic Corporation. All Rights Reserved.
 */

/*
 * Copyright 2003, 2004, 2005, 2006 PathScale, Inc.  All Rights Reserved.
 */

/*

  Copyright (C) 2000, 2001 Silicon Graphics, Inc.  All Rights Reserved.

   Path64 is free software; you can redistribute it and/or modify it
   under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   Path64 is distributed in the hope that it will be useful, but WITHOUT
   ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
   or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public
   License for more details.

   You should have received a copy of the GNU General Public License
   along with Path64; see the file COPYING.  If not, write to the Free
   Software Foundation, 51 Franklin Street, Fifth Floor, Boston, MA
   02110-1301, USA.

   Special thanks goes to SGI for their continued support to open source

*/

#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "linker.h"                    
#include "cmplrs/rcodes.h"              // for RC_* codes

#include "ipc_compile_run.h"

// The functions below are based on the code from driver/run.c

static void ipc_execv(std::vector<std::string> arguments)
{
    char **argv = (char **)malloc((arguments.size() + 1) * sizeof(char*));
    int i = 0;
    for(i = 0; i < arguments.size() ; i++ ){
        argv[i] = (char*) malloc(arguments[i].length() * sizeof(char) + 1);
        strcpy(argv[i], arguments[i].c_str());
        if(ipa_opt[IPA_SHOW].flag){
            fprintf(stderr,"%s ", argv[i]);
        }
    }
    argv[i] = NULL;

    if(ipa_opt[IPA_SHOW].flag){
        fprintf(stderr,"\n");
    }

    execvp(argv[0], argv);
    fprintf(stderr, "cannot exec %s", arguments[0].c_str());
    exit (RC_SYSTEM_ERROR);
    /* NOTREACHED */
}

/* exec another program, putting result in output.
 * This is simple version of full run_phase. */
void
ipc_run_simple_program (std::vector<std::string> argv)
{
	int forkpid;
	int fdout;
	int waitpid;
	int waitstatus;
	int termsig;

        const char * name = argv[0].c_str();

	/* fork a process */
	forkpid = fork();
	if (forkpid == -1) {
		fprintf(stderr, "no more processes");
		exit (RC_SYSTEM_ERROR);
		/* NOTREACHED */
	}

	if (forkpid == 0) {
		/* child */

		ipc_execv(argv);
	} else {
		/* parent */
		int procid;	/* id of the /proc file */
		while ((waitpid = wait (&waitstatus)) != forkpid) {
			if (waitpid == -1) {
				fprintf(stderr, "bad return from wait");
				exit(RC_SYSTEM_ERROR);
				/* NOTREACHED */
			}
		}
		if (WIFSTOPPED(waitstatus)) {
			termsig = WSTOPSIG(waitstatus);
			fprintf(stderr, "STOPPED signal received from %s", name);
			exit(RC_SYSTEM_ERROR);
			/* NOTREACHED */
		} else if (WIFEXITED(waitstatus)) {
		        int status = WEXITSTATUS(waitstatus);
			if (status != RC_OKAY) {
				/* internal error */
				/* most internal errors use exit code of 1 */
				fprintf(stderr, "INTERNAL ERROR: %s returned non-zero status %d",
					name, status);
		         	exit(RC_SYSTEM_ERROR);
			} 
			return;
		} else if(WIFSIGNALED(waitstatus)){
			termsig = WTERMSIG(waitstatus);
			switch (termsig) {
			case SIGHUP:
			case SIGINT:
			case SIGQUIT:
			case SIGKILL:
			case SIGTERM:
			default:
				fprintf(stderr, "%s died due to signal %d",
					       name, termsig);
		         	exit(RC_SYSTEM_ERROR);
				break;
			}
			if (termsig == SIGKILL) {
				fprintf(stderr, "Probably caused by running out of swap space -- check %s", LOGFILE);
			}
			exit(RC_SYSTEM_ERROR);
		} else {
			/* cannot happen, I think! */
			fprintf(stderr, "INTERNAL ERROR: driver exec'ing is confused");
			return;
		}
	}
}

