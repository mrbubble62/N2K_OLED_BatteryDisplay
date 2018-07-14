#define ARDUINO_ARCH_ESP32 
#define EEPROM_ADDR_CONFIG 0 // starting eeprom address for config params
#define ESP32_CAN_TX_PIN GPIO_NUM_14
#define ESP32_CAN_RX_PIN GPIO_NUM_12

#include <Arduino.h>
#include <NMEA2000_CAN.h>
#include <N2kMessages.h>
#include <N2kMessagesEnumToStr.h>
#include <SPI.h>
#include <string.h>
#include <EEPROM.h>
#include <Time.h>
#include <TimeLib.h>
#include <print.h>
#include "fonts.h"
#include "ESP8266_SSD1322.h"

unsigned long lastTime = millis();
unsigned long timePassed;
byte i = 0;


// 23 SDA MOSI
// 18 CLK
#define OLED_CS     17  // Pin 10, CS - Chip select
#define OLED_DC     5  // Pin 9 - DC digital signal
#define OLED_RESET  19   // using hardware !RESET from Arduino instead

//hardware SPI - only way to go. Can get 110 FPS
ESP8266_SSD1322 display(OLED_DC, OLED_RESET, OLED_CS);

time_t NetTime = 0;  //NMEA network time is invalid
// convert N2K days since1970 to unix time seconds since 1970
time_t N2KDate(uint16_t DaysSince1970) {
	return DaysSince1970 * 24 * 3600;
}

// EEPROM **********************
// EEPROM configuration structure
#define MAGIC 12348 // EPROM struct version check, change this whenever tConfig structure changes
struct tConfig {
	uint16_t Magic; //test if eeprom initialized
	double Zero;
	double Gain;
	uint8_t batteryInstance;
	uint8_t deviceInstance;
	uint8_t sourceAddr;
};

// EEPROM contents
tConfig config;

// Default EEPROM contents
const tConfig defConfig PROGMEM = {
	MAGIC,
	0, // zero
	1,  // unity gain
	0, // battery
	0,	// deviceInstance
	6 // sourceAddr
};

Stream *OutputStream;

//*****************************************************************************
tN2kMsg N2kMsg; // N2K message constructor
int SID = 1;

// 	Received Messages			
typedef struct {
	unsigned long PGN;
	void(*Handler)(const tN2kMsg &N2kMsg);
} tNMEA2000Handler;

const unsigned long ReceiveMessage[] PROGMEM = { 126992L,127488L,127489L,127508L,130312L,0 }; //systemtime,EngineRapid,EngineDynamicParameters,DCBatStatus,Temperature,Pressure
const unsigned long TransmitMessages[] PROGMEM = { 127508L, 0 }; //Battery DC State

void SystemTime(const tN2kMsg &N2kMsg);
void EngineRapid(const tN2kMsg &N2kMsg);
void EngineDynamicParameters(const tN2kMsg &N2kMsg);
void DCBatStatus(const tN2kMsg &N2kMsg);
void BatteryConfigurationStatus(const tN2kMsg &N2kMsg);
void Temperature(const tN2kMsg &N2kMsg);

tNMEA2000Handler NMEA2000Handlers[] = {
	{ 126992L,&SystemTime },
	{ 127488L,&EngineRapid },
	{ 127489L,&EngineDynamicParameters },
	{ 127508L,&DCBatStatus },
	{ 130312L,&Temperature },
	{ 0,0 }
};

//NMEA 2000 message handler
void HandleNMEA2000Msg(const tN2kMsg &N2kMsg) {
	int iHandler;
	// Find handler
	//OutputStream->print("In Main Handler: "); OutputStream->println(N2kMsg.PGN);
	for (iHandler = 0; NMEA2000Handlers[iHandler].PGN != 0 && !(N2kMsg.PGN == NMEA2000Handlers[iHandler].PGN); iHandler++);
	if (NMEA2000Handlers[iHandler].PGN != 0) {
		NMEA2000Handlers[iHandler].Handler(N2kMsg);
	}
}

void setup() {
	Serial.begin(115200);
	Serial.println("STARTING");
	OutputStream = &Serial;

	// Initialize display and perform reset
	display.begin(true);
	// init done
	display.Set_Gray_Scale_Table();
	display.clearDisplay();
	//display.print("OK");
	display.display();
	//display.clearDisplay();
	//delay(500);

	NMEA2000.EnableForward(false);
	NMEA2000.SetProductInformation("07062018", // Manufacturer's Model serial code
		666, // Manufacturer's product code
		"Battery Display",  // Manufacturer's Model ID
		"1.0.0.1 (2015-08-14)",  // Manufacturer's Software version code
		"1.0.0.0 (2015-08-14)" // Manufacturer's Model version
	);
	//// Det device information
	//NMEA2000.SetHeartbeatInterval(config.HeartbeatInterval);
	NMEA2000.SetDeviceInformation(70618, // Unique number. Use e.g. Serial number.
		130, // Device function=Temperature See codes on http://www.nmea.org/Assets/20120726%20nmea%202000%20class%20%26%20function%20codes%20v%202.00.pdf
		120, // Device class=Sensor Communication Interface. See codes on http://www.nmea.org/Assets/20120726%20nmea%202000%20class%20%26%20function%20codes%20v%202.00.pdf
		2040 // Just choosen free from code list on http://www.nmea.org/Assets/20121020%20nmea%202000%20registration%20list.pdf                               
	);
	NMEA2000.SetMode(tNMEA2000::N2km_ListenAndNode, config.sourceAddr);
	NMEA2000.ExtendTransmitMessages(TransmitMessages);
	NMEA2000.ExtendReceiveMessages(ReceiveMessage);
	NMEA2000.SetN2kCANMsgBufSize(5);
	NMEA2000.SetMsgHandler(HandleNMEA2000Msg);
	NMEA2000.Open();
	//delay(2000);

}

float volts = 12.7;
float amps = 0;
float V = 12.7;
float A = 0;
uint16_t rpm;
float oilTemp;
float oilPressure;
float waterTemp;

// check voltage thresholds, change according to battery chemistry
void vsim() {
	volts = volts + ((random(10) - 5.0) / 10);
	if (volts > 16.0) volts = 16.0;
	if (volts < 10.5) volts = 10.5;

	amps = amps + ((random(8) - 3.5) / 2.0);
	if (amps < -140) amps = -140;
	if (amps > 40) amps = 40;
}


uint16_t g = 0;

void displayVA(uint8_t h, float A, float V)
{
	display.setFont(NIXI);
	display.setFontKern(-5);
	display.setTextColor(WHITE);
	display.setCursor(0, h);
	display.print(V, 1);
	display.setCursor(85, h + 12);
	display.setFont(NIXI20);
	display.print("V");
	display.setFont(NIXI);
	display.setFontKern(0);
	if (A < 0)
		display.setCursor(115, h);
	else
		display.setCursor(130, h);
	if (abs(A) > 99.99)
		display.print(A, 0);
	else if (abs(A) > 9.99)
		display.print(A, 1);
	else
		display.print(A, 2);
	display.setCursor(h + 12);
	display.setFont(NIXI20);
	display.setFontKern(2);
	display.print("A");
}
void displayRPM(uint8_t x, uint8_t y, uint16_t rpm)
{
	display.setFont(GLCDFONT);
	display.setTextColor(WHITE);
	display.setCursor(x, y);
	display.print("RPM ");
	display.print(rpm);
}
void displayPSI(uint8_t x, uint8_t y, uint16_t psi, String lable = "")
{
	display.setFont(GLCDFONT);
	display.setTextColor(WHITE);
	display.setCursor(x, y);
	if (lable.length()>0) display.print(lable);
	display.print(psi);
	display.print("psi");
}
void displayTemp(uint8_t x, uint8_t y, float t, String lable = "")
{
	display.setFont(GLCDFONT);
	display.setTextColor(WHITE);
	display.setCursor(x, y);
	if (lable.length()>0) display.print(lable);
	display.print(t, 0);
	display.print("*F");
}

long timer1;
bool blink;
// loop controls
uint32_t delt_t = 0; // 250 ms loop
uint32_t count = 0; // 250 ms loop
long slowloop = 0; // 1s loop
				   // USB serial commands
bool SDEBUG = true; // toggle debug print
bool SDATA = false; // toggle data print

void loop() {
	char command = getCommand();
	switch (command) {
	case 'd':
		SDEBUG = !SDEBUG;
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
		
		//draw new values and if voltage < 11.60 and discharing then blink
		if (V < 11.60 && A < 0) {
			if (timer1 > 1000) { timer1 = 0; blink = !blink; }
		}
		else if (V > 15.60 && A > 0) {
			if (timer1 > 1000) { timer1 = 0; blink = !blink; }
		}
		else {
			blink = false;
		}
		//**** Update Display *****
		//blink if 
		if (!blink) displayVA(5, A, V);
		//
		displayRPM(3, 52, rpm);
		displayTemp(62, 52, oilTemp, "Eng:");
		displayPSI(130, 52, oilPressure, "Oil:");
		displayTemp(195, 52,waterTemp, "Water:");
		display.display();
		//erase
		display.clearDisplay();
		//****** end display  ********
		// debug
		if (SDEBUG) { amps = amps + 0.01; }
		
		SetN2kDCBatStatus(N2kMsg, 1, 12.5, 1.511, 243.21, SID);
		NMEA2000.SendMsg(N2kMsg);

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
	EEPROM.get(EEPROM_ADDR_CONFIG, config);
}

//Write to EEPROM - Teensy non-volatile area size is 2048 bytes  100,000 cycles
void UpdateConfig() {
	Blink(5, 2000);
	Serial.println("Updating config");
	config.Magic = MAGIC;
	EEPROM.put(EEPROM_ADDR_CONFIG, config);
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
	Serial.print(volts,4);
	Serial.print("Current: ");
	Serial.print(amps, 4);
	Serial.print("V Rel: ");
	Serial.print(" ");
}

void PrintConfig() {
	Serial.print("Center zero offset: ");
	//Serial.print(offsetVoltage);
	Serial.print("V\n");
	Serial.print("Gain: ");
	//Serial.print(gain, 6);
	Serial.println("\n");
}

void PrintHelp()
{
	Serial.println(F("Mr Bubble Rudder Reference"));
	Serial.println(F("Commands: \"p\"=show data, \"d\"=show ADC voltages, \"z\"=save current position as zero reference, \"+\"=ingrease gain,\"-\"=decrease gain, \"w\"=save gain setting to EEPROM"));
	Serial.println(F("Alignment: Center the wheel and press 'z' to align the rudder reference, turn to 90 degrees and use the + and - keys to adjust gain"));
}


void SystemTime(const tN2kMsg &N2kMsg) {
	unsigned char SID;
	uint16_t SystemDate;
	double SystemTime;
	tN2kTimeSource TimeSource;
	unsigned long time = 0L;
	const unsigned long DEFAULT_TIME = 1357041600; // Jan 1 2013 
	if (ParseN2kSystemTime(N2kMsg, SID, SystemDate, SystemTime, TimeSource)) {
		time = SystemDate * 3600 * 24 + SystemTime;
		if (time < DEFAULT_TIME) { // check the value is a valid time (greater than Jan 1 2013)
			time = 0L; // return 0 to indicate that the time is not valid
		}
		if (time) {
			NetTime = time;
			setTime(NetTime);
			//setTime(t);
			//digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN));
		}
		else {
			// time is off by 60s so invalidate nettime
			if (abs(NetTime - now()) > 60) NetTime = 0;
		}
		// serial print time
	/*	if (STIME) {
			Serial.print("Last Network time:"); PrintDateTime(NetTime); PrintN2kEnumType(TimeSource, OutputStream);
			Serial.print("\nCurrent local time:"); PrintDateTime(now());
		}*/
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
		rpm = EngineSpeed;
	//	PrintLabelValWithConversionCheckUnDef("Engine rapid params: ", EngineInstance, 0, true);
		PrintLabelValWithConversionCheckUnDef("  RPM: ", EngineSpeed, 0, true);
	//	PrintLabelValWithConversionCheckUnDef("  boost pressure (Pa): ", EngineBoostPressure, 0, true);
	//	PrintLabelValWithConversionCheckUnDef("  tilt trim: ", EngineTiltTrim, 0, true);
	}
	else {
		OutputStream->print("Failed to parse PGN: "); OutputStream->println(N2kMsg.PGN);
	}
}
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
		oilPressure = EngineOilPress;
		PrintLabelValWithConversionCheckUnDef("Engine dynamic params: ", EngineInstance, 0, true);
		PrintLabelValWithConversionCheckUnDef("  oil pressure (Pa): ", EngineOilPress, 0, true);
		PrintLabelValWithConversionCheckUnDef("  oil temp (C): ", EngineOilTemp, &KelvinToC, true);
		PrintLabelValWithConversionCheckUnDef("  coolant temp (C): ", EngineCoolantTemp, &KelvinToC, true);
		PrintLabelValWithConversionCheckUnDef("  altenator voltage (V): ", AltenatorVoltage, 0, true);
		PrintLabelValWithConversionCheckUnDef("  fuel rate (l/h): ", FuelRate, 0, true);
		PrintLabelValWithConversionCheckUnDef("  engine hours (h): ", EngineHours, &SecondsToh, true);
		//PrintLabelValWithConversionCheckUnDef("  coolant pressure (Pa): ", EngineCoolantPress, 0, true);
		PrintLabelValWithConversionCheckUnDef("  fuel pressure (Pa): ", EngineFuelPress, 0, true);
		//PrintLabelValWithConversionCheckUnDef("  engine load (%): ", EngineLoad, 0, true);
		//PrintLabelValWithConversionCheckUnDef("  engine torque (%): ", EngineTorque, 0, true);
	}
	else {
		OutputStream->print("Failed to parse PGN: "); OutputStream->println(N2kMsg.PGN);
	}
}

void Temperature(const tN2kMsg &N2kMsg) {
	unsigned char SID;
	unsigned char TempInstance;
	tN2kTempSource TempSource;
	double ActualTemperature;
	double SetTemperature;

	if (ParseN2kTemperature(N2kMsg, SID, TempInstance, TempSource, ActualTemperature, SetTemperature)) {
		OutputStream->print("Temperature source: "); PrintN2kEnumType(TempSource, OutputStream, false);
		PrintLabelValWithConversionCheckUnDef(", actual temperature: ", ActualTemperature, &KelvinToC);
		PrintLabelValWithConversionCheckUnDef(", set temperature: ", SetTemperature, &KelvinToC, true);
	}
	else {
		OutputStream->print("Failed to parse PGN: ");  OutputStream->println(N2kMsg.PGN);
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
	if (ParseN2kDCBatStatus(N2kMsg, BatteryInstance, voltage, amperes, tempK,SID))
	{
		volts = voltage;
		amps = amperes;
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

// pad zeros for time format
void PrintDigits(int digits) {
	if (digits < 10)
		display.print('0');
	display.print(digits);
}
// print datetime
void PrintDateTime(time_t t) {
	
	// display the given time
	display.print(" ");
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