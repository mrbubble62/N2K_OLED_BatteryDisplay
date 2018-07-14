# N2K_OLED_BatteryDisplay

Displays Voltage and current for battery instance (stored in eeprom)

Also displays engine RPM, Oil pressure and temperature, time in EST sync from GPS

## Parts list:

* ESP32 (Lolin NANO32)
* SDD1132 256x64 Graphic OLED Display
* KSZ-3R33S 3.3v DC-DC 
* 100uF 50v capacitor
* CAN transceiver

## Pins
Display 4-Wire Serial SPI 
| OLED | ESP32 |
|-----|----|
|RES|19|
|CS|17|
|DC|5|
|SCL|18|
|SDA|23|
|GND | 0v|
|VCC | 3.3v|

| CAN Tranceiver | ESP32 |
|-----|----|
|CTX|14|
|CRX|12|

