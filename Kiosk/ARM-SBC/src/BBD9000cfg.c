#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/file.h> 
#include <stdlib.h>


#include "BBD9000cfg.h"

cfg_t *conf_cfg_init (BBD9000mem *shmem) {
cfg_t *cfg;

	// Declaration of options in the config file
	static cfg_opt_t opts[] = {
	/* Coop Name */
		CFG_STR("coop-name",      "Baltimore Biodiesel", CFGF_NONE),
		CFG_INT("kiosk_id",       6, CFGF_NONE),
		CFG_STR("kiosk_name",     "Test Kiosk", CFGF_NONE),
	/* File Paths */
		CFG_STR("BBD9000LOG",      "BBD9000.log", CFGF_NONE),
		CFG_STR("BBD9000-run",     "BBD9000-run.conf", CFGF_NONE),
		CFG_STR("BBD9000-cal",     "BBD9000-cal.conf", CFGF_NONE),
		CFG_STR("BBD9000key",      "BBD9000.pem", CFGF_NONE),
		CFG_STR("BBD9000srv_key",  "BBD-pub.pem", CFGF_NONE),
		CFG_STR("BBD9000ccGWconf", "AuthDotNet.conf", CFGF_NONE),
		CFG_STR("BBD9000patch",    "BBD9000patch", CFGF_NONE),
		CFG_STR("BBD9000run",      "/var/run/BBD9000", CFGF_NONE),
		
	/* SmartIO device and baud */
		CFG_STR("SmartIOdev", "/dev/ttyAM1", CFGF_NONE),
		CFG_INT("SmartIObaud", 115200, CFGF_NONE),
        CFG_BOOL("has-DRSN", cfg_true, CFGF_NONE),

	/* modem device */
		CFG_STR("Modemdev", "/dev/tts/0", CFGF_NONE),

		CFG_INT("smartIO_interval", 500, CFGF_NONE),

	// Lat + lon are for 921 S. clinton St., Baltimore, MD:
	// 39.28 (39°16') |  -76.57 (-76°34')
		CFG_FLOAT("location_lat", 39.28, CFGF_NONE),
		CFG_FLOAT("location_lon", -76.57, CFGF_NONE),

		CFG_STR("server_URL", "http://baltimorebiodiesel.org/", CFGF_NONE),
		CFG_STR("patch_server", "http://baltimorebiodiesel.org/Patches/", CFGF_NONE),

		CFG_FLOAT("cc_threshold", 1.00, CFGF_NONE),

		CFG_INT("network_timeout", 30, CFGF_NONE),

	// Various timeouts
		CFG_INT("status_interval_net", 43200000, CFGF_NONE),
		CFG_INT("status_interval_no_net", 30000, CFGF_NONE),
		CFG_INT("max_status_interval_no_net", 43200000, CFGF_NONE),
		CFG_INT("dispensing_timeout", 120000, CFGF_NONE),
		CFG_INT("input_timeout", 30000, CFGF_NONE),
		CFG_INT("lcd_timeout", 3000, CFGF_NONE),
		CFG_INT("lcd_refresh_timeout", 30000, CFGF_NONE),
		CFG_INT("strike_rly_timeout", 10000, CFGF_NONE),
		CFG_INT("CC_reswipe_timeout", 60000, CFGF_NONE),
		CFG_INT("door_close_timeout", 45000, CFGF_NONE),
		CFG_INT("netIdle_timeout", 300000, CFGF_NONE),
		
		CFG_END()
    };

    cfg = cfg_init(opts, 0);
    return (cfg);
}

int conf_cfg_read (BBD9000mem *shmem) {
cfg_t *cfg;
char tmpPath[PATH_SIZE],BBD9000root[PATH_SIZE];

	// Initialize defaults
	cfg = conf_cfg_init(shmem);

	strncpy (BBD9000root,shmem->root_path,sizeof(BBD9000root));

	// Get default overrides from the configuration file
	if ( cfg_parse(cfg, shmem->BBD9000conf) != CFG_SUCCESS ) {
		return (-1);
	}

	// Copy stuff from the configuration
	strncpy (shmem->coop_name, cfg_getstr (cfg, "coop-name"), NAME_SIZE);

	shmem->kiosk_id = cfg_getint (cfg,"kiosk_id");
	strncpy (shmem->kiosk_name, cfg_getstr (cfg, "kiosk_name"), NAME_SIZE);

	strncpy (tmpPath,cfg_getstr (cfg, "BBD9000LOG"),sizeof (tmpPath));
	if (*tmpPath == '/')
		strncpy (shmem->BBD9000LOG,tmpPath,sizeof (shmem->BBD9000LOG) );
	else
		snprintf (shmem->BBD9000LOG,sizeof (shmem->BBD9000LOG),"%s/%s",BBD9000root,tmpPath);

	strncpy (tmpPath,cfg_getstr (cfg, "BBD9000-run"),sizeof (tmpPath));
	if (*tmpPath == '/')
		strncpy (shmem->run_conf,tmpPath,sizeof (shmem->run_conf));
	else
		snprintf (shmem->run_conf,sizeof (shmem->run_conf),"%s/%s",BBD9000root,tmpPath);

	strncpy (tmpPath,cfg_getstr (cfg, "BBD9000-cal"),sizeof (tmpPath));
	if (*tmpPath == '/')
		strncpy (shmem->cal_conf,tmpPath,sizeof (shmem->cal_conf));
	else
		snprintf (shmem->cal_conf,sizeof (shmem->cal_conf),"%s/%s",BBD9000root,tmpPath);
	
	strncpy (tmpPath,cfg_getstr (cfg, "BBD9000key"),sizeof (tmpPath));
	if (*tmpPath == '/')
		strncpy (shmem->BBD9000key,tmpPath,sizeof (shmem->BBD9000key));
	else
		snprintf (shmem->BBD9000key,sizeof (shmem->BBD9000key),"%s/%s",BBD9000root,tmpPath);
	
	
	strncpy (tmpPath,cfg_getstr (cfg, "BBD9000srv_key"),sizeof (tmpPath));
	if (*tmpPath == '/')
		strncpy (shmem->BBD9000srv_key,tmpPath,sizeof (shmem->BBD9000srv_key));
	else
		snprintf (shmem->BBD9000srv_key,sizeof (shmem->BBD9000srv_key),"%s/%s",BBD9000root,tmpPath);
	
	strncpy (tmpPath,cfg_getstr (cfg, "BBD9000ccGWconf"),sizeof (tmpPath));
	if (*tmpPath == '/')
		strncpy (shmem->BBD9000ccGWconf,tmpPath,sizeof (shmem->BBD9000ccGWconf));
	else
		snprintf (shmem->BBD9000ccGWconf,sizeof (shmem->BBD9000ccGWconf),"%s/%s",BBD9000root,tmpPath);
	
	strncpy (tmpPath,cfg_getstr (cfg, "BBD9000patch"),sizeof (tmpPath));
	if (*tmpPath == '/')
		strncpy (shmem->BBD9000patch,tmpPath,sizeof (shmem->BBD9000patch));
	else
		snprintf (shmem->BBD9000patch,sizeof (shmem->BBD9000patch),"%s/%s",BBD9000root,tmpPath);

	/* root directory in a shared-memory filesystem (i.e. /var/run/BBD9000) */
	strncpy (shmem->BBD9000run,cfg_getstr (cfg, "BBD9000run"),sizeof (shmem->BBD9000run));
	// The rest of these paths are dependent on the root
	snprintf (shmem->BBD9000mem, sizeof (shmem->BBD9000mem), "%s/%s", shmem->BBD9000run, "BBD9000-mem");
	snprintf (shmem->BBD9000out, sizeof (shmem->BBD9000out), "%s/%s", shmem->BBD9000run, "BBD9000-out");
	snprintf (shmem->BBD9000evt, sizeof (shmem->BBD9000evt), "%s/%s", shmem->BBD9000run, "BBD9000-evt");
	snprintf (shmem->BBD9000tim, sizeof (shmem->BBD9000tim), "%s/%s", shmem->BBD9000run, "BBD9000-tim");
	snprintf (shmem->BBD9000srv, sizeof (shmem->BBD9000srv), "%s/%s", shmem->BBD9000run, "BBD9000-srv");
	snprintf (shmem->BBD9000ccg, sizeof (shmem->BBD9000ccg), "%s/%s", shmem->BBD9000run, "BBD9000-ccg");
	snprintf (shmem->BBD9000net, sizeof (shmem->BBD9000net), "%s/%s", shmem->BBD9000run, "BBD9000-net");	

	/* SmartIO device and baud */
	strncpy (shmem->SmartIOdev,cfg_getstr (cfg, "SmartIOdev"),sizeof (shmem->SmartIOdev));
	shmem->SmartIObaud = cfg_getint (cfg,"SmartIObaud");
	shmem->has_DRSN = cfg_getbool(cfg, "has-DRSN") ? 1 : 0;


	/* modem device - might be blank */
	strncpy (shmem->Modemdev,cfg_getstr (cfg, "Modemdev"),sizeof (shmem->Modemdev));

	shmem->smartIO_interval = cfg_getint (cfg,"smartIO_interval");

	shmem->lat = cfg_getfloat (cfg,"location_lat");
	shmem->lon = cfg_getfloat (cfg,"location_lon");

	strncpy (shmem->server_URL,cfg_getstr (cfg, "server_URL"), sizeof (shmem->server_URL) );
	strncpy (shmem->patch_server,cfg_getstr (cfg, "patch_server"), sizeof (shmem->patch_server) );

	shmem->cc_threshold = cfg_getfloat (cfg,"cc_threshold");

	shmem->network_timeout = cfg_getint (cfg,"network_timeout");
	shmem->status_interval_net = cfg_getint (cfg,"status_interval_net");
	shmem->status_interval_no_net = cfg_getint (cfg,"status_interval_no_net");
	shmem->max_status_interval_no_net = cfg_getint (cfg,"max_status_interval_no_net");
	shmem->dispensing_timeout = cfg_getint (cfg,"dispensing_timeout");
	shmem->input_timeout = cfg_getint (cfg,"input_timeout");
	shmem->lcd_timeout = cfg_getint (cfg,"lcd_timeout");
	shmem->lcd_refresh_timeout = cfg_getint (cfg,"lcd_refresh_timeout");
	shmem->strike_rly_timeout = cfg_getint (cfg,"strike_rly_timeout");
	shmem->CC_reswipe_timeout = cfg_getint (cfg,"CC_reswipe_timeout");
	shmem->door_close_timeout = cfg_getint (cfg,"door_close_timeout");
	shmem->netIdle_timeout = cfg_getint (cfg,"netIdle_timeout");


	// We're assuming that the network is off
	shmem->status_interval = shmem->status_interval_no_net;
	shmem->server = 0;

	/*
	  End of BBD9000mem initialization from BBD9000.conf
	*/
	cfg_free(cfg);
	return (0);

}



cfg_t *cal_cfg_init (BBD9000mem *shmem) {
cfg_t *cfg;

	// Declaration of options in the config file
	static cfg_opt_t opts[] = {
	// Calibrated counts per gallon (meter's K-Factor)
		CFG_FLOAT("flowmeter_pulses_per_gallon", 196.841, CFGF_NONE),
	// Current above which the "Pump ON" event is triggered
		CFG_FLOAT("pump_on_threshold", 1.5, CFGF_NONE),
	// Current below which the "Pump OFF" event is triggered
		CFG_FLOAT("pump_off_threshold", 0.6, CFGF_NONE),
	// Voltage below which the "Voltage Alarm" event is triggered
		CFG_FLOAT("valrm_on_threshold", 10.5, CFGF_NONE),
	// Voltage above which the "Voltage Alarm Reset" event is triggered
		CFG_FLOAT("valrm_off_threshold", 11.5, CFGF_NONE),
	// Capacity in gallons
		CFG_INT("tank_capacity", 500, CFGF_NONE),
	// Level of fuel at which to send an alert
		CFG_INT("low_fuel_alarm", 75, CFGF_NONE),
	// level of gallons at which to stop pumping fuel
		CFG_INT("no_fuel_cutoff", 5, CFGF_NONE),
	// Operator code
		CFG_STR("operator_code",    "", CFGF_NONE),
	// ADC0 calibration - voltage (low raw ADC value, low calibrated value, high raw ADC value, high calibrated value)
		CFG_INT   ("ADC0_RAW1", 100   , CFGF_NONE),
		CFG_FLOAT ("ADC0_CAL1",   0   , CFGF_NONE),
		CFG_INT   ("ADC0_RAW2", 782   , CFGF_NONE),
		CFG_FLOAT ("ADC0_CAL2",  14.82, CFGF_NONE),
	// ADC1 calibration - current (low raw ADC value, low calibrated value, high raw ADC value, high calibrated value)
		CFG_INT   ("ADC1_RAW1", 127   , CFGF_NONE),
		CFG_FLOAT ("ADC1_CAL1",   0   , CFGF_NONE),
		CFG_INT   ("ADC1_RAW2", 615   , CFGF_NONE),
		CFG_FLOAT ("ADC1_CAL2",  50.00, CFGF_NONE),
		
		CFG_END()
	};

    cfg = cfg_init(opts, 0);
    return (cfg);
}

int cal_cfg_read  (BBD9000mem *shmem) {
cfg_t *cfg;

	// Initialize defaults
	cfg = cal_cfg_init(shmem);

	// Get default overrides from the configuration file
	if ( cfg_parse(cfg, shmem->cal_conf) != CFG_SUCCESS ) {
		return (-1);
	}
	
	// Set shmem based on configuration defaults over-ridden by file
	cal_cfg_to_mem (cfg,shmem);

	cfg_free(cfg);
	return (0);

}

void cal_cfg_from_mem (cfg_t *cfg,BBD9000mem *shmem) {

	if (! cfg) return;

	cfg_setfloat(cfg, "flowmeter_pulses_per_gallon", shmem->flowmeter_pulses_per_gallon);
	cfg_setfloat(cfg, "pump_on_threshold", shmem->pump_on_threshold);
	cfg_setfloat(cfg, "pump_off_threshold", shmem->pump_off_threshold);
	cfg_setfloat(cfg, "valrm_on_threshold", shmem->valrm_on_threshold);
	cfg_setfloat(cfg, "valrm_off_threshold", shmem->valrm_off_threshold);
	cfg_setint(cfg, "tank_capacity", shmem->tank_capacity);
	cfg_setint(cfg, "low_fuel_alarm", shmem->low_fuel_alarm);
	cfg_setint(cfg, "no_fuel_cutoff", shmem->no_fuel_cutoff);
	cfg_setstr(cfg, "operator_code", shmem->operator_code);

	cfg_setint   (cfg, "ADC0_RAW1", shmem->ADC0_cal.raw1);
	cfg_setfloat (cfg, "ADC0_CAL1", shmem->ADC0_cal.cal1);
	cfg_setint   (cfg, "ADC0_RAW2", shmem->ADC0_cal.raw2);
	cfg_setfloat (cfg, "ADC0_CAL2", shmem->ADC0_cal.cal2);

	cfg_setint   (cfg, "ADC1_RAW1", shmem->ADC1_cal.raw1);
	cfg_setfloat (cfg, "ADC1_CAL1", shmem->ADC1_cal.cal1);
	cfg_setint   (cfg, "ADC1_RAW2", shmem->ADC1_cal.raw2);
	cfg_setfloat (cfg, "ADC1_CAL2", shmem->ADC1_cal.cal2);
}

void cal_cfg_to_mem (cfg_t *cfg,BBD9000mem *shmem) {

	if (! cfg) return;

	shmem->flowmeter_pulses_per_gallon = cfg_getfloat (cfg,"flowmeter_pulses_per_gallon");
	shmem->pump_on_threshold = cfg_getfloat (cfg,"pump_on_threshold");
	shmem->pump_off_threshold = cfg_getfloat (cfg,"pump_off_threshold");
	shmem->valrm_on_threshold = cfg_getfloat (cfg,"valrm_on_threshold");
	shmem->valrm_off_threshold = cfg_getfloat (cfg,"valrm_off_threshold");
	shmem->tank_capacity = cfg_getint (cfg,"tank_capacity");
	shmem->low_fuel_alarm = cfg_getint (cfg,"low_fuel_alarm");
	shmem->no_fuel_cutoff = cfg_getint (cfg,"no_fuel_cutoff");
	strncpy (shmem->operator_code,cfg_getstr (cfg, "operator_code"),sizeof (shmem->operator_code));

	shmem->ADC0_cal.raw1 = cfg_getint   (cfg, "ADC0_RAW1");
	shmem->ADC0_cal.cal1 = cfg_getfloat (cfg, "ADC0_CAL1");
	shmem->ADC0_cal.raw2 = cfg_getint   (cfg, "ADC0_RAW2");
	shmem->ADC0_cal.cal2 = cfg_getfloat (cfg, "ADC0_CAL2");

	shmem->ADC1_cal.raw1 = cfg_getint   (cfg, "ADC1_RAW1");
	shmem->ADC1_cal.cal1 = cfg_getfloat (cfg, "ADC1_CAL1");
	shmem->ADC1_cal.raw2 = cfg_getint   (cfg, "ADC1_RAW2");
	shmem->ADC1_cal.cal2 = cfg_getfloat (cfg, "ADC1_CAL2");

}

void cal_cfg_write (BBD9000mem *shmem) {
cfg_t *cfg;
FILE *conf_fp;

	// Initialize defaults
	cfg = cal_cfg_init(shmem);
	
	// Set based on shmem
	cal_cfg_from_mem (cfg, shmem);

	// Override print functions
	cfg_set_print_func(cfg, "flowmeter_pulses_per_gallon", print_float3);
	cfg_set_print_func(cfg, "pump_on_threshold", print_float2);
	cfg_set_print_func(cfg, "pump_off_threshold", print_float2);
	cfg_set_print_func(cfg, "valrm_on_threshold", print_float2);
	cfg_set_print_func(cfg, "valrm_off_threshold", print_float2);
	cfg_set_print_func(cfg, "ADC0_CAL1", print_float2);
	cfg_set_print_func(cfg, "ADC0_CAL2", print_float2);
	cfg_set_print_func(cfg, "ADC1_CAL1", print_float2);
	cfg_set_print_func(cfg, "ADC1_CAL2", print_float2);

	// Save the configuration.
	conf_fp = fopen (shmem->cal_conf,"w");
	if (conf_fp) {
		cfg_print(cfg, conf_fp);
		fclose(conf_fp);
	}
	cfg_free(cfg);
}


cfg_t *run_cfg_init (BBD9000mem *shmem) {
cfg_t *cfg;

	// Declaration of options in the config file
	static cfg_opt_t opts[] = {
	// Total gallons evar
		CFG_FLOAT("cumulative_gallons", 0, CFGF_NONE),
	// Available gallons now
		CFG_FLOAT("avail_gallons", 500, CFGF_NONE),
	// Reason for the reboot ("patch", "power", "restart")
		CFG_STR("boot_reason", "", CFGF_NONE),
	// Time of reboot
		CFG_INT("boot_time", 0, CFGF_NONE),
	// FSM state at reboot
		CFG_STR("boot_state", "UNKNOWN", CFGF_NONE),
	// The patch program cats any messages here.
	// The patch status should never be written to this file except by the patch program
	// A patch status of "" with a boot_reason of "patch" is an error
		CFG_STR("patch_status", "", CFGF_NONE),

	// Last price-per-gallon (base, or member price)
		CFG_FLOAT("last_ppg", 0, CFGF_NONE),

	// Fuel type, i.e. B20, B99, B100, etc.
		CFG_STR("fuel_type", "B99", CFGF_NONE),

	// transaction info
		CFG_INT("cc_resp_doVoid", 0, CFGF_NONE),
		CFG_STR("cc_resp_trans_id", "", CFGF_NONE),
		CFG_STR("cc_resp_auth_code", "", CFGF_NONE),
		CFG_STR("cc_resp_error", "", CFGF_NONE),
		CFG_FLOAT("memb_pre_auth", 0, CFGF_NONE),
		CFG_FLOAT("memb_dollars", 0, CFGF_NONE),
		CFG_STR("msr_name", "", CFGF_NONE),
		CFG_STR("memb_number", "", CFGF_NONE),
		CFG_STR("cc_resp_message", "", CFGF_NONE),
		CFG_FLOAT("memb_credit", 0, CFGF_NONE),
		CFG_FLOAT("memb_gallons", 0, CFGF_NONE),
		CFG_FLOAT("memb_ppg", 0, CFGF_NONE),
		CFG_FLOAT("renewal_fee", 0, CFGF_NONE),
		CFG_FLOAT("full_membership_fee", 0, CFGF_NONE),
		CFG_FLOAT("temp_membership_fee", 0, CFGF_NONE),
		CFG_FLOAT("upgrade_fee", 0, CFGF_NONE),

		CFG_END()
    };

    cfg = cfg_init(opts, 0);
    cfg_setfloat (cfg, "avail_gallons", shmem->tank_capacity);
    return (cfg);
}


int run_cfg_read  (BBD9000mem *shmem) {
cfg_t *cfg;

	// Initialize defaults
	cfg = run_cfg_init(shmem);

	// Get default overrides from the configuration file
	if ( cfg_parse(cfg, shmem->run_conf) != CFG_SUCCESS ) {
		return (-1);
	}

	shmem->cumulative_gallons = cfg_getfloat (cfg,"cumulative_gallons");
	shmem->avail_gallons = cfg_getfloat (cfg,"avail_gallons");
	strncpy (shmem->boot_reason,cfg_getstr (cfg, "boot_reason"),sizeof (shmem->boot_reason));
	shmem->boot_time = cfg_getint (cfg,"boot_time");
	strncpy (shmem->boot_state,cfg_getstr (cfg, "boot_state"),sizeof (shmem->boot_state));
	strncpy (shmem->patch_status,cfg_getstr (cfg, "patch_status"),sizeof (shmem->patch_status));

	shmem->last_ppg = cfg_getfloat (cfg,"last_ppg");
	if (FABS(shmem->last_ppg) < 0.001) shmem->last_ppg = 0;

	strncpy (shmem->fuel_type,cfg_getstr (cfg, "fuel_type"),sizeof (shmem->fuel_type));


	// Read any transaction info
	shmem->cc_resp.doVoid = cfg_getint (cfg,"cc_resp_doVoid");
	strncpy (shmem->cc_resp.trans_id,cfg_getstr (cfg, "cc_resp_trans_id"),sizeof (shmem->cc_resp.trans_id));
	strncpy (shmem->cc_resp.auth_code,cfg_getstr (cfg, "cc_resp_auth_code"),sizeof (shmem->cc_resp.auth_code));
	strncpy (shmem->cc_resp.error,cfg_getstr (cfg, "cc_resp_error"),sizeof (shmem->cc_resp.error));
	shmem->memb_pre_auth = cfg_getfloat (cfg,"memb_pre_auth");
	shmem->memb_dollars = cfg_getfloat (cfg,"memb_dollars");
	strncpy (shmem->msr_name,cfg_getstr (cfg, "msr_name"),sizeof (shmem->msr_name));
	strncpy (shmem->memb_number,cfg_getstr (cfg, "memb_number"),sizeof (shmem->memb_number));
	strncpy (shmem->cc_resp.message,cfg_getstr (cfg, "cc_resp_message"),sizeof (shmem->cc_resp.message));

	shmem->memb_credit = cfg_getfloat (cfg,"memb_credit");
	shmem->memb_gallons = cfg_getfloat (cfg,"memb_gallons");
	shmem->memb_ppg = cfg_getfloat (cfg,"memb_ppg");
	shmem->renewal_fee = cfg_getfloat (cfg,"renewal_fee");
	shmem->full_membership_fee = cfg_getfloat (cfg,"full_membership_fee");
	shmem->temp_membership_fee = cfg_getfloat (cfg,"temp_membership_fee");
	shmem->upgrade_fee = cfg_getfloat (cfg,"upgrade_fee");
	// Set the sale flags
	if (FABS(shmem->memb_credit) < 0.005) shmem->memb_credit = 0;
	if (FABS(shmem->memb_gallons) < 0.001) shmem->memb_gallons = 0;
	if (FABS(shmem->memb_ppg) < 0.001) shmem->memb_ppg = 0;
	if (FABS(shmem->renewal_fee) < 0.001) {
		shmem->renewal_fee = 0;
		shmem->memb_renewal_sale = 0;
	} else {
		shmem->memb_renewal_sale = 1;
	}
	if (FABS(shmem->full_membership_fee) < 0.001) {
		shmem->full_membership_fee = 0;
		shmem->memb_full_membership_sale = 0;
	} else {
		shmem->memb_full_membership_sale = 1;
	}
	if (FABS(shmem->temp_membership_fee) < 0.001) {
		shmem->temp_membership_fee = 0;
		shmem->memb_temp_membership_sale = 0;
	} else {
		shmem->memb_temp_membership_sale = 1;
	}
	if (FABS(shmem->upgrade_fee) < 0.001) {
		shmem->upgrade_fee = 0;
		shmem->memb_upgrade_sale = 0;
	} else {
		shmem->memb_upgrade_sale = 1;
	}

	// Clean up a pending transaction if there's no ID
	if (! *(shmem->cc_resp.trans_id) ) {
		memset (&(shmem->cc_resp),0,sizeof (cc_resp_s));
	}


	cfg_free(cfg);
	return (0);
}

void run_cfg_write (BBD9000mem *shmem) {
FILE *conf_fp;

	// Since we can't seem to be able to over-ride writing the patch_status option
	// we're bypassing the configuration writing by libconfuse and doing it manualy
	conf_fp = fopen (shmem->run_conf,"w");
	if (conf_fp) {
		fprintf (conf_fp,"cumulative_gallons = %.3f\n",shmem->cumulative_gallons);
		fprintf (conf_fp,"avail_gallons = %.3f\n",shmem->avail_gallons);
		fprintf (conf_fp,"boot_reason = \"%s\"\n",shmem->boot_reason);
		fprintf (conf_fp,"boot_time = %ld\n",(long)shmem->boot_time);
		fprintf (conf_fp,"boot_state = \"%s\"\n",shmem->status);

		fprintf (conf_fp,"last_ppg = %.2f\n",shmem->last_ppg);
		fprintf (conf_fp,"fuel_type = \"%s\"\n",shmem->fuel_type);

		// write any transaction info if it exists
		// Clean up a pending transaction if there's no ID
		if (! *(shmem->cc_resp.trans_id) ) {
			memset (&(shmem->cc_resp),0,sizeof (cc_resp_s));
		} else {
			fprintf (conf_fp,"cc_resp_doVoid = %d\n",(int) (shmem->cc_resp.doVoid ? 1 : 0));
			fprintf (conf_fp,"cc_resp_trans_id = \"%s\"\n",shmem->cc_resp.trans_id);
			fprintf (conf_fp,"cc_resp_auth_code = \"%s\"\n",shmem->cc_resp.auth_code);
			fprintf (conf_fp,"cc_resp_error = \"%s\"\n",shmem->cc_resp.error);
			fprintf (conf_fp,"memb_pre_auth = %.2f\n",shmem->memb_pre_auth);
			fprintf (conf_fp,"memb_dollars = %.2f\n",shmem->memb_dollars);
			fprintf (conf_fp,"msr_name = \"%s\"\n",shmem->msr_name);
			fprintf (conf_fp,"memb_number = \"%s\"\n",shmem->memb_number);
			fprintf (conf_fp,"cc_resp_message = \"%s\"\n",shmem->cc_resp.message);

			if (FABS(shmem->memb_credit) > 0.005)
				fprintf (conf_fp,"memb_credit = %.2f\n",shmem->memb_credit);
			if (shmem->memb_gallons > 0.001)
				fprintf (conf_fp,"memb_gallons = %.3f\nmemb_ppg = %.2f\n",shmem->memb_gallons,shmem->memb_ppg);
			if (shmem->memb_renewal_sale)
				fprintf (conf_fp,"renewal_fee = %.2f\n",shmem->renewal_fee);
			if (shmem->memb_full_membership_sale)
				fprintf (conf_fp,"full_membership_fee = %.2f\n",shmem->full_membership_fee);
			if (shmem->memb_temp_membership_sale)
				fprintf (conf_fp,"temp_membership_fee = %.2f\n",shmem->temp_membership_fee);
			if (shmem->memb_upgrade_sale)
				fprintf (conf_fp,"upgrade_fee = %.2f\n",shmem->upgrade_fee);
		}
		
		fclose  (conf_fp);
	}
}

void print_float2(cfg_opt_t *opt, unsigned int index, FILE *fp) {
	float value = cfg_opt_getnfloat(opt, index);
    fprintf(fp, "%.2f",value);
}

void print_float3(cfg_opt_t *opt, unsigned int index, FILE *fp) {
	float value = cfg_opt_getnfloat(opt, index);
    fprintf(fp, "%.3f",value);
}

// msg can be "start", "stop", "restart", "idle", "check" or NULL
// This will block and return with a lock if get_lock is true
int netlink (BBD9000mem *shmem, const char *msg, char get_lock) {
char line[STR_SIZE];
	
	if (msg && (! strcmp (msg,"start") || ! strcmp (msg,"stop") || ! strcmp (msg,"restart") || ! strcmp (msg,"idle") || ! strcmp (msg,"check")) ) {
		sprintf (line,"NETLOCK=%s %s/%s %s >/dev/null 2>&1 &",shmem->BBD9000net, shmem->root_path, BBD9000netlink, msg);
		system (line);
	}
	
	if (get_lock) {
		usleep (400000); // give enough time for netlink to startup and acquire the lock. 200ms is not enough, 300ms enough
		return (netlock(shmem));
	}
	return (-1);
}


int netlock (BBD9000mem *shmem) {
int fdlock;
char *char_p,SMS_stat[32];
char bigBuf[READ_BUF_SIZE];
long nread;

	// Open the file read/write
	if((fdlock = open(shmem->BBD9000net, O_RDWR, 0666)) < 0)
		return (-1);

 	if(flock(fdlock, LOCK_EX) == -1) {
		close (fdlock);
		return (-3);
	}

	// The netlock file contains recent network status.
	// the first line is "offline", "ethernet", "cellular", or "WiFi"
	// the second line is ethernet status:
	//  <IP Address> <status>
	//    <IP Address> is either a valid IP address or "No-IP-Addr"
	//    <status> is "Offline", "No-WAN", "DNS-Offline", or "Up"
	// the third line is cellular status
	//  <IP Address> <status> <nnn/No> dBm (<n/->/5) SIM: <OK/None/Error> SMS: <Checkin/None>
	// the fourth line is WiFi data - unimplemented; blank or absent.
	//  <IP Address> <status> <nnn/No> dBm (<n/->/5) SIM: <OK/None/Error> SMS: <Checkin/None>
	nread = read(fdlock, bigBuf, READ_BUF_SIZE);
	if (nread > 0) {
		bigBuf[nread]='\0';
		sscanf (bigBuf,"%[^\n]%*[\n]%[^\n]%*[\n]%[^\n]%*[\n]%[^\n]%*[\n]",
			shmem->net_status,shmem->net_status_ethernet,shmem->net_status_cellular,shmem->net_status_WiFi
		);
	}

	// parse the SMS status
	char_p = strrchr (shmem->net_status_cellular,':');
	if (char_p && char_p -3 > shmem->net_status_cellular) {
		char_p -= 3;
		sscanf (char_p,"SMS: %s",SMS_stat);
		if (! strcmp (SMS_stat,"Checkin") ) shmem->checkin_msg = 1;
	} 

	if (!strncmp (shmem->net_status,"offline",4)) { // No network, so no lock
		netunlock (shmem, fdlock);
		return (-5);
	}

	return (fdlock);
}


void netunlock (BBD9000mem *shmem, int fdlock) {

	if (fdlock > -1) {
		flock(fdlock, LOCK_UN);
		close (fdlock);
	}
}
