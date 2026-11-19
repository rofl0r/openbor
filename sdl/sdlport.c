/*
 * OpenBOR - http://www.LavaLit.com
 * -----------------------------------------------------------------------
 * Licensed under the BSD license, see LICENSE in OpenBOR root for details.
 *
 * Copyright (c) 2004 - 2011 OpenBOR Team
 */

#include "sdlport.h"
#include "packfile.h"
#include "video.h"
#include "menu.h"
#include "../source/crashhandler.h"

#define appExit exit
#undef exit

char packfile[128] = { "bor.pak" };
char savesDir[128] = { "Saves" };
char logsDir[128] = { "Logs" };
char screenShotsDir[128] = { "ScreenShots" };

void borExit(int reset) {
	SDL_Delay(10);
	appExit(0);
}

int main(int argc, char *argv[]) {

	/* Install as early as possible so native faults during SDL startup are logged. */
	bor_install_crash_handler();

	initSDL();

	packfile_mode(0);

	dirExists(paksDir, 1);
	dirExists(savesDir, 1);
	dirExists(logsDir, 1);
	dirExists(screenShotsDir, 1);

	Menu();
	openborMain(argc, argv);
	borExit(0);
	return 0;
}
