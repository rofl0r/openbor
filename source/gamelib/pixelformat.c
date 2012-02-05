/*
 * OpenBOR - http://www.LavaLit.com
 * -----------------------------------------------------------------------
 * Licensed under the BSD license, see LICENSE in OpenBOR root for details.
 *
 * Copyright (c) 2004 - 2011 OpenBOR Team
 */

///////////////////////////////////////////////////////////////////////////
//         This file defines some commmon methods used by the gamelib
////////////////////////////////////////////////////////////////////////////

#include "types.h"

#ifndef NULL
#define NULL (void*)0
#endif

int pixelformat = PIXEL_8;
int screenformat = PIXEL_8;
int pixelbytes[(int) 5] = { 1, 1, 2, 3, 4 };


unsigned short colour16(unsigned char r, unsigned char g, unsigned char b) {
#if WII
	return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
#else
	return ((b & 0xF8) << 8) | ((g & 0xFC) << 3) | (r >> 3);
#endif
}

unsigned colour32(unsigned char r, unsigned char g, unsigned char b) {
#if WII
	return ((r << 16) | (g << 8) | b);
#else
	return ((b << 16) | (g << 8) | r);
#endif
}

#define _multiply(c1,c2) (((c1)*(c2))>>8)
#define _screen(c1,c2) ((((c1)^255)*((c2)^255)/255)^255)
#define _hardlight(c1,c2) ((c1)<128?_multiply((c1)<<1,(c2)):_screen(((c1)-128)<<1,(c2)))
#define _overlay(c1,c2) ((c2)<128?_multiply((c2)<<1,(c1)):_screen(((c2)-128)<<1,(c1)))
#define _dodge(c1,c2) (((c2)<<8)/(256-(c1)))
#define _channel(src,dest,alpha) (((src*alpha)+(dest*(255-alpha)))>>8)
#define _color(r,g,b) (((r)<<16)|((g)<<8)|(b))

// common blend function
unsigned blend_multiply(register unsigned color1, register unsigned color2) {
	return _multiply(color1, color2);
}

unsigned blend_screen(register unsigned color1, register unsigned color2) {
	return _screen(color1, color2);
}

unsigned blend_overlay(register unsigned color1, register unsigned color2) {
	return _overlay(color1, color2);
}

unsigned blend_hardlight(register unsigned color1, register unsigned color2) {
	return _hardlight(color1, color2);
}

unsigned blend_dodge(register unsigned color1, register unsigned color2) {
	unsigned c = _dodge(color1, color2);
	return c > 255 ? 255 : c;
}

unsigned blend_half(register unsigned color1, register unsigned color2) {
	return (color1 + color2) >> 1;
}

/////////////////////// blend 2 32bit colours //////////////////////
// color1 front colour,
// color2 bg colour
////////////////////////////////////////////////////////////////////
unsigned blend_multiply32(register unsigned color1, register unsigned color2) {
	return _color(_multiply(color1 >> 16, color2 >> 16),
		      _multiply((color1 & 0xFF00) >> 8, (color2 & 0xFF00) >> 8),
		      _multiply(color1 & 0xFF, color2 & 0xFF));
}

unsigned blend_screen32(register unsigned color1, register unsigned color2) {
	return _color(_screen(color1 >> 16, color2 >> 16),
		      _screen((color1 & 0xFF00) >> 8, (color2 & 0xFF00) >> 8), _screen(color1 & 0xFF, color2 & 0xFF));
}

unsigned blend_overlay32(register unsigned color1, register unsigned color2) {
	int r1 = color1 >> 16, r2 = color2 >> 16;
	int g1 = (color1 & 0xFF00) >> 8, g2 = (color2 & 0xFF00) >> 8;
	int b1 = color1 & 0xFF, b2 = color2 & 0xFF;
	return _color(_overlay(r1, r2), _overlay(g1, g2), _overlay(b1, b2));
}

unsigned blend_hardlight32(register unsigned color1, register unsigned color2) {
	int r1 = color1 >> 16, r2 = color2 >> 16;
	int g1 = (color1 & 0xFF00) >> 8, g2 = (color2 & 0xFF00) >> 8;
	int b1 = color1 & 0xFF, b2 = color2 & 0xFF;
	return _color(_hardlight(r1, r2), _hardlight(g1, g2), _hardlight(b1, b2));
}

unsigned blend_dodge32(register unsigned color1, register unsigned color2) {
	unsigned r = _dodge(color1 >> 16, color2 >> 16);
	unsigned g = _dodge((color1 & 0xFF00) >> 8, (color2 & 0xFF00) >> 8);
	unsigned b = _dodge(color1 & 0xFF, color2 & 0xFF);
	return _color(r > 255 ? 255 : r, g > 255 ? 255 : g, b > 255 ? 255 : b);
}

unsigned blend_half32(register unsigned color1, register unsigned color2) {
	return _color(((color1 >> 16) + (color2 >> 16)) >> 1,
		      (((color1 & 0xFF00) >> 8) + ((color2 & 0xFF00) >> 8)) >> 1,
		      ((color1 & 0xFF) + (color2 & 0xFF)) >> 1);
}

unsigned blend_channel32(register unsigned color1, register unsigned color2, register unsigned a) {
	int r1 = color1 >> 16, r2 = color2 >> 16;
	int g1 = (color1 & 0xFF00) >> 8, g2 = (color2 & 0xFF00) >> 8;
	int b1 = color1 & 0xFF, b2 = color2 & 0xFF;
	return _color(_channel(r1, r2, a), _channel(g1, g2, a), _channel(b1, b2, a));
}

/////////////////////// blend 2 16bit colours //////////////////////
// color1 front colour,
// color2 bg colour
////////////////////////////////////////////////////////////////////

#define _r1 (color1>>11)
#define _g1 ((color1&0x7E0)>>5)
#define _b1 (color1&0x1F)
#define _r2 (color2>>11)
#define _g2 ((color2&0x7E0)>>5)
#define _b2 (color2&0x1F)
#define _multiply16(c1,c2,m) ((c1)*(c2)/(m))
#define _screen16(c1,c2,m) ((((c1)^(m))*((c2)^(m))/(m))^(m))
#define _hardlight16(c1,c2,m,m2) ((c1)<(m)?_multiply16(((c1)<<1),(c2),(m2)):_screen16((((c1)-(m))<<1),(c2),(m2)))
#define _overlay16(c1,c2,m,m2) ((c2)<(m)?_multiply16(((c2)<<1),(c1),(m2)):_screen16((((c2)-(m))<<1),(c1),(m2)))
#define _dodge16(c1,c2,m) ((c2)*(m)/((m)-(c1)))
#define _channel16(src,dest,alpha) (((src*alpha)+(dest*(255-alpha)))>>8)
#define _color16(r,g,b) ( ((r)<<11)|((g)<<5)|b )

//16bit blending, bit565
unsigned short blend_multiply16(unsigned short color1, unsigned short color2) {
	return _color16(_multiply16(_r1, _r2, 0x1F), _multiply16(_g1, _g2, 0x3F), _multiply16(_b1, _b2, 0x1F));
}

unsigned short blend_screen16(unsigned short color1, unsigned short color2) {
	return _color16(_screen16(_r1, _r2, 0x1F), _screen16(_g1, _g2, 0x3F), _screen16(_b1, _b2, 0x1F));
}

unsigned short blend_overlay16(unsigned short color1, unsigned short color2) {
	unsigned r1 = _r1, r2 = _r2;
	unsigned g1 = _g1, g2 = _g2;
	unsigned b1 = _b1, b2 = _b2;
	return _color16(_overlay16(r1, r2, 0x10, 0x1F), _overlay16(g1, g2, 0x20, 0x3F), _overlay16(b1, b2, 0x10, 0x1F));
}

unsigned short blend_hardlight16(unsigned short color1, unsigned short color2) {
	unsigned r1 = _r1, r2 = _r2;
	unsigned g1 = _g1, g2 = _g2;
	unsigned b1 = _b1, b2 = _b2;
	return _color16(_hardlight16(r1, r2, 0x10, 0x1F), _hardlight16(g1, g2, 0x20, 0x3F),
			_hardlight16(b1, b2, 0x10, 0x1F));
}

unsigned short blend_dodge16(unsigned short color1, unsigned short color2) {
	int r = _dodge16(_r1, _r2, 0x20);
	int g = _dodge16(_g1, _g2, 0x40);
	int b = _dodge16(_b1, _b2, 0x20);
	if(r > 0x1F)
		r = 0x1F;
	if(g > 0x3F)
		g = 0x3F;
	if(b > 0x1F)
		b = 0x1F;
	return _color16(r, g, b);
}

unsigned short blend_half16(unsigned short color1, unsigned short color2) {
	return _color16((_r1 + _r2) >> 1, (_g1 + _g2) >> 1, (_b1 + _b2) >> 1);
}

unsigned short blend_channel16(unsigned short color1, unsigned short color2, register unsigned a) {
	return _color16(_channel16(_r1, _r2, a), _channel16(_g1, _g2, a), _channel16(_b1, _b2, a));
}

unsigned char *blendtables[MAX_BLENDINGS] = { NULL, NULL, NULL, NULL, NULL, NULL };
blend16fp blendfunctions16[MAX_BLENDINGS] =
    { blend_screen16, blend_multiply16, blend_overlay16, blend_hardlight16, blend_dodge16, blend_half16 };
blend32fp blendfunctions[MAX_BLENDINGS] =
    { blend_screen, blend_multiply, blend_overlay, blend_hardlight, blend_dodge, blend_half };
blend32fp blendfunctions32[MAX_BLENDINGS] =
    { blend_screen32, blend_multiply32, blend_overlay32, blend_hardlight32, blend_dodge32, blend_half32 };

// This method set blending tables for 8bit mode
// Need a list of blending table handles which
// are created by palette.c
void set_blendtables(unsigned char *tables[]) {
	int i;
	for(i = 0; i < MAX_BLENDINGS; i++)
		blendtables[i] = tables[i];
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//      copy/reverse copy methods, extremely big function, but may improve the speed
//      assume the size is never greater than 480(screen size in some platforms)
//      TODO: test on other platforms besides windows
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#define pcpy_b unsigned l; \
	if (len) { \
		l = len; \
		if(l < 961) \
			do { \
				l--; \
				pdest[l] = pp[psrc[l]]; \
			} while (l); \
	}

#define revpcpy_b unsigned i, l; \
	if (len) { \
		l = len; \
		i = 960 - l; \
		if(l < 961) \
			do { \
				l--; \
				pdest[l] = pp[psrc[i]]; \
				i++; \
			} while (l); \
	}


//copy psrc to pdest, pdest reversed order
void u8revcpy(unsigned char *pdest, const unsigned char *psrc, unsigned len) {
	unsigned i, l;
	psrc -= 960 - (int) len;
	pdest -= (int) len - 1;
	if(len) {
		l = len;
		i = 960 - l;
		if(l < 961)
			do {
				l--;
				pdest[l] = psrc[i];
				i++;
			} while(l);
	}
}

// reverse copy with a remap table
void u8revpcpy(unsigned char *pdest, const unsigned char *psrc, unsigned char *pp, unsigned len) {
	psrc -= 960 - (int) len;
	pdest -= (int) len - 1;
	revpcpy_b;
}

//copy with a remap table
void u8pcpy(unsigned char *pdest, const unsigned char *psrc, unsigned char *pp, unsigned len) {
	pcpy_b;
}

//----------16bit
// reverse copy with a remap table
void u16revpcpy(unsigned short *pdest, const unsigned char *psrc, unsigned short *pp, unsigned len) {
	psrc -= 960 - (int) len;
	pdest -= len - 1;
	revpcpy_b;
}

//copy with a remap table
void u16pcpy(unsigned short *pdest, const unsigned char *psrc, unsigned short *pp, unsigned len) {
	pcpy_b;
}

void u32revpcpy(unsigned *pdest, const unsigned char *psrc, unsigned *pp, unsigned len) {
	psrc -= 960 - (int) len;
	pdest -= (int) len - 1;
	revpcpy_b;
}

//copy with a remap table
void u32pcpy(unsigned *pdest, const unsigned char *psrc, unsigned *pp, unsigned len) {
	pcpy_b;
}
