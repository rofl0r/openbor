/*
 * OpenBOR - http://www.LavaLit.com
 * -----------------------------------------------------------------------
 * Licensed under the BSD license, see LICENSE in OpenBOR root for details.
 *
 * Copyright (c) 2004 - 2011 OpenBOR Team
 */

#ifndef	LOADIMAGE_H
#define LOADIMAGE_H

// Blah.

int loadscreen(char *filename, char *packfile, unsigned char *pal, int format, s_screen ** screen);
#ifdef DEBUG
s_bitmap *loadbitmap_(const char* caller_func, char *filename, char *packfile, int format);
#define loadbitmap(x, y, z) loadbitmap_(__FUNCTION__, x, y, z)
#else
s_bitmap *loadbitmap(char *filename, char *packfile, int format);
#endif
int loadimagepalette(char *filename, char *packfile, unsigned char *pal);
#endif
