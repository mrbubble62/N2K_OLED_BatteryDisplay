/**
 * This is an example for the Newhaven NHD-3.12-25664UCY2 OLED based on SSD1322 drivers
 * The NHD-3.12-25664UCY2 is sold through Digikey and Mouser
 *
 * Details in
 *   data sheet (http://www.newhavendisplay.com/specs/NHD-3.12-25664UCY2.pdf)
 *   app note (http://www.newhavendisplay.com/app_notes/SSD1322.pdf)
 *
 * Based on Adafruit SSD1306 driver (https://github.com/adafruit/Adafruit_SSD1306)
 *   for which the original header is left below:
 */

/*********************************************************************
This is a library for the 256 x 64 pixel 16 color gray scale OLEDs
based on SSD1322 drivers

These displays use SPI to communicate, 4 or 5 pins are required to
interface

Adafruit invests time and resources providing this open source code,
please support Adafruit and open-source hardware by purchasing
products from Adafruit!

Written by Limor Fried/Ladyada  for Adafruit Industries.
BSD license, check license.txt for more information
All text above, and the splash screen must be included in any redistribution
*********************************************************************/

#include <cstdint>

#ifdef __AVR__
#include <avr/pgmspace.h>
#elif defined(ESP8266) || defined(ESP32)
#include <pgmspace.h>
#endif

#if !defined(__SAM3X8E__) &&  !defined(ESP8266) && !defined(ARDUINO_ARCH_ARC32)  && !defined(ARDUINO_ARCH_ESP32)
//#include <util/delay.h>
#endif
#include <stdlib.h>
#include "Adafruit_mfGFX.h"
#include "ESP8266_SSD1322.h"

#ifndef _swap_int16_t
#define _swap_int16_t(a, b) { int16_t t = a; a = b; b = t; }
#endif

typedef unsigned char byte;

// the memory buffer for the LCD
static uint8_t buffer[SSD1322_LCDHEIGHT * SSD1322_LCDWIDTH / (8 / SSD1322_BITS_PER_PIXEL)] = { 0x00 };

// the most basic function, set a single pixel
void ESP8266_SSD1322::drawPixel(int16_t x, int16_t y, uint16_t gscale)
{
  // check rotation, move pixel around if necessary
  switch (getRotation())
  {
    case 1:
      _swap_int16_t(x, y);
      x = WIDTH - x - 1;
    break;
    case 2:
      x = WIDTH - x - 1;
      y = HEIGHT - y - 1;
    break;
    case 3:
      _swap_int16_t(x, y);
      y = HEIGHT - y - 1;
    break;
  }

  if ((x < 0) || (x >= width()) || (y < 0) || (y >= height()))
    return;
	register uint8_t mask = ((x % 2) ? gscale : gscale << 4);
	register uint8_t *pBuf = &buffer[(x >> 1) + (y * (SSD1322_LCDWIDTH / 2))];
	register uint8_t b1 = *pBuf;
	b1 &= (x % 2) ? 0xF0 : 0x0F; // cleardown nibble to be replaced
	// write our value in
	*pBuf++ = b1 | mask;
}


// constructor for hardware SPI - we indicate DataCommand, ChipSelect, Reset
ESP8266_SSD1322::ESP8266_SSD1322(int8_t DC, int8_t RST, int8_t CS) : Adafruit_GFX(SSD1322_LCDWIDTH, SSD1322_LCDHEIGHT) {
	dc = DC;
	rst = RST;
	cs = CS;
	hwSPI = true;
}

void ESP8266_SSD1322::begin(bool reset) {
	// set pin directions
	pinMode(dc, OUTPUT);
	pinMode(cs, OUTPUT);
	if (hwSPI) {
		SPI.begin();
		SPI.setClockDivider (SPI_CLOCK_DIV2); // 26/2 = 13 MHz (freq ESP8266 26 MHz)
	}

	if (reset && rst)
	{
		// Setup reset pin direction
		pinMode(rst, OUTPUT);
		// bring out of reset
		digitalWrite(rst, HIGH);
		delay(100);
		// bring reset low
		digitalWrite(rst, LOW);
		delay(400);
		// bring out of reset
		digitalWrite(rst, HIGH);
	}

	ssd1322_command(SSD1322_SETCOMMANDLOCK);// 0xFD
	ssd1322_data(0x12);// Unlock OLED driver IC

	ssd1322_command(SSD1322_DISPLAYOFF);// 0xAE

	ssd1322_command(SSD1322_SETCLOCKDIVIDER);// 0xB3
	ssd1322_data(0x91);// 0xB3

	ssd1322_command(SSD1322_SETMUXRATIO);// 0xCA
	ssd1322_data(0x3F);// duty = 1/64

	ssd1322_command(SSD1322_SETDISPLAYOFFSET);// 0xA2
	ssd1322_data(0x00);

	ssd1322_command(SSD1322_SETSTARTLINE);// 0xA1
	ssd1322_data(0x00);

	ssd1322_command(SSD1322_SETREMAP);// 0xA0
	ssd1322_data(0x14);//Horizontal address increment,Disable Column Address Re-map,Enable Nibble Re-map,Scan from COM[N-1] to COM0,Disable COM Split Odd Even
	ssd1322_data(0x11);//Enable Dual COM mode

	ssd1322_command(SSD1322_SETGPIO);// 0xB5
	ssd1322_data(0x00);// Disable GPIO Pins Input

	ssd1322_command(SSD1322_FUNCTIONSEL);// 0xAB
	ssd1322_data(0x01);// selection external vdd

	ssd1322_command(SSD1322_DISPLAYENHANCE);// 0xB4
	ssd1322_data(0xA0);// enables the external VSL
	ssd1322_data(0xFD);// 0xfFD,Enhanced low GS display quality;default is 0xb5(normal),

	ssd1322_command(SSD1322_SETCONTRASTCURRENT);// 0xC1
	ssd1322_data(0xFF);// 0xFF - default is 0x7f

	ssd1322_command(SSD1322_MASTERCURRENTCONTROL);// 0xC7
	ssd1322_data(0x0F);// default is 0x0F

	// Set grayscale
	ssd1322_command(SSD1322_SELECTDEFAULTGRAYSCALE); // 0xB9

 	ssd1322_command(SSD1322_SETPHASELENGTH);// 0xB1
	ssd1322_data(0xE2);// default is 0x74

	ssd1322_command(SSD1322_DISPLAYENHANCEB);// 0xD1
	ssd1322_data(0x82);// Reserved;default is 0xa2(normal)
	ssd1322_data(0x20);//

	ssd1322_command(SSD1322_SETPRECHARGEVOLTAGE);// 0xBB
	ssd1322_data(0x1F);// 0.6xVcc

	ssd1322_command(SSD1322_SETSECONDPRECHARGEPERIOD);// 0xB6
	ssd1322_data(0x08);// default

	ssd1322_command(SSD1322_SETVCOMH);// 0xBE
	ssd1322_data(0x07);// 0.86xVcc;default is 0x04

	ssd1322_command(SSD1322_NORMALDISPLAY);// 0xA6

	ssd1322_command(SSD1322_EXITPARTIALDISPLAY);// 0xA9

	//Clear down image ram before opening display
	fill(0x00);

	ssd1322_command(SSD1322_DISPLAYON);// 0xAF
}

void ESP8266_SSD1322::invertDisplay(uint8_t i) {
	if (i) {
		ssd1322_command(SSD1322_INVERSEDISPLAY);
	} else {
		ssd1322_command(SSD1322_NORMALDISPLAY);
	}
}

// startscrollright
// Activate a right handed scroll for rows start through stop
// Hint, the display is 16 rows tall. To scroll the whole display, run:
// display.scrollright(0x00, 0x0F)
void ESP8266_SSD1322::startscrollright(uint8_t start, uint8_t stop) {
	ssd1322_command(SSD1322_RIGHT_HORIZONTAL_SCROLL);
	ssd1322_command(0X00);
	ssd1322_command(start);
	ssd1322_command(0X00);
	ssd1322_command(stop);
	ssd1322_command(0X00);
	ssd1322_command(0XFF);
	ssd1322_command(SSD1322_ACTIVATE_SCROLL);
}

// startscrollleft
// Activate a right handed scroll for rows start through stop
// Hint, the display is 16 rows tall. To scroll the whole display, run:
// display.scrollright(0x00, 0x0F)
void ESP8266_SSD1322::startscrollleft(uint8_t start, uint8_t stop) {
	ssd1322_command(SSD1322_LEFT_HORIZONTAL_SCROLL);
	ssd1322_command(0X00);
	ssd1322_command(start);
	ssd1322_command(0X00);
	ssd1322_command(stop);
	ssd1322_command(0X00);
	ssd1322_command(0XFF);
	ssd1322_command(SSD1322_ACTIVATE_SCROLL);
}

// startscrolldiagright
// Activate a diagonal scroll for rows start through stop
// Hint, the display is 16 rows tall. To scroll the whole display, run:
// display.scrollright(0x00, 0x0F)
void ESP8266_SSD1322::startscrolldiagright(uint8_t start, uint8_t stop) {
	ssd1322_command(SSD1322_SET_VERTICAL_SCROLL_AREA);
	ssd1322_command(0X00);
	ssd1322_command(SSD1322_LCDHEIGHT);
	ssd1322_command(SSD1322_VERTICAL_AND_RIGHT_HORIZONTAL_SCROLL);
	ssd1322_command(0X00);
	ssd1322_command(start);
	ssd1322_command(0X00);
	ssd1322_command(stop);
	ssd1322_command(0X01);
	ssd1322_command(SSD1322_ACTIVATE_SCROLL);
}

// startscrolldiagleft
// Activate a diagonal scroll for rows start through stop
// Hint, the display is 16 rows tall. To scroll the whole display, run:
// display.scrollright(0x00, 0x0F)
void ESP8266_SSD1322::startscrolldiagleft(uint8_t start, uint8_t stop) {
	ssd1322_command(SSD1322_SET_VERTICAL_SCROLL_AREA);
	ssd1322_command(0X00);
	ssd1322_command(SSD1322_LCDHEIGHT);
	ssd1322_command(SSD1322_VERTICAL_AND_LEFT_HORIZONTAL_SCROLL);
	ssd1322_command(0X00);
	ssd1322_command(start);
	ssd1322_command(0X00);
	ssd1322_command(stop);
	ssd1322_command(0X01);
	ssd1322_command(SSD1322_ACTIVATE_SCROLL);
}

void ESP8266_SSD1322::stopscroll(void) {
	ssd1322_command(SSD1322_DEACTIVATE_SCROLL);
}

// Dim the display
// dim = true: display is dimmed
// dim = false: display is normal
void ESP8266_SSD1322::dim(bool dim) {
	uint8_t contrast;

	if (dim) {
		contrast = 0; // Dimmed display
	}
	else {
		contrast = 0xCF;
	}
	/*else {
		if (_vccstate == SSD1322_EXTERNALVCC) {
			contrast = 0x9F;
		} else {
			contrast = 0xCF;
		}
	}*/
	// the range of contrast to too small to be really useful
	// it is useful to dim the display
	ssd1322_command(SSD1322_SETCONTRASTCURRENT);
	ssd1322_command(contrast);
}

void ESP8266_SSD1322::ssd1322_command(uint8_t c) {
	// SPI
	digitalWrite(cs, HIGH);
	digitalWrite(dc, LOW);
	digitalWrite(cs, LOW);
	fastSPIwrite(c);
	digitalWrite(cs, HIGH);
}

void ESP8266_SSD1322::ssd1322_data(uint8_t c) {
	digitalWrite(cs, HIGH);
	digitalWrite(dc, HIGH);
	digitalWrite(cs, LOW);
	fastSPIwrite(c);
	digitalWrite(cs, HIGH);
}

void ESP8266_SSD1322::ssd1322_dataBytes(uint8_t *buf, uint32_t size) {
	digitalWrite(cs, HIGH);
	digitalWrite(dc, HIGH);
	digitalWrite(cs, LOW);
	fastSPIwriteBytes(buf, size);
	digitalWrite(cs, HIGH);
}

void ESP8266_SSD1322::display() {

    ssd1322_command(SSD1322_SETCOLUMNADDR);
    ssd1322_data(MIN_SEG);
    ssd1322_data(MAX_SEG);

    ssd1322_command(SSD1322_SETROWADDR);
    ssd1322_data(0);
    ssd1322_data(63);

    ssd1322_command(SSD1322_WRITERAM);

    register uint16_t bufSize = (SSD1322_LCDHEIGHT * SSD1322_LCDWIDTH / (8 / SSD1322_BITS_PER_PIXEL)); // bytes
	register uint8_t *pBuf = buffer;

	// Write as quick as possible 64 bits at a time
	ssd1322_dataBytes(pBuf, bufSize);
}

// clear everything
void ESP8266_SSD1322::clearDisplay(void) {
	memset(buffer, 0, (SSD1322_LCDHEIGHT * SSD1322_LCDWIDTH / (8 / SSD1322_BITS_PER_PIXEL)));
}

inline void ESP8266_SSD1322::fastSPIwrite(uint8_t d) {
	if (hwSPI) {
		(void) SPI.transfer(d);
	} else {
		for (uint8_t bit = 0x80; bit; bit >>= 1) {
			*clkport &= ~clkpinmask;
			if (d & bit)
				*mosiport |= mosipinmask;
			else
				*mosiport &= ~mosipinmask;
			*clkport |= clkpinmask;
		}
	}
	//*csport |= cspinmask;
}

inline void ESP8266_SSD1322::fastSPIwriteBytes(uint8_t * data, uint32_t const size) {
	SPI.writeBytes(data, size);
}

void ESP8266_SSD1322::drawFastHLine(int16_t x, int16_t y, int16_t w, uint16_t color) {
	bool bSwap = false;
	switch (rotation) {
	case 0:
		// 0 degree rotation, do nothing
		break;
	case 1:
		// 90 degree rotation, swap x & y for rotation, then invert x
		bSwap = true;
		_swap_int16_t(x, y)
		;
		x = WIDTH - x - 1;
		break;
	case 2:
		// 180 degree rotation, invert x and y - then shift y around for height.
		x = WIDTH - x - 1;
		y = HEIGHT - y - 1;
		x -= (w - 1);
		break;
	case 3:
		// 270 degree rotation, swap x & y for rotation, then invert y  and adjust y for w (not to become h)
		bSwap = true;
		_swap_int16_t(x, y)
		;
		y = HEIGHT - y - 1;
		y -= (w - 1);
		break;
	}

	if (bSwap) {
		drawFastVLineInternal(x, y, w, color);
	} else {
		drawFastHLineInternal(x, y, w, color);
	}
}

void ESP8266_SSD1322::drawFastHLineInternal(int16_t x, int16_t y, int16_t w, uint16_t color) {
	// Do bounds/limit checks
	if (y < 0 || y >= HEIGHT) {
		return;
	}

	// make sure we don't try to draw below 0
	if (x < 0) {
		w += x;
		x = 0;
	}

	// make sure we don't go off the edge of the display
	if ((x + w) > WIDTH) {
		w = (WIDTH - x);
	}

	// if our width is now negative, punt
	if (w <= 0) {
		return;
	}

	// set up the pointer for  movement through the buffer
	// adjust the buffer pointer for the current row
	register uint8_t *pBuf = buffer;
	pBuf += (x >> 1) + (y * (SSD1322_LCDWIDTH / 2));

	register uint8_t oddmask = color;
	register uint8_t evenmask = (color << 4);
	register uint8_t fullmask = (color << 4) + color;
	uint8_t byteLen = w / 2;

	if (((x % 2) == 0) && ((w % 2) == 0))  // Start at even and length is even
	{
		while (byteLen--)
		{
			*pBuf++ = fullmask;
		}

		return;
	}

	if (((x % 2) == 1) && ((w % 2) == 1)) // Start at odd and length is odd
	{
		register uint8_t b1 = *pBuf;
		b1 &= (x % 2) ? 0xF0 : 0x0F; // cleardown nibble to be replaced

		// write our value in
		*pBuf++ = b1 | oddmask;

		while (byteLen--)
		{
			*pBuf++ = fullmask;
		}
		return;
	}

	if (((x % 2) == 0) && ((w % 2) == 1)) // Start at even and length is odd
	{
		while (byteLen--)
		{
			*pBuf++ = fullmask;
		}

		register uint8_t b1 = *pBuf;
		b1 &= 0x0F; // cleardown nibble to be replaced

		// write our value in
		*pBuf++ = b1 | evenmask;
		return;
	}

	if (((x % 2) == 1) && ((w % 2) == 0)) // Start at odd and length is even
	{
		register uint8_t b1 = *pBuf;
		b1 &= (x % 2) ? 0xF0 : 0x0F; // cleardown nibble to be replaced

		// write our value in
		*pBuf++ = b1 | oddmask;

		while (byteLen--)
		{
			*pBuf++ = fullmask;
		}

		b1 = *pBuf;
		b1 &= 0x0F; // cleardown nibble to be replaced

		// write our value in
		*pBuf++ = b1 | evenmask;
		return;
	}
}

void ESP8266_SSD1322::drawFastVLine(int16_t x, int16_t y, int16_t h,
		uint16_t color) {
	bool bSwap = false;
	switch (rotation) {
	case 0:
		break;
	case 1:
		// 90 degree rotation, swap x & y for rotation, then invert x and adjust x for h (now to become w)
		bSwap = true;
		_swap_int16_t(x, y);
		x = WIDTH - x - 1;
		x -= (h - 1);
		break;
	case 2:
		// 180 degree rotation, invert x and y - then shift y around for height.
		x = WIDTH - x - 1;
		y = HEIGHT - y - 1;
		y -= (h - 1);
		break;
	case 3:
		// 270 degree rotation, swap x & y for rotation, then invert y
		bSwap = true;
		_swap_int16_t(x, y);
		y = HEIGHT - y - 1;
		break;
	}

	if (bSwap) {
		drawFastHLineInternal(x, y, h, color);
	} else {
		drawFastVLineInternal(x, y, h, color);
	}
}

void ESP8266_SSD1322::drawFastVLineInternal(int16_t x, int16_t __y,
		int16_t __h, uint16_t color) {

	// do nothing if we're off the left or right side of the screen
	if (x < 0 || x >= WIDTH) {
		return;
	}

	// make sure we don't try to draw below 0
	if (__y < 0) {
		// __y is negative, this will subtract enough from __h to account for __y being 0
		__h += __y;
		__y = 0;

	}

	// make sure we don't go past the height of the display
	if ((__y + __h) > HEIGHT) {
		__h = (HEIGHT - __y);
	}

	// if our height is now negative, punt
	if (__h <= 0) {
		return;
	}

	// this display doesn't need ints for coordinates, use local byte registers for faster juggling
	register uint8_t y = __y;
	register uint8_t h = __h;

	// set up the pointer for fast movement through the buffer
	register uint8_t *pBuf = buffer;
	// adjust the buffer pointer for the current row
	pBuf += (x >> 1) + (y  * (SSD1322_LCDWIDTH / 2));
	register uint8_t mask = ((x % 2) ? color : color << 4);
	while (h--)
	{
		register uint8_t b1 = *pBuf;
		b1 &= (x % 2) ? 0xF0 : 0x0F; // cleardown nibble to be replaced
		// write our value in
		*pBuf = b1 | mask;
		// adjust the buffer forward to next row worth of data
		pBuf += SSD1322_LCDWIDTH / 2;
	};
}

/**
 * Fill the display with the specified colour by setting
 * every pixel to the colour.
 * @param colour - fill the display with this colour.
 */
void ESP8266_SSD1322::fill(uint8_t colour)
{
    uint8_t x,y;

    ssd1322_command(SSD1322_SETCOLUMNADDR);
    ssd1322_data(MIN_SEG);
    ssd1322_data(MAX_SEG);

    ssd1322_command(SSD1322_SETROWADDR);
    ssd1322_data(0);
    ssd1322_data(63);

    colour = (colour & 0x0F) | (colour << 4);

    ssd1322_command(SSD1322_WRITERAM);

	for(y=0; y<64; y++)
    {
		for(x=0; x<64; x++)
		{
		    ssd1322_data(colour);
		    ssd1322_data(colour);
		}
    }
    delay(0);
}

void ESP8266_SSD1322::fastDrawBitmap(int16_t x, int16_t y, const uint8_t *bitmap, int16_t w, int16_t h, uint8_t color)
{
	// do nothing if we're off the left or right side of the screen
	if (x < 0 || x >= WIDTH)
	{
		return;
	}

	// TODO - NEEDS SOME WORK TO HANDLE XPOS that is not multiple of 8 bits
	// calc start pos in the buffer
	register uint8_t *pBuf = &buffer[(x >> 1) + (y * (SSD1322_LCDWIDTH / 2))];
	register uint8_t wInBytes = w >> 1; // Divide by 2, as 2 pixels per byte (4 bits per pixel)
	uint16_t bitPos = 0;

	// loop the height
	for (int lh = 0; lh < h; lh++)
	{
		// loop the width
		for (int lw = 0; lw < wInBytes; lw++)
		{
			*pBuf++ = pgm_read_byte(bitmap + bitPos++);
		}

		pBuf += (SSD1322_LCDWIDTH / 2) - wInBytes; // Move buffer position to next row
	}
}



/***************************************************************************************
** Function name:           drawNumber unsigned with size
** Descriptions:            drawNumber
***************************************************************************************/
int ESP8266_SSD1322::drawNumber(long long_num,int poX, int poY, int size)
{
    char tmp[10];
    if (long_num < 0) sprintf(tmp, "%li", long_num);
    else sprintf(tmp, "%lu", long_num);
    return drawString(tmp, poX, poY, size);
}

/***************************************************************************************
** Function name:           drawChar
** Descriptions:            draw char
***************************************************************************************/
int ESP8266_SSD1322::drawChar(char c, int x, int y, int size)
{
//Serial.println("drawChar:E");
#ifdef LOAD_GLCD
  // Use Adafruit font 5x7
  if (size == 0)
  {
     setCursor(x, y);
     return  print(c);
  }
#endif
  int retVal = drawUnicode(c, x, y, size);
//Serial.println("drawChar:X");
  return retVal;
}

/***************************************************************************************
** Function name:           drawString
** Descriptions:            draw string
***************************************************************************************/
int ESP8266_SSD1322::drawString(char *string, int poX, int poY, int size)
{
#ifdef LOAD_GLCD
	// Use Adafruit font 5x7
   if (size == 0)
   {
	   setCursor(poX, poY);
	   print(string);
	   return 0;
   }
#endif
    int sumX = 0;

    while(*string)
    {
        int xPlus = drawChar(*string, poX, poY, size);
        sumX += xPlus;
        string++;
        poX += xPlus;                            /* Move cursor right       */
    }
    return sumX;
}

///***************************************************************************************
//** Function name:           drawCentreString
//** Descriptions:            draw string across centre
//***************************************************************************************/
//int ESP8266_SSD1322::drawCentreString(char *string, int dX, int poY, int size)
//{
//#ifdef LOAD_GLCD
//	// Use Adafruit font fixed 5x7
//   if (size == 0)
//   {
//	   int len = strlen(string) * (5 + 1);
//	   int poX = dX - len / 2;
//	   setCursor(poX, poY);
//	   print(string);
//	   return 0;
//   }
//#endif
//
//    int sumX = 0;
//    int len = 0;
//    char *pointer = string;
//    char ascii;
//
//    while(*pointer)
//    {
//        ascii = *pointer;
//        //if (size==0)len += 1+pgm_read_byte((uint8_t *)widtbl_log+ascii);
//        //if (size==1)len += 1+pgm_read_byte((uint8_t *)widtbl_f8+ascii-32);
//#ifdef LOAD_FONT2
//        if (size==2)len += 1+pgm_read_byte((uint8_t *)widtbl_f16+ascii-32);
//#endif
//        //if (size==3)len += 1+pgm_read_byte((uint8_t *)widtbl_f48+ascii-32)/2;
//#ifdef LOAD_FONT4
//        if (size==4)len += pgm_read_byte((uint8_t *)widtbl_f32+ascii-32)-3;
//#endif
//        //if (size==5) len += pgm_read_byte((uint8_t *)widtbl_f48+ascii-32)-3;
//#ifdef LOAD_FONT6
//        if (size==6) len += pgm_read_byte((uint8_t *)widtbl_f64+ascii-32)-3;
//#endif
//#ifdef LOAD_FONT7
//        if (size==7) len += pgm_read_byte((uint8_t *)widtbl_f7s+ascii-32)+2;
//#endif
//#ifdef LOAD_FONT8
//        if (size==8) len += pgm_read_byte((uint8_t *)widtbl_F10+ascii-32)+gap_F10;
//#endif
//        pointer++;
//    }
//    len = len*textsize;
//    int poX = dX - len/2;
//
//    if (poX < 0) poX = 0;
//
//    while(*string)
//    {
//
//        int xPlus = drawChar(*string, poX, poY, size);
//        sumX += xPlus;
//        string++;
//        poX += xPlus;                  /* Move cursor right            */
//    }
//
//    return sumX;
//}
//
///***************************************************************************************
//** Function name:           drawRightString
//** Descriptions:            draw string right justified
//***************************************************************************************/
//int ESP8266_SSD1322::drawRightString(char *string, int dX, int poY, int size)
//{
//    int sumX = 0;
//    int len = 0;
//    char *pointer = string;
//    char ascii;
//
//    while(*pointer)
//    {
//        ascii = *pointer;
//        //if (size==0)len += 1+pgm_read_byte((uint8_t *)widtbl_log+ascii);
//        //if (size==1)len += 1+pgm_read_byte((uint8_t *)widtbl_f8+ascii-32);
//#ifdef LOAD_FONT2
//        if (size==2)len += 1+pgm_read_byte((uint8_t *)widtbl_f16+ascii-32);
//#endif
//        //if (size==3)len += 1+pgm_read_byte((uint8_t *)widtbl_f48+ascii-32)/2;
//#ifdef LOAD_FONT4
//        if (size==4)len += pgm_read_byte((uint8_t *)widtbl_f32+ascii-32)-3;
//#endif
//        //if (size==5) len += pgm_read_byte((uint8_t *)widtbl_f48+ascii-32)-3;
//#ifdef LOAD_FONT6
//        if (size==6) len += pgm_read_byte((uint8_t *)widtbl_f64+ascii-32)-3;
//#endif
//#ifdef LOAD_FONT7
//        if (size==7) len += pgm_read_byte((uint8_t *)widtbl_f7s+ascii-32)+2;
//#endif
//#ifdef LOAD_FONT8
//        if (size==8) len += pgm_read_byte((uint8_t *)widtbl_F10+ascii-32)+gap_F10;
//#endif
//        pointer++;
//    }
//    len = len*textsize;
//    int poX = dX - len;
//    if (poX < 0) poX = 0;
//    while(*string)
//    {
//        int xPlus = drawChar(*string, poX, poY, size);
//        sumX += xPlus;
//        string++;
//        poX += xPlus;          /* Move cursor right            */
//    }
//    return sumX;
//}

/***************************************************************************************
** Function name:           drawFloat
** Descriptions:            drawFloat
***************************************************************************************/
int ESP8266_SSD1322::drawFloat(float floatNumber, int decimal, int poX, int poY, int size)
{
    unsigned long temp=0;
    float decy=0.0;
    float rounding = 0.5;
    float eep = 0.000001;
    int sumX    = 0;
    int xPlus   = 0;
    if(floatNumber-0.0 < eep)       // floatNumber < 0
    {
        xPlus = drawChar('-',poX, poY, size);
        floatNumber = -floatNumber;
        poX  += xPlus;
        sumX += xPlus;
    }

    for (unsigned char i=0; i<decimal; ++i)
    {
        rounding /= 10.0;
    }

    floatNumber += rounding;
    temp = (long)floatNumber;
    xPlus = drawNumber(temp,poX, poY, size);
    poX  += xPlus;
    sumX += xPlus;

    if(decimal>0)
    {
        xPlus = drawChar('.',poX, poY, size);
        poX += xPlus;                            /* Move cursor right            */
        sumX += xPlus;
    }
    else
    {
        return sumX;
    }

    decy = floatNumber - temp;
    for(unsigned char i=0; i<decimal; i++)
    {
        decy *= 10;                                /* for the next decimal         */
        temp = decy;                               /* get the decimal              */
        xPlus = drawNumber(temp,poX, poY, size);
        poX += xPlus;                              /* Move cursor right            */
        sumX += xPlus;
        decy -= temp;
    }
    return sumX;
}

inline static byte readPixels(const byte* loc, bool invert)
{
	byte pixels = pgm_read_byte((uint8_t *)loc);
	if(invert)	pixels = ~pixels;
	return pixels;
}

// Ultra fast bitmap drawing
// Only downside is that height must be a multiple of 8, otherwise it gets rounded down to the nearest multiple of 8
// Drawing bitmaps that are completely on-screen and have a Y co-ordinate that is a multiple of 8 results in best performance
// PS - Sorry about the poorly named variables ;_;
// Optimize: Use a local variable temp buffer then apply to global variable OLED buffer?
void ESP8266_SSD1322::ultraFastDrawBitmap(int16_t x, int16_t y, const uint8_t *bitmap, uint8_t w, uint8_t h, uint8_t color, bool invert)
{
	byte yy = y;
	byte offsetY = 0;
	byte h2 = h / 8;
	byte pixelOffset = (y % 8);
	byte thing3 = (yy+h);
	for(byte hh=0;hh<h2;hh++)
	{
		byte hhh = (hh*8) + y;
		byte hhhh = hhh + 8;

		if(offsetY && (hhhh < yy || hhhh > SSD1322_LCDWIDTH || hhh > thing3))
		continue;

		byte offsetMask = 0xFF;
		if(offsetY)
		{
			if(hhh < yy)
			offsetMask = (0xFF<<(yy-hhh));
			else if(hhhh > thing3)
			offsetMask = (0xFF>>(hhhh-thing3));
		}
		unsigned int aa = ((hhh / 8) * SSD1322_LCDWIDTH);
		// If() outside of loop makes it faster (doesn't have to kee re-evaluating it)
		// Downside is code duplication
		if(!pixelOffset && hhh < SSD1322_LCDWIDTH)
		{
			for(byte ww=0;ww<w;ww++)
			{
				// Workout X co-ordinate in frame buffer to place next 8 pixels
				byte xx = ww + x;

				// Stop if X co-ordinate is outside the frame
				if(xx >= SSD1322_LCDWIDTH)
				continue;

				// Read pixels
				byte pixels = readPixels((bitmap + (hh*w)) + ww, invert) & offsetMask;
				buffer[xx + aa] |= pixels;
			}
		}
		else
		{
			unsigned int aaa = ((hhhh / 8) * SSD1322_LCDWIDTH);
			for(byte ww=0;ww<w;ww++)
			{
				// Workout X co-ordinate in frame buffer to place next 8 pixels
				byte xx = ww + x;

				// Stop if X co-ordinate is outside the frame
				if(xx >= SSD1322_LCDWIDTH)
				continue;

				// Read pixels
				byte pixels = readPixels((bitmap + (hh*w)) + ww, invert) & offsetMask;

				//
				if(hhh < SSD1322_LCDHEIGHT)
				{
					buffer[xx + aa] |= pixels << pixelOffset;
				}
				//
				if(hhhh < SSD1322_LCDHEIGHT)
				{
					buffer[xx + aaa] |= pixels >> (8 - pixelOffset);
				}
			}
		}
	}
}

//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
//  Gray Scale Table Setting (Full Screen)
//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
void ESP8266_SSD1322::Set_Gray_Scale_Table()
{
	ssd1322_command(0xB8);			// Set Gray Scale Table
	ssd1322_data(0x0C);			//   Gray Scale Level 1
	ssd1322_data(0x18);			//   Gray Scale Level 2
	ssd1322_data(0x24);			//   Gray Scale Level 3
	ssd1322_data(0x30);			//   Gray Scale Level 4
	ssd1322_data(0x3C);			//   Gray Scale Level 5
	ssd1322_data(0x48);			//   Gray Scale Level 6
	ssd1322_data(0x54);			//   Gray Scale Level 7
	ssd1322_data(0x60);			//   Gray Scale Level 8
	ssd1322_data(0x6C);			//   Gray Scale Level 9
	ssd1322_data(0x78);			//   Gray Scale Level 10
	ssd1322_data(0x84);			//   Gray Scale Level 11
	ssd1322_data(0x90);			//   Gray Scale Level 12
	ssd1322_data(0x9C);			//   Gray Scale Level 13
	ssd1322_data(0xA8);			//   Gray Scale Level 14
	ssd1322_data(0xB4);			//   Gray Scale Level 15
	ssd1322_command(0x00);			// Enable Gray Scale Table
}


