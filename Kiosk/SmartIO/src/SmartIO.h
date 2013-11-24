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
#ifndef SmartIO_h
#define SmartIO_h

#define VERSION_STR "BBD9000 v3.3.3"
//
// ADC setup
//
// ADC channel assignment
#define ADC_VIN_CH   0  // ADC0 - Vin to the board
#define ADC_IP_CH    1  // ADC1 - Pump current (I)
#define NSAMPLES   256  // if more than 64, adc_sum needs to be uint32_t
#define NCHANNELS    2
// ADC0 calibration points - raw ADC readings and "centi-volts" (1/100 of a volt)
#define ADC0_RAW1_DEF       0
#define ADC0_CAL1_DEF       0
#define ADC0_RAW2_DEF     614
#define ADC0_CAL2_DEF    1200
// ADC1 calibration points - raw ADC readings and "centi-amps" (1/100 of an amp)
#define ADC1_RAW1_DEF      127
#define ADC1_CAL1_DEF        0
#define ADC1_RAW2_DEF      820
#define ADC1_CAL2_DEF     5000

// Slope and intercept formulas for ADCs
#define ADC_M_FUNC(cal1,raw1,cal2,raw2) ( (((long)cal2 - (long)cal1)*8192) / ((long)raw2 - (long)raw1) )
#define ADC_B_FUNC(cal1,raw1,ADCM) ( ((long)cal1*8192) - ((long)ADCM*(long)raw1) )
#define ADC_TO_CAL(adc,adcm,adcb) ( (uint16_t)((((long)adcm*adc) + (long)adcb) / 8192) )
#define CAL_TO_ADC(cal,adcm,adcb) ( (uint16_t)((((long)cal * 8192) - (long)adcb) / (long)adcm) )

// Default threshold values
// These are calibrated values! (centi-volts and centi-amps)
// Calibrated values are stored as defaults, RAW values are used at run-time.
#define VIN_ALARM_THRESH_CAL_DEF   1050
#define VIN_OK_THRESH_CAL_DEF      1150
#define IP_ON_THRESH_CAL_DEF        150
#define IP_OFF_THRESH_CAL_DEF        60


//
// DIO outputs
//
#define AUX_PORT   PORTA
#define AUX_BIT    6
#define AUX_DDR    DDRA
#define AUX_PIN    PINA

#define RLY1_PORT  PORTA
#define RLY1_BIT   7
#define RLY1_DDR   DDRA
#define RLY1_PIN   PINA

#define LCDBL_PORT PORTC
#define LCDBL_BIT  6
#define LCDBL_DDR  DDRC
#define LCDBL_PIN  PINC

#define LGHT_PORT  PORTC
#define LGHT_BIT   7
#define LGHT_DDR   DDRC
#define LGHT_PIN   PINC

#define STK_PORT   PORTD
#define STK_BIT    7
#define STK_DDR    DDRD
#define STK_PIN    PIND

//
// DIO inputs
//
// interrupts per milliseconds
// Depends on clock and interrupt timer settings
# define INT_PER_MS 2
// DIOs for keypad
#define KP_PORT PORTB
#define KP_PIN  PINB
#define KP_R1     0
#define KP_R2     1
#define KP_R3     2
#define KP_R4     3
#define KP_C1     4
#define KP_C2     5
#define KP_C3     6
#define KP_C4     7
#define MTN_PORT  PORTA
#define MTN_PIN   PINA
#define MTN_DDR   DDRA
#define MTN_BIT   5
#define FLM_PORT  PORTD
#define FLM_PIN   PIND
#define FLM_DDR   DDRD
#define FLM1_BIT  4
#define FLM2_BIT  5
#define DRSN_PORT PORTD
#define DRSN_PIN  PIND
#define DRSN_DDR  DDRD
#define DRSN_BIT  6
#define AUXI_PORT   PORTA
#define AUXI_BIT    4
#define AUXI_DDR    DDRA
#define AUXI_PIN    PINA

// bit mask for the keypad input bits
#define KP_INPUTS  ((1 << KP_R1) | (1 << KP_R2) | (1 << KP_R3) | (1 << KP_R4))
// bit mask for the keypad output bits
#define KP_OUTPUTS ((1 << KP_C1) | (1 << KP_C2) | (1 << KP_C3) | (1 << KP_C4))
#define KP_DDR     DDRB
#define FLM_BITS  ((1 << FLM1_BIT) | (1 << FLM2_BIT))
// NaPion is *NOT* open-collector
// May need pull-down!
// #define PORTA_PULLUPS  (1 << MTN_BIT)
// Bits 2 and 3 and 4 are led to an ADC header, but have no software function
#define PORTA_PULLUPS  ((1 << 2) | (1 << 3))
#define PORTA_PULLDOWNS  (1 << MTN_BIT)
#define PORTB_PULLUPS  KP_INPUTS
#define PORTB_PULLDOWNS  KP_OUTPUTS
// Since hardware 3.3.2, DIOs are tied to a comparator
//#define PORTD_PULLUPS  ((1 << FLM1_BIT) | (1 << FLM2_BIT) | (1 << DRSN_BIT))
#define PORTD_PULLUPS  (0)
#define PORTD_PULLDOWNS  (0)

// DIO debounce counter values (# of interrupts)
// Worst case flowmeter calculation:
//  20 GPM = 0.33 GPS
// flowmeter with 2x Hall-effect switches, ~200 k-factor
// 400 edges per gallon at each HE
//  0.33 GPS * 400 edges/gal = 133 edges/sec
//  2000 DIO polls/sec / 133.33 edges/second = 15 polls/edge
// Original version had two counts, but recorded glitches on pump start/stop.
#define FLM_CNT_MAX   6
#define KP_CNT_MAX   32
#define DRSN_CNT_MAX 32
// MTN_CNT_MAX is run-time settable (see MTN_STOPPED_CNT_MAX and MTN_PRESENT_CNT_MAX)

// Bits in DIO_DB, DIO_NS and DIO_LAST
// These implement the debounce counters
#define FLM1_DB  0
#define FLM2_DB  1
#define MTN_DB   2
#define R1_DB    3
#define R2_DB    4
#define R3_DB    5
#define R4_DB    6
#define DRSN_DB  7
#define FLM_DBS  ((1 << FLM1_DB) | (1 << FLM2_DB))
#define KP_DBS ((1 << R1_DB) | (1 << R2_DB) | (1 << R3_DB) | (1 << R4_DB))

// bits in the DIO_STATUS register
#define KEY_PRESSED   0
#define FLM_CHANGED   1
#define MTN_CHANGED   2
#define MTN_LOGIC     3
#define MTN_PRESENT   4
#define DRSN_CHANGED  5
#define DRSN_LOGIC    6
#define DRSN_OPEN     7
#define DIO_LOGIC_BITS ((1 << MTN_LOGIC) | (1 << DRSN_LOGIC))
// Set default DIO logic to both positive.(door open on high, motion on high)
#define DIO_LOGIC_DEF ((1 << MTN_LOGIC) | (1 << DRSN_LOGIC))

// bits in the API_STATUS register
// This register is used to flag which things need to be set/output
// through the rs232 interface.
#define DO_FLM_CUR 0
#define DO_FLM_TOT 1
#define DO_FLM_MS  2
#define DO_MTN_MS  3
#define DO_MSR     4
#define DO_LCD1    5
#define DO_LCD2    6
#define DO_RESET   7

// bits in the API_STATUS2 register
// This register is used to flag which things need to be set/output
// through the rs232 interface.
// These are the DIOs
#define DO_AUX     0
#define DO_LCDBL   1
#define DO_RLY1    2
#define DO_LGHT    3
#define DO_STK     4
#define DO_MTN     5
#define DO_DRSN    6
#define DO_NOTHING 7

// bits in the ADC_STATUS register - one per ADC
#define ALRM_CHG  0  // alarm status changed
#define ALRM      1  // alarm is on (different meanings for ADC0 and ADC1)
#define DO_THR    2  // get/set alarm threshold values
#define DO_CAL    3  // get/set ADC calibration settings
#define RAW_OUT   4  // request to output raw ADC reading
#define VAL_OUT   5  // request to output calibrated value reading

//
// Default debounce counts for runtime-settable counters
//
// MTN_STOPPED_CNT_DEF For a stopped -> started transition
// MTN_PRESENT_CNT_DEF For a motion present -> motion stopped transition
// _DEF are in ms.
#define MTN_STOPPED_CNT_DEF 4 // 4ms
#define MTN_PRESENT_CNT_DEF 10000 // 10 s
// Flowmeter speed-limit
#define FLM_EVT_TIMER_DEF 100 // 100 ms 

// String sizes
#define CMD_SIZE 8
#define MAX_READ_BUF_U32 10
#define MAX_READ_BUF_U16  6
#define MAX_READ_BUF_U8   4
#define MAX_READ_BUF_1_0  1
// Track1 max is 76 (no sentinels or LRC)
// Track2 max is 37 (no sentinels or LRC)
#define MSR1_MAX 79
#define MSR2_MAX 40

//
// ADC info structure
//
typedef struct {
	uint8_t CH; // the ADC channel for this struct
	volatile uint16_t avg;
	volatile uint32_t sum;

	// Calibration and raw values
	// Calibrated values are in centi-volts (1/100 volt) and centi-amps (1/100 amp)
	volatile uint16_t raw1;
	volatile uint16_t cal1;
	volatile uint16_t raw2;
	volatile uint16_t cal2;

	// Slope and intercepts for ADCs
	volatile int32_t cal_m,cal_b;
	// Raw ADC value to produce a calibrated value of 0
	// Check the raw reading against this value before reporting
	// a calibrated value to avoid reporting calibrated values below 0.
	volatile uint16_t MIN;

	// Thresholds for ADCs - RAW ADC values (not calibrated)
	volatile uint16_t ON_THRESH;
	volatile uint16_t OFF_THRESH;
	// These are the calibrated thresholds
	// The raw values are determined from these whenever
	// they are changed, and whenever the ADC calibrations are changed.
	volatile uint16_t ON_THRESH_CAL;
	volatile uint16_t OFF_THRESH_CAL;
	// Status register (see bit definitions above)
	volatile uint8_t  STATUS;
} ADC_info_t;


//
// LCD info structure
//
#define LCD_CHARS_MAX 80
#define LCD_SCROLL_INTR 500 // Number of interrupts between scrolls
typedef struct {
	volatile char line[LCD_CHARS_MAX+1];
	volatile uint8_t scrollPos,len;
} LCD_info_t;

//
// Global SmartIO structure
//
typedef struct {
	ADC_info_t ADC0;
	ADC_info_t ADC1;
	volatile uint8_t theCh; // keeps track of current ADC channel (the one being sampled)

	// DIO status register
	volatile uint8_t DIO_STATUS;

	// DIO API Status register
	volatile uint8_t API_STATUS;
	volatile uint8_t API_STATUS2;

	// API overflow counter
	// This counts how many times we were too busy to read serial input.
	// not a count of dropped characters, but screwed up command fields
	volatile uint32_t API_overflow;

	// Motion counter limits
	volatile uint16_t MTN_STOPPED_CNT_MAX;
	volatile uint16_t MTN_PRESENT_CNT_MAX;
	
	// Current keypad key
	volatile char KP_KEY;

	// Flowmeter counters
	volatile uint32_t FLM_CUMULATIVE;
	volatile uint32_t FLM_CURRENT;
	volatile uint16_t FLM_EVT_TIMER;
	volatile uint16_t FLM_EVT_TIMER_MAX;

	// Space to store 4 uint16s as strings
	volatile char int1_buf[MAX_READ_BUF_U16+1],int2_buf[MAX_READ_BUF_U16+1];
	volatile char int3_buf[MAX_READ_BUF_U16+1],int4_buf[MAX_READ_BUF_U16+1];
	// Space to store 1 uint32 as string
	volatile char int32[MAX_READ_BUF_U32+1];
	// Space to store output strings
	volatile char int16Out_buf[MAX_READ_BUF_U16+1];
	volatile char int32Out_buf[MAX_READ_BUF_U32+1];
	volatile char MSR1[MSR1_MAX],MSR2[MSR2_MAX];
	LCD_info_t LCD[2];
	volatile uint16_t lcd_intr_c;
	volatile PGM_P cmd_lbl;
} SmartIO_info_t;
extern SmartIO_info_t SmartIOinfo;
extern char EOL[], TAB[];


void initTimer (void);
void initADC (void);
void initDIO (void);
void initVars (void);
char getKey (uint8_t r, uint8_t c);
uint16_t getADC (void);
void uint16_to_string (uint16_t value_E1, char pos, char *destination);
uint16_t string_to_uint16 (char *string, volatile uint16_t fact);


#endif /* SmartIO_h */
