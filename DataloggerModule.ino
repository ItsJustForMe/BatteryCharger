#include <Timer.h>
#include "gjroutines.h"
#include "datalogger.h"

// These constants are dependent on used Arduino-type
const float ardADsteps = 1024;						// Number of discrete steps in A/D-converter. For Arduino Uno (10 bits) this is 1024
const float ardVcc = 5.0;							// Make sure the voltage of your board matches this value

// These constants are generic for the purpose of this module
const int MAX_BATT_RELAYS = 3;						// Max # of relays on the batteries. Remember, array-indexes go from 0 to MAX_RELAYS
const int MAX_MODE_RELAYS = 2;						// Max # relays involved in mode-setting

// These constants depend on the used resistors in the voltmeter-divider
const float voltDivider = 1.0 / 2.0;				// How much the volt-divider breaks the measured voltage.

// Relay-related variables
relay battRelays[MAX_BATT_RELAYS] = {				// Pin-numbers and states for battery-relays
	{ 7, OFF },
	{ 8, OFF },
	{ 9, OFF } };

relay modeRelays[MAX_MODE_RELAYS] = {				// Pin-numbers for relays for mode-selection
	{ 10, OFF },
	{ 11, OFF } };

battselect batterySelect;

const int modePoll = 0;								// Reflects row 0 in array modeRelays
const int modeCharge = 1;							// Reflects row 1 in array modeRelays
const int modeNone = -1;							// Reflects no rows in array modeRelays
const int buttonPin = 5;							// Button goes to HIGH when pressed, is LOW when unpressed
const int voltPin = 5;								// This is the analog pin used for voltage-metering

const int battSwitchDelay = 3000;					// Deay between disconnecting and connecting batteries to solar-charge-controller

// Timer-variables
SimpleTimer timer;
int tmrPoll;									// For measurements of voltage, etc.
int tmrSwitchCheck;								// Timer to determine if switching battery-to-load is needed (based on charge of motor-batteries)
int tmrButtonCheck;								// Timer to check for button press
int tmrShowStatus;								// Timer to show status every now and then

// Define time between calls to routines (in millisecs)
const unsigned long pollInterval = 23000;			// Interval between voltage polls. Production = 300.000 (= 5 minutes)
const unsigned long switchInterval = 21000;			// Check if switching load to other battery ius needed. Production = 7.200.000 (= 2 hours)
const unsigned long buttonInterval = 1000;			// Interval between button checks
const unsigned long statusInterval = 15000;			// Interval to show status of system

void setup()
{
	Serial.print("Arduino "); Serial.print(boardType()); Serial.println(" running. Setup...");
	Serial.print("Free mem: "); Serial.print(freeRAM()); Serial.println(" bytes");
	
	initRelays();
	setBattSelect(AUTO);

	analogReference(DEFAULT);					// Just to be sure. Analog ranges from 0-5V (on 5V-Arduino boards)
	pinMode(buttonPin, INPUT_PULLUP);
	pinMode(voltPin, INPUT);

	tmrPoll = timer.setInterval(pollInterval, pollData);
	tmrSwitchCheck = timer.setInterval(switchInterval, switchCheck);
	tmrButtonCheck = timer.setInterval(buttonInterval, buttonCheck);
	tmrShowStatus = timer.setInterval(statusInterval, showStatus);

	
	switchCheck();						// Select battery with lowest voltage and start charging

	Serial.println("Starting loop...");
}

void loop()
{
	// Data-logging will be triggered by timer-functions
	timer.run();
}

void initRelays() {
	// Initializes the system by performing the following:
	// - Sets all relay-pins to OUTPUT
	// - Switches all battery-relays to 'off'
	// - Switches all mode-relays to 'off'

	Serial.print("Initializing relays...");

	for (int i = 0; i < MAX_BATT_RELAYS; i++) {	// Init all battery-relays
		pinMode(battRelays[i].pin, OUTPUT);		// Set the corresponding pin-mode
		battDisconnect(i);						// Switch off relay.
	}

	for (int i = 0; i < MAX_MODE_RELAYS; i++) {
		int pin = modeRelays[i].pin;	// Get the pin-number for this relay
		pinMode(pin, OUTPUT);			// Set the corresponding pin-mode
		digitalWrite(pin, LOW);			// Switch off relay.
		modeRelays[i].status = OFF;		// Register the state for this relay
	}
	Serial.println(" Done!");
}

void pollData() {
	timer.disable(tmrPoll);				// No interrupts for this timer while polling, please
	timer.disable(tmrSwitchCheck);		// This can take a while, so don't check batteries in between

	setMode(modePoll);					// But now we're going to poll for all kinds of data

	Serial.println("Polling data...");
	// Serial.print("Voltage = "); Serial.print(getVoltage(voltPin)); Serial.println("V");

	// Measure:
	// - Solar input voltage
	// - Battery 1 voltage
	// - Battery 2 voltage (this is one of the bike-batteries)
	// - (maybe select all batteries and measure voltage)
	// - Temp-sensor on Arduino-board
	// - Current trough solar-input

	// Write measured data to 'somewhere'
	// Possible scenario's
	// - Upload to server to text-file
	// - Upload to MySQL-database on server using PHP-requests
	// - First store on local SD-card and then bulk-upload using either option mentioned above

	setMode(modeCharge);					// Restore the mode that we were in 

	// Restart the interrupt-timer for this routine
	timer.restartTimer(tmrPoll);			// Reset to zero...
	timer.enable(tmrPoll);					// ... and start.
	timer.restartTimer(tmrSwitchCheck);		// Reset to zero...
	timer.enable(tmrSwitchCheck);			// ... and start.
}

void switchCheck() {
	// Checks if switching the charging to another battery is needed
	// The battery with the lowest voltage will be selected
	int battLowest;
	float voltLowest, voltCurr;

	if (getBattSelect() == AUTO) {
		timer.disable(tmrSwitchCheck);					// No need to interrupt this if we're in this routine

		setMode(modePoll);								// Stop charging, otherwise we can't measure voltages
		voltLowest = 999.9;
		for (int i = 0; i < MAX_BATT_RELAYS; i++) {		// Check voltage on all battery-relays
			setBattery(i);								// Select the battery
			voltCurr = getVoltage(battRelays[i].pin);	// Read the voltage
			if (voltCurr < voltLowest) {
				voltLowest = voltCurr;
				battLowest = i;
			}
		}
		setBattery(battLowest);
		setMode(modeCharge);

		// Restart the interrupt-timer for this routine
		timer.restartTimer(tmrSwitchCheck);				// Reset to zero...
		timer.enable(tmrSwitchCheck);					// ... and start.
	} // if (getMode() == modeCharge)
}

void buttonCheck() {
	int butVal;

	timer.disable(tmrButtonCheck);				// Make sure we're not interrupted while processing the button

	butVal = digitalRead(buttonPin);
	if (random(5) == 1) {
//	if (butVal == HIGH) {
		// activate next battery
		Serial.println("Button pressed!");
		setBattery(getBattery()+1);
	}

	// Restart the interrupt-timer for this routine
	timer.restartTimer(tmrButtonCheck);			// Reset to zero...
	timer.enable(tmrButtonCheck);				// ... and start.
}

void showStatus() {
	Serial.println("----------------- STATUS -----------------");
	Serial.println("[Work in progress]");
	Serial.print("Free memory: "); Serial.println(freeRAM());
	Serial.print("System mode: "); Serial.print(modeName(getMode()));
	if (getMode() == modeCharge) { Serial.print(". Charging "); Serial.println(battName(getBattery())); }
	Serial.print("Battery select: "); Serial.println(battSelectName(getBattSelect()));
	Serial.println("------------------------------------------");
}

battselect getBattSelect() {
	return batterySelect;
}

void setBattSelect(battselect newSysMode) {
	batterySelect = newSysMode;
}

float getVoltage(int analogPin) {
	return (analogRead(analogPin) / ardADsteps) * ardVcc / voltDivider;
}

void setBattery(int battIndex) {
// Sets the relays to connect the indicated battery (numbered 0 - [MAX_BATT_RELAYS-1] via the relays to the Arduino-board.
// Depending on the mode the slected battery will be charged or measured (poll-mode)
// To make sure the solar-charge-controller doesn't get confused we wait 3 seconds between disconnecting the current battery
// and connecting the selected battery.
	int currBattery = -999;

	if (battIndex > (MAX_BATT_RELAYS - 1) || battIndex < 0) battIndex = 0;

	currBattery = getBattery();
	if (currBattery != battIndex) {								// Only switch if requested battery is different from currently active one
		battDisconnect(currBattery);
		if (getMode() == modeCharge) delay(battSwitchDelay);	// Wait predefined time but only if we're charging
		battConnect(battIndex);									// Connect requested battery
	}
}

int getBattery() {
	// Returns the index of the battery for which the relay is activated.
	int battIndex = -5;						// If no active batt-relay found, do not return a valid battery-index.

	for (int i = 0; i < MAX_BATT_RELAYS; i++) {
		if (battRelays[i].status == ON) {
			battIndex = i;
			i = MAX_BATT_RELAYS;
		}
	}
	return battIndex;
}

void battConnect(int battIndex) {
// Connects the indicated battery to the wiring allowing it to be either polled (measured) or charged, depending 
// on the selected mode (either POLL, CHARGE or NONE).

	if (battIndex >= 0 && battIndex < MAX_BATT_RELAYS) {
		digitalWrite(battRelays[battIndex].pin, HIGH);
		battRelays[battIndex].status = ON;
	}
}

void battDisconnect(int battIndex) {
// Disconnects the specified battery

	if (battIndex >= 0 && battIndex < MAX_BATT_RELAYS) {		// Index is in range
		digitalWrite(battRelays[battIndex].pin, LOW);
		battRelays[battIndex].status = OFF;
	}
}

void setMode(int sysMode) {
// Activates the correct relays to allow mode POLL, CHARGE or NONE.
// All batteries will be disconnected.
	static unsigned long lastCallTime = 0;					// Helps to prevent multiple calls within a short period
	unsigned long timeSinceLast;

	timeSinceLast = millis() - lastCallTime;
	if (timeSinceLast < battSwitchDelay) {
		delay(battSwitchDelay - timeSinceLast);
	}
	lastCallTime = millis();

	if (sysMode != getMode()) {								// Only do something when needed
		battDisconnect(-999);								// Disconnect all batteries

		switch (sysMode) {
		case modePoll:
			digitalWrite(modeRelays[modeCharge].pin, LOW);	// Disconnect from charger
			modeRelays[modeCharge].status = OFF;

			digitalWrite(modeRelays[modePoll].pin, HIGH);	// Connect to polling-wiring
			modeRelays[modePoll].status = ON;

			break;
		case modeCharge: 
			digitalWrite(modeRelays[modePoll].pin, LOW);	// Disconnect from polling-wiring
			modeRelays[modePoll].status = OFF;

			digitalWrite(modeRelays[modeCharge].pin, HIGH);	// Connect to charge-wiring
			modeRelays[modeCharge].status = ON;

			break;
		case modeNone:
			// break; // Don't use break here: If some other (unexpected) value is passed then default to safe option
		default:					
			for (int i = 0; i < MAX_MODE_RELAYS; i++) {
				digitalWrite(modeRelays[i].pin, LOW);	// Disconnect from polling-wiring
				modeRelays[i].status = OFF;
			}		// for (int i = 0 ....
		}			// switch (sysMode)
	}				// if (sysMode != getMode())
}

int getMode() {
	int currMode = modeNone;

	for (int i = 0; i < MAX_MODE_RELAYS; i++) {
		if (modeRelays[i].status == ON) {
			currMode = i;
			i = MAX_MODE_RELAYS;
		}
	}
	return currMode;
}
