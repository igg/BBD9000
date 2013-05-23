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

#include <openssl/rsa.h>
#include <openssl/sha.h>
#include <openssl/pem.h>
#include <openssl/err.h>

#include <curl/curl.h>
#include <curl/easy.h>

// for res_init()
#include <netinet/in.h>
#include <arpa/nameser.h>
#include <resolv.h>

#include "BBD9000mem.h"
#include "BBD9000cfg.h"


#define ENCRYPT_ERROR  -1
#define SIGN_ERROR     -2
#define POST_ERROR     -3
#define OFFLINE_ERROR  -4
#define TIMEOUT_ERROR  -5
#define DNS_ERROR      -6
#define DECRYPT_ERROR  -7
#define VERIFY_ERROR   -8
#define MESSAGE_SUCCESS 1


#define DEBUGGING 1
#define MSG_PADDING 24 // reserved to add time string at the end


typedef struct {
	CURL *curl;

	char *volatile_msgs;
	unsigned long  volatile_msgs_size;

	char *message;
	unsigned long msg_size;

	char buffer[BIG_BUFFER_SIZE];
	unsigned long buf_size;

	char crypt_buffer[BIG_BUFFER_SIZE+((BIG_BUFFER_SIZE/RSA_BLOCK_SIZE)*(RSA_BLOCK_SIZE-RSA_TEXT_SIZE))+RSA_BLOCK_SIZE];
	unsigned long crypt_buf_size;
	unsigned long crypt_buf_read;
	

	char crypt_resp_buffer[BIG_BUFFER_SIZE+((BIG_BUFFER_SIZE/RSA_BLOCK_SIZE)*(RSA_BLOCK_SIZE-RSA_TEXT_SIZE))+RSA_BLOCK_SIZE];
	unsigned long crypt_resp_buf_size;

	char resp[BIG_BUFFER_SIZE];
	unsigned long resp_size;

	char *line_ptr, *eol_ptr;
	RSA *my_priv_key, *serv_pub_key;
	int saved_state;
	char first_message;
	time_t last_adj_s_time;  // first adjustment after clock set&running
	time_t last_adj_d_time;	 // corresponds to last_adj_s_time
} selfStruct;

// FUNCTION PROTOTYPES
size_t writeBuffer (void* ptr, size_t size, size_t nmemb, selfStruct* self);
int sendMessage (selfStruct* self);
void reshuffle_message (selfStruct *self) ;
void process_message (selfStruct *self);
void process_resp (selfStruct *self);
void checkDNS ();
void logMessage (FILE *log_fp, const char *template, ...);
void sigTermHandler (int signum);
void sigPwrHandler (int signum);
void saveState ();
void updateSaveState ();
int user_hz ();


// Message response handlers
void do_auth (selfStruct* self);
int do_notices (selfStruct* self);
void push_notice (selfStruct* self);
int checkTime (selfStruct* self);
void do_msg (selfStruct* self);
void do_GW (selfStruct* self) ;
void do_time (selfStruct* self);

// GLOBALS
BBD9000mem *shmem;
FILE *evt_fp;
FILE *log_fp;
#define SERVER_STATE_FILE "BBD9000server.save"
char state_file[PATH_SIZE];
selfStruct self_glob;


size_t readBuffer( void *ptr, size_t size, size_t nmemb, selfStruct* self) {
size_t read_size;
read_size = nmemb*size;

	if (self->crypt_buf_read+read_size > self->crypt_buf_size)
		read_size = self->crypt_buf_size - self->crypt_buf_read;

	if (read_size)
		memcpy(ptr, self->crypt_buffer+self->crypt_buf_read, read_size);

	self->crypt_buf_read += read_size;
	return (read_size);
}

size_t writeBuffer (void* ptr, size_t size, size_t nmemb, selfStruct* self) {
size_t write_size;
write_size = nmemb*size;

	if (self->crypt_resp_buf_size + write_size > sizeof (self->crypt_resp_buffer)) return 0;

	memcpy(self->crypt_resp_buffer+self->crypt_resp_buf_size, ptr, write_size);
	self->crypt_resp_buf_size += write_size;
	return (write_size);
}


void initCurl (selfStruct *self) {
char tmp_str[STR_SIZE];
struct curl_slist* responseHeaders = NULL ;


	// initialize the selfStruct
	memset (self,0,sizeof(selfStruct));
	self->message = self->buffer;

	// cURL initialization
	curl_global_init(CURL_GLOBAL_ALL);
	self->curl = curl_easy_init();
	assert(self->curl != NULL);

	sprintf (tmp_str,"cURL on BBD9000-%04u",shmem->kiosk_id);
	curl_easy_setopt(self->curl, CURLOPT_USERAGENT, tmp_str);
	
	sprintf (tmp_str,"%s?kioskID=%d",shmem->server_URL,shmem->kiosk_id);
    curl_easy_setopt(self->curl, CURLOPT_URL, tmp_str);
	logMessage (log_fp,"Init URL:%s",tmp_str);

	// Block Expect header for lighthttp 1.4
	responseHeaders = curl_slist_append(responseHeaders, "Expect:");

	curl_easy_setopt (self->curl, CURLOPT_TIMEOUT, shmem->network_timeout);
	curl_easy_setopt (self->curl, CURLOPT_DNS_CACHE_TIMEOUT, (long)(60*60*12)); // 12 hrs
	curl_easy_setopt (self->curl, CURLOPT_WRITEFUNCTION, writeBuffer);
	curl_easy_setopt (self->curl, CURLOPT_WRITEDATA, self);

	// PUT method reader function
	curl_easy_setopt(self->curl, CURLOPT_UPLOAD, 1L);
	curl_easy_setopt(self->curl, CURLOPT_PUT, 1L);
	curl_easy_setopt (self->curl, CURLOPT_READFUNCTION, readBuffer);
	curl_easy_setopt (self->curl, CURLOPT_READDATA, self);
	// PUT method content-type
	responseHeaders = curl_slist_append(responseHeaders, "Content-Type: application/octet-stream");

	// Set the HTTP headers configured above
	curl_easy_setopt(self->curl, CURLOPT_HTTPHEADER, responseHeaders);
}

int sendMessage (selfStruct* self) {
unsigned char *msg_ptr, *message;
char *crypt_ptr;
unsigned char *hash;
long flen,msg_left,enc_len,msg_size;
unsigned int sig_len;
unsigned long nblocks,blocknum;
//struct curl_httppost *post=NULL;
//struct curl_httppost *last=NULL;
long server_resp,result_code;
char null_msg[RSA_BLOCK_SIZE];
int pt_len;
int retries=3;
int ret_code=POST_ERROR;
int netlock_fd;

	
	// If this is the first message since startup, we send a null message to establish server coms
	// and synchronize the clocks.
	if (self->first_message) {
		memset (null_msg,0,sizeof(null_msg));
		message = null_msg;
		msg_size = 0;
	} else {
		message = self->buffer;
		// The regular message always points at the buffer.
		// Its length is determined by wether or not we have auth or checkin messages
		msg_size = self->buf_size;
	}

	// message length
	flen = msg_size;
	
	// insert the date pad
	flen += sprintf (message+msg_size,"%llu\n",(unsigned long long)(time(NULL)));

	// process the message in blocks
	// N.B.: terminating null is not encoded
	msg_left = flen;
	msg_ptr = (unsigned char *)message;
	crypt_ptr = self->crypt_buffer;
	self->crypt_buf_size = 0;
	while (msg_left > 0) {
		enc_len = msg_left;
		if (enc_len > RSA_TEXT_SIZE) enc_len = RSA_TEXT_SIZE;
		if (RSA_public_encrypt(enc_len, msg_ptr, crypt_ptr, self->serv_pub_key, RSA_PKCS1_OAEP_PADDING) < 0) {
			*(message+msg_size) = '\0';
			strcpy (shmem->net_error,"Encrypting");
			return (ENCRYPT_ERROR);
		}
		msg_left -= enc_len;
		msg_ptr += enc_len;
		crypt_ptr += RSA_BLOCK_SIZE;
		self->crypt_buf_size += RSA_BLOCK_SIZE;
	}

	// hash of the plain text
	// last parameter NULL places it in a static array.
	hash = SHA1( (unsigned char *)message, flen, NULL);

	// NULL-terminate the message at the place we inserted the date pad
	*(message+msg_size) = '\0';

	// sign the hash with our private key
	if (RSA_sign(NID_sha1, hash, SHA_DIGEST_LENGTH, crypt_ptr, &sig_len, self->my_priv_key) < 0) {
		strcpy (shmem->net_error,"Signing");
		return (SIGN_ERROR);
	}
	self->crypt_buf_size += RSA_BLOCK_SIZE;


// 	sprintf (post_var,"%u",shmem->kiosk_id);
// 	curl_formadd(&post, &last,
// 		CURLFORM_COPYNAME, "kioskID",
// 		CURLFORM_COPYCONTENTS, post_var,
// 		CURLFORM_END);
	
	// get the base64-encoded cryptext
// 	curl_formadd(&post, &last,
// 		CURLFORM_COPYNAME, "message",
// 		CURLFORM_PTRCONTENTS, self->crypt_buffer,
// 		CURLFORM_CONTENTSLENGTH, self->crypt_buf_size,
// 		CURLFORM_END);


	// POST and get the response
//    curl_easy_setopt(self->curl, CURLOPT_HTTPPOST, post);
//logMessage (log_fp,"Request size :%lu",self->crypt_buf_size);
	curl_easy_setopt(self->curl, CURLOPT_INFILESIZE_LARGE, (curl_off_t)self->crypt_buf_size);


	netlock_fd = netlock (shmem); // block until we get a network lock
	if (netlock_fd < 0) {// network is offline
		// try to start the network
		if (shmem->fsmSync) {
			fprintf (evt_fp,"Net Status\tConnecting\n");
			fflush (evt_fp);
		}
		netlock_fd = netlink (shmem,"start", 1);
		if (netlock_fd < 0) {
			logMessage (log_fp,"netlock - start failed: %d",netlock_fd);
			if (shmem->fsmSync) {
				fprintf (evt_fp,"Net Status\tRe-connecting\n");
				fflush (evt_fp);
			}
		// try a restart - this will power-cycle the modem
			netlock_fd = netlink (shmem,"restart", 1);
			// give up for now
			if (netlock_fd < 0) {
				logMessage (log_fp,"netlock - restart failed: %d",netlock_fd);
				strcpy (shmem->net_error,"offline");
				ret_code = OFFLINE_ERROR;
				retries = 0;
			} else {
				logMessage (log_fp,"netlock - restart succeeded");
				if (shmem->fsmSync) {
					fprintf (evt_fp,"Net Status\tConnected\n");
					fflush (evt_fp);
				}
			}
		} else {
			if (shmem->fsmSync) {
				fprintf (evt_fp,"Net Status\tConnected\n");
				fflush (evt_fp);
			}
		}
	}

    while (retries) {

		checkDNS();

		self->crypt_resp_buf_size = 0;
		self->crypt_buf_read = 0;
		result_code = curl_easy_perform (self->curl);
		netunlock (shmem, netlock_fd); // Release the network lock

		curl_easy_getinfo(self->curl, CURLINFO_RESPONSE_CODE, &server_resp);

		// Process the server response code
		if (result_code == CURLE_OPERATION_TIMEDOUT) {
			strcpy (shmem->net_error,"timeout");
			ret_code = TIMEOUT_ERROR; // set up return code in case we run out of retries
		} else if  (result_code == CURLE_COULDNT_RESOLVE_HOST) {// DNS servers reset?
			strcpy (shmem->net_error,"DNS");
			ret_code = DNS_ERROR; // set up return code in case we run out of retries
		} else if  ( !(result_code == CURLE_OK && server_resp == 200) ) {
			if (result_code != CURLE_OK) sprintf (shmem->net_error,"client %ld",result_code);
			else sprintf (shmem->net_error,"server %ld",server_resp);
			ret_code = POST_ERROR; // set up return code in case we run out of retries
			break; // We give up if its a server error
		} else if (self->crypt_resp_buf_size < RSA_BLOCK_SIZE*2) {
			sprintf (shmem->net_error,"server %ld",server_resp);
			ret_code = POST_ERROR; // set up return code in case we run out of retries
			break; // We give up if its a server error
		} else {
			ret_code = MESSAGE_SUCCESS;
			break;
		}
		retries--;
		// restart the network - we should have broken out if we're OK.
#ifdef DEBUGGING
{
char *curlURL=NULL;
curl_easy_getinfo(self->curl, CURLINFO_EFFECTIVE_URL , &curlURL);
logMessage (log_fp,"retries %d; libcurl code: %d, URL:%s",retries,(int)result_code,curlURL);
}
#endif
		if (ret_code != MESSAGE_SUCCESS && retries) {
			if (shmem->fsmSync) {
				fprintf (evt_fp,"Net Status\tRe-connecting\n");
				fflush (evt_fp);
			}
			netlock_fd = netlink (shmem,"restart", 1);
			// give up if we can't do it
			if (netlock_fd < 0) {
				logMessage (log_fp,"Retry %d. netlock - restart failed: %d",retries,netlock_fd);
				strcpy (shmem->net_error,"offline");
				ret_code = OFFLINE_ERROR;
				break;
			} else {
				logMessage (log_fp,"Retry %d. netlock restart succeeded", retries);
				if (shmem->fsmSync) {
					fprintf (evt_fp,"Net Status\tConnected\n");
					fflush (evt_fp);
				}
			}
		}
	}
// #ifdef DEBUGGING
// {
// long numConnects;
// curl_easy_getinfo(self->curl, CURLINFO_NUM_CONNECTS , &numConnects);
// logMessage (log_fp,"%ld new connections",numConnects);
// }
// #endif
//	curl_formfree (post);
 	

	if (ret_code != MESSAGE_SUCCESS) {
		return (ret_code);
	}


// decode the response, which consists of the crypted message followed by the signature

	self->resp_size = 0;
	*(self->resp+self->resp_size) = '\0';
	blocknum = 0;
	nblocks = self->crypt_resp_buf_size / RSA_BLOCK_SIZE; // last block is the signature
//logMessage (log_fp,"crypt resp size: %lu bytes, %lu blocks",self->crypt_resp_buf_size,nblocks);
	while (blocknum < nblocks) {
		// decrypt if there is more to read.
		// RSA_private_decrypt returns the length of the decrypted block.
		if (blocknum < nblocks-1) {
			if ( (pt_len = RSA_private_decrypt(RSA_BLOCK_SIZE, 
				self->crypt_resp_buffer + (blocknum*RSA_BLOCK_SIZE),
				self->resp+self->resp_size,
				self->my_priv_key, RSA_PKCS1_OAEP_PADDING)) < 0) {
					strcpy (shmem->net_error,"Decryption");
					logMessage (log_fp,"Decrypt failed:");
#ifdef DEBUGGING
ERR_print_errors_fp (log_fp);
fflush(log_fp);
#endif
					return (DECRYPT_ERROR);
			}
			self->resp_size += pt_len;
			blocknum++;
		// If there is nothing left to read, then its the signature block.
		} else {
			// NULL-terminate the response (this is not part of the signature though)
			*(self->resp+self->resp_size) = '\0';
			// make a hash of the plaintext
			hash = SHA1(self->resp,self->resp_size, NULL);
			if (RSA_verify(NID_sha1, hash, SHA_DIGEST_LENGTH,
				self->crypt_resp_buffer + (blocknum*RSA_BLOCK_SIZE), RSA_BLOCK_SIZE, self->serv_pub_key) != 1) {
				// failed
					strcpy (shmem->net_error,"Serv. verify");
					logMessage (log_fp,"Verify failed:");
#ifdef DEBUGGING
ERR_print_errors_fp (log_fp);
fflush(log_fp);
#endif
					self->resp_size = 0;
					*(self->resp) = '\0';
					return (VERIFY_ERROR);
			}
			blocknum++;
		}
	}

// Verified	

	self->first_message = 0;
	*(shmem->net_error) = '\0';
	shmem->last_status = time (NULL);
	return (MESSAGE_SUCCESS);	

}

int main () {
selfStruct *self;
FILE *key_fp, *state_fp, *fifo_fp;
int shmem_fd;
int fifo_fd;
fd_set rfds;
long nread;
struct timeval wait_timeval, *wait_timeval_p;
int rv=0;

	/* vvvv Init */

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


	// Open the log
	log_fp = fopen (shmem->BBD9000LOG,"a");
	assert(log_fp != NULL);
	logMessage (log_fp,"BBD9000server started");

	self = &self_glob;
	memset (self,0,sizeof(self_glob));

	/* clear our self structure */
	initCurl( self );

	// Set up our private key and the server public key
	key_fp = fopen (shmem->BBD9000key,"r");
	assert (key_fp != NULL);
	self->my_priv_key = PEM_read_RSAPrivateKey(key_fp, NULL, NULL, NULL);
	assert (self->my_priv_key != NULL);
	assert (RSA_check_key(self->my_priv_key));
	fclose (key_fp);
	logMessage (log_fp,"Read kiosk key");

	key_fp = fopen (shmem->BBD9000srv_key,"r");
	assert (key_fp != NULL);
	self->serv_pub_key = PEM_read_RSA_PUBKEY(key_fp, NULL, NULL, NULL);
	assert (self->serv_pub_key != NULL);
	// Oddly enough, this causes a Bus Error in libopenssl.  Nice.
	//assert (RSA_check_key(self->serv_pub_key));
	fclose (key_fp);
	logMessage (log_fp,"Read server key");


	/* open the event FIFO */
	evt_fp = fopen (BBD9000EVT,"w");
	assert(evt_fp != NULL);

	/* ^^^^ Init */

	// Register our signal handler to save state
	signal (SIGTERM    , sigTermHandler );
	signal (SIGINT     , sigTermHandler );
	signal (SIGQUIT    , sigTermHandler );
	signal (SIGPIPE    , sigTermHandler );
	signal (SIGPWRALRM , sigPwrHandler  );     /* SIGUSR1. Voltage ALRM on BBD9000 SIGPWRALRM */
	signal (SIGPWROK   , sigPwrHandler  );     /* SIGUSR2. Voltage ALRM RESET on BBD9000 SIGPWROK */
	
	// Set the first_message flag so that the first message sent on startup is a null message
	// for a clock resync.
	self->first_message = 1;
	// Send a null message to the server
	process_message ( self );

	// load any stale messages in the saved state file
	// These will not be sent until first_message is cleared
	snprintf (state_file,sizeof (state_file),"%s/%s",shmem->root_path,SERVER_STATE_FILE);
	state_fp = fopen (state_file,"r");
	if (state_fp) {
		self->buf_size = fread (self->buffer, 1, BIG_BUFFER_SIZE, state_fp);
		fclose (state_fp);
		unlink (state_file);
		self->message[self->buf_size]='\0';
		self->msg_size = self->buf_size;
	}

	// Open the server FIFO
	// We're opening it read/write so that there is always a writer
	// and select() doesn't end up giving the fd a stuck EOF when a writer disconnects and reconnects
	// That's a weird thing that happens with FIFOs that doesn't happen with regular files.
	// They can have an EOF that magically goes away without us doing anything.
	fifo_fd = open(BBD9000srv, O_RDWR);
	assert (fifo_fd > -1);
	fifo_fp = fopen(BBD9000srv, "r+");
	assert (fifo_fp != NULL);
	logMessage (log_fp,"Reading FIFO...");

	// We're ready to accept messages
	shmem->serverSync = 1;

	while (1) {
		if (self->buf_size && self->buffer[self->buf_size-1] == '\n') {
			process_message ( self );
		}
		// Set the timeout based on msg_size and server state
		if (self->buf_size && self->buffer[self->buf_size-1] == '\n') {
			// Note that a non-zero-length message with server set to true is impossible
			wait_timeval.tv_sec = (int) (shmem->status_interval_no_net / 1000);
			wait_timeval.tv_usec = (int) (shmem->status_interval_no_net * 1000);
			wait_timeval_p = &wait_timeval;
		} else {
			wait_timeval_p = NULL;
		}

		// Read the FIFO with a timeout
		// If the message is empty, the timeout is infinite.
		FD_ZERO(&rfds);
		FD_SET(fifo_fd, &rfds);
		rv = select(fifo_fd+1, &rfds, NULL, NULL, wait_timeval_p);
		if (rv > 0 && FD_ISSET(fifo_fd, &rfds)) {
			// Read the FIFO, appending at the end of any existing message.
			nread = read(fifo_fd, self->buffer+self->buf_size, BIG_BUFFER_SIZE-self->buf_size-MSG_PADDING);
			if (nread < 0) {
				logMessage (log_fp,"reading fifo: %d",nread);
			} else if (nread == 0) {
				logMessage (log_fp,"got EOF");
				close (fifo_fd);
				fifo_fd = open(BBD9000srv, O_RDWR);
			} else {
				// set terminating NULL
				self->buf_size += nread;
				self->msg_size += nread;
				*(self->buffer + self->buf_size) = '\0';

				// Messages always end in a newline.
				if ( self->buffer[self->buf_size-1] == '\n' ) {
					 // don't save volatile messages, so reshuffle right away - not in process_message()
					reshuffle_message (self);
					updateSaveState();
				} else {
					logMessage (log_fp,"buffer doesn't end in NL: [%s]",self->buffer);
				}
			}
		}
	}

	munmap(shmem, SHMEM_SIZE);
	close (shmem_fd);
	
	close (fifo_fd);
	self->resp_size = 0;
	*(self->resp) = '\0';
	
	exit(EXIT_SUCCESS);
}

void reshuffle_message (selfStruct *self) {
char *line_p,*msg_p, *last_p, buf_c;
unsigned long msg_size;
char net_msg[32];
int lock_fp;

	// status and auth messages go to the back of the buffer to volatile_msgs
	self->volatile_msgs = self->buffer+self->buf_size;
	self->volatile_msgs_size = 0;
	self->msg_size = 0;


	// buffer start is line start
	line_p = self->buffer;
	last_p = self->buffer + self->buf_size;
//logMessage (log_fp,"buffer in: [%s]",self->buffer);
	while (line_p < last_p) {
//logMessage (log_fp,"line in: [%.32s]",line_p);
		if ( !strncmp (line_p, "status",6) || !strncmp (line_p, "auth",4) || !strncmp (line_p, "GW",2)
			|| !strncmp (line_p, "net\t",4) ) {
			// find the end of the message
			msg_p = line_p;
			buf_c = *msg_p;
			while (buf_c && buf_c != '\n') buf_c = *msg_p++;
			msg_size = msg_p - line_p; // including the terminating NULL or newline
			// A terminating NULL without newline at this point would be a real bummer
			if ( *msg_p && *(msg_p+1) ) { // not already at the end of the buffer
				// Copy the message to the end of the buffer & terminate it
				strncpy (self->volatile_msgs+self->volatile_msgs_size, line_p, msg_size);
				*(self->volatile_msgs+self->volatile_msgs_size+msg_size) = '\0';

				// Move the buffer up
				// Amount to move is total amount minus what we already processed
				memmove(line_p, msg_p, self->buf_size - (line_p - self->buffer)); // memmove for overlapping bytes
				*(self->buffer+self->buf_size) = '\0';
			}
			self->volatile_msgs -= msg_size;

			// adjust the volatile message size
			self->volatile_msgs_size += msg_size;
			// know when to stop processing lines
			last_p -= msg_size;

			// Set msg_p to point at the new location
			msg_p = self->volatile_msgs+self->volatile_msgs_size-msg_size;
// {
// char temp_c[1024];
// logMessage (log_fp,"buffer to NULL: [%s]",self->buffer);
// snprintf (temp_c,self->buf_size+1,self->buffer);
// logMessage (log_fp,"buffer (%d): [%s]",self->buf_size,temp_c);
// snprintf (temp_c,self->msg_size+1,self->message);
// logMessage (log_fp,"message (%d): [%s]",self->msg_size,temp_c);
//logMessage (log_fp,"volatile message to NULL: [%s]",self->volatile_msgs);
// snprintf (temp_c,self->volatile_msgs_size+1,self->volatile_msgs);
// logMessage (log_fp,"volatile message (%d): [%s]",self->volatile_msgs_size,temp_c);
// }
// logMessage (log_fp,"isolated message : [%s]",msg_p);

			// These are so volatile, they're not even sent to the server
			if (!strncmp (msg_p, "net\t",4)) {
				sscanf (msg_p,"%*[^\t]\t%[^\n]",net_msg);
				memset (msg_p,0,msg_size);
				self->buf_size -= msg_size;
				self->volatile_msgs_size -= msg_size;
				lock_fp = netlink (shmem, net_msg, 1);
				netunlock (shmem,lock_fp);
				if (shmem->fsmSync) {
					fprintf (evt_fp,"Net Status\tUpdate\n");
					fflush (evt_fp);
				}
			} else if (!strncmp (msg_p, "foo\t",4)) {
				memset (msg_p,0,msg_size);
				self->buf_size -= msg_size;
				self->volatile_msgs_size -= msg_size;
			}
		} else {
			// Get to the next line
			msg_p = line_p++;
			buf_c = *msg_p;
			while (buf_c && buf_c != '\n') buf_c = *msg_p++;
			self->msg_size += (msg_p - line_p)+1;
			line_p = msg_p;
		}
	}

// A single volatile message results in a duplicate.
// FIXME: test
//{
//char temp_c[1024];

//logMessage (log_fp,"final buffer buf_size: %d msg_size: %d, volatile_msgs_size: %d to NULL: [%s]",
//self->buf_size, self->msg_size, self->volatile_msgs_size, self->buffer);

// snprintf (temp_c,self->buf_size+1,self->buffer);
// logMessage (log_fp,"final buffer (%d): [%s]",self->buf_size,temp_c);
//snprintf (temp_c,self->msg_size+1,self->message);
//logMessage (log_fp,"final message (%d): [%s]",self->msg_size,temp_c);
// logMessage (log_fp,"final volatile message to NULL: [%s]",self->volatile_msgs);
//snprintf (temp_c,self->volatile_msgs_size+1,self->volatile_msgs);
//logMessage (log_fp,"final volatile message (%d): [%s]",self->volatile_msgs_size,temp_c);
// logMessage (log_fp,"---------------------------");
//}
// *(self->buffer) = '\0';
// self->buf_size = 0;
// self->msg_size = 0;
// self->volatile_msgs_size = 0;
// FIXME: test
}


void process_message (selfStruct *self) {
int ret;

//	logMessage (log_fp,"Message: [%.16s...] %d bytes",self->buffer,self->buf_size);

	// Send the whole message block to the server
	// We keep calling this until the message buffer is empty
	do  {
		ret = sendMessage (self);

		if (ret == MESSAGE_SUCCESS) {
			process_resp ( self );
			updateSaveState();
			if (!shmem->server) {
				shmem->server = 1;
				if (shmem->fsmSync) {
					fprintf (evt_fp,"Server OK\n");
					fflush (evt_fp);
				}
			}
		} else {
			// We ditch the status/auth messages once we've sent them or failed
			if (self->volatile_msgs_size) *(self->volatile_msgs) = '\0';
			self->buf_size = self->msg_size;
			self->volatile_msgs_size = 0;
		}
		
	} while (ret == MESSAGE_SUCCESS && self->buf_size > 0);


	if (ret != MESSAGE_SUCCESS) { // fail!
		if (ret == TIMEOUT_ERROR) {
			shmem->server = 0;
			if (shmem->fsmSync) {
				fprintf (evt_fp,"Server Timeout\n");
				fflush (evt_fp);
			}
		} else {
		// Some other kind of problem
			shmem->server = 0;
			if (shmem->fsmSync) {
				fprintf (evt_fp,"Server Error\t%d\n",ret);
				fflush (evt_fp);
			}
		}
	} else {
	// Successfully processed entire block
	
	// Send a Patch event if we have any patches
	// We only set the number of patches once we finished processing them all
		if ( *(shmem->patches[0].path) ) {
			int i=0;
			while (i < MAX_PATCHES && shmem->patches[i].path[0]) i++;
			shmem->npatches = i;
			if (shmem->fsmSync) {
				fprintf (evt_fp,"Patch\n");
				fflush (evt_fp);
			}
		}
	}
}

// This processes the raw server response, which is line-based
// This clears the message buffer, so the server comms must have been
// successful before calling this.
// N.B.:
// If the kiosk message had an auth or other message requiring a response,
// and the server response was successful, but does not contain an auth response,
// Then this is an error on the server's part *** THAT WILL NOT BE CAUGHT BY THIS CODE !!! ***
void process_resp (selfStruct *self) {
char *line_ptr, *eol_ptr, *resp;
long resp_size;

	// Check to see if the message has non-notice lines.
	// If so, we must have a response from the server matching the type
	// It is an error for this response line not to exist in the server response
	

	resp = self->resp;
	resp_size = self->resp_size;
	line_ptr = self->resp;
	eol_ptr = strchr (line_ptr,'\n');
	if (!eol_ptr) eol_ptr = resp + resp_size;
	self->line_ptr = line_ptr;
	self->eol_ptr = eol_ptr;

	// Catch a resync response, which means we have to resend everything
	if (!strncmp (line_ptr,"resync\t",7)) {
		do_time (self);
		return;
	}

	// Clear out the message buffer
	*(self->message) = '\0';
	self->msg_size = 0;
	self->buf_size = 0;
	if (self->volatile_msgs_size) *(self->volatile_msgs) = '\0';
	self->volatile_msgs_size = 0;

	while (*line_ptr) {
		eol_ptr = strchr (line_ptr,'\n');
		if (!eol_ptr) eol_ptr = resp + resp_size;

		// Parse a response line
		self->line_ptr = line_ptr;
		self->eol_ptr = eol_ptr;
		if (!strncmp (line_ptr,"auth\t",5)) {
			do_auth (self);
		} else if (!strncmp (line_ptr,"msg\t",4)) {
			do_msg (self);
		} else if (!strncmp (line_ptr,"GW ",3)) {
			do_GW (self);
		} else if (!strncmp (line_ptr,"time\t",5)) {
			do_time (self);
		}

		// Go to the next line or end
		if (*eol_ptr == '\n') line_ptr = eol_ptr+1;
		else line_ptr = eol_ptr;
	}

}

/*
  These are response handlers
  The message is sent to the server either way - if something special is to
  be done with the response, then implement a handler for it below
*/

void do_auth (selfStruct *self) {
char type[MEMB_INFO_LNGTH],status[MEMB_INFO_LNGTH],number[MEMB_INFO_LNGTH],reason[MEMB_INFO_LNGTH],GW_string[STR_SIZE];
float ppg=0,credit=0,preauth=0,renewal=0,membership=0,trial_surcharge=0,upgrade_fee=0;
int id;
int nscanned=0;

	if ( strncmp (self->line_ptr,"auth\t",5) ) return;

	memset (type,0,MEMB_INFO_LNGTH);
	memset (status,0,MEMB_INFO_LNGTH);
	memset (number,0,MEMB_INFO_LNGTH);
	memset (shmem->memb_type,0,MEMB_INFO_LNGTH);
	memset (shmem->memb_status,0,MEMB_INFO_LNGTH);
	memset (shmem->memb_number,0,MEMB_INFO_LNGTH);
	shmem->memb_ppg = 0;
	shmem->memb_credit = 0;
	shmem->memb_fuel_pre_auth = 0;
	shmem->renewal_fee = 0;
	shmem->trial_membership_surcharge = 0;
	shmem->upgrade_fee = 0;
	
//logMessage (log_fp,"line: [%s]",self->line_ptr);
	if ( !strncmp (self->line_ptr+5,"Failed",6) ) {
		nscanned = sscanf (self->line_ptr,"%*s%*s%s%f%f%*[ \t]%[^\n]",reason,&membership,&trial_surcharge,GW_string);
//logMessage (log_fp,"failed auth: nscanned: %d, membership: %.2f, trial: %.2f, GW: %s",nscanned,membership,trial_surcharge,GW_string);
		if (nscanned == 4) {
			shmem->full_membership_fee = membership;
			shmem->trial_membership_surcharge = trial_surcharge;
			strcpy (shmem->GW_string,GW_string);
			if (! strncmp (GW_string,"Server GW",9)) shmem->server_GW = 1;
		}
		if (shmem->fsmSync) {
			fprintf (evt_fp,"Authentication Failed\t%s\n",reason);
			fflush (evt_fp);
		}
	} else {
		nscanned = sscanf (self->line_ptr,"%*s%d%s%s%f%f%f%s%f%f%f%*[ \t]%[^\n]",&id,type,status,&ppg,&preauth,&credit,number,
			&renewal,&trial_surcharge,&upgrade_fee,GW_string);
		if (nscanned == 11 || nscanned == 7) {
			shmem->memb_id = id;
			strcpy (shmem->memb_type,type);
			strcpy (shmem->memb_status,status);
			strcpy (shmem->memb_number,number);
			shmem->memb_ppg = ppg;
			
			preauth *=  ppg;
			// Round up to a multiple of 5
			// preauth = preauth / 5.0 > (int)preauth / 5.0 ? ( 1+(int)preauth / 5 ) * 5 : preauth;
			// Should maybe be just up to nearest $1?
			preauth = preauth > (int)preauth ? 1+(int)preauth : (int)preauth;
			// Set the pre-auth only if we're authenticating
			// If we are registering a new user, then the pre-auth
			// is already set with the CC company, so keep it.
			// The pre-auth from the server is a fuel pre-auth in gallons
			if (shmem->memb_fuel_pre_auth <= 0) {
				shmem->memb_fuel_pre_auth = (int) preauth;
			}
			shmem->memb_credit = credit;
			if (nscanned == 11) {
				shmem->renewal_fee = renewal;
				shmem->trial_membership_surcharge = trial_surcharge;
				shmem->upgrade_fee = upgrade_fee;
				strcpy (shmem->GW_string,GW_string);
				if (! strncmp (GW_string,"Server GW",9)) shmem->server_GW = 1;
				else shmem->server_GW = 0;
			}
			if (!strncmp (status,"EXPIRED",7) ) {
				if (shmem->fsmSync) {
					fprintf (evt_fp,"Membership Expired\n");
				}
			} else {
				if (shmem->fsmSync) {
					fprintf (evt_fp,"Authentication Success\n");
				}
			}
			if (shmem->fsmSync) {
				fflush (evt_fp);
			}
		} else {
			if (shmem->fsmSync) {
				fprintf (evt_fp,"Authentication Failed\n");
				fflush (evt_fp);
			}
		}
	}
}



void do_msg (selfStruct* self) {
char key[STR_SIZE],val[STR_SIZE];
unsigned int msg_id;
long nbytes;
int i,ret,nscanned;
long t_start, t_end;
float avail_gallons, last_ppg;
char fuel_type[8];

	if ( strncmp (self->line_ptr,"msg\t",4) ) return;
	if (! self->resp) return;
	
	// Server response with messages:
	// msg msg_id key val
	sscanf (self->line_ptr,"msg\t%u\t%s\t%[^\n]",&msg_id,key,val);
	
	if (!strcmp (key,"fuel")) {
	// Server sends $fuel_avail,$fuel_type,$last_ppg
		nscanned = sscanf (val,"%f%s%f",&avail_gallons,fuel_type,&last_ppg);
		if (nscanned == 3) {
			if ( fabs (avail_gallons - shmem->avail_gallons) > 0.001 ) {
				shmem->avail_gallons = avail_gallons;
				if (shmem->fsmSync) {
					fprintf (evt_fp,"Fuel Level\t%.3f\n",shmem->avail_gallons);
					fflush (evt_fp);
				}
			}
			strncpy (shmem->fuel_type,fuel_type,7);
			shmem->last_ppg = last_ppg;
		}
	} else if (!strcmp (key,"add_gallons")) {
		shmem->avail_gallons += atof (val);
		if (shmem->fsmSync) {
			fprintf (evt_fp,"Fuel Level\t%.3f\n",shmem->avail_gallons);
			fflush (evt_fp);
		}
	} else if (!strcmp (key,"patch")) {
		// find the first empty patch
		i=0;
		while (i < MAX_PATCHES && shmem->patches[i].path[0]) i++;
		logMessage (log_fp,"npatches: %d",i);
		if (i < MAX_PATCHES) {
			ret = sscanf (val,"%[^\t]\t%[^\t]\t%[^\n]",shmem->patches[i].path,shmem->patches[i].srcMD5,shmem->patches[i].dstMD5);
			logMessage (log_fp,"patch (ret=%d): %s\t%s\t%s",ret,shmem->patches[i].path,shmem->patches[i].srcMD5,shmem->patches[i].dstMD5);

			if (ret != 3) return;
		} else {
			return;
		}
	} else if (!strcmp (key,"maint")) {
		nscanned = sscanf (val,"%ld%ld",&t_start,&t_end);
		if (nscanned == 2) {
			shmem->sched_maint_start = t_start;
			shmem->sched_maint_end = t_end;
		}
	} else if (!strcmp (key,"operator_code")) {
		strcpy (shmem->operator_code,val);
		cal_cfg_write (shmem);
	}
	// Send the ack message.
	// If we get a fault during the ack, then this message
	// may come back to us again.
	// If we wait for a successful ack before doing the modification,
	// then a fault during the ack may mean that the message was
	// deleted from the server but we did not process it due to a bad ack.
	// Repeated messages are considered to be more benign that dropped ones,
	// so we do it at the end like so.
	nbytes = snprintf (self->buffer+self->buf_size,BIG_BUFFER_SIZE-self->buf_size,
		"notice\tack\t%u\n",msg_id);
	if (nbytes < BIG_BUFFER_SIZE-self->buf_size) self->buf_size += nbytes;
	else self->buf_size = BIG_BUFFER_SIZE-1;

	return;
}


void do_GW (selfStruct* self) {
cc_resp_s t_resp;
char gw_status[CC_RESP_CODE_SIZE],gw_code[CC_RESP_CODE_SIZE],gw_reas_code[CC_RESP_CODE_SIZE];
int nscanned;

/* From server:
		$GW_resp .= " Declined\t";
		$GW_resp .= join ("\t",$code,$reason_code,$gw_trans_id,$auth_code,$gw_message);
		Busy, Timeout and Error may not have GW info
*/
	if (! strncmp (self->line_ptr,"GW ",3) ) {
		memset (gw_code, 0, sizeof (gw_code));
		memset (gw_reas_code, 0, sizeof (gw_code));
		memset (&t_resp, 0, sizeof (t_resp));

		nscanned = sscanf (self->line_ptr+3,"%[^\t\n]\t%[^\t\n]\t%[^\t\n]\t%[^\t\n]\t%[^\t\n]\t%[^\t\n]\t%[^\n]",
			gw_status, gw_code, gw_reas_code, t_resp.trans_id, t_resp.auth_code, t_resp.message, t_resp.MD5_hash);
//logMessage (log_fp,"nscanned: %d, gw_status [%s] gw_code [%s] gw_reas_code [%s] trans_id [%s], auth_code [%s], message [%s] MD5_hash [%s]",
//nscanned, gw_status, gw_code, gw_reas_code, t_resp.trans_id, t_resp.auth_code, t_resp.message, t_resp.MD5_hash);
		if (nscanned == 7) {
			t_resp.code = (char) atoi (gw_code);
			t_resp.reas_code = (char) atoi (gw_reas_code);
		} 
		if (t_resp.code) shmem->cc_resp.code = t_resp.code;
		if (t_resp.reas_code) shmem->cc_resp.reas_code = t_resp.reas_code;
		memset (shmem->cc_resp.message,0,sizeof (t_resp.message));
		strcpy (shmem->cc_resp.message,t_resp.message);
		memset (shmem->cc_resp.MD5_hash,0,sizeof (t_resp.MD5_hash));
		strcpy (shmem->cc_resp.MD5_hash,t_resp.MD5_hash);
	
		// Copy the transaction ID and auth code only if code == 1.
		if (t_resp.code == 1) {
			if (strlen (t_resp.auth_code) > 0) {
				memset (shmem->cc_resp.auth_code,0,sizeof (t_resp.auth_code));
				strcpy (shmem->cc_resp.auth_code,t_resp.auth_code);
			}
			
			if (strlen (t_resp.trans_id) > 0) {
				memset (shmem->cc_resp.trans_id,0,sizeof (t_resp.trans_id));
				strcpy (shmem->cc_resp.trans_id,t_resp.trans_id);
			}
		}
		if (shmem->fsmSync) {
			fprintf (evt_fp,"GW %s\n",gw_status);
			fflush (evt_fp);
		}
	}
}


void do_time (selfStruct* self) {
int s_time=0;
time_t d_time, k_time;
struct timeval t_now;
int is_time=0;
float ppm_drift, kern_ppm, freq_adj;
FILE *time_drift_fp;
struct timex kern_timex;
long hz, tick_adj;

	if (! strncmp (self->line_ptr,"time\t",5) ) {
		is_time = sscanf (self->line_ptr,"time\t%u",&s_time);
	} else if (! strncmp (self->line_ptr,"resync\t",7) ) {
		is_time = sscanf (self->line_ptr,"resync\t%u",&s_time);
	}
	
	if (! is_time || ! s_time) return;
	k_time = time (NULL);
	d_time = s_time - k_time;
//logMessage (log_fp,"server time delta: %d",(int)d_time);
	if (s_time > 0 && ABS(d_time) > 5) {
		gettimeofday (&t_now, NULL);
		t_now.tv_sec = s_time;
		settimeofday (&t_now, NULL);
		shmem->delta_t = d_time;
		logMessage (log_fp,"Change time by delta: %ds",(int)d_time);
		if (shmem->fsmSync) {
			fprintf (evt_fp,"Change Time\t%d\n",shmem->delta_t);
			fflush (evt_fp);
		}

		// Initially, last_adj_s_time is 0, so it doesn't count toward the drift
		// The first time, the offset can be due to being shutdown without an RTC
		// After the first time, we're measuring the system clock's drift.
		// The clock may already have a drift compensation, so this is relative drift.
		// which must be added to an existing drift.
		if (self->last_adj_s_time) {
			ppm_drift = (float)d_time / (float)(s_time - self->last_adj_s_time);
			ppm_drift *= 1000000.0;

			// Read the current frequency offset
			kern_timex.modes = 0;
			adjtimex(&kern_timex);
			kern_ppm = (float)kern_timex.freq  / 65536.0;
			
			// adjust the number of ticks, as well as ppm drift.
			hz = user_hz();
//logMessage (log_fp,"user HZ: %d",hz);

			if (hz < 1) hz = 100;
			tick_adj = 0;

			tick_adj = ceil((ppm_drift + kern_ppm - hz)/hz);
			freq_adj = (ppm_drift - tick_adj*hz);

// 			kern_ppm += ppm_drift;
// 			if (kern_ppm > hz)
// 				tick_adj = -(-ppm_drift + hz/2)/hz;
// 			else if (kern_ppm < -hz)
// 				tick_adj = (ppm_drift + hz/2)/hz;
// 			kern_ppm += tick_adj*hz;

			kern_timex.freq += (long) ( (freq_adj * 65536.0) + 0.5 );
			kern_timex.tick += tick_adj;

			logMessage (log_fp,"New relative drift: %.3f ppm. tick adj: %d, freq adj: %.3f, new tick: %d, new freq: %d (%.3f ppm)",
				ppm_drift,tick_adj,freq_adj,kern_timex.tick,kern_timex.freq,(float)kern_timex.freq/65536.0);

			// write the ticks and frequency for the next startup
			time_drift_fp = fopen (BBD9000timeDrift,"w+"); // create/truncate
			if (time_drift_fp) {
				fprintf (time_drift_fp,"/sbin/adjtimex -f %ld -t %ld\n",kern_timex.freq,kern_timex.tick);
				fclose (time_drift_fp);
			}
			chmod (BBD9000timeDrift,0755);
			// send them to the kernel
			kern_timex.modes = ADJ_FREQUENCY | ADJ_TICK;
			adjtimex(&kern_timex);
		}

		self->last_adj_s_time = s_time;
		self->last_adj_d_time = d_time;  // we're not really using this for anything
	}
}

void checkDNS () {
static time_t st_mtime_last=0;
struct stat resolvStat;

	if ( stat ("/etc/resolv.conf", &resolvStat) == 0 ) {
		if (resolvStat.st_mtime > st_mtime_last) {
			res_init();
			st_mtime_last = resolvStat.st_mtime;
		}
	}

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


void sigTermHandler (int signum) {

	saveState();
	exit(EXIT_SUCCESS);
}

void sigPwrHandler (int signum) {

	// Save state
	if (signum == SIGPWRALRM) {
		saveState();
	} else if (signum == SIGPWROK) {
	// Restore state
	// If we're still alive, then no state to restore - we just unlink the state file.
		unlink (state_file);
		self_glob.saved_state = 0;
	}
}

void saveState () {

	self_glob.saved_state = 1;
	updateSaveState();
}

void updateSaveState () {
FILE *state_fp;

//	reshuffle_message (&self_glob);
//	shmem->server_msg_size = self_glob.msg_size;

	if (! self_glob.saved_state) return;
	
	if (! self_glob.msg_size) {
		unlink (state_file);
	} else {
		state_fp = fopen (state_file,"w");
		if (state_fp) {
			fwrite (self_glob.message, 1, self_glob.msg_size, state_fp);
			fclose (state_fp);
		}
	}
}

int user_hz () {
FILE *stat, *uptime;
unsigned long ticks;
float secs;

// determine USER_HZ
// From man proc(5):
// /proc/stat
// cpu 3357 0 4313 1362393
//  "The 4th value should be USER_HZ times the second entry in /proc/uptime."
	if (! (stat = fopen ("/proc/stat","r")) ) {
		return (-1);
	}
	if (! (uptime = fopen ("/proc/uptime","r")) ) {
		fclose (stat);
		return (-1);
	}
	fscanf (stat,"cpu %*u %*u %*u %lu",&ticks);
	fscanf (uptime,"%*f %f",&secs);
	fclose (stat);
	fclose (uptime);
	
	if (secs > 10.0 && ticks > 10) {
		return ( (int) ( ((float) ticks / secs) + 0.5 ) );
	} else {
		return (-1);
	}	
	
}


