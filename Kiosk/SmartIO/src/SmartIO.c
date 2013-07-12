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
#include <avr/interrupt.h>
#include <stdio.h>
#include <avr/pgmspace.h>
#include <stdlib.h>

#include "uart2.h"
#include "lcd.h"
#include "SmartIO.h"



char EOL[]="\r\n";
char TAB[]="\t";
SmartIO_info_t SmartIOinfo;


void uint16_to_string (uint16_t val, char pos, char *dest) {
char pos2=pos;

	// Add Leading Zeros
	if (pos == 1 && val < 10) *dest++ = '0';
	else if (pos == 2 && val < 10) {*dest++ = '0';*dest++ = '0';}
	else if (pos == 2 && val < 100) *dest++ = '0';
	else if (pos == 3 && val < 10) {*dest++ = '0';*dest++ = '0';*dest++ = '0';}
	else if (pos == 3 && val < 100) {*dest++ = '0';*dest++ = '0';}
	else if (pos == 3 && val < 1000) *dest++ = '0';
   
	// Convert int16 Value Into A String (Base = 10)
	// ultoa 
	utoa (val, dest, 10);
   
	// Get to the terminus
	while (*dest++);
	// move pos chars to the right
	pos2 = pos+1;
	while (pos2) {
		*dest = *(dest-1);
		dest--;
		pos2--;
	}
	*dest = '.';
}
/*
  Returns an integer interpreted from the string as a floating point number multiplied by fact
*/
uint16_t string_to_uint16 (char *string, volatile uint16_t fact) {
char *dec_pos=string;
long res;

	// leading crap results in a 0 return
	if (!( (*dec_pos >= '0' && *dec_pos <= '9') || *dec_pos == '.')) return (0);

	// get to the end of the digits
	while (*dec_pos >= '0' && *dec_pos <= '9') dec_pos++;

	// replace '.' with '\0' and convert left-of-decimal to integer
	if (*dec_pos == '.') {
		*dec_pos = '\0';
	
		// convert digits in front of decimal to res
		res = atol (string) * fact;
		// Add digits after decimal point, dividing factor by 10
		dec_pos++;
		fact /= 10;
		while (fact && (*dec_pos >= '0' && *dec_pos <= '9')) {
			res += (*dec_pos - '0') * fact;
			dec_pos++;
			fact /= 10;
		}
	} else {
		res = atol (string) * fact;
	}
	return (res);
}




void initTimer (void) {

	// Counter TOP at 36 to generate 2 kHz with 256 pre-scale (@ 18.432 MHz)
	OCR0A = 36;

	// Clear Timer on Compare Match (CTC) Mode
	TCCR0A |= ( (1 << WGM01) | (0 << WGM00) );

	// Set prescaler
	/*
	  CS02:CS01:CS00
	   0    0    0    No clock source (Timer/Counter stopped) 
	   0    0    1    clk /1
	   0    1    0    clk /8
	   0    1    1    clk /64
	   1    0    0    clk /256
	   1    0    1    clkI/1024
	*/
	TCCR0B |= ( (1 << CS02) | (0 << CS01) | (0 << CS00) );

	// Enable compare interrupt.
	TIMSK0 |= (1 << OCIE0A);
}



void initADC (void) {

	SmartIOinfo.ADC0.CH = 0;
	SmartIOinfo.ADC1.CH = 1;
	SmartIOinfo.ADC0.STATUS = 0;
	SmartIOinfo.ADC1.STATUS = 0;
	SmartIOinfo.ADC0.avg = 0;
	SmartIOinfo.ADC0.sum = 0;
	SmartIOinfo.ADC1.avg = 0;
	SmartIOinfo.ADC1.sum = 0;

	// Set ADC clk prescaler to 128
	ADCSRA |= (1 << ADPS2) | (1 << ADPS1) | (1 << ADPS0);
	// Disable digital inputs
	DIDR0 |= (1 << ADC1D) | (1 << ADC0D);
	// Set the Vref
	// We have a capacitor from AREF to ground.
	ADMUX |= (0<<REFS1) | (1<<REFS0);
	// Turn on the ADC
	ADCSRA |= (1 << ADEN);

	// Configure the first sample
	SmartIOinfo.theCh = 0;
	ADMUX = (ADMUX & 0xF0) | ADC_VIN_CH;
	// Start a conversion
	ADCSRA |= (1 << ADSC); 

	// Start with the voltage alarm on
	// If voltage is above the alarm threshold, this will trigger
	// a voltage alarm reset
	SmartIOinfo.ADC0.STATUS |= (1 << ALRM);  // voltage alarm on
}


void initDIO (void) {
// First make everything an input
	DDRA = 0;
	DDRB = 0;
	DDRC = 0;
	DDRD = 0;
// Set keypad outputs
	KP_DDR  |= KP_OUTPUTS;
// Set KP outputs to 0
	KP_PORT &= ~KP_OUTPUTS;
// Wave the dead chicken
	KP_PIN |= KP_INPUTS;
// Turn on pull-ups on the inputs
	KP_PORT |= KP_INPUTS;

// Clear the current key
	SmartIOinfo.KP_KEY = 0;

// Clear the current flowmeter counters
	SmartIOinfo.FLM_CURRENT = 0;
	SmartIOinfo.FLM_EVT_TIMER = 0;

// Clear the API overflow counter
	SmartIOinfo.API_overflow = 0;

// Set up pullups on inputs (NC ports are inputs with pullups)
	PORTA |= PORTA_PULLUPS;
	PORTD |= PORTD_PULLUPS;

//
// Set up DIO Outputs
//
	AUX_DDR    |=  (1 <<   AUX_BIT);
	AUX_PIN    &= ~(1 <<   AUX_BIT);
	AUX_PORT   &= ~(1 <<   AUX_BIT);

	RLY1_DDR   |=  (1 <<  RLY1_BIT);
	RLY1_PIN   &= ~(1 <<  RLY1_BIT);
	RLY1_PORT  &= ~(1 <<  RLY1_BIT);

	LCDBL_DDR  |=  (1 << LCDBL_BIT);
	LCDBL_PIN  &= ~(1 << LCDBL_BIT);
	LCDBL_PORT &= ~(1 << LCDBL_BIT);

	LGHT_DDR   |=  (1 <<  LGHT_BIT);
	LGHT_PIN   &= ~(1 <<  LGHT_BIT);
	LGHT_PORT  &= ~(1 <<  LGHT_BIT);

	STK_DDR    |=  (1 <<   STK_BIT);
	STK_PIN    &= ~(1 <<   STK_BIT);
	STK_PORT   &= ~(1 <<   STK_BIT);
	
}

void initVars (void) {

	SmartIOinfo.FLM_CUMULATIVE = 0;
	SmartIOinfo.DIO_STATUS = DIO_LOGIC_DEF;
	SmartIOinfo.FLM_EVT_TIMER_MAX = FLM_EVT_TIMER_DEF * INT_PER_MS;
	SmartIOinfo.MTN_STOPPED_CNT_MAX = MTN_STOPPED_CNT_DEF * INT_PER_MS;
	SmartIOinfo.MTN_PRESENT_CNT_MAX = MTN_PRESENT_CNT_DEF * INT_PER_MS;
	SmartIOinfo.ADC0.RAW1 = ADC0_RAW1_DEF;
	SmartIOinfo.ADC0.CAL1 = ADC0_CAL1_DEF;
	SmartIOinfo.ADC0.RAW2 = ADC0_RAW2_DEF;
	SmartIOinfo.ADC0.CAL2 = ADC0_CAL2_DEF;

// Slope and intercept for ADC0
	SmartIOinfo.ADC0.M = ADC_M_FUNC(SmartIOinfo.ADC0.CAL1,SmartIOinfo.ADC0.RAW1,SmartIOinfo.ADC0.CAL2,SmartIOinfo.ADC0.RAW2);
	SmartIOinfo.ADC0.B = ADC_B_FUNC(SmartIOinfo.ADC0.CAL1,SmartIOinfo.ADC0.RAW1,SmartIOinfo.ADC0.M);
	SmartIOinfo.ADC0.MIN = CAL_TO_ADC(0,SmartIOinfo.ADC0.M,SmartIOinfo.ADC0.B);
	if (SmartIOinfo.ADC0.MIN > 1023) SmartIOinfo.ADC0.MIN = 0;

// Alarm thresholds
// N.B.: Defaults are stored as calibrated values!
// The thresholds while running are RAW values
	SmartIOinfo.ADC0.ON_THRESH_CAL = VIN_ALARM_THRESH_CAL_DEF;
	SmartIOinfo.ADC0.ON_THRESH = CAL_TO_ADC(SmartIOinfo.ADC0.ON_THRESH_CAL,SmartIOinfo.ADC0.M,SmartIOinfo.ADC0.B);
	SmartIOinfo.ADC0.OFF_THRESH_CAL = VIN_OK_THRESH_CAL_DEF;
	SmartIOinfo.ADC0.OFF_THRESH = CAL_TO_ADC(SmartIOinfo.ADC0.OFF_THRESH_CAL,SmartIOinfo.ADC0.M,SmartIOinfo.ADC0.B);

// ADC1 calibrations
	SmartIOinfo.ADC1.RAW1 = ADC1_RAW1_DEF;
	SmartIOinfo.ADC1.CAL1 = ADC1_CAL1_DEF;
	SmartIOinfo.ADC1.RAW2 = ADC1_RAW2_DEF;
	SmartIOinfo.ADC1.CAL2 = ADC1_CAL2_DEF;

// Slope and intercept for ADC1
	SmartIOinfo.ADC1.M = ADC_M_FUNC(SmartIOinfo.ADC1.CAL1,SmartIOinfo.ADC1.RAW1,SmartIOinfo.ADC1.CAL2,SmartIOinfo.ADC1.RAW2);
	SmartIOinfo.ADC1.B = ADC_B_FUNC(SmartIOinfo.ADC1.CAL1,SmartIOinfo.ADC1.RAW1,SmartIOinfo.ADC1.M);
	SmartIOinfo.ADC1.MIN = CAL_TO_ADC(0,SmartIOinfo.ADC1.M,SmartIOinfo.ADC1.B);
	if (SmartIOinfo.ADC1.MIN > 1023) SmartIOinfo.ADC1.MIN = 0;

// Event thresholds
// N.B.: Defaults are stored as calibrated values!
// The thresholds while running are RAW values
// The RAW values are computed form the calibrated values whenever
// the ADC calibration changes, or the calibrated thresholds change.
// The calibrated values are also kept in RAM
	SmartIOinfo.ADC1.ON_THRESH_CAL = IP_ON_THRESH_CAL_DEF;
	SmartIOinfo.ADC1.ON_THRESH = CAL_TO_ADC(SmartIOinfo.ADC1.ON_THRESH_CAL,SmartIOinfo.ADC1.M,SmartIOinfo.ADC1.B);
	SmartIOinfo.ADC1.OFF_THRESH_CAL = IP_OFF_THRESH_CAL_DEF;
	SmartIOinfo.ADC1.OFF_THRESH = CAL_TO_ADC(SmartIOinfo.ADC1.OFF_THRESH_CAL,SmartIOinfo.ADC1.M,SmartIOinfo.ADC1.B);
}

ISR ( TIMER0_COMPA_vect ) {
// ADC defines
uint8_t thisCh;
uint16_t newADC;
// DIO debouncer defines
static unsigned char FLM1_CNT = FLM_CNT_MAX;
static unsigned char FLM2_CNT = FLM_CNT_MAX;
static uint16_t MTN_CNT_MAX = MTN_STOPPED_CNT_DEF*INT_PER_MS;
static uint16_t MTN_CNT  = MTN_STOPPED_CNT_DEF*INT_PER_MS;;
static unsigned char R1_CNT   = KP_CNT_MAX;
static unsigned char R2_CNT   = KP_CNT_MAX;
static unsigned char R3_CNT   = KP_CNT_MAX;
static unsigned char R4_CNT   = KP_CNT_MAX;
static unsigned char DRSN_CNT = DRSN_CNT_MAX;
// The debounced state
static unsigned char DIO_DB   = 0;
// The new state
unsigned char DIO_NS   = 0;
// The changes
unsigned char DIO_CH   = 0;
// Stable bits
unsigned char DIO_S = 0;
unsigned char kp_r=0, kp_c=0, kp_i=0;  // keypad row, column and all inputs


// ISR timings:
// 452 cycles ADC1
// 616 cycles with no inputs ADC1
// +136 cycles worst-case KP-decode
// 752 cycles worst-case
// UART@115200 = 160 cycles per bit, 1600 cycles per byte (+1 start, +1 stop)
// Should not have UART rx overflow in worst case.
// N.B.: Using stacked vertical counters in this case, with 3 different count cycles and 8 channels
//       does not have an advantage in code size or speed.  Using explicit counters provides
//       more flexibility and makes the code easier to read.
// This explicit timer code is a few cycles faster and uses the same number of data bytes


// Deal with ADC stuff
	thisCh = SmartIOinfo.theCh;
	newADC = getADC ();  // this switches channels and starts a new reading, returning the old one
	if (thisCh == 0) {
	// New voltage reading
		if (newADC < SmartIOinfo.ADC0.ON_THRESH) {
			if (!(SmartIOinfo.ADC0.STATUS & (1<<ALRM)))
				SmartIOinfo.ADC0.STATUS |= (1 << ALRM_CHG);  // voltage alarm status changed
			SmartIOinfo.ADC0.STATUS |= (1 << ALRM);  // voltage alarm on
		} else if (newADC > SmartIOinfo.ADC0.OFF_THRESH) {
			if (SmartIOinfo.ADC0.STATUS & (1<<ALRM))
				SmartIOinfo.ADC0.STATUS |= (1 << ALRM_CHG);  // voltage alarm status changed
			SmartIOinfo.ADC0.STATUS &= ~(1 << ALRM);  // voltage alarm off
		}
	} else {
	// New current reading
		if (newADC > SmartIOinfo.ADC1.ON_THRESH) {
			if (!(SmartIOinfo.ADC1.STATUS & (1<<ALRM)))
				SmartIOinfo.ADC1.STATUS |= (1 << ALRM_CHG);  // pump changed
			SmartIOinfo.ADC1.STATUS |= (1 << ALRM);  // pump on
		} else if (newADC < SmartIOinfo.ADC1.OFF_THRESH) {
			if (SmartIOinfo.ADC1.STATUS & (1<<ALRM))
				SmartIOinfo.ADC1.STATUS |= (1 << ALRM_CHG);  // pump changed
			SmartIOinfo.ADC1.STATUS &= ~(1 << ALRM);  // pump is off
		}
	}
// End of ADC stuff
//
// Deal with DIO stuff
//
// Saw some pullups disappearing for unexplained reasons once,
// so a bit of cargo-cult here
	PORTA |= PORTA_PULLUPS;
	PORTA &= ~PORTA_PULLDOWNS;

	PORTB |= PORTB_PULLUPS;
	PORTB &= ~PORTB_PULLDOWNS;

	PORTD |= PORTD_PULLUPS;
	PORTD &= ~PORTD_PULLDOWNS;

// All DIOs are debounced to various extents
// Put the current DIOs into the right bits in the counters
// DIO_NS is initialized to zero in each pass, so we only set the high bits	
	if (FLM_PIN  & (1 << FLM1_BIT)) DIO_NS  |= (1 << FLM1_DB);
	if (FLM_PIN  & (1 << FLM2_BIT)) DIO_NS  |= (1 << FLM2_DB);
// Motion and door have settable logic, but DIO_NS/CH/DB bits are always positive logic
	if (MTN_PIN  & (1 << MTN_BIT ) && (SmartIOinfo.DIO_STATUS & (1 << MTN_LOGIC))) DIO_NS  |= (1 << MTN_DB);
	else if (!(MTN_PIN  & (1 << MTN_BIT )) && !(SmartIOinfo.DIO_STATUS & (1 << MTN_LOGIC))) DIO_NS  |= (1 << MTN_DB);
	if ((DRSN_PIN & (1 << DRSN_BIT)) && (SmartIOinfo.DIO_STATUS & (1 << DRSN_LOGIC))) DIO_NS  |= (1 << DRSN_DB);
	else if (!(DRSN_PIN & (1 << DRSN_BIT)) && !(SmartIOinfo.DIO_STATUS & (1 << DRSN_LOGIC))) DIO_NS  |= (1 << DRSN_DB);
// Keypad bits are active-low
	if (!(KP_PIN   & (1 << KP_R1))) DIO_NS  |= (1 << R1_DB);
	if (!(KP_PIN   & (1 << KP_R2))) DIO_NS  |= (1 << R2_DB);
	if (!(KP_PIN   & (1 << KP_R3))) DIO_NS  |= (1 << R3_DB);
	if (!(KP_PIN   & (1 << KP_R4))) DIO_NS  |= (1 << R4_DB);
	// Set bits that changed since debounce
	DIO_CH = DIO_NS ^ DIO_DB;

	if (!(DIO_CH & (1 << FLM1_DB))) FLM1_CNT = FLM_CNT_MAX;  // no change
	else if (FLM1_CNT)  FLM1_CNT--;// count still positive
	else  DIO_S |= (1<<FLM1_DB); // count reached 0

	if (!(DIO_CH & (1 << FLM2_DB))) FLM2_CNT = FLM_CNT_MAX;
	else if (FLM2_CNT) FLM2_CNT--;
	else DIO_S |= (1<<FLM2_DB);

	// The value of MTN_CNT_MAX depends on the motion sensor's state (set below)
	if (!(DIO_CH & (1 << MTN_DB ))) MTN_CNT  = MTN_CNT_MAX;
	else if (MTN_CNT) MTN_CNT--;
	else DIO_S |= (1<<MTN_DB);

	if (!(DIO_CH & (1 << DRSN_DB))) DRSN_CNT = DRSN_CNT_MAX;
	else if (DRSN_CNT) DRSN_CNT--;
	else DIO_S |= (1<<DRSN_DB);

	if (!(DIO_CH & (1 << R1_DB  ))) R1_CNT   = KP_CNT_MAX;
	else if (R1_CNT) R1_CNT--;
	else {
		DIO_S |= (1<<R1_DB);
		kp_r = 1;
	}

	if (!(DIO_CH & (1 << R2_DB  ))) R2_CNT   = KP_CNT_MAX;
	else if (R2_CNT) R2_CNT--;
	else {
		DIO_S |= (1<<R2_DB);
		kp_r = 2;
	}

	if (!(DIO_CH & (1 << R3_DB  ))) R3_CNT   = KP_CNT_MAX;
	else if (R3_CNT) R3_CNT--;
	else {
		DIO_S |= (1<<R3_DB);
		kp_r = 3;
	}

	if (!(DIO_CH & (1 << R4_DB  ))) R4_CNT   = KP_CNT_MAX;
	else if (R4_CNT) R4_CNT--;
	else {
		DIO_S |= (1<<R4_DB);
		kp_r = 4;
	}

	// New debounce values are stable bits anded with the new sample
	// and unstable bits that were 1s originally	
	DIO_DB = (DIO_S & DIO_NS) | (DIO_DB & ~DIO_S);

// Set up events based on changed debounced states
	// Decrement the flowmeter event timer regardless of flowmeter state changes
	if (SmartIOinfo.FLM_EVT_TIMER > 0) SmartIOinfo.FLM_EVT_TIMER--;

	if (DIO_S & FLM_DBS) {
		SmartIOinfo.DIO_STATUS |= (1 << FLM_CHANGED);
		SmartIOinfo.FLM_CUMULATIVE++;
		SmartIOinfo.FLM_CURRENT++;
	}

	// Keypress.
	// Only process if:
	//   at least one input changed, KEY_PRESSED is clear, debounced state has at least one high
	if ( (DIO_S & KP_DBS) && !(SmartIOinfo.DIO_STATUS & (1<<KEY_PRESSED)) && (DIO_DB & KP_DBS)) {
		// A row goes low when connected to a column which is outputing a 0
		// Otherwise, the rows are at 1 due to the pullups.
		// To determine the column, we set the columns to 1 until the 0-row changes to 1

		// 130 cycles worst-case to decode the key
		kp_i = KP_PIN & KP_INPUTS; // to look for any change in the inputs
		kp_c = 0;
		KP_PORT |= (1 << KP_C1);
		_delay_loop_1 ( 1 ); // let things settle for a bit
		if ( (KP_PIN & KP_INPUTS) != kp_i ) kp_c = 1;
		KP_PORT &= ~(1 << KP_C1);

		if (!kp_c) {
			KP_PORT |= (1 << KP_C2);
			_delay_loop_1 ( 1 ); // let things settle for a bit
			if ( (KP_PIN & KP_INPUTS) != kp_i ) kp_c = 2;
			KP_PORT &= ~(1 << KP_C2);
		}

		if (!kp_c) {
			KP_PORT |= (1 << KP_C3);
			_delay_loop_1 ( 1 ); // let things settle for a bit
			if ( (KP_PIN & KP_INPUTS) != kp_i ) kp_c = 3;
			KP_PORT &= ~(1 << KP_C3);
		}

		if (!kp_c) {
			KP_PORT |= (1 << KP_C4);
			_delay_loop_1 ( 1 ); // let things settle for a bit
			if ( (KP_PIN & KP_INPUTS) != kp_i ) kp_c = 4;
			KP_PORT &= ~(1 << KP_C4);
		}

		if (kp_r && kp_c) {
			SmartIOinfo.KP_KEY = getKey (kp_r,kp_c);
			if (SmartIOinfo.KP_KEY) SmartIOinfo.DIO_STATUS |= (1 << KEY_PRESSED);
		}
// END of keypad processing
	}

//
// Process Motion and Door sensor
// Both of these have a specified logic for "motion present" and "door open"
// DIO_DB is always positive logic
//
// Motion
// Two different timer settings depending on state
	if (DIO_S & (1<<MTN_DB)) {
		SmartIOinfo.DIO_STATUS |= (1 << MTN_CHANGED);
		if (DIO_DB & (1<<MTN_DB)) {
			SmartIOinfo.DIO_STATUS |= (1 << MTN_PRESENT);
			MTN_CNT_MAX = SmartIOinfo.MTN_PRESENT_CNT_MAX;
		} else {
			SmartIOinfo.DIO_STATUS &= ~(1 << MTN_PRESENT);
			MTN_CNT_MAX = SmartIOinfo.MTN_STOPPED_CNT_MAX;
		}
	}
// Door Sensor
	if (DIO_S & (1<<DRSN_DB)) {
		SmartIOinfo.DIO_STATUS |= (1 << DRSN_CHANGED);
		if (DIO_DB & (1<<DRSN_DB)) SmartIOinfo.DIO_STATUS |= (1 << DRSN_OPEN);
		else SmartIOinfo.DIO_STATUS &= ~(1 << DRSN_OPEN);
	}
// LCD scroll timer
	SmartIOinfo.lcd_intr_c++;
//  END of ISR
}

char getKey (uint8_t r, uint8_t c) {
	switch (r) {
		case 1:
			switch (c) {
				case 1:
					return ('1');
				break;
				case 2:
					return ('2');
				break;
				case 3:
					return ('3');
				break;
				case 4:
					return ('A');
				break;
				default:
					return (0);
				break;
			}
		break;
		case 2:
			switch (c) {
				case 1:
					return ('4');
				break;
				case 2:
					return ('5');
				break;
				case 3:
					return ('6');
				break;
				case 4:
					return ('B');
				break;
				default:
					return (0);
				break;
			}
		break;
		case 3:
			switch (c) {
				case 1:
					return ('7');
				break;
				case 2:
					return ('8');
				break;
				case 3:
					return ('9');
				break;
				case 4:
					return ('C');
				break;
				default:
					return (0);
				break;
			}
		break;
		case 4:
			switch (c) {
				case 1:
					return ('*');
				break;
				case 2:
					return ('0');
				break;
				case 3:
					return ('#');
				break;
				case 4:
					return ('D');
				break;
				default:
					return (0);
				break;
			}
		break;
		default:
		return (0);
	}
	return (0);
}

// Current measurement is based on finding the maximum of the peak in the AC wave.
// Current sensor is unipolar, so the sensor is 0 for the entire negative half of the wave.
// The current sensor has no low-pass filter to the ADC because the averaging reduces sensitivity.
// Peak detection is done by getting the maximum of the last NIPSAMP samples, and reporting this
// as the current ADC value.  These maxima are then running-averaged like Vin over NSAMPLES samples.
// 2 channels at 2 kHz is 1 kHz sample per channel.
// 18 samples of the (potentially) AC current is 1.1 60 Hz waves, ensuring 1 positive peak.
#define NIPSAMP 18
uint16_t getADC (void) {
uint16_t adc_value;
uint32_t tmp_sum;
uint32_t delta;
static uint16_t max=0,new_max=0;
static uint8_t ipsamp=0;

	// Read 10-bit ADC, with low-byte first, shifting the high-byte by 8.
	adc_value = ADC;   
	
	if (SmartIOinfo.theCh == 0) {
		tmp_sum = SmartIOinfo.ADC0.sum;
	} else {
		if (adc_value > new_max) new_max = adc_value;
		ipsamp++;
		if (ipsamp >= NIPSAMP) {
			max = new_max;
			new_max = 0;
			ipsamp = 0;
		}
		adc_value = max;
		tmp_sum = SmartIOinfo.ADC1.sum;
	}
	
	// Compute a cumulative average
	delta = tmp_sum / NSAMPLES;  // keep n-1 samples
	tmp_sum -= delta;
	tmp_sum += adc_value;       // add in the new sample
	if (SmartIOinfo.theCh == 0) {
		SmartIOinfo.ADC0.sum = tmp_sum;
		tmp_sum /= NSAMPLES;        // put the cumulative average in tmp_sum
		SmartIOinfo.ADC0.avg = tmp_sum;
		ADMUX = (ADMUX & 0xF0) | ADC_IP_CH;
		SmartIOinfo.theCh = 1;
	} else {
		SmartIOinfo.ADC1.sum = tmp_sum;
		tmp_sum /= NSAMPLES;        // put the cumulative average in tmp_sum
		SmartIOinfo.ADC1.avg = tmp_sum;
		ADMUX = (ADMUX & 0xF0) | ADC_VIN_CH;
		SmartIOinfo.theCh = 0;
	}

	// Start a conversion
	ADCSRA |= (1 << ADSC);
	
	return (tmp_sum); // actually, the cumulative average
}

