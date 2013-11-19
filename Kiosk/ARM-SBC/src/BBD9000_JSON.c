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
/* gcc -o BBD9000_JSON BBD9000_JSON.c */
#include <unistd.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <assert.h>
#include <strings.h>
#include <string.h>
#include <errno.h>



#include "BBD9000mem.h"

// Defining DEVEL allows the display and editing of much more information
// This is defined by the makefile
//#define DEVEL 1
//#undef DEVEL

/***************** start of the getcgivars() module **********************/

/*************************************************************************/
/**                                                                     **/
/**     getcgivars.c-- routine to read CGI input variables into an      **/
/**         array of strings.                                           **/
/**                                                                     **/
/**     Written in 1996 by James Marshall, james@jmarshall.com, except  **/
/**     that the x2c() and unescape_url() routines were lifted directly **/
/**     from NCSA's sample program util.c, packaged with their HTTPD.   **/
/**                                                                     **/
/**     For the latest, see http://www.jmarshall.com/easy/cgi/ .        **/
/**                                                                     **/
/*************************************************************************/


/** Convert a two-char hex string into the char it represents. **/
char x2c(char *what) {
   register char digit;

   digit = (what[0] >= 'A' ? ((what[0] & 0xdf) - 'A')+10 : (what[0] - '0'));
   digit *= 16;
   digit += (what[1] >= 'A' ? ((what[1] & 0xdf) - 'A')+10 : (what[1] - '0'));
   return(digit);
}

/** Reduce any %xx escape sequences to the characters they represent. **/
void unescape_url(char *url) {
    register int i,j;

    for(i=0,j=0; url[j]; ++i,++j) {
        if((url[i] = url[j]) == '%') {
            url[i] = x2c(&url[j+1]) ;
            j+= 2 ;
        }
    }
    url[i] = '\0' ;
}


/** Read the CGI input and place all name/val pairs into list.        **/
/** Returns list containing name1, value1, name2, value2, ... , NULL  **/
/* IGG: Modified to return NULL if there are any errors (no output to browser) */
char **getcgivars() {
    register int i ;
    char *request_method ;
    int content_length;
    char *cgiinput ;
    char **cgivars;
    char **pairlist ;
    int paircount ;
    char *nvpair ;
    char *eqpos ;


    /** Depending on the request method, read all CGI input into cgiinput. **/
    request_method= getenv("REQUEST_METHOD") ;
    if (!request_method) return (NULL);
    

    if (!strcmp(request_method, "GET") || !strcmp(request_method, "HEAD") ) {
        /* Some servers apparently don't provide QUERY_STRING if it's empty, */
        /*   so avoid strdup()'ing a NULL pointer here.                      */
        char *qs ;
        qs= getenv("QUERY_STRING") ;
        cgiinput= strdup(qs  ? qs  : "") ;
    }
    else if (!strcmp(request_method, "POST")) {
        /* strcasecmp() is not supported in Windows-- use strcmpi() instead */
        if ( strcasecmp(getenv("CONTENT_TYPE"), "application/x-www-form-urlencoded")) return (NULL);
        if ( !(content_length = atoi(getenv("CONTENT_LENGTH"))) ) return (NULL);
        if ( !(cgiinput= (char *) malloc(content_length+1)) ) return (NULL);
        if (!fread(cgiinput, content_length, 1, stdin)) {
        	free (cgiinput);
			return (NULL);
        }
        cgiinput[content_length]='\0' ;
    }
    else {
    	return (NULL);
    }

    /** Change all plusses back to spaces. **/
    for (i=0; cgiinput[i]; i++) if (cgiinput[i] == '+') cgiinput[i] = ' ' ;

    /** First, split on "&" and ";" to extract the name-value pairs into **/
    /**   pairlist.                                                      **/
    pairlist= (char **) malloc(256*sizeof(char **)) ;
    paircount= 0 ;
    nvpair= strtok(cgiinput, "&;") ;
    while (nvpair) {
        pairlist[paircount++]= strdup(nvpair) ;
        if (!(paircount%256))
            pairlist= (char **) realloc(pairlist,(paircount+256)*sizeof(char **)) ;
        nvpair= strtok(NULL, "&;") ;
    }
    pairlist[paircount]= 0 ;    /* terminate the list with NULL */

    /** Then, from the list of pairs, extract the names and values. **/
    cgivars= (char **) malloc((paircount*2+1)*sizeof(char **)) ;
    for (i= 0; i<paircount; i++) {
        if ( (eqpos=strchr(pairlist[i], '=')) ) {
            *eqpos= '\0' ;
            unescape_url(cgivars[i*2+1]= strdup(eqpos+1)) ;
        } else {
            unescape_url(cgivars[i*2+1]= strdup("")) ;
        }
        unescape_url(cgivars[i*2]= strdup(pairlist[i])) ;
    }
    cgivars[paircount*2]= 0 ;   /* terminate the list with NULL */
    
    /** Free anything that needs to be freed. **/
    free(cgiinput) ;
    for (i=0; pairlist[i]; i++) free(pairlist[i]) ;
    free(pairlist) ;

    /** Return the list of name-value strings. **/
    return cgivars ;
    
}

/***************** end of the getcgivars() module ********************/



int main (int argc, const char **argv) {
const char *BBD9000MEMpath;
BBD9000mem *shmem;
int shmem_fd;
char line[256],*chp,cpuRead=0;
FILE *uptime_fp, *BBD9000OUT_fp;
size_t n_char=255;
float cpu1,cpu5,cpu15;
struct timeval t_now;

#ifdef DEVEL
char **cgivars ;
int i=0;
#endif


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

	/* determine our CPU load by calling uptime */
	uptime_fp = popen ("/usr/bin/uptime","r");
	while (!feof(uptime_fp)) {
		fgets (line, n_char, uptime_fp);
		chp = strrchr(line, ':' );
		if (chp && sscanf (chp,":%f,%f,%f",&cpu1,&cpu5,&cpu15) == 3) cpuRead=1;
		strcpy (line,"");
	}
	pclose(uptime_fp);

	/* Success */


	/* Get our CGI vars if any */
	// Disabled in non-DEVEL version
    BBD9000OUT_fp = fopen (shmem->BBD9000out,"w");
#ifdef DEVEL
    if ( (cgivars = getcgivars()) && (cgivars[0]) ) {
		for (i=0; cgivars[i]; i+= 2) {
			if ( !strcmp (cgivars[i],"avail_gallons") ) {
				shmem->avail_gallons = atof (cgivars[i+1]);
			} else if ( !strcmp (cgivars[i],"LightsRly") ) {
				fprintf (BBD9000OUT_fp,"%s\t%s\n",cgivars[i],cgivars[i+1]);
			} else if ( !strcmp (cgivars[i],"StrikeRly") ) {
				fprintf (BBD9000OUT_fp,"%s\t%s\n",cgivars[i],cgivars[i+1]);
			} else if ( !strcmp (cgivars[i],"PumpRly") ) {
				fprintf (BBD9000OUT_fp,"%s\t%s\n",cgivars[i],cgivars[i+1]);
			} else if ( !strcmp (cgivars[i],"AuxRly") ) {
				fprintf (BBD9000OUT_fp,"%s\t%s\n",cgivars[i],cgivars[i+1]);
			}
		}

	}
		
#endif
//		fprintf (BBD9000OUT_fp,"PMP-THR\n");
//		fprintf (BBD9000OUT_fp,"ICAL\n");
//		fprintf (BBD9000OUT_fp,"IRAW\n");
		fprintf (BBD9000OUT_fp,"PMP\n");

//		fprintf (BBD9000OUT_fp,"VIN-THR\n");
//		fprintf (BBD9000OUT_fp,"VCAL\n");
//		fprintf (BBD9000OUT_fp,"VRAW\n");
		fprintf (BBD9000OUT_fp,"VIN\n");

		fflush (BBD9000OUT_fp);
		fclose (BBD9000OUT_fp);
		usleep (40000);

	// Without CGI variables, we send the standard response
	printf ("Content-Type: %s\r\n\r\n","text/plain");

	/* Our shared memory object in JSON */
	printf ("{\n");
	printf ("\"BBD9000ID\":%lu,\n", (unsigned long)shmem->kiosk_id);
	/* In fine JS tradition, times are in milliseconds since the epoch */
	printf ("\"StartTime\":%llu,\n",
		(unsigned long long)shmem->t_start.tv_sec*1000 + (shmem->t_start.tv_usec/1000) );
	gettimeofday(&t_now, NULL);
	printf ("\"SystemTime\":%llu,\n",(unsigned long long)t_now.tv_sec*1000 + (t_now.tv_usec/1000) );
	printf ("\"UpdateTime\":%llu,\n",
		(unsigned long long)shmem->t_update.tv_sec*1000 + (shmem->t_update.tv_usec/1000) );
	printf ("\"TwilightStart\":%llu,\n",
		(unsigned long long)shmem->twilight_start*1000);
	printf ("\"TwilightEnd\":%llu,\n",
		(unsigned long long)shmem->twilight_end*1000);
	if (cpuRead) {
		printf ("\"CPU_Load1\":%d,\n\"CPU_Load5\":%d,\n\"CPU_Load15\":%d,\n",
			(int)(cpu1*100.0),(int)(cpu5*100.0),(int)(cpu15*100));
	}

	printf ("\"Status\":\"%s\",\n",shmem->status);
	printf ("\"avail_gallons\":\"%.1f\",\n",shmem->avail_gallons);
#ifdef DEVEL
	// In DEVEL mode, the MSR and keypad buffers are shown
	printf ("\"Keypad\":\"%s\",\n",shmem->keypad_buffer);
	printf ("\"CC_Name\":\"%s\",\n",shmem->msr_CCname);
#endif
#ifndef DEVEL
	// In non-DEVEL mode, the buffer is either stars or blank
	if (*(shmem->keypad_buffer))
		printf ("\"Keypad\":\"**********\",\n");
	else
		printf ("\"Keypad\":\"\",\n");
	if (*(shmem->msr_CCname)) {
		printf ("\"CC_Name\":\"**********\",\n");
	} else {
		printf ("\"CC_Name\":\"\",\n");
	}
#endif
	printf ("\"LCD1\":\"%s\",\n",shmem->LCD1);
	printf ("\"LCD2\":\"%s\",\n",shmem->LCD2);

	printf ("\"Motion\":%d,\n",shmem->motion);
	printf ("\"Door\":%d,\n",shmem->door_open);
	printf ("\"LightsRly\":%d,\n",shmem->LightsRly);
	printf ("\"StrikeRly\":%d,\n",shmem->StrikeRly);
	printf ("\"PumpRly\":%d,\n",shmem->PumpRly);
	printf ("\"AuxRly\":%d,\n",shmem->AuxRly);

	printf ("\"Temp_C\":%3.2f,\n\"Temp_F\":%3.2f,\n", shmem->temp_c,shmem->temp_f);
	printf ("\"Vin\":%.2f,\n",shmem->voltage);
	printf ("\"Current\":%.1f,\n",shmem->current);
#ifdef DEVEL
// None of this unless in DEVEL mode

	printf ("\"Events\":[\n");
	for (i=0; shmem->event_queue[i].name[0]; i++) {
		printf ("\t{\"Name\":\"%s\",\"Value\":\"%.5s\",\"Time\":%llu}",
			shmem->event_queue[i].name,shmem->event_queue[i].value,
			(unsigned long long)shmem->event_queue[i].time.tv_sec*1000 + (shmem->event_queue[i].time.tv_usec/1000)
		);
		if (shmem->event_queue[i+1].name[0]) printf (",\n");
		else printf ("\n");
	}
	printf ("]\n");
#endif
	printf ("}\n");
	

	munmap(shmem, SHMEM_SIZE);
	close (shmem_fd);
	return (0);

}

