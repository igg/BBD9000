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

#define _GNU_SOURCE
/*
  The timer library used here is called Timer_q: http://www.and.org/timer_q/overview
*/
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <sys/time.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/fcntl.h>
#include <assert.h>
#include <sys/mman.h>
#include <stdarg.h> // va_start, etc 
#include <signal.h>

#include <timer_q.h>

#include "BBD9000mem.h"

#ifndef FALSE
# define FALSE 0
#endif

#ifndef TRUE
# define TRUE 1
#endif
#define MAX_BUF_SIZE 256
#define DEBUGGING 1

typedef struct {
	char name[EVT_NAME_SIZE];
	char val[EVT_VALUE_SIZE];
	Timer_q_double_node node;
	char active;
} timer_s;
#define nTIMERS 64  // That's as many as there can ever be (ever)
timer_s timers[nTIMERS];
Timer_q_base *base = NULL;

void initPIDs ();
void checkPIDs ();
void logMessage (FILE *log_fp, const char *template, ...);
void sigReboot (int signum);
void fixTime ();

FILE *BBD9000EVT_fp, *log_fp;
BBD9000mem *shmem;

// PIDs we will monitor
#define NPIDS 5
char *pid_files[] = {
	"BBD9000SmartIO",
	"BBD9000authorizeDotNet",
	"BBD9000server",
	"BBD9000temp",
	"BBD9000fsm"
};
#define pid_root "/var/run"
#define pid_ext ".pid"
int pids[NPIDS];
#define pid_timer "CHK-PIDS"
#define pid_timeout 123

/* real timer code from this point onwards... */
static void timer_func1(int type, void *data)
{
timer_s *timer = (timer_s *)data;

	timer->active = 0;
	if (type == TIMER_Q_TYPE_CALL_DEL) return;

// Expiring the watchdog timer causes a reboot
	if ( ! strcmp (timer->name,WATCHDOG_TIMER) ) {
		// Use the alarm handler to reboot
		// to make sure we don't send more than one reboot
		// sending 0 cancels the alarm, so we have to set it to at least 1.
		alarm (1);

// Expiring the PID timer causes a PID check
	} else if ( ! strcmp (timer->name,pid_timer) ) {
		checkPIDs();
// Otherwise, we send the timeout event
	} else {
		fprintf(BBD9000EVT_fp, "%s Timeout\n", timer->name);
		fflush(BBD9000EVT_fp);
	}
}

timer_s *find_timer (char *name) {
int i,found=-1,blank=-1;

	for (i=0; i< nTIMERS && found < 0; i++) {
		if (*(timers[i].name) == *name && !strcmp (timers[i].name,name)) found = i;
		else if ( ! *(timers[i].name) && blank < 0 ) blank = i;
	}

	if (found > -1) return ( &(timers[found]) );
	if (blank > -1) {
		strcpy (timers[blank].name,name);
		return ( &(timers[blank]) );
	}
	return (NULL); // out of timers
}

void clear_timer (timer_s *timer) {
	memset (timer,0,sizeof(timer));
}

int main(void)
{
int shmem_fd;

long wait_period = 0;
int  rv=0, timer_millisecs;
int input_fd;
FILE *input_fp;
fd_set read_fds;
char read_buf[MAX_BUF_SIZE], mode[8], name[EVT_NAME_SIZE], val[EVT_VALUE_SIZE];
timer_s *timer;

struct timeval s_tv, wait_timeval, *wait_timeval_p;
const struct timeval *tv = NULL;


	/* chdir to the root of the filesystem to not block dismounting */
	chdir("/");

	/* open the shared memory object */
	shmem_fd = open(BBD9000MEM, O_RDWR|O_SYNC);
	if (shmem_fd < 0) {
		fprintf (stderr,"Could not open shared POSIX memory segment %s: %s\n",BBD9000MEM, strerror (errno));
		exit (-1);
	}

	/* mmap our shared memory */
	shmem = (BBD9000mem *) mmap(0, SHMEM_SIZE, PROT_READ|PROT_WRITE, MAP_SHARED, shmem_fd, 0);
	assert(&shmem != MAP_FAILED);
	assert(shmem);


	// Open the event fifo
	BBD9000EVT_fp = fopen (BBD9000EVT,"w");
	assert(BBD9000EVT_fp != NULL);

	// Open the log
	log_fp = fopen (shmem->BBD9000LOG,"a");
	assert(log_fp != NULL);
	logMessage (log_fp, "BBD9000timer started");

// Even though we are only reading this FIFO, we open it for writing as well
// The reason is the way select works.  If it detects an EOF on the FIFO (i.e. all writers closed the FIFO)
// the select will never block, returning an EOF every time, thus defeating the purpose of select.
// Don't know if the EOF is ever cleared, so that select returns to blocking if other writers connect.
// The solution is to open the FIFO read and write so that there is always at least one writer (this process).
	input_fd = open (BBD9000TIMER, O_RDWR | O_NONBLOCK);
	if (input_fd < 0) {
		logMessage (log_fp, "Couldn't open %s: %s",BBD9000TIMER,strerror(errno));
	}
	// We're going to use line-buffered input to make our life easier
	// But we need the file-descriptor for the select call also, so we open both
	input_fp = fdopen (input_fd, "r");
	if (input_fp == NULL) {
		close (input_fd);
		logMessage (log_fp, "Couldn't fopen %s: %s",BBD9000TIMER,strerror(errno));
	}
// Turn off buffering on this stream to make fgets work with select()
// Otherwise, fgets can read more than one line into its buffer, causing select() to block,
// but return only the first line read.
	setvbuf (input_fp, NULL, _IOLBF, 0);


	// Register our signal handlers
	// This process should be killed with SIGKILL (-9) unless a reboot is wanted
	// Ignore these signals
	signal (SIGHUP     , SIG_IGN);     /* Hangup (POSIX).  */
	signal (SIGPROF    , SIG_IGN);     /* Profiling alarm clock (4.2 BSD).  */
	signal (SIGWINCH   , SIG_IGN);     /* Window size change (4.3 BSD, Sun).  */
	signal (SIGPOLL    , SIG_IGN);     /* Pollable event occurred (System V).  */
	signal (SIGIO      , SIG_IGN);     /* I/O now possible (4.2 BSD).  */
	signal (SIGPWR     , SIG_IGN);     /* Power failure restart (System V).  */
	signal (SIGSYS     , SIG_IGN);     /* Bad system call.  */
	signal (SIGUNUSED  , SIG_IGN);
	signal (SIGPWRALRM , SIG_IGN);     /* SIGUSR1. Voltage ALRM on BBD9000 SIGPWRALRM */
	signal (SIGPWROK   , SIG_IGN);     /* SIGUSR2. Voltage ALRM RESET on BBD9000 SIGPWROK */
	signal (SIGCHLD    , SIG_IGN);     /* Same as SIGCHLD (System V).  */
	signal (SIGTSTP    , SIG_IGN);     /* Keyboard stop (POSIX).  */
	signal (SIGTTIN    , SIG_IGN);     /* Background read from tty (POSIX).  */
	signal (SIGTTOU    , SIG_IGN);     /* Background write to tty (POSIX).  */
	signal (SIGURG     , SIG_IGN);     /* Urgent condition on socket (4.2 BSD).  */

	// Reboot on these signals
	signal (SIGINT     , sigReboot);   /* Interrupt (ANSI).  */
	signal (SIGQUIT    , sigReboot);   /* Quit (POSIX).  */
	signal (SIGILL     , sigReboot);   /* Illegal instruction (ANSI).  */
	signal (SIGTRAP    , sigReboot);   /* Trace trap (POSIX).  */
	signal (SIGABRT    , sigReboot);   /* Abort (ANSI).  */
	signal (SIGIOT     , sigReboot);   /* IOT trap (4.2 BSD).  */
	signal (SIGBUS     , sigReboot);   /* BUS error (4.2 BSD).  */
	signal (SIGFPE     , sigReboot);   /* Floating-point exception (ANSI).  */
	signal (SIGSEGV    , sigReboot);   /* Segmentation violation (ANSI).  */
	signal (SIGPIPE    , sigReboot);   /* Broken pipe (POSIX).  */
	signal (SIGALRM    , sigReboot);   /* Alarm clock (POSIX).  */
	signal (SIGTERM    , sigReboot);   /* Termination (ANSI).  */
	signal (SIGSTKFLT  , sigReboot);   /* Stack fault.  */
	signal (SIGXCPU    , sigReboot);   /* CPU limit exceeded (4.2 BSD).  */
	signal (SIGXFSZ    , sigReboot);   /* File size limit exceeded (4.2 BSD).  */
	signal (SIGVTALRM  , sigReboot);   /* Virtual alarm clock (4.2 BSD).  */

	// Initialize our timers
	memset (timers,0,sizeof(timers));
	base = timer_q_add_base(timer_func1, TIMER_Q_FLAG_BASE_DEFAULT);
	assert (base != NULL);

	logMessage (log_fp, "Waiting for Server and SmartIO synch");
	// Wait here until the SmartIO and server are synchronized
	while (!shmem->SmartIOsync || !shmem->serverSync || !shmem->fsmSync) {
		usleep (200000);
	}
	logMessage (log_fp, "Server and SmartIO synchronized");

	// Initialize the watchdog timer
	// If the Watchdog timer ever expires, this causes a reboot.
	// If the timer queue is ever empty, this also causes a reboot.
	// The fsm must reset the Watchdog timer before it expires.
	timer = find_timer(WATCHDOG_TIMER);
	gettimeofday(&s_tv, NULL);
	TIMER_Q_TIMEVAL_ADD_SECS(&s_tv, WATCHDOG_TIMEOUT, 0);
	timer->active = 1;
	timer_q_add_static_node((Timer_q_node *)&(timer->node),
		base, timer, &s_tv,TIMER_Q_FLAG_NODE_DOUBLE);
	// For good measure we also send ourselves an alarm signal
	alarm (WATCHDOG_TIMEOUT);


	// Initialize the timer for checking PIDs
	// If any of the PIDs don't respond to signal 0, we reboot.
	// Note that all processes listed above must have started by now
	initPIDs();
	checkPIDs();

	logMessage (log_fp, "Processing timers");

	while (1)
	{
		rv = 0;
		tv = timer_q_first_timeval();
		wait_timeval_p = &wait_timeval;
		if (tv) {
			gettimeofday(&s_tv, NULL);
			wait_period = timer_q_timeval_diff_usecs(tv, &s_tv);
			if (wait_period <= 0 && tv) {
				timer_q_run_norm(&s_tv);
				// set input timeout to 0
				memset (&wait_timeval,0,sizeof(struct timeval));
			} else {
				memset (&wait_timeval,0,sizeof(struct timeval));
				// set input timeout to wait_period
				TIMER_Q_TIMEVAL_ADD_SECS(&wait_timeval, 0, wait_period);
			}
		} else {
			// set input timeout to block indefinitely (queue is empty)
			wait_timeval_p = NULL;
		}
		FD_ZERO(&read_fds);
		FD_SET(input_fd, &read_fds);
//		logMessage (log_fp, "select(): %d",wait_timeval.tv_sec);
		rv = select(input_fd+1, &read_fds, NULL, NULL, wait_timeval_p);
		if (rv > 0 && FD_ISSET(input_fd, &read_fds)) {
		// Didn't timeout  - a new request
			while (fgets (read_buf, MAX_BUF_SIZE, input_fp)) {
				memset (mode,0,8);
				memset (name,0,EVT_NAME_SIZE);
				memset (val,0,EVT_VALUE_SIZE);
				sscanf (read_buf,"%[^\t]\t%[^\t\n]\t%[^\n]",mode,name,val);
				if (*mode && *name) {
					if ( !strcmp (mode,"CHANGE") ) {
						fixTime();
						continue;
					}
					timer = find_timer(name);
//logMessage (log_fp, "name: %s, found:[%s] %s",name, timer ? timer->name : "not found", val);
					if (!timer) continue; // This can happen when we're out of timers.
					gettimeofday(&s_tv, NULL);
					if ( !strcmp (mode,"SET") ) {
						timer_millisecs = atoi(val);
						if (timer_millisecs < 200000) {
							TIMER_Q_TIMEVAL_ADD_SECS(&s_tv, 0, timer_millisecs * 1000);
						} else if (timer_millisecs > 0) {
							TIMER_Q_TIMEVAL_ADD_SECS(&s_tv, timer_millisecs / 1000, 0);
						} else { // bogus value
							if (!timer->active)
								clear_timer (timer);
							// If its active, leave it alone
							continue;
						}
						if (timer->active) {
							timer_q_del_node(base, (Timer_q_node *)&(timer->node));
						}
						timer->active = 1;
						timer_q_add_static_node((Timer_q_node *)&(timer->node),
							base, timer, &s_tv,TIMER_Q_FLAG_NODE_DOUBLE);
						// If this is a watchdog timer reset, reset the alarm
						if (!strcmp (name,WATCHDOG_TIMER)) alarm (WATCHDOG_TIMEOUT);
					} else if ( !strcmp (mode,"DEL") ) {
						if (timer->active) {
							timer_q_del_node(base, (Timer_q_node *)&(timer->node));
						}
						clear_timer (timer);
					}
				}
			}
		}
//		logMessage (log_fp, "timer_q_run_norm");
		gettimeofday(&s_tv, NULL);
		timer_q_run_norm(&s_tv);
	}
		
exit (EXIT_SUCCESS);
}


void initPIDs () {
int i;
char path[PATH_SIZE];
FILE *fp;

	// find the PIDs we're supposed to monitor
	for (i=0; i<NPIDS; i++) {
		pids[i] = 0;
		sprintf (path,"%s/%s%s",pid_root,pid_files[i],pid_ext);
		fp = fopen (path,"r");
		if (fp) {
			fscanf (fp,"%d",&(pids[i]));
			fclose (fp);
		}
	}
}

void checkPIDs () {
int i;
int all_ok=1;
timer_s *timer;
struct timeval s_tv;
char dead_program[128];


	*dead_program = '\0';
	for (i=0; i<NPIDS; i++) {
		
		if (pids[i]) {
			if ( kill (pids[i],0) ) {
				all_ok=0;
				strcpy (dead_program,pid_files[i]);
			}
		} else {
			all_ok=0;
		}
	}
	
	if (!all_ok) {
		if (*dead_program) {
			sprintf (shmem->boot_reason,"%s died",dead_program);
		} else {
			strcpy (shmem->boot_reason,"Dead PID");
		}
		sigReboot (0);
	}
	
	// Initialize the timer for checking PIDs
	// If any of the PIDs don't respond to signal 0, we reboot.
	timer = find_timer(pid_timer);
	gettimeofday(&s_tv, NULL);
	TIMER_Q_TIMEVAL_ADD_SECS(&s_tv, pid_timeout, 0);
	timer->active = 1;
	timer_q_add_static_node((Timer_q_node *)&(timer->node),
		base, timer, &s_tv,TIMER_Q_FLAG_NODE_DOUBLE);
}


void logMessage (FILE *log_fp, const char *template, ...) {
va_list ap;
time_t t_now;
char buf[STR_SIZE+1];

	if (! DEBUGGING) return;

	t_now = time(NULL);
	strftime (buf, STR_SIZE, "%Y-%m-%d %H:%M:%S", localtime (&t_now));
	fprintf (log_fp,"timer %s: ",buf);

	// process the printf-style parameters
	va_start (ap, template);
	vfprintf (log_fp, template, ap);
	va_end (ap);
	fprintf (log_fp,"\n");
	fflush (log_fp);
}

void sigReboot (int signum) {

	if (signum == SIGALRM) {
		strcpy (shmem->boot_reason,"watchdog");
	} else if (signum == 0) {
	// This gets set elsewhere
//		strcpy (shmem->boot_reason,"Dead PID");
	} else {
		sprintf (shmem->boot_reason,"SIG %d",signum);
	}
	logMessage (log_fp, "Reboot after %s",shmem->boot_reason);
	system (BBD9000reboot);
	// Sleep here lets other processes that rely on timer to exit cleanly.
	// Obviously, we ignore any timer requests at this point.
	sleep (1); // give users of our pipe a chance to shutdown
	exit(EXIT_SUCCESS);
}

void fixTime () {
struct timeval s_tv;
int i;
timer_s *timer;

	if (shmem->delta_t == 0) return;
	logMessage (log_fp, "time now: %d delta %d",(int)(time(NULL)),shmem->delta_t);
	alarm (WATCHDOG_TIMEOUT);
	for (i=0; i < nTIMERS; i++) {
		timer = &(timers[i]);
		if (timer->active) {
			timer_q_cntl_node((Timer_q_node *)&(timer->node),
				TIMER_Q_CNTL_NODE_GET_TIMEVAL, &s_tv);
			timer_q_del_node(base, (Timer_q_node *)&(timer->node));
			TIMER_Q_TIMEVAL_ADD_SECS(&s_tv, shmem->delta_t, 0);
			timer_q_add_static_node((Timer_q_node *)&(timer->node),
				base, timer, &s_tv,TIMER_Q_FLAG_NODE_DOUBLE);
		}
	}
	shmem->delta_t = 0;
	gettimeofday(&s_tv, NULL);
	timer_q_run_norm(&s_tv);
//	logMessage (log_fp, "fixed time");
}

