#include <TimerOne.h>
#include <MsTimer2.h>

const char SET_PERIOD_HEADER      = 'p';
const char SET_FREQUENCY_HEADER   = 'f';
const char SET_PULSE_WIDTH_HEADER = 'w';
const char SET_DUTY_CYCLE_HEADER  = 'c';
const char SET_PERIOD_RAMP_HEADER = 'P';
const char SET_PULSE_WIDTH_RAMP_HEADER = 'W';


#define pwmRegister OCR1A  // the logical pin, can be set to OCR1B
const int outPin =  9;     // the physical output pin
const int inPin = 2;       // the physical input pin (can be 2 or 3)
const int sampleTime = 1000; // milliseconds to sample input pin

long period = 1000;     // the period in microseconds
int duty = 512;         // duty as a range from 0 to 1024, 512 is 50% duty cycle
long pulseWidth = 500;  // pulse width in microseconds (synchronized with duty)
long periodRamp = 0;    // ramp period up and down every sample by periodRamp microseconds
long periodRampMax = 0; // value to ramp up to
long pulseRamp = 0;     // ramp pulse width up and down every sample by periodRamp microseconds
long pulseRampMax = 0;  // value to ramp up to

int prescale[] = {0,1,8,64,256,1024}; // the range of prescale values


volatile long PULSE_COUNTER = 0;
volatile bool OUTPUT_READY = false;
volatile char inputInterrupt = 0;
void pulse_ISR() {
	PULSE_COUNTER++;
}
void time_ISR() {
	OUTPUT_READY = true;
}

void stop_measuring() {
	// stop timing
	MsTimer2::stop();
	detachInterrupt(inputInterrupt);
	OUTPUT_READY = false;
}

void start_measuring() {
	PULSE_COUNTER = 0;
	OUTPUT_READY = false;
	// interrupt on change for inPin (physical pin 2 or 3)
	attachInterrupt(inputInterrupt, pulse_ISR, CHANGE);
	MsTimer2::set(sampleTime, time_ISR); // 1s period
	MsTimer2::start();
}

void setup() {
	Serial.begin(9600);
	pinMode(outPin, OUTPUT);
	pinMode(inPin, INPUT);
	Timer1.initialize(period);        // initialize timer1, 1000 microseconds
	Timer1.pwm(9, duty);              // setup pwm on pin 9, 50% duty cycle
	pulseWidth = getPulseWidth();

	if (inPin == 3) inputInterrupt = 1;
	start_measuring();
}


void loop() {
	processSerial();
}

void processSerial()
{
	static long val = 0;

	if ( Serial.available()) {
		char ch = Serial.read();
		if ( !( ch == '\r' || ch == '\n') ) stop_measuring();

		if(ch >= '0' && ch <= '9') {              // is ch a number?
			val = val * 10 + ch - '0';           // yes, accumulate the value
		} else if(ch == SET_PERIOD_HEADER) {
			period = val;
			Serial.print("Setting period to ");
			Serial.println(val);
			setPeriod(val);
			periodRamp = 0;
			pulseRamp = 0;
			start_measuring();
			val = 0;
		} else if(ch == SET_FREQUENCY_HEADER) {
			if(val > 0) {
				Serial.print("Setting frequency to ");
				Serial.println(val);
				setPeriod (1000000 / val);
				periodRamp = 0;
				pulseRamp = 0;
			}
			start_measuring();
			val = 0;
		} else if(ch ==  SET_PULSE_WIDTH_HEADER) {
			if( setPulseWidth(val) ) {
				Serial.print("Setting Pulse width to ");
				Serial.println(pulseWidth);
				periodRamp = 0;
				pulseRamp = 0;
			} else {
				Serial.println("Pulse width too long for current period");
			}
			start_measuring();
			val = 0;
		} else if(ch == SET_DUTY_CYCLE_HEADER) {
			if( val >0 && val < 100) {
				Serial.print("Setting Duty Cycle to ");
				Serial.println(val);
				periodRamp = 0;
				pulseRamp = 0;
				duty = map(val,1,99, 1, ICR1);
				pwmRegister = duty;
				pulseWidth = getPulseWidth();
			}
			start_measuring();
			val = 0;
		} else if(ch == SET_PERIOD_RAMP_HEADER) {
			if (val < period) {
				Serial.print("Ramping period by ");
				Serial.print(val);
				Serial.println(" us");
				periodRamp = -val;
				periodRampMax = period;
			} else {
				Serial.println("Ramp too high for current period");
			}
			start_measuring();
			val = 0;
		} else if(ch == SET_PULSE_WIDTH_RAMP_HEADER) {
			if ( val < pulseWidth) {
				Serial.print("Ramping pulse width by ");
				Serial.print(val);
				Serial.println(" us");
				pulseRamp = -val;
				pulseRampMax = pulseWidth;
			} else {
				Serial.println("Ramp too high for current pulse width");
			}
			start_measuring();
			val = 0;
		}
	}
	
	if (OUTPUT_READY) {
		// stop measuring
		stop_measuring();
		show();
		if (pulseRamp) {
			if (pulseWidth + pulseRamp > pulseRampMax || pulseWidth + pulseRamp <= 0) pulseRamp = -pulseRamp;
			setPulseWidth(pulseWidth + pulseRamp);
		}
		if (periodRamp) {
			if (period + periodRamp > periodRampMax || period + periodRamp <= 0) periodRamp = -periodRamp;
			setPeriod(period + periodRamp);
		}
		start_measuring();
	}
}

void setPeriod (long microseconds) {
	Timer1.setPeriod(microseconds);
	Timer1.setPwmDuty(outPin, duty);  // don't change the duty cycle
	period = microseconds;
	pulseWidth = getPulseWidth();
}

bool setPulseWidth(long microseconds) {
	bool ret = false;

	int prescaleValue = prescale[Timer1.clockSelectBits];
	// calculate time per tick in ns
	long  precision = (F_CPU / 128000)  * prescaleValue  ;   
	period = precision * ICR1 / 1000; // period in microseconds
	if( microseconds < period) {
		duty = map(microseconds, 0,period, 0,1024);
		if( duty < 1)
			duty = 1;
		if(microseconds > 0 && duty < RESOLUTION) {
			Timer1.pwm(outPin, duty);
			ret = true;
		}
	}
	pulseWidth = getPulseWidth();
	return ret;
}

long getPulseWidth() {

	int prescaleValue = prescale[Timer1.clockSelectBits];
	// calculate time per tick in ns
	long  precision = (F_CPU / 128000)  * prescaleValue  ;   
	period = precision * ICR1 / 1000; // period in microseconds
	return (map(duty, 0,1024, 0, period));
}



void show() {
	Serial.print("Period = ");
	Serial.print(period);
	Serial.print(" us, pulse width = ");
	Serial.print( pulseWidth );      
	Serial.print(" us (");
	// pwmRegister is ICR1A or ICR1B
	Serial.print( map( pwmRegister, 0,ICR1, 1,99));      
	Serial.print( "%), ");      
	Serial.print( PULSE_COUNTER );      
	Serial.print(" edges counted in ");
	Serial.print( sampleTime );      
	Serial.println(" ms ");
}

