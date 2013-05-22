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
#include <sys/ioctl.h>

// Stuff from shmem.h
// struct timeval utility macros
#define TV_ADD_MS(x, y)	{ \
	(x).tv_usec += (y) * 1000; \
	(x).tv_sec += (x).tv_usec / 1000000; \
	(x).tv_usec = (x).tv_usec % 1000000; \
}
#define READ_BUF_SIZE 511
#define STR_SIZE 256
// End of shmem.h stuff

typedef struct {
	char port_dev[STR_SIZE];
	int ser_fd;
	char do_verbose;
	char error[STR_SIZE];
	char signal_valid;
	int sig_dBm, sig_bars;
	char SIM_valid;
	char ICCID[STR_SIZE], IMSI[STR_SIZE];
	char checkin_SMS;
	struct termios oldtio;
} selfStruct;

// Globals
selfStruct self_glob;

// int shmem_fd;
// FILE *log_fp;
// BBD9000mem *shmem;


// Prototypes
int readSer (selfStruct *self, const char *expect, int timeout) ;
void doSerLine (selfStruct *self, const char *buf) ;
int initSer (const char *modem, speed_t baudrate, struct termios *oldtio);
void writeSer (selfStruct *self, const char *buf) ;
void resetSerFD (selfStruct *self) ;
int intrSerDTR (selfStruct *self) ;
int rstrSerDTR (selfStruct *self) ;
int intrSerESC (selfStruct *self) ;
int rstrSerESC (selfStruct *self) ;
int intrSer (selfStruct *self) ;
int rstrSer (selfStruct *self) ;
int do_modem_status (selfStruct *self) ;
void usage () ;

// Poll interval - seconds
#define CELL_POLL_SECS 10
#define READ_TIMEOUT_MS 100
// This determines how we interrupt the data connection
// using the escape sequence '+++' or deasserting DTR
#define MODEM_INTR_ESC 1
#define MODEM_INTR_DTR 0
#define MAX_RETRIES 3


// Baud rate defines
#define BAUD_CNT 17
speed_t baud_const[BAUD_CNT] = { B50, B75, B110, B134, B150, B200, B300, B600,
		B1200, B1800, B2400, B4800, B9600, B19200, B38400, B57600, B115200 };
unsigned long baud_value[BAUD_CNT] = { 50, 75, 110, 134, 150, 200, 300, 600,
		1200, 1800, 2400, 4800, 9600, 19200, 38400, 57600, 115200 };
int getBaudID (int baud);

// Signal handler
void clean_exit (selfStruct *self, int status) ;
void sigTermHandler (int signum);

int main (int argc, char **argv) {
int got_OK, got_connect, retries, got_expect, i;
char useDTR=0;
int exit_stat=EXIT_SUCCESS;
char data_mode=1;
selfStruct *self;
char do_shutdown=0, do_status=0;




	/* vvvv Init */
	self = &self_glob;
	memset (self,0,sizeof(selfStruct));

	/* chdir to the root of the filesystem to not block dismounting */
	chdir("/");

	if (argc < 3) usage();
	for (i=3; i < argc; i++) {
		if (!strcmp (argv[i],"-v")) self->do_verbose = 1;
	}
	
	strcpy (self->port_dev,argv[1]);

	if (! strcmp (argv[2],"status")) do_status = 1;
	if (! strcmp (argv[2],"shutdown")) do_shutdown = 1;
	if (! strcmp (argv[2],"reboot")) do_shutdown = do_status = 1;

	

	self->ser_fd = initSer (self->port_dev, baud_const[getBaudID(115200)], &(self_glob.oldtio));
	if (self->ser_fd < 0) {
		sprintf (self->error,"Couldn't open serial line %s: %s",self->port_dev,strerror(errno));
		clean_exit (self,EXIT_FAILURE);
	}

	// Register our signal handler to restore serial line
	signal (SIGTERM, sigTermHandler);
	signal (SIGINT, sigTermHandler);
	signal (SIGQUIT, sigTermHandler);


	/* ^^^^ Init */
	writeSer (self,"AT\r");
	got_OK = readSer (self, "OK",100);
	if (got_OK) data_mode=0;

	if (!got_OK) {
		if (self->do_verbose) fprintf (stderr,"Interrupting modem with escape sequence\n");
		for (retries = 0; !(got_OK = intrSerESC (self)) && retries < MAX_RETRIES; retries++);
	}

	if (!got_OK) {
		if (self->do_verbose) fprintf (stderr,"Interrupting modem with DTR\n");
		for (retries = 0; !(got_OK = intrSerDTR (self)) && retries < MAX_RETRIES; retries++);
		if (got_OK) useDTR = 1;
	}
	if (got_OK) {

		if (do_shutdown) {
			// Power down.
			// We do this instead of a watchdog reboot to make sure the SIM boots as well
			writeSer (self,"AT+CFUN=0\r");
			if (! readSer (self, "SHUTDOWN",10000)) {
				sprintf (self->error,"Could not shutdown modem");
				clean_exit (self,EXIT_FAILURE);
			}
		}

		if (do_shutdown && do_status) {
		// wait for the modem to reset itself
			// The power-up takes ~ 7 seconds.
			// Getting the modem to register a signal takes ~ 20 seconds
			sleep (30);
			retries = 30;
			got_expect = 0;
			while (retries-- && !got_expect) {
				writeSer (self,"AT\r");
				got_expect = readSer (self, "OK",100);
				if (!got_expect) sleep (1);
			}
			if (!got_expect) {
				sprintf (self->error,"Could not reach modem after reboot");
				clean_exit (self,EXIT_FAILURE);
			}
		}
		
		if (do_status) {
			if (! do_modem_status (self) ) clean_exit (self,EXIT_FAILURE);
		}

		if (data_mode && ! do_shutdown) {
			if (useDTR) {
				for (retries = 0; !(got_connect = rstrSerDTR (self)) && retries < MAX_RETRIES; retries++);
			} else {
				for (retries = 0; !(got_connect = rstrSerESC (self)) && retries < MAX_RETRIES; retries++);
			}
			if (retries >= MAX_RETRIES) exit_stat = EXIT_FAILURE;
		}
	} else {
		if (self->do_verbose) fprintf (stderr,"Could not interrupt modem\n");
		sprintf (self->error,"Could not interrupt modem");
		clean_exit (self,EXIT_FAILURE);
	}

	if (self->do_verbose) fprintf (stderr,"-+-+-+-+-+-+-+-+-+-+-+\n");
	if (self->signal_valid) {
		fprintf (stdout,"%d dBm (%d/5)",self->sig_dBm,self->sig_bars);
	} else {
		fprintf (stdout,"No dBm (-/5)");
	}
	if ( *(self->ICCID) && *(self->IMSI) ) {
		fprintf (stdout," SIM: OK");
	} else if ( !*(self->ICCID) && !*(self->IMSI) ) {
		fprintf (stdout," SIM: None");
	} else {
		fprintf (stdout," SIM: Error");
	}
	if (self->checkin_SMS) {
		fprintf (stdout," SMS: Checkin\n");
	} else {
		fprintf (stdout," SMS: None\n");
	}

	return (exit_stat);
}

int do_modem_status (selfStruct *self) {
int got_expect;
int retries;

	// Signal quality
	// This does not start right away when the modem reboots
	// reports +CSQ: 99,99 until ready.
	// Don't know what it would report if there's really no signal
	// Trouble is, we could have rebooted the modem and exited.
	// In this case, the modem won't be ready for about 30 seconds, but will then
	// get it signal strength.
	// OTOH, if we just want the modem status quickly, and it really has no reception
	// We still have to assume we're waiting for it after a reboot.
	retries=15;
	while (!self->signal_valid && retries--) {
		writeSer (self,"AT+CSQ\r");
		got_expect = readSer (self, "OK",100);
		if (!self->signal_valid) sleep(2);
	}
	
	if (!got_expect && self->do_verbose) {
		sprintf (self->error,"Could not read cellular signal");
		return (0);
	}

	// SIM ID (ICCID) AT*E2SSN  N.B.: This number is usually printed on the SIM card
	writeSer (self,"AT*E2SSN\r");
	readSer (self, "OK",100);
	// SIM IMSI AT+CIMI N.B.: This number is considered "secret"
	writeSer (self,"AT+CIMI\r");
	readSer (self, "OK",100);
	
	// Text mode SMS
	writeSer (self,"AT+CMGF=1\r");
	readSer (self, "OK",100);
	writeSer (self,"AT+CSMP=17,167\r");
	readSer (self, "OK",100);
	// List text messages (this also reads them)
	writeSer (self,"AT+CMGL\r");
	readSer (self, NULL,100);
	// Delete text messages
	writeSer (self,"AT+CMGD=0,4\r");
	readSer (self, NULL,100);
	
	return (1);
}

int readSer (selfStruct *self, const char *expect, int timeout) {
fd_set read_fds;
int nfds;
struct timeval wait_timeval;
char read_buf[READ_BUF_SIZE+1], *char_p, *line_p, *read_buf_p;
ssize_t nread=0,nread_tot=0;
char got_expect=0;

	if (self->do_verbose) fprintf (stderr,"vvvvvvvvvv\n");
	// Read response until timeout
	read_buf_p = read_buf;
	line_p = read_buf;
	do {
		// Setup the timeout for waiting on the serial line
		// This has to be done for each call to select
		memset (&wait_timeval,0,sizeof(struct timeval));
		TV_ADD_MS (wait_timeval,timeout); // timeout for no modem output
		FD_ZERO(&read_fds);
		FD_SET(self->ser_fd, &read_fds);
		nfds = select (FD_SETSIZE, &read_fds, NULL, NULL, &wait_timeval);

		if (FD_ISSET(self->ser_fd, &read_fds)) {
			if (self->do_verbose) fprintf (stderr,"Reading modem\n");
			nread = read (self->ser_fd,read_buf_p,READ_BUF_SIZE-nread_tot);
			if (nread < 1) {
				resetSerFD (self);
				break;
			}
			read_buf_p[nread] = '\0';
			if (self->do_verbose) fprintf (stderr,"nread = %d; [%s]\n",nread,read_buf_p);
			char_p = read_buf_p;
			read_buf_p += nread;
			nread_tot += nread;
			while (*char_p) {
				// find the end of the line
				while (*char_p && *char_p != '\r' && *char_p != '\n') *char_p++;
				if (*char_p) {
					// find the end of \r\n, setting them to null
					while (*char_p == '\r' || *char_p == '\n') *char_p++ = '\0';
					if (self->do_verbose) fprintf (stderr,"line: [%s]\n",line_p);
					if (*line_p) {
						doSerLine (self, line_p);
						if (expect && !strncmp (line_p,expect,strlen(expect))) {
							got_expect = 1;
							tcflush(self->ser_fd, TCOFLUSH);
							if (self->do_verbose) fprintf (stderr,"got_expect: %s\n",expect);
						}
					}
					if (*char_p) {
					// Another line in the buffer
						line_p = char_p;
					} else {
					// Last line in the buffer
						read_buf_p = read_buf;
						line_p = read_buf;
						nread_tot = 0;
					}
				}
			}
		}
	} while (nfds > 0 && got_expect == 0 && nread > -1);
	if (self->do_verbose) fprintf (stderr,"^^^^^^^^^^ nfds: %d, got_expect: %d, nread: %d\n",nfds,got_expect,nread );
	if (nfds < 0 || nread < 0)
		return (-1);
	else
		return (got_expect);
}

void writeSer (selfStruct *self, const char *buf) {
int buf_siz = strlen (buf), retries=10;
ssize_t nwritten=0, write_tot=0;

	while (retries && write_tot < buf_siz) {
		nwritten = write_tot = 0;
		if (self->do_verbose) fprintf (stderr,"retries: %d writing: %s (%d)\n",retries, buf,buf_siz);
		while (write_tot < buf_siz && nwritten > -1) {
			nwritten = write (self->ser_fd,buf+write_tot,buf_siz-write_tot);
			if (self->do_verbose) fprintf (stderr,"nwritten: %d\n",(int)nwritten);
			if (nwritten > -1) write_tot += nwritten;
		}
		if (nwritten < 0) {
			if (self->do_verbose) fprintf (stderr,"Write error: %s\n",strerror(errno));
			resetSerFD (self);
			retries--;
		}
	}

	if (!retries) {
		sprintf (self->error,"Writing to serial port");
		clean_exit (self,EXIT_FAILURE);
	}
}

void resetSerFD (selfStruct *self) {
	close (self->ser_fd);
	self->ser_fd = initSer (self->port_dev, baud_const[getBaudID(115200)], NULL);
	if (self->ser_fd < 0) {
		sprintf (self->error,"Couldn't reopen serial line %s after I/O error: %s",self->port_dev,strerror(errno));
		clean_exit (self,EXIT_FAILURE);
	}
}

int intrSerDTR (selfStruct *self) {
int ser_bits;
int got_OK=0;
struct termios tio;

	tcgetattr(self->ser_fd, &tio);           /* get the termio information */
	tio.c_cflag &= ~HUPCL;             /* clear the HUPCL bit */
	tcsetattr(self->ser_fd, TCSANOW, &tio);  /* set the termio information */

	ioctl(self->ser_fd, TIOCMGET, &ser_bits); /* get the serial port status */
	ser_bits |= TIOCM_DTR;              /* de-assert DTR (assert is ser_bits &= ~TIOCM_DTR;) */
	ioctl(self->ser_fd, TIOCMSET, &ser_bits); /* set the serial port status */

	usleep (100000); // 100ms sleep
	writeSer (self,"AT\r");
	got_OK = readSer (self, "OK",100);
	if (!got_OK) {
		writeSer (self,"+++AT\r");
		got_OK = readSer (self, "OK",100);
	}
	return (got_OK);
}

int intrSerESC (selfStruct *self) {
int got_OK=0;

	// Attention/interrupt
	writeSer (self,"+++AT\r");
// # Guard time (seconds)
// AT*E2ESC=1
	usleep (1100000);
	got_OK = readSer (self, "OK",100);
	if (!got_OK) {
		writeSer (self,"AT\r");
		got_OK = readSer (self, "OK",100);
	}
	return (got_OK);
}


int rstrSerDTR (selfStruct *self) {
int ser_bits;
int got_connect=0;

	writeSer (self,"ATO\r");
	got_connect = readSer (self, "CONNECT",100);

	// Reassert DRT
	ioctl(self->ser_fd, TIOCMGET, &ser_bits); /* get the serial port status */
	ser_bits &= ~TIOCM_DTR;             /* assert DTR (deassert is ser_bits |= TIOCM_DTR;) */
	ioctl(self->ser_fd, TIOCMSET, &ser_bits); /* set the serial port status */

	return (got_connect);
}

int rstrSerESC (selfStruct *self) {
int got_connect=0;

	writeSer (self,"ATO\r");
	got_connect = readSer (self, "CONNECT",100);
	return (got_connect);
}

int intrSer (selfStruct *self) {
int got_OK=0;

	if (MODEM_INTR_DTR) got_OK = intrSerDTR (self);
	else got_OK = intrSerESC (self);
	return (got_OK);
}

int rstrSer (selfStruct *self) {
int got_connect=0;

	// Back to data mode
	if (MODEM_INTR_DTR) got_connect = rstrSerDTR (self);
	else got_connect = rstrSerESC (self);
	return (got_connect);
}



void doSerLine (selfStruct *self, const char *buf) {
int int_val;
int nscanned;
char *charp;
char str_buf[READ_BUF_SIZE+1];
static int is_SMS=0, is_ICCID=0, is_IMSI=0;

	if (!buf || !*buf) return;
	if (self->do_verbose) fprintf (stderr,"Line from modem: [%s]\n",buf);

	//CSQ: signal quality
	//+CSQ: 5,0
	nscanned = sscanf (buf,"+CSQ: %d",&int_val);
	if (nscanned == 1) {
		is_SMS = 0;
		if (int_val < 32) {
			self->sig_dBm = (int_val * 2) + -113;
			self->sig_bars = (self->sig_dBm - -113) / 7;
			self->signal_valid = 1;
		} else {
			self->signal_valid = 0;
		}
	if (self->do_verbose) fprintf (stderr,"Signal (CSQ): %d dBm, %d bars\n",self->sig_dBm,self->sig_bars);

	}

	//AT*E2SSN: ICCID
	//line follows the echo line, and is not ERROR
	if (is_ICCID && strncmp (buf,"ERROR",5)) sscanf (buf,"%[^\r\n]",self->ICCID);
	if (! strcmp (buf,"AT*E2SSN")) is_ICCID = 1;
	else is_ICCID = 0;

	//AT+CIMI: IMSI
	//line follows the echo line, and is not ERROR
	if (is_IMSI && strncmp (buf,"ERROR",5)) sscanf (buf,"%[^\r\n]",self->IMSI);
	if (! strcmp (buf,"AT+CIMI")) is_IMSI = 1;
	else is_IMSI = 0;

	//CMGL: List SMS messages
	nscanned = sscanf (buf,"+CMGL: %d",&int_val);
	if (nscanned == 1) {
		is_SMS = int_val;
		if (self->do_verbose) fprintf (stderr,"SMS #%d\n",int_val);
	} else if (is_SMS) {
		strcpy (str_buf,buf);
		if (self->do_verbose) fprintf (stderr,"Read SMS %d: %s\n",is_SMS,str_buf);
		charp = str_buf;
		while (*charp) {*charp = tolower(*charp);charp++;}
		if (strstr (str_buf,"server checkin")) {
			self->checkin_SMS = 1;
			if (self->do_verbose) fprintf (stderr,"SMS Checkin request\n");
		}
		is_SMS = 0;
	}
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
	ser_fd = open(modem, O_RDWR | O_NOCTTY | O_NONBLOCK);
	if (ser_fd < 0) {
		return (ser_fd);
	}
	
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
	cfsetispeed (&newtio, baudrate);

	newtio.c_cflag |= CS8;
	newtio.c_cflag |= CLOCAL;
	newtio.c_cflag |= CREAD;
	newtio.c_cflag |= CRTSCTS;



	/*
	  IGNPAR  : ignore parity errors (8N1)
	  ICRNL   : map CR to NL (otherwise a CR input on the other computer
	            will not terminate input)
	            otherwise make device raw (no other input processing)
	*/
	newtio.c_iflag = IGNPAR;
//	newtio.c_iflag |= ICRNL;
//	newtio.c_iflag |= INLCR;

//	newtio.c_iflag |= IGNCR;
	
	/*
	Raw output.
	*/
	newtio.c_oflag = 0;
//	newtio.c_oflag |= OPOST;
//	newtio.c_oflag |= ONLCR;
//	newtio.c_oflag |= ONLRET;


	/*
	ICANON  : enable canonical input
	disable all echo functionality, and don't send signals to calling program
	*/
	newtio.c_lflag = 0;

	newtio.c_cc[VMIN]     = 0;

	
	/*
	  now clean the modem line and activate the settings for the port
	*/
	tcflush(ser_fd, TCIOFLUSH);
	tcsetattr(ser_fd,TCSANOW,&newtio);

	return (ser_fd);
}

void clean_exit (selfStruct *self, int status) {

	tcsetattr(self->ser_fd,TCSANOW,&(self->oldtio));
	close (self->ser_fd);
	
	if (*(self->error)) fprintf (stderr,"Error: %s\n",self->error);

	exit(status);
}


void sigTermHandler (int signum) {

	tcsetattr(self_glob.ser_fd,TCSANOW,&(self_glob.oldtio));
	close (self_glob.ser_fd);

	exit(EXIT_SUCCESS);
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

void usage () {
	fprintf (stderr,"%s <modem dev> <command> [-v]\n","BBD9000cell");
	fprintf (stderr,"Commands:\n");
	fprintf (stderr,"  status    report modem status (signal, SIMid, checkin SMS\n");
	fprintf (stderr,"  reboot    reboot the modem by powering it down - status is reported\n");
	fprintf (stderr,"  shutdown  power down the modem. Modem will restart in ~30 sec., but don't wait for it\n");
	fprintf (stderr,"-v  produce verbose output\n");
	exit (EXIT_FAILURE);
}
