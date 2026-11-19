/*
 * OpenBOR - http://lavalit.com
 * -----------------------------------------------------------------------
 * Licensed under the BSD license, see LICENSE in OpenBOR root for details.
 *
 * Copyright (c) 2004 - 2011 OpenBOR Team
 */

#include <SDL/SDL.h>
#include "video.h"
#include "globals.h"
#include "control.h"
#include "stristr.h"
#include "sblaster.h"
#include "joysticks.h"
#include "openbor.h"

SDL_Joystick *joystick[JOY_LIST_TOTAL];
s_joysticks joysticks[JOY_LIST_TOTAL];

static int usejoy;
static int numjoy;
static int lastkey;
static int lastjoy;

/*
 * Optimized Single-Pass Control Event Polling for Embedded Hardware
 * Safely populates unified bitmasks for low-overhead button grids
 */
void getPads(Uint8 * keystate) {
	int i, axis;
	SDL_Event ev;

	// Reset state buffers efficiently on every loop trace
	for(i = 0; i < JOY_LIST_TOTAL; i++) {
		joysticks[i].Data = 0;
		joysticks[i].Axes = 0;
		joysticks[i].Hats = 0;
		joysticks[i].Buttons = 0;
	}

	// Single-pass event pump processes keyboard states and system signals smoothly
	while(SDL_PollEvent(&ev)) {
		switch (ev.type) {
			case SDL_KEYDOWN:
				lastkey = ev.key.keysym.sym;
				if((keystate[SDLK_LALT] || keystate[SDLK_RALT]) && (lastkey == SDLK_RETURN)) {
					video_fullscreen_flip();
					keystate[SDLK_RETURN] = 0;
				}
				if(lastkey == SDLK_F10) {
					quit_game = 1;
					return;
				}
				break;

			case SDL_QUIT:
				quit_game = 1;
				return;

			case SDL_JOYBUTTONDOWN:
				if (ev.jbutton.which < JOY_LIST_TOTAL) {
					lastjoy = 1 + ev.jbutton.which * JOY_MAX_INPUTS + ev.jbutton.button;
				}
				break;

			default:
				break;
		}
	}

	// Dynamic real-time button remapping layer (Bypasses double event loops)
	if(usejoy) {
		SDL_JoystickUpdate();
		for(i = 0; i < numjoy; i++) {
			if (!joystick[i]) continue;

			// Flatten and query button loops tightly
			int max_btns = joysticks[i].NumButtons > 16 ? 16 : joysticks[i].NumButtons;
			for(int j = 0; j < max_btns; j++) {
				if (SDL_JoystickGetButton(joystick[i], j)) {
					joysticks[i].Buttons |= (1 << j);
				}
			}

			// Flatten axis maps (Handles hardware registers for Wii and DC sticks)
			int max_axes = joysticks[i].NumAxes > 4 ? 4 : joysticks[i].NumAxes;
			for(int j = 0; j < max_axes; j++) {
				axis = SDL_JoystickGetAxis(joystick[i], j);
				if(axis < -12000) {
					joysticks[i].Axes |= (1 << (j * 2));
				} else if(axis > +12000) {
					joysticks[i].Axes |= (2 << (j * 2));
				}
			}

			// Clean D-pad Hat mappings safely
			int max_hats = joysticks[i].NumHats > 2 ? 2 : joysticks[i].NumHats;
			for(int j = 0; j < max_hats; j++) {
				joysticks[i].Hats |= (SDL_JoystickGetHat(joystick[i], j) << (j * 4));
			}

			// Combine directly into a packed bitmask structure to flat-line CPU cycle overhead
			joysticks[i].Data = joysticks[i].Buttons;
			joysticks[i].Data |= (joysticks[i].Axes << max_btns);
			joysticks[i].Data |= (joysticks[i].Hats << (max_btns + (max_axes * 2)));
		}
	}
}

static int flag_to_index(u32 flag) {
	if (!flag) return 0;
	int index = 0;
	while (!(flag & 1) && index < 31) {
		flag >>= 1;
		index++;
	}
	return index;
}

void joystick_scan(int scan) {
	int i, j;
	if(!scan) return;
	
	numjoy = SDL_NumJoysticks();
	if(numjoy > JOY_LIST_TOTAL) numjoy = JOY_LIST_TOTAL;
	
	if(!numjoy) {
		printf("No Joystick(s) Found!\n");
		return;
	}
	
	for(i = 0; i < numjoy; i++) {
		joystick[i] = SDL_JoystickOpen(i);
		if (!joystick[i]) continue;
		
		joysticks[i].NumHats = SDL_JoystickNumHats(joystick[i]);
		joysticks[i].NumAxes = SDL_JoystickNumAxes(joystick[i]);
		joysticks[i].NumButtons = SDL_JoystickNumButtons(joystick[i]);
		joysticks[i].Name = SDL_JoystickName(i);
		
		for(j = 1; j < JOY_MAX_INPUTS + 1; j++) {
			joysticks[i].KeyName[j] = PC_GetJoystickKeyName(i, j);
		}
		
		printf("Input Port %d: %s Activated Successfully.\n", i + 1, joysticks[i].Name);
	}
}

void control_exit() {
	int i;
	usejoy = 0;
	for(i = 0; i < numjoy; i++) {
		if (joystick[i]) {
			SDL_JoystickClose(joystick[i]);
			joystick[i] = NULL;
		}
	}
	memset(joysticks, 0, sizeof(s_joysticks) * JOY_LIST_TOTAL);
	numjoy = 0;
}

void control_init(int joy_enable) {
	int i, j;
	usejoy = joy_enable;
	memset(joysticks, 0, sizeof(s_joysticks) * JOY_LIST_TOTAL);
	for(i = 0; i < JOY_LIST_TOTAL; i++) {
		for(j = 0; j < JOY_MAX_INPUTS + 1; j++) {
			joysticks[i].KeyName[j] = (j == 0) ? JoystickKeyName[0] : JoystickKeyName[j + (i * JOY_MAX_INPUTS)];
		}
	}
	joystick_scan(usejoy);
}

int control_usejoy(int enable) {
	usejoy = enable;
	return 0;
}

int control_getjoyenabled() {
	return usejoy;
}

void control_setkey(s_playercontrols * pcontrols, unsigned int flag, int key) {
	if(!pcontrols) return;
	pcontrols->settings[flag_to_index(flag)] = key;
	pcontrols->keyflags = pcontrols->newkeyflags = 0;
}

int keyboard_getlastkey() {
	int ret = lastkey;
	lastkey = 0;
	return ret;
}

int control_scankey() {
	static unsigned ready = 0;
	unsigned k = keyboard_getlastkey();
	unsigned j = lastjoy;
	lastjoy = 0;

	if(ready && (k || j)) {
		ready = 0;
		if(k) return k;
		if(j) return JOY_LIST_FIRST + j;
		return -1;
	}
	ready = (!k || !j);
	return 0;
}

char *control_getkeyname(unsigned int keycode) {
	int i;
	for(i = 0; i < JOY_LIST_TOTAL; i++) {
		unsigned int start = JOY_LIST_FIRST + 1 + (i * JOY_MAX_INPUTS);
		unsigned int end = JOY_LIST_FIRST + JOY_MAX_INPUTS + (i * JOY_MAX_INPUTS);
		if((keycode >= start) && (keycode <= end)) {
			return (char *) joysticks[i].KeyName[keycode - (JOY_LIST_FIRST + (i * JOY_MAX_INPUTS))];
		}
	}
	if(keycode > SDLK_FIRST && keycode < SDLK_LAST)
		return JOY_GetKeyName(keycode);
	return "...";
}

void control_update(s_playercontrols ** playercontrols, int numplayers) {
	unsigned k;
	unsigned i;
	int player, t;
	s_playercontrols *pcontrols;
	Uint8 *keystate = SDL_GetKeyState(NULL);

	getPads(keystate);
	if(quit_game) return;

	for(player = 0; player < numplayers; player++) {
		pcontrols = playercontrols[player];
		k = 0;

		// Tight pointer processing for the digital mapping array matrices
		for(i = 0; i < 32; i++) {
			t = pcontrols->settings[i];
			if(t >= SDLK_FIRST && t < SDLK_LAST) {
				if(keystate[t]) k |= (1 << i);
			}
		}

		if(usejoy && player < numjoy) {
			for(i = 0; i < 32; i++) {
				t = pcontrols->settings[i];
				if(t >= JOY_LIST_FIRST && t <= JOY_LIST_LAST) {
					int portnum = (t - JOY_LIST_FIRST - 1) / JOY_MAX_INPUTS;
					int shiftby = (t - JOY_LIST_FIRST - 1) % JOY_MAX_INPUTS;
					if(portnum == player) {
						if((joysticks[portnum].Data >> shiftby) & 1) k |= (1 << i);
					}
				}
			}
		}
		pcontrols->kb_break = 0;
		pcontrols->newkeyflags = k & (~pcontrols->keyflags);
		pcontrols->keyflags = k;
	}
}

void control_rumble(int port, int msec) {
	// Embedded rumble register parameters hook wrapper allocation pipeline
}
