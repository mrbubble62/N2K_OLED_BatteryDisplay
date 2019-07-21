//BOARD WEMOS LoLin32

//#define ARDUINO_ARCH_ESP32 
#define EEPROM_ADDR_CONFIG 10 // starting eeprom address for config params
#define ESP32_CAN_TX_PIN GPIO_NUM_14
#define ESP32_CAN_RX_PIN GPIO_NUM_12

#include <Arduino.h>
#include <NMEA2000_CAN.h>
#include <N2kMessages.h>
#include <N2kMessagesEnumToStr.h>
#include <SPI.h>
#include <string.h>
//#include <EEPROM.h> // Teensy
#include <Preferences.h> // ESP32 NVS
#include <Time.h>
#include <TimeLib.h>
#include "TimeZone.h"
#include <print.h>
#include "fonts.h"
#include "ESP8266_SSD1322.h"
#include "TimeZone.h"

#define AUTOIDDEVICES 1 // automatically detect engine and battery instance for vessels with only one motor and one battery
#define BATTERYINSTANCE 1
#define ENGINEINSTANCE 1

// Timezone rules
// https://github.com/JChristensen/Timezone
TimeChangeRule usEDT = { "EDT", Second, Sun, Mar, 2, -240 };  //UTC - 4 hours
TimeChangeRule usEST = { "EST", First, Sun, Nov, 2, -300 };   //UTC - 5 hours
Timezone myTZ(usEDT, usEST);


float volts = 0;
float amps = 0;
float V = 0;
float A = 0;
uint16_t rpm;
float oilTemp;
float oilPressure;
float waterTemp;
uint8_t Brightness = 15;
uint8_t startX = 0;
String msg = "";

// Debugging Serial 
Stream *OutputStream;

// Debugging toggles
bool SDEBUG = false; // toggle debug print
bool SDATA = false; // toggle data print

uint32_t delt_t = 0; // 250 ms loop
uint32_t count = 0; // 250 ms loop
long slowloop = 0; // 1s loop

#define STALEDATATIME 5  // seconds before invalidating displayed data
long batteryDataValid = 255;
long engineDataValid = 255;
long timeDataValid = 255;
bool blink; // battery display warning flash

// SSD1322 Display **************
// 23 SDA MOSI
// 18 CLK
#define OLED_CS     17  // CS - Chip select
#define OLED_DC     5   // DC digital signal
#define OLED_RESET  19  // Originally pin 16 but froze on CAN RX

ESP8266_SSD1322 display(OLED_DC, OLED_RESET, OLED_CS);

// Clock *******************
time_t NetTime = 0;  //0 = NMEA network time is invalid

// convert N2K days since1970 to unix time seconds since 1970					
time_t N2KDate(uint16_t DaysSince1970) {
	return DaysSince1970 * 24 * 3600;
}

// EEPROM **********************
Preferences EEPROM;

// EEPROM configuration structure
#define MAGIC 12352 // EPROM struct version check, change this whenever tConfig structure changes
typedef struct {
	uint16_t Magic; //test if eeprom initialized
	uint8_t batteryInstance;
	uint8_t	engineInstance;
	uint8_t deviceInstance;  // changed by MFD
	uint8_t sourceAddr;  //NMEA 2000 source address negoitaited
} tConfig;

// EEPROM contents
tConfig config;

// Default EEPROM contents
const tConfig defConfig PROGMEM = {
	MAGIC,
	BATTERYINSTANCE, // battery instance (default in my Battery monitor)
	ENGINEINSTANCE,
	0, // deviceInstance
	6  // sourceAddr
};

// N2K *****************************************************************************
tN2kMsg N2kMsg; // N2K message constructor
int SID = 1;

// 	Received Messages			
typedef struct {
	unsigned long PGN;
	void(*Handler)(const tN2kMsg &N2kMsg);
} tNMEA2000Handler;

const unsigned long ReceiveMessage[] PROGMEM = { 126992L,127488L,127489L,127508L,0 }; //systemtime,EngineRapid,EngineDynamicParameters,DCBatStatus,Temperature,Pressure
const unsigned long TransmitMessages[] PROGMEM = { 127508L, 0 }; //Battery DC State

void SystemTime(const tN2kMsg &N2kMsg);
void EngineRapid(const tN2kMsg &N2kMsg);
void EngineDynamicParameters(const tN2kMsg &N2kMsg);
void DCBatStatus(const tN2kMsg &N2kMsg);
void BatteryConfigurationStatus(const tN2kMsg &N2kMsg);

tNMEA2000Handler NMEA2000Handlers[] = {
	{ 126992L,&SystemTime },
	{ 127488L,&EngineRapid },
	{ 127489L,&EngineDynamicParameters },
	{ 127508L,&DCBatStatus },
	{ 0,0 }
};

//NMEA 2000 message handler
void HandleNMEA2000Msg(const tN2kMsg &N2kMsg) {
	int iHandler;
	for (iHandler = 0; NMEA2000Handlers[iHandler].PGN != 0 && !(N2kMsg.PGN == NMEA2000Handlers[iHandler].PGN); iHandler++);
	if (NMEA2000Handlers[iHandler].PGN != 0) {
		NMEA2000Handlers[iHandler].Handler(N2kMsg);
	}
}

void displayVA(uint8_t h, float A, float V, bool invaliddata);
void displayRPM(uint8_t x, uint8_t y, uint16_t rpm, bool invaliddata);
void displayPSI(uint8_t x, uint8_t y, uint16_t psi, bool invaliddata, String lable);
void displayTemp(uint8_t x, uint8_t y, float t, bool invaliddata, String lable);
void displayTime(uint8_t x, uint8_t y, time_t t, String lable);

void setup() {
	Serial.begin(115200);
	Serial.println("STARTING");
	OutputStream = &Serial;
	ReadConfig();
	if (config.Magic != MAGIC) {
		Serial.print("MAGIC");
		Serial.println(config.Magic);
		Serial.println(F("EEPROM invalid, initializing..\n"));
		InitializeEEPROM();
		delay(200);
		Serial.println(F("Done.\n"));
	}
	else {
		Serial.println(F("Loaded saved config\n"));
		Serial.print("SRC:"); Serial.println(config.sourceAddr);
		Serial.print("INST:"); Serial.println(config.deviceInstance);
		Serial.print("BAT:"); Serial.println(config.batteryInstance);
		Serial.print("ENG:"); Serial.println(config.engineInstance);
	}
	// Initialize display and perform reset
	Serial.println(F("Initialize display"));
	display.begin(true);
	// init done
	display.Set_Gray_Scale_Table();
	display.clearDisplay();
	display.display();

	NMEA2000.EnableForward(false);
	NMEA2000.SetProductInformation("07062018", // Manufacturer's Model serial code
		666, // Manufacturer's product code
		"Battery Display",  // Manufacturer's Model ID
		"1.0.0.1 (2015-08-14)",  // Manufacturer's Software version code
		"1.0.0.0 (2015-08-14)", // Manufacturer's Model version
		config.deviceInstance
	);
	NMEA2000.SetDeviceInformation(70618, // Unique number. Use e.g. Serial number.
		130, // Device function
		120, // Device class
		2040, // Test registration number
		config.deviceInstance
	);
	NMEA2000.SetMode(tNMEA2000::N2km_ListenAndNode, config.sourceAddr);
	NMEA2000.ExtendTransmitMessages(TransmitMessages, config.deviceInstance);
	NMEA2000.ExtendReceiveMessages(ReceiveMessage, config.deviceInstance);
	NMEA2000.SetN2kCANMsgBufSize(100);
	NMEA2000.SetMsgHandler(HandleNMEA2000Msg);
	NMEA2000.Open();
	clearMsgBuffer();
	newMsg("Message log:");
	SetMsg("");
	Serial.println(F("Begin main loop\n"));
}

// main loop
void loop() {
	char command = getCommand();
	switch (command) {
	case 'd': // toggle debug
		SDEBUG = !SDEBUG;
		break;
	case 'p': //toggle serial printing
		SDATA = !SDATA;
		break;
	default:
		break;
	}

	// 250ms Loop
	delt_t = millis() - count;
	if (delt_t > 250) { // fast update once per 250ms independent of read rate
		count = millis();
		digitalWrite(LED_BUILTIN, LOW);

		//slight damping
		V = (V + volts) / 2;
		A = (A + amps) / 2;

		// Warning voltage thresholds should change according to battery chemistry
		//draw new values and if voltage < 11.60 and discharing then blink
		if (V < 11.60 && A < 0) { // excess discharge warning - 11.60 for flooded cell
			blink = !blink;
			SetMsg("!!! Low Battery !!!");
		}
		else if (V > 15.60 && A > 0) { // excess charging voltage warning
			blink = !blink;
			SetMsg("!!! Excess charging voltage !!!");
		}
		else {
			blink = false;
			SetMsg("");
		}

		// scroll message
		horizontalScroll();

		if (!SDATA) {
			//**** Update Display *****
			if (!blink) displayVA(5, A, V, (batteryDataValid > STALEDATATIME) ? true : false);
			displayRPM(3, 52, rpm, (engineDataValid > STALEDATATIME) ? true : false);
			if (engineDataValid <= STALEDATATIME) {
				displayPSI(60, 52, oilPressure, false, "Oil:");
				displayTemp(0, 0, oilTemp, false, " @");
			}
			else
			{
				displayPSI(60, 52, oilPressure, true, "Oil:");
				displayTemp(0, 0, oilTemp, true, " @");
			}
			if (timeDataValid < 255) {
				displayTime(0, 0, myTZ.toLocal(now()), " Time ");
			}
			else {
				display.print("      NO GPS");
			}
			display.display();
		}

		//erase
		display.clearDisplay();
		//****** end display  ********
		// debug
		if (SDEBUG) {
			amps = amps + 0.01;
			batteryDataValid = 0;
		}
		if (SDATA) {
			printDebug();
			SetMsg("1234567890");
			//showMessage("Test message");
		}

		slowloop++;
	}
	// 1000ms
	if (slowloop > 3) { slowloop = 0; SlowLoop(); }
	NMEA2000.ParseMessages();
}

// slow message loop
void SlowLoop() {
	// Slow loop N2K message processing
	// check my address 
	if (config.sourceAddr != NMEA2000.GetN2kSource()) {
		Serial.print("\nAddress changed to: "); Serial.print(config.sourceAddr);
		config.sourceAddr = NMEA2000.GetN2kSource();
		UpdateConfig();
	}
	// check device instance for instructions from MFD
	tNMEA2000::tDeviceInformation DeviceInformation = NMEA2000.GetDeviceInformation();
	uint8_t DeviceInstance = DeviceInformation.GetDeviceInstance();
	if (DeviceInstance != config.deviceInstance) {
		Serial.print("\nInstance changed to: "); Serial.print(DeviceInstance);
		config.deviceInstance = DeviceInstance;
		UpdateConfig();
	}
	digitalWrite(LED_BUILTIN, HIGH);
	batteryDataValid++;
	engineDataValid++;
	timeDataValid++;
}

void displayVA(uint8_t h, float A, float V, bool invaliddata)
{
	display.setFont(NIXI);
	display.setFontKern(-5);
	display.setTextColor(WHITE);
	display.setCursor(0, h);
	if (invaliddata) display.print("--.-");
	else display.print(V, 1);
	display.setCursor(85, h + 12);
	display.setFont(NIXI20);
	display.print("V");
	display.setFont(NIXI);
	display.setFontKern(0);
	if (A < 0)
		display.setCursor(115, h);
	else
		display.setCursor(130, h);
	if (invaliddata) display.print("-.--");
	else {
		if (abs(A) > 99.99)
			display.print(A, 0);
		else if (abs(A) > 9.99)
			display.print(A, 1);
		else
			display.print(A, 1);
	}
	display.setCursor(h + 12);
	display.setFont(NIXI20);
	display.setFontKern(2);
	display.print("A");
}

void displayRPM(uint8_t x, uint8_t y, uint16_t rpm, bool invaliddata)
{
	display.setFont(GLCDFONT);
	display.setTextColor(Brightness);
	if (x>0 || y>0)	display.setCursor(x, y);
	display.print("RPM:");
	if (invaliddata) display.print("----");
	else display.print(rpm);
}

void displayPSI(uint8_t x, uint8_t y, uint16_t psi, bool invaliddata, String lable = "")
{
	display.setFont(GLCDFONT);
	display.setTextColor(Brightness);
	if (x>0 || y>0)	display.setCursor(x, y);
	if (lable.length()>0) display.print(lable);
	if (invaliddata) display.print("-- ");
	else display.print(psi);
	display.print("psi");
}

void displayTemp(uint8_t x, uint8_t y, float t, bool invaliddata, String lable = "")
{
	display.setFont(GLCDFONT);
	display.setTextColor(WHITE);
	if (x>0 || y>0)	display.setCursor(x, y);
	if (lable.length()>0) display.print(lable);
	if (invaliddata) display.print("--");
	else display.print(t, 0);
	display.print("F");
}

void displayTime(uint8_t x, uint8_t y, time_t t, String lable = "")
{
	display.setFont(GLCDFONT);
	display.setTextColor(WHITE);
	if (x>0 || y>0)	display.setCursor(x, y);
	if (lable.length()>0) display.print(lable);
	PrintDigits(hour(t));
	display.print(":");
	PrintDigits(minute(t));
	display.print(":");
	PrintDigits(second(t));
}


//*****************************************************************************
template<typename T> void PrintLabelValWithConversionCheckUnDef(const char* label, T val, double(*ConvFunc)(double val) = 0, bool AddLf = false) {
	OutputStream->print(label);
	if (!N2kIsNA(val)) {
		if (ConvFunc) { OutputStream->print(ConvFunc(val)); }
		else { OutputStream->print(val); }
	}
	else OutputStream->print("not available");
	if (AddLf) OutputStream->println();
}

// EEPROM *****************************************************************************
//Load From EEPROM 
void ReadConfig() {
	EEPROM.begin("config");
	EEPROM.getBytes("config", &config, sizeof(config)); //ESP32 NVS Preferences
	//EEPROM.get(EEPROM_ADDR_CONFIG, config); // Teensy EEPROM
}

//Write to EEPROM - Teensy non-volatile area size is 2048 bytes  100,000 cycles
void UpdateConfig() {
	Blink(5, 2000);
	Serial.println("Updating config");
	EEPROM.putBytes("config", &config, sizeof(config)); //ESP32 NVS Preferences
	//EEPROM.put(EEPROM_ADDR_CONFIG, config);  //Teensy EEPROM
}

// Load default config into EEPROM
void InitializeEEPROM() {
	Serial.println("Initialize EEPROM");
	config = defConfig;
	UpdateConfig();
}

// Debug print **************
void printDebug() {
	Serial.print("Voltage: ");
	Serial.print(volts, 4);
	Serial.print("Current: ");
	Serial.print(amps, 4);
	Serial.print("V Rel: ");
	Serial.print(" ");
}

// N2K message parsing
void SystemTime(const tN2kMsg &N2kMsg) {
	unsigned char SID;
	uint16_t SystemDate;
	double SystemTime;
	tN2kTimeSource TimeSource;
	unsigned long time = 0L;
	const unsigned long DEFAULT_TIME = 1357041600; // Jan 1 2013 
	if (ParseN2kSystemTime(N2kMsg, SID, SystemDate, SystemTime, TimeSource)) {
		time = SystemDate * 3600 * 24 + SystemTime;
		timeDataValid = 0;
		if (time < DEFAULT_TIME) { // check the value is a valid time (greater than Jan 1 2013)
			time = 0L; // return to 0 indicate that the time is not valid
		}
		if (time) {
			NetTime = time;
			setTime(NetTime);
		}
		else {
			// time is now off by 60s so invalidate nettime
			if (abs(NetTime - now()) > 60) NetTime = 0;
		}
	}
	else {
		OutputStream->print(F("Failed to parse SystemTime PGN: ")); OutputStream->println(N2kMsg.PGN);
	}
}

void EngineRapid(const tN2kMsg &N2kMsg) {
	unsigned char EngineInstance;
	double EngineSpeed;
	double EngineBoostPressure;
	int8_t EngineTiltTrim;

	if (ParseN2kEngineParamRapid(N2kMsg, EngineInstance, EngineSpeed, EngineBoostPressure, EngineTiltTrim)) {
		if (EngineInstance == config.engineInstance) {
			rpm = EngineSpeed;
			engineDataValid = 0;
		}
		else {
			if (engineDataValid > 60 && AUTOIDDEVICES) {
				config.engineInstance = EngineInstance;
				UpdateConfig();
			}
		}
		if (SDEBUG) {
			PrintLabelValWithConversionCheckUnDef("Engine rapid params: ", EngineInstance, 0, true);
			PrintLabelValWithConversionCheckUnDef("  RPM: ", EngineSpeed, 0, true);
			PrintLabelValWithConversionCheckUnDef("  boost pressure (Pa): ", EngineBoostPressure, 0, true);
			PrintLabelValWithConversionCheckUnDef("  tilt trim: ", EngineTiltTrim, 0, true);
		}
	}
	else {
		OutputStream->print("Failed to parse PGN: "); OutputStream->println(N2kMsg.PGN);
	}
}

inline double PascalToPSI(double v) { return v * 0.000145038; }

void EngineDynamicParameters(const tN2kMsg &N2kMsg) {
	unsigned char EngineInstance;
	double EngineOilPress;
	double EngineOilTemp;
	double EngineCoolantTemp;
	double AltenatorVoltage;
	double FuelRate;
	double EngineHours;
	double EngineCoolantPress;
	double EngineFuelPress;
	int8_t EngineLoad;
	int8_t EngineTorque;

	if (ParseN2kEngineDynamicParam(N2kMsg, EngineInstance, EngineOilPress, EngineOilTemp, EngineCoolantTemp,
		AltenatorVoltage, FuelRate, EngineHours,
		EngineCoolantPress, EngineFuelPress,
		EngineLoad, EngineTorque)) {
		oilPressure = PascalToPSI(EngineOilPress);
		oilTemp = KelvinToF(EngineOilTemp);
		engineDataValid = 0;
		if (SDEBUG) {
			PrintLabelValWithConversionCheckUnDef("Engine dynamic params: ", EngineInstance, 0, true);
			PrintLabelValWithConversionCheckUnDef("  oil pressure (Pa): ", EngineOilPress, 0, true);
			PrintLabelValWithConversionCheckUnDef("  oil temp (C): ", EngineOilTemp, &KelvinToC, true);
			PrintLabelValWithConversionCheckUnDef("  coolant temp (C): ", EngineCoolantTemp, &KelvinToC, true);
			PrintLabelValWithConversionCheckUnDef("  altenator voltage (V): ", AltenatorVoltage, 0, true);
			PrintLabelValWithConversionCheckUnDef("  fuel rate (l/h): ", FuelRate, 0, true);
			PrintLabelValWithConversionCheckUnDef("  engine hours (h): ", EngineHours, &SecondsToh, true);
			PrintLabelValWithConversionCheckUnDef("  coolant pressure (Pa): ", EngineCoolantPress, 0, true);
			PrintLabelValWithConversionCheckUnDef("  fuel pressure (Pa): ", EngineFuelPress, 0, true);
			PrintLabelValWithConversionCheckUnDef("  engine load (%): ", EngineLoad, 0, true);
			PrintLabelValWithConversionCheckUnDef("  engine torque (%): ", EngineTorque, 0, true);
		}
	}
	else {
		OutputStream->print("Failed to parse PGN: "); OutputStream->println(N2kMsg.PGN);
	}
}

void BatteryConfigurationStatus(const tN2kMsg &N2kMsg) {
	unsigned char BatInstance;
	tN2kBatType BatType;
	tN2kBatEqSupport SupportsEqual;
	tN2kBatNomVolt BatNominalVoltage;
	tN2kBatChem BatChemistry;
	double BatCapacity;
	int8_t BatTemperatureCoefficient;
	double PeukertExponent;
	int8_t ChargeEfficiencyFactor;

	if (ParseN2kBatConf(N2kMsg, BatInstance, BatType, SupportsEqual, BatNominalVoltage, BatChemistry, BatCapacity, BatTemperatureCoefficient, PeukertExponent, ChargeEfficiencyFactor)) {
		PrintLabelValWithConversionCheckUnDef("Battery instance: ", BatInstance, 0, true);
		OutputStream->print("  - type: "); PrintN2kEnumType(BatType, OutputStream);
		OutputStream->print("  - support equal.: "); PrintN2kEnumType(SupportsEqual, OutputStream);
		OutputStream->print("  - nominal voltage: "); PrintN2kEnumType(BatNominalVoltage, OutputStream);
		OutputStream->print("  - chemistry: "); PrintN2kEnumType(BatChemistry, OutputStream);
		PrintLabelValWithConversionCheckUnDef("  - capacity (Ah): ", BatCapacity, &CoulombToAh, true);
		PrintLabelValWithConversionCheckUnDef("  - temperature coefficient (%): ", BatTemperatureCoefficient, 0, true);
		PrintLabelValWithConversionCheckUnDef("  - peukert exponent: ", PeukertExponent, 0, true);
		PrintLabelValWithConversionCheckUnDef("  - charge efficiency factor (%): ", ChargeEfficiencyFactor, 0, true);
	}
	else {
		OutputStream->print("Failed to parse PGN: "); OutputStream->println(N2kMsg.PGN);
	}
}

void DCStatus(const tN2kMsg &N2kMsg) {
	unsigned char SID;
	unsigned char DCInstance;
	tN2kDCType DCType;
	unsigned char StateOfCharge;
	unsigned char StateOfHealth;
	double TimeRemaining;
	double RippleVoltage;

	if (ParseN2kDCStatus(N2kMsg, SID, DCInstance, DCType, StateOfCharge, StateOfHealth, TimeRemaining, RippleVoltage)) {
		OutputStream->print("DC instance: ");
		OutputStream->println(DCInstance);
		OutputStream->print("  - type: "); PrintN2kEnumType(DCType, OutputStream);
		OutputStream->print("  - state of charge (%): "); OutputStream->println(StateOfCharge);
		OutputStream->print("  - state of health (%): "); OutputStream->println(StateOfHealth);
		OutputStream->print("  - time remaining (h): "); OutputStream->println(TimeRemaining / 60);
		OutputStream->print("  - ripple voltage: "); OutputStream->println(RippleVoltage);
	}
	else {
		OutputStream->print("Failed to parse PGN: ");  OutputStream->println(N2kMsg.PGN);
	}
}

void DCBatStatus(const tN2kMsg &N2kMsg) {
	unsigned char SID;
	unsigned char BatteryInstance;
	double voltage;
	double amperes;
	double tempK;
	if (ParseN2kDCBatStatus(N2kMsg, BatteryInstance, voltage, amperes, tempK, SID))
	{
		if (SDEBUG) {
			OutputStream->print("Battery Instance: ");
			OutputStream->println(BatteryInstance);
			OutputStream->print("  - voltage: "); OutputStream->print(voltage);
			OutputStream->print("V  - amperes: "); OutputStream->print(amperes);
			OutputStream->print("A  - temp: "); OutputStream->print(tempK);
			OutputStream->print("K");
		}
		if (BatteryInstance == config.batteryInstance)
		{
			volts = voltage;
			amps = amperes;
			batteryDataValid = 0;
		}
		else {
			if (batteryDataValid > 60 && AUTOIDDEVICES) {
				config.batteryInstance = BatteryInstance;
				UpdateConfig();
			}
		}
	}
}

//*****************************************************************************
// LED blinker count flashes in duration ms
void Blink(int count, unsigned long duration) {
	unsigned long d = duration / count;
	for (int counter = 0; counter < count; counter++) {
		digitalWrite(LED_BUILTIN, HIGH);
		delay(d / 2);
		digitalWrite(LED_BUILTIN, LOW);
		delay(d / 2);
	}
}

// read serial input
char getCommand()
{
	char c = '\0';
	if (Serial.available())
	{
		c = Serial.read();
	}
	return c;
}

// debug voltage simulation 
void vsim() {
	volts = volts + ((random(10) - 5.0) / 10);
	if (volts > 16.0) volts = 16.0;
	if (volts < 10.5) volts = 10.5;

	amps = amps + ((random(8) - 3.5) / 2.0);
	if (amps < -140) amps = -140;
	if (amps > 40) amps = 40;
}

String printDigits(int digits) {

	if (digits < 10)
		return '0' + String(digits);
	else
		return String(digits);
}

String DateTimeString(time_t t) {
	return String(year(t)) + "-" + String(month(t)) + "-" + String(day(t))
		+ " " + printDigits(hour(t)) + ":" + printDigits(minute(t)) + ":" + printDigits(second(t));
}

// pad zeros for time format
void PrintDigits(int digits) {
	if (digits < 10)
		display.print('0');
	display.print(digits);
}

// print datetime
void PrintDateTime(time_t t) {
	display.print(year(t));
	display.print("-");
	display.print(month(t));
	display.print("-");
	display.print(day(t));
	display.print(" ");
	PrintDigits(hour(t));
	display.print(":");
	PrintDigits(minute(t));
	display.print(":");
	PrintDigits(second(t));
	display.print(" GMT ");
}

void PrintDate(time_t t) {
	display.print(" ");
	PrintDigits(month(t));
	display.print("/");
	PrintDigits(day(t));
	display.print("/");
	display.print(year(t) - 2000);
}
void PrintTime(time_t t) {
	display.print("  Time ");
	PrintDigits(hour(t));
	display.print(":");
	PrintDigits(minute(t));
	display.print(":");
	PrintDigits(second(t));
	//	display.print(" EST");
}

void showMessage(String msg) {
	display.setFont(GLCDFONT);
	display.setTextColor(Brightness);

	display.setCursor(0, 0);
	display.clearDisplay();
	for (int a = 1; a < 16; a++) {
		display.setTextColor(a);
		display.print(" ");
		display.print(a);
		display.print(".");
		for (int i = 0; i < msg.length(); i++) {

			display.print(msg[i]);
			display.display();
			delay(10);
		}
		delay(100);
	}
	delay(5000);
}

String msgbuffer[8];
uint8_t msgIndex = 0;
void newMsg(String msg)
{
	//display is 42 chars wide with GLCDFONT
	msgbuffer[msgIndex] = msg.substring(0, 40);
	msgIndex++;
	if (msgIndex > 7) msgIndex = 0;
	msgToDisplay();
}

void clearMsgBuffer()
{
	for (int i = 0; i < 8; i++) {
		msgbuffer[i] = "";
	}
}

// vertical scroll messages
void msgToDisplay()
{
	display.clearDisplay();
	display.setCursor(0, 0);
	uint8_t start = msgIndex;
	for (int i = start; i < 8; i++) {
		display.println(msgbuffer[i]);
	}
	for (int i = 0; i < start; i++) {
		display.println(msgbuffer[i]);
	}
	display.display();
}

void SetMsg(String text)
{
	msg = "                                       " + text;
}

void horizontalScroll()
{
	display.setCursor(0, 42);
	display.setFont(GLCDFONT);
	display.setTextColor(Brightness);
	char c;
	uint8_t out = 0;
	uint8_t len = msg.length();
	if (len < 40) msg += " ";
	for (int x = startX; x < startX + 40; x++) {
		display.print(msg[x]);
		out++;
	}
	if (out < 39) {
		display.print(" ");
		for (int x = out; x < 40; x++) {
			display.print(msg[x - out]);
		}
	}
	startX++;
	if (startX > len) startX = 0;
}

