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

#define appExit exit
#undef exit

char packfile[128] = { "bor.pak" };
char paksDir[128] = { "Paks" };
char savesDir[128] = { "Saves" };
char logsDir[128] = { "Logs" };
char screenShotsDir[128] = { "ScreenShots" };

void borExit(int reset) {
	SDL_Delay(10);
	appExit(0);
}

int main(int argc, char *argv[]) {
#ifdef CUSTOM_SIGNAL_HANDLER
	struct sigaction sigact;
#endif

#ifdef CUSTOM_SIGNAL_HANDLER
	sigact.sa_sigaction = handleFatalSignal;
	sigact.sa_flags = SA_RESTART | SA_SIGINFO;

	if(sigaction(SIGSEGV, &sigact, NULL) != 0) {
		printf("Error setting signal handler for %d (%s)\n", SIGSEGV, strsignal(SIGSEGV));
		exit(EXIT_FAILURE);
	}
#endif

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
