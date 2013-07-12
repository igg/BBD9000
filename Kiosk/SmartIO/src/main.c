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
#include <util/delay.h>
#include <avr/pgmspace.h>
#include <avr/interrupt.h>
#include <avr/wdt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "parser.h"
#include "lcd.h"
#include "uart2.h"
#include "SmartIO.h"

void do_DIO_out (PGM_P name, volatile uint8_t *pin, uint8_t maskO, uint8_t maskSTATUS) ;
void do_ADC_THR (PGM_P name, ADC_info_t *ADCinfo) ;
void do_ADC_CAL (PGM_P name, ADC_info_t *ADCinfo) ;
void do_ADC_RAW_OUT (PGM_P name, ADC_info_t *ADCinfo) ;
void do_ADC_CAL_OUT (PGM_P name, ADC_info_t *ADCinfo) ;
void do_GetSet_U32 (PGM_P name, volatile uint32_t *val, unsigned char bit);
void do_GetSet_U16_ms (PGM_P name, volatile uint16_t *val, unsigned char bit);
void do_MSR ();
void do_WriteLCD (uint8_t line);
void do_ScrollLCD (uint8_t line);



int main(void) {
char outStr[64];

	// Disable the watchdog in case we got here from a reboot
//	MCUSR = 0;
//	wdt_disable();
	wdt_enable(WDTO_500MS);


	initVars();
	initDIO();
	initADC();
	initTimer();
	uartInit();                 // initialize UARTs (serial ports)
	uartSetBaudRate(0, 115200); // set UART0 speed to 115200 baud (CPU)
	uartSetBaudRate(1,   9600); // set UART1 speed to 9600 baud (MSR)
	// This is a command parser and handler switcher
	uartSetRxHandler(0, UART0_read_parse);
	// This is the MSR reader (send bytes directly to UART0)
	uartSetRxHandler(1, MSR_read);


	lcd_init();
	lcd_pos( 0, 0 );
	strcpy_P (outStr,PSTR(VERSION_STR));
	lcd_text(outStr);
	lcd_pos( 1, 0 );
	strcpy_P (outStr,PSTR("Starting up..."));
	lcd_text(outStr);
	
	// enable interrupts
	sei();

	// Discard one flowmeter tick
	if (SmartIOinfo.DIO_STATUS & (1<< FLM_CHANGED)) {
		SmartIOinfo.DIO_STATUS &= ~(1 << FLM_CHANGED);
		SmartIOinfo.FLM_CURRENT--;
		SmartIOinfo.FLM_CUMULATIVE--;
	}

	// Send Ready over the serial line, but leave the display alone
	uartSendString (0,EOL);
	uartSendString_P(0, PSTR("Ready"));
	uartSendString (0,EOL);

	while (1) {
		// reset the watchdog
		wdt_reset();

		if ( (SmartIOinfo.DIO_STATUS & (1<< FLM_CHANGED)) && SmartIOinfo.FLM_EVT_TIMER == 0 ) {
			uartSendString_P (0, PSTR("FLM\t"));
			ultoa (SmartIOinfo.FLM_CURRENT, outStr, 10);
			uartSendString (0,outStr);
			uartSendString (0,EOL);
			SmartIOinfo.FLM_EVT_TIMER = SmartIOinfo.FLM_EVT_TIMER_MAX;
			SmartIOinfo.DIO_STATUS &= ~(1 << FLM_CHANGED);
		}
		if (SmartIOinfo.DIO_STATUS & (1<< KEY_PRESSED)) {
			uartSendString_P (0,PSTR("KP\t"));
			uartAddToTxBuffer (0,SmartIOinfo.KP_KEY); // may trigger immediate interrupt.
			SmartIOinfo.KP_KEY = '\0';
			uartSendString (0,EOL);
			SmartIOinfo.DIO_STATUS &= ~(1 << KEY_PRESSED);
		}
		// MTN-STRT MTN-STOP
		if (SmartIOinfo.DIO_STATUS & (1<< MTN_CHANGED)) {
			if (SmartIOinfo.DIO_STATUS & (1<< MTN_PRESENT))
				uartSendString_P (0,PSTR("MTN-STRT"));
			else
				uartSendString_P (0,PSTR("MTN-STOP"));
			uartSendString (0,EOL);
			SmartIOinfo.DIO_STATUS &= ~(1 << MTN_CHANGED);
		}
		// DRSN-OPND DRSN-CLSD
		if (SmartIOinfo.DIO_STATUS & (1<< DRSN_CHANGED)) {
			if (SmartIOinfo.DIO_STATUS & (1<< DRSN_OPEN)) 
				uartSendString_P (0,PSTR("DRSN-OPND"));
			else
				uartSendString_P (0,PSTR("DRSN-CLSD"));
			uartSendString (0,EOL);
			SmartIOinfo.DIO_STATUS &= ~(1 << DRSN_CHANGED);
		}
		// VALRM-STRT VALRM-STOP
		if (SmartIOinfo.ADC0.STATUS & (1<< ALRM_CHG)) {
			if (SmartIOinfo.ADC0.STATUS & (1<< ALRM)) 
				uartSendString_P (0,PSTR("VALRM-STRT"));
			else
				uartSendString_P (0,PSTR("VALRM-STOP"));
			uartSendString (0,EOL);

			SmartIOinfo.ADC0.STATUS &= ~(1 << ALRM_CHG);
		}
		// PMP-ON PMP-OFF
		if (SmartIOinfo.ADC1.STATUS & (1<< ALRM_CHG)) {
			if (SmartIOinfo.ADC1.STATUS & (1<< ALRM)) 
				uartSendString_P (0,PSTR("PMP-ON"));
			else
				uartSendString_P (0,PSTR("PMP-OFF"));
			uartSendString (0,EOL);
			SmartIOinfo.ADC1.STATUS &= ~(1 << ALRM_CHG);
		}
		
		//
		// Process commands from serial interface
		//
		// Commands affecting ADC0 - voltage
		if (SmartIOinfo.ADC0.STATUS & (1<< DO_THR)) {
			do_ADC_THR (
				(PGM_P)pgm_read_word (&cmds[VIN_THR_CMD].name),
				&(SmartIOinfo.ADC0)
			);
		}
		if (SmartIOinfo.ADC0.STATUS & (1<< DO_CAL)) {
			do_ADC_CAL (
				(PGM_P)pgm_read_word (&cmds[VCAL_CMD].name),
				&(SmartIOinfo.ADC0)
			);
		}
		if (SmartIOinfo.ADC0.STATUS & (1<< RAW_OUT)) {
			do_ADC_RAW_OUT (
				(PGM_P)pgm_read_word (&cmds[VRAW_CMD].name),
				&(SmartIOinfo.ADC0)
			);
		}
		if (SmartIOinfo.ADC0.STATUS & (1<< VAL_OUT)) {
			do_ADC_CAL_OUT (
				(PGM_P)pgm_read_word (&cmds[VIN_CMD].name),
				&(SmartIOinfo.ADC0)
			);
		}

		// Commands affecting ADC1 - current
		if (SmartIOinfo.ADC1.STATUS & (1<< DO_THR)) {
			do_ADC_THR (
				(PGM_P)pgm_read_word (&cmds[PMP_THR_CMD].name),
				&(SmartIOinfo.ADC1)
			);
		}
		if (SmartIOinfo.ADC1.STATUS & (1<< DO_CAL)) {
			do_ADC_CAL (
				(PGM_P)pgm_read_word (&cmds[ICAL_CMD].name),
				&(SmartIOinfo.ADC1)
			);
		}
		if (SmartIOinfo.ADC1.STATUS & (1<< RAW_OUT)) {
			do_ADC_RAW_OUT (
				(PGM_P)pgm_read_word (&cmds[IRAW_CMD].name),
				&(SmartIOinfo.ADC1)
			);
		}
		if (SmartIOinfo.ADC1.STATUS & (1<< VAL_OUT)) {
			do_ADC_CAL_OUT (
				(PGM_P)pgm_read_word (&cmds[PMP_CMD].name),
				&(SmartIOinfo.ADC1)
			);
		}
		
		// Other commands
		if (SmartIOinfo.API_STATUS & (1<< DO_FLM_CUR)) {
			do_GetSet_U32 (
				(PGM_P)pgm_read_word (&cmds[FLM_CUR_CMD].name),
				&(SmartIOinfo.FLM_CURRENT), DO_FLM_CUR
			);
		}
		if (SmartIOinfo.API_STATUS & (1<< DO_FLM_TOT)) {
			do_GetSet_U32 (
				(PGM_P)pgm_read_word (&cmds[FLM_TOT_CMD].name),
				&(SmartIOinfo.FLM_CUMULATIVE), DO_FLM_TOT
			);
		}
		if (SmartIOinfo.API_STATUS & (1<< DO_FLM_MS)) {
			do_GetSet_U16_ms (
				(PGM_P)pgm_read_word (&cmds[FLM_MS_CMD].name),
				&(SmartIOinfo.FLM_EVT_TIMER_MAX), DO_FLM_MS
			);
		}
		if (SmartIOinfo.API_STATUS & (1<< DO_MTN_MS)) {
			do_GetSet_U16_ms (
				(PGM_P)pgm_read_word (&cmds[MTN_MS_CMD].name),
				&(SmartIOinfo.MTN_PRESENT_CNT_MAX), DO_MTN_MS
			);
		}
		if (SmartIOinfo.API_STATUS & (1<< DO_MSR)) {
			do_MSR ();
		}
		if (SmartIOinfo.API_STATUS & (1<< DO_LCD1)) {
			do_WriteLCD (0);
		}
		if (SmartIOinfo.API_STATUS & (1<< DO_LCD2)) {
			do_WriteLCD (1);
		}
		if (SmartIOinfo.lcd_intr_c > LCD_SCROLL_INTR) {
			if (SmartIOinfo.LCD[0].len > LCD_CHARS) do_ScrollLCD (0);
			if (SmartIOinfo.LCD[1].len > LCD_CHARS) do_ScrollLCD (1);
		}
		
		// API_STATUS2 register - report DIO states
		if (SmartIOinfo.API_STATUS2 & (1<< DO_AUX)) {
			do_DIO_out (
				(PGM_P)pgm_read_word (&cmds[AUX_CMD].name),
				&AUX_PIN,(1 << AUX_BIT),(1 << DO_AUX)
			);
		}
		if (SmartIOinfo.API_STATUS2 & (1<< DO_LCDBL)) {
			do_DIO_out (
				(PGM_P)pgm_read_word (&cmds[LCDBL_CMD].name),
				&LCDBL_PIN,(1 << LCDBL_BIT),(1 << DO_LCDBL)
			);
		}
		if (SmartIOinfo.API_STATUS2 & (1<< DO_RLY1)) {
			do_DIO_out (
				(PGM_P)pgm_read_word (&cmds[RLY1_CMD].name),
				&RLY1_PIN,(1 << RLY1_BIT),(1 << DO_RLY1)
			);
		}
		if (SmartIOinfo.API_STATUS2 & (1<< DO_LGHT)) {
			do_DIO_out (
				(PGM_P)pgm_read_word (&cmds[LGHT_CMD].name),
				&LGHT_PIN,(1 << LGHT_BIT),(1 << DO_LGHT)
			);
		}
		if (SmartIOinfo.API_STATUS2 & (1<< DO_STK)) {
			do_DIO_out (
				(PGM_P)pgm_read_word (&cmds[STK_CMD].name),
				&STK_PIN,(1 << STK_BIT),(1 << DO_STK)
			);
		}
		if (SmartIOinfo.API_STATUS2 & (1<< DO_MTN)) {
			do_DIO_out (
				(PGM_P)pgm_read_word (&cmds[MTN_CMD].name),
				&(SmartIOinfo.DIO_STATUS),(1 << MTN_PRESENT),(1 << DO_MTN)
			);
		}
		if (SmartIOinfo.API_STATUS2 & (1<< DO_DRSN)) {
			do_DIO_out (
				(PGM_P)pgm_read_word (&cmds[DRSN_CMD].name),
				&(SmartIOinfo.DIO_STATUS),(1 << DRSN_OPEN),(1 << DO_DRSN)
			);
		}

		if (SmartIOinfo.API_STATUS & (1<< DO_RESET)) {
		// Use the watchdog timer to reset.
			wdt_enable(WDTO_15MS);
			while(1);
		}
	}
}

void do_DIO_out (PGM_P name, volatile uint8_t *pin, uint8_t maskO, uint8_t maskSTATUS) {
	uartSendString_P (0,(PGM_P)name);
	uartAddToTxBuffer (0,'\t');
	uartAddToTxBuffer (0,((*pin  & maskO) ? '1' : '0') );
	uartSendString (0,EOL);
	SmartIOinfo.API_STATUS2 &= ~maskSTATUS;
}


void do_ADC_THR (PGM_P name, ADC_info_t *ADCinfo) {
char *on_buf=(char *)SmartIOinfo.int1_buf;
char *off_buf=(char *)SmartIOinfo.int2_buf;
long on, off;
uint16_t on_thresh, off_thresh;
char *int16Out_buf=(char *)SmartIOinfo.int16Out_buf;

	// SET
	if (*on_buf || *off_buf) {
		// Set the first char to NULL as soon as we're done
		// so the ISR can use them again
		on = string_to_uint16 (on_buf, 100);
		*on_buf = '\0';
		off = string_to_uint16 (off_buf, 100);
		*off_buf = '\0';
	
		if (on < 0 || on > UINT16_MAX) {
			on = ADCinfo->ON_THRESH_CAL;
		}
		if (off < 0 || off > UINT16_MAX) {
			off = ADCinfo->OFF_THRESH_CAL;
		}
		
		on_thresh = CAL_TO_ADC(on,ADCinfo->M,ADCinfo->B);
		off_thresh = CAL_TO_ADC(off,ADCinfo->M,ADCinfo->B);
	
		ADCinfo->ON_THRESH_CAL  = (uint16_t) on;
		ADCinfo->ON_THRESH      = on_thresh;
		ADCinfo->OFF_THRESH_CAL = (uint16_t) off;
		ADCinfo->OFF_THRESH     = off_thresh;

	}

	// Print result
	uartSendString_P (0,(PGM_P)name);
	uartAddToTxBuffer (0,'\t');

	uint16_to_string (ADCinfo->ON_THRESH_CAL, 2, int16Out_buf);
	uartSendString (0,int16Out_buf);
	uartSendString (0,TAB);

	uint16_to_string (ADCinfo->OFF_THRESH_CAL, 2, int16Out_buf);
	uartSendString (0,int16Out_buf);
	uartSendString (0,EOL);

	*(SmartIOinfo.int16Out_buf) = '\0';

	ADCinfo->STATUS &= ~(1 << DO_THR);
}

void do_ADC_CAL (PGM_P name, ADC_info_t *ADCinfo) {
char *raw1_buf=(char *)SmartIOinfo.int1_buf;
char *cal1_buf=(char *)SmartIOinfo.int2_buf;
char *raw2_buf=(char *)SmartIOinfo.int3_buf;
char *cal2_buf=(char *)SmartIOinfo.int4_buf;
long raw1, cal1, raw2, cal2, m, b;
uint16_t on_thresh, off_thresh;
char *int16Out_buf=(char *)SmartIOinfo.int16Out_buf;

	// SET
	if (*raw1_buf || *cal1_buf || *raw2_buf || *cal2_buf) {
		// Set the first char to NULL as soon as we're done
		// so the ISR can use them again
		raw1 = atol (raw1_buf);
		*raw1_buf = '\0';
		cal1 = string_to_uint16 (cal1_buf, 100);
		*cal1_buf = '\0';
		raw2 = atol (raw2_buf);
		*raw2_buf = '\0';
		cal2 = string_to_uint16 (cal2_buf, 100);
		*cal2_buf = '\0';
	
		// Ensure the numerical range is OK
		if (raw1 < 0 || raw1 > UINT16_MAX) {
			raw1 = ADCinfo->RAW1;
		}	
		if (cal1 < 0 || cal1 > UINT16_MAX) {
			cal1 = ADCinfo->CAL1;
		}
		if (raw2 < 0 || raw2 > UINT16_MAX) {
			raw2 = ADCinfo->RAW2;
		}
		if (cal2 < 0 || cal2 > UINT16_MAX) {
			cal2 = ADCinfo->CAL2;
		}
		
		// Slope and intercept for ADC based on calibrations
		m = ADC_M_FUNC(cal1,raw1,cal2,raw2);
		b = ADC_B_FUNC(cal1,raw1,m);
		// The raw values for the thresholds need to be recalculated
		on_thresh = CAL_TO_ADC(ADCinfo->ON_THRESH_CAL,m,b);
		off_thresh = CAL_TO_ADC(ADCinfo->OFF_THRESH_CAL,m,b);

		ADCinfo->CAL1       = cal1;
		ADCinfo->RAW1       = raw1;
		ADCinfo->CAL2       = cal2;
		ADCinfo->RAW2       = raw2;
		ADCinfo->M          = m;
		ADCinfo->B          = b;
		ADCinfo->ON_THRESH  = on_thresh;
		ADCinfo->OFF_THRESH = off_thresh;
		ADCinfo->MIN        = CAL_TO_ADC(0,m,b);
		if (ADCinfo->MIN > 1023) ADCinfo->MIN = 0;

	}

	// Print result
	uartSendString_P (0,(PGM_P)name);
	uartAddToTxBuffer (0,'\t');

	utoa (ADCinfo->RAW1, int16Out_buf, 10);
	uartSendString (0,int16Out_buf);
	uartSendString (0,TAB);

	uint16_to_string (ADCinfo->CAL1, 2, int16Out_buf);
	uartSendString (0,int16Out_buf);
	uartSendString (0,TAB);

	utoa (ADCinfo->RAW2, int16Out_buf, 10);
	uartSendString (0,int16Out_buf);
	uartSendString (0,TAB);

	uint16_to_string (ADCinfo->CAL2, 2, int16Out_buf);
	uartSendString (0,int16Out_buf);
	uartSendString (0,EOL);
	*(SmartIOinfo.int16Out_buf) = '\0';

	ADCinfo->STATUS &= ~(1 << DO_CAL);
}


void do_ADC_RAW_OUT (PGM_P name, ADC_info_t *ADCinfo) {
char *int16Out_buf=(char *)SmartIOinfo.int16Out_buf;

	uartSendString_P (0,(PGM_P)name);
	uartAddToTxBuffer (0,'\t');

	utoa (ADCinfo->avg, int16Out_buf, 10);
	uartSendString (0,int16Out_buf);
	uartSendString (0,EOL);

	*int16Out_buf = '\0';
	ADCinfo->STATUS &= ~(1 << RAW_OUT);
}

void do_ADC_CAL_OUT (PGM_P name, ADC_info_t *ADCinfo) {
char *int16Out_buf=(char *)SmartIOinfo.int16Out_buf;

	uartSendString_P (0,(PGM_P)name);
	uartAddToTxBuffer (0,'\t');

	if (ADCinfo->avg > ADCinfo->MIN)
		uint16_to_string (ADC_TO_CAL(ADCinfo->avg,ADCinfo->M,ADCinfo->B), 2, int16Out_buf);
	else
		uint16_to_string (0, 2, int16Out_buf);
	uartSendString (0,int16Out_buf);
	uartSendString (0,EOL);

	*int16Out_buf = '\0';
	ADCinfo->STATUS &= ~(1 << VAL_OUT);
}

void do_GetSet_U32 (PGM_P name, volatile uint32_t *val, unsigned char bit) {

	// SET
	if (*(SmartIOinfo.int32)) {
		*val = strtoul ((char *)SmartIOinfo.int32,NULL,10);
		*(SmartIOinfo.int32) = '\0';

	}
	
	// GET
	uartSendString_P (0,(PGM_P)name);
	uartAddToTxBuffer (0,'\t');

	ultoa (*val, (char *)SmartIOinfo.int32Out_buf, 10);
	uartSendString (0,(char *)SmartIOinfo.int32Out_buf);
	*(SmartIOinfo.int32Out_buf) = '\0';
	uartSendString (0,EOL);

	SmartIOinfo.API_STATUS &= ~(1 << bit);
}


void do_GetSet_U16_ms (PGM_P name, volatile uint16_t *val, unsigned char bit) {
long tmp;

	// SET
	if (*(SmartIOinfo.int1_buf)) {
		tmp = atol ((char *)SmartIOinfo.int1_buf);
		*(SmartIOinfo.int1_buf) = '\0';
		tmp *= INT_PER_MS;
		if (tmp >= 0 && tmp < UINT16_MAX) *val = tmp;

	}
	
	// GET
	uartSendString_P (0,(PGM_P)name);
	uartAddToTxBuffer (0,'\t');

	utoa ((*val) / INT_PER_MS, (char *)SmartIOinfo.int16Out_buf, 10);
	uartSendString (0,(char *)SmartIOinfo.int16Out_buf);
	*(SmartIOinfo.int16Out_buf) = '\0';
	uartSendString (0,EOL);

	SmartIOinfo.API_STATUS &= ~(1 << bit);
}

void do_MSR () {
	uartSendString_P (0,PSTR("MSR\t"));

	// Send track 1
	uartSendString (0,(char *)SmartIOinfo.MSR1);
	uartAddToTxBuffer (0,'\t');
	// Be paranoid
	memset ((char *)SmartIOinfo.MSR1,0,MSR1_MAX);

	// Send track 2
	uartSendString (0,(char *)SmartIOinfo.MSR2);
	uartSendString (0,EOL);
	// Be paranoid
	memset ((char *)SmartIOinfo.MSR2,0,MSR2_MAX);

	SmartIOinfo.API_STATUS &= ~(1 << DO_MSR);
}

void do_WriteLCD (uint8_t line) {
uint8_t nc=0;
volatile char *chp = NULL;

	lcd_pos( line, 0 );
	chp = SmartIOinfo.LCD[line].line;

	while (nc < LCD_CHARS) {
		// Either write chars or spaces if we run out of chars
		if (*chp) {
			lcd_data (*chp);
			chp++;
		} else {
			lcd_data (' ');
		}
		nc++;
	}
	if (line == 0) SmartIOinfo.API_STATUS &= ~(1 << DO_LCD1);
	else SmartIOinfo.API_STATUS &= ~(1 << DO_LCD2);
	
}

void do_ScrollLCD (uint8_t line) {
uint8_t nc;
LCD_info_t *lcd=&(SmartIOinfo.LCD[line]);

	lcd_pos( line, 0 );

	for (nc=0; nc < LCD_CHARS; nc++) {
		lcd_data (*(lcd->line+((lcd->scrollPos+nc)%lcd->len)));
	}
	SmartIOinfo.lcd_intr_c = 0;
	lcd->scrollPos = (lcd->scrollPos+1)%lcd->len;

}
