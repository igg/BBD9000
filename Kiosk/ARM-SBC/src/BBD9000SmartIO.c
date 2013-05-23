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
#include <stdlib.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <sys/time.h>
#include <stdio.h>
#include <fcntl.h>
#include <assert.h>
#include <time.h>
#include <string.h>
#include <errno.h>
#include <string.h>
#include <ctype.h>
#include <stdarg.h>

#include <sys/stat.h>
#include <termios.h>
#include <unistd.h>
#include <signal.h>

#include "BBD9000mem.h"

// Globals
struct termios oldtio;
int BBD9000OUT_fd,ser_fd,shmem_fd;
FILE *BBD9000OUT_fp, *BBD9000EVT_fp, *ser_fp, *log_fp;
BBD9000mem *shmem;


// Prototypes
void logMessage (FILE *log_fp, const char *template, ...);
void getSetting (const char *buf, FILE* ser_fp, FILE *fifo_out) ;
void doSer (const char *buf, FILE *fifo_out) ;
void doFifoIN (const char *buf, FILE *ser_fp) ;
int checkSmartIO (char *read_buf, FILE *ser_fp) ;
int initSer (const char *modem, speed_t baudrate, struct termios *oldtio);

// Defines
#define MAX_RESET_RETRIES 5
#define MAX_RESPONSE_RETRIES 5

// Baud rate defines
#define BAUD_CNT 17
speed_t baud_const[BAUD_CNT] = { B50, B75, B110, B134, B150, B200, B300, B600,
		B1200, B1800, B2400, B4800, B9600, B19200, B38400, B57600, B115200 };
unsigned long baud_value[BAUD_CNT] = { 50, 75, 110, 134, 150, 200, 300, 600,
		1200, 1800, 2400, 4800, 9600, 19200, 38400, 57600, 115200 };
int getBaudID (int baud);

// Signal handler
void sigTermHandler (int signum);

int main () {
fd_set read_fds;
int nfds;
char read_buf[READ_BUF_SIZE+1];


	/* vvvv Init */

	/* chdir to the root of the filesystem to not block dismounting */
	chdir("/");

	/* open the shared memory object */
	shmem_fd = open(BBD9000MEM, O_RDWR|O_SYNC);
	assert(shmem_fd != -1);

	/* mmap our shared memory */
	shmem = (BBD9000mem *) mmap(0, SHMEM_SIZE, PROT_READ|PROT_WRITE, MAP_SHARED, shmem_fd, 0);
	assert(&shmem != MAP_FAILED);

	/* open the event FIFO */
	BBD9000EVT_fp = fopen (BBD9000EVT,"w");
	assert(BBD9000EVT_fp != NULL);

	// Open the log
	log_fp = fopen (shmem->BBD9000LOG,"a");
	assert(log_fp != NULL);
	logMessage (log_fp,"BBD9000SmartIO started");

// Even though we are only reading this FIFO, we open it for writing as well
// The reason is the way select works.  If it detects an EOF on the FIFO (i.e. all writers closed the FIFO)
// the select will never block, returning an EOF every time, thus defeating the purpose of select.
// Don't know if the EOF is ever cleared, so that select returns to blocking if other writers connect.
// The solution is to open the FIFO read and write so that there is always at least one writer (this process).
	BBD9000OUT_fd = open(BBD9000OUT, O_RDWR | O_NONBLOCK);
	if (BBD9000OUT_fd < 0) {
		logMessage (log_fp,"Couldn't open %s: %s\n",BBD9000OUT,strerror(errno));
		exit (EXIT_FAILURE);
	}
	// We're going to use line-buffered input to make our life easier
	// But we need the file-descriptor for the select call also, so we open both
	BBD9000OUT_fp = fdopen (BBD9000OUT_fd, "r");
	if (BBD9000OUT_fp == NULL) {
		logMessage (log_fp,"Couldn't fopen %s: %s\n",BBD9000OUT,strerror(errno));
		exit (EXIT_FAILURE);
	}
// Turn off buffering on this stream to make fgets work with select()
// Otherwise, fgets can read more than one line into its buffer, causing select() to block,
// but return only the first line read.
	setvbuf (BBD9000OUT_fp, NULL, _IOLBF, 0);

// Initialize the serial line and get its file descriptor
	ser_fd = initSer (shmem->SmartIOdev, baud_const[getBaudID(shmem->SmartIObaud)], &oldtio);
	if (ser_fd < 0) {
		logMessage (log_fp,"Couldn't open serial line %s: %s\n",shmem->SmartIOdev,strerror(errno));
		exit (EXIT_FAILURE);
	}
// Get a stream for the serial line too
	ser_fp = fdopen (ser_fd, "r+");
	if (ser_fp == NULL) {
		logMessage (log_fp,"Couldn't fopen serial line %s: %s\n",shmem->SmartIOdev,strerror(errno));
		exit (EXIT_FAILURE);
	}
// Turn off buffering on this stream to make fgets work with select()
// Otherwise, fgets can read more than one line into its buffer, causing select() to block,
// but return only the first line read.
	setvbuf (ser_fp, NULL, _IOLBF, 0);

	// Register our signal handler to restore serial line
	signal (SIGTERM, sigTermHandler);
	signal (SIGPIPE, sigTermHandler);

	// Ignoring power signals
	signal (SIGPWRALRM , SIG_IGN);     /* SIGUSR1. Voltage ALRM on BBD9000 SIGPWRALRM */
	signal (SIGPWROK   , SIG_IGN);     /* SIGUSR2. Voltage ALRM RESET on BBD9000 SIGPWROK */

	/* ^^^^ Init */

	// BBD9000init has taken care of making sure SmartIO is functioning.	
	// ensure that SmartIO is synchronized with shmem
	getSetting ("FLM-TOT", ser_fp, BBD9000EVT_fp);
	getSetting ("PMP-THR", ser_fp, BBD9000EVT_fp);
	getSetting ("VIN", ser_fp, BBD9000EVT_fp);
	getSetting ("PMP", ser_fp, BBD9000EVT_fp);
	getSetting ("DRSN", ser_fp, BBD9000EVT_fp);
	getSetting ("MTN", ser_fp, BBD9000EVT_fp);

	logMessage (log_fp,"BBD9000SmartIO synchronized");
	shmem->SmartIOsync = 1;

	while (1) {
		FD_ZERO(&read_fds);
		FD_SET(ser_fd, &read_fds);
		FD_SET(BBD9000OUT_fd, &read_fds);
		nfds = select (FD_SETSIZE, &read_fds, NULL, NULL, NULL);

		// Read a line or more, return pointer to next line

		if (FD_ISSET(ser_fd, &read_fds)) {
			while (fgets (read_buf,READ_BUF_SIZE,ser_fp)) {
				doSer (read_buf,BBD9000EVT_fp);
			}
		}

		if (FD_ISSET(BBD9000OUT_fd, &read_fds)) {
			while (fgets (read_buf,READ_BUF_SIZE,BBD9000OUT_fp)) {
				doFifoIN (read_buf,ser_fp);
//				usleep (10000); // don't overwhelm the SmartIO
			}
		}
	}
}


void getSetting (const char *buf, FILE* ser_fp, FILE *fifo_out) {
char read_buf[READ_BUF_SIZE+1], *char_p;

	fprintf (ser_fp,"%s\n",buf);
	fflush (ser_fp);
	char_p = NULL;
	while (! char_p ) {
		usleep (10000);
		char_p = fgets (read_buf,READ_BUF_SIZE,ser_fp);
		if (strncmp (read_buf,buf,strlen(buf)) ) char_p = NULL;
		else doSer (read_buf,BBD9000EVT_fp);
	}
}

void doSer (const char *buf, FILE *fifo_out) {
char evt[EVT_NAME_SIZE+1],val[EVT_VALUE_SIZE+1];
unsigned long val_ul;
float val_f,val_f2;
double val_d;
int i,val_i,nscanned;
struct timeval t_now;

	gettimeofday(&t_now, NULL);

	*evt = *val = '\0';
	sscanf (buf,"%" xstr(EVT_NAME_SIZE) "[^\t\r\n]%*[\t\r\n]%" xstr(EVT_VALUE_SIZE) "[^\r\n]",evt,val);
//logMessage (log_fp,"SmartIO Line:[%s]",buf);
	if (!strcmp (evt,"FLM") ) {
		nscanned = sscanf (val,"%lu",&val_ul);
		if (nscanned && val_ul > shmem->flowmeter_counts) {
			if (shmem->t_update_flowmeter.tv_sec) {
				val_f = (float)(val_ul-shmem->flowmeter_counts) / TV_ELAPSED_MS (t_now,shmem->t_update_flowmeter);
				if (val_f > shmem->max_counts_per_ms) {
					shmem->max_counts_per_ms = val_f;
				}
			} else {
				shmem->max_counts_per_ms = 0;
			}
		} else {
			shmem->max_counts_per_ms = 0;
		}
		if (nscanned) {
			shmem->flowmeter_counts = val_ul;
			shmem->t_update_flowmeter = t_now;
			fprintf (fifo_out,"Flowmeter\t%s\n",val);
		}
	} else if (!strcmp (evt,"KP") && *val) {
		fprintf (fifo_out,"Keypad\t%s\n",val);
		if (*val == '#') {
			fprintf (fifo_out,"Keypad Valid\n");
		} else if (*val == '*') {
			memset (shmem->keypad_buffer,0,KEYPAD_BUFF_SIZE);
		} else {
			for (i=0; shmem->keypad_buffer[i]; i++) {};
			if (i >= KEYPAD_BUFF_SIZE-1) {
				memmove (&(shmem->keypad_buffer[0]),&(shmem->keypad_buffer[1]),KEYPAD_BUFF_SIZE-1);
				i--;
			}
			shmem->keypad_buffer[i] = *val;
		}
	} else if (!strcmp (evt,"MTN-STRT") ) {
		shmem->motion = 1;
		fprintf (fifo_out,"Motion Detected\n");
	} else if (!strcmp (evt,"MTN-STOP") ) {
		shmem->motion = 0;
		fprintf (fifo_out,"Motion Stopped\n");
	} else if (!strcmp (evt,"DRSN-OPND") ) {
		shmem->door_open = 1;
		fprintf (fifo_out,"Door Opened\n");
	} else if (!strcmp (evt,"DRSN-CLSD") ) {
		shmem->door_open = 0;
		fprintf (fifo_out,"Door Closed\n");
	} else if (!strcmp (evt,"VALRM-STRT") ) {
		shmem->valarm = 1;
		fprintf (fifo_out,"Voltage Alarm\n");
	} else if (!strcmp (evt,"VALRM-STOP") ) {
		shmem->valarm = 0;
		fprintf (fifo_out,"Voltage Alarm Reset\n");
	} else if (!strcmp (evt,"PMP-ON") ) {
		shmem->pump = 1;
		fprintf (fifo_out,"Pump ON\n");
	} else if (!strcmp (evt,"PMP-OFF") ) {
		shmem->pump = 0;
		fprintf (fifo_out,"Pump OFF\n");
	} else if (!strcmp (evt,"MSR") && *val) {
		fprintf (fifo_out,"MSR\t%s\n",val);

	// These are externally initiated - not self-triggered by the SmartIO
	} else if (!strcmp (evt,"MTN") && *val) {
 		val_i = atoi (val);
 		if (val_i) shmem->motion = 1;
 		else shmem->motion = 0;
//		logMessage (log_fp,"MTN is %d",shmem->motion);
	} else if (!strcmp (evt,"DRSN")  && *val) {
 		val_i = atoi (val);
 		if (val_i) shmem->door_open = 1;
 		else shmem->door_open = 0;
//		logMessage (log_fp,"DRSN is %d",shmem->door_open);
	} else if (!strcmp (evt,"VIN")  && *val) {
		sscanf (val,"%f",&val_f);
		shmem->voltage = val_f;
		shmem->t_voltage = t_now;
//		logMessage (log_fp,"Updated VIN to %f",shmem->voltage);
	} else if (!strcmp (evt,"PMP")  && *val) {
		sscanf (val,"%f",&val_f);
		shmem->current = val_f;
		shmem->t_current = t_now;
//		logMessage (log_fp,"Updated PMP to %f",shmem->current);
	} else if (!strcmp (evt,"PMP-THR") ) {
		if (sscanf (val,"%f\t%f",&val_f,&val_f2) == 2) {
	//		logMessage (log_fp,"Reading PMP-THR %f %f",val_f,val_f2);
			// values in shmem supersede values in the SmartIO EEPROM
			if (shmem->pump_on_threshold-val_f > 0.001 ||
				shmem->pump_on_threshold-val_f < -0.001 ||
				shmem->pump_off_threshold-val_f2 < -0.001 ||
				shmem->pump_off_threshold-val_f2 > 0.001) {
					fprintf (ser_fp,"PMP-THR\t%.2f\t%.2f\n",shmem->pump_on_threshold,shmem->pump_off_threshold);
					fflush (ser_fp);
					logMessage (log_fp,"Updated EEPROM pump thresholds from (%.2f,%.2f) to (%.2f,%.2f)",
						val_f,val_f2,shmem->pump_on_threshold,shmem->pump_off_threshold);
			}
		}
	} else if (!strcmp (evt,"VIN-THR") ) {
		if (sscanf (val,"%f\t%f",&val_f,&val_f2) == 2) {
	//		logMessage (log_fp,"Reading VIN-THR %f %f",val_f,val_f2);
			// values in shmem supersede values in the SmartIO EEPROM
			if (shmem->valrm_on_threshold-val_f > 0.001 ||
				shmem->valrm_on_threshold-val_f < -0.001 ||
				shmem->valrm_off_threshold-val_f2 < -0.001 ||
				shmem->valrm_off_threshold-val_f2 > 0.001) {
					fprintf (ser_fp,"VIN-THR\t%.2f\t%.2f\n",shmem->valrm_on_threshold,shmem->valrm_off_threshold);
					fflush (ser_fp);
					logMessage (log_fp,"Updated EEPROM Voltage alarm thresholds from (%.2f,%.2f) to (%.2f,%.2f)",
						val_f,val_f2,shmem->valrm_on_threshold,shmem->valrm_off_threshold);
			}
		}
	} else if (!strcmp (evt,"FLM-TOT") ) {
		if (sscanf (val,"%lu",&val_ul) == 1) {
			val_d = (double)val_ul / shmem->flowmeter_pulses_per_gallon;
	//		logMessage (log_fp,"Reading FLM-TOT %ul",val_ul);
			// values in shmem supersede values in the SmartIO EEPROM
			if (val_d - shmem->cumulative_gallons > 0.01 ||
				val_d - shmem->cumulative_gallons < -0.01) {
					fprintf (ser_fp,"FLM-TOT\t%lu\n",(unsigned long) (shmem->cumulative_gallons * shmem->flowmeter_pulses_per_gallon));
					fflush (ser_fp);
					logMessage (log_fp,"Updated EEPROM cumulative flowmeter count from %lu (%.4f g) to %lu (%.4f g)",
						val_ul,val_d,
						(unsigned long) (shmem->cumulative_gallons * shmem->flowmeter_pulses_per_gallon),shmem->cumulative_gallons );
			}
		}
	} else if ( !strcmp (evt,"Ready") ) {
		logMessage (log_fp,"SmartIO Reboot");
	}

	shmem->t_update = t_now;

	fflush (fifo_out);
}

void doFifoIN (const char *buf, FILE *ser_fp) {
char evt[EVT_NAME_SIZE+1],val[EVT_VALUE_SIZE+1];
unsigned long val_ul;
struct timeval t_now;

//logMessage (log_fp,"FIFO Line:[%s]",buf);
	gettimeofday(&t_now, NULL);

	*evt = *val = '\0';
	sscanf (buf,"%" xstr(EVT_NAME_SIZE) "[^\t\r\n]%*[\t\r\n]%" xstr(EVT_VALUE_SIZE) "[^\r\n]",evt,val);
	sscanf (val,"%lu",&val_ul);

	if (*val && !strcmp (evt,"LightsRly") ) {
		if (val_ul) shmem->LightsRly = 1;
		else shmem->LightsRly = 0;
		fprintf (ser_fp,"LGHT\t%d\n",shmem->LightsRly);
		fprintf (ser_fp,"LCDBL\t%d\n",shmem->LightsRly);
	} else if (*val && !strcmp (evt,"StrikeRly") ) {
		if (val_ul) shmem->StrikeRly = 1;
		else shmem->StrikeRly = 0;
		fprintf (ser_fp,"STK\t%d\n",shmem->StrikeRly);
	} else if (*val && !strcmp (evt,"PumpRly") ) {
		if (val_ul) shmem->PumpRly = 1;
		else shmem->PumpRly = 0;
		fprintf (ser_fp,"RLY1\t%d\n",shmem->PumpRly);
	} else if (*val && !strcmp (evt,"AuxRly") ) {
		if (val_ul) shmem->AuxRly = 1;
		else shmem->AuxRly = 0;
		fprintf (ser_fp,"AUX\t%d\n",shmem->AuxRly);
	} else if (!strcmp (evt,"LCD1") ) {
		strncpy (shmem->LCD1,val,LCD_MAX_LINE_SIZE);
		fprintf (ser_fp,"LCD1\t%s\n",shmem->LCD1);
	} else if (!strcmp (evt,"LCD2") ) {
		strncpy (shmem->LCD2,val,LCD_MAX_LINE_SIZE);
		fprintf (ser_fp,"LCD2\t%s\n",shmem->LCD2);
	} else if (*val && !strcmp (evt,"Flow") ) {
		shmem->t_update_flowmeter = t_now;
		shmem->flowmeter_counts = (u_int32_t) val_ul;
		fprintf (ser_fp,"FLM-CUR\t%lu\n",val_ul);


// Issue queries to SmartIO
	} else if (!strcmp (evt,"FLM-TOT") ) {
		fprintf (ser_fp,"FLM-TOT\n");
	} else if (!strcmp (evt,"MTN") ) {
		fprintf (ser_fp,"MTN\n");
	} else if (!strcmp (evt,"DRSN") ) {
		fprintf (ser_fp,"DRSN\n");
	} else if (!strcmp (evt,"PMP") ) {
		fprintf (ser_fp,"PMP\n");
	} else if (!strcmp (evt,"VIN") ) {
		fprintf (ser_fp,"VIN\n");
	}


	shmem->t_update = t_now;
	fflush (ser_fp);
//	tcflush(ser_fd, TCIOFLUSH);
}

// Returns the serial line's fd
// The speed parameter must be a speed_t type, and one of these values defined in termios.h
// B0  B50  B75  B110  B134  B150  B200 B300  B600  B1200  B1800  B2400  B4800
// B9600  B19200  B38400  B57600  B115200 B230400  B460800

// if oldtio is non-NULL, save the old termio structure there for subsequent reset.
int initSer (const char *modem, speed_t baudrate, struct termios *oldtio) {
int ser_fd;
struct termios newtio;

	/*
	  Initialize the serial port.
	  This is taken from the Serial-Programming-HOWTO
	*/
	ser_fd = open(shmem->SmartIOdev, O_RDWR | O_NOCTTY | O_NONBLOCK );
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


void sigTermHandler (int signum) {

	if (signal (SIGTERM, sigTermHandler) == SIG_IGN)
		signal (SIGTERM, SIG_IGN);


	tcsetattr(ser_fd,TCSANOW,&oldtio);
	close (ser_fd);

	exit(EXIT_SUCCESS);
}

void logMessage (FILE *log_fp, const char *template, ...) {
va_list ap;
time_t t_now;
char buf[STR_SIZE+1];


	t_now = time(NULL);
	strftime (buf, STR_SIZE, "%Y-%m-%d %H:%M:%S", localtime (&t_now));
	fprintf (log_fp,"SmartIO %s: ",buf);

	// process the printf-style parameters
	va_start (ap, template);
	vfprintf (log_fp, template, ap);
	va_end (ap);
	fprintf (log_fp,"\n");
	fflush (log_fp);
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
