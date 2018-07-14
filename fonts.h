/*
Multifont GFX library is adapted from Adafruit_GFX library by Paul Kourany
v1.0.0, May 2014 Initial Release
v1.0.1, June 2014 Font Compilation update
v1.0.2, Aug 2015 Added charWidth(char) function to return char width in pixels

Please read README.pdf for details
*/

#ifndef _fonts_h
#define _fonts_h


//Font selection for compiling - comment out or uncomment definitions as required
//NOTE: GLCDFONT is default font and always included
#define UIFONT
#define UISMFONT
#define BIGNUMFONT
#define NIXIFONT
// Font selection descriptors - Add an entry for each new font and number sequentially
#define GLCDFONT	0
#define TEST		1
#define UI		2
#define UISM		3
#define BIGNUM 4
#define NIXI 5
#define NIXI20 6
#define FONT_START 0
#define FONT_END 1

struct FontDescriptor
{
	uint8_t	width;		// width in bits
	uint8_t	height; 	// char height in bits
	uint16_t offset;	// offset of char into char array
};

extern const uint8_t glcdfontBitmaps[];
extern const FontDescriptor glcdfontDescriptors[];

#ifdef TEST
extern const uint8_t testBitmaps[];
extern const FontDescriptor testDescriptors[];
#endif

#ifdef UI
extern const uint8_t UI_26ptBitmaps[];
extern const FontDescriptor UI_26ptDescriptors[];
#endif

#ifdef UISM
extern const uint8_t UISM_26ptBitmaps[];
extern const FontDescriptor UISM_26ptDescriptors[];
#endif

#ifdef BIGNUM
extern const uint8_t BIGNUMBitmaps[];
extern const FontDescriptor BIGNUMDescriptors[];
#endif

#ifdef NIXI
extern const uint8_t nixieOne_36ptBitmaps[];
extern const FontDescriptor nixieOne_36ptDescriptors[];
extern const uint8_t nixieOne_20ptBitmaps[];
extern const FontDescriptor nixieOne_20ptDescriptors[];
#endif

#endif
