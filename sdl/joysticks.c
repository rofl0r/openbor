/*
 * OpenBOR - http://lavalit.com
 * -----------------------------------------------------------------------
 * Licensed under the BSD license, see LICENSE in OpenBOR root for details.
 *
 * Copyright (c) 2004 - 2011 OpenBOR Team
 */

#include <SDL/SDL.h>
#include "joysticks.h"

// Unified fixed layout array size for retro console gamepads
const char *JoystickKeyName[JOY_NAME_SIZE] = {
	"...",
	"Up", "Right", "Down", "Left",
	"Button 1", "Button 2", "Button 3", "Button 4",
	"Button 5", "Button 6", "Button 7", "Button 8",
	"Button 9", "Button 10", "Button 11", "Button 12",
	"undefined"
};

// Sony layout names mapped directly for the PSP Hardware Grid layout
const char *SonyKeyName[JOY_NAME_SIZE] = {
	"...",
	"Up", "Right", "Down", "Left",
	"Select", "L Trigger", "R Trigger", "Start",
	"Triangle", "Circle", "Cross", "Square",
	"undefined"
};

// Mapped safely for Dreamcast or standard fallback setups
const char *MicrosoftKeyName[JOY_NAME_SIZE] = {
	"...",
	"Up", "Right", "Down", "Left",
	"A", "B", "X", "Y",
	"Left Trigger", "Right Trigger", "Back", "Start",
	"undefined"
};

// Gamepark / Legacy handheld layouts preserved
const char *GameparkKeyName[JOY_NAME_SIZE] = {
	"...",
	"Up", "Right", "Down", "Left",
	"Start", "Select", "L-Trigger", "R-Trigger",
	"A", "B", "X", "Y",
	"undefined"
};

// Enforced strict 32-bit bitmask layout array to optimize CPU branching
const int JoystickBits[JOY_MAX_INPUTS + 1] = {
	0x00000000,		// No Buttons Pressed
	0x00000001,		// Up / D-Pad Up
	0x00000002,		// Right / D-Pad Right
	0x00000004,		// Down / D-Pad Down
	0x00000008,		// Left / D-Pad Left
	0x00000010,		// Face Button 1 (Cross / A)
	0x00000020,		// Face Button 2 (Circle / B)
	0x00000040,		// Face Button 3 (Square / X)
	0x00000080,		// Face Button 4 (Triangle / Y)
	0x00000100,		// Left Shoulder / Trigger
	0x00000200,		// Right Shoulder / Trigger
	0x00000400,		// Select / Back
	0x00000800,		// Start
	0x00001000,		// Ext Button 1
	0x00002000,		// Ext Button 2
	0x00004000,		// Ext Button 3
	0x00008000		// Ext Button 4
};

// --- Stripped Out Massive Redundant PC String Arrays to Save System RAM ---

const char *PC_GetJoystickKeyName(int portnum, int keynum) {
#if defined(PSP)
	if(keynum < JOY_MAX_INPUTS) return SonyKeyName[keynum + 1];
#elif defined(WII) || defined(DREAMCAST)
	if(keynum < JOY_MAX_INPUTS) return MicrosoftKeyName[keynum + 1];
#endif
	if(keynum < JOY_MAX_INPUTS) return JoystickKeyName[keynum + 1];
	return "undefined";
}

#ifdef DINGOO
char *DINGOO_GetKeyName(int keycode) {
	if(keycode == DINGOO_BUTTON_UP)       return "Up";
	else if(keycode == DINGOO_BUTTON_DOWN)   return "Down";
	else if(keycode == DINGOO_BUTTON_LEFT)   return "Left";
	else if(keycode == DINGOO_BUTTON_RIGHT)  return "Right";
	else if(keycode == DINGOO_BUTTON_A)      return "A";
	else if(keycode == DINGOO_BUTTON_B)      return "B";
	else if(keycode == DINGOO_BUTTON_X)      return "X";
	else if(keycode == DINGOO_BUTTON_Y)      return "Y";
	else if(keycode == DINGOO_BUTTON_L)      return "L";
	else if(keycode == DINGOO_BUTTON_R)      return "R";
	else if(keycode == DINGOO_BUTTON_START)  return "Start";
	else if(keycode == DINGOO_BUTTON_SELECT) return "Select";
	return "...";
}
#endif

// Consolidated native console button remapper wrapper
char *JOY_GetKeyName(int keycode) {
#ifdef DINGOO
	return DINGOO_GetKeyName(keycode);
#elif defined(PSP)
	// Bind to native PSP button strings directly
	switch(keycode) {
		case SDLK_UP:     return "Up";
		case SDLK_DOWN:   return "Down";
		case SDLK_LEFT:   return "Left";
		case SDLK_RIGHT:  return "Right";
		case SDLK_RETURN: return "Start";
		case SDLK_ESCAPE: return "Select";
		default:          return "Button";
	}
#else
	return SDL_GetKeyName(keycode);
#endif
}
