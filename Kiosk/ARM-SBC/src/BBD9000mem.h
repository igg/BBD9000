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
#ifndef BBD9000mem_h
#define BBD9000mem_h



#include <sys/types.h>
#include <sys/time.h>
#include <signal.h>

// For stringifying numeric constants at compile time
// e.g.:
// #define SIZE 4
// char foo[dcl(SIZE)]
// sscanf (string,"%" xstr(SIZE) "s", foo);
// NO!:
// sscanf (string,"%" xstr(SIZE-1) "s", foo);
// becomes sscanf (string,"%4-1s",foo);
#define xstr(s) str(s)
#define str(s) #s


#define BBD9000conf_def "BBD9000.conf"
/*
  Name of the POSIX shared memory object for:
  int shm_open(const char *name, int oflag, mode_t mode);
  This object should be created exactly once - by a boot script
  shmem = shm_open(BBD9000MEM, O_RDWR, O_CREAT|O_TRUNC);
  This call was successfull if shmem >= 0
  The BBD9000mem struct should be initialized with default values
  and written to shmem, followed by a call to close(shmem).
  After initalization, other processes can call
  shmem = shm_open(BBD9000MEM, O_RDWR, 0);
  and then either read(), write(), or mmap() shmem.
*/

#define BBD9000ID_FILE "/etc/BBD9000ID"

#define BBD9000netlink    "netlink" // binary, relative to shmem->root_path
// This contains an adjtimex command to set the frequency and ticks for the system clock
#define BBD9000timeDrift  "/etc/init.d/adjtimex"
// These are relative to BBD9000root
#define BBD9000SmartIOhex        "SmartIO/SmartIO.hex"
#define BBD9000SmartIObootloader "SmartIO/bootloader"
#define BBD9000reboot "/sbin/shutdown -t 5 -r now"
#define BBD9000restart "/etc/init.d/BBD9000 restart"
#define BBD9000_PWR_CMD "/etc/init.d/BBD9000 power-alrm"
#define BBD9000_PWR_OK_CMD "/etc/init.d/BBD9000 power-ok"
#define SIGPWRALRM SIGUSR1
#define SIGPWROK SIGUSR2

// Name of the watchdog timer
// Needs to be known by BBD9000timer and BBD9000fsm
#define WATCHDOG_TIMER "REBOOT"
// TIMEOUT is how long until reboot
#define WATCHDOG_TIMEOUT 60 // Seconds!!!
// RESET is how often we update the timer
#define WATCHDOG_RESET 30 // Seconds!!!

#define NAME_SIZE  32
#define EVT_VALUE_SIZE 256
#define N_EVT 21
typedef struct {
	char name[NAME_SIZE];
	char value[EVT_VALUE_SIZE];
	struct timeval time;
} event;

/*
  For a 1024 bit key, the message is 128 bytes - 42 bytes padding = 86 bytes.
  Messages bigger than this will use more RSA blocks, each 86 bytes.
*/
#define RSA_SIZE 1024
#define RSA_BLOCK_SIZE (RSA_SIZE/8)
#define RSA_TEXT_SIZE (RSA_BLOCK_SIZE-42)

#define BIG_BUFFER_SIZE 4096
#define PARAM_SIZE 64
#define PATH_SIZE 128
#define STR_SIZE 256
#define READ_BUF_SIZE 511
#define MD5_SIZE 36

#define CC_RESP_CODE_SIZE 16
#define CC_MESSAGE_SIZE 64
typedef struct {
	char  code;
	short reas_code;
	char  message[CC_MESSAGE_SIZE];
	char  auth_code[CC_RESP_CODE_SIZE];
	char  trans_id[CC_RESP_CODE_SIZE];
	char  MD5_hash[CC_MESSAGE_SIZE];
	char  doVoid; // set to true if we're trying to void this transaction
	char  error[CC_MESSAGE_SIZE]; // This is for any local errors we're receiving.
} cc_resp_s;

typedef struct {
	char  path [PATH_SIZE];
	char srcMD5[MD5_SIZE];
	char dstMD5[MD5_SIZE];
} patch;
#define MAX_PATCHES 32

#define FLOWMETER_BUFF_SIZE 32

typedef struct {
	int raw1;
	float cal1;
	int raw2;
	float cal2;
} ADC_cal;
/*
  The structure in BBD9000MEM
*/
#define BAD_TEMP         99999
#define KEYPAD_BUFF_SIZE    64
#define MIN_OP_SPN_LENGTH    4
#define MAX_OP_SPN_LENGTH   20
#define LCD_LINE_SIZE       20
#define LCD_MAX_LINE_SIZE   80
#define URL_SIZE           128
#define MSR_BUF_SIZE       128
#define MEMB_INFO_LNGTH     16
#define STATUS_SIZE         32

typedef struct {
	char root_path [PATH_SIZE];
	u_int32_t  kiosk_id;
	char       kiosk_name[NAME_SIZE];
	char       coop_name[NAME_SIZE];
	char       status[STATUS_SIZE];
	int        status_idx;
	// This is set to "restart" by default.
	// fsm can set this to "power" or "patch" also.
	char       boot_reason[STR_SIZE];
	// This is the status at boot.
	char       boot_state[STR_SIZE];
	// This is the time() at which the run-time configuration was saved
	time_t     boot_time;
	int        delta_t; // time correction for the timer process
	
	struct timeval    t_start;
	/* last update (absolute time) */
	struct timeval    t_update;

	/* file paths */
	char BBD9000conf[PATH_SIZE];
	char BBD9000LOG[PATH_SIZE];
	char run_conf[PATH_SIZE];
	char cal_conf[PATH_SIZE];
	char BBD9000key[PATH_SIZE];
	char BBD9000srv_key[PATH_SIZE];
	char BBD9000ccGWconf[PATH_SIZE];
	char BBD9000patch[PATH_SIZE];
	char BBD9000run[PATH_SIZE];
// Where all the FIFOs are (set up by BBD9000cfg/conf_cfg_read() from BBD9000run)
// BBD9000init starts sub-processes with a parameter to the main shared memory segment
	char BBD9000mem[PATH_SIZE];
	char BBD9000out[PATH_SIZE];
	char BBD9000evt[PATH_SIZE];
	char BBD9000tim[PATH_SIZE];
	char BBD9000srv[PATH_SIZE];
	char BBD9000ccg[PATH_SIZE];
	char BBD9000net[PATH_SIZE];

	/* SmartIO device and baudrate */
	char SmartIOdev[PATH_SIZE];
	int SmartIObaud;
	char SmartIOsync; // set to true once we've synchronized with SmartIO
	
	/* modem device */
	char Modemdev[PATH_SIZE];
	

	/* Board temperature in Celicius and Fahrenheit */
	float      temp_c, temp_f;
	/* Interval for sampling ADC */
	u_int32_t  smartIO_interval;
	/* counter for number of skipped samples */
	u_int32_t  temp_missed_frames;

	/* ADC stuff */
	float voltage;
	float current;
	struct timeval t_voltage, t_current;
	ADC_cal ADC0_cal, ADC1_cal;

	
	// pump on?
	char pump;
	// event mask
	char pump_evt;
	// Voltage alarm?
	char valarm;
	
	// Threshold currents for pump on and pump off events
	float pump_on_threshold;
	float pump_off_threshold;
	
	// Threshold voltages for voltage alarms
	float valrm_on_threshold;
	float valrm_off_threshold;

	/* keypad stuff */
	char keypad_buffer[KEYPAD_BUFF_SIZE];
	/* This is the hashed operator SPN */
	char operator_code[STR_SIZE];
	
	/* motion */
	char motion;

	/* door */
	char door_open;
	char has_DRSN;

	/* lcd stuff */
	char LCD1[LCD_MAX_LINE_SIZE+1];
	char LCD2[LCD_MAX_LINE_SIZE+1];
	
	/* msr */
	char msr_track1[MSR_BUF_SIZE];
	char msr_track2[MSR_BUF_SIZE];
	time_t msr_exp;
	char msr_name[MSR_BUF_SIZE];
	char msr_CCname[MSR_BUF_SIZE];
	char msr_last4[5];
	
	
	/* Relays (wether or not being driven) */
	char LightsRly;
	char StrikeRly;
	char PumpRly;
	char AuxRly;

	/* Twilight calculations */
	double     lon, lat;
	time_t twilight_start;
	time_t twilight_end;
	
	/* Fuel monitoring */
	u_int32_t  flowmeter_counts;
	struct timeval    t_update_flowmeter;
	float max_counts_per_ms;
	// The flowmeter K factor
	float      flowmeter_pulses_per_gallon;
	double     cumulative_gallons;
	u_int32_t  tank_capacity;
	float      avail_gallons;
	int        low_fuel_alarm;
	int        no_fuel_cutoff;
	float      last_ppg;
	char       fuel_type[8];

	/* Server URL */
	char       server_URL[URL_SIZE];
	u_int32_t  server_msg_size;
	// Unlike most time settings, this is in SECONDS!!
	u_int32_t  network_timeout;
	char       patch_server[URL_SIZE];

	// networks active
	char server;
	char gateway;
	char serverSync; // BBD9000server sets this once it determines network state
	char fsmSync; // BBD9000fsm sets this once its ready to accept messages
	// Statuses from netlink
	char net_status[LCD_MAX_LINE_SIZE];
	char net_status_cellular[LCD_MAX_LINE_SIZE];
	char net_status_ethernet[LCD_MAX_LINE_SIZE];
	char net_status_WiFi[LCD_MAX_LINE_SIZE];
	char net_error[LCD_MAX_LINE_SIZE]; // error from server comms
	char gw_error[LCD_MAX_LINE_SIZE]; // error from gateway comms

	// last event is NULL_EVENT
	event event_queue[N_EVT];

	// Various timeouts during operation
	u_int32_t  status_interval;         // the one being used currently
	u_int32_t  status_interval_net;     // when we have a network
	u_int32_t  status_interval_no_net;  // when we have no network
	u_int32_t  max_status_interval_no_net;  // maximum time to wait between retries
	time_t     last_status;             // time() of last status
	u_int32_t  dispensing_timeout;
	u_int32_t  input_timeout;
	u_int32_t  lcd_timeout;
	u_int32_t  lcd_refresh_timeout;
	u_int32_t  strike_rly_timeout;
	u_int32_t  CC_reswipe_timeout;
	u_int32_t  door_close_timeout;
	u_int32_t  smartIO_timeout;
	u_int32_t  netIdle_timeout;

	// Membership costs
	float renewal_fee;
	float full_membership_fee;
	float temp_membership_fee;
	float trial_membership_surcharge;
	float upgrade_fee;

	// This information is filled-in during dispensing
	int    memb_id;
	char   memb_type[MEMB_INFO_LNGTH];
	char   memb_number[MEMB_INFO_LNGTH];
	char   memb_status[MEMB_INFO_LNGTH];
	char   memb_spn[KEYPAD_BUFF_SIZE];
	int    memb_fuel_pre_auth;
	float  memb_pre_auth;
	char   memb_renewal_sale;
	char   memb_full_membership_sale;
	char   memb_temp_membership_sale;
	char   memb_upgrade_sale;
	char   is_violation;
	float  memb_ppg;
	float  memb_credit;
	float  memb_gallons;
	float  memb_dollars;

	// CC processing threshold (dollars)
	float cc_threshold;

	// Gateway response info
	cc_resp_s cc_resp;
	
	// Gateway access string
	char GW_string[STR_SIZE];
	
	// Server does gateway interactions
	int  server_GW;
	
	// Software patch information
	int npatches;
	patch patches[MAX_PATCHES];
	char patch_status[STR_SIZE];

	// network status says its time to checkin via SMS
	char checkin_msg;
	
	// Kiosk scheduled maintenance.  Server message:
	// maint	t_start	t_end
	// both set to 0 when time() is > sched_maint_end;
	// kiosk is off line while sched_maint_start < time() > sched_maint_end
	// if (sched_maint_start  && time() < sched_maint_start), kiosk displays:
	// --------------------
	// Baltimore Biodiesel. Sched. Maint.: mm/dd hha-hhp!
	// Fuel info...
	// --------------------
	//
	// if (sched_maint_start > time() && sched_maint_end < time()), kiosk displays:
	// --------------------
	// Baltimore Biodiesel
	// Scheduled Maintenance: mm/dd hha-hhp
	// --------------------
	time_t sched_maint_start;
	time_t sched_maint_end;
	
} BBD9000mem;

#define SHMEM_SIZE ( (1 + (sizeof (BBD9000mem) / getpagesize()))*getpagesize() )

// struct timeval utility macros
#define TV_ADD_MS(x, y)	{ \
	(x).tv_usec += (y) * 1000; \
	(x).tv_sec += (x).tv_usec / 1000000; \
	(x).tv_usec = (x).tv_usec % 1000000; \
}

#define TV_GTE(x, y) ((x).tv_sec > (y).tv_sec || ((x).tv_sec == (y).tv_sec && \
	(x).tv_usec >= (y).tv_usec)) 

#define TV_ELAPSED_MS(x, y)	((((x).tv_sec - (y).tv_sec) * 1000000 + \
	((x).tv_usec - (y).tv_usec)) / 1000)

#define ABS(x) (x > 0 ? x : -x)
#define FABS(x) (x > 0.0 ? x : -x)


#endif /* BBD9000mem_h */

