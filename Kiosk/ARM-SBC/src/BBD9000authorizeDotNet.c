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
#include <fcntl.h>
#include <assert.h>
#include <string.h>
#include <errno.h>
#include <signal.h>

#include <curl/curl.h>
#include <curl/easy.h>

// for res_init()
#include <netinet/in.h>
#include <arpa/nameser.h>
#include <resolv.h>

#include <confuse.h> // for reading configuration file

#include "BBD9000mem.h"
#include "BBD9000cfg.h"


// Stuff for cURL
#include <curl/curl.h>
#include <curl/easy.h>
#define POST_VAR_SIZE 1024
#define MAX_RESP_SIZE 1024

#define ENCRYPT_ERROR  -1
#define SIGN_ERROR     -2
#define POST_ERROR     -3
#define OFFLINE_ERROR  -4
#define TIMEOUT_ERROR  -5
#define DNS_ERROR      -6
#define DECRYPT_ERROR  -7
#define VERIFY_ERROR   -8
#define BAD_PARAM      -9
#define BUSY_ERROR    -10

#define MESSAGE_SUCCESS 1


#define MESSAGE_SUCCESS 1
#define AUTH_NET_INFO_SIZE 32

typedef struct {
	CURL* curl;
	char post [POST_VAR_SIZE];
	char response [MAX_RESP_SIZE];
	char response_msg [NAME_SIZE];
	size_t resp_len;
	size_t resp_capacity;

	// Authorize.net account information
	char  CC_URL[STR_SIZE];
	char  old_CC_URL[STR_SIZE];
	char  x_cpversion[STR_SIZE];
	char  x_login[STR_SIZE];
	char  x_market_type[STR_SIZE];
	char  x_device_type[STR_SIZE];
	char  x_tran_key[STR_SIZE];

} srvrStruct;

size_t writeBuffer (void* ptr, size_t size, size_t nmemb, srvrStruct* self);
// End of cURL stuff

#define DEBUGGING 1

// FUNCTION PROTOTYPES
void initPostString (srvrStruct* self);
void catPreAuth (srvrStruct* self, float amount);
void catCapture (srvrStruct* self, float amount);
void catAuthCapture (srvrStruct* self, float amount);
void catVoid (srvrStruct* self);
void catItemList (srvrStruct* self);
void initCurl (srvrStruct *self);
int sendCC_POST (srvrStruct *self);
void parseCCresp (char *text, cc_resp_s *resp);
int isCode (int *codes,int in_code);
void doResp (srvrStruct *self, int ret);
void parse_GWstring (srvrStruct *self);
void clear_GWinfo (srvrStruct *self);
void logMessage (const char *template, ...);
void checkDNS ();


// high-level handlers:
int doPreAuth (srvrStruct *self, char *message);
int doCapture (srvrStruct *self, char *message);
int doAuthCapture (srvrStruct *self, char *message);
int doVoid (srvrStruct *self, char *message);

// GLOBALS
BBD9000mem *shmem;
FILE *log_fp;
FILE *evt_fp;



size_t writeBuffer (void* ptr, size_t size, size_t nmemb, srvrStruct* self) {
size_t write_size;
write_size = nmemb*size;

	if ( write_size + self->resp_len > self->resp_capacity) {
		return 0;
	}
	memcpy(self->response+self->resp_len, ptr, write_size);
	self->resp_len += write_size;
	*( (char *)(self->response+write_size) ) = '\0';
	
	return nmemb;
}

/*
  This initializes the post string with values common to all transactions
*/
void initPostString (srvrStruct* self) {

// Write out our transaction string
// Note that we're not url-escaping anything

	parse_GWstring (self);

	snprintf (self->post,sizeof(self->post),
		"x_cpversion=%s&x_market_type=%s&x_device_type=%s"
		"&x_response_format=1&x_delim_char=|"
		"&x_login=%s&x_tran_key=%s"
		"&x_cust_id=%s",
			self->x_cpversion,self->x_market_type,self->x_device_type,
			self->x_login,self->x_tran_key,
			shmem->memb_number
	);
}



/*
  Add on stuff to do a AUTH_ONLY transaction
*/
void catPreAuth (srvrStruct* self, float amount) {
char string[POST_VAR_SIZE];

	if ( *(shmem->msr_track1) ) {
		snprintf (string,sizeof(string),
			"&x_track1=%s",shmem->msr_track1);
	} else if ( *(shmem->msr_track2) ) {
		snprintf (string,sizeof(string),
			"&x_track2=%s",shmem->msr_track2);
	}
	strcat (self->post,string);

	snprintf (string,sizeof(string),
		"&x_type=AUTH_ONLY"
		"&x_amount=%.2f",
		amount);
	
	strcat (self->post,string);

}

/*
  Add stuff to do a PRIOR_AUTH_CAPTURE transaction
*/
void catCapture (srvrStruct* self, float amount) {
char string[POST_VAR_SIZE];

	snprintf (string,sizeof(string),
		"&x_type=PRIOR_AUTH_CAPTURE"
		"&x_ref_trans_id=%s"
		"&x_amount=%.2f",
		shmem->cc_resp.trans_id,
		amount);
	
	strcat (self->post,string);

}

/*
  Add stuff to do a AUTH_CAPTURE transaction
*/
void catAuthCapture (srvrStruct* self, float amount) {
char string[POST_VAR_SIZE];

	if ( *(shmem->msr_track1) ) {
		snprintf (string,sizeof(string),
			"&x_track1=%s",shmem->msr_track1);
	} else if ( *(shmem->msr_track2) ) {
		snprintf (string,sizeof(string),
			"&x_track2=%s",shmem->msr_track2);
	}
	strcat (self->post,string);

	snprintf (string,sizeof(string),
		"&x_type=AUTH_CAPTURE"
		"&x_amount=%.2f",amount);
	strcat (self->post,string);
}

/*
  Add stuff to do a VOID transaction
  We don't send an amount with a VOID
*/
void catVoid (srvrStruct* self) {
char string[POST_VAR_SIZE];

	snprintf (string,sizeof(string),
		"&x_type=VOID"
		"&x_ref_trans_id=%s",
		shmem->cc_resp.trans_id
	);
	
	strcat (self->post,string);
}

/*
  Adds a '&' followed by a properly formatted list of items based on things in shmem
*/
void catItemList (srvrStruct* self) {
	char item_string[POST_VAR_SIZE];
	int item_num;

// Write out itemized list
// x_line_item=item2<|>golf bag<|>Wilson golf carry bag, red<|>1<|>39.99<|>Y
	item_num = 1;
	if (shmem->memb_gallons > 0.0) {
		snprintf (item_string,sizeof(item_string),
			"&x_line_item=item%d<|>Fuel<|><|>%.3f<|>%.2f<|>NO",
			item_num,shmem->memb_gallons,shmem->memb_ppg);
		item_num++;
		strcat (self->post,item_string);
	}
	if (shmem->memb_renewal_sale) {
		snprintf (item_string,sizeof(item_string),
			"&x_line_item=item%d<|>Membership renewal<|><|>1<|>%.2f<|>NO",
			item_num,shmem->renewal_fee);
		item_num++;
		strcat (self->post,item_string);
	}
	if (shmem->memb_upgrade_sale) {
		snprintf (item_string,sizeof(item_string),
			"&x_line_item=item%d<|>Membership upgrade<|><|>1<|>%.2f<|>NO",
			item_num,shmem->upgrade_fee);
		item_num++;
		strcat (self->post,item_string);
	}
// Authorize.net's QuickBooks report likes to have credit reported as negative.
// What are the chances this is consistent with any other combination of software?
// Important update:  I should have looked more carefully at auth.net's card-present
// manual, where it says these have to be positive numbers.
// N.B.:  CC transactions are rejected if the amounts or quantities are negative!
// Does auth.net add them up to match the total?  They don't say either way - may in the future.
	if (shmem->memb_credit > 0) {
		snprintf (item_string,sizeof(item_string),
			"&x_line_item=item%d<|>Credit<|><|>1<|>%.2f<|>NO",
			item_num,shmem->memb_credit);
		item_num++;
		strcat (self->post,item_string);
	}
	if (shmem->memb_credit < 0) {
		snprintf (item_string,sizeof(item_string),
			"&x_line_item=item%d<|>Debt<|><|>1<|>%.2f<|>NO",
			item_num,-(shmem->memb_credit));
		item_num++;
		strcat (self->post,item_string);
	}
}


void initCurl (srvrStruct *self) {


	// cURL initialization
	self->curl = curl_easy_init();
	assert(self->curl != NULL);

	curl_easy_setopt (self->curl, CURLOPT_USERAGENT, "cURL on BBD9000");
	
	curl_easy_setopt (self->curl, CURLOPT_TIMEOUT, shmem->network_timeout);
	curl_easy_setopt (self->curl, CURLOPT_DNS_CACHE_TIMEOUT, (long)(60*60*2)); // 2 hrs
	 
	curl_easy_setopt (self->curl, CURLOPT_WRITEFUNCTION, writeBuffer);
	curl_easy_setopt (self->curl, CURLOPT_WRITEDATA, self);

}

int sendCC_POST (srvrStruct *self) {
	int result_code;
	long server_resp;
	int retries=3;
	int ret_code=POST_ERROR;
	int netlock_fd;

	// clear-out the response info
	memset (self->response,0,sizeof(self->response));
	self->resp_len = 0;
	
	curl_easy_setopt(self->curl, CURLOPT_POSTFIELDS, self->post);

	netlock_fd = netlock (shmem); // block until we get a network lock
	if (netlock_fd < 0) {// network is offline
		// try to start the network
		fprintf (evt_fp,"Server Status\tConnecting\n");
		fflush (evt_fp);
		netlock_fd = netlink (shmem,"start", 1);
		if (netlock_fd < 0) {
			fprintf (evt_fp,"Server Status\tRe-connecting\n");
			fflush (evt_fp);
		// try a restart - this will power-cycle the modem
			netlock_fd = netlink (shmem,"restart", 1);
			// give up for now
			if (netlock_fd < 0) {
				strcpy (shmem->net_error,"offline");
				ret_code = OFFLINE_ERROR;
				retries = 0;
			} else {
				fprintf (evt_fp,"Server Status\tConnected\n");
				fflush (evt_fp);
			}
		} else {
			fprintf (evt_fp,"Server Status\tConnected\n");
			fflush (evt_fp);
		}
	}


	while (retries) {
    	checkDNS();

#ifdef DEBUGGING
logMessage ("retries %d; To [%s]",retries,self->CC_URL);
#endif

		result_code = curl_easy_perform (self->curl);
		netunlock (shmem, netlock_fd); // Release the network lock

		curl_easy_getinfo(self->curl, CURLINFO_RESPONSE_CODE, &server_resp);
#ifdef DEBUGGING
logMessage ("result_code: %d, server_resp: %ld",result_code,server_resp);
#endif
	 
		if (result_code == CURLE_OPERATION_TIMEDOUT) {
			strcpy (shmem->gw_error,"timeout");
			ret_code = TIMEOUT_ERROR; // set up return code in case we run out of retries
		} else if  (result_code == CURLE_COULDNT_RESOLVE_HOST) {// DNS servers reset?
			strcpy (shmem->gw_error,"DNS");
			ret_code = DNS_ERROR; // set up return code in case we run out of retries
		} else if  ( server_resp == 503 ) {// auth.net sends this if its busy
			ret_code = BUSY_ERROR; // set up return code in case we run out of retries
			fprintf (evt_fp,"Server Status\tGW Busy\n");
			fflush (evt_fp);
			sleep (5);
		} else if  ( !(result_code == CURLE_OK && server_resp == 200) ) {
			if (result_code != CURLE_OK) sprintf (shmem->gw_error,"client %d",result_code);
			else sprintf (shmem->gw_error,"server %ld",server_resp);
			ret_code = POST_ERROR; // set up return code in case we run out of retries
		} else {
			parseCCresp (self->response,&(shmem->cc_resp));

#ifdef DEBUGGING
//	logMessage ("Response: [%s]",self->response);
//	logMessage ("Response code: [%d]",shmem->cc_resp.code);
//	logMessage ("Reason code: [%d]",shmem->cc_resp.reas_code);
//	logMessage ("Message: [%s]",shmem->cc_resp.message);
//	logMessage ("Auth code: [%s]",shmem->cc_resp.auth_code);
//	logMessage ("Trans ID: [%s]",shmem->cc_resp.trans_id);
//	logMessage ("MD5 hash: [%s]",shmem->cc_resp.MD5_hash);
#endif
			return (MESSAGE_SUCCESS);
		}

		retries--;
		// restart the network
		if (ret_code != BUSY_ERROR && retries) {
			fprintf (evt_fp,"Server Status\tRe-connecting\n");
			fflush (evt_fp);
			netlock_fd = netlink (shmem,"restart", 1);
			// give up if we can't do it
			if (netlock_fd < 0) {
				strcpy (shmem->gw_error,"offline");
				ret_code = OFFLINE_ERROR;
				break;
			} else {
				fprintf (evt_fp,"Server Status\tConnected\n");
				fflush (evt_fp);
			}
		}
	}
	
	return (ret_code);

}


void parseCCresp (char *text, cc_resp_s *resp) {
char *cp=text,*fp;
char field[STR_SIZE];
cc_resp_s t_resp;

	memset (&t_resp,0,sizeof (cc_resp_s));

	while (1) {
	// Ignore up to the first |
		while (*cp != '|' && *cp) cp++;
		if (*cp != '|') break;
		cp++;
	// copy the response code field
		fp=field;
		while (*cp != '|' && *cp) *fp++ = *cp++;
		if (*cp != '|') break;
		*fp = '\0';
		t_resp.code = atoi (field);
	// copy the reason code field
		cp++;
		fp=field;
		while (*cp != '|' && *cp) *fp++ = *cp++;
		if (*cp != '|') break;
		*fp = '\0';
		t_resp.reas_code = atoi (field);
	// copy the message field
		cp++;
		fp=t_resp.message;
		while (*cp != '|' && *cp) *fp++ = *cp++;
		if (*cp != '|') break;
		*fp = '\0';
	// copy the auth_code field
		cp++;
		fp=t_resp.auth_code;
		while (*cp != '|' && *cp) *fp++ = *cp++;
		if (*cp != '|') break;
		*fp = '\0';
	// skip the AVS field
		cp++;
		while (*cp != '|' && *cp) cp++;
		if (*cp != '|') break;
		*fp = '\0';
	// skip the card code response field
		cp++;
		while (*cp != '|' && *cp) cp++;
		if (*cp != '|') break;
		*fp = '\0';
	// copy the transaction ID field
		cp++;
		fp=t_resp.trans_id;
		while (*cp != '|' && *cp) *fp++ = *cp++;
		if (*cp != '|') break;
		*fp = '\0';
	// copy the MD5 Hash field
		cp++;
		fp=t_resp.MD5_hash;
		while (*cp != '|' && *cp) *fp++ = *cp++;

		break;
	}
	
	if (t_resp.code) resp->code = t_resp.code;
	if (t_resp.reas_code) resp->reas_code = t_resp.reas_code;
	memset (resp->message,0,sizeof (resp->message));
	strcpy (resp->message,t_resp.message);
	memset (resp->MD5_hash,0,sizeof (resp->MD5_hash));
	strcpy (resp->MD5_hash,t_resp.MD5_hash);

	// Copy the transaction ID and auth code only if code == 1.
	if (t_resp.code == 1) {
		if (strlen (t_resp.auth_code) > 0) {
			memset (resp->auth_code,0,sizeof (resp->auth_code));
			strcpy (resp->auth_code,t_resp.auth_code);
		}
		
		if (strlen (t_resp.trans_id) > 0) {
			memset (resp->trans_id,0,sizeof (resp->trans_id));
			strcpy (resp->trans_id,t_resp.trans_id);
		}
	}
	
	return;

}


int doPreAuth (srvrStruct *self, char *message) {
float amount;
int ret;

	// Check we have the params we need
	if (!( *(shmem->msr_track1) || *(shmem->msr_track2) )) return BAD_PARAM;
	amount = (float) atof (message);
	if (amount < 0.0) return BAD_PARAM;

	// First try track 1 if it exists
	initPostString (self);
	catPreAuth (self, amount);
	ret = sendCC_POST (self);
	
	// If we get a MESSAGE_SUCCESS with a parsed
	// response containing a reason_code of 88, wipe out track1 and try again with track2
	if (ret == MESSAGE_SUCCESS && shmem->cc_resp.code == 3 && shmem->cc_resp.reas_code == 88
		&& *(shmem->msr_track2) ) {
			memset (shmem->msr_track1,0,sizeof(shmem->msr_track1));
			initPostString (self);
			catPreAuth (self, amount);
			ret = sendCC_POST (self);
	}
	
	return (ret);
}

int doCapture (srvrStruct *self, char *message) {
float amount;
int ret;


	// Check we have the params we need
	if (! *(shmem->cc_resp.trans_id) ) return BAD_PARAM;
	amount = (float) atof (message);
	if (amount < 0.0) return BAD_PARAM;

	// First try track 1 if it exists
	initPostString (self);
	catCapture (self, amount);
	catItemList (self);
	ret = sendCC_POST (self);
	
	// If we get a MESSAGE_SUCCESS with a parsed
	// response containing a reason_code of 88, wipe out track1 and try again with track2
	if (ret == MESSAGE_SUCCESS && shmem->cc_resp.code == 3 && shmem->cc_resp.reas_code == 88
		&& strlen (shmem->msr_track2) > 2) {
			memset (shmem->msr_track1,0,sizeof(shmem->msr_track1));
			initPostString (self);
			catCapture (self, amount);
			catItemList (self);
			ret = sendCC_POST (self);
	}
	
	return (ret);
}

int doAuthCapture (srvrStruct *self, char *message) {
float amount;
int ret;


	// Check we have the params we need
	if (!( *(shmem->msr_track1) || *(shmem->msr_track2) )) return BAD_PARAM;
	amount = (float) atof (message);
	if (amount < 0.0) return BAD_PARAM;

	// First try track 1 if it exists
	initPostString (self);
	catAuthCapture (self, amount);
	catItemList (self);
	ret = sendCC_POST (self);
	
	// If we get a MESSAGE_SUCCESS with a parsed
	// response containing a reason_code of 88, wipe out track1 and try again with track2
	if (ret == MESSAGE_SUCCESS && shmem->cc_resp.code == 3 && shmem->cc_resp.reas_code == 88
		&& strlen (shmem->msr_track2) > 2) {
			memset (shmem->msr_track1,0,sizeof(shmem->msr_track1));
			initPostString (self);
			catAuthCapture (self, amount);
			catItemList (self);
			ret = sendCC_POST (self);
	}
	
	return (ret);
}


int doVoid (srvrStruct *self, char *message) {
int ret;

	// Check we have the params we need
	if (! *(shmem->cc_resp.trans_id) ) return BAD_PARAM;

	// First try track 1 if it exists
	initPostString (self);
	catVoid (self);
	ret = sendCC_POST (self);
	
	// If we get a MESSAGE_SUCCESS with a parsed
	// response containing a reason_code of 88, wipe out track1 and try again with track2
	if (ret == MESSAGE_SUCCESS && shmem->cc_resp.code == 3 && shmem->cc_resp.reas_code == 88
		&& strlen (shmem->msr_track2) > 2) {
			memset (shmem->msr_track1,0,sizeof(shmem->msr_track1));
			initPostString (self);
			catVoid (self);
			ret = sendCC_POST (self);
	}
	
	return (ret);
}


int isCode (int *codes,int in_code) {
int *code;

	code = codes;
	while (*code && *code != in_code) {
		code++;
	}
	return (*code);
}


/*
  Process responses from the CC processor to send to the event FIFO as a string.
*/
void doResp (srvrStruct *self, int ret) {
// The response codes are 0-terminated
int busy_codes[] = {19,20,21,22,23,25,26,57,58,59,60,61,62,63,120,121,122,181,0};
int declined_codes[] = {36,49,0};
int type_codes[] = {17,28,0};
int lost_codes[] = {11,16,33,0}; // 11 is a duplicate transaction - the kiosk's transaction's been lost
int expired_codes[] = {8,0};

	switch (ret) {

	case MESSAGE_SUCCESS:
	// Accepted
		if (shmem->cc_resp.code == 1 && shmem->cc_resp.reas_code == 1) {
			strcpy (self->response_msg,"Accepted");
	// ISF - Insufficient funds - no way to know.  Generic "Declined" message
		} else if (shmem->cc_resp.code == 2) {
			strcpy (self->response_msg,"Declined");
		} else if (shmem->cc_resp.code == 3) {
	// CardTypeRejected
			if (isCode (type_codes,shmem->cc_resp.reas_code)) {
				strcpy (self->response_msg,"CCTypeRejected");
	// Declined
			} else if (isCode (declined_codes,shmem->cc_resp.reas_code)) {
				strcpy (self->response_msg,"Declined");
	// Trans ID not found
			} else if (isCode (lost_codes,shmem->cc_resp.reas_code)) {
				strcpy (self->response_msg,"TransLost");
	// Expired
			} else if (isCode (expired_codes,shmem->cc_resp.reas_code)) {
				strcpy (self->response_msg,"CardExpired");
	// Busy
			} else if (isCode (busy_codes,shmem->cc_resp.reas_code)) {
				strcpy (self->response_msg,"Busy");
	// ProcessorError - unparsed error
			} else {
				strcpy (self->response_msg,"Error");
			}
		} else {
	// ProcessorError - unparsed error
			strcpy (self->response_msg,"Error");
		}
	break;

// Busy
	case BUSY_ERROR:
		strcpy (self->response_msg,"Busy");
	break;

// NetworkTimeout
	case TIMEOUT_ERROR:
		strcpy (self->response_msg,"Timeout");
	break;

// Malformed request
	case BAD_PARAM:
		strcpy (self->response_msg,"Error");
	break;

// Generic cURL error
	default:
		strcpy (self->response_msg,"CURL Error");
	break;
	}
}

int main (int argc, const char **argv) {
const char *BBD9000MEMpath;
int ret;
FILE *fifo_fp=NULL;
char buf[MAX_RESP_SIZE], *read;

char resp[MAX_RESP_SIZE],type[MAX_RESP_SIZE],message[MAX_RESP_SIZE];
int shmem_fd;

srvrStruct self;




	/* vvvv Init */
	// This is a sub-process, so the shared memory segment must be provided as the first parameter
	if (argc < 2) {
		fprintf (stderr,"%s: path to shared memory segment is a required parameter\n", argv[0]);
		exit (-1);
	}
	BBD9000MEMpath = argv[1];

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

	/* open the event FIFO */
	evt_fp = fopen (shmem->BBD9000evt,"w");
	assert(evt_fp != NULL);
	
	// Open the log
	log_fp = fopen (shmem->BBD9000LOG,"a");
	assert(log_fp != NULL);
#ifdef DEBUGGING
	logMessage ("BBD9000authorizeDotNet started");
#endif

	// Ignoring SIGPIPEs.
	signal (SIGPIPE, SIG_IGN);
	// Ignoring power signals
	signal (SIGPWRALRM , SIG_IGN);     /* SIGUSR1. Voltage ALRM on BBD9000 SIGPWRALRM */
	signal (SIGPWROK   , SIG_IGN);     /* SIGUSR2. Voltage ALRM RESET on BBD9000 SIGPWROK */


	// initialize our self structure
	memset (&self,0,sizeof(self));
	self.resp_capacity = sizeof(self.response);

	// cURL initialization
	initCurl(&self);

	/* ^^^^ Init */
	while (1) {

		/* Open the pipe for reading - this blocks until a writer opens the file */
		if (!fifo_fp) fifo_fp = fopen(shmem->BBD9000ccg, "r");
		assert (fifo_fp != NULL);

		/* Read from the pipe - this blocks only if there are writers with the file open */
		read = fgets(buf, sizeof(buf), fifo_fp);
		while (read) {
			memset(type, 0, sizeof(type));
			memset(message, 0, sizeof(message));
			memset(resp, 0, sizeof(resp));
			sscanf (buf,"%[^\t]\t%[^\n]",type,message);
#ifdef DEBUGGING
logMessage ("Type: [%s] Message: [%s]",type,message);
#endif
			// Notices should go to the server in order, so if its a notice,
			// we push it to the end, then process the notice queue
			if ( !strcmp (type,"PreAuth") ) {
				ret = doPreAuth (&self,message);
			} else if ( !strcmp (type,"Capture") ) {
				ret = doCapture (&self,message);
			} else if ( !strcmp (type,"AuthCapture") ) {
				ret = doAuthCapture (&self,message);
			} else if ( !strcmp (type,"Void") ) {
				ret = doVoid (&self,message);
			} else {
				ret = BAD_PARAM;
			}
#ifdef DEBUGGING
logMessage ("Ret: [%d]",ret);
#endif

			doResp (&self,ret);
			
			// forget our account info
			clear_GWinfo (&self);
			
			fprintf (evt_fp,"GW %s\n",self.response_msg);
			fflush (evt_fp);


			read = fgets(buf, sizeof(buf), fifo_fp);
		} 
		// all writers exited and we're getting eofs
		fclose (fifo_fp);
		fifo_fp = NULL;
	}

	munmap(shmem, SHMEM_SIZE);
	close (shmem_fd);
	curl_easy_cleanup(self.curl);
	exit(EXIT_SUCCESS);
}


void parse_GWstring (srvrStruct *self) {


	strcpy (self->old_CC_URL,self->CC_URL);
	sscanf (shmem->GW_string,
		"%s%s%s%s%s%s",
		self->CC_URL,
		self->x_cpversion,
		self->x_login,
		self->x_market_type,
		self->x_device_type,
		self->x_tran_key
	);

	if (strcmp (self->old_CC_URL,self->CC_URL))
		curl_easy_setopt (self->curl, CURLOPT_URL, self->CC_URL);

}


void clear_GWinfo (srvrStruct *self) {

	memset(self->CC_URL, 0, sizeof(self->CC_URL));
	memset(self->x_cpversion, 0, sizeof(self->x_cpversion));
	memset(self->x_login, 0, sizeof(self->x_login));
	memset(self->x_market_type, 0, sizeof(self->x_market_type));
	memset(self->x_device_type, 0, sizeof(self->x_device_type));
	memset(self->x_tran_key, 0, sizeof(self->x_tran_key));
	// We can't clear the track info here because FSM may try again
}



void logMessage (const char *template, ...) {
va_list ap;
time_t t_now;
char buf[STR_SIZE];


	t_now = time(NULL);
	strftime (buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", localtime (&t_now));
	fprintf (log_fp,"gateway %s: ",buf);

	// process the printf-style parameters
	va_start (ap, template);
	vfprintf (log_fp, template, ap);
	va_end (ap);
	fprintf (log_fp,"\n");
	fflush (log_fp);
}



void checkDNS () {
static time_t st_mtime_last=0;
struct stat resolvStat;

	if ( stat ("/etc/resolv.conf", &resolvStat) == 0 ) {
		if (resolvStat.st_mtime > st_mtime_last) {
			res_init();
			st_mtime_last = resolvStat.st_mtime;
			logMessage("Called res_init\n");
		}
	}

}
