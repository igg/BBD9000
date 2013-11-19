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
/* gcc -o BBD9000init -lrt BBD9000init.c */
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/time.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <termios.h>
#include <assert.h>
#include <math.h>
#include <limits.h> // PATH_MAX

#include "BBD9000mem.h"
#include "BBD9000cfg.h"

// Defines
#define MAX_RESET_RETRIES 5

struct termios oldtio;
int ser_fd;

int checkSmartIO (BBD9000mem *shmem);
int initSer (const char *modem, speed_t baudrate, struct termios *oldtio);

// Baud rate defines
#define BAUD_CNT 17
speed_t baud_const[BAUD_CNT] = { B50, B75, B110, B134, B150, B200, B300, B600,
		B1200, B1800, B2400, B4800, B9600, B19200, B38400, B57600, B115200 };
unsigned long baud_value[BAUD_CNT] = { 50, 75, 110, 134, 150, 200, 300, 600,
		1200, 1800, 2400, 4800, 9600, 19200, 38400, 57600, 115200 };
int getBaudID (int baud);
ssize_t wait_ser_resp (int ser_fd, char *read_buf, size_t buf_len, char *resp, long ms_timeout);
void wait_ser_clear (int ser_fd, long ms_timeout);
long elapsed_msecs (struct timeval *start);

void sigTermHandler (int signum);


int main (int argc, char **argv) {
int shmem_fd;
int ret_val;
BBD9000mem shmem_s, *shmem=&shmem_s;
char BBD9000root[PATH_MAX], tmpPath[PATH_MAX], str[STR_SIZE], *chp;
struct stat filestat ;
mode_t fmode;


	/* initialize the whole struct to 0 */
	memset(shmem, 0, sizeof(BBD9000mem));

	// The location of the shared memory segment must be known to all sub-processes
	// All sub-processes depend on the BBD9000_SHMEM environment variable. They do not read the configuration file.
	// The BBD9000run setting in BBD9000.conf specifies the directory where these files are kept.
	// 
	// For bootstrapping, BBD9000_ROOT env variable or a parameter to this executable can be used
	// to specify the location of BBD9000.conf
	// If the system is already running and the shared memory segment exists, this program will exit
	// with a line to stdout reporting the location of the shared memory segment:
	// BBD9000_SHMEM = /path/to/shmem
	// So, to boot-strap the system, run BBD9000init twice with the same parameters.
	// The first run will initialize everything.
	// The second run will return the BBD9000_SHMEM path on stdout.
	// If BBD9000_SHMEM already exists, a message will also be sent to stderr, but the exit will be 0
	// 
	// The root path to BBD9000 configuration files can be provided in BBD9000_ROOT env variable,
	// or as the only parameter to this program.  Parameter over-rides the env variable.
	// Otherwise, we will attempt to find the path to the executable and set BBD9000_ROOT to its directory
	// All other configuration files are expected to be in the same directory as BBD9000.conf
	tmpPath[0] = '\0';
	if ( argc > 1) {
		strncpy (tmpPath, argv[1], sizeof (tmpPath));
	} else if ( (chp = getenv ("BBD9000_ROOT")) ) {
	// chp is not null, BBD9000_ROOT env variable exists
		strncpy (tmpPath, chp, sizeof (tmpPath));
	} else {
		// Get the path to this executable.
		// This is platform-specific
		#if defined(__FreeBSD__)
			int mib[4];
			mib[0] = CTL_KERN;
			mib[1] = KERN_PROC;
			mib[2] = KERN_PROC_PATHNAME;
			mib[3] = -1;
			char buf[1024];
			size_t cb = sizeof(buf);
			sysctl(mib, 4, buf, &cb, NULL, 0);

		#elif defined(linux) || defined(__linux) || defined(__linux__) || defined(__TOS_LINUX__)
			readlink ("/proc/self/exe", tmpPath, sizeof(tmpPath)-1);

		#elif defined(__APPLE__) || defined(__TOS_MACOS__)
			uint32_t size = sizeof(tmpPath);
			_NSGetExecutablePath(tmpPath, &size);

		#endif
	}
	// put the config file at the end of whatever path we got and make it absolute
	snprintf (BBD9000root,sizeof(BBD9000root),"%s/%s",tmpPath,BBD9000conf_def);
	if (! realpath(BBD9000root, tmpPath) ) {
		fprintf (stderr,"The configuration file path %s could not be resolved: %s\n",BBD9000root, strerror(errno));
		exit (-1);
	} else {
		// tmpPath is the absolute one
		// Set up the BBD9000root - where all the configuration files live
		chp = strrchr (tmpPath,'/');
		if (chp) *chp = '\0';
		strncpy (shmem->root_path,tmpPath,sizeof (shmem->root_path));
	}

	/* chdir to the root of the filesystem to not block dismounting */
	chdir("/");

	// The path from root to the main configuration file is hard-coded (BBD9000.conf)
	snprintf (shmem->BBD9000conf,PATH_SIZE,"%s/%s",shmem->root_path,BBD9000conf_def);
	// Read our main configuration
	if ( conf_cfg_read (shmem) != 0 ) {
		fprintf (stderr,"Initialization failed: Error parsing %s\n",shmem->BBD9000conf);
		exit (-1);
	}

	// The files used while running are set by cal_cfg_read().
	// If shmem already exists, we are already running, and won't reinitialize.
	// In this case, print BBD9000_SHMEM to stdout, print message to stderr, and exit normally
	if ( ! stat (shmem->BBD9000mem, &filestat) ) {
		fprintf (stdout,"BBD9000_SHMEM = %s\n",shmem->BBD9000mem);
		fprintf (stderr,"Initialization failed: Shared memory segment already exists at %s\n",shmem->BBD9000mem);
		fprintf (stderr,"To re-initialize, delete this file and try again.\n");
		exit (0);
	}

	/* Record time that this structure was initialized */
	gettimeofday(&(shmem->t_start), NULL);

	printf ("Initializing BBD9000-%d\n",shmem->kiosk_id);
	printf ("Scanned configuration file in %s\n",shmem->BBD9000conf);
	fflush (stdout);


	// Upon initialization, alarms are off - SmartIO will report any alarms upon reset
	shmem->valarm = 0;
	shmem->pump = 0;

	// Read our calibrations
	printf ("Scanning configuration file in %s\n",shmem->cal_conf);
	if ( cal_cfg_read (shmem) != 0 ) {
		fprintf (stderr,"Initialization failed: Error parsing %s\n",shmem->cal_conf);
		exit (-1);
	}

	// The calibrations are saved back to the conf file
	cal_cfg_write (shmem);

	printf ("Scanning configuration file in %s\n",shmem->run_conf);
	fflush (stdout);
	if ( run_cfg_read (shmem) != 0 ) {
		fprintf (stderr,"Initialization failed: Error parsing %s\n",shmem->run_conf);
		exit (-1);
	}

	// The shared memory and fifos live in the shmem->BBD9000run directory read from config
	// The final directory component of this path may not exist and needs to be created
	// This has to be done each time because this is a temporary filesystem.
	fmode = S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH;
	if ( stat (shmem->BBD9000run, &filestat) ) {
	// returned error, create it
		if (mkdir(shmem->BBD9000run, fmode)) {
			fprintf (stderr,"Initialization failed: Error creating BBD9000run directory %s: %s\n",shmem->BBD9000run,strerror(errno));
			exit (-1);
		}
	} else if (! (filestat.st_mode & S_IFDIR) ) {
	// no error from fstat, but not a directory
		fprintf (stderr,"Initialization failed: BBD9000run %s exists, but is not a directory\n",shmem->BBD9000run);
		exit (-1);
	}

	// Check for the kiosk's key file
	if ( stat (shmem->BBD9000key, &filestat) ) {
		strcpy (str,shmem->BBD9000key);
		chp = strrchr (str,'.');
		if (chp) strcpy (chp,"_pub.pem");
		else strcat (str,"_pub.pem");

		fprintf (stderr,"*** The BBD9000 private key does not exist.\n");
		fprintf (stderr,"*** To create it and its corresponding public key, use these two commands:\n");
		fprintf (stderr,"*** openssl genrsa -out %s 1024\n",shmem->BBD9000key);
		fprintf (stderr,"*** openssl rsa -in %s -outform PEM -out %s -pubout\n",shmem->BBD9000key,str);
		exit (1);
	}

	// Check for the server's key file
	if ( stat (shmem->BBD9000srv_key, &filestat) ) {
		strcpy (str,shmem->BBD9000srv_key);
		chp = strrchr (str,'/');
		if (chp) chp++;
		else chp = shmem->BBD9000srv_key;

		fprintf (stderr,"*** The BBD public server key does not exist.\n");
		fprintf (stderr,"*** A copy of it is available on the server (%s)\n",chp);
		exit (1);
	}

	/*
	* Verify, and if necessary reflash the SmartIO
	*/
	snprintf (tmpPath,PATH_SIZE,"%s/%s",shmem->root_path,BBD9000SmartIOhex);
	if ( stat (tmpPath, &filestat) ) {
		fprintf (stderr,"*** The SmartIO hex file (%s) does not exist!\n",tmpPath);
		exit (1);
	}
	fprintf (stdout,"Verifying SmartIO...");
	fflush (stdout);
	snprintf (str,STR_SIZE,"%s/%s -d %s -b %d -r '\\n\\nRESET\\n' -a 100 -s -v %s",
		shmem->root_path,BBD9000SmartIObootloader,
		shmem->SmartIOdev,shmem->SmartIObaud,
		tmpPath
	);
	if (system (str)) {
		fprintf (stdout,"Failed. Reflashing...");
		fflush (stdout);
		snprintf (str,STR_SIZE,"%s/%s -d %s -b %d -r '\\n\\nRESET\\n' -a 100 -s -p %s",
			shmem->root_path,BBD9000SmartIObootloader,
			shmem->SmartIOdev,shmem->SmartIObaud,
			tmpPath
		);
		if (system (str)) {
			fprintf (stdout,"Failed - exiting\n");
			exit (1);
		} else {
			fprintf (stdout,"Success\n");
		}
	} else {
		fprintf (stdout,"Verified\n");
	}

	checkSmartIO (shmem);

	/*
	* Create the named pipes (FIFOs)
	*/
	// conf_cfg_read() sets up the paths to the shared memory objects and fifos.

    /* Create the named pipe for events */
	fmode = S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH;
	ret_val = mkfifo(shmem->BBD9000evt, fmode);
	if ((ret_val != 0) && (errno != EEXIST)) {
		fprintf(stderr,"Error creating the named pipe %s\n",shmem->BBD9000evt);
		exit (1);
	} else {
		fprintf(stdout,"Created event FIFO %s\n",shmem->BBD9000evt);
	}
	chmod (shmem->BBD9000evt,fmode);

    /* Create the named pipe for timers */
	fmode = S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH;
	ret_val = mkfifo(shmem->BBD9000tim, fmode);
	if ((ret_val != 0) && (errno != EEXIST)) {
		fprintf(stderr,"Error creating the named pipe %s\n",shmem->BBD9000tim);
		exit (1);
	} else {
		fprintf(stdout,"Created timer FIFO %s\n",shmem->BBD9000tim);
	}
	chmod (shmem->BBD9000tim,fmode);

    /* Create the named pipe for hardware output */
	fmode = S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH;
	ret_val = mkfifo(shmem->BBD9000out, fmode);
	if ((ret_val != 0) && (errno != EEXIST)) {
		fprintf(stderr,"Error creating the named pipe %s\n",shmem->BBD9000out);
		exit (1);
	} else {
		fprintf(stdout,"Created peripheral output FIFO %s\n",shmem->BBD9000out);
	}
	chmod (shmem->BBD9000out,fmode);

    /* Create the named pipe for server communication */
	fmode = S_IRUSR | S_IWUSR;
	ret_val = mkfifo(shmem->BBD9000srv, fmode);
	if ((ret_val != 0) && (errno != EEXIST)) {
		fprintf(stderr,"Error creating the named pipe %s\n",shmem->BBD9000srv);
		exit (1);
	} else {
		fprintf(stdout,"Created server communications FIFO %s\n",shmem->BBD9000srv);
	}
	chmod (shmem->BBD9000srv,fmode);

    /* Create the named pipe for CC processor communication */
	fmode = S_IRUSR | S_IWUSR;
	ret_val = mkfifo(shmem->BBD9000ccg, fmode);
	if ((ret_val != 0) && (errno != EEXIST)) {
		fprintf(stderr,"Error creating the named pipe %s\n",shmem->BBD9000ccg);
		exit (1);
	} else {
		fprintf(stdout,"Created CC processor communications FIFO %s\n",shmem->BBD9000ccg);
	}
	chmod (shmem->BBD9000ccg,fmode);

    /* update network info - this will transmit a ping packet */
	fprintf(stdout,"Checking network status\n");
	netlink (shmem, "check", 0);
	fmode = S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH;
	chmod (shmem->BBD9000net,fmode);


	// open/create the shared memory object
	fmode = S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH;
	shmem_fd = open(shmem->BBD9000mem, O_RDWR | O_CREAT | O_TRUNC, fmode);
	if (shmem_fd < 0) {
		fprintf (stderr,"Could not open shared memory segment %s: %s\n",shmem->BBD9000mem, strerror (errno));
		exit (-1);
	}
	fchmod (shmem_fd,fmode);

	/* write our structure to shared memory */
	if ( write (shmem_fd, (void *)shmem, sizeof (BBD9000mem)) != sizeof (BBD9000mem) ) {
		fprintf (stderr,"Could not write to shared object %s: %s\n",shmem->BBD9000mem, strerror (errno));
		close (shmem_fd);
		unlink (shmem->BBD9000mem);
		exit (-1);
	}

	/* Success */
	fprintf (stdout,"Successfully innitialized %s; size=%lu bytes, %lu pages\n",
		shmem->BBD9000mem, (unsigned long)sizeof (BBD9000mem), (unsigned long)(SHMEM_SIZE / getpagesize())
	);
	close (shmem_fd);


	fprintf(stdout,"Done.  BBD9000 initialized.\n");
	exit (0);

}

int checkSmartIO (BBD9000mem *shmem) {
FILE *ser_fp;
char read_buf[READ_BUF_SIZE+1];
int reset_retries = MAX_RESET_RETRIES;
float vin=0.0,vold=0.0;
ssize_t res;


// Initialize the serial line and get its file descriptor
	ser_fd = initSer (shmem->SmartIOdev, baud_const[getBaudID(shmem->SmartIObaud)], &oldtio);
	if (ser_fd < 0) {
		fprintf (stderr,"Couldn't open SmartIO serial line %s: %s\n",shmem->SmartIOdev,strerror(errno));
		exit (EXIT_FAILURE);
	}
// Get a stream for the serial line too
	ser_fp = fdopen (ser_fd, "r+");
	if (ser_fp == NULL) {
		fprintf (stderr,"Couldn't fopen SmartIO serial line %s: %s\n",shmem->SmartIOdev,strerror(errno));
		exit (EXIT_FAILURE);
	}
// Use line buffering on this stream to match "canonical" serial IO setup
	setvbuf (ser_fp, NULL, _IOLBF, 0);

	// Register our signal handler to restore serial line
	signal (SIGTERM, sigTermHandler);
	signal (SIGPIPE, sigTermHandler);

	// Ignoring power signals
	signal (SIGPWRALRM , SIG_IGN);     /* SIGUSR1. Voltage ALRM on BBD9000 SIGPWRALRM */
	signal (SIGPWROK   , SIG_IGN);     /* SIGUSR2. Voltage ALRM RESET on BBD9000 SIGPWROK */

	fprintf(stdout,"Waiting for SmartIO reboot...");
	fflush (stdout);

	// Wait for SmartIO to reset itself
	while (reset_retries) {
		reset_retries--;
	// Reset the SmartIO board and wait for it to signal "Ready".
		fprintf (ser_fp,"\n\n\nRESET\n");
		fflush (ser_fp);
		if ( (res = wait_ser_resp (ser_fd, read_buf, READ_BUF_SIZE, "Ready", 2000)) ) break;
		wait_ser_clear (ser_fd, 200);
	}
	
	if (res) {
		fprintf(stdout,"Ready\nSetting calibrations...");
	} else {
		fprintf(stdout,"Could not reboot SmartIO - exiting\n");
		exit (EXIT_FAILURE);
	}
	
	// Drain any other output for a while...
	wait_ser_clear (ser_fd, 200);
	
	// Write the calibration settings to the SmartIO
	// This will stubbornly try to do this forever...
	// The calibration settings in the config file supersede any the SmartIO has

	// The voltage calibration points
	res = 0;
	while (!res) {
		fprintf (ser_fp,"VCAL\t%d\t%.2f\t%d\t%.2f\n",shmem->ADC0_cal.raw1, shmem->ADC0_cal.cal1, shmem->ADC0_cal.raw2, shmem->ADC0_cal.cal2);
		fflush (ser_fp);
		fprintf(stdout,"VCAL...");

		if ( (res = wait_ser_resp (ser_fd, read_buf, READ_BUF_SIZE, "VCAL", 200)) ) break;
		wait_ser_clear (ser_fd, 200);
	}
	// The voltage alarm threshold
	res = 0;
	while (!res) {	
		fprintf (ser_fp,"VIN-THR\t%.2f\t%.2f\n",shmem->valrm_on_threshold,shmem->valrm_off_threshold);
		fflush (ser_fp);
		fprintf(stdout,"VIN-THR...");
		if ( (res = wait_ser_resp (ser_fd, read_buf, READ_BUF_SIZE, "VIN-THR", 200)) ) break;
		wait_ser_clear (ser_fd, 200);
	}
	// Current calibration
	res = 0;
	while (!res) {	
		fprintf (ser_fp,"ICAL\t%d\t%.2f\t%d\t%.2f\n",shmem->ADC1_cal.raw1, shmem->ADC1_cal.cal1, shmem->ADC1_cal.raw2, shmem->ADC1_cal.cal2);
		fflush (ser_fp);
		fprintf(stdout,"ICAL...");
		if ( (res = wait_ser_resp (ser_fd, read_buf, READ_BUF_SIZE, "ICAL", 200)) ) break;
		wait_ser_clear (ser_fd, 200);
	}
	// Pump on/off thresholds
	res = 0;
	while (!res) {	
		fprintf (ser_fp,"PMP-THR\t%.2f\t%.2f\n",shmem->pump_on_threshold,shmem->pump_off_threshold);
		fflush (ser_fp);
		fprintf(stdout,"PMP-THR...");
		if ( (res = wait_ser_resp (ser_fd, read_buf, READ_BUF_SIZE, "PMP-THR", 200)) ) break;
		wait_ser_clear (ser_fd, 200);
	}
	// Flowmeter total
	res = 0;
	while (!res) {	
		fprintf (ser_fp,"FLM-TOT\t%lu\n",(unsigned long) (shmem->cumulative_gallons * shmem->flowmeter_pulses_per_gallon));
		fflush (ser_fp);
		fprintf(stdout,"FLM-TOT...");
		if ( (res = wait_ser_resp (ser_fd, read_buf, READ_BUF_SIZE, "FLM-TOT", 200)) ) break;
		wait_ser_clear (ser_fd, 200);
	}

	fprintf(stdout,"\nOK\nWaiting for stable voltage...");

	// Wait for SmartIO to stabilize the voltage
	// Potentially, forever.
	res = 0;
	while (!res) {	
		fprintf (ser_fp,"VIN\n");
		fflush (ser_fp);
		res = wait_ser_resp (ser_fd, read_buf, READ_BUF_SIZE, "VIN", 200);
		if (res) {
			vin = 0;
			sscanf (read_buf,"VIN\t%f",&vin);
			if (vin > 0.0) {
				fprintf(stdout,"%.2f...",vin);
				fflush (stdout);
				if ( (fabs(vin - vold)/vin > 0.005) || vin < shmem->valrm_on_threshold)
					res = 0; // not ready
				else break;
				vold = vin;
			} else {
				res = 0; // not a "VIN" line
			}
		}
		wait_ser_clear (ser_fd, 300);
	}
	shmem->voltage = vin;
	fprintf(stdout,"\nOK\n");

	return (1);

}


// Returns the serial line's fd
// The speed parameter must be a speed_t type, and one of these values defined in termios.h
// B0  B50  B75  B110  B134  B150  B200 B300  B600  B1200  B1800  B2400  B4800
// B9600  B19200  B38400  B57600  B115200 B230400  B460800

// if oldtio is non-NULL, save the old termio structure there for subsequent reset.
int initSer (const char *modem, speed_t baudrate, struct termios *oldtio) {
struct termios newtio;

	/*
	  Initialize the serial port.
	  This is taken from the Serial-Programming-HOWTO
	*/
	ser_fd = open(modem, O_RDWR | O_NOCTTY | O_NONBLOCK);
	assert(ser_fd != -1);
	
	if (oldtio) tcgetattr(ser_fd,oldtio); /* save current port settings */
	memset(&newtio, 0, sizeof(newtio));

	/*
	  BAUDRATE: Set bps rate. You could also use cfsetispeed and cfsetospeed.
	  CRTSCTS : output hardware flow control (only used if the cable has
	          all necessary lines. See sect. 7 of Serial-HOWTO)
	  CS8     : 8n1 (8bit,no parity,1 stopbit)
	  CLOCAL  : local connection, no modem contol
	  CREAD   : enable receiving characters
	*/
	newtio.c_cflag |= CS8;
	newtio.c_cflag |= CLOCAL;
	newtio.c_cflag |= CREAD;

	cfsetospeed (&newtio, baudrate);
	cfsetispeed (&newtio, baudrate);



	/*
	  IGNPAR  : ignore parity errors (8N1)
	  ICRNL   : map CR to NL (otherwise a CR input on the other computer
	            will not terminate input)
	            otherwise make device raw (no other input processing)
	*/
	newtio.c_iflag |= IGNPAR;
	newtio.c_iflag |= ICRNL;
	
	/*
	Raw output.
	*/
	newtio.c_oflag = 0;
	
	/*
	ICANON  : enable canonical input
	disable all echo functionality, and don't send signals to calling program
	*/
	newtio.c_lflag |= ICANON;


	/*
	  initialize all control characters
	  default values can be found in /usr/include/termios.h, and are given
	  in the comments, but we don't need them here
	  IGG: Not sure we need any of these, other than possibly VMIN
	*/
	newtio.c_cc[VINTR]    = 0;     /* Ctrl-c */
	newtio.c_cc[VQUIT]    = 0;     /* Ctrl-\ */
	newtio.c_cc[VERASE]   = 0;     /* del */
	newtio.c_cc[VKILL]    = 0;     /* @ */
	newtio.c_cc[VEOF]     = 4;     /* Ctrl-d */
	newtio.c_cc[VTIME]    = 0;     /* inter-character timer unused */
	newtio.c_cc[VMIN]     = 1;     /* blocking read until 1 character arrives */
//	newtio.c_cc[VSWTC]    = 0;     /* '\0' */
	newtio.c_cc[VSTART]   = 0;     /* Ctrl-q */
	newtio.c_cc[VSTOP]    = 0;     /* Ctrl-s */
	newtio.c_cc[VSUSP]    = 0;     /* Ctrl-z */
	newtio.c_cc[VEOL]     = 0;     /* '\0' */
	newtio.c_cc[VREPRINT] = 0;     /* Ctrl-r */
	newtio.c_cc[VDISCARD] = 0;     /* Ctrl-u */
	newtio.c_cc[VWERASE]  = 0;     /* Ctrl-w */
	newtio.c_cc[VLNEXT]   = 0;     /* Ctrl-v */
	newtio.c_cc[VEOL2]    = 0;     /* '\0' */


	
	/*
	  now clean the modem line and activate the settings for the port
	*/
	tcflush(ser_fd, TCIFLUSH);
	tcsetattr(ser_fd,TCSANOW,&newtio);
	
	return (ser_fd);
}

int getBaudID (int baud) {
int i, baudid=-1;

	// Checking baudrate
	for(i = 0; i < BAUD_CNT; i++) {
		if (baud_value[i] == baud) {
			baudid = i;
			break;
		}
	}
	
	assert(baudid != -1);
	return (baudid);
}

// Assumes line-buffered "canonical" reading, resp must appear as first string on the line.
// Assumes ser_fd open with O_NONBLOCK (non-blocking reads).
// Returns response size if response found. Returns 0 otherwise.
ssize_t wait_ser_resp (int ser_fd, char *read_buf, size_t buf_len, char *resp, long ms_timeout) {
	int resp_l = strlen (resp);
	struct timeval t_now;
	ssize_t res;
	gettimeofday(&t_now, NULL);

	*read_buf = '\0';
	while (elapsed_msecs (&t_now) < ms_timeout) {
		*read_buf = '\0';
		res = read(ser_fd,read_buf,buf_len);
		if (res > -1) {
			read_buf[res] = '\0';
			if ( ! strncmp (read_buf,resp,resp_l) ) break;
			else res = 0;
		} else res = 0;
		usleep (1000);
	}
	return (res);
}

// Consumes and ignores serial line input until ms_timeout is reached
// Assumes ser_fd open with O_NONBLOCK (non-blocking reads).
void wait_ser_clear (int ser_fd, long ms_timeout) {
	struct timeval t_now;
	gettimeofday(&t_now, NULL);
	while (elapsed_msecs (&t_now) < ms_timeout) {
		tcflush (ser_fd,TCIFLUSH);
		usleep (1000);
	}
}

long elapsed_msecs (struct timeval *start) {
long int msecs, ssecs=start->tv_sec, susecs=start->tv_usec;
struct timeval t_now;

	gettimeofday(&t_now, NULL);

	/* Perform the carry for the later subtraction by updating y. */
	if (t_now.tv_usec < susecs) {
		int nsec = (susecs - t_now.tv_usec) / 1000000 + 1;
		susecs -= 1000000 * nsec;
		ssecs += nsec;
	}
	if (t_now.tv_usec - susecs > 1000000) {
		int nsec = (t_now.tv_usec - susecs) / 1000000;
		susecs += 1000000 * nsec;
		ssecs -= nsec;
	}

	msecs = (t_now.tv_sec - ssecs) * 1000;
	msecs += (t_now.tv_usec - susecs) / 1000;
	return msecs;
}




void sigTermHandler (int signum) {

	if (signal (SIGTERM, sigTermHandler) == SIG_IGN)
		signal (SIGTERM, SIG_IGN);


	tcsetattr(ser_fd,TCSANOW,&oldtio);
	close (ser_fd);

	exit(EXIT_SUCCESS);
}

