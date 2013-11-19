/*
*------------------------------------------------------------------------------
* Copyright (C) 2008  Ilya G. Goldberg
* 
* This program is free software; you can redistribute it and/or
* modify it under the terms of the GNU General Public License
* as published by the Free Software Foundation; either version 2
* of the License, or (at your option) any later version.
* 
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
* 
* You should have received a copy of the GNU General Public License
* along with this program; if not, write to the Free Software
* Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
*------------------------------------------------------------------------------
*/

/*------------------------------------------------------------------------------
* Written by:	Ilya G. Goldberg <igg at cathilya dot org>
*------------------------------------------------------------------------------
*/

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif  /* HAVE_CONFIG_H */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/timex.h> 
#include <fcntl.h>
#include <assert.h>
#include <string.h>
#include <signal.h>
#include <math.h>
#include <errno.h>

void logMessage (FILE *log_fp, const char *template, ...);

#define DEBUGGING 1

#include "BBD9000mem.h"
#include "BBD9000cfg.h"


// GLOBALS
BBD9000mem *shmem;
FILE *evt_fp;
FILE *log_fp;




int main (int argc, const char **argv) {
const char *BBD9000MEMpath;
int shmem_fd;
int fifo_fd;
struct timeval wait_timeval, *wait_timeval_p;
int buf_siz=0;
char tmpstr[256];
time_t t_now;

	// This is a sub-process.
	// The shared memory segment path must be provided in the BBD9000_SHMEM environment variable
	if ( ! (BBD9000MEMpath = getenv ("BBD9000_SHMEM")) ) {
		fprintf (stderr,"%s: path to shared memory segment must be specified in the BBD9000_SHMEM environment variable\n", argv[0]);
		exit (-1);
	}

	/* chdir to the root of the filesystem to not block dismounting */
	chdir("/");

	/* open the shared memory object */
	shmem_fd = open(BBD9000MEMpath, O_RDWR|O_SYNC);
	if (shmem_fd < 0) {
		fprintf (stderr,"%s: Could not open shared memory segment %s: %s\n", argv[0], BBD9000MEMpath, strerror (errno));
		exit (-1);
	}

	/* mmap our shared memory */
	shmem = (BBD9000mem *) mmap(0, SHMEM_SIZE, PROT_READ|PROT_WRITE, MAP_SHARED, shmem_fd, 0);
	assert(&shmem != MAP_FAILED);


	// Open the log
	log_fp = fopen (shmem->BBD9000LOG,"a");
	assert(log_fp != NULL);

	/* open the event FIFO */
	evt_fp = fopen (BBD9000EVT,"w");
	assert(evt_fp != NULL);

	/* ^^^^ Init */


	while (1) {
	t_now = time(NULL);
	strftime (tmpstr, 256, "%Y-%m-%d %H:%M:%S", localtime (&t_now));
	fprintf (stdout,"%s: ",tmpstr);

// 		buf_siz = strftime( tmpstr, 256, "shutdown %-m/%-e %-l",
// 				localtime(&(shmem->sched_maint_start)) );
// 		buf_siz = strftime( tmpstr+buf_siz,  256, "-%-l%P !", localtime(&(shmem->sched_maint_end)) );
// 		fprintf (stdout,"sched_maint_start: %d, end: %d, string:[%s]",
// 			shmem->sched_maint_start,shmem->sched_maint_end, tmpstr);
// 		
// 		if (shmem->checkin_msg) fprintf (stdout," Checkin\n");
// 		else  fprintf (stdout,"\n");

 		if (shmem->server) fprintf (stdout," Server OK\n");
 		else  fprintf (stdout,"NO Server\n");
		
		sleep (1);
	}
	munmap(shmem, SHMEM_SIZE);
	close (shmem_fd);
	
	close (fifo_fd);
	exit(EXIT_SUCCESS);
}



void logMessage (FILE *log_fp, const char *template, ...) {
va_list ap;
time_t t_now;
char buf[STR_SIZE+1];

	if (! DEBUGGING) return;

	t_now = time(NULL);
	strftime (buf, STR_SIZE, "%Y-%m-%d %H:%M:%S", localtime (&t_now));
	fprintf (log_fp,"server %s: ",buf);

	// process the printf-style parameters
	va_start (ap, template);
	vfprintf (log_fp, template, ap);
	va_end (ap);
	fprintf (log_fp,"\n");
	fflush (log_fp);
}

