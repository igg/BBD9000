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
#include <avr/io.h>
#include <avr/pgmspace.h>
#include <avr/wdt.h>
#include <stdlib.h>
#include <ctype.h>
#include "parser.h"
#include "lcd.h"
#include "uart2.h"
#include "SmartIO.h"


//
// Commands and their handler functions
// Handler functions become read functions for UART0 once the command is identified
void do_AUX (unsigned char c) ;
const char AUX_name[] PROGMEM = "AUX";

void do_LCD1 (unsigned char c) ;
const char LCD1_name[] PROGMEM = "LCD1";

void do_LCD2 (unsigned char c) ;
const char LCD2_name[] PROGMEM = "LCD2";

void do_LCDBL (unsigned char c) ;
const char LCDBL_name[] PROGMEM = "LCDBL";

void do_RLY1 (unsigned char c) ;
const char RLY1_name[] PROGMEM = "RLY1";

void do_LGHT (unsigned char c) ;
const char LGHT_name[] PROGMEM = "LGHT";

void do_FLM_CUR (unsigned char c) ;
const char FLM_CUR_name[] PROGMEM = "FLM-CUR";

void do_FLM_MS (unsigned char c) ;
const char FLM_MS_name[] PROGMEM = "FLM-MS";

void do_FLM_TOT (unsigned char c) ;
const char FLM_TOT_name[] PROGMEM = "FLM-TOT";

void do_STK (unsigned char c) ;
const char STK_name[] PROGMEM = "STK";

void do_VIN (unsigned char c) ;
const char VIN_name[] PROGMEM = "VIN";

void do_VIN_THR (unsigned char c) ;
const char VIN_THR_name[] PROGMEM = "VIN-THR";

void do_VRAW (unsigned char c) ;
const char VRAW_name[] PROGMEM = "VRAW";

void do_VCAL (unsigned char c) ;
const char VCAL_name[] PROGMEM = "VCAL";

void do_PMP (unsigned char c) ;
const char PMP_name[] PROGMEM = "PMP";

void do_IRAW (unsigned char c) ;
const char IRAW_name[] PROGMEM = "IRAW";

void do_ICAL (unsigned char c) ;
const char ICAL_name[] PROGMEM = "ICAL";

void do_PMP_THR (unsigned char c) ;
const char PMP_THR_name[] PROGMEM = "PMP-THR";

void do_MTN (unsigned char c) ;
const char MTN_name[] PROGMEM = "MTN";

void do_MTN_MS (unsigned char c) ;
const char MTN_MS_name[] PROGMEM = "MTN-MS";

void do_DRSN (unsigned char c) ;
const char DRSN_name[] PROGMEM = "DRSN";

void do_RESET (unsigned char c) ;
const char RESET_name[] PROGMEM = "RESET";

// Handler helpers
void do_LCD (unsigned char c, unsigned char line) ;
void do_ON_OFF (unsigned char c, volatile uint8_t *port, uint8_t maskI, uint8_t API_bit);
void do_UINT32 (unsigned char c, unsigned char bit);
void do_UINT16 (unsigned char c, unsigned char bit);
void do_WaitEOL (unsigned char c, volatile uint8_t *status, uint8_t bit);
void do_GetSetFields (unsigned char c, volatile uint8_t *status, uint8_t bit, uint8_t nfields);

// The command and handler set (for bsearch)
// N.B.: These are in sort order!
const cmd_entry cmds [] PROGMEM = {// Command action
	{AUX_name     , do_AUX     },  // Get/Set AUX switch
	{DRSN_name    , do_DRSN    },  // Get current DRSN value, Set DRSN logic
	{FLM_CUR_name , do_FLM_CUR },  // Get/Set current counter setting
	{FLM_MS_name  , do_FLM_MS  },  // Get/Set speedlimit
	{FLM_TOT_name , do_FLM_TOT },  // Get/Set cumulative flowmeter counts
	{ICAL_name    , do_ICAL    },  // Get/Set calibration values (raw1<TAB>cal1<TAB>raw2<TAB>cal2)
	{IRAW_name    , do_IRAW    },  // Get current raw value (get only)
	{LCD1_name    , do_LCD1    },  // Set LCD text
	{LCD2_name    , do_LCD2    },  // Set LCD text
	{LCDBL_name   , do_LCDBL   },  // Get/Set LCD BL
	{LGHT_name    , do_LGHT    },  // Get/Set light
	{MTN_name     , do_MTN     },  // Get current MTN, set MTN logic
	{MTN_MS_name  , do_MTN_MS  },  // Get/Set motion timeout
	{PMP_name     , do_PMP     },  // Get calibrated pump current (get only, no value)
	{PMP_THR_name , do_PMP_THR },  // Get/Set pump threshold (calibrated values: on-threshold<TAB>off-threshold)
	{RESET_name   , do_RESET   },  // Save EEPROM and Reboot immediately (get only, no value)
	{RLY1_name    , do_RLY1    },  // Get/Set pump relay
	{STK_name     , do_STK     },  // Get/Set Strike value
	{VCAL_name    , do_VCAL    },  // Get/Set Voltage calibrations (raw1<TAB>cal1<TAB>raw2<TAB>cal2)
	{VIN_name     , do_VIN     },  // Get calibrated voltage reading (get only, no value)
	{VIN_THR_name , do_VIN_THR },  // Get/Set calibrated voltage alarm thresholds (on-threshold<TAB>off-threshold)
	{VRAW_name    , do_VRAW    }   // Get raw voltage ADC value (get only, no value)
};
#define N_CMD sizeof (cmds) / sizeof (cmd_entry)


/*
  Comparison algorithm for bsearch
  The first parameter is the key, which is in SRAM.
  The second parameter is a pointer to FLASH
*/
int cmp_nam (const void *key0, const void *cmp0) {
cmd_entry_P cmp = (cmd_entry_P)cmp0;
register char cmp_ch,keyc,*keyp = (char *)key0;
register PGM_P cmp_chp;



	cmp_chp = (PGM_P)pgm_read_word (&cmp->name);
	cmp_ch = pgm_read_byte(cmp_chp++);
	keyc = (*keyp++);

	while (keyc && cmp_ch && keyc == cmp_ch) {
		cmp_ch = pgm_read_byte(cmp_chp++);
		keyc = *keyp++;
	}
	return (keyc - cmp_ch);
}

/*
  Most of these (the ones that function as commands) act as read functions
  for the UART ISR.
  The read-function switch occurs as soon as UART0_read_parse() figures out which one to call.
  These read functions consume the input until they are done or give up, and then
  they switch the read function back to UART0_read_parse().
  These functions are responsible for consuming all input until a new command can
  potentially be interpreted - when a \r or \n character appears
*/

// Consume input into a CMD_SIZE buffer
// This is not circular.  If we exceed CMD_SIZE, we just start reading back at 0.
void UART0_read_parse (unsigned char c) {
cmd_entry_P cmd_f;
static char key[CMD_SIZE]="",*keyp=key,*keyp_s=key+CMD_SIZE-1;
handler_fp theHndlr;


	// If we're at a command boundary, find the command
	if (c == '\r' || c == '\n' || c == '\t') {
		*keyp = '\0';	
		cmd_f = (cmd_entry_P)bsearch (key, cmds, N_CMD, sizeof(cmd_entry), cmp_nam);
		// If we've found a command, call its handler.	
		// Note that the handler consumes the rest of the command parameters
		// meaning, that UART0 is ready for another command
		if (cmd_f) {
			theHndlr = (handler_fp)pgm_read_word(&(cmd_f->handler));
			// set handler
			uartSetRxHandler(0, theHndlr);
			// set the label
			SmartIOinfo.cmd_lbl = (PGM_P)pgm_read_word (&cmd_f->name);
			// send it the char we got
			(theHndlr) (c);
		}
		keyp = key;
		*key = '\0';
	// If we're not at a command boundary, assume we're gathering a command
	} else if (keyp < keyp_s){
		*keyp++ = c;
		*keyp = '\0';
	// If we've run out of room for the command, start over
	// The command buffer is not circular !
	} else {
		keyp = key;
	}
	
}

void do_AUX (unsigned char c) {
	do_ON_OFF (c,&AUX_PORT,(1 << AUX_BIT),DO_AUX);
}

void do_LCD1 (unsigned char c) {
	do_LCD (c, 0);
}

void do_LCD2 (unsigned char c) {
	do_LCD (c, 1);
}

void do_LCDBL (unsigned char c) {
	do_ON_OFF (c,&LCDBL_PORT,(1 << LCDBL_BIT),DO_LCDBL);
}

void do_RLY1 (unsigned char c) {
	do_ON_OFF (c,&RLY1_PORT,(1 << RLY1_BIT),DO_RLY1);
}

void do_LGHT (unsigned char c) {
	do_ON_OFF (c,&LGHT_PORT,(1 << LGHT_BIT),DO_LGHT);
}

void do_FLM_CUR (unsigned char c) {
	do_UINT32 (c, DO_FLM_CUR);
}

void do_FLM_MS (unsigned char c) {
	do_GetSetFields (c, &(SmartIOinfo.API_STATUS), DO_FLM_MS, 1);
}

void do_FLM_TOT (unsigned char c) {
	do_UINT32 (c, DO_FLM_TOT);
}

void do_STK (unsigned char c) {
	do_ON_OFF (c,&STK_PORT,(1 << STK_BIT),DO_STK);
}

void do_VIN (unsigned char c) {	
	do_WaitEOL (c, &(SmartIOinfo.ADC0.STATUS),VAL_OUT);
}

void do_VIN_THR (unsigned char c) {
	do_GetSetFields (c, &(SmartIOinfo.ADC0.STATUS), DO_THR, 2);
}

void do_VRAW (unsigned char c) {	
	do_WaitEOL (c, &(SmartIOinfo.ADC0.STATUS),RAW_OUT);
}

void do_VCAL (unsigned char c) {
	do_GetSetFields (c, &(SmartIOinfo.ADC0.STATUS), DO_CAL, 4);
}

void do_PMP (unsigned char c) {
	do_WaitEOL (c, &(SmartIOinfo.ADC1.STATUS),VAL_OUT);
}

void do_IRAW (unsigned char c) {	
	do_WaitEOL (c, &(SmartIOinfo.ADC1.STATUS),RAW_OUT);
}

void do_ICAL (unsigned char c) {
	do_GetSetFields (c, &(SmartIOinfo.ADC1.STATUS), DO_CAL, 4);
}

void do_PMP_THR (unsigned char c) {
	do_GetSetFields (c, &(SmartIOinfo.ADC1.STATUS), DO_THR, 2);
}

void do_MTN (unsigned char c) {
	do_ON_OFF (c,&(SmartIOinfo.DIO_STATUS),(1 << MTN_LOGIC),DO_MTN);
}

void do_MTN_MS (unsigned char c) {
	do_GetSetFields (c, &(SmartIOinfo.API_STATUS), DO_MTN_MS, 1);
}

void do_DRSN (unsigned char c) {
	do_ON_OFF (c,&(SmartIOinfo.DIO_STATUS),(1 << DRSN_LOGIC),DO_DRSN);
}

void do_RESET (unsigned char c) {
	do_WaitEOL (c, &(SmartIOinfo.API_STATUS),DO_RESET);
}


void do_LCD (unsigned char c, unsigned char line) {
static uint8_t nc=0;
static volatile char *LCD_buf;

	if (c == '\r' || c == '\n') { // end of line
		if (nc) LCD_buf[nc] = '\0';
		// request action
		if (line == 0) SmartIOinfo.API_STATUS |= (1 << DO_LCD1);
		else SmartIOinfo.API_STATUS |= (1 << DO_LCD2);

		SmartIOinfo.LCD[line].len = nc;
		SmartIOinfo.LCD[line].scrollPos = 0;
		// Done with this command - set handler back to default
		nc=0;
		LCD_buf = NULL;
		uartSetRxHandler(0, UART0_read_parse);
		return;
	}
	
	if (c == '\t') {
		SmartIOinfo.LCD[line].len = 0;
		SmartIOinfo.LCD[line].scrollPos = 0;
		LCD_buf = SmartIOinfo.LCD[line].line;
		*LCD_buf = '\0';
		nc = 0;
		return;
	}

	if (LCD_buf && nc < LCD_CHARS_MAX) {
		LCD_buf[nc] = c;
		nc++; // increment only if less than LCD_CHARS_MAX
	}
	return;
}

//  The output bit is different than the input bit if there is a logic bit
//  All of these trigger output by using the API_STATUS2 register - the API_bit says which
void do_ON_OFF (unsigned char c, volatile uint8_t *port, uint8_t maskI, uint8_t API_bit) {
static uint8_t nc=0;
	
	if (c == '\r' || c == '\n') {  // end of line
		// if nc == 0, we report the current setting
		if (nc == 0) SmartIOinfo.API_STATUS2 |= (1 << API_bit);
		// Either way, this is the end of the command
		// Set handler back to default
		nc=0;
		uartSetRxHandler(0, UART0_read_parse);
		return;
	}
	if (c == '\t') return; // ignore tabs

	if (nc < MAX_READ_BUF_1_0) {
		if (c == '1') {
			*port |=  maskI;
		} else if (c == '0') {
			*port &= ~maskI;
		}
		nc++;
	}
	return;
}

void do_UINT32 (unsigned char c, unsigned char bit) {
static uint8_t nc=0;
static char *int32_buf=NULL;
	
	if (c == '\r' || c == '\n') {  // end of line
		if (nc) int32_buf[nc] = '\0';
		SmartIOinfo.API_STATUS |= (1 << bit); // request action
		// Set handler back to default
		nc=0;
		int32_buf = NULL;
		uartSetRxHandler(0, UART0_read_parse);
		return;
	}
	if (c == '\t') {
		int32_buf = (char *)SmartIOinfo.int32;
		if (*int32_buf) { // Too busy to read it.
			int32_buf = NULL;
			SmartIOinfo.API_overflow++;
		}
		return;
	}

	if (int32_buf && nc < MAX_READ_BUF_U32 && isdigit(c)) {
		int32_buf[nc] = c;
		nc++;
	}
	return;
}

void do_WaitEOL (unsigned char c, volatile uint8_t *status, uint8_t bit) {	

	if (c == '\r' || c == '\n') {  // end of line
		*status |= (1 << bit);  // Request action from main
		// Set handler back to default
		uartSetRxHandler(0, UART0_read_parse);
	}
	return;
}

void do_GetSetFields (unsigned char c, volatile uint8_t *status, uint8_t bit, uint8_t nfields) {
char *int1_buf=(char *)SmartIOinfo.int1_buf;
char *int2_buf=(char *)SmartIOinfo.int2_buf;
char *int3_buf=(char *)SmartIOinfo.int3_buf;
char *int4_buf=(char *)SmartIOinfo.int4_buf;
static char *chp;
static uint8_t nc_int1=0,nc_int2=0,nc_int3=0,nc_int4=0, *nch, field=0;


	if (c == '\r' || c == '\n') {  // end of line
		if (nc_int1) int1_buf[nc_int1] = '\0';
		if (nc_int2) int2_buf[nc_int2] = '\0';
		if (nc_int3) int3_buf[nc_int3] = '\0';
		if (nc_int4) int4_buf[nc_int4] = '\0';

		*status |= (1 << bit);  // Request action from main

		// Set handler back to default
		nc_int1 = nc_int2 = nc_int3 = nc_int4 = 0;
		chp = NULL;
		nch = NULL;
		field=0;
		uartSetRxHandler(0, UART0_read_parse);
		return;
	}
	
	// switch buffers when we get a tab character
	// The first char is set to null as soon as the main (non-ISR) function
	// is done with the string.
	// This does not guarantee that we don't miss anything, just reduces the chances.
	// It does guarantee that main and ISR don't work on the same string.
	if (c == '\t' && field < nfields) {
		if (field == 0) {
			if (*int1_buf) { // still busy
				nch = NULL;
				chp = NULL;
				SmartIOinfo.API_overflow++;
			} else {
				chp = int1_buf;
				nch = &nc_int1;
			}
			field++;
		} else if (field == 1) {
			if (*int2_buf) { // still busy
				nch = NULL;
				chp = NULL;
				SmartIOinfo.API_overflow++;
			} else {
				chp = int2_buf;
				nch = &nc_int2;
			}
			field++;
		} else if (field == 2) {
			if (*int3_buf) { // still busy
				nch = NULL;
				chp = NULL;
				SmartIOinfo.API_overflow++;
			} else {
				chp = int3_buf;
				nch = &nc_int3;
			}
			field++;
		} else if (field == 3) {
			if (*int4_buf) { // still busy
				nch = NULL;
				chp = NULL;
				SmartIOinfo.API_overflow++;
			} else {
				chp = int4_buf;
				nch = &nc_int4;
			}
			field++;
		}
		
		return; // don't do anything else with tabs
	}

	if (nch && *nch < MAX_READ_BUF_U16 && (isdigit(c) || c == '.') ) {
		*(chp+*nch) = c;
		(*nch)++;
	}
	return;
}



//
// MSR Reader Function
//
// This reader makes a string like:
// MSR<TAB><Track1><TAB><Track2><EOL>
// in SmartIOinfo.MSR
//
// track 1 starts with '%' and ends with '?'
// track 2 starts with ';' and ends with '?'
// The start and end sentinels, or any carriage returns/line-feeds are not put into the string.
// MSR hardware setup:
//   * must transmit all sentinels (start and stop for both tracks)
//   * must not transmit any LRC bytes
//   * may transmit track1 and track2 in any order
//   * may or may not have other track-separation characters (all ignored)
//   * All data transmitted outside of track sentinels is ignored.
//   * RS232 assumed 8N1@9600 baud (settings may be adjusted elsewhere in code)
#define TRACK1 1
#define TRACK2 2

void MSR_read (unsigned char c) {
static char *chp = NULL;
static uint8_t track=0;  // the current track being read
static uint8_t tracks=0; // the tracks that have been read

	// Start of track 1
	if (c == '%' && track == 0) {
		track = 1;
		chp = (char *)(SmartIOinfo.MSR1);
		if (*chp) { // Too busy
			chp = NULL;
			SmartIOinfo.API_overflow++;
		}
		return;
	}

	// Start of track 2
	if (c == ';' && track == 0) {
		track = 2;
		chp = (char *)(SmartIOinfo.MSR2);
		if (*chp) { // Too busy
			chp = NULL;
			SmartIOinfo.API_overflow++;
		}

		return;
	}

	// End of track
	if (c == '?' && track) {
		tracks |= (1 << track);
		*chp++ = '\0'; // terminate the string
		if ( tracks == ( (1 << TRACK1) | (1 << TRACK2) ) ) { // Got both
			tracks = 0;
			SmartIOinfo.API_STATUS |= (1 << DO_MSR); // Signal Main to send MSR
		}
		track = 0;
		chp = NULL;
		return;
	}
	
	// Don't send newlines
	if (c == '\r' || c == '\n') return;

	// If we're doing a track, store the character
	if (track && chp) *chp++ = c;

	return;
}

