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

/*
  serial8N1
  serial8N1 -b 11536 -d /dev/AMD1 -f
  This is a simple and limited serial communication utility for sending
  stdin and stdout over the specified serial device at the specified baudrate in 8N1
  Without the -e flag, the program terminates on eof on stdin.
  Any output from the device after this eof is ignored.
  The program does not need input in order to produce output from the device
  Compile: gcc -Wall -o serial8N1 serial8N1.c
*/

#include <stdlib.h>
#include <fcntl.h>
#include <termios.h>
#include <unistd.h>
#include <stdio.h>
#include <time.h>
#include <string.h>
#include <errno.h>
#include <signal.h>


// Globals
struct termios oldtio;
int ser_fd;
FILE *ser_fp;


// Prototypes
int initSer (const char *modem, speed_t baudrate, struct termios *oldtio, int doCTS, int doCRLF);
void usage();

// Defines
#define READ_BUF_SIZE 1024

// Baud rate defines
#define BAUD_CNT 17
speed_t baud_const[BAUD_CNT] = { B50, B75, B110, B134, B150, B200, B300, B600,
		B1200, B1800, B2400, B4800, B9600, B19200, B38400, B57600, B115200 };
unsigned long baud_value[BAUD_CNT] = { 50, 75, 110, 134, 150, 200, 300, 600,
		1200, 1800, 2400, 4800, 9600, 19200, 38400, 57600, 115200 };
int getBaudID (int baud);

// Signal handler
void sigTermHandler (int signum);

int main (int argc, char **argv) {
fd_set read_fds;
int i, baud=0, doCTS=0, doCRLF=0, type=0;
char read_buf[READ_BUF_SIZE], *device = NULL, *in = NULL, *out = NULL;
FILE *in_fp, *out_fp;
int in_fd, out_fd;


	/* vvvv Init */

	for(i = 1; i < argc; i++) {
		if(*argv[i] == '-') {
			type = argv[i][1];
			if (type == 'h') doCTS = 1;
			else if (type == 'c') doCRLF = 1;
		} else {
			switch(type) {
				case 'd':
					device = argv[i];
					break;
				case 'b':
					baud = getBaudID(atoi(argv[i]));
					break;
				case 'i':
					in = argv[i];
					break;
				case 'o':
					out = argv[i];
					break;
				default:
					fprintf(stderr,"Unrecognized parameter '%c'!\n",(char)type);
					usage();
			}
			type = 0;
		}
	}
	
	if (baud < 1 || device == NULL) {
		usage();
	}
	
	if (in) {
		in_fd = open(in, O_RDWR | O_NONBLOCK);
		if (in_fd < 0) {
			fprintf (stderr,"Could not open %s: %s\n",in,strerror(errno));
			exit(EXIT_FAILURE);
		}
		in_fp = fdopen (in_fd, "r");
		if (!in_fp) {
			fprintf (stderr,"Could not open %s: %s\n",in,strerror(errno));
			exit(EXIT_FAILURE);
		}
	} else {
		in_fp = stdin;
		in_fd = STDIN_FILENO;
	}
	
	if (out) {
		out_fd = open(out, O_RDWR | O_NONBLOCK);
		if (out_fd < 0) {
			fprintf (stderr,"Could not open %s: %s\n",out,strerror(errno));
			exit(EXIT_FAILURE);
		}
		out_fp = fdopen (out_fd, "w");
		if (!out_fp) {
			fprintf (stderr,"Could not open %s: %s\n",out,strerror(errno));
			exit(EXIT_FAILURE);
		}
	} else {
		out_fp = stdout;
		out_fd = STDOUT_FILENO;
	}

	/* chdir to the root of the filesystem to not block dismounting */
	chdir("/");

// Initialize the serial line and get its file descriptor
	ser_fd = initSer (device, baud_const[baud], &oldtio, doCTS, doCRLF);
	if (ser_fd < 0) {
		fprintf (stderr,"Couldn't open serial line %s: %s\n",device,strerror(errno));
		exit(EXIT_FAILURE);
	}
// Get a stream for the serial line too
	ser_fp = fdopen (ser_fd, "r+");
	if (ser_fp == NULL) {
		fprintf (stderr,"Couldn't fopen serial line %s: %s\n",device,strerror(errno));
		exit(EXIT_FAILURE);
	}
// Turn off buffering on this stream to make fgets work with select()
// Otherwise, fgets can read more than one line into its buffer, causing select() to block,
// but return only the first line read.
	setvbuf (ser_fp, NULL, _IONBF, 0);

	// Register our signal handler to restore serial line
	if (signal (SIGTERM, sigTermHandler) == SIG_IGN)
		signal (SIGTERM, SIG_IGN);
	if (signal (SIGINT, sigTermHandler) == SIG_IGN)
		signal (SIGINT, SIG_IGN);
	if (signal (SIGQUIT, sigTermHandler) == SIG_IGN)
		signal (SIGQUIT, SIG_IGN);
	if (signal (SIGALRM, sigTermHandler) == SIG_IGN)
		signal (SIGALRM, SIG_IGN);

	// Ignoring SIGPIPEs.  This may drop data, but we must live on!
	signal (SIGPIPE, SIG_IGN);

// Turn off buffering on the input stream to make fgets work with select()
// Otherwise, fgets can read more than one line into its buffer, causing select() to block,
// but return only the first line read.
	setvbuf (in_fp, NULL, _IONBF, 0);
	setvbuf (out_fp, NULL, _IONBF, 0);
	setvbuf (ser_fp, NULL, _IONBF, 0);

	/* ^^^^ Init */

	while (1) {
		FD_ZERO(&read_fds);
		FD_SET(ser_fd, &read_fds);
		FD_SET(in_fd, &read_fds);
		select (FD_SETSIZE, &read_fds, NULL, NULL, NULL);

		if (FD_ISSET(ser_fd, &read_fds)) {
			fgets (read_buf,READ_BUF_SIZE,ser_fp);
			fputs (read_buf,out_fp);
		}

		if (FD_ISSET(in_fd, &read_fds) && !feof(in_fp)) {
			fgets (read_buf,READ_BUF_SIZE,in_fp);
			fputs (read_buf,ser_fp);
			if (feof(in_fp)) {
				alarm (1); // Exit when the alarm expires to read last bit of serial
			}
		}
	}
}

// Returns the serial line's fd
// The speed parameter must be a speed_t type, and one of these values defined in termios.h
// B0  B50  B75  B110  B134  B150  B200 B300  B600  B1200  B1800  B2400  B4800
// B9600  B19200  B38400  B57600  B115200 B230400  B460800

// if oldtio is non-NULL, save the old termio structure there for subsequent reset.
int initSer (const char *modem, speed_t baudrate, struct termios *oldtio, int doCTS, int doCRLF) {
int ser_fd;
struct termios newtio;

	/*
	  Initialize the serial port.
	  This is taken from the Serial-Programming-HOWTO
	*/
	ser_fd = open(modem, O_RDWR | O_NOCTTY );
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
//	newtio.c_cflag = baudrate | CRTSCTS | CS8 | CLOCAL | CREAD;
	cfsetospeed (&newtio, baudrate);
	cfsetispeed (&newtio, baudrate);

	newtio.c_cflag |= CS8;
	newtio.c_cflag |= CLOCAL;
	newtio.c_cflag |= CREAD;
	if (doCTS) newtio.c_cflag |= CRTSCTS;




	/*
	  IGNPAR  : ignore bytes with parity errors
	  ICRNL   : map CR to NL (otherwise a CR input on the other computer
	            will not terminate input)
	            otherwise make device raw (no other input processing)
	*/
	newtio.c_iflag |= IGNPAR;
	newtio.c_iflag |= ICRNL;
	newtio.c_iflag |= IGNCR;
//	newtio.c_iflag |= INLCR;
	
	
	/*
	Raw output.
	*/
	newtio.c_oflag = 0;
	// ONLCR maps newlines into CR-LF pairs
	if (doCRLF) {
		newtio.c_oflag |= OPOST;
		newtio.c_oflag |= ONLCR;
	}
	
	
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
// 	newtio.c_cc[VINTR]    = 0;     /* Ctrl-c */
// 	newtio.c_cc[VQUIT]    = 0;     /* Ctrl-\ */
// 	newtio.c_cc[VERASE]   = 0;     /* del */
// 	newtio.c_cc[VKILL]    = 0;     /* @ */
// 	newtio.c_cc[VEOF]     = 4;     /* Ctrl-d */
// 	newtio.c_cc[VTIME]    = 0;     /* inter-character timer unused */
// 	newtio.c_cc[VMIN]     = 1;     /* blocking read until 1 character arrives */
// //	newtio.c_cc[VSWTC]    = 0;     /* '\0' */
// 	newtio.c_cc[VSTART]   = 0;     /* Ctrl-q */
// 	newtio.c_cc[VSTOP]    = 0;     /* Ctrl-s */
// 	newtio.c_cc[VSUSP]    = 0;     /* Ctrl-z */
// 	newtio.c_cc[VEOL]     = 0;     /* '\0' */
// 	newtio.c_cc[VREPRINT] = 0;     /* Ctrl-r */
// 	newtio.c_cc[VDISCARD] = 0;     /* Ctrl-u */
// 	newtio.c_cc[VWERASE]  = 0;     /* Ctrl-w */
// 	newtio.c_cc[VLNEXT]   = 0;     /* Ctrl-v */
// 	newtio.c_cc[VEOL2]    = 0;     /* '\0' */


	
	/*
	  now clean the modem line and activate the settings for the port
	*/
	tcflush(ser_fd, TCIOFLUSH);
	tcsetattr(ser_fd,TCSANOW,&newtio);
	
	return (ser_fd);
}


void sigTermHandler (int signum) {

	if (signal (SIGTERM, sigTermHandler) == SIG_IGN)
		signal (SIGTERM, SIG_IGN);
	if (signal (SIGINT, sigTermHandler) == SIG_IGN)
		signal (SIGINT, SIG_IGN);
	if (signal (SIGQUIT, sigTermHandler) == SIG_IGN)
		signal (SIGQUIT, SIG_IGN);

	tcsetattr(ser_fd,TCSANOW,&oldtio);
	close (ser_fd);

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
	
	return (baudid);
}

void usage() {
	fprintf(stderr,"./serial8N1 [-d /dev/ttyS0] [-b 9600] [-f]\n");
	fprintf(stderr,"Send input and output to RS232 device at specified baud in 8N1 format\n");
	fprintf(stderr,"  -d RS232 Device (in /dev directory)\n");
	fprintf(stderr,"  -b Baudrate. Supported baudrates:\n");
	fprintf(stderr,"     50, 75, 110, 134, 150, 200, 300, 600, 1200, 1800\n");
	fprintf(stderr,"     2400, 4800, 9600, 19200, 38400, 57600, 115200\n");
	fprintf(stderr,"  -i Input file or named pipe (created with mkfifo)\n");
	fprintf(stderr,"  -o Output file or pipe (created with mkfifo)\n");
	fprintf(stderr,"  -h Hardware flow control (Xon/Xoff not supported)\n");
	fprintf(stderr,"  -c Output newlines as CR-LF (default: raw output)\n");
	fprintf(stderr,"N.B.: The program will end 1 second after the input reaches EOF.\n");
	fprintf(stderr,"Using a named pipe for input prevents getting an EOF on intput.\n");
	fprintf(stderr,"Examples:\n");
	fprintf(stderr,"  Send and receive text interactively until ^D\n");
	fprintf(stderr,"    cat | /BBD9000/serial8N1 -d /dev/ttyAM1 -b 115200\n");
	fprintf(stderr,"  Sent a reset to the modem on /dev/tts/0, using hardware flow and NL/CRLF remapping\n");
	fprintf(stderr,"    echo \"ATZ\" | /BBD9000/serial8N1 -d /dev/tts/0 -b 115200 -h -c\n");
	fprintf(stderr,"\n");
	fprintf(stderr,"Author: Ilya Goldberg (igg at cathilya dot org)\n");
	fprintf(stderr,"License: GNU GPL\n");

	exit(EXIT_FAILURE);
}

