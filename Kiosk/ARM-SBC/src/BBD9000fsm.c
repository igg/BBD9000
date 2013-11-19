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
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <sys/time.h>
#include <stdio.h>
#include <fcntl.h>
#include <assert.h>
#include <time.h>
#include <errno.h>
#include <ctype.h>
#include <sys/stat.h>
#include <string.h>
#include <stdarg.h>
#include <crypt.h>
#include <signal.h>



#include "BBD9000mem.h"
#include "BBD9000cfg.h"


// Signal handler
void sigTermHandler (int signum);
void sigPipeHandler (int signum);
unsigned sigPipeCounter=0;

// State handler prototypes

// Our state function prototypes
// These take an event number and value as parameters, returning a new state number.
int doGlobal          (const int evt, const char *val);
int doIdle            (const int evt, const char *val);
int doOperator        (const int evt, const char *val);
int doShutdown        (const int evt, const char *val);
int doNoFuel          (const int evt, const char *val);
int doNoServer        (const int evt, const char *val);
int doUserLogin       (const int evt, const char *val);
int doServerAuth      (const int evt, const char *val);
int doAskRenew        (const int evt, const char *val);
int doAskBuyMemb      (const int evt, const char *val);
int doAskUpgrade      (const int evt, const char *val);
int doBuyMemb         (const int evt, const char *val);
int doSPNConf         (const int evt, const char *val);
int doAskRenewExpired (const int evt, const char *val);
int doRenewExpired    (const int evt, const char *val);
int doGatewayPreAuth  (const int evt, const char *val);
int doAskDeclined     (const int evt, const char *val);
int doMSRReswipe      (const int evt, const char *val);
int doSetPreAuth      (const int evt, const char *val);
int doRegNewMemb      (const int evt, const char *val);
int doDispensing      (const int evt, const char *val);
int doGatewayCapture  (const int evt, const char *val);
int doWaitDoorClosed  (const int evt, const char *val);
int doCalMenu         (const int evt, const char *val);
int doCalOnAmps       (const int evt, const char *val);
int doCalOffAmps      (const int evt, const char *val);
int doCalKFact        (const int evt, const char *val);
int doSetTankCap      (const int evt, const char *val);
int doSetLowFuel      (const int evt, const char *val);
int doSetNoFuel       (const int evt, const char *val);
int doSetOperSPN      (const int evt, const char *val);
int doConfOperSPN     (const int evt, const char *val);
int doSetAvailGal     (const int evt, const char *val);
int doSetFuelType     (const int evt, const char *val);
int doSetPPG          (const int evt, const char *val);
int doShowStatus      (const int evt, const char *val);
int doShowNetStatus   (const int evt, const char *val);

// Assign handler functions to state names
// Note that state numbers correspond to this array index.
// The first state handler (index 0) is the global event handler that
// gets called for every event regardless of state.
// This handler can change the state of the system.
// The system's state handler will be called after the global one.
// A state of 0 is not valid.
#define Idle_State             1
#define Operator_State         2
#define Shutdown_State         3
#define NoFuel_State           4
#define NoServer_State         5
#define UserLogin_State        6
#define ServerAuth_State       7
#define AskRenew_State         8
#define AskBuyMemb_State       9
#define AskUpgrade_State      10
#define BuyMemb_State         11
#define SPNConf_State         12
#define AskRenewExpired_State 13
#define RenewExpired_State    14
#define GatewayPreAuth_State  15
#define AskDeclined_State     16
#define MSRReswipe_State      17
#define SetPreAuth_State      18
#define RegNewMemb_State      19
#define Dispensing_State      20
#define GatewayCapture_State  21
#define WaitDoorClosed_State  22
#define CalMenu_State         23
#define CalOnAmps_State       24
#define CalOffAmps_State      25
#define CalKFact_State        26
#define SetTankCap_State      27
#define SetLowFuel_State      28
#define SetNoFuel_State       29
#define SetOperSPN_State      30
#define ConfOperSPN_State     31
#define SetAvailGal_State     32
#define SetFuelType_State     33
#define SetPPG_State          34
#define ShowStatus_State      35
#define ShowNetStatus_State   36


typedef struct {
	char name[NAME_SIZE];
	int (*handler)(const int evt, const char *val);
	int can_enter_operator; // numeric keypad input cannot be confused with operator code
} state_hndlr;

// These are in the same order as the state defines above!
state_hndlr state_handlers[] = {
	{"Global"           , doGlobal          , 1},
	{"Idle"             , doIdle            , 1},
	{"Operator"         , doOperator        , 0},
	{"Shutdown"         , doShutdown        , 1},
	{"No Fuel"          , doNoFuel          , 1},
	{"No Server"        , doNoServer        , 1},
	{"User Login"       , doUserLogin       , 0},
	{"Server Auth"      , doServerAuth      , 1},
	{"Ask Renew"        , doAskRenew        , 1},
	{"Ask Buy Memb"     , doAskBuyMemb      , 1},
	{"Ask Upgrade"      , doAskUpgrade      , 1},
	{"Buy Memb"         , doBuyMemb         , 1},
	{"SPN Conf"         , doSPNConf         , 0},
	{"Ask Renew Expired", doAskRenewExpired , 1},
	{"Renew Expired"    , doRenewExpired    , 1},
	{"Gateway Pre-Auth" , doGatewayPreAuth  , 1},
	{"Ask Declined"     , doAskDeclined     , 1},
	{"MSR Reswipe"      , doMSRReswipe      , 1},
	{"Set Pre-Auth"     , doSetPreAuth      , 0},
	{"Reg New Memb"     , doRegNewMemb      , 1},
	{"Dispensing"       , doDispensing      , 1},
	{"Gateway Capture"  , doGatewayCapture  , 1},
	{"Wait Door Closed" , doWaitDoorClosed  , 1},
	{"Cal Menu"         , doCalMenu         , 0}, // These are operator sub-states, so
	{"Cal On Amps"      , doCalOnAmps       , 0}, // can't enter operator
	{"Cal Off Amps"     , doCalOffAmps      , 0},
	{"Cal K Fact"       , doCalKFact        , 0},
	{"Set Tank Cap"     , doSetTankCap      , 0},
	{"Set Low Fuel"     , doSetLowFuel      , 0},
	{"Set No Fuel"      , doSetNoFuel       , 0},
	{"Set Oper SPN"     , doSetOperSPN      , 0},
	{"Conf Oper SPN"    , doConfOperSPN     , 0},
	{"Set Avail Gal"    , doSetAvailGal     , 0},
	{"Set Fuel Type"    , doSetFuelType     , 0},
	{"Set Price"        , doSetPPG          , 0},
	{"Show Status"      , doShowStatus      , 0},
	{"Show Net Status"  , doShowNetStatus   , 0}
};

// Define events and their names
// Note correspondence between the two
#define NULL_Evt                   0 // This is never put on the queue
#define EnterState_Evt             1 // This is never put on the queue
#define ExitState_Evt              2 // This is never put on the queue
#define VoltageAlarm_Evt           3
#define VoltageAlarm_Reset_Evt     4
#define ServerTimeout_Evt          5
#define ServerError_Evt            6
#define ServerOK_Evt               7
#define StatusTimeout_Evt          8
#define KeypadValid_Evt            9
#define MSR_Evt                   10
#define Keypad_Evt                11
#define AuthenticationSuccess_Evt 12
#define DoorOpened_Evt            13
#define DoorClosed_Evt            14
#define StrikeRlyTimeout_Evt      15
#define Flowmeter_Evt             16
#define PumpOFF_Evt               17
#define PumpON_Evt                18
#define DispensingTimeout_Evt     19
#define AuthenticationFailed_Evt  20
#define MembershipExpired_Evt     21
#define MotionDetected_Evt        22
#define MotionStopped_Evt         23
#define InputTimeout_Evt          24
#define LCDTimeout_Evt            25
#define LCDRefreshTimeout_Evt     26
#define CCprocessor_Evt           27
#define CCReswipeTimeout_Evt      28
#define DoorCloseTimeout_Evt      29
#define OperatorTimeout_Evt       30
#define OutOfFuel_Evt             31
#define FuelLevel_Evt             32
#define GW_OK_Evt                 33
#define GW_Accepted_Evt           34
#define GW_Busy_Evt               35
#define GW_CCTypeRejected_Evt     36
#define GW_Declined_Evt           37
#define GW_CardExpired_Evt        38
#define GW_TransLost_Evt          39
#define GW_Error_Evt              40
#define GW_Timeout_Evt            41
#define GW_CURLErr_Evt            42
#define GW_RetryTimeout_Evt       43
#define Patch_Evt                 44
#define SmartIOTimeout_Evt        45
#define ServerRespTimeout_Evt     46
// N.B.: This is the timeout to reset the watchdog timer, 
// When the actual WATCHDOG_TIMER expires, we reboot.
#define WatchdogRstTimeout_Evt    47

#define ChangeTime_Evt            48
#define NetStatus_Evt             49
#define NetIdleTimeout_Evt        50

#define TestTimeout_Evt           51

// Note that this array gets sorted for getting IDs by name

typedef struct {
	char name[NAME_SIZE];
	int  ID;
} evt_def;

evt_def evt_defs[] = {
	{"NULL"                  , NULL_Evt                  },
	{"Enter State"           , EnterState_Evt            },
	{"Exit State"            , ExitState_Evt             },
	{"Voltage Alarm"         , VoltageAlarm_Evt          },
	{"Voltage Alarm Reset"   , VoltageAlarm_Reset_Evt    },
	{"Server Timeout"        , ServerTimeout_Evt         },
	{"Server Error"          , ServerError_Evt           },
	{"Server OK"             , ServerOK_Evt              },
	{"Status Timeout"        , StatusTimeout_Evt         },
	{"Keypad Valid"          , KeypadValid_Evt           },
	{"MSR"                   , MSR_Evt                   },
	{"Keypad"                , Keypad_Evt                },
	{"Authentication Success", AuthenticationSuccess_Evt },
	{"Door Opened"           , DoorOpened_Evt            },
	{"Door Closed"           , DoorClosed_Evt            },
	{"StrikeRly Timeout"     , StrikeRlyTimeout_Evt      },
	{"Flowmeter"             , Flowmeter_Evt             },
	{"Pump OFF"              , PumpOFF_Evt               },
	{"Pump ON"               , PumpON_Evt                },
	{"Dispensing Timeout"    , DispensingTimeout_Evt     },
	{"Authentication Failed" , AuthenticationFailed_Evt  },
	{"Membership Expired"    , MembershipExpired_Evt     },
	{"Motion Detected"       , MotionDetected_Evt        },
	{"Motion Stopped"        , MotionStopped_Evt         },
	{"Input Timeout"         , InputTimeout_Evt          },
	{"LCD Timeout"           , LCDTimeout_Evt            },
	{"LCD-Refresh Timeout"   , LCDRefreshTimeout_Evt     },
	{"CCReswipe Timeout"     , CCReswipeTimeout_Evt      },
	{"DoorClose Timeout"     , DoorCloseTimeout_Evt      },
	{"Operator Timeout"      , OperatorTimeout_Evt       },
	{"Out of Fuel"           , OutOfFuel_Evt             },
	{"Fuel Level"            , FuelLevel_Evt             },
	{"GW OK"                 , GW_OK_Evt                 },
	{"GW Accepted"           , GW_Accepted_Evt           },
	{"GW Busy"               , GW_Busy_Evt               },
	{"GW CCTypeRejected"     , GW_CCTypeRejected_Evt     },
	{"GW Declined"           , GW_Declined_Evt           },
	{"GW CardExpired"        , GW_CardExpired_Evt        },
	{"GW TransLost"          , GW_TransLost_Evt          },
	{"GW Error"              , GW_Error_Evt              },
	{"GW Timeout"            , GW_Timeout_Evt            },
	{"GW CURL Error"         , GW_CURLErr_Evt            },
	{"GW Retry Timeout"      , GW_RetryTimeout_Evt       },
	{"Patch"                 , Patch_Evt                 },
	{"SmartIO Timeout"       , SmartIOTimeout_Evt        },
	{"ServerResp Timeout"    , ServerRespTimeout_Evt     },
	{"Watchdog-Rst Timeout"  , WatchdogRstTimeout_Evt    },
	{"Change Time"           , ChangeTime_Evt            },
	{"Net Status"            , NetStatus_Evt             },
	{"Net Idle Timeout"      , NetIdleTimeout_Evt        },

	{"Test Timeout"          , TestTimeout_Evt           }
};


// Utility function prototypes
void pushEvent (const char *evt_txt);
void processEvent (const char *evt_txt);
void doStatusTimeout ();	
void doKeypadStars (int force);
void doFlowmeter ();
int checkOperator (int force);
void encrypt_SPN (char *crypt_str, char *plain_str);
int doReset ();
void doTimeout (int timeout, char *evt);
void cancelTimeout (char *evt);
void cancelTimeouts ();
void doNotice (const char *type, const char *template, ...);
void setStatus (int state_idx);
int isStatus (char state_idx);
int getEvt (const char *evt);
void checkLights();
void doLCD (int line, int force, const char *message, ...);
int doRestore ();
void logMessage (const char *template, ...);
float getMaxFlowGPM ();
int parseMSR (const char *message);
void clearMSR ();
void doGW (const char *type, float amount) ;
void doSrvCC (const char *type, float amount, char flush) ;
void doPatch ();
time_t MSRDateToEpoch(char *date);
char *getSalesList ();
char isPPPup () ;
int event_cmp (const void *e1, const void *e2);

// GLOBALS
FILE *out_fp, *log_fp, *srv_fp, *tmr_fp;
BBD9000mem *shmem;
float tank_gals_at_start = 0.0;
cfg_t *cal_cfg;
// temp fuel setting to allow canceling the whole thing
float avail_gallons;
char fuel_type[8];
float last_ppg;


// Misc. defines

#define MSR_EXPIRED -1
#define MSR_INVALID -2
#define MSR_VALID    0

#define DEBUGGING 1


int main (int argc, const char **argv) {
const char *BBD9000MEMpath;
int shmem_fd;
FILE *fifo_fp = NULL;
char buf[READ_BUF_SIZE], *read;
int nevents;


	/* vvvv Init */
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
	
	// Open the log
	log_fp = fopen (shmem->BBD9000LOG,"a");
	assert(log_fp != NULL);
	logMessage ("BBD9000fsm started");

	/* Open the pipe for reading - this blocks until a writer opens the file */
	fifo_fp = fopen(shmem->BBD9000evt, "r");
	assert (fifo_fp != NULL);

	/* open the output FIFO */
	out_fp = fopen (shmem->BBD9000out,"w");
	assert(out_fp != NULL);
	
	/* Open the server FIFO */
	srv_fp = fopen (shmem->BBD9000srv, "w");
	assert(srv_fp != NULL);

	/* Open the timer FIFO */
	tmr_fp = fopen (shmem->BBD9000tim, "w");
	assert (tmr_fp != NULL);

	// Register our signal handlers to save state
	signal (SIGTERM    , sigTermHandler);
	signal (SIGINT     , sigTermHandler);
	signal (SIGQUIT    , sigTermHandler);
	signal (SIGPIPE    , sigPipeHandler);
	// We're ignoring power signals because we're the ones initiating them
	signal (SIGPWRALRM , SIG_IGN);     /* SIGUSR1. Voltage ALRM on BBD9000 SIGPWRALRM */
	signal (SIGPWROK   , SIG_IGN);     /* SIGUSR2. Voltage ALRM RESET on BBD9000 SIGPWROK */

	// Get a calibration configuration based on shmem
	cal_cfg = cal_cfg_init (shmem);
	cal_cfg_from_mem (cal_cfg,shmem);

	// Sort our event array
	nevents = sizeof (evt_defs) / sizeof (evt_def);
	qsort (evt_defs, nevents, sizeof (evt_def), event_cmp);

	// Initialize our state
	doLCD (0, 1, NULL);
	doLCD (1, 1, shmem->coop_name);
	doRestore ();

	// Set our first watchdog reset
	doTimeout (WATCHDOG_TIMEOUT*1000,WATCHDOG_TIMER);
	doTimeout (WATCHDOG_RESET*1000,"Watchdog-Rst");

	// Initial status message will be sent because
	// the last_status was at time 0.
	// doStatusTimeout ();

	shmem->fsmSync = 1;
	logMessage ("FSM Synchronized");
	while (1) {

		/* Open the pipe for reading - this blocks until a writer opens the file */
		if (!fifo_fp) fifo_fp = fopen(shmem->BBD9000evt, "r");
		assert (fifo_fp != NULL);

		/* Read from the pipe - this blocks only if there are writers with the file open */
		read = fgets(buf, sizeof(buf), fifo_fp);
		while (read) {
			processEvent (read);
			read = fgets(buf, sizeof(buf), fifo_fp);
		} 
		// all writers exited and we're getting eofs
		fclose (fifo_fp);
		fifo_fp = NULL;
	}

	munmap(shmem, SHMEM_SIZE);
	close (shmem_fd);
	fclose (out_fp);
	fclose (srv_fp);
	fclose (tmr_fp);

	exit(EXIT_SUCCESS);
}

void pushEvent (const char *evt_txt) {
FILE *evt_fp;

	evt_fp = fopen(shmem->BBD9000evt, "a");
	if (evt_fp) {
		fprintf (evt_fp,"%s\n",evt_txt);
		fclose (evt_fp);
	}

}

void processEvent (const char *evt_txt) {
int i,evtID;
char evt[NAME_SIZE],val[EVT_VALUE_SIZE];
int old_state = shmem->status_idx;
int new_state;

// FIXME:  pump power hack. This is a hack to inhibit flowmeter events when the pump was just turned on/off
// The second half of this hack is setting the inhibit time when we get a PUMP-ON/PUMP-OFF event below.
// The third part is dealing with flowmeter counts.
// While in either inhibit window, the flowmeter count remains at 0.
// The first flowmeter count outside this window sets the count on the SmartIO to 0.
// check to see if we're inhibiting
static struct timeval t_pump;
static char format_str[64];
struct timeval t_now;
static char is_inhibit=0;
unsigned long inhibit_ms=300;
static u_int32_t flm_counts=0;
float max_cnts_per_ms=0;

	if (!format_str[0])
		// Escaaape!! Keep Escaping!!
		// This should turn into something like:
		// %4[^\t\r\n]%*[\t\r\n]%5[^\r\n]
		snprintf (format_str,64,"%%%d[^\\t\\r\\n]%%*[\\t\\r\\n]%%%d[^\\r\\n]",NAME_SIZE-1,EVT_VALUE_SIZE-1);

	*evt = *val = '\0';
	sscanf (evt_txt, format_str, evt, val);

// Get the event ID
	evtID = getEvt (evt);

	if (evtID == PumpON_Evt || evtID == PumpOFF_Evt) {
		gettimeofday(&t_pump, NULL);
		is_inhibit = 1;
		flm_counts = shmem->flowmeter_counts;
		max_cnts_per_ms = shmem->max_counts_per_ms;
		// Make sure that the inhibit expires in a timely fashion
		doTimeout (inhibit_ms,"FLM-Inhibit"); // this is an unrecognized event
	} else if (is_inhibit) {
		// inhibit on, within inhibit time and flowmeter event: ignore
		if ( evtID == Flowmeter_Evt ) {
			shmem->max_counts_per_ms = max_cnts_per_ms;
			shmem->flowmeter_counts = flm_counts;
			evtID = -1;
		}

		gettimeofday(&t_now, NULL);

		if ( TV_ELAPSED_MS (t_now,t_pump) >= inhibit_ms ) {
			is_inhibit = 0;
			shmem->max_counts_per_ms = max_cnts_per_ms;
			shmem->flowmeter_counts = flm_counts;
			fprintf (out_fp,"Flow\t%lu\n",(unsigned long)flm_counts);
			fflush (out_fp);
		}
	}

// return on invalid events
	if (evtID < 0) return;


// Update the event queue
	for (i=0; shmem->event_queue[i].name[0]; i++) {};
	if (i >= N_EVT-1) {
		memmove (&(shmem->event_queue[0]),&(shmem->event_queue[1]),sizeof (event)*(N_EVT-1));
		i--;
	}
	snprintf (shmem->event_queue[i].name,sizeof(shmem->event_queue[i].name),evt);
	snprintf (shmem->event_queue[i].value,sizeof(shmem->event_queue[i].value),val);
	gettimeofday(&(shmem->event_queue[i].time), NULL);
	
// Call the Global handler
	new_state = state_handlers[0].handler (evtID,val);

// Set our new state
	setStatus (new_state);

// Call this state's handler
// Only if the global handler didn't change state
	if (new_state == old_state)
		new_state = state_handlers[old_state].handler (evtID,val);

// Set our new state
	setStatus (new_state);

}


/*******************
  State Handlers
********************/

// This event handler gets called for all states
int doGlobal         (const int evt, const char *val) {
int new_state = shmem->status_idx;


	switch (evt) {

	case ChangeTime_Evt:
		fprintf (tmr_fp,"CHANGE\t%s\n",val);
		fflush (tmr_fp);
	break;

	case WatchdogRstTimeout_Evt:
		// If we don't reset WATCHDOG_TIMER in time, BBBD9000timer will reboot.
		doTimeout (WATCHDOG_TIMEOUT*1000,WATCHDOG_TIMER);
		doTimeout (WATCHDOG_RESET*1000,"Watchdog-Rst");
	break;

	case VoltageAlarm_Evt:
		new_state = Shutdown_State;
	break;

	// If there is still motion, the LCD should stay on.
	// If we keep the LCD on due to motion, reset the LCD timeout
	// The state handler will take care of switching the message
	// if there's motion, and an LCD timeout occurs.
	case LCDTimeout_Evt:
		if (shmem->motion) {
			checkLights();
			doTimeout (shmem->lcd_timeout,"LCD");
		} else {
			doLCD (1, 0, "");
			doLCD (2, 0, "");
			fprintf (out_fp,"LightsRly\t0\n");
			fflush (out_fp);
		}
	break;
	
	case LCDRefreshTimeout_Evt:
		// Refresh the LCD with any stored message.
		doLCD (0, 0, NULL);
	break;

	case StrikeRlyTimeout_Evt:
		fprintf (out_fp,"StrikeRly\t0\n");
		fflush (out_fp);
	break;

	// This event is only handled by the No Fuel state
	// The number of gallons available (shmem->avail_gallons) is already handled.
	// doReset must check avail_gallons when deciding what state to end up in.
	case FuelLevel_Evt:
	break;
	
	case KeypadValid_Evt:
		if ( checkOperator (0) ) {
			memset (shmem->keypad_buffer,0,sizeof(shmem->keypad_buffer));
			new_state = Operator_State;
		}
	break;


	case Flowmeter_Evt:
	// This event is handled globally to update the counters.
	// This handler also updates the display if we're dispensing or in Operator
	// If we're not in either of these states, then a violation monitor and security message sending is enabled
	// Since the event will be passed onto the state handler, additional actions can be taken there
	// for example, in Operator mode, the raw flowmeter count is also displayed.
		doFlowmeter();
	break;

	case DoorOpened_Evt:
		// Catch out-of-state door opening events
		if (! (isStatus (Dispensing_State) || isStatus (Operator_State)) ) {
			cancelTimeout ("Dispensing");
			fprintf (out_fp,"PumpRly\t0\n");
			fflush (out_fp);
			if ( !shmem->is_violation ) {
				shmem->is_violation = 1;
				doNotice ("alert","Security\tDoor opened while in %s state.",shmem->status);
				fflush (srv_fp);
			}
		}
	break;

	default:
	break;
	}
	
	return (new_state);
}

int doIdle           (const int evt, const char *val) {
int new_state = shmem->status_idx;
int MSR_status;
static char is_idle=0;
char LCD1[LCD_MAX_LINE_SIZE], LCD2[LCD_MAX_LINE_SIZE];
time_t t_now;
size_t buf_siz=0;

	// Deal with maintenance message
	t_now = time(NULL);
	if (shmem->sched_maint_start) {
		if (t_now < shmem->sched_maint_start) { // start is in the future
			buf_siz += snprintf (LCD1+buf_siz, sizeof(LCD1)-buf_siz, "%s", shmem->coop_name);
			buf_siz += strftime( LCD1+buf_siz, sizeof(LCD1)-buf_siz, " - Refueling shutdown %-m/%-e %-l",
				localtime(&(shmem->sched_maint_start)) );
			buf_siz += strftime( LCD1+buf_siz,  sizeof(LCD1)-buf_siz, "-%-l%P !", localtime(&(shmem->sched_maint_end)) );
			sprintf (LCD2,"%s $%4.2f/gal. Swipe CC to begin.",shmem->fuel_type,shmem->last_ppg);
		} else if (t_now >= shmem->sched_maint_start && t_now <= shmem->sched_maint_end) { // in window
			sprintf (LCD1,shmem->coop_name);
			buf_siz = strftime( LCD2, sizeof(LCD2), "Refueling shutdown %-m/%-e %-l",
				localtime(&(shmem->sched_maint_start)) );
			buf_siz = strftime( LCD2+buf_siz, sizeof(LCD2)-buf_siz, "-%-l%P", localtime(&(shmem->sched_maint_end)) );
		} else { // start has passed
			shmem->sched_maint_start = shmem->sched_maint_end = 0;
		}
	}

	if (!shmem->sched_maint_start) {
		sprintf (LCD1,shmem->coop_name);
		sprintf (LCD2,"%s $%4.2f/gal. Swipe CC to begin.",shmem->fuel_type,shmem->last_ppg);
	}

	switch (evt) {
	
	// Upon entry, make sure we have an LCD timeout and a status timeout
	case EnterState_Evt:
	// Perform any software patches
		doPatch();
	// To setup the status timeout, check the last time we checked in
	// Setup the status to comply with the maximum status interval.
		if ((time (NULL) - shmem->last_status) > (shmem->status_interval / 1000) || shmem->checkin_msg) {
			doStatusTimeout();
		} else {
			doTimeout (shmem->status_interval - ((time (NULL) - shmem->last_status)*1000),"Status");
		}

		doTimeout (shmem->lcd_timeout,"LCD");
		doTimeout (shmem->netIdle_timeout,"Net Idle");
		is_idle = 0;
	break;

	// Upon exit, cancel the LCD and status timeouts
	case ExitState_Evt:
		cancelTimeout ("LCD");
		cancelTimeout ("Status");
		cancelTimeout ("Net Idle");
		is_idle = 0;
	break;

	case StatusTimeout_Evt:
		is_idle = 0;
		doTimeout (shmem->netIdle_timeout,"Net Idle");
		doStatusTimeout();
	break;

	case Patch_Evt:
		is_idle = 0;
		doTimeout (shmem->netIdle_timeout,"Net Idle");
		doPatch();
	break;
	
	case ServerTimeout_Evt:
		new_state = NoServer_State;
		doLCD (1, 0, "Server Communication");
		doLCD (2, 0, "Network Timeout");
		doTimeout (shmem->lcd_timeout,"LCD");
	break;

	case ServerError_Evt:
		new_state = NoServer_State;
		doLCD (1, 0, "Server Communication");
		doLCD (2, 0, "Error: %s",shmem->net_error);
		fflush (out_fp);
		doTimeout (shmem->lcd_timeout,"LCD");
	break;

	case NetStatus_Evt:
		if (shmem->checkin_msg) {
			is_idle = 0;
			doTimeout (shmem->netIdle_timeout,"Net Idle");
			doStatusTimeout();
		}
	break;

	case NetIdleTimeout_Evt:
		doTimeout (shmem->netIdle_timeout,"Net Idle");
		fprintf (srv_fp,"net\tidle\n");
		fflush (srv_fp);
		is_idle = 1;
	break;

	// Switch to UserLogin_State on motion, MSR or Keypad
	case MSR_Evt:
		if (shmem->sched_maint_start && t_now >= shmem->sched_maint_start && t_now <= shmem->sched_maint_end) {
			doLCD (1, 1, LCD1);
			doLCD (2, 1, LCD2);
		} else {
		// Make sure the net is started
			fprintf (srv_fp,"net\tstart\n");
			fflush (srv_fp);
			is_idle = 0;
			doTimeout (shmem->netIdle_timeout,"Net Idle");
	
			doTimeout (shmem->input_timeout,"Input");
			cancelTimeout ("LCD");
			checkLights();
	
			MSR_status = parseMSR (val);
			if (MSR_status == MSR_VALID) {
				doLCD (1, 1, "%s",shmem->msr_name);
				doLCD (2, 1, "Enter SPN, then #");
				new_state = UserLogin_State;
			} else if (MSR_status == MSR_INVALID) {
				doLCD (1, 1, "Credit Card Invalid:");
				doLCD (2, 1, "Try a different CC");
			} else if (MSR_status == MSR_EXPIRED) {
				doLCD (1, 1, "Credit Card Expired:");
				doLCD (2, 1, "Try a different CC");
			}
		}
	break;

// Input timeout goes back to Idle_State
	case InputTimeout_Evt:
	break;

	// The global handler turns off the display and light if there is no motion.
	// If there is still motion, then switch messages
	// The global handler re-sets the LCD timer if there was motion
	// We do the same thing if a new motion event comes in.
	case Keypad_Evt:
	case LCDRefreshTimeout_Evt:
	case MotionDetected_Evt:
		if (!is_idle) doTimeout (shmem->netIdle_timeout,"Net Idle"); // delay going idle, but keep up once there
		// Don't do this if we got an LCDTimeout, but there is no motion detected
		if ( ! (evt == LCDRefreshTimeout_Evt && !shmem->motion) ) {
		// Effectively, this means the user is still there, but there may not be motion detected
			doTimeout (shmem->input_timeout,"LCD");
			checkLights();
			doLCD (1, 0, LCD1);
			doLCD (2, 0, LCD2);
		}
	break;

	default:
	break;
	}
	
	return (new_state);
}

int doOperator       (const int evt, const char *val) {
int new_state = shmem->status_idx;
// Hack to avoid enter/exit events when entering/exiting sub-states
// When a substate is entered, this state gets an ExitState_Evt, which we want to ignore.
// Likewise, when a substate exits back to operator,
// we want to ignore the EnterState_Evt here.
static char is_substate=0;
static char wait_door_closed=0;

// check lights on any event
	checkLights();


	if (is_substate) {
		if (evt == EnterState_Evt) {
			is_substate = 0;
			// When re-entering from a sub-state with the door closed,
			// we re-open the door.
			if (! (shmem->door_open && shmem->has_DRSN)) {
				fprintf (out_fp,"StrikeRly\t1\n");
				doTimeout (shmem->strike_rly_timeout,"StrikeRly");
				fflush (out_fp);
			}
			return (new_state);
		} else if (evt == ExitState_Evt) {
			return (new_state);
		}
	}

	switch (evt) {


	// Upon entry, clear any violation, reset fuel totals, set the strike relay, cacled LCD and Input timeouts
	case EnterState_Evt:
		// Clear the violation flag
		if (shmem->is_violation) {
			if (shmem->is_violation > 1 && tank_gals_at_start != 0) {
				doNotice ("alert","Security\tUnauthorized dispensing stopped (Operator). "
					"Available: %.3fg. total: %.3fg",shmem->avail_gallons, tank_gals_at_start - shmem->avail_gallons);
			}
			shmem->is_violation = 0;
		}
		tank_gals_at_start = shmem->avail_gallons;
		shmem->flowmeter_counts = 0;
		shmem->memb_credit = shmem->memb_dollars = shmem->memb_ppg = shmem->memb_gallons = 0.0;
	
		fprintf (out_fp,"Flow\t0\n");
	
		doTimeout (shmem->strike_rly_timeout,"StrikeRly");

		// 4-hour operator timeout
		doTimeout (14400000,"Operator");
	
		cancelTimeout ("Input");
		cancelTimeout ("LCD");
	
	
		doNotice ("notice","Operator mode started: %.3fg",tank_gals_at_start);
		fflush (srv_fp);

	
		doLCD (1, 1, "Operator Mode ON");
		if (! (shmem->door_open && shmem->has_DRSN) ) {
			doLCD (2, 1, "Open Door: Push&Pull");
		} else {
			doLCD (2, 1, "%5.1f $%4.2f $%7.2f",shmem->memb_gallons,shmem->memb_ppg,shmem->memb_dollars);
		}
		fprintf (out_fp,"PumpRly\t1\n");
		fprintf (out_fp,"StrikeRly\t1\n");
		fflush (out_fp);

		wait_door_closed = 0;
	break;

	// a sub-state may have put a message on the LCD, this refreshes it for our state
	case LCDRefreshTimeout_Evt:
		if (! (shmem->door_open && shmem->has_DRSN) && shmem->StrikeRly) {
			doLCD (1, 0, "Operator Mode ON");
			doLCD (2, 0, "Open Door: Push&Pull");
		} else if (wait_door_closed  && shmem->has_DRSN) {
			doLCD (1, 0, "Operator Canceled");
			doLCD (2, 0, "Please shut the door");
		} else {
			doLCD (1, 0, "Operator Mode ON");
			doLCD (2, 0, "%5.1f $%4.2f $%7.2f",shmem->memb_gallons,shmem->memb_ppg,shmem->memb_dollars);
		}
	break;


// Get out of Operator_State
// Only goes to Idle_State
	case ExitState_Evt:

		cancelTimeout ("Operator");
		cancelTimeout ("DoorClose");
		logMessage ("Ending Operator mode");
		fprintf (out_fp,"StrikeRly\t0\n");
		fflush (out_fp);		

		cancelTimeout ("StrikeRly");
		
		doNotice ("notice","Operator mode total: %.3fg max flow: %.1f",
			tank_gals_at_start - shmem->avail_gallons,getMaxFlowGPM());
		fflush (srv_fp);

		tank_gals_at_start = shmem->avail_gallons;

		wait_door_closed = 0;
	break;

	case KeypadValid_Evt:
		if (checkOperator (1)) {
			fprintf (out_fp,"PumpRly\t0\n");
			fprintf (out_fp,"StrikeRly\t0\n");
			memset (shmem->keypad_buffer,0,sizeof(shmem->keypad_buffer));
			doLCD (1, 1, "Operator Canceled");
			if (shmem->door_open && shmem->has_DRSN) {
				doLCD (2, 1, "Please shut the door");
				fflush (out_fp);
				doTimeout (shmem->door_close_timeout,"DoorClose");
				wait_door_closed = 1;
			} else {
				fflush (out_fp);
				new_state = doReset();
			}
		} else if (!*shmem->keypad_buffer && !wait_door_closed) {
			new_state = CalMenu_State;
			is_substate = 1;
		}
	break;

	case DoorCloseTimeout_Evt:
		if (shmem->door_open && shmem->has_DRSN) {
			doNotice ("alert","Security\tDoor left open after operator mode",
				shmem->memb_id,shmem->memb_number);
			fflush (srv_fp);
		}
		wait_door_closed = 0;
		new_state = doReset();
	break;


	case OperatorTimeout_Evt:
		doLCD (1, 1, "Operator Canceled");
		doLCD (2, 1, "Operator Timeout");
		new_state = doReset();
	break;

	case DoorClosed_Evt:
		doLCD (1, 1, "Operator Canceled");
		doLCD (2, 1, "Door closed");
		fprintf (out_fp,"PumpRly\t0\n");
		fflush (out_fp);
		cancelTimeout ("DoorClose");
		wait_door_closed = 0;
		new_state = doReset();
	break;
	
	case StrikeRlyTimeout_Evt:
		// The relay is turned off in the global handler
		if (! (shmem->door_open && shmem->has_DRSN) ) {
			doLCD (1, 1, "Operator Canceled");
			doLCD (2, 1, "Door left closed");
			new_state = doReset();
		} 
	break;

// in-state operations
	case DoorOpened_Evt:
	// turn off the strike (in global handler) in 1/2 a sec
		doTimeout (500,"StrikeRly");
		doLCD (1, 1, "Operator: Door Open");
		doLCD (2, 1, "%5.1f $%4.2f $%7.2f",shmem->memb_gallons,shmem->memb_ppg,shmem->memb_dollars);
		fflush (out_fp);
	break;

	case PumpON_Evt:
		doLCD (1, 1, "Operator: Pump ON");
	break;

	case PumpOFF_Evt:
		doLCD (1, 1, "Operator: Pump OFF");
	break;

	case Flowmeter_Evt:
		doLCD (1, 1, "Flowmtr: %10lu",(unsigned long)shmem->flowmeter_counts);
	break;

	case OutOfFuel_Evt:
		doLCD (1, 1, "Operator: No Fuel");
	break;

	case FuelLevel_Evt:
		doLCD (1, 1, "Operator: Fuel Level");
	break;
	

	default:
	break;
	}

	return (new_state);
}

int doShutdown       (const int evt, const char *val) {
int new_state = shmem->status_idx;

	switch (evt) {

	// Upon entry,
	case EnterState_Evt:
		fprintf (out_fp,"LightsRly\t0\nStrikeRly\t0\nPumpRly\t0\nAuxRly\t0\n");
		fflush (out_fp);
	

		doNotice ("alert","Power\tPower interruption");
		if ( *(shmem->cc_resp.trans_id) ) {
			doNotice ("alert","Gateway\tPower Loss: Gave up while %s trans ID:%s (auth: %s). %s"
				"Error: %s. Pre-auth: $%.2f, Amount: $%.2f. "
				"CC Name: %s (Mmbshp. #:%s). GW message: %s.",
				(shmem->cc_resp.doVoid ? "voiding" : "capturing"),
				shmem->cc_resp.trans_id,shmem->cc_resp.auth_code,
				getSalesList(),
				shmem->cc_resp.error,shmem->memb_pre_auth,shmem->memb_dollars,
				shmem->msr_name,shmem->memb_number,shmem->cc_resp.message
			);
		}
		fflush (srv_fp);
		
	// Save our state
		strcpy (shmem->boot_reason,"power");
		time(&(shmem->boot_time));
		run_cfg_write (shmem);

		doLCD (2, 1, "No power - shutdown");

	// Cancel all timeout events except watchdog
		cancelTimeouts();
	
	// Send a power signal to the other processes
		system(BBD9000_PWR_CMD);
	break;

	// The only way to get out of this state is with a VoltageAlarm_Reset_Evt
	case VoltageAlarm_Reset_Evt:
		system(BBD9000_PWR_OK_CMD);
		unlink (shmem->run_conf);
		memset (shmem->patch_status,0,sizeof (shmem->patch_status));
		memset (shmem->boot_reason,0,sizeof (shmem->boot_reason));
		strcpy (shmem->boot_reason,"restart");
		doLCD (2, 1, "Power restored");
		doNotice ("alert","Power\tPower restored");
		fflush (srv_fp);
		doTimeout (shmem->status_interval,"Status");
		doTimeout (shmem->status_interval,"LCD");
		
		// Process pending transactions
		if (*shmem->cc_resp.trans_id) {
			new_state = GatewayCapture_State;
		} else {
			new_state = doReset();
		}
	break;
	default:
	break;
	}
	
	return (new_state);
}

int doNoFuel         (const int evt, const char *val) {
int new_state = shmem->status_idx;
time_t status_t;
char is_idle=0;

	switch (evt) {
	
	case EnterState_Evt:
		doNotice ("alert","Fuel\tOut of fuel: %.2f gal. left",shmem->avail_gallons);
		fflush (srv_fp);
	// Perform any software patches
		doPatch();
	// To setup the status timeout, check the last time we checked in
	// Setup the status to comply with the maximum status interval.
		status_t = (time (NULL) - shmem->last_status)*1000;
		if (status_t > shmem->status_interval) {
			doStatusTimeout();
		} else {
			doTimeout (shmem->status_interval - status_t,"Status");
		}

		doTimeout (shmem->lcd_timeout,"LCD");
		doTimeout (shmem->netIdle_timeout,"Net Idle");
		is_idle = 0;
	break;

	// Upon exit, cancel the LCD and status timeouts
	case ExitState_Evt:
		cancelTimeout ("LCD");
		cancelTimeout ("Status");
		cancelTimeout ("Net Idle");
		is_idle = 0;
	break;

	case StatusTimeout_Evt:
		doTimeout (shmem->netIdle_timeout,"Net Idle");
		is_idle = 0;
		doStatusTimeout();
	break;

	case Patch_Evt:
		doTimeout (shmem->netIdle_timeout,"Net Idle");
		is_idle = 0;
		doPatch();
	break;
	
	case ServerTimeout_Evt:
		new_state = NoServer_State;
		doLCD (1, 0, "Server Communication");
		doLCD (2, 0, "Network Timeout");
		doTimeout (shmem->lcd_timeout,"LCD");
	break;

	case ServerError_Evt:
		new_state = NoServer_State;
		doLCD (1, 0, "Server Communication");
		doLCD (2, 0, "Error: %s",shmem->net_error);
		fflush (out_fp);
		doTimeout (shmem->lcd_timeout,"LCD");
	break;

	case NetStatus_Evt:
		if (shmem->checkin_msg) {
			doTimeout (shmem->netIdle_timeout,"Net Idle");
			is_idle = 0;
			doStatusTimeout();
		}
	break;

	case NetIdleTimeout_Evt:
		doTimeout (shmem->netIdle_timeout,"Net Idle");
		fprintf (srv_fp,"net\tidle\n");
		fflush (srv_fp);
		is_idle = 1;
	break;

	case MSR_Evt:
		parseMSR (""); // clear out the MSR info
	// The global handler turns off the display and light if there is no motion.
	// If there is still motion, then switch messages
	// The global handler re-sets the LCD timer if there was motion
	// We do the same thing if a new motion event comes in.
	case Keypad_Evt:
	case LCDRefreshTimeout_Evt:
	case MotionDetected_Evt:
		if (!is_idle) doTimeout (shmem->netIdle_timeout,"Net Idle"); // delay going idle, but keep up once there
		// Don't do this if we got an LCDTimeout, but there is no motion detected
		if ( ! (evt == LCDRefreshTimeout_Evt && !shmem->motion) ) {
		// Effectively, this means the user is still there, but there may not be motion detected
			doTimeout (shmem->input_timeout,"LCD");
			checkLights();
			doLCD (1, 0, "Offline: No fuel");
			doLCD (2, 0, "Please try later");
		}
	break;

	case FuelLevel_Evt:
		doTimeout (shmem->netIdle_timeout,"Net Idle");
		is_idle = 0;
		new_state = doReset();
	break;
	}
	
	return (new_state);
}

int doNoServer       (const int evt, const char *val) {
int new_state = shmem->status_idx;
static char reason[STR_SIZE]="";
static long retry_backoff;

	switch (evt) {
	
	// Upon entry, make sure we have an LCD timeout and a status timeout
	case EnterState_Evt:
		shmem->server = 0;
		// The server does not resend status messages, so we set our status timeout
		// and try to send status messages until the server is OK
		shmem->status_interval = shmem->status_interval_no_net;
		doTimeout (shmem->status_interval,"Status");
		retry_backoff = shmem->status_interval;
	break;

	case ExitState_Evt:
	break;

	case StatusTimeout_Evt:
		doStatusTimeout();
	break;

	case NetStatus_Evt:
		doTimeout (retry_backoff,"Status");
	break;

	case ServerTimeout_Evt:
	case ServerError_Evt:
		strcpy (reason,shmem->net_error);
		retry_backoff *= 2;
		if (retry_backoff > shmem->max_status_interval_no_net) retry_backoff = shmem->max_status_interval_no_net;
		doTimeout (retry_backoff,"Status");
	break;

	case ServerOK_Evt:
		shmem->server = 1;
		shmem->status_interval = shmem->status_interval_net;

		doLCD (1, 0, shmem->coop_name);
		doLCD (2, 0, "Network restored");
		// If we have our network back, capture any stale transaction
		if ( *(shmem->cc_resp.trans_id) ) new_state = GatewayCapture_State;
		else new_state = doReset();
	break;

	case MSR_Evt:
		parseMSR (""); // clear out the MSR info
	case Keypad_Evt:
	case LCDRefreshTimeout_Evt:
	case MotionDetected_Evt:
		// Don't do this if we got an LCDTimeout, but there is no motion detected
		if ( ! (evt == LCDRefreshTimeout_Evt && !shmem->motion) ) {
		// Effectively, this means the user is still there, but there may not be motion detected
			doTimeout (shmem->input_timeout,"LCD");
			checkLights();
			doLCD (1, 0, "Network Error:%s ",reason);
			doLCD (2, 0, "Please try later");
		}
	break;

	default:
	break;
	}

	return (new_state);
}

// This state gets entered only if we have a valid CC
int doUserLogin      (const int evt, const char *val) {
int new_state = shmem->status_idx;
	switch (evt) {

	// Upon entry, clear the keypad buffer and make sure we have an input timeout.
	case EnterState_Evt:
		doTimeout (shmem->input_timeout,"Input");
		cancelTimeout ("LCD");
		checkLights();
		doLCD (1, 0, "%s",shmem->msr_name);
		doLCD (2, 0, "Enter SPN, then #");
	break;

	case ExitState_Evt:
		cancelTimeout ("Input");
	break;



// State transitions
// Input timeout goes back to Idle_State
	case InputTimeout_Evt:
		doLCD (2, 0, "Input Timeout");
		new_state = doReset();
	break;

// KeypadValid_Evt with a non-zero shmem->msr_CCname sends us to ServerAuth_State
// Otherwise, ask to swipe card
	case KeypadValid_Evt:
		doLCD (1, 0, "%s",shmem->msr_name);
		doLCD (2, 0, "Authorizing...");
		new_state = ServerAuth_State;
	break;

// In-state operations
	case Keypad_Evt:
		if (!shmem->keypad_buffer[1]) { // first key
			doLCD (1, 0, "Enter SPN, then #");
		}
		doKeypadStars(1);
		doTimeout (shmem->input_timeout,"Input");
	break;

	default:
	break;
	}
	
	return (new_state);
}

int doServerAuth     (const int evt, const char *val) {
int new_state = shmem->status_idx;

	switch (evt) {

	// Upon entry, cancel input and LCD timeouts.
	case EnterState_Evt:
		cancelTimeout ("LCD");
		cancelTimeout ("Input");
		// We may never get a response, so set a bail-out timeout
		// The length is the network timeout plus 30 seconds
		// N.B.: network_timeout is in seconds
		doTimeout ((shmem->network_timeout*1100)+30000,"ServerResp");

		strncpy (shmem->memb_spn,shmem->keypad_buffer,sizeof(shmem->memb_spn));
		memset (shmem->keypad_buffer,0,sizeof(shmem->keypad_buffer));
	
		doLCD (2, 1, "Authorizing...");
		fprintf (srv_fp,"auth\t%s\t%s\t%s\n",shmem->msr_CCname,shmem->msr_last4,shmem->memb_spn);
		fflush (srv_fp);
	break;


	case ExitState_Evt:
		cancelTimeout ("ServerResp");
	break;

	case AuthenticationSuccess_Evt:
		if (shmem->is_violation) {
			if (shmem->is_violation > 1 && tank_gals_at_start != 0) {
				doNotice ("alert","Security\tUnauthorized dispensing stopped (Member auth.). "
					"Available: %.3fg. total: %.3fg",shmem->avail_gallons, tank_gals_at_start - shmem->avail_gallons);
			}
			shmem->is_violation = 0;
		}
		if (shmem->memb_credit != 0.0) {
			shmem->memb_dollars = -shmem->memb_credit;
		} else {
			shmem->memb_dollars = 0.0;
		}
	
		if ( !strcmp (shmem->memb_status,"ASK_RENEWAL") ) {
			new_state = AskRenew_State;
		} else if (shmem->upgrade_fee > 0.0) {
			new_state = AskUpgrade_State;
		} else {
			new_state = GatewayPreAuth_State;
		}
	break;

	// Differentiate reasons for failure
	case AuthenticationFailed_Evt:
		if (!strcmp (val,"SPN")) {
			doLCD (1, 1, "Authorization failed");
			doLCD (2, 1, "Bad SPN");
			new_state = doReset();
		} else if (shmem->full_membership_fee > 0 || shmem->trial_membership_surcharge > 0) {
		// The server is offering to sell a membership
			new_state = AskBuyMemb_State;
		} else {
			doLCD (1, 1, "Authorization failed");
			doLCD (2, 1, "Sorry!");
			new_state = doReset();
		}
	break;
	
	case MembershipExpired_Evt:
		new_state = AskRenewExpired_State;
	break;


	case ServerRespTimeout_Evt:
	case ServerTimeout_Evt:
		new_state = NoServer_State;
		doLCD (1, 1, "Server Communication");
		doLCD (2, 1, "Network Timeout");
		doTimeout (shmem->lcd_timeout,"LCD");
	break;


	case NetStatus_Evt:
		if (!strcmp (val,"Connected") || !strcmp (val,"Update")) {
			doLCD (2, 1, "Authorizing...");
		} else {
			doLCD (2, 1, val);
		}
	break;

	case ServerError_Evt:
		new_state = NoServer_State;
		doLCD (1, 1, "Server Communication");
		doLCD (2, 1, "Error: %s",shmem->net_error);
		doTimeout (shmem->lcd_timeout,"LCD");
	break;

	default:
	break;
	}
	
	return (new_state);
}

int doAskRenew       (const int evt, const char *val) {
int new_state = shmem->status_idx;


	switch (evt) {

	// Upon entry, cancel input and LCD timeouts.
	case EnterState_Evt:
		doLCD (1, 1, "Renew Membership?");
		doLCD (2, 1, "+$%7.2f *=No #=Yes",shmem->renewal_fee);
		doTimeout (shmem->input_timeout,"Input");
		cancelTimeout ("LCD");
		checkLights();
	break;


	case ExitState_Evt:
		cancelTimeout ("Input");
	break;

	case Keypad_Evt:
		if (*val == '*') {
			if (shmem->upgrade_fee > 0.0) {
				new_state = AskUpgrade_State;
			} else {
				new_state = GatewayPreAuth_State;
			}
		} else {
			doTimeout (shmem->input_timeout,"Input");
		}
	break;

	case KeypadValid_Evt:
		shmem->memb_dollars += shmem->renewal_fee;
		shmem->memb_renewal_sale = 1;
		if (shmem->upgrade_fee > 0.0) {
			new_state = AskUpgrade_State;
		} else {
			new_state = GatewayPreAuth_State;
		}
	break;

	case InputTimeout_Evt:
		new_state = GatewayPreAuth_State;
	break;

	}

	return (new_state);
}

// This state is entered after an server authorization rejection.
// "# to get membership" is displayed on the LCD as a prompt
// This goes into BuyMemb_State or back to idle.
int doAskBuyMemb     (const int evt, const char *val) {
int new_state = shmem->status_idx;

	switch (evt) {

	// Upon entry, setup input timeout
	// Write prompt on LCD2
	case EnterState_Evt:
		doLCD (1, 1, "Unrecognized Member");
		doLCD (2, 1, "#:New Memb. *:Cancel");
		doTimeout (shmem->input_timeout,"Input");
		cancelTimeout ("LCD");
		checkLights();
	break;

	case ExitState_Evt:
		cancelTimeout ("Input");
	break;

	case Keypad_Evt:
		if (*val == '*') {
			doLCD (1, 1, "Transaction Canceled");
			doLCD (2, 1, "by user");
			new_state = doReset();
		} else {
			doTimeout (shmem->input_timeout,"Input");
		}
	break;

	case KeypadValid_Evt:
		new_state = BuyMemb_State;
	break;

	case InputTimeout_Evt:
		doLCD (1, 1, "Transaction Canceled");
		doLCD (2, 1, "Input Timeout");
		new_state = doReset();
	break;

	default:
	break;
	}

	return (new_state);
}



// This state is entered from AskRenew, RenewExpired, or ServerAuth
// when a recognized membership can be upgraded (shmem->upgrade_fee > 0)
// The LCD prompts to upgrade membership (KP: 1) or pay surcharge (KP: 2)
// if 1 or 2 is pressed, this goes to the GatewayPreAuth_State
// if * is pressed, the transaction is canceled.
// If shmem->renewal_fee is > 0 and !shmem->memb_renewal_sale
//   then the shmem->renewal_fee is added to shmem->upgrade_fee
int doAskUpgrade          (const int evt, const char *val) {
int new_state = shmem->status_idx;

	switch (evt) {

	// Upon entry, setup input timeout
	// Write prompt
	case EnterState_Evt:
	// |--------+---------|
	// 1:Upgrade = +$100.00
	// 2:Trial = +$0.75/gal
		if (shmem->renewal_fee > 0 && !shmem->memb_renewal_sale) {
			doLCD (1, 1, "1:Upgrade = +$%.2f", shmem->renewal_fee+shmem->upgrade_fee);
		} else {
			doLCD (1, 1, "1:Upgrade = +$%.2f", shmem->upgrade_fee);
		}
		doLCD (2, 1, "2:Trial = +$%.2f/gal", shmem->trial_membership_surcharge);
		doTimeout (shmem->input_timeout,"Input");
		cancelTimeout ("LCD");
		checkLights();
	break;

	case ExitState_Evt:
		cancelTimeout ("Input");
	break;

	case Keypad_Evt:
		doTimeout (shmem->input_timeout,"Input");
		if (*val == '*') {
			doLCD (1, 1, "Transaction Canceled");
			doLCD (2, 1, "by user");
			new_state = doReset();
		} else if (*val == '1') {
			shmem->memb_upgrade_sale = 1;
			shmem->memb_dollars += shmem->upgrade_fee;
			if (shmem->renewal_fee > 0 && !shmem->memb_renewal_sale) {
				shmem->memb_renewal_sale = 1;
				shmem->memb_dollars += shmem->renewal_fee;
			}
			new_state = GatewayPreAuth_State;
		} else if (*val == '2') {
			shmem->memb_ppg += shmem->trial_membership_surcharge;
			new_state = GatewayPreAuth_State;
		}
	break;

	case InputTimeout_Evt:
		doLCD (1, 1, "Transaction Canceled");
		doLCD (2, 1, "Input Timeout");
		new_state = doReset();
	break;

	default:
	break;
	}
	
	return (new_state);

}

// This state is entered when a recognized membership is expired
// This goes into RenewExpired_State or back to idle.
int doAskRenewExpired     (const int evt, const char *val) {
int new_state = shmem->status_idx;

	switch (evt) {

	// Upon entry, setup input timeout
	// Write prompt on LCD2
	case EnterState_Evt:
		doLCD (1, 1, "Membership Expired!");
		doLCD (2, 1, "#:Continue  *:Cancel");
		doTimeout (shmem->input_timeout,"Input");
		cancelTimeout ("LCD");
		checkLights();
	break;

	case ExitState_Evt:
		cancelTimeout ("Input");
	break;

	case Keypad_Evt:
		doTimeout (shmem->input_timeout,"Input");
		if (*val == '*') {
			doLCD (1, 1, "Transaction Canceled");
			doLCD (2, 1, "by user");
			new_state = doReset();
		} else {
			doTimeout (shmem->input_timeout,"Input");
		}
	break;

	case KeypadValid_Evt:
		new_state = RenewExpired_State;
	break;

	case InputTimeout_Evt:
		doLCD (1, 1, "Transaction Canceled");
		doLCD (2, 1, "Input Timeout");
		new_state = doReset();
	break;

	default:
	break;
	}

	return (new_state);
}


// This is entered from AskRenewExpired_State when the '#' key is pressed
// The LCD prompts to renew membership (KP: 1) or pay surcharge (KP: 2)
// if 1 or 2 is pressed, this goes to the GatewayPreAuth_State
int doRenewExpired        (const int evt, const char *val) {
int new_state = shmem->status_idx;

	switch (evt) {

	// Upon entry, setup input timeout
	// Write prompt
	case EnterState_Evt:
	// |--------+---------|
	// 1:Renew = +$30.00
	// 2:Trial = +$0.75/gal
		doLCD (1, 1, "1:Renew = +$%.2f", shmem->renewal_fee);
		doLCD (2, 1, "2:Trial = +$%.2f/gal", shmem->trial_membership_surcharge);
		doTimeout (shmem->input_timeout,"Input");
		cancelTimeout ("LCD");
		checkLights();
	break;

	case ExitState_Evt:
		cancelTimeout ("Input");
	break;

	case Keypad_Evt:
		doTimeout (shmem->input_timeout,"Input");
		if (*val == '*') {
			doLCD (1, 1, "Transaction Canceled");
			doLCD (2, 1, "by user");
			new_state = doReset();
			break;
		} else if (*val == '1') {
			shmem->memb_dollars += shmem->renewal_fee;
			shmem->memb_renewal_sale = 1;
			if (shmem->upgrade_fee > 0.0) {
				new_state = AskUpgrade_State;
			} else {
				new_state = GatewayPreAuth_State;
			}
		} else if (*val == '2') {
			if (shmem->upgrade_fee > 0.0) {
				new_state = AskUpgrade_State;
			} else {
				shmem->memb_ppg += shmem->trial_membership_surcharge;
				new_state = GatewayPreAuth_State;
			}
		}
		
	break;

	case InputTimeout_Evt:
		doLCD (1, 1, "Transaction Canceled");
		doLCD (2, 1, "Input Timeout");
		new_state = doReset();
	break;

	default:
	break;
	}
	
	return (new_state);

}

// This is entered from AskBuyMemb_State when the '#' key is pressed
// The LCD prompts to buy a full (KP: 1) or partial (KP: 2) membership
// if 1 or 2 is pressed, this goes to the SPNConf_State to confirm the SPN
// on InputTimeout_Evt, it goes to Idle.
int doBuyMemb        (const int evt, const char *val) {
int new_state = shmem->status_idx;

	switch (evt) {

	// Upon entry, setup input timeout
	// Write prompt
	// |--------+---------|
	// 1:Full  = +$100.00
	// 2:Trial = +$0.75/gal
	case EnterState_Evt:
		doLCD (1, 1, "1:Full  = $%.2f", shmem->full_membership_fee);
		doLCD (2, 1, "2:Trial = +$%.2f/gal", shmem->trial_membership_surcharge);
		doTimeout (shmem->input_timeout,"Input");
		cancelTimeout ("LCD");
		checkLights();
	break;

	case ExitState_Evt:
		cancelTimeout ("Input");
	break;

	case Keypad_Evt:
		doTimeout (shmem->input_timeout,"Input");
		if (*val == '*') {
			doLCD (1, 1, "Transaction Canceled");
			doLCD (2, 1, "by user");
			new_state = doReset();
		} else if (*val == '1') {
			shmem->memb_full_membership_sale = 1;
			new_state = SPNConf_State;
		} else if (*val == '2') {
			shmem->memb_temp_membership_sale = 1;
			new_state = SPNConf_State;
		}
	break;

	case InputTimeout_Evt:
		doLCD (1, 1, "Transaction Canceled");
		doLCD (2, 1, "Input Timeout");
		new_state = doReset();
	break;

	default:
	break;
	}
	
	return (new_state);

}

// This is entered from BuyMemb_State when a full or temporary membership is bought
// The LCD prompts to re-enter the SPN
// If the SPNs match, the state switches to GatewayPreAuth_State
// On input timeout or mismatch, it goes back to Idle.
int doSPNConf        (const int evt, const char *val) {
int new_state = shmem->status_idx;

	switch (evt) {

	// Upon entry, setup input timeout
	// Clear keypad, Write prompt
	case EnterState_Evt:
		doLCD (1, 1, "Re-key SPN to Accept");
		doLCD (2, 1, "Membership Agreement");
		memset (shmem->keypad_buffer,0,sizeof(shmem->keypad_buffer));
		doTimeout (shmem->input_timeout,"Input");
		cancelTimeout ("LCD");
		checkLights();
	break;

	case ExitState_Evt:
		cancelTimeout ("Input");
	break;

	case InputTimeout_Evt:
		doLCD (1, 1, "Transaction Canceled");
		doLCD (2, 1, "Input Timeout");
		new_state = doReset();
	break;

	case Keypad_Evt:
		doTimeout (shmem->input_timeout,"Input");
		doKeypadStars(1);
	break;


// Check keypad buffer against shmem->memb_spn
// 
	case KeypadValid_Evt:
		if (!strcmp (shmem->keypad_buffer,shmem->memb_spn)) {
			new_state = SetPreAuth_State;
		} else {
			cancelTimeout ("Input");
			doLCD (1, 1, "Transaction Canceled");
			doLCD (2, 1, "SPN mismatch");
			new_state = doReset();
		}
	break;

	default:
	break;
	}
	
	return (new_state);
}


int doGatewayPreAuth (const int evt, const char *val) {
int new_state = shmem->status_idx;
float pre_auth;
static int retries=0;
#define MAX_GW_BUSY_RETRIES 3

	switch (evt) {
	
	case EnterState_Evt:
		cancelTimeout ("Input");
		cancelTimeout ("LCD");
		// We may never get a response, so set a bail-out timeout
		// The length is the network timeout plus 10%.
		// N.B.: network_timeout is in seconds
		doTimeout ((shmem->network_timeout*1100)+30000,"ServerResp");

		// Reset busy retries
		retries=0;
	
		pre_auth = (float) shmem->memb_fuel_pre_auth;
		shmem->memb_dollars = (-shmem->memb_credit) +
			(shmem->memb_renewal_sale ? shmem->renewal_fee : 0) +
			(shmem->memb_full_membership_sale ? shmem->full_membership_fee : 0) +
			(shmem->memb_temp_membership_sale ? shmem->temp_membership_fee : 0) +
			(shmem->memb_upgrade_sale ? shmem->upgrade_fee : 0)
		;

		pre_auth += shmem->memb_dollars;

		// We will use this amount to compare our total against when dispensing.
		shmem->memb_pre_auth = pre_auth;

		// If this will be covered by credit, don't register a CC transaction
		if (pre_auth < shmem->cc_threshold) {
			if (shmem->memb_full_membership_sale || shmem->memb_temp_membership_sale) {
				new_state = RegNewMemb_State;
			} else if (shmem->memb_upgrade_sale || shmem->memb_renewal_sale) {
				if (shmem->memb_upgrade_sale)
					doNotice ("sale","%d\t%s\t%.0f\t%.3f\t%.2f",shmem->memb_id,
						"upgrade",1.0,shmem->upgrade_fee,shmem->upgrade_fee);
				if (shmem->memb_renewal_sale)
					doNotice ("sale","%d\t%s\t%.0f\t%.3f\t%.2f",shmem->memb_id,
						"renewal",1.0,shmem->renewal_fee,shmem->renewal_fee);
				fflush (srv_fp);
				// note that we're not updating the credit from these sales, so we keep them around
				// The credit in the DB will be more negative than the credit we have at the kiosk.
				// When buying a new membership, the sale is registered in the DB, resulting in a negative credit.
				// When we register a new membership, its an authorization, so the DB responds with the negative
				// credit for the membership sale.
				// So, the membership sale flags are set to 0 in doRegNewMembership, because the price is now reflected in the credit.
				if (shmem->memb_ppg > 0.0 && shmem->memb_fuel_pre_auth > 0) {
					new_state = Dispensing_State;
				} else {
					new_state = GatewayCapture_State;
				}
			} else {
				if (shmem->memb_ppg > 0.0 && shmem->memb_fuel_pre_auth > 0) {
					new_state = Dispensing_State;
				} else {
					new_state = GatewayCapture_State;
				}
			}
		} else {
			// If we're making a new membership, we will register it with the server if the pre auth succeeds
			// At this point we're ready to do a pre-auth.
			// Since we're going to do a couple of re-tries if the gateway is busy,
			// We're going to set up a short-duration retry timeout and do it with the retry code
			doTimeout (20,"GW Retry");
		}
	break;

	case ExitState_Evt:
		cancelTimeout ("ServerResp");
		cancelTimeout ("GW Retry");
		// We don't need these after pre-auth.
		memset (shmem->msr_track1,0,sizeof (shmem->msr_track1));
		memset (shmem->msr_track2,0,sizeof (shmem->msr_track2));
	break;

	case GW_RetryTimeout_Evt:
		doLCD (1, 1, "Pre-authorizing CC");
		doLCD (2, 1, "For $%.2f...",shmem->memb_pre_auth);

		doGW ("PreAuth",shmem->memb_pre_auth);

		// We may never get a response, so set a bail-out timeout
		// The length is the network timeout plus 10%.
		// N.B.: network_timeout is in seconds
		doTimeout ((shmem->network_timeout*1100)+30000,"ServerResp");
	break;

// On pre-auth success, go to member registration state otherwise to dispensing state if member can buy fuel
// or just finish out the CC transaction.
	case GW_Accepted_Evt:
		cancelTimeout ("GW Retry");
		// Register the pre-auth transaction with the server
		// Check server_GW
		doSrvCC ("PREAUTH", shmem->memb_dollars, 1);

		if (shmem->memb_full_membership_sale || shmem->memb_temp_membership_sale) {
			new_state = RegNewMemb_State;
		} else {
			if (shmem->memb_upgrade_sale)
				doNotice ("sale","%d\t%s\t%.0f\t%.3f\t%.2f",shmem->memb_id,
					"upgrade",1.0,shmem->upgrade_fee,shmem->upgrade_fee);
			if (shmem->memb_renewal_sale)
				doNotice ("sale","%d\t%s\t%.0f\t%.3f\t%.2f",shmem->memb_id,
					"renewal",1.0,shmem->renewal_fee,shmem->renewal_fee);
			fflush (srv_fp);
			if (shmem->memb_ppg > 0.0 && shmem->memb_fuel_pre_auth > 0) {
				new_state = Dispensing_State;
			} else {
				new_state = GatewayCapture_State;
			}
		}
	break;

	case GW_Busy_Evt:
		retries++;
		doLCD (1, 1, "CC Gateway busy");
		if (retries < MAX_GW_BUSY_RETRIES) {
			doTimeout (retries*1000,"GW Retry");
			doLCD (2, 1, "Retrying in %d sec.",retries);
		} else {
			doLCD (2, 1, "Transaction canceled");
			new_state = doReset();
		}
	break;

	case GW_CCTypeRejected_Evt:
		doLCD (1, 1, "CC Type Rejected");
		new_state = MSRReswipe_State;
	break;

	case GW_CardExpired_Evt:
		doLCD (1, 1, "CC Expired");
		new_state = MSRReswipe_State;
	break;

	case GW_Declined_Evt:
		if (shmem->memb_fuel_pre_auth > 0) {
			new_state=AskDeclined_State;
		} else {
			doLCD (1, 1, "CC Declined");
			doLCD (2, 1, "Transaction Canceled");
			new_state = doReset();
		}
	break;


	case NetStatus_Evt:
		if (!strcmp (val,"Connected") || !strcmp (val,"Update")) {
			doLCD (1, 1, "Pre-authorizing CC");
			doLCD (2, 1, "For $%.2f...",shmem->memb_pre_auth);
		} else {
			doLCD (2, 1, val);
		}
	break;

	// Deal with gateway errors.
	// Since we have no pending transaction, we're just going to reset
	case GW_TransLost_Evt:
	case GW_Error_Evt:
		doLCD (1, 1, "GW Communication");
		doLCD (2, 1, "Error:%s", shmem->gw_error);
		new_state = doReset();
	break;

	case ServerRespTimeout_Evt:
	case GW_Timeout_Evt:
		doLCD (1, 1, "GW Communication");
		doLCD (2, 1, "Network Timeout");
		new_state = doReset();
	break;

	case GW_CURLErr_Evt:
		doLCD (1, 1, "GW Communication");
		doLCD (2, 1, "Error:%s", shmem->gw_error);
		new_state = doReset();
	break;

	// Since we're communicating with our server also, process these as well
	case ServerTimeout_Evt:
		doLCD (1, 1, "Server Communication");
		doLCD (2, 1, "Network Timeout");
		new_state = doReset();
	break;

	case ServerError_Evt:
		doLCD (1, 1, "Server Communication");
		doLCD (2, 1, "Error:%s", shmem->net_error);
		new_state = doReset();
	break;

	default:
	break;
	}
	
	return (new_state);
}

// This state is entered when a pre-auth is declined
// It asks the user if they want to try a different card, or
// lower their fuel_preauth
// Resets on input timeout, and '*'.
// This goes into RenewExpired_State or back to idle.
int doAskDeclined     (const int evt, const char *val) {
int new_state = shmem->status_idx;

	switch (evt) {

	// Upon entry, setup input timeout
	// Write prompt on LCD2
	case EnterState_Evt:
		doLCD (1, 1, "CC Declined");
		doLCD (2, 1, "1:New CC   2:Less $$");
		doTimeout (shmem->input_timeout,"Input");
		cancelTimeout ("LCD");
		checkLights();
	break;

	case ExitState_Evt:
		cancelTimeout ("Input");
	break;

	
	case Keypad_Evt:
		if (*val == '1') {
			new_state = MSRReswipe_State;
		} else if (*val == '2') {
			new_state = SetPreAuth_State;
		} else if (*val == '*') {
			doLCD (1, 1, "Transaction Canceled");
			doLCD (2, 1, "by user");
			new_state = doReset();
		} else {
			doTimeout (shmem->input_timeout,"Input");
		}
	break;

	case InputTimeout_Evt:
		doLCD (1, 1, "Transaction Canceled");
		doLCD (2, 1, "Input Timeout");
		new_state = doReset();
	break;

	default:
	break;
	}

	return (new_state);
}

// This state is entered from GatewayPreAuth_State
int doMSRReswipe     (const int evt, const char *val) {
int new_state = shmem->status_idx;
int MSR_status;

	switch (evt) {

	// Upon entry, setup an input timeout
	// This state is setup to exit to GatewayPreAuth_State
	// Or, reset on input timeout
	case EnterState_Evt:
		doLCD (2, 1, "Try a different CC");
		doTimeout (shmem->input_timeout,"Input");
		cancelTimeout ("LCD");
		checkLights();
	break;

	case ExitState_Evt:
		cancelTimeout ("Input");
	break;

	case InputTimeout_Evt:
		doLCD (1, 1, "Transaction Canceled");
		doLCD (2, 1, "Input Timeout");
		new_state = doReset();
	break;

	case Keypad_Evt:
		doTimeout (shmem->input_timeout,"Input");
		if (*val == '*') {
			doLCD (1, 1, "Transaction Canceled");
			doLCD (2, 1, "by user");
			new_state = doReset();
		}
	break;

	case MSR_Evt:
		MSR_status = parseMSR (val);
		if (MSR_status == MSR_VALID) {
			doLCD (1, 1, shmem->msr_name);
			new_state = GatewayPreAuth_State;
		} else if (MSR_status == MSR_INVALID) {
			doLCD (1, 1, "Credit Card Invalid:");
			doLCD (2, 1, "Try a different CC");
		} else if (MSR_status == MSR_EXPIRED) {
			doLCD (1, 1, "Credit Card Expired:");
			doLCD (2, 1, "Try a different CC");
		}
	break;

	default:
	break;
	}
	
	return (new_state);
}

// This state lets a member enter a different fuel pre-authorization amount
// This is done for all new members prior to the pre-auth
// Or, if the pre-auth fails for existing members.
// Note than in either case, there is no valid pre-auth transaction in place,
// So we call doReset instead of entering the GatewayCapture_State with a void
int doSetPreAuth     (const int evt, const char *val) {
int new_state = shmem->status_idx;
int preauth;

	switch (evt) {

	// Upon entry, setup the display
	// Note that we are only adjusting the fuel portion of the pre-auth
	// which may also include a membership fee.
	// Setup an input timeout.
	case EnterState_Evt:
		memset (shmem->keypad_buffer,0,sizeof(shmem->keypad_buffer));
		doLCD (1, 1, "Max. Fuel Purchase:");
		doLCD (2, 1, "$%3d (*=clr #=enter)", shmem->memb_fuel_pre_auth);
		doTimeout (shmem->input_timeout,"Input");
		cancelTimeout ("LCD");
		checkLights();
	break;

	case ExitState_Evt:
		cancelTimeout ("Input");
	break;

	case Keypad_Evt:
		doTimeout (shmem->input_timeout,"Input");
		preauth = atoi (shmem->keypad_buffer);
		sprintf (shmem->keypad_buffer,"%d",preauth);
		doLCD (2, 1, "$%3d (*=clr #=enter)", preauth);
	break;

	case KeypadValid_Evt:
		cancelTimeout ("Input");
		shmem->memb_fuel_pre_auth = atoi (shmem->keypad_buffer);
		if (shmem->memb_fuel_pre_auth > 0) {
			new_state = GatewayPreAuth_State;
		} else { // Even if there's a membership or renewal purchase request, cancel everything
			doLCD (1, 1, "Transaction Canceled");
			if (shmem->memb_full_membership_sale || shmem->memb_temp_membership_sale) {
				doLCD (2, 1, "Membership canceled");
			} else if (shmem->memb_renewal_sale) {
				doLCD (2, 1, "Renewal canceled");
			} else {
				doLCD (2, 1, "");
			}
			fflush (out_fp);
			new_state = doReset();
		}
	break;

	case InputTimeout_Evt:
		doLCD (1, 1, "Transaction Canceled");
		doLCD (2, 1, "Input Timeout");
		new_state = doReset();
	break;

	default:
	break;
	}
	
	return (new_state);
}



// State for registering a new membership with the server
// Note that we have a transaction pre-authorized when entering this state,
// So we have to deal with it (void or capture).  Can't just drop it.
int doRegNewMemb     (const int evt, const char *val) {
int new_state = shmem->status_idx;


	switch (evt) {

	// Upon entry, call the server to register the new member
	// new_memb type CC_mag_name last4 SPN pre_auth CC_trans_id CC_auth
	case EnterState_Evt:
		doLCD (1, 1, shmem->msr_name);
		doLCD (2, 1, "Registering Memb...");
		if (shmem->memb_full_membership_sale) {
			fprintf (srv_fp,"new_memb\tFULL\t%s\t%s\t%s\t%d\t%s\t%s\t%.2f\n",
				shmem->msr_CCname,shmem->msr_last4,shmem->memb_spn,shmem->memb_fuel_pre_auth,
				shmem->cc_resp.trans_id,shmem->cc_resp.auth_code,shmem->full_membership_fee);
		} else {
			fprintf (srv_fp,"new_memb\tONE-DAY\t%s\t%s\t%s\t%d\t%s\t%s\t%.2f\n",
				shmem->msr_CCname,shmem->msr_last4,shmem->memb_spn,shmem->memb_fuel_pre_auth,
				shmem->cc_resp.trans_id,shmem->cc_resp.auth_code,shmem->temp_membership_fee);
		}
		fflush (srv_fp);
		// We may never get a response, so set a bail-out timeout
		// The length is the network timeout plus 10%.
		// N.B.: network_timeout is in seconds
		doTimeout ((shmem->network_timeout*1100)+30000,"ServerResp");
	break;

	case ExitState_Evt:
		cancelTimeout ("ServerResp");
	break;

	case AuthenticationSuccess_Evt:
		if (shmem->is_violation) {
			if (shmem->is_violation > 1 && tank_gals_at_start != 0) {
				doNotice ("alert","Security\tUnauthorized dispensing stopped (New Member). "
					"Available: %.3fg. total: %.3fg",shmem->avail_gallons, tank_gals_at_start - shmem->avail_gallons);
			}
			shmem->is_violation = 0;
		}
		if (shmem->memb_credit != 0.0) {
			shmem->memb_dollars = -shmem->memb_credit;
		} else {
			shmem->memb_dollars = 0.0;
		}
		// the membership sale was registered, so now it shows up as a negative credit
		// Don't want both the sale and the credit, so blank out the sale
		shmem->memb_full_membership_sale = 0;
		shmem->memb_temp_membership_sale = 0;

		if (shmem->memb_ppg > 0.0 && shmem->memb_fuel_pre_auth > 0) {
			new_state = Dispensing_State;
		} else {
			new_state = GatewayCapture_State;
		}
	break;

	// Differentiate reasons for failure
	// Note that we have a transaction pre-authorized!
	// doGatewayCapture probes for a transaction in process, and voids it.
	// But we have to tell it that we want the transaction voided.
	case AuthenticationFailed_Evt:
		shmem->cc_resp.doVoid = 1;
		if (!strcmp (val,"DuplicateCC")) {
			doLCD (1, 1, "Registration failed:");
			doLCD (2, 1, "Duplicate CC");
			new_state = GatewayCapture_State;
		} else if (!strcmp (val,"BadCCAuth")) {
			doLCD (1, 1, "Registration failed:");
			doLCD (2, 1, "Bad CC Auth. Code");
			new_state = GatewayCapture_State;
		} else {
			doLCD (1, 1, "Registration failed:");
			doLCD (2, 1, "Sorry!");
			new_state = GatewayCapture_State;
		}
	break;

	case ServerRespTimeout_Evt:
	case ServerTimeout_Evt:
		shmem->cc_resp.doVoid = 1;
		doLCD (1, 1, "Server Timeout");
		doLCD (2, 1, "Transaction Canceled");
		new_state = GatewayCapture_State;
	break;


	case NetStatus_Evt:
		if (!strcmp (val,"Connected") || !strcmp (val,"Update")) {
			doLCD (2, 1, "Registering Memb...");
		} else {
			doLCD (2, 1, val);
		}
	break;

	case ServerError_Evt:
		shmem->cc_resp.doVoid = 1;
		doLCD (1, 1, "Error:%s",shmem->net_error);
		doLCD (2, 1, "Transaction Canceled");
		new_state = GatewayCapture_State;
	break;

	default:
	break;
	}

	return (new_state);
}


int doDispensing     (const int evt, const char *val) {
int new_state = shmem->status_idx;


	switch (evt) {

	// Upon entry, clear any violation, reset fuel totals, set the strike relay, cacled LCD and Input timeouts
	// Upon entry, reset fuel totals, set the strike relay, set Strike and Dispensing timeouts, cancel LCD and Input timeouts
	case EnterState_Evt:	
		tank_gals_at_start = shmem->avail_gallons;
		fprintf (out_fp,"Flow\t0\n");
		shmem->memb_gallons = 0.0;
		shmem->flowmeter_counts = 0;
		doTimeout (shmem->dispensing_timeout,"Dispensing");
		cancelTimeout ("Input");
		cancelTimeout ("LCD");
		doLCD (1, 1, shmem->msr_name);
		if (!shmem->door_open && shmem->has_DRSN) {
			doTimeout (shmem->strike_rly_timeout,"StrikeRly");
			fprintf (out_fp,"StrikeRly\t1\n");
			fflush (out_fp);
			doLCD (2, 1, "Open Door: Push&Pull");
		} else if (shmem->door_open || !shmem->has_DRSN) {
			cancelTimeout ("StrikeRly");
			fprintf (out_fp,"StrikeRly\t0\n");
			fprintf (out_fp,"PumpRly\t1\n");
			fflush (out_fp);
			doLCD (2, 1, "%5.1f $%4.2f $%7.2f",shmem->memb_gallons,shmem->memb_ppg,shmem->memb_dollars);
		}
	break;

	case ExitState_Evt:
		cancelTimeout ("Dispensing");
		cancelTimeout ("StrikeRly");
		fprintf (out_fp,"PumpRly\t0\n");
		fprintf (out_fp,"StrikeRly\t0\n");
		fflush (out_fp);
		doLCD (2, 1, "Done - Thank You!");
//logMessage ("Exiting Dispensing with mem_gallons=%.3f, trans_id: [%s]",shmem->memb_gallons, shmem->cc_resp.trans_id);

		if (shmem->memb_gallons > 0.005) {
			doNotice ("sale","%d\t%s\t%.3f\t%.3f\t%.2f\t%.1f",
				shmem->memb_id,shmem->fuel_type,shmem->memb_gallons,shmem->memb_ppg,(shmem->memb_ppg*shmem->memb_gallons),getMaxFlowGPM());
		}

		// This is our version of the total amount owed (or credit remaining if negative)
		shmem->memb_dollars = (-shmem->memb_credit) +
			(shmem->memb_gallons * shmem->memb_ppg) +
			(shmem->memb_renewal_sale ? shmem->renewal_fee : 0) +
			(shmem->memb_full_membership_sale ? shmem->full_membership_fee : 0) +
			(shmem->memb_temp_membership_sale ? shmem->temp_membership_fee : 0) +
			(shmem->memb_upgrade_sale ? shmem->upgrade_fee : 0)
		;

		// --------------------
		// Low on Fuel: 75 gal
		if (shmem->avail_gallons < shmem->no_fuel_cutoff) {
		// If fuel ran out send an alarm
			doNotice ("alert","Fuel\tOUT of fuel: %.2f gallons left",shmem->avail_gallons);
			doLCD (1, 1, "Out of Fuel!");
		// If fuel is running out send an alarm
		} else if (shmem->avail_gallons < shmem->low_fuel_alarm) {
			doNotice ("alert","Fuel\tLow fuel: %.2f gallons left",shmem->avail_gallons);
			doLCD (1, 1, "Low on Fuel: %.0f gal",shmem->avail_gallons);
		} else {
			float other_chrgs = 
				(shmem->memb_renewal_sale ? shmem->renewal_fee : 0) +
				(shmem->memb_full_membership_sale ? shmem->full_membership_fee : 0) +
				(shmem->memb_temp_membership_sale ? shmem->temp_membership_fee : 0) +
				(shmem->memb_upgrade_sale ? shmem->upgrade_fee : 0)
			;
			if (other_chrgs >= 0.0) {
				doLCD (1, 1, "$%.2f Fuel +$%.2f Fees",
					(shmem->memb_gallons * shmem->memb_ppg),other_chrgs);
			} else {
				doLCD (1, 1, "$%.2f Fuel",shmem->memb_gallons * shmem->memb_ppg);
			}
		}
		if ( shmem->memb_dollars > shmem->memb_credit ) {
				doLCD (2, 1, "$%.2f Credit Remaining",shmem->memb_dollars - shmem->memb_credit);
		}
		fprintf (out_fp,"PMP\nVIN\n");
		fflush (out_fp);
		fflush (srv_fp);

	break;

	case DoorOpened_Evt:
		doTimeout (500,"StrikeRly");
		doTimeout (shmem->dispensing_timeout,"Dispensing");
		fprintf (out_fp,"PumpRly\t1\n");
		doLCD (1, 1, shmem->msr_name);
		doLCD (2, 1, "%5.1f $%4.2f $%7.2f",shmem->memb_gallons,shmem->memb_ppg,shmem->memb_dollars);
	break;

	case Flowmeter_Evt:
		// Refresh the dispensing timeout
		doTimeout (shmem->dispensing_timeout,"Dispensing");
	break;

	case OutOfFuel_Evt:
		doLCD (1, 1, "Out of Fuel!");
		new_state = GatewayCapture_State;
	break;
	
	case DispensingTimeout_Evt:
		doLCD (1, 1, "Dispensing Timeout");
		new_state = GatewayCapture_State;
	break;

	case PumpOFF_Evt:
		new_state = GatewayCapture_State;
	break;

	case DoorClosed_Evt:
		new_state = GatewayCapture_State;
	break;
	
	default:
	break;
	}

	return (new_state);
}

// This state is used to finalize an existing pre-authorized transaction
// Either by capturing it or by voiding it.
// Voiding is signaled by setting the shmem->cc_resp.doVoid flag.
// This is state can only exit to Idle_State, and only if the transaction is finalized
// This is the only state that can clear the transaction record
// If the transaction record is clear upon entry (no pre-auth transaction), then it goes to reset
int doGatewayCapture (const int evt, const char *val) {
int new_state = shmem->status_idx;
static int retries=0;
static char lcd_timedout=0;
static float memb_dollars;

#define MAX_GW_CAP_RETRIES 5
#define RETRY_MSEC_MULTIPLIER 5000 // Every retry adds this many seconds before the next.

	switch (evt) {

	// Upon entry, check if there is a transaction, and if so,
	// reset retries, initiate the Gateway message
	case EnterState_Evt:
		retries=0;
		lcd_timedout=0;
		memb_dollars = 0;
//logMessage ("In GW capt., trans_id: [%s]",shmem->cc_resp.trans_id);
		if (! *(shmem->cc_resp.trans_id) ) {
			// Zero-out the transaction stuff, and do a full reset.
			memset (&(shmem->cc_resp),0,sizeof (cc_resp_s));
			new_state = doReset();
		} else {
			shmem->memb_dollars = (-shmem->memb_credit) +
				(shmem->memb_gallons * shmem->memb_ppg) +
				(shmem->memb_renewal_sale ? shmem->renewal_fee : 0) +
				(shmem->memb_full_membership_sale ? shmem->full_membership_fee : 0) +
				(shmem->memb_temp_membership_sale ? shmem->temp_membership_fee : 0) +
				(shmem->memb_upgrade_sale ? shmem->upgrade_fee : 0)
			;
//logMessage ("In GW Capt. mem_gallons=%.3f",shmem->memb_gallons);
//logMessage ("In GW Capt, memb_dollars=%.3f",shmem->memb_dollars);

			// The transaction amount cannot be greater than the pre-auth
			memb_dollars = shmem->memb_dollars <= shmem->memb_pre_auth ? shmem->memb_dollars : shmem->memb_pre_auth;
//logMessage ("In GW Capt, memb_dollars2=%.3f",shmem->memb_dollars);

			// If the transaction amount is less than shmem->cc_threshold, then we void the trasnaction
			if (memb_dollars < shmem->cc_threshold) shmem->cc_resp.doVoid = 1;

			// We're going to set an LCD timeout since we might be here a while
			doTimeout (shmem->lcd_timeout,"LCD");
			// We haven't actually sent anything to the gateway yet, so use the GW Retry timeout to do so.
			doTimeout (20,"GW Retry");
		}
		// We may never get a response, so set a bail-out timeout
		// The length is the network timeout plus 10%.
		// N.B.: network_timeout is in seconds
		doTimeout ((shmem->network_timeout*1100)+30000,"ServerResp");
	break;

	case ExitState_Evt:
		retries=0;
		lcd_timedout=0;
		memb_dollars = 0;
		cancelTimeout ("ServerResp");
	break;

	// After the first LCD timeout, hide the details of what we're doing
	// Note that the global handler has already cleared the display.
	case LCDTimeout_Evt:
		lcd_timedout =1;
	break;

	case Keypad_Evt:
	case LCDRefreshTimeout_Evt:
	case MotionDetected_Evt:
		// Don't do this if we got an LCDTimeout, but there is no motion detected
		if ( ! (evt == LCDRefreshTimeout_Evt && !shmem->motion) ) {
		// Effectively, this means the user is still there, but there may not be motion detected
		// If an LCD timeout already happened, we obscure the details of what we're doing
			doTimeout (shmem->input_timeout,"LCD");
			checkLights();
			if (lcd_timedout) {
				doLCD (1, 0, "Processing.");
				doLCD (2, 0, "Please wait");
			}
		}
	break;

	case GW_Accepted_Evt:
		cancelTimeout ("GW Retry");

		// Tell the user what happened
		// --------------------
		// Voided $999.99
		// Processed $999.99
		if (shmem->cc_resp.doVoid) {
			if (!lcd_timedout) {
				doLCD (2, 1, "Voided $%.2f",shmem->memb_pre_auth);
			}
			// Register CC transaction with server
			// Check server_GW
			doSrvCC ("VOIDED", memb_dollars, 1);
		} else {
			if (!lcd_timedout) {
				doLCD (2, 1, "Processed $%.2f",memb_dollars);
			}
			// Register CC transaction with server
			// Only successfully captured transactions are set to the captured amount
			// The rest are set to the pre-auth amount.
			// Check server_GW
			doSrvCC ("CAPTURED", memb_dollars, 1);
		}

		// Zero-out the transaction stuff, and do a full reset.
		memset (&(shmem->cc_resp),0,sizeof (cc_resp_s));
		new_state = doReset();
	break;

	case ServerRespTimeout_Evt:
	case GW_RetryTimeout_Evt:
		// We may never get a response, so set a bail-out timeout
		// The length is the network timeout plus 10%.
		// N.B.: network_timeout is in seconds
		doTimeout ((shmem->network_timeout*1100)+30000,"ServerResp");
		// Somebody canceled this for us.
		if (! *(shmem->cc_resp.trans_id) ) {
			// Zero-out the transaction stuff, and do a full reset.
			memset (&(shmem->cc_resp),0,sizeof (cc_resp_s));
			new_state = doReset();
			break;
		}
		// If we've reached our maximum retries, we give up.
		// 15 retries adding 5 seconds to each one takes 10 minutes.
		// We void the transaction at the kiosk and ship it up to the server to deal with
		// We also issue an alert that we're giving up.
		if (retries > MAX_GW_CAP_RETRIES) {
			// Register CC transaction with server
			// Check server_GW
			doSrvCC ("ERROR", memb_dollars, 0);
			doNotice ("alert","Gateway\tGave up while %s trans. ID:%s (auth: %s) after %d retries. %s"
				"Error: %s. Pre-auth: $%.2f, Amount: $%.2f. "
				"CC Name: %s (Mmbshp. #:%s). GW message: %s.",
				(shmem->cc_resp.doVoid ? "voiding" : "capturing"),
				shmem->cc_resp.trans_id,shmem->cc_resp.auth_code,retries,
				getSalesList(),
				shmem->cc_resp.error,shmem->memb_pre_auth,memb_dollars,
				shmem->msr_name,shmem->memb_number,shmem->cc_resp.message
			);
			fflush (srv_fp);
			memset (&(shmem->cc_resp),0,sizeof (cc_resp_s));
			new_state = doReset();
			break;
		}
		
		// --------------------
		// Voiding $999.99
		// Processing $999.99
		if (shmem->cc_resp.doVoid) {
			if (!lcd_timedout) {
				doLCD (2, 1, "Voiding $%.2f",shmem->memb_pre_auth);
			}
			doGW ("Void",shmem->memb_pre_auth);
		} else {
			if (!lcd_timedout) {
				doLCD (2, 1, "Processing $%.2f",memb_dollars);
			}
			doGW ("Capture",memb_dollars);
		}
	break;

	// --------------------
	// GW Timeout: retrying
	// GW Error: retrying
	// SW Error: retrying
	case GW_Timeout_Evt:
		strcpy (shmem->cc_resp.error,"Gateway Timeout");
		retries++;
		doTimeout (retries*RETRY_MSEC_MULTIPLIER,"GW Retry");
		if (!lcd_timedout) {
			doLCD (2, 1, "GW Timeout: retrying");
		}
	break;

	case GW_Error_Evt:
		strcpy (shmem->cc_resp.error,"Generic Error");
		retries++;
		doTimeout (retries*RETRY_MSEC_MULTIPLIER,"GW Retry");
		if (!lcd_timedout) {
			doLCD (2, 1, "GW Error: retrying");
		}
	break;

	case GW_CURLErr_Evt:
		strcpy (shmem->cc_resp.error,"CURL Error");
		retries++;
		doTimeout (retries*RETRY_MSEC_MULTIPLIER,"GW Retry");
		if (!lcd_timedout) {
			doLCD (2, 1, "SW Error: retrying");
		}
	break;

	case GW_Busy_Evt:
		strcpy (shmem->cc_resp.error,"Gateway Busy");
		retries++;
		doTimeout (retries*RETRY_MSEC_MULTIPLIER,"GW Retry");
		if (!lcd_timedout) {
			doLCD (2, 1, "GW Busy: retrying");
		}
	break;

	// These should never occur on a pre-auth transaction
	case GW_CCTypeRejected_Evt:
	// Register a problem with the server
	// Check server_GW
		doSrvCC ("ERROR", memb_dollars, 0);
		doNotice ("alert","Gateway\tRejected CC type while %s trans. ID:%s (auth: %s). %s"
			"Pre-auth: $%.2f, Amount: $%.2f.  "
			"CC Name: %s (Mmbshp. #:%s). GW message: %s",
			(shmem->cc_resp.doVoid ? "voiding" : "capturing"),
			shmem->cc_resp.trans_id,shmem->cc_resp.auth_code,
			shmem->memb_pre_auth,memb_dollars,
			shmem->msr_name,shmem->memb_number,shmem->cc_resp.message
		);
		fflush (srv_fp);
		memset (&(shmem->cc_resp),0,sizeof (cc_resp_s));
		new_state = doReset();
	break;

	case GW_Declined_Evt:
	// Register a problem with the server
	// Check server_GW
		doSrvCC ("ERROR", memb_dollars, 0);
		doNotice ("alert","Gateway\tDeclined CC while %s trans. ID:%s (auth: %s). %s"
			"Pre-auth: $%.2f, Amount: $%.2f.  "
			"CC Name: %s (Mmbshp. #:%s). GW message: %s",
			(shmem->cc_resp.doVoid ? "voiding" : "capturing"),
			shmem->cc_resp.trans_id,shmem->cc_resp.auth_code,
			getSalesList(),
			shmem->memb_pre_auth,memb_dollars,
			shmem->msr_name,shmem->memb_number,shmem->cc_resp.message
		);
		fflush (srv_fp);
		memset (&(shmem->cc_resp),0,sizeof (cc_resp_s));
		new_state = doReset();
	break;

	case GW_CardExpired_Evt:
	// Register a problem with the server
	// Check server_GW
		doSrvCC ("ERROR", memb_dollars, 0);
		doNotice ("alert","Gateway\tCC expired while %s trans. ID:%s (auth: %s). %s"
			"Pre-auth: $%.2f, Amount: $%.2f.  "
			"CC Name: %s (Mmbshp. #:%s). GW message: %s",
			(shmem->cc_resp.doVoid ? "voiding" : "capturing"),
			shmem->cc_resp.trans_id,shmem->cc_resp.auth_code,
			getSalesList(),
			shmem->memb_pre_auth,memb_dollars,
			shmem->msr_name,shmem->memb_number,shmem->cc_resp.message
		);
		fflush (srv_fp);
		memset (&(shmem->cc_resp),0,sizeof (cc_resp_s));
		new_state = doReset();
	break;

	case GW_TransLost_Evt:
	// Register a problem with the server
	// Check server_GW
		doSrvCC ("ERROR", memb_dollars, 0);
		doNotice ("alert","Gateway\tLost transaction while %s trans. ID:%s (auth: %s). %s"
			"Pre-auth: $%.2f, Amount: $%.2f.  "
			"CC Name: %s (Mmbshp. #:%s). GW message: %s",
			(shmem->cc_resp.doVoid ? "voiding" : "capturing"),
			shmem->cc_resp.trans_id,shmem->cc_resp.auth_code,
			getSalesList(),
			shmem->memb_pre_auth,memb_dollars,
			shmem->msr_name,shmem->memb_number,shmem->cc_resp.message
		);
		fflush (srv_fp);
		memset (&(shmem->cc_resp),0,sizeof (cc_resp_s));
		new_state = doReset();
	break;

	// Since we're communicating with our server also, process these as well
	case ServerTimeout_Evt:
		doLCD (1, 1, "Server Communication");
		doLCD (2, 1, "Network Timeout");
		new_state = NoServer_State; // prevent reset
	break;


	case NetStatus_Evt:
		if (!strcmp (val,"Connected") || !strcmp (val,"Update")) {
			if (shmem->cc_resp.doVoid) {
					doLCD (2, 1, "Voiding $%.2f",shmem->memb_pre_auth);
			} else {
					doLCD (2, 1, "Processing $%.2f",memb_dollars);
			}
		} else {
			doLCD (2, 1, val);
		}
	break;

	case ServerError_Evt:
		doLCD (1, 1, "Server Communication");
		doLCD (2, 1, "Error: %s",shmem->net_error);
		new_state = NoServer_State; // prevent reset
	break;

	default:
	break;
	}
	
	return (new_state);
}




// This state is entered from GatewayPreAuth_State
int doWaitDoorClosed     (const int evt, const char *val) {
int new_state = shmem->status_idx;

	switch (evt) {

	// Upon entry, setup an input timeout
	// This state is setup to exit to GatewayPreAuth_State
	// Or, reset on input timeout
	case EnterState_Evt:
		doLCD (1, 0, "Thank You!");
		doLCD (2, 0, "Please shut the door");
		doTimeout (shmem->door_close_timeout,"DoorClose");
	break;

	case ExitState_Evt:
		cancelTimeout ("DoorClose");
		doTimeout (shmem->lcd_timeout,"LCD");
	break;

	case DoorCloseTimeout_Evt:
		doNotice ("alert","Security\tDoor left open by member ID :%d (member # %s)",
			shmem->memb_id,shmem->memb_number);
		fflush (srv_fp);
		new_state = doReset();
	break;


	case DoorClosed_Evt:
		doLCD (1, 1, "Thank You!");
		doLCD (2, 1, "Remember fuel cap!");
		new_state = doReset();
	break;

	default:
	break;
	}
	
	return (new_state);
}

// This state is entered from Operator_State
// Exits back to operator on '*'
// Enters CalOnAmps_State, SetAvailGal_State, SetOperSPN_State
// Or reboots on '2#'
int doCalMenu         (const int evt, const char *val) {
int new_state = shmem->status_idx;
static int operator_exit=0, do_cancel=0;

	switch (evt) {
	// Upon entry, setup an input timeout
	// Put menu on LCD
	// --------------------
	// -----Cal Menu-------
	// --------------------
	// 1:Refill    2:Status
	// 3:Calibrate 4:Reboot
	// --------------------
	case EnterState_Evt:
		doLCD (1, 1, "1:Refill    2:Status");
		doLCD (2, 1, "3:Calibrate 4:Reboot");
		doTimeout (shmem->input_timeout,"Input");
		cancelTimeout ("LCD");
		checkLights();
		operator_exit=0;
		do_cancel = 0;
		memset (shmem->keypad_buffer,0,sizeof(shmem->keypad_buffer));
	break;

	case ExitState_Evt:
		cancelTimeout ("Input");
		memset (shmem->keypad_buffer,0,sizeof(shmem->keypad_buffer));
		if (operator_exit && operator_exit != new_state) {
			// Enter operator state to reset its substate
			doOperator (EnterState_Evt,"");
			doOperator (ExitState_Evt,"");
			do_cancel = 1;
			doLCD (2, 1, "Operator aborted");
		}
		operator_exit=0;
		
		if (do_cancel) {
		// Reset calibration settings
			doLCD (1, 1, "Settings Unchanged");
			cal_cfg_from_mem (cal_cfg,shmem);
		}
		do_cancel = 0;
	break;

	// Prevent doOperator from canceling if door stays closed
	// Global handler takes care of turning the relay off
	case StrikeRlyTimeout_Evt:
	break;

	// doOperator may have put a message on the LCD, this refreshes it for our state
	case LCDRefreshTimeout_Evt:
		doLCD (1, 1, "1:Refill    2:Status");
		doLCD (2, 1, "3:Calibrate 4:Reboot");
	break;

	case Keypad_Evt:
		// A '*' with an already empty keypad buffer is a reset
		if (*val == '*') {
			new_state = Operator_State;
			doLCD (2, 1, "User canceled");
			do_cancel = 1;
		} else {
			do_cancel = 0;
			if (*val == '1') {
				new_state = SetAvailGal_State;
			} else if (*val == '4') {
				// reboot
				doLCD (1, 1, "Rebooting. Please Wait...");
				usleep (100000);
				system (BBD9000reboot);
				sleep (30);
			} else if (*val == '3') {
				new_state = CalOnAmps_State;
			} else if (*val == '2') {
				new_state = ShowStatus_State;
			} else {
				doLCD (2, 1, "Unrecognized option");
				// Reset calibration settings
				do_cancel = 1;
				new_state = Operator_State;
			}
		}
	break;

	case InputTimeout_Evt:
		doLCD (2, 1, "Input Timeout");
		new_state = Operator_State;
		// Reset calibration settings
		do_cancel = 1;
	break;

	case DoorClosed_Evt:
		doLCD (2, 1, "Door Closed");
		operator_exit = new_state = doOperator (evt,val);
		// Reset calibration settings
		do_cancel = 1;
	break;

	default:
		// This is a sub-state of Operator, so we have it process any events we haven't over-ridden
		operator_exit = new_state = doOperator (evt,val);
	break;
	}
	
	return (new_state);
}

int doCalOnAmps       (const int evt, const char *val) {
int new_state = shmem->status_idx;
static int operator_exit=0, do_cancel=0;

	switch (evt) {
	// Upon entry, setup an input timeout
	// Put menu on LCD
	// --------------------
	// -----Cal On Amps----
	// --------------------
	// On Amps:     00.10 A    <- current threshold
	// C:00.00   ON:00.00 A    <- current amps
	// --------------------
	case EnterState_Evt:
		doLCD (1, 1, "On Amps:     %5.2f A",shmem->pump_on_threshold);
		doLCD (2, 1, "C:%5.2f   ON:%5.2f A",shmem->current,
			cfg_getfloat (cal_cfg,"pump_on_threshold")
		);
		doTimeout (shmem->input_timeout,"Input");
		cancelTimeout ("LCD");
		checkLights();
		doTimeout (shmem->smartIO_interval,"SmartIO");
		operator_exit=0;
		do_cancel=0;
		memset (shmem->keypad_buffer,0,sizeof(shmem->keypad_buffer));
	break;

	case ExitState_Evt:
		cancelTimeout ("Input");
		cancelTimeout ("SmartIO");
		memset (shmem->keypad_buffer,0,sizeof(shmem->keypad_buffer));
		if (operator_exit && operator_exit != new_state) {
			// Enter operator state to reset its substate
			doOperator (EnterState_Evt,"");
			doOperator (ExitState_Evt,"");
			do_cancel = 1;
			doLCD (2, 1, "Operator aborted");
		}
		operator_exit=0;
		
		if (do_cancel) {
		// Reset calibration settings
			doLCD (1, 1, "Settings Unchanged");
			cal_cfg_from_mem (cal_cfg,shmem);
		}
		do_cancel = 0;
	break;

	// Prevent doOperator from canceling if door stays closed
	// Global handler takes care of turning the relay off
	case StrikeRlyTimeout_Evt:
	break;

	// doOperator may have put a message on the LCD, this refreshes it for our state
	case LCDRefreshTimeout_Evt:
		doLCD (1, 0, "On Amps:     %5.2f A",shmem->pump_on_threshold);
		doLCD (2, 0, "C:%5.2f   ON:%5.2f A",shmem->current,
			cfg_getfloat (cal_cfg,"pump_on_threshold")
		);
	break;

	case SmartIOTimeout_Evt:
		doTimeout (shmem->smartIO_interval,"SmartIO");
		doLCD (2, 1, "C:%5.2f   ON:%5.2f A",shmem->current,
			cfg_getfloat (cal_cfg,"pump_on_threshold")
		);
		fprintf (out_fp,"PMP\n");
		fflush (out_fp);
	break;

	case Keypad_Evt:
		doTimeout (shmem->input_timeout,"Input");
		// A '*' with an already empty keypad buffer is a reset
		if (*val == '*') {
			if (do_cancel) {
				cancelTimeout ("SmartIO");
				new_state = Operator_State;
				doLCD (2, 1, "User canceled");
				break;
			}
			do_cancel = 1;
		} else {
			do_cancel = 0;
		}

		// If the buffer is empty ('*' was pressed, or no entry), set it to shmem
		if (! *(shmem->keypad_buffer) ) {
			cfg_setfloat(cal_cfg, "pump_on_threshold", shmem->pump_on_threshold);
		} else {
			cfg_setfloat(cal_cfg, "pump_on_threshold", (float)(atoi (shmem->keypad_buffer)) / 100.0);
		}
		doLCD (2, 1, "C:%5.2f   ON:%5.2f A",shmem->current,
			cfg_getfloat (cal_cfg,"pump_on_threshold")
		);
	break;

	case KeypadValid_Evt:
		cancelTimeout ("Input");
		do_cancel = 0;
		if (! *(shmem->keypad_buffer) ) {
			cfg_setfloat(cal_cfg, "pump_on_threshold", shmem->pump_on_threshold);
		} else {
			cfg_setfloat(cal_cfg, "pump_on_threshold", (float)(atoi (shmem->keypad_buffer)) / 100.0);
		}
		new_state = CalOffAmps_State;
	break;

	case InputTimeout_Evt:
		doLCD (2, 1, "Input Timeout");
		new_state = Operator_State;
		// Reset calibration settings
		do_cancel = 1;
	break;

	case DoorClosed_Evt:
		doLCD (2, 1, "Door Closed");
		operator_exit = new_state = doOperator (evt,val);
		// Reset calibration settings
		do_cancel = 1;
	break;

	default:
		// This is a sub-state of Operator, so we have it process any events we haven't over-ridden
		operator_exit = new_state = doOperator (evt,val);
	break;
	}
	
	return (new_state);
}


int doCalOffAmps      (const int evt, const char *val) {
int new_state = shmem->status_idx;
static int operator_exit=0, do_cancel=0;

	switch (evt) {
	// Upon entry, setup an input timeout
	// Put menu on LCD
	// --------------------
	// -----Cal Off Amps---
	// --------------------
	// Off Amps:    00.10 A    <- current threshold
	// C:00.00  OFF:00.00 A    <- current amps
	// --------------------
	case EnterState_Evt:
		doLCD (1, 1, "Off Amps:    %5.2f A",shmem->pump_off_threshold);
		doLCD (2, 1, "C:%5.2f  OFF:%5.2f A",shmem->current,
			cfg_getfloat (cal_cfg,"pump_off_threshold")
		);
		doTimeout (shmem->input_timeout,"Input");
		cancelTimeout ("LCD");
		checkLights();
		doTimeout (shmem->smartIO_interval,"SmartIO");
		operator_exit=0;
		do_cancel=0;
		memset (shmem->keypad_buffer,0,sizeof(shmem->keypad_buffer));
	break;

	case ExitState_Evt:
		cancelTimeout ("Input");
		cancelTimeout ("SmartIO");
		memset (shmem->keypad_buffer,0,sizeof(shmem->keypad_buffer));
		if (operator_exit && operator_exit != new_state) {
			// Enter operator state to reset its substate
			doOperator (EnterState_Evt,"");
			doOperator (ExitState_Evt,"");
			do_cancel = 1;
			doLCD (2, 1, "Operator aborted");
		}
		operator_exit=0;
		
		if (do_cancel) {
		// Reset calibration settings
			doLCD (1, 1, "Settings Unchanged");
			cal_cfg_from_mem (cal_cfg,shmem);
		}
		do_cancel = 0;
	break;

	// Prevent doOperator from canceling if door stays closed
	// Global handler takes care of turning the relay off
	case StrikeRlyTimeout_Evt:
	break;

	// doOperator may have put a message on the LCD, this refreshes it for our state
	case LCDRefreshTimeout_Evt:
		doLCD (1, 0, "Off Amps:    %5.2f A",shmem->pump_off_threshold);
		doLCD (2, 0, "C:%5.2f  OFF:%5.2f A",shmem->current,
			cfg_getfloat (cal_cfg,"pump_off_threshold")
		);
	break;

	case SmartIOTimeout_Evt:
		doTimeout (shmem->smartIO_interval,"SmartIO");
		doLCD (2, 1, "C:%5.2f  OFF:%5.2f A",shmem->current,
			cfg_getfloat (cal_cfg,"pump_off_threshold")
		);
		fprintf (out_fp,"PMP\n");
		fflush (out_fp);
	break;

	case Keypad_Evt:
		doTimeout (shmem->input_timeout,"Input");
		// A '*' with an already empty keypad buffer is a reset
		if (*val == '*') {
			if (do_cancel) {
				cancelTimeout ("SmartIO");
				new_state = Operator_State;
				doLCD (2, 1, "User canceled");
				break;
			}
			do_cancel = 1;
		} else {
			do_cancel = 0;
		}

		// If the buffer is empty ('*' was pressed, or no entry), set it to shmem
		if (! *(shmem->keypad_buffer) ) {
			cfg_setfloat(cal_cfg, "pump_off_threshold", shmem->pump_off_threshold);
		} else {
			cfg_setfloat(cal_cfg, "pump_off_threshold", (float)(atoi (shmem->keypad_buffer)) / 100.0);
		}
		doLCD (2, 1, "C:%5.2f  OFF:%5.2f A",shmem->current,
			cfg_getfloat (cal_cfg,"pump_off_threshold")
		);
	break;

	case KeypadValid_Evt:
		cancelTimeout ("Input");
		do_cancel = 0;
		if (! *(shmem->keypad_buffer) ) {
			cfg_setfloat(cal_cfg, "pump_off_threshold", shmem->pump_off_threshold);
		} else {
			cfg_setfloat(cal_cfg, "pump_off_threshold", (float)(atoi (shmem->keypad_buffer)) / 100.0);
		}
		new_state = CalKFact_State;
	break;

	case InputTimeout_Evt:
		doLCD (2, 1, "Input Timeout");
		new_state = Operator_State;
		// Reset calibration settings
		do_cancel = 1;
	break;

	case DoorClosed_Evt:
		doLCD (2, 1, "Door Closed");
		operator_exit = new_state = doOperator (evt,val);
		// Reset calibration settings
		do_cancel = 1;
	break;

	default:
		// This is a sub-state of Operator, so we have it process any events we haven't over-ridden
		operator_exit = new_state = doOperator (evt,val);
	break;
	}
	
	return (new_state);
}


int doCalKFact        (const int evt, const char *val) {
int new_state = shmem->status_idx;
static int operator_exit=0, do_cancel=0;

	switch (evt) {
	// Upon entry, setup an input timeout
	// Put menu on LCD
	// --------------------
	// -----Cal K Fact-----
	// --------------------
	// Pulses/gal.: 1234.56
	// Set to:      1234.56
	// --------------------
	case EnterState_Evt:
		doLCD (1, 1, "Pulses/gal.: %7.2f",shmem->flowmeter_pulses_per_gallon);
		doLCD (2, 1, "Set to:      %7.2f",cfg_getfloat (cal_cfg,"flowmeter_pulses_per_gallon"));
		doTimeout (shmem->input_timeout,"Input");
		cancelTimeout ("LCD");
		checkLights();
		operator_exit=0;
		do_cancel=0;
		memset (shmem->keypad_buffer,0,sizeof(shmem->keypad_buffer));
	break;

	case ExitState_Evt:
		cancelTimeout ("Input");
		memset (shmem->keypad_buffer,0,sizeof(shmem->keypad_buffer));
		if (operator_exit && operator_exit != new_state) {
			// Enter operator state to reset its substate
			doOperator (EnterState_Evt,"");
			doOperator (ExitState_Evt,"");
			do_cancel = 1;
			doLCD (2, 1, "Operator aborted");
		}
		operator_exit=0;
		
		if (do_cancel) {
		// Reset calibration settings
			doLCD (1, 1, "Settings Unchanged");
			cal_cfg_from_mem (cal_cfg,shmem);
		}
		do_cancel = 0;
	break;

	// Prevent doOperator from canceling if door stays closed
	// Global handler takes care of turning the relay off
	case StrikeRlyTimeout_Evt:
	break;

	// doOperator may have put a message on the LCD, this refreshes it for our state
	case LCDRefreshTimeout_Evt:
		doLCD (1, 0, "Pulses/gal.: %7.2f",shmem->flowmeter_pulses_per_gallon);
		doLCD (2, 0, "Set to:      %7.2f",cfg_getfloat (cal_cfg,"flowmeter_pulses_per_gallon"));
	break;

	case Keypad_Evt:
		doTimeout (shmem->input_timeout,"Input");
		// A '*' with an already empty keypad buffer is a reset
		if (*val == '*') {
			if (do_cancel) {
				new_state = Operator_State;
				doLCD (2, 1, "User canceled");
				break;
			}
			do_cancel = 1;
		} else {
			do_cancel = 0;
		}

		// If the buffer is empty ('*' was pressed, or no entry), set it to shmem
		if (! *(shmem->keypad_buffer) ) {
			cfg_setfloat(cal_cfg, "flowmeter_pulses_per_gallon",shmem->flowmeter_pulses_per_gallon);
		} else {
			cfg_setfloat(cal_cfg, "flowmeter_pulses_per_gallon",
				(float)(atoi (shmem->keypad_buffer)) / 100.0);
		}
		doLCD (2, 1, "Set to:      %7.2f",cfg_getfloat (cal_cfg,"flowmeter_pulses_per_gallon"));
	break;

	case KeypadValid_Evt:
		cancelTimeout ("Input");
		do_cancel = 0;
		if (! *(shmem->keypad_buffer) ) {
			cfg_setfloat(cal_cfg, "flowmeter_pulses_per_gallon",shmem->flowmeter_pulses_per_gallon);
		} else {
			cfg_setfloat(cal_cfg, "flowmeter_pulses_per_gallon",
				(float)(atoi (shmem->keypad_buffer)) / 100.0);
		}
		new_state = SetTankCap_State;
	break;

	case InputTimeout_Evt:
		doLCD (2, 1, "Input Timeout");
		new_state = Operator_State;
		// Reset calibration settings
		do_cancel = 1;
	break;

	case DoorClosed_Evt:
		doLCD (2, 1, "Door Closed");
		operator_exit = new_state = doOperator (evt,val);
		// Reset calibration settings
		do_cancel = 1;
	break;

	default:
		// This is a sub-state of Operator, so we have it process any events we haven't over-ridden
		operator_exit = new_state = doOperator (evt,val);
	break;
	}
	
	return (new_state);
}


int doSetTankCap      (const int evt, const char *val) {
int new_state = shmem->status_idx;
static int operator_exit=0, do_cancel=0;

	switch (evt) {
	// Upon entry, setup an input timeout
	// Put menu on LCD
	// --------------------
	// ----Set Tank Cap----
	// --------------------
	// Tank capacity: 1234g
	// Set to:        1234g
	// --------------------
	case EnterState_Evt:
		doLCD (1, 1, "Tank capacity: %4dg",shmem->tank_capacity);
		doLCD (2, 1, "Set to:        %4dg",(int)cfg_getint (cal_cfg,"tank_capacity"));
		doTimeout (shmem->input_timeout,"Input");
		cancelTimeout ("LCD");
		checkLights();
		operator_exit=0;
		do_cancel=0;
		memset (shmem->keypad_buffer,0,sizeof(shmem->keypad_buffer));
	break;

	case ExitState_Evt:
		cancelTimeout ("Input");
		memset (shmem->keypad_buffer,0,sizeof(shmem->keypad_buffer));
		if (operator_exit && operator_exit != new_state) {
			// Enter operator state to reset its substate
			doOperator (EnterState_Evt,"");
			doOperator (ExitState_Evt,"");
			do_cancel = 1;
			doLCD (2, 1, "Operator aborted");
		}
		operator_exit=0;
		
		if (do_cancel) {
		// Reset calibration settings
			doLCD (1, 1, "Settings Unchanged");
			cal_cfg_from_mem (cal_cfg,shmem);
		}
		do_cancel = 0;
	break;

	// Prevent doOperator from canceling if door stays closed
	// Global handler takes care of turning the relay off
	case StrikeRlyTimeout_Evt:
	break;

	// doOperator may have put a message on the LCD, this refreshes it for our state
	case LCDRefreshTimeout_Evt:
		doLCD (1, 0, "Tank capacity: %4dg",shmem->tank_capacity);
		doLCD (2, 0, "Set to:        %4dg",(int)cfg_getint (cal_cfg,"tank_capacity"));
	break;

	case Keypad_Evt:
		doTimeout (shmem->input_timeout,"Input");
		// A '*' with an already empty keypad buffer is a reset
		if (*val == '*') {
			if (do_cancel) {
				new_state = Operator_State;
				doLCD (2, 1, "User canceled");
				break;
			}
			do_cancel = 1;
		} else {
			do_cancel = 0;
		}

		// If the buffer is empty ('*' was pressed, or no entry), set it to shmem
		if (! *(shmem->keypad_buffer) ) {
			cfg_setint(cal_cfg, "tank_capacity", shmem->tank_capacity);
		} else {
			cfg_setint(cal_cfg, "tank_capacity", atoi (shmem->keypad_buffer));
		}
		doLCD (2, 1, "Set to:        %4dg",(int)cfg_getint (cal_cfg,"tank_capacity"));
	break;

	case KeypadValid_Evt:
		cancelTimeout ("Input");
		do_cancel = 0;
		if (! *(shmem->keypad_buffer) ) {
			cfg_setint(cal_cfg, "tank_capacity", shmem->tank_capacity);
		} else {
			cfg_setint(cal_cfg, "tank_capacity", atoi (shmem->keypad_buffer));
		}
		new_state = SetLowFuel_State;
	break;

	case InputTimeout_Evt:
		doLCD (2, 1, "Input Timeout");
		new_state = Operator_State;
		// Reset calibration settings
		do_cancel = 1;
	break;

	case DoorClosed_Evt:
		doLCD (2, 1, "Door Closed");
		operator_exit = new_state = doOperator (evt,val);
		// Reset calibration settings
		do_cancel = 1;
	break;

	default:
		// This is a sub-state of Operator, so we have it process any events we haven't over-ridden
		operator_exit = new_state = doOperator (evt,val);
	break;
	}
	
	return (new_state);
}


int doSetLowFuel      (const int evt, const char *val) {
int new_state = shmem->status_idx;
static int operator_exit=0, do_cancel=0;

	switch (evt) {
	// Upon entry, setup an input timeout
	// Put menu on LCD
	// --------------------
	// -Set Low Fuel Alrm--
	// --------------------
	// Low fuel alrm: 1234g
	// Set to:        1234g
	// --------------------
	case EnterState_Evt:
		doLCD (1, 1, "Low fuel alrm: %4dg",shmem->low_fuel_alarm);
		doLCD (2, 1, "Set to:        %4dg",(int)cfg_getint (cal_cfg,"low_fuel_alarm"));
		doTimeout (shmem->input_timeout,"Input");
		cancelTimeout ("LCD");
		checkLights();
		operator_exit=0;
		do_cancel=0;
		memset (shmem->keypad_buffer,0,sizeof(shmem->keypad_buffer));
	break;

	case ExitState_Evt:
		cancelTimeout ("Input");
		memset (shmem->keypad_buffer,0,sizeof(shmem->keypad_buffer));
		if (operator_exit && operator_exit != new_state) {
			// Enter operator state to reset its substate
			doOperator (EnterState_Evt,"");
			doOperator (ExitState_Evt,"");
			do_cancel = 1;
			doLCD (2, 1, "Operator aborted");
		}
		operator_exit=0;
		
		if (do_cancel) {
		// Reset calibration settings
			doLCD (1, 1, "Settings Unchanged");
			cal_cfg_from_mem (cal_cfg,shmem);
		}
		do_cancel = 0;
	break;

	// Prevent doOperator from canceling if door stays closed
	// Global handler takes care of turning the relay off
	case StrikeRlyTimeout_Evt:
	break;

	// doOperator may have put a message on the LCD, this refreshes it for our state
	case LCDRefreshTimeout_Evt:
		doLCD (1, 0, "Low fuel alrm: %4dg",shmem->low_fuel_alarm);
		doLCD (2, 0, "Set to:        %4dg",(int)cfg_getint (cal_cfg,"low_fuel_alarm"));
	break;

	case Keypad_Evt:
		doTimeout (shmem->input_timeout,"Input");
		// A '*' with an already empty keypad buffer is a reset
		if (*val == '*') {
			if (do_cancel) {
				new_state = Operator_State;
				doLCD (2, 1, "User canceled");
				break;
			}
			do_cancel = 1;
		} else {
			do_cancel = 0;
		}

		// If the buffer is empty ('*' was pressed, or no entry), set it to shmem
		if (! *(shmem->keypad_buffer) ) {
			cfg_setint(cal_cfg, "low_fuel_alarm", shmem->low_fuel_alarm);
		} else {
			cfg_setint(cal_cfg, "low_fuel_alarm", atoi (shmem->keypad_buffer));
		}
		doLCD (2, 1, "Set to:        %4dg",(int)cfg_getint (cal_cfg,"low_fuel_alarm"));
	break;

	case KeypadValid_Evt:
		cancelTimeout ("Input");
		do_cancel = 0;
		// If the buffer is empty ('*' was pressed, or no entry), set it to shmem
		if (! *(shmem->keypad_buffer) ) {
			cfg_setint(cal_cfg, "low_fuel_alarm", shmem->low_fuel_alarm);
		} else {
			cfg_setint(cal_cfg, "low_fuel_alarm", atoi (shmem->keypad_buffer));
		}
		new_state = SetNoFuel_State;
	break;

	case InputTimeout_Evt:
		doLCD (2, 1, "Input Timeout");
		new_state = Operator_State;
		// Reset calibration settings
		do_cancel = 1;
	break;

	case DoorClosed_Evt:
		doLCD (2, 1, "Door Closed");
		operator_exit = new_state = doOperator (evt,val);
		// Reset calibration settings
		do_cancel = 1;
	break;

	default:
		// This is a sub-state of Operator, so we have it process any events we haven't over-ridden
		operator_exit = new_state = doOperator (evt,val);
	break;
	}
	
	return (new_state);
}


int doSetNoFuel       (const int evt, const char *val) {
int new_state = shmem->status_idx;
static int operator_exit=0, do_cancel=0;

	switch (evt) {
	// Upon entry, setup an input timeout
	// Put menu on LCD
	// --------------------
	// --Set No Fuel Alrm--
	// --------------------
	// No fuel cutoff:1234g
	// Set to:        1234g
	// --------------------
	case EnterState_Evt:
		doLCD (1, 1, "No fuel cutoff:%4dg",shmem->no_fuel_cutoff);
		doLCD (2, 1, "Set to:        %4dg",(int)cfg_getint (cal_cfg,"no_fuel_cutoff"));
		doTimeout (shmem->input_timeout,"Input");
		cancelTimeout ("LCD");
		checkLights();
		operator_exit=0;
		do_cancel=0;
		memset (shmem->keypad_buffer,0,sizeof(shmem->keypad_buffer));
	break;

	case ExitState_Evt:
		cancelTimeout ("Input");
		memset (shmem->keypad_buffer,0,sizeof(shmem->keypad_buffer));
		if (operator_exit && operator_exit != new_state) {
			// Enter operator state to reset its substate
			doOperator (EnterState_Evt,"");
			doOperator (ExitState_Evt,"");
			do_cancel = 1;
			doLCD (2, 1, "Operator aborted");
		}
		operator_exit=0;
		
		if (do_cancel) {
		// Reset calibration settings
			doLCD (1, 1, "Settings Unchanged");
			cal_cfg_from_mem (cal_cfg,shmem);
		}
		do_cancel = 0;
	break;

	// Prevent doOperator from canceling if door stays closed
	// Global handler takes care of turning the relay off
	case StrikeRlyTimeout_Evt:
	break;

	// doOperator may have put a message on the LCD, this refreshes it for our state
	case LCDRefreshTimeout_Evt:
		doLCD (1, 0, "No fuel cutoff:%4dg",shmem->no_fuel_cutoff);
		doLCD (2, 0, "Set to:        %4dg",(int)cfg_getint (cal_cfg,"no_fuel_cutoff"));
	break;

	case Keypad_Evt:
		doTimeout (shmem->input_timeout,"Input");
		// A '*' with an already empty keypad buffer is a reset
		if (*val == '*') {
			if (do_cancel) {
				new_state = Operator_State;
				doLCD (2, 1, "User canceled");
				break;
			}
			do_cancel = 1;
		} else {
			do_cancel = 0;
		}

		// If the buffer is empty ('*' was pressed, or no entry), set it to shmem
		if (! *(shmem->keypad_buffer) ) {
			cfg_setint(cal_cfg, "no_fuel_cutoff", shmem->no_fuel_cutoff);
		} else {
			cfg_setint(cal_cfg, "no_fuel_cutoff", atoi (shmem->keypad_buffer));
		}
		doLCD (2, 1, "Set to:        %4dg",(int)cfg_getint (cal_cfg,"no_fuel_cutoff"));
	break;

	case KeypadValid_Evt:
		cancelTimeout ("Input");
		do_cancel = 0;
		// If the buffer is empty ('*' was pressed, or no entry), set it to shmem
		if (! *(shmem->keypad_buffer) ) {
			cfg_setint(cal_cfg, "no_fuel_cutoff", shmem->no_fuel_cutoff);
		} else {
			cfg_setint(cal_cfg, "no_fuel_cutoff", atoi (shmem->keypad_buffer));
		}

		new_state = SetOperSPN_State;
	break;

	case InputTimeout_Evt:
		doLCD (2, 1, "Input Timeout");
		new_state = Operator_State;
		// Reset calibration settings
		do_cancel = 1;
	break;

	case DoorClosed_Evt:
		doLCD (2, 1, "Door Closed");
		operator_exit = new_state = doOperator (evt,val);
		// Reset calibration settings
		do_cancel = 1;
	break;

	default:
		// This is a sub-state of Operator, so we have it process any events we haven't over-ridden
		operator_exit = new_state = doOperator (evt,val);
	break;
	}
	
	return (new_state);
}


int doSetOperSPN      (const int evt, const char *val) {
int new_state = shmem->status_idx;
static int operator_exit=0, do_cancel=0;

	switch (evt) {
	// Upon entry, setup an input timeout
	// Put menu on LCD
	// --------------------
	// ----Set Oper SPN----
	// --------------------
	// New Operator SPN:
	// ********************
	// --------------------
	case EnterState_Evt:
		doLCD (1, 1, "New Operator SPN:");
		doLCD (2, 1, "********************");
		doTimeout (shmem->input_timeout,"Input");
		cancelTimeout ("LCD");
		checkLights();
		operator_exit=0;
		do_cancel=0;
		memset (shmem->keypad_buffer,0,sizeof(shmem->keypad_buffer));

	break;

	case ExitState_Evt:
		// The keypad buffer contains the plaintext SPN for confirmation in ConfOperSPN
		// We zero it out only if we're canceling.
		cancelTimeout ("Input");
		if (operator_exit && operator_exit != new_state) {
			memset (shmem->keypad_buffer,0,sizeof(shmem->keypad_buffer));
			// Enter operator state to reset its substate
			doOperator (EnterState_Evt,"");
			doOperator (ExitState_Evt,"");
			do_cancel = 1;
			doLCD (2, 1, "Operator aborted");
		}
		operator_exit=0;
		
		if (do_cancel) {
		// Reset calibration settings
			doLCD (1, 1, "Settings Unchanged");
			cal_cfg_from_mem (cal_cfg,shmem);
			memset (shmem->keypad_buffer,0,sizeof(shmem->keypad_buffer));
		}
		do_cancel = 0;
	break;

	// Prevent doOperator from canceling if door stays closed
	// Global handler takes care of turning the relay off
	case StrikeRlyTimeout_Evt:
	break;

	// doOperator may have put a message on the LCD, this refreshes it for our state
	case LCDRefreshTimeout_Evt:
		doLCD (1, 0, "New Operator SPN:");
		if (*(shmem->keypad_buffer)) {
			doKeypadStars(0);
		} else {
			doLCD (2, 0, "********************");
		}
	break;

	case Keypad_Evt:
		doTimeout (shmem->input_timeout,"Input");
		// A '*' with an already empty keypad buffer is a reset
		if (*val == '*') {
			if (do_cancel) {
				new_state = Operator_State;
				doLCD (2, 1, "User canceled");
				break;
			}
			do_cancel = 1;
		}

		do_cancel = 0;
		// If the buffer is empty ('*' was pressed, or no entry), set it to shmem
		if (! *(shmem->keypad_buffer) ) {
			doLCD (2, 1, "********************");
		} else {
			doKeypadStars(1);
		}
	break;

	case KeypadValid_Evt:
		cancelTimeout ("Input");
		do_cancel = 0;
		if ( !*(shmem->keypad_buffer) ) {
			doLCD (2, 1, "SPN Unchanged");
			cfg_setstr(cal_cfg, "operator_code", shmem->operator_code);
			// Set shmem based on cal_cfg - new settings are in cal_cfg
			cal_cfg_to_mem (cal_cfg,shmem);
			new_state = Operator_State;
			cfg_setstr(cal_cfg, "operator_code", shmem->operator_code);

			// Set shmem based on cal_cfg - new settings are in cal_cfg
			cal_cfg_to_mem (cal_cfg,shmem);
			// Save the settings
			cal_cfg_write(shmem);
			doLCD (1, 1, "Settings Saved");
			doLCD (2, 0, "");
		} else if ( strlen (shmem->keypad_buffer) < MIN_OP_SPN_LENGTH ) {
			doLCD (2, 1, "SPN < %d digits",MIN_OP_SPN_LENGTH);
			new_state = Operator_State;
			do_cancel = 1;
		} else if ( strlen (shmem->keypad_buffer) > MAX_OP_SPN_LENGTH) {
			doLCD (2, 1, "SPN > %d digits",MAX_OP_SPN_LENGTH);
			new_state = Operator_State;
			do_cancel = 1;
		} else {
			new_state = ConfOperSPN_State;
		}
	break;

	case InputTimeout_Evt:
		doLCD (2, 1, "Input Timeout");
		new_state = Operator_State;
		// Reset calibration settings
		do_cancel = 1;
	break;

	case DoorClosed_Evt:
		doLCD (2, 1, "Door Closed");
		operator_exit = new_state = doOperator (evt,val);
		// Reset calibration settings
		do_cancel = 1;
	break;

	default:
		// This is a sub-state of Operator, so we have it process any events we haven't over-ridden
		operator_exit = new_state = doOperator (evt,val);
	break;
	}
	
	return (new_state);
}


int doConfOperSPN     (const int evt, const char *val) {
int new_state = shmem->status_idx;
static int operator_exit=0, do_cancel=0;
static char prev_spn[KEYPAD_BUFF_SIZE];

	switch (evt) {
	// Upon entry, setup an input timeout
	// Put menu on LCD
	// --------------------
	// -----Cal Menu-------
	// --------------------
	// Confirm Opertr. SPN
	// ********************
	// --------------------
	case EnterState_Evt:
		doLCD (1, 1, "Confirm Opertr. SPN");
		doLCD (2, 1, "********************");
		doTimeout (shmem->input_timeout,"Input");
		cancelTimeout ("LCD");
		checkLights();
		operator_exit=0;
		do_cancel=0;
		// Copy the keypad buffer for comparisson
		memset (prev_spn,0,sizeof(prev_spn));
		strcpy (prev_spn,shmem->keypad_buffer);
		memset (shmem->keypad_buffer,0,sizeof(shmem->keypad_buffer));
	break;

	case ExitState_Evt:
		cancelTimeout ("Input");
		memset (shmem->keypad_buffer,0,sizeof(shmem->keypad_buffer));
		memset (prev_spn,0,sizeof(prev_spn));
		if (operator_exit && operator_exit != new_state) {
			// Enter operator state to reset its substate
			doOperator (EnterState_Evt,"");
			doOperator (ExitState_Evt,"");
			do_cancel = 1;
			doLCD (2, 1, "Operator aborted");
		}
		operator_exit=0;
		
		if (do_cancel) {
		// Reset calibration settings
			doLCD (1, 1, "Settings Unchanged");
			cal_cfg_from_mem (cal_cfg,shmem);
		}
		do_cancel = 0;
	break;

	// Prevent doOperator from canceling if door stays closed
	// Global handler takes care of turning the relay off
	case StrikeRlyTimeout_Evt:
	break;

	// doOperator may have put a message on the LCD, this refreshes it for our state
	case LCDRefreshTimeout_Evt:
		doLCD (1, 0, "Confirm Opertr. SPN");
		if (*(shmem->keypad_buffer)) {
			doKeypadStars(0);
		} else {
			doLCD (2, 0, "********************");
		}
	break;

	case Keypad_Evt:
		doTimeout (shmem->input_timeout,"Input");
		// A '*' with an already empty keypad buffer is a reset
		if (*val == '*') {
			if (do_cancel) {
				new_state = Operator_State;
				doLCD (2, 1, "User canceled");
				break;
			}
			do_cancel = 1;
		}

		do_cancel = 0;
		// If the buffer is empty ('*' was pressed, or no entry), set it to shmem
		if (! *(shmem->keypad_buffer) ) {
			doLCD (2, 1, "********************");
		} else {
			doKeypadStars(1);
		}
	break;

	case KeypadValid_Evt:
		cancelTimeout ("Input");
		do_cancel = 0;
		if ( !strcmp (shmem->keypad_buffer,prev_spn) ) {
			encrypt_SPN (shmem->operator_code,shmem->keypad_buffer);
			cfg_setstr(cal_cfg, "operator_code", shmem->operator_code);

			// Set shmem based on cal_cfg - new settings are in cal_cfg
			cal_cfg_to_mem (cal_cfg,shmem);
			// Save the settings
			cal_cfg_write(shmem);
			doLCD (1, 1, "Settings Saved");
			doLCD (2, 1, "");
		} else {
			doLCD (1, 1, "Settings Unchanged");
			doLCD (2, 1, "SPN mismatch");
			do_cancel = 1;
		}
		new_state = Operator_State;
	break;

	case InputTimeout_Evt:
		doLCD (2, 1, "Input Timeout");
		new_state = Operator_State;
		// Reset calibration settings
		do_cancel = 1;
	break;

	case DoorClosed_Evt:
		doLCD (2, 1, "Door Closed");
		operator_exit = new_state = doOperator (evt,val);
		// Reset calibration settings
		do_cancel = 1;
	break;

	default:
		// This is a sub-state of Operator, so we have it process any events we haven't over-ridden
		operator_exit = new_state = doOperator (evt,val);
	break;
	}
	
	return (new_state);
}


int doSetAvailGal     (const int evt, const char *val) {
int new_state = shmem->status_idx;
static int operator_exit=0, do_cancel=0;

	if (! *(shmem->keypad_buffer) ) {
		avail_gallons = shmem->avail_gallons;
	} else {
		avail_gallons = (float)(atoi(shmem->keypad_buffer)) / 100.0;
	}

	switch (evt) {
	// Upon entry, setup an input timeout
	// Put menu on LCD
	// --------------------
	// -----Cal Menu-------
	// --------------------
	// Avail. Gals.:9999.99
	// Set to:      1234.00
	// --------------------
	case EnterState_Evt:
		doLCD (1, 1, "Avail. Gals.:%7.2f",shmem->avail_gallons);
		doLCD (2, 1, "Set to:      %7.2f",avail_gallons);
		doTimeout (shmem->input_timeout,"Input");
		cancelTimeout ("LCD");
		checkLights();
		operator_exit=0;
		do_cancel=0;
		memset (shmem->keypad_buffer,0,sizeof(shmem->keypad_buffer));
	break;

	case ExitState_Evt:
		cancelTimeout ("Input");
		memset (shmem->keypad_buffer,0,sizeof(shmem->keypad_buffer));
		if (operator_exit && operator_exit != new_state) {
			// Enter operator state to reset its substate
			doOperator (EnterState_Evt,"");
			doOperator (ExitState_Evt,"");
			do_cancel = 1;
			doLCD (2, 1, "Operator aborted");
		}
		operator_exit=0;
		
		if (do_cancel) {
		// Reset calibration settings
			doLCD (1, 1, "Settings Unchanged");
		}
		do_cancel = 0;
	break;

	// Prevent doOperator from canceling if door stays closed
	// Global handler takes care of turning the relay off
	case StrikeRlyTimeout_Evt:
	break;

	// doOperator may have put a message on the LCD, this refreshes it for our state
	case LCDRefreshTimeout_Evt:
		doLCD (1, 0, "Avail. Gals.:%7.2f",shmem->avail_gallons);
		doLCD (2, 0, "Set to:      %7.2f",avail_gallons);
	break;

	case Keypad_Evt:
		doTimeout (shmem->input_timeout,"Input");
		// A '*' with an already empty keypad buffer is a reset
		if (*val == '*') {
			if (do_cancel) {
				new_state = Operator_State;
				doLCD (2, 1, "User canceled");
				break;
			}
			do_cancel = 1;
		} else {
			do_cancel = 0;
		}

		doLCD (2, 1, "Set to:      %7.2f",avail_gallons);
	break;

	case KeypadValid_Evt:
		cancelTimeout ("Input");
		do_cancel = 0;

		// avail_gallons is a run configuration setting, not cal.
		// nothing to save until we write the run-cal.
		new_state = SetFuelType_State;
	break;

	case InputTimeout_Evt:
		doLCD (2, 1, "Input Timeout");
		new_state = Operator_State;
		// Reset calibration settings
		do_cancel = 1;
	break;

	case DoorClosed_Evt:
		doLCD (2, 1, "Door Closed");
		operator_exit = new_state = doOperator (evt,val);
		// Reset calibration settings
		do_cancel = 1;
	break;

	default:
		// This is a sub-state of Operator, so we have it process any events we haven't over-ridden
		operator_exit = new_state = doOperator (evt,val);
	break;
	}
	
	return (new_state);
}


int doSetFuelType     (const int evt, const char *val) {
int new_state = shmem->status_idx;
static int operator_exit=0, do_cancel=0;

	if (! *(shmem->keypad_buffer) ) {
		snprintf (fuel_type,8,"%s",shmem->fuel_type);
	} else {
		if (!strcmp (shmem->keypad_buffer,"999")) {
			strcpy (fuel_type,"B99.9");
		} else if (atoi(shmem->keypad_buffer) <= 100) {
			snprintf (fuel_type,8,"B%d",atoi(shmem->keypad_buffer));
		} else {
			memset (shmem->keypad_buffer,0,sizeof(shmem->keypad_buffer));
			snprintf (fuel_type,8,"%s",shmem->fuel_type);
		}
	}

	switch (evt) {
	// Upon entry, setup an input timeout
	// Put menu on LCD
	// --------------------
	// -----Cal Menu-------
	// --------------------
	// Cur. Fuel Type:B99.9
	// Set to:        B
	// --------------------
	case EnterState_Evt:
		doLCD (1, 1, "Cur. Fuel Type:%5s",shmem->fuel_type);
		doLCD (2, 1, "Set to:        %5s",fuel_type);
		doTimeout (shmem->input_timeout,"Input");
		cancelTimeout ("LCD");
		checkLights();
		operator_exit=0;
		do_cancel=0;
		memset (shmem->keypad_buffer,0,sizeof(shmem->keypad_buffer));
	break;

	case ExitState_Evt:
		cancelTimeout ("Input");
		memset (shmem->keypad_buffer,0,sizeof(shmem->keypad_buffer));
		if (operator_exit && operator_exit != new_state) {
			// Enter operator state to reset its substate
			doOperator (EnterState_Evt,"");
			doOperator (ExitState_Evt,"");
			do_cancel = 1;
			doLCD (2, 1, "Operator aborted");
		}
		operator_exit=0;
		
		if (do_cancel) {
		// Reset calibration settings
			doLCD (1, 1, "Settings Unchanged");
		}
		do_cancel = 0;
	break;

	// Prevent doOperator from canceling if door stays closed
	// Global handler takes care of turning the relay off
	case StrikeRlyTimeout_Evt:
	break;

	// doOperator may have put a message on the LCD, this refreshes it for our state
	case LCDRefreshTimeout_Evt:
		doLCD (1, 0, "Cur. Fuel Type:%5s",shmem->fuel_type);
		doLCD (2, 0, "Set to:        %5s",fuel_type);
	break;

	case Keypad_Evt:
		doTimeout (shmem->input_timeout,"Input");
		// A '*' with an already empty keypad buffer is a reset
		if (*val == '*') {
			if (do_cancel) {
				new_state = Operator_State;
				doLCD (2, 1, "User canceled");
				break;
			}
			do_cancel = 1;
		} else {
			do_cancel = 0;
		}
		doLCD (2, 1, "Set to:        %5s",fuel_type);
	break;

	case KeypadValid_Evt:
		cancelTimeout ("Input");
		do_cancel = 0;
		new_state = SetPPG_State;
	break;

	case InputTimeout_Evt:
		doLCD (2, 1, "Input Timeout");
		new_state = Operator_State;
		// Reset calibration settings
		do_cancel = 1;
	break;

	case DoorClosed_Evt:
		doLCD (2, 1, "Door Closed");
		operator_exit = new_state = doOperator (evt,val);
		// Reset calibration settings
		do_cancel = 1;
	break;

	default:
		// This is a sub-state of Operator, so we have it process any events we haven't over-ridden
		operator_exit = new_state = doOperator (evt,val);
	break;
	}
	
	return (new_state);
}


int doSetPPG     (const int evt, const char *val) {
int new_state = shmem->status_idx;
static int operator_exit=0, do_cancel=0;


	if (! *(shmem->keypad_buffer) ) {
		last_ppg = shmem->last_ppg;
	} else {
		last_ppg = (float) atoi(shmem->keypad_buffer) / 100.0;
		if (last_ppg > 9.99) {
			last_ppg = shmem->last_ppg;
			memset (shmem->keypad_buffer,0,sizeof(shmem->keypad_buffer));
		}
	}

	switch (evt) {
	// Upon entry, setup an input timeout
	// Put menu on LCD
	// --------------------
	// -----Cal Menu-------
	// --------------------
	// Memb. Price: $0.00/g
	// Set to:      $0.00/g
	// --------------------
	case EnterState_Evt:
		doLCD (1, 1, "Memb. Price: $%4.2f",shmem->last_ppg);
		doLCD (2, 1, "Set to:      $%4.2f",last_ppg);
		doTimeout (shmem->input_timeout,"Input");
		cancelTimeout ("LCD");
		checkLights();
		operator_exit=0;
		do_cancel=0;
		memset (shmem->keypad_buffer,0,sizeof(shmem->keypad_buffer));
	break;

	case ExitState_Evt:
		cancelTimeout ("Input");
		memset (shmem->keypad_buffer,0,sizeof(shmem->keypad_buffer));
		if (operator_exit && operator_exit != new_state) {
			// Enter operator state to reset its substate
			doOperator (EnterState_Evt,"");
			doOperator (ExitState_Evt,"");
			do_cancel = 1;
			doLCD (2, 1, "Operator aborted");
		}
		operator_exit=0;
		
		if (do_cancel) {
		// Reset calibration settings
			doLCD (1, 1, "Settings Unchanged");
		}
		do_cancel = 0;
	break;

	// Prevent doOperator from canceling if door stays closed
	// Global handler takes care of turning the relay off
	case StrikeRlyTimeout_Evt:
	break;

	// doOperator may have put a message on the LCD, this refreshes it for our state
	case LCDRefreshTimeout_Evt:
		doLCD (1, 0, "Memb. Price: $%4.2f",shmem->last_ppg);
		doLCD (2, 0, "Set to:      $%4.2f",last_ppg);
	break;

	case Keypad_Evt:
		doTimeout (shmem->input_timeout,"Input");
		// A '*' with an already empty keypad buffer is a reset
		if (*val == '*') {
			if (do_cancel) {
				new_state = Operator_State;
				doLCD (2, 1, "User canceled");
				break;
			}
			do_cancel = 1;
		} else {
			do_cancel = 0;
		}
		doLCD (2, 1, "Set to:      $%4.2f",last_ppg);
	break;

	case KeypadValid_Evt:
		cancelTimeout ("Input");
		do_cancel = 0;
		doLCD (1, 1, "Settings Saved");
		doLCD (2, 1, "");
		shmem->avail_gallons = avail_gallons;
		strncpy (shmem->fuel_type,fuel_type,sizeof(shmem->fuel_type)-1);
		shmem->last_ppg = last_ppg;
		doNotice ("fuel","%.3f\t%s\t%.2f",
			avail_gallons,fuel_type,last_ppg);
		new_state = Operator_State;
	break;

	case InputTimeout_Evt:
		doLCD (2, 1, "Input Timeout");
		new_state = Operator_State;
		// Reset calibration settings
		do_cancel = 1;
	break;

	case DoorClosed_Evt:
		doLCD (2, 1, "Door Closed");
		operator_exit = new_state = doOperator (evt,val);
		// Reset calibration settings
		do_cancel = 1;
	break;

	default:
		// This is a sub-state of Operator, so we have it process any events we haven't over-ridden
		operator_exit = new_state = doOperator (evt,val);
	break;
	}
	
	return (new_state);
}



int doShowStatus     (const int evt, const char *val) {
int new_state = shmem->status_idx;
static int operator_exit=0;
char short_net[10];

	if ( ! strcmp(shmem->net_status,"offline") ) strcpy (short_net,"O/L");
	else if ( ! strcmp(shmem->net_status,"cellular") ) strcpy (short_net,"cell");
	else if ( ! strcmp(shmem->net_status,"ethernet") ) strcpy (short_net,"ether");
	else if ( ! strcmp(shmem->net_status,"WiFi") ) strcpy (short_net,"WiFi");
	else if (*(shmem->net_status)) snprintf (short_net,5,shmem->net_status);
	else snprintf (short_net,5,"???");

	switch (evt) {
	// Upon entry, setup an input timeout
	// Put menu on LCD
	// --------------------
	// ----Show Status-----
	// --------------------
	// 00.00 V   00.00 Amps
	// 0000.00 g Net: ether
	// --------------------
	case EnterState_Evt:
		doLCD (1, 1, "%5.2f V   %5.2f Amps",shmem->voltage, shmem->current);
		doLCD (2, 1, "%7.2f g Net: %s",shmem->avail_gallons, short_net);
		doTimeout (shmem->input_timeout,"Input");
		doTimeout (shmem->smartIO_interval,"SmartIO");
		cancelTimeout ("LCD");
		checkLights();
		operator_exit=0;
		memset (shmem->keypad_buffer,0,sizeof(shmem->keypad_buffer));
	break;

	case ExitState_Evt:
		cancelTimeout ("Input");
		cancelTimeout ("SmartIO");
		memset (shmem->keypad_buffer,0,sizeof(shmem->keypad_buffer));
		if (operator_exit && operator_exit != new_state) {
			// Enter operator state to reset its substate
			doOperator (EnterState_Evt,"");
			doOperator (ExitState_Evt,"");
			doLCD (2, 1, "Operator aborted");
		} else {
			doLCD (0, 1, NULL); // release the LCD
		}
		operator_exit=0;
	break;

	// Prevent doOperator from canceling if door stays closed
	// Global handler takes care of turning the relay off
	case StrikeRlyTimeout_Evt:
	break;

	// doOperator may have put a message on the LCD, this refreshes it for our state
	case LCDRefreshTimeout_Evt:
		doLCD (1, 1, "%5.2f V   %5.2f Amps",shmem->voltage, shmem->current);
		doLCD (2, 1, "%7.2f g Net: %s",shmem->avail_gallons, short_net);
	break;

	case SmartIOTimeout_Evt:
		doTimeout (shmem->smartIO_interval,"SmartIO");
		doLCD (1, 1, "%5.2f V   %5.2f Amps",shmem->voltage, shmem->current);
		doLCD (2, 1, "%7.2f g Net: %s",shmem->avail_gallons, short_net);
		fprintf (out_fp,"PMP\nVIN\n");
		fflush (out_fp);
	break;

	case Keypad_Evt:
		doTimeout (shmem->input_timeout,"Input");
		if (*val == '*') {
				new_state = Operator_State;
			}
	break;

	case KeypadValid_Evt:
		cancelTimeout ("Input");
		new_state = ShowNetStatus_State;
	break;

	case InputTimeout_Evt:
		doTimeout (shmem->input_timeout,"Input");
	break;

	case DoorClosed_Evt:
		doLCD (2, 1, "Door Closed");
		operator_exit = new_state = doOperator (evt,val);
	break;

	default:
		// This is a sub-state of Operator, so we have it process any events we haven't over-ridden
		operator_exit = new_state = doOperator (evt,val);
	break;
	}
	
	return (new_state);
}



int doShowNetStatus     (const int evt, const char *val) {
int new_state = shmem->status_idx;
static int operator_exit=0;

	switch (evt) {
	// Upon entry, setup an input timeout
	// Put menu on LCD
	// --------------------
	// ----Show Status-----
	// --------------------
	// Eth...
	// Cel...
	// --------------------
	case EnterState_Evt:
		doLCD (1, 1, "Eth:%s.",shmem->net_status_ethernet);
		doLCD (2, 1, "Cel:%s.",shmem->net_status_cellular);
		doTimeout (shmem->input_timeout,"Input");
		cancelTimeout ("LCD");
		checkLights();
		operator_exit=0;
		memset (shmem->keypad_buffer,0,sizeof(shmem->keypad_buffer));
	break;

	case ExitState_Evt:
		cancelTimeout ("Input");
		memset (shmem->keypad_buffer,0,sizeof(shmem->keypad_buffer));
		if (operator_exit && operator_exit != new_state) {
			// Enter operator state to reset its substate
			doOperator (EnterState_Evt,"");
			doOperator (ExitState_Evt,"");
			doLCD (2, 1, "Operator aborted");
		} else {
			doLCD (0, 1, NULL); // release the LCD
		}
		operator_exit=0;
	break;

	// Prevent doOperator from canceling if door stays closed
	// Global handler takes care of turning the relay off
	case StrikeRlyTimeout_Evt:
	break;

	// doOperator may have put a message on the LCD, this refreshes it for our state
	case LCDRefreshTimeout_Evt:
		doLCD (1, 1, "Eth:%s",shmem->net_status_ethernet);
		doLCD (2, 1, "Cel:%s",shmem->net_status_cellular);
	break;

	case Keypad_Evt:
		doTimeout (shmem->input_timeout,"Input");
	break;

	case KeypadValid_Evt:
		cancelTimeout ("Input");
		new_state = Operator_State;
	break;

	case InputTimeout_Evt:
		doTimeout (shmem->input_timeout,"Input");
	break;

	case DoorClosed_Evt:
		doLCD (2, 1, "Door Closed");
		operator_exit = new_state = doOperator (evt,val);
	break;

	default:
		// This is a sub-state of Operator, so we have it process any events we haven't over-ridden
		operator_exit = new_state = doOperator (evt,val);
	break;
	}
	
	return (new_state);
}




/*******************
  Utility Functions
********************/

void doStatusTimeout () {	
struct timeval t_start, t_now;

	gettimeofday(&t_start, NULL);
	t_now = t_start;
	fprintf (out_fp,"PMP\nVIN\n");
	fflush (out_fp);
	while (  (TV_ELAPSED_MS (t_start,shmem->t_voltage) > 1000 ||  TV_ELAPSED_MS (t_start,shmem->t_voltage) > 1000) && TV_ELAPSED_MS (t_now,t_start) < 1000) {
		usleep (100000); // Give SmartIO a chance to respond - up to 1 sec from while condition
		gettimeofday(&t_now, NULL);
	}

	// Setup the timeout
	if (shmem->server) {
		shmem->status_interval = shmem->status_interval_net;
	} else {
		shmem->status_interval = shmem->status_interval_no_net;
	}

	doTimeout (shmem->status_interval,"Status");
	// Server expects: ($status,$fuel_avail,$fuel_type,$last_ppg,$voltage,$next_checkin)
	fprintf (srv_fp,"status\t%s\t%.3f\t%s\t%.2f\t%.2f\t%d\n",shmem->status,shmem->avail_gallons,shmem->fuel_type,shmem->last_ppg,shmem->voltage,(int)((shmem->status_interval/1000)+time(NULL)+60) );
	fflush (srv_fp);
	shmem->last_status = time (NULL);
	shmem->checkin_msg = 0;
}

void doKeypadStars (int force) {
char *key,*stars_p,stars[KEYPAD_BUFF_SIZE];

	key = shmem->keypad_buffer;
	stars_p = stars;
	while (*key) {
		*stars_p++ = '*';
		key++;
	}
	*stars_p = '\0';
	doLCD (2, force, stars);
}


void doFlowmeter () {
int count;
float gallons, d_gallons;
static float last_gallons=0.0;

	// count is pulses since last counter reset
	count = shmem->flowmeter_counts;

	if (tank_gals_at_start == 0.0)
		tank_gals_at_start = shmem->avail_gallons;

	gallons = (float)count / shmem->flowmeter_pulses_per_gallon;
	if (gallons > last_gallons)
		d_gallons = gallons - last_gallons;
	else
		d_gallons = gallons;
	
	shmem->cumulative_gallons += d_gallons;
	shmem->avail_gallons -= d_gallons;
	shmem->memb_gallons = gallons;

	if (shmem->avail_gallons < shmem->no_fuel_cutoff) {
		doLCD (1, 1, "Out of fuel!");
		fprintf (out_fp,"PumpRly\t0\n");
		fflush (out_fp);
	}

	if ( isStatus (Dispensing_State) || isStatus (Operator_State) ) {
		// process what flowed
		shmem->memb_dollars = (-shmem->memb_credit) +
			(shmem->memb_gallons * shmem->memb_ppg) +
			(shmem->memb_renewal_sale ? shmem->renewal_fee : 0) +
			(shmem->memb_full_membership_sale ? shmem->full_membership_fee : 0) +
			(shmem->memb_temp_membership_sale ? shmem->temp_membership_fee : 0) +
			(shmem->memb_upgrade_sale ? shmem->upgrade_fee : 0)
		;
		// The server registers membership sales when making a new membership and reflects the ammount in the credit.
		if ( isStatus (Dispensing_State) && shmem->memb_dollars >= shmem->memb_pre_auth ) {
			doLCD (1, 1, "Pre-auth reached");
			fprintf (out_fp,"PumpRly\t0\n");
			fflush (out_fp);
		}
		
		doLCD (2, 1, "%5.1f $%4.2f $%7.2f",
			shmem->memb_gallons,shmem->memb_ppg,shmem->memb_dollars);
		fflush (out_fp);
	} else {
		cancelTimeout ("Dispensing");
		fprintf (out_fp,"PumpRly\t0\n");
		fflush (out_fp);
		if ( !shmem->is_violation ) {
		// We're not going to set a violation until we see 0.01 gallons go through
			tank_gals_at_start = shmem->avail_gallons;
			shmem->is_violation = 1;
		} else if (shmem->is_violation < 2 && tank_gals_at_start - shmem->avail_gallons > 0.01) {
			doNotice ("alert","Security\tUnauthorized dispensing started: %.3fgal. Last memb ID:%d, State:%s",
				tank_gals_at_start,shmem->memb_id,shmem->status);
			fflush (srv_fp);
			shmem->is_violation = 2;
		}
	}


	last_gallons = gallons;	
}


int checkOperator (int force) {

	if ( strlen (shmem->operator_code) < MIN_OP_SPN_LENGTH ) return 0;
	// force forces checking regardless of state
	if (!force && !(state_handlers[shmem->status_idx].can_enter_operator) ) return 0;

	/* Encrypt the user's password,
	  passing the expected password in as the salt. */
	if (! strcmp (crypt(shmem->keypad_buffer, shmem->operator_code), shmem->operator_code) ) {
		logMessage ("Operator SPN accepted");
		return (1);
	}
	
	return 0;
}

void encrypt_SPN (char *crypt_str, char *plain_str) {
int dev_rand_fp;
int i;
unsigned long seed[2];
char salt[] = "$1$........";
const char *const seedchars =
 "./0123456789ABCDEFGHIJKLMNOPQRST"
 "UVWXYZabcdefghijklmnopqrstuvwxyz";
	
	// Get two longs worth of noise
	dev_rand_fp = open ("/dev/urandom", O_RDONLY);
	if (dev_rand_fp > -1 ) {
		read (dev_rand_fp, seed, 8);
		close (dev_rand_fp);
	} else {
		seed[0] = time(NULL);
		seed[1] = getpid() ^ (seed[0] >> 14 & 0x30000);
	}
	
	/* Turn it into printable characters from `seedchars'. */
	for (i = 0; i < 8; i++)
		salt[3+i] = seedchars[(seed[i/5] >> (i%5)*6) & 0x3f];

	strcpy (crypt_str,crypt(plain_str, salt));	
}

int doReset () {
int new_state;

	clearMSR();
	memset(shmem->GW_string, 0, sizeof (shmem->GW_string));
	memset (shmem->memb_spn,0,sizeof (shmem->memb_spn));
	memset (&(shmem->cc_resp),0,sizeof (shmem->cc_resp));
	memset (shmem->keypad_buffer,0,sizeof (shmem->keypad_buffer));
	memset (shmem->memb_type,0,sizeof (shmem->memb_type));
	memset (shmem->memb_status,0,sizeof (shmem->memb_status));
	shmem->memb_fuel_pre_auth = 0;
	shmem->memb_pre_auth = 0.0;
	shmem->memb_renewal_sale = shmem->memb_full_membership_sale = 
		shmem->memb_temp_membership_sale = shmem->memb_upgrade_sale = 0;
	shmem->flowmeter_counts = 0;
	shmem->memb_credit = shmem->memb_dollars = shmem->memb_ppg = shmem->memb_gallons = 0.0;
	shmem->renewal_fee = shmem->full_membership_fee = 
		shmem->temp_membership_fee = shmem->trial_membership_surcharge = shmem->upgrade_fee = 0.0;

	// Reset calibration settings
	cal_cfg_from_mem (cal_cfg,shmem);

	fprintf (out_fp,"Flow\t0\n");
	fprintf (out_fp,"PumpRly\t0\n");
	fprintf (out_fp,"StrikeRly\t0\n");
	fprintf (out_fp,"PMP\nVIN\n");
	fflush (out_fp);
	doTimeout (shmem->lcd_timeout,"LCD");

	// This is called before going into an idle state.
	// We need to make sure we get into the right state depending on conditions

	if (shmem->motion && (shmem->door_open && shmem->has_DRSN)) {
		new_state = WaitDoorClosed_State;
	} else if (shmem->server) {
		if (shmem->avail_gallons > shmem->no_fuel_cutoff) {
			new_state = Idle_State;
		} else if (shmem->avail_gallons <= shmem->no_fuel_cutoff) {
			new_state = NoFuel_State;
		}
	} else {
		new_state = NoServer_State;
	}

	return (new_state);

}


void doTimeout (int timeout, char *evt) {

	fprintf (tmr_fp,"SET\t%s\t%d\n",evt,timeout);
	fflush (tmr_fp);
}

void cancelTimeout (char *evt) {


	fprintf (tmr_fp,"DEL\t%s\n",evt);
	fflush (tmr_fp);
}

void cancelTimeouts () {

	fprintf (tmr_fp,
		"DEL\tServer\n"
		"DEL\tStatus\n"
		"DEL\tStrikeRly\n"
		"DEL\tDispensing\n"
		"DEL\tInput\n"
		"DEL\tLCD\n"
		"DEL\tLCD-Refresh\n"
		"DEL\tCCReswipe\n"
		"DEL\tDoorClose\n"
		"DEL\tOperator\n"
		"DEL\tGW\n"
		"DEL\tGW Retry\n"
		"DEL\tSmartIO\n"
		"DEL\tServerResp\n"
		"DEL\tTest\n"
	);
	fflush (tmr_fp);
}


void doNotice (const char *type, const char *template, ...) {
va_list ap;

	fprintf (srv_fp,"notice\t%s\t%d\t",type,(int)time(NULL));
	// process the printf-style parameters
	va_start (ap, template);
	vfprintf (srv_fp,template, ap);
	va_end (ap);
	fprintf (srv_fp,"\n");
}

void setStatus (int state_idx) {
int new_state = state_idx;

	if (new_state == shmem->status_idx) return;
	// Switching states continues until the returned state is accepted by
	// the EnterState_Evt
	do {
		if (new_state != 0) {
		// The ExitState_Evt cannot return a new state
			state_handlers[shmem->status_idx].handler (ExitState_Evt,"");
		}

		shmem->status_idx = new_state;
		strcpy (shmem->status,state_handlers[new_state].name);
		new_state = state_handlers[new_state].handler (EnterState_Evt,"");
	} while (new_state != shmem->status_idx);
}

int isStatus (char state_idx) {
	if (shmem->status_idx == state_idx) return 1;
	return 0;
}

int getEvt (const char *evt) {
evt_def *result,target;

	strcpy (target.name,evt);
	result = bsearch (&target, evt_defs, sizeof (evt_defs) / sizeof (evt_def),
		sizeof (evt_def), event_cmp);
	if (result) return (result->ID);
	return (-1);
}

void checkLights() {
time_t t_now = time (NULL);

	// Its dark if its later than shmem->twilight_end
	// or earlier than shmem->twilight_start
	if ( t_now > shmem->twilight_end || t_now < shmem->twilight_start ) {
	// Its dark
		if (! shmem->LightsRly) {
			fprintf (out_fp,"LightsRly\t1\n");
			fflush (out_fp);
		}
	} else {
	// Its light
		if (shmem->LightsRly) {
			fprintf (out_fp,"LightsRly\t0\n");
			fflush (out_fp);
		}
	}
}

// This function makes sure the LCD is not updated too rapidly.
// If the LCD was updated too recently, it stores the message and sets an "LCD-Refresh" timeout.
// The global handler calls this function with a NULL message on a LCDRefreshTimeout_Evt.
// If the LCD changes, a new LCD-Refresh timeout is set in case it needs to change again.
void doLCD (int line, int force, const char *message, ...) {
va_list ap;
static struct timeval last_update[2];
static char LCD[2][LCD_MAX_LINE_SIZE];
char parsed_msg[LCD_MAX_LINE_SIZE];
unsigned long msg_size;
static int first=1;
struct timeval t_now;
char can_refresh = 0;
unsigned long since_last[2],last_min;
unsigned long lcd_refresh_timeout;
int idx = line-1;

	if (message) {
		va_start (ap, message);
		vsnprintf (parsed_msg, sizeof(parsed_msg), message, ap);
		va_end (ap);
		msg_size = strlen (parsed_msg);
		if (msg_size > LCD_LINE_SIZE) {
			if (*(parsed_msg+msg_size-1) == ' ')
				strcat (parsed_msg,"| "); // add a break for scrolling
			else 
				strcat (parsed_msg," | "); // add a break for scrolling
		}
	}

	gettimeofday(&t_now, NULL);
	lcd_refresh_timeout = shmem->lcd_refresh_timeout;
	if (first) {
		last_update[0] = t_now;
		last_update[1] = t_now;
		memset (LCD[0],0,sizeof(LCD[0]));
		memset (LCD[1],0,sizeof(LCD[1]));
		memset (parsed_msg,0,sizeof(parsed_msg));
		first = 0;
	}

	// Store the message if its not NULL
	if (message && line > 0 && line <= 2) {
		since_last[idx] = TV_ELAPSED_MS (t_now,last_update[idx]);
		if ( force || since_last[idx] >= lcd_refresh_timeout ) can_refresh = 1;
		strcpy (LCD[idx],parsed_msg);
	} else if (!message && !force) {
	// refresh the LCD with any stored message (message = NULL, force = 0)
		if ( ! (line > 0 && line <= 2) ) {
			since_last[0] = TV_ELAPSED_MS (t_now,last_update[0]);
			since_last[1] = TV_ELAPSED_MS (t_now,last_update[1]);
			last_min = (since_last[0] < since_last[1] ? since_last[0] : since_last[1]);
			since_last[0] = last_min;
			since_last[1] = last_min;
			if ( last_min >= lcd_refresh_timeout ) can_refresh = 1;
		} else {
			since_last[idx] = TV_ELAPSED_MS (t_now,last_update[idx]);
			if (force || since_last[idx] >= lcd_refresh_timeout ) can_refresh = 1;
		}
	// release the LCD, so that the next message will be displayed - forced or not.
	//  (message = NULL, force = 1)
	} else if (!message && force) {
		if ( ! (line > 0 && line <= 2) ) {
			last_update[0] = t_now;
			TV_ADD_MS (last_update[0],-(lcd_refresh_timeout+1));
			last_update[1] = t_now;
			TV_ADD_MS (last_update[1],-(lcd_refresh_timeout+1));
		} else {
			last_update[idx] = t_now;
			TV_ADD_MS (last_update[idx],-(lcd_refresh_timeout+1));
		}
	} else if (message && ! (line > 0 && line <= 2) ) {
		return; // error
	}

	if (message && can_refresh) {
	// If the LCD changes, write the passed-in message, store the time, and reissue an LCD-Refresh timeout
//logMessage ("Sending %d: [%s]",idx+1,LCD[idx]);
		if (line == 1 && strcmp (shmem->LCD1,LCD[0])) {
			fprintf (out_fp,"LCD1\t%s\n",LCD[0]);
			fflush (out_fp);
			doTimeout (lcd_refresh_timeout,"LCD-Refresh");
			last_update[0] = t_now;
		} else if (line == 2 && strcmp (shmem->LCD2,LCD[1])) {
//logMessage ("Sending %d: [%s], shmem [%s]",line,LCD[line-1], shmem->LCD2);
			fprintf (out_fp,"LCD2\t%s\n",LCD[1]);
			fflush (out_fp);
			doTimeout (lcd_refresh_timeout,"LCD-Refresh");
			last_update[1] = t_now;
		}
	} else if (message && !can_refresh) {
//logMessage ("Delaying %d: [%s]",idx+1,LCD[idx]);
	// Set a LCD-Refresh timeout
		doTimeout (lcd_refresh_timeout - since_last[idx],"LCD-Refresh");
	} else if (!message && can_refresh) {
	// If the LCD changes, write the stored messages, store the time, and reissue an LCD-Refresh timeout
		if (strcmp (shmem->LCD1,LCD[0]) || strcmp (shmem->LCD2,LCD[1])) {
//logMessage ("Sending %d: [%s]",1,LCD[0]);
//logMessage ("Sending %d: [%s]",2,LCD[1]);
			fprintf (out_fp,"LCD1\t%s\n",LCD[0]);
			fprintf (out_fp,"LCD2\t%s\n",LCD[1]);
			last_update[0] = t_now;
			last_update[1] = t_now;
			fflush (out_fp);
			doTimeout (lcd_refresh_timeout,"LCD-Refresh");
		} else {
	// clear the LCD-Refresh timeout
			cancelTimeout ("LCD-Refresh");
		}
	} else if (!message && !can_refresh) {
	// Reset the LCD-Refresh timeout
		doTimeout (lcd_refresh_timeout - last_min,"LCD-Refresh");
	}
}

int doRestore () {
char date_str[STR_SIZE];

	// Wait here until we're synchronized with SmartIO
	while (!shmem->SmartIOsync) {
		usleep (10000);
	}
	logMessage ("SmartIO synchronized");


	doLCD (2, 0, "Checking network...");
	// Wait here until we're synchronized with the server
	// The server will either connect and reset the clock,
	// Or exhaust its timeouts and set server state
	while (!shmem->serverSync) {
		usleep (10000);
	}
	logMessage ("Server synchronized");
	// Report the network status on the LCD
	doLCD (2, 1, "Net: %s",shmem->net_status);

	// Read our run-time save state if it exists
	run_cfg_read(shmem);
	if (shmem->boot_time)
		strftime( date_str, sizeof(date_str), "%F %T", localtime(&(shmem->boot_time)) );

	// Avoid filling up the server queue if its already full.
	if (shmem->server_msg_size < (RSA_TEXT_SIZE*4) ) {
		// Patch notification
		if (!strcmp (shmem->boot_reason,"patch")) {
			if (*shmem->patch_status) {
				doNotice ("alert","Software\tRestart after patch on %s: %s. Last FSM state: '%s'.",
					date_str,shmem->patch_status, shmem->boot_state );
			} else {
				doNotice ("alert","Software\tRestart after patch on %s: No patch status recorded!. Last FSM state: '%s'.",
					date_str, shmem->boot_state );
			}
		// Some other reason - generic message
		} else if (*(shmem->boot_reason)) {
			doNotice ("alert","Software\tRestart after '%s'. Shutdown on %s. Last FSM state: '%s'.",
				shmem->boot_reason, date_str, shmem->boot_state );
		// First time?  Aborted shutdown?
		} else if (!*(shmem->boot_reason) && shmem->boot_time > 0) {
			doNotice ("alert","Software\tRestart without saved reason on %s. Last FSM state: '%s'.",
				date_str, shmem->boot_state );
		} else if ( !*(shmem->boot_reason) ) {
			doNotice ("alert","Software\tRestart without saved state.");
		}
		
		if ( *(shmem->cc_resp.trans_id) ) {
			doNotice ("alert","Gateway\tRestarted with stale %s trans ID:%s (auth: %s). %s"
				"Error: %s. Pre-auth: $%.2f, Amount: $%.2f. "
				"CC Name: %s (Mmbshp. #:%s). GW message: %s.",
				(shmem->cc_resp.doVoid ? "void" : "capture"),
				shmem->cc_resp.trans_id,shmem->cc_resp.auth_code,
				getSalesList(),
				shmem->cc_resp.error,shmem->memb_pre_auth,shmem->memb_dollars,
				shmem->msr_name,shmem->memb_number,shmem->cc_resp.message
			);
		}
		// We're starting in the no server state, which won't reset the transaction until
		// the server says we're OK.
	
		fflush (srv_fp);
	}

	// Delete the runtime configuration
	unlink (shmem->run_conf);
	memset (shmem->patch_status,0,sizeof (shmem->patch_status));
	memset (shmem->boot_state,0,sizeof (shmem->boot_state));
	memset (shmem->boot_reason,0,sizeof (shmem->boot_reason));
	strcpy (shmem->boot_reason,"restart");

	doLCD (2, 0, "State restored");
	fflush (out_fp);
	if (shmem->server)
		setStatus(Idle_State);
	else
		setStatus(NoServer_State);

	return (1);

}

void logMessage (const char *template, ...) {
va_list ap;
time_t t_now;
char buf[STR_SIZE];


	t_now = time(NULL);
	strftime (buf, STR_SIZE, "%Y-%m-%d %H:%M:%S", localtime (&t_now));
	fprintf (log_fp,"fsm [%s] %s: ",shmem->status,buf);

	// process the printf-style parameters
	va_start (ap, template);
	vfprintf (log_fp, template, ap);
	va_end (ap);
	fprintf (log_fp,"\n");
	fflush (log_fp);
}


float getMaxFlowGPM () {
float flow_mult;

	// The maximum number of counts per ms is recorded by SmartIO.c
	// Here, we convert this to Gallons per minute.
	
	// get max counts per minute
	flow_mult = 60000.0 * shmem->max_counts_per_ms;
	// get max gallons per minute
	flow_mult /= shmem->flowmeter_pulses_per_gallon;

	return (flow_mult);
}


int parseMSR (const char *message) {
char *chp1, the_ch, *name_s, *CCname_s;
char is_cap=1, is_space=0, had_comma=0;

	
	sscanf (message,"%[^\t]\t%[^\n]",shmem->msr_track1,shmem->msr_track2);
	if ( ! (*shmem->msr_track1 && *shmem->msr_track2) ) {
		clearMSR ();
		return (MSR_INVALID);
	}

	// parse the name out of track 1
	chp1 = strchr (shmem->msr_track1,'^');
	if (chp1) {
		// Copy the last 4 digits of the account
		strncpy (shmem->msr_last4,chp1-4,4);
		shmem->msr_last4[4] = '\0';

		// Start parsing the name field
		chp1++;
		name_s = shmem->msr_name;
		CCname_s = shmem->msr_CCname;
		is_cap=1;
		is_space=0;
		while (*chp1 && *chp1 != '^') {

			the_ch = *chp1++;
			*CCname_s++ = the_ch;

			if (!isalpha (the_ch)) is_cap = 1;
			if (isalpha (the_ch) && !is_cap) the_ch = tolower (the_ch);
			if (isalpha (the_ch) && is_cap) is_cap = 0;

			if (the_ch == '/') {
				if (!had_comma) *name_s++ = ',';
				had_comma=1;
				the_ch = ' ';
			}
			if (the_ch != ' ') {
				*name_s++ = the_ch;
				is_space = 0;
			} else if (the_ch == ' ' && !is_space) {
				is_space = 1;
				*name_s++ = the_ch;
			}
		}
		*name_s = '\0';
		*CCname_s = '\0';
		if (*(name_s-1) == ' ') *(name_s-1) = '\0';
	}

	logMessage ("MSR Name: [%s] CC-Name: [%s]\n",shmem->msr_name,shmem->msr_CCname);


	// Get the date from track2
	chp1 = strchr (shmem->msr_track2,'=');
	if (chp1) {
		chp1++;
		shmem->msr_exp = MSRDateToEpoch (chp1);
		if (time(NULL) > shmem->msr_exp) {
			clearMSR ();
			return (MSR_EXPIRED);
		}
	} else {
		clearMSR ();
		return (MSR_INVALID);
	}

	return (MSR_VALID);
	
}

void clearMSR () {
	memset (shmem->msr_track1,0,sizeof (shmem->msr_track1));
	memset (shmem->msr_track2,0,sizeof (shmem->msr_track2));
	shmem->msr_exp = 0;
	memset (shmem->msr_name,0,sizeof (shmem->msr_name));
	memset (shmem->msr_CCname,0,sizeof (shmem->msr_CCname));
	memset (shmem->msr_last4,0,sizeof(shmem->msr_last4));
}


void doGW (const char *type, float amount) {
FILE *fp;

	if (shmem->server_GW) {
		if (!strcmp(type,"PreAuth"))
			fprintf (srv_fp,"GW\t%s\t%d\t%.2f\t%s\t%s\n",type,shmem->memb_id,amount,shmem->msr_track1,shmem->msr_track2);
		else if (!strcmp(type,"Capture"))
			fprintf (srv_fp,"GW\t%s\t%d\t%.2f\t%s\n",type,shmem->memb_id,amount,shmem->cc_resp.trans_id);
		else if (!strcmp(type,"AuthCapture"))
			fprintf (srv_fp,"GW\t%s\t%d\t%.2f\t%s\t%s\n",type,shmem->memb_id,amount,shmem->msr_track1,shmem->msr_track2);
		else if (!strcmp(type,"Void"))
			fprintf (srv_fp,"GW\t%s\t%d\t%s\n",type,shmem->memb_id,shmem->cc_resp.trans_id);
		fflush (srv_fp);
		// Don't clear msr yet - we have retries to do.
	} else {
		fp = fopen (shmem->BBD9000ccg,"w");
		assert (fp != NULL);
		fprintf (fp,"%s\t%.2f\n",type,amount);
		fflush (fp);
		fclose (fp);
	}
}

// Type is "CAPTURED", "VOIDED", "PREAUTH", "ERROR"
// FSM does GW retries, sending an "ERROR" type when giving up
//   If the server gets a GW response, it will record the necessary info, so it doesn't need it resent.
//      we need to be careful with the server's duplicate message detector - should be OK b/c they are volatile.
//   If the kiosk gives up on the gateway, the server needs to be notified of that
//   With server_GW, Only send cc messages of type "ERROR"
// Server and local GW respond with 'GW <msg>'.
// Served adds GW info to its response, parsed out by the kiosk's server program and put into shmem->cc_resp[

void doSrvCC (const char *type, float amount, char flush) {
	if (shmem->server_GW) {
		if ( ! strcmp (type,"ERROR") ) {
			doNotice ("cc","%d\t%.2f\t%.2f\t%d\t%d\t%s\t%s\t%s\t%s\t%s",
				shmem->memb_id,amount,shmem->memb_pre_auth,
				shmem->cc_resp.code,shmem->cc_resp.reas_code,
				shmem->cc_resp.auth_code,shmem->cc_resp.trans_id,shmem->msr_last4,
				shmem->cc_resp.message,"ERROR");
			if (flush) fflush (srv_fp);
		}
	} else {
		doNotice ("cc","%d\t%.2f\t%.2f\t%d\t%d\t%s\t%s\t%s\t%s\t%s",
			shmem->memb_id,amount,shmem->memb_pre_auth,
			shmem->cc_resp.code,shmem->cc_resp.reas_code,
			shmem->cc_resp.auth_code,shmem->cc_resp.trans_id,shmem->msr_last4,
			shmem->cc_resp.message,type);
		if (flush) fflush (srv_fp);
	}
}


void doPatch () {
char cmd[BIG_BUFFER_SIZE];
int i=0;
long nbytes=0,size=0;

	if (! shmem->npatches ) return;
	nbytes = snprintf (cmd+size,sizeof(cmd)-size,
		"%s %s",shmem->BBD9000patch,shmem->patch_server);
	if (nbytes < sizeof(cmd)-size) size += nbytes;
	else size = sizeof(cmd)-1;

	for (i=0; i < MAX_PATCHES; i++) {
		nbytes = snprintf (cmd+size,sizeof(cmd)-size,
			" %s %s %s",shmem->patches[i].path,shmem->patches[i].srcMD5,shmem->patches[i].dstMD5);
		if (nbytes < sizeof(cmd)-size) size += nbytes;
		else {
			size = sizeof(cmd)-1;
		}
		memset (&(shmem->patches[i]),0,sizeof(patch));
	}
	logMessage ("cmd: %s",cmd);

// Set our boot reason to patch (will be saved in sigTermHandler)
	strcpy (shmem->boot_reason,"patch");


	// Note that no patches are applied if we had a buffer overrun.
	if (size < sizeof(cmd)-1) {
	// This program will receive a SIGTERM from /etc/init.d/BBD9000 stop regardless of outcome
	// After this final patch cleanup is done, the computer is rebooted.
	// The only reason system (cmd) would return here is if there was an error starting it up.
		if ( system (cmd) != 0 ) {
		// Error in applying the patches
		// Oh, well - BBD9000patch notifies the server
			doNotice ("alert","Software\tPatch command failed");
			strcpy (shmem->boot_reason,"restart");
			logMessage ("Patch command failed");
		} else {
		// Patching worked - need to shutdown
		// We should never return here if patching worked because we will have received a SIGTERM
			doNotice ("alert","Software\tPatch command returned to fsm");
			strcpy (shmem->boot_reason,"restart");
			logMessage ("Patch command returned to fsm");
		}
	}
}



time_t MSRDateToEpoch(char *date) {
	struct tm tm_exp;
	time_t t_exp;
	int month, year;
	memset (&tm_exp,0,sizeof(struct tm));
	

	sscanf (date,"%2d%2d",&year,&month);
	if (year < 70) {
		// Assuming post 2000, tm wants years after 1900
		year += 100;
	}
	/*
	  Cards expire on the last day of the month, so
	  they are valid until midnight of the 0th day of
	  the following month.
	  Months on CCs are 1-based, while in tm are 0-based
	  So, if the month is 12, we set it to 0, otherwise
	  the CC month is 1 more than the tm month.
	*/
	if (month == 12) month = 0;
	if (month > 11 ) return (0);

	tm_exp.tm_year = year;
	tm_exp.tm_mon = month;
	tm_exp.tm_mday = 1; // days are of course 1-based (sigh)
	// get the time_t value for the local expiration day
	t_exp = mktime (&tm_exp);
	return (t_exp);
}

void sigPipeHandler (int signum) {
	sigPipeCounter++;
	logMessage ("SIGPIPE #%d, reopening",sigPipeCounter);

	/* open the output FIFO */
	fclose (out_fp);
	out_fp = fopen (shmem->BBD9000out,"w");
	assert(out_fp != NULL);
	
	/* Open the server FIFO */
	fclose (srv_fp);
	srv_fp = fopen (shmem->BBD9000srv, "w");
	assert(srv_fp != NULL);

	/* Open the timer FIFO */
	fclose (tmr_fp);
	tmr_fp = fopen (shmem->BBD9000tim, "w");
	assert (tmr_fp != NULL);
}


void sigTermHandler (int signum) {


	fprintf (out_fp,"LightsRly\t0\nStrikeRly\t0\nPumpRly\t0\nAuxRly\t0\n");
	fflush (out_fp);
	
	if (signum == SIGPIPE) strcpy (shmem->boot_reason,"FSM SIGPIPE");

// Save our state
	time(&(shmem->boot_time));
	run_cfg_write (shmem);

// Cancel all timeout events
	cancelTimeouts();

// Log the error
	doLCD (2, 1, "Reboot: %s",shmem->boot_reason);
	logMessage ("Reboot: %s",shmem->boot_reason);

// BBD9000timer.c will reboot if we exit, unless it gets killed first.
	exit(EXIT_SUCCESS);
}

char *getSalesList () {
static char sale_str[STR_SIZE];
int sale_str_size=0;

	*sale_str = '\0';
	if (FABS(shmem->memb_credit) > 0.005)
		sale_str_size += sprintf (sale_str+sale_str_size,"Credit: $%.2f",shmem->memb_credit);
	if (shmem->memb_gallons > 0.001)
		sale_str_size += sprintf (sale_str+sale_str_size,"%sFuel (%s): %.3f gal @ $%.2f = $%.2f",
			sale_str_size ? ", " : "",
			shmem->fuel_type,
			shmem->memb_gallons, shmem->memb_ppg, (shmem->memb_gallons*shmem->memb_ppg));
	if (shmem->memb_renewal_sale)
		sale_str_size += sprintf (sale_str+sale_str_size,"%sRenewal: $%.2f",
			sale_str_size ? ", " : "", shmem->renewal_fee);
	if (shmem->memb_full_membership_sale)
		sale_str_size += sprintf (sale_str+sale_str_size,"%sFull membership: $%.2f",
			sale_str_size ? ", " : "", shmem->full_membership_fee);
	if (shmem->memb_temp_membership_sale)
		sale_str_size += sprintf (sale_str+sale_str_size,"%sTemp membership: $%.2f",
			sale_str_size ? ", " : "", shmem->temp_membership_fee);
	if (shmem->memb_upgrade_sale)
		sale_str_size += sprintf (sale_str+sale_str_size,"%sMembership upgrade: $%.2f",
			sale_str_size ? ", " : "", shmem->upgrade_fee);

	if (sale_str_size) sprintf (sale_str+sale_str_size,". ");
	return (&(sale_str[0]));
}

char isPPPup () {
FILE *fp;
int oct1,oct2,oct3,oct4, nscanned, ipup=0;
char ip[256];

	fp = popen("ifconfig ppp0 2>/dev/null | grep 'inet addr' | cut -d ':' -f 2 | cut -d ' ' -f 1", "r");
	if (fp == NULL) return 0;
	while (fgets(ip, 256, fp) != NULL) {
		nscanned = sscanf (ip,"%d.%d.%d.%d",&oct1,&oct2,&oct3,&oct4);
		if (nscanned == 4) ipup = 1;
	}
	pclose(fp);
	
	if (ipup) return ('U');
	else return ('D');
}

int event_cmp (const void *e1, const void *e2) {
	return strcmp (((evt_def *)(e1))->name, ((evt_def *)(e2))->name);
}


