/*
 * OpenBOR - http://www.LavaLit.com
 * -----------------------------------------------------------------------
 * Licensed under the BSD license, see LICENSE in OpenBOR root for details.
 *
 * Copyright (c) 2004 - 2011 OpenBOR Team
 */

#include <SDL/SDL_framerate.h>
#include <math.h>
#include "types.h"
#include "video.h"
#include "vga.h"
#include "screen.h"
#include "sdlport.h"
#include "opengl.h"
#include "openbor.h"
#include "gfxtypes.h"
#include "gfx.h"

#ifndef SKIP_CODE
#include "pngdec.h"
#include "../resources/OpenBOR_Icon_32x32_png.h"
#endif

#define SDL_mutex_lock(X) SDL_mutexP(X)
#define SDL_mutex_unlock(X) SDL_mutexV(X)

FPSmanager framerate_manager;
s_videomodes stored_videomodes;
static SDL_Surface *screen = NULL;
static SDL_Surface *bscreen = NULL;
static SDL_Surface *bscreen2 = NULL;
static SDL_mutex *screens_lock;
static SDL_Color sdl_colors[256];
static int bytes_per_pixel = 1;
int stretch = 0;
int opengl = 0;			// OpenGL backend currently in use?
int nativeWidth, nativeHeight;	// monitor resolution used in fullscreen mode
u8 pDeltaBuffer[480 * 2592];

void finishSDL(void) {
	SDL_DestroyMutex(screens_lock);
	SDL_Quit();
}

void initSDL(void) {
	const SDL_VideoInfo *video_info;
	int init_flags = SDL_INIT_VIDEO | SDL_INIT_TIMER | SDL_INIT_JOYSTICK;

#ifdef CUSTOM_SIGNAL_HANDLER
	init_flags |= SDL_INIT_NOPARACHUTE;
#endif

	if(SDL_Init(init_flags) < 0) {
		printf("SDL Failed to Init!!!! (%s)\n", SDL_GetError());
		borExit(0);
	}
	screens_lock = SDL_CreateMutex();
	SDL_ShowCursor(SDL_DISABLE);
	atexit(finishSDL);
#ifndef SKIP_CODE
	SDL_WM_SetCaption("OpenBOR", NULL);
	SDL_WM_SetIcon((SDL_Surface *) pngToSurface((void *) openbor_icon_32x32_png.data), NULL);
#endif
#if !defined(GLES)
	if(SDL_GL_LoadLibrary(NULL) < 0) {
		printf("Warning: couldn't load OpenGL library (%s)\n", SDL_GetError());
	}
#endif

	// Store the monitor's current resolution before setting the video mode for the first time
	video_info = SDL_GetVideoInfo();
	nativeWidth = video_info->current_w;
	nativeHeight = video_info->current_h;

	SDL_initFramerate(&framerate_manager);
	SDL_setFramerate(&framerate_manager, 200);
}

static unsigned masks[4][4] =
    { {0, 0, 0, 0}, {0x1F, 0x07E0, 0xF800, 0}, {0xFF, 0xFF00, 0xFF0000, 0}, {0xFF, 0xFF00, 0xFF0000, 0} };

static void lock_screens(void) {
	while(SDL_mutex_lock(screens_lock) == -1) {
		printf("warning: couldn't aquire screens_lock\n");
		SDL_Delay(1);
	}
}

static void unlock_screens(void) {
	SDL_mutex_unlock(screens_lock);
}

int video_set_mode(s_videomodes videomodes) {
	stored_videomodes = videomodes;
	lock_screens();

	if(screen)
		SDL_FreeAndNullVideoSurface(screen);
	if(bscreen)
		SDL_FreeAndNullVideoSurface(bscreen);
	if(bscreen2)
		SDL_FreeAndNullVideoSurface(bscreen2);

#ifdef OPENGL
	// try OpenGL initialization first
	if((savedata.usegl || savedata.fullscreen) && video_gl_set_mode(videomodes))
		return 1;
	else
#endif
		opengl = 0;

	// FIXME: OpenGL surfaces aren't freed when switching from OpenGL to SDL

	bytes_per_pixel = videomodes.pixel;
	if(videomodes.hRes == 0 && videomodes.vRes == 0) {
		Term_Gfx();
		return 0;
	}

	if(savedata.screen[videoMode][0]) {
		screen =
		    SDL_SetVideoMode(videomodes.hRes * savedata.screen[videoMode][0],
				     videomodes.vRes * savedata.screen[videoMode][0], 16,
				     savedata.
				     fullscreen ? (SDL_SWSURFACE | SDL_DOUBLEBUF | SDL_FULLSCREEN) : (SDL_SWSURFACE |
												      SDL_DOUBLEBUF));
		if(!screen) {
			fprintf(stderr, "couldn't get screen!\n");
			fflush(stderr);
		}
		SDL_ShowCursor(SDL_DISABLE);
		bscreen = SDL_AllocSurface(SDL_SWSURFACE, videomodes.hRes, videomodes.vRes, 8 * bytes_per_pixel, masks[bytes_per_pixel - 1][0], masks[bytes_per_pixel - 1][1], masks[bytes_per_pixel - 1][2], masks[bytes_per_pixel - 1][3]);	// 24bit mask
		bscreen2 =
		    SDL_AllocSurface(SDL_SWSURFACE, videomodes.hRes + 4, videomodes.vRes + 8, 16, masks[1][2],
				     masks[1][1], masks[1][0], masks[1][3]);
		Init_Gfx(565, 16);
		memset(pDeltaBuffer, 0x00, 1244160);
		if(!screen || !bscreen || !bscreen2)
			goto exit_failure_unlock;
	} else {
		if(bytes_per_pixel > 1) {
			bscreen = SDL_AllocSurface(SDL_SWSURFACE, videomodes.hRes, videomodes.vRes, 8 * bytes_per_pixel, masks[bytes_per_pixel - 1][0], masks[bytes_per_pixel - 1][1], masks[bytes_per_pixel - 1][2], masks[bytes_per_pixel - 1][3]);	// 24bit mask
			if(!bscreen)
				goto exit_failure_unlock;
		}
		screen =
		    SDL_SetVideoMode(videomodes.hRes, videomodes.vRes, 8 * bytes_per_pixel,
				     savedata.
				     fullscreen ? (SDL_SWSURFACE | SDL_DOUBLEBUF | SDL_FULLSCREEN) : (SDL_SWSURFACE |
												      SDL_DOUBLEBUF));
		SDL_ShowCursor(SDL_DISABLE);
		if(!screen)
			goto exit_failure_unlock;
	}

	if(bytes_per_pixel == 1) {
		SDL_SetColors(screen, sdl_colors, 0, 256);
		if(bscreen)
			SDL_SetColors(bscreen, sdl_colors, 0, 256);
	}

	unlock_screens();
	video_clearscreen();
	return 1;

	exit_failure_unlock:
	unlock_screens();
	return 0;
}

void video_fullscreen_flip() {
	size_t w, h;
#ifdef OPENGL
	if(savedata.usegl) {
		video_gl_fullscreen_flip();
		return;
	}
#endif
	savedata.fullscreen ^= 1;

	if(savedata.fullscreen) {
		// OpenGL has better fullscreen than SDL
		if(video_set_mode(stored_videomodes))
			return;
	} else if(opengl) {
		// switch from OpenGL fullscreen to SDL windowed
		if(video_set_mode(stored_videomodes))
			return;
		return;
	}

	lock_screens();
	// switch between SDL fullscreen and SDL windowed
	if(screen) {
		w = screen->w;
		h = screen->h;
		SDL_FreeAndNullVideoSurface(screen);
	} else {
		w = 320;
		h = 240;
	}

	if(savedata.screen[videoMode][0])
		screen =
		    SDL_SetVideoMode(w, h, 16,
				     savedata.
				     fullscreen ? (SDL_SWSURFACE | SDL_DOUBLEBUF | SDL_FULLSCREEN) : (SDL_SWSURFACE |
												      SDL_DOUBLEBUF));
	else
		screen =
		    SDL_SetVideoMode(w, h, 8 * bytes_per_pixel,
				     savedata.
				     fullscreen ? (SDL_SWSURFACE | SDL_DOUBLEBUF | SDL_FULLSCREEN) : (SDL_SWSURFACE |
												      SDL_DOUBLEBUF));
	SDL_ShowCursor(SDL_DISABLE);
	SDL_SetColors(screen, sdl_colors, 0, 256);
	if(bscreen)
		SDL_SetColors(bscreen, sdl_colors, 0, 256);
	unlock_screens();
}

//16bit, scale 2x 4x 8x ...
void _stretchblit(SDL_Surface * src, SDL_Rect * srcrect, SDL_Surface * dst, SDL_Rect * dstrect) {
	SDL_Rect rect;
	int dst_x, dst_y, dst_w, dst_h, dst_row, src_row;
	int i;
	Uint16 *psrc, *pdst;

	if(!srcrect) {
		rect.x = rect.y = 0;
		rect.w = src->w;
		rect.h = src->h;
		srcrect = &rect;
	}
	dst_w = savedata.screen[videoMode][0] * srcrect->w;
	dst_h = savedata.screen[videoMode][0] * srcrect->h;
	if(!dstrect) {
		dst_x = dst_y = 0;
		if(dst_w > dst->w)
			dst_w = dst->w;
		if(dst_h > dst->h)
			dst_h = dst->h;
	} else {
		dst_x = dstrect->x;
		dst_y = dstrect->y;
		if(dst_w > dstrect->w)
			dst_w = dstrect->w;
		if(dst_h > dstrect->h)
			dst_h = dstrect->h;
	}
	psrc = (Uint16 *) src->pixels + srcrect->x + srcrect->y * src->pitch / 2;
	pdst = (Uint16 *) dst->pixels + dst_x + dst_y * dst->pitch / 2;
	dst_row = dst->pitch / 2;
	src_row = src->pitch / 2;
	while(dst_h > 0) {
		for(i = 0; i < dst_w; i++) {
			*(pdst + i) = *(psrc + (i / savedata.screen[videoMode][0]));
		}

		for(i = 1, pdst += dst_row; i < savedata.screen[videoMode][0] && dst_h; i++, dst_h--, pdst += dst_row) {
			memcpy(pdst, pdst - dst_row, dst_w << 1);
		}
		dst_h--;
		psrc += src_row;
	}
}

static int lock_if_necessary(SDL_Surface * myscreen) {
	int gotlock;

	lock_retry:
	gotlock = SDL_MUSTLOCK(myscreen);
	if(gotlock) {
		gotlock = SDL_LockSurface(myscreen) == 0;
		if(!gotlock) {
			SDL_Delay(1);
			goto lock_retry;
		}
	}
	return gotlock;
}

int video_copy_screen(s_screen * src) {
	unsigned char *sp;
	char *dp;
	int width, height, linew, slinew;
	int h;
	int gotlock, gotblock = 0, gotblock2;
	SDL_Surface *ds = NULL;
	SDL_Rect rectdes, rectsrc;

#ifdef OPENGL
	// use video_gl_copy_screen if in OpenGL mode
	if(opengl)
		return video_gl_copy_screen(src);
#endif

	width = screen->w;
	if(width > src->width)
		width = src->width;
	height = screen->h;
	if(height > src->height)
		height = src->height;
	if(!width || !height)
		return 0;
	h = height;

	lock_screens();
	if(bscreen) {
		rectdes.x = rectdes.y = 0;
		rectdes.w = width * savedata.screen[videoMode][0];
		rectdes.h = height * savedata.screen[videoMode][0];
		if(bscreen2) {
			rectsrc.x = 2;
			rectsrc.y = 4;
		} else {
			rectsrc.x = 0;
			rectsrc.y = 0;
		}
		rectsrc.w = width;
		rectsrc.h = height;
		gotblock = lock_if_necessary(bscreen);
	}
	// Copy to linear video ram
	gotlock = lock_if_necessary(screen);

	sp = (unsigned char *) src->data;
	ds = (bscreen ? bscreen : screen);
	dp = ds->pixels;

	linew = width * bytes_per_pixel;
	slinew = src->width * bytes_per_pixel;

	do {
		memcpy(dp, sp, linew);
		sp += slinew;
		dp += ds->pitch;
	} while(--h);

	if(gotlock)
		SDL_UnlockSurface(screen);

	if(bscreen) {
		if(gotblock)
			SDL_UnlockSurface(bscreen);

		ds = bscreen2 ? bscreen2 : screen;

		if(SDL_BlitSurface(bscreen, NULL, ds, &rectsrc) == -2)
			PDEBUG("XXXXXXXXXXXXXX SDL_BLITSURFACE returned -2!");

		if(bscreen2) {
			gotblock2 = lock_if_necessary(bscreen2);
			gotlock = lock_if_necessary(screen);

			if(savedata.screen[videoMode][0] == 2)
				(*GfxBlitters[(int) savedata.screen[videoMode][1]])
				    ((u8 *) bscreen2->pixels + bscreen2->pitch * 4 + 4,
				     bscreen2->pitch,
				     pDeltaBuffer + bscreen2->pitch,
				     (u8 *) screen->pixels, screen->pitch, screen->w >> 1, screen->h >> 1);
			else
				_stretchblit(bscreen2, &rectsrc, screen, &rectdes);

			if(gotblock2)
				SDL_UnlockSurface(bscreen2);
			if(gotlock)
				SDL_UnlockSurface(screen);
		}
	}
	SDL_Flip(screen);
	unlock_screens();

	SDL_framerateDelay(&framerate_manager);

	return 1;
}

void video_clearscreen() {
	int gotlock;

#ifdef OPENGL
	if(opengl) {
		video_gl_clearscreen();
		return;
	}
#endif
	lock_screens();
	gotlock = lock_if_necessary(screen);

	memset(screen->pixels, 0, screen->pitch * screen->h);
	if(gotlock)
		SDL_UnlockSurface(screen);
	if(bscreen) {
		gotlock = lock_if_necessary(bscreen);
		memset(bscreen->pixels, 0, bscreen->pitch * bscreen->h);
		if(gotlock)
			SDL_UnlockSurface(bscreen);
	}
	unlock_screens();
}

void video_stretch(int enable) {
	if(screen || opengl)
		video_clearscreen();
	stretch = enable;
}

void vga_vwait(void) {
	static int prevtick = 0;
	int now = SDL_GetTicks();
	int wait = 1000 / 60 - (now - prevtick);
	if(wait > 0) {
		SDL_Delay(wait);
	} else
		SDL_Delay(1);
	prevtick = now;
}

void vga_setpalette(unsigned char *palette) {
	int i;
#ifdef OPENGL
	video_gl_setpalette(palette);
#endif
	for(i = 0; i < 256; i++) {
		sdl_colors[i].r = palette[0];
		sdl_colors[i].g = palette[1];
		sdl_colors[i].b = palette[2];
		palette += 3;
	}
	if(!opengl) {
		lock_screens();
		SDL_SetColors(screen, sdl_colors, 0, 256);
		if(bscreen)
			SDL_SetColors(bscreen, sdl_colors, 0, 256);
		unlock_screens();
	}
}

// TODO: give this function a boolean (int) return type denoting success/failure
void vga_set_color_correction(int gm, int br) {
#ifdef OPENGL	
	if(opengl)
		video_gl_set_color_correction(gm, br);
#endif
}
