#include "openbor.h"
#include "movie.h"
#include "video.h"
#include "gfx.h"

void movie_options(void) {
	int quit = 0;
	int selector = 1;	// 0

	bothnewkeys = 0;

	while(!quit) {
		_menutextm(2, 1, 0, "Movie Mode");
		_menutextm((selector == 0), 2, 0, "Save Movie");
		_menutextm((selector == 1), 3, 0, "Load Movie");
		_menutextm((selector == 2), 5, 0, "Back");
		update(0, 0);

		if(bothnewkeys & FLAG_ESC)
			quit = 1;
		if(bothnewkeys & FLAG_MOVEUP) {
			--selector;
			if(SAMPLE_BEEP >= 0)
				sound_play_sample(SAMPLE_BEEP, 0, savedata.effectvol, savedata.effectvol, 100);
		}
		if(bothnewkeys & FLAG_MOVEDOWN) {
			++selector;
			if(SAMPLE_BEEP >= 0)
				sound_play_sample(SAMPLE_BEEP, 0, savedata.effectvol, savedata.effectvol, 100);
		}
		if(selector < 0)
			selector = 2;
		if(selector > 2)
			selector = 0;
		if(bothnewkeys & FLAG_ANYBUTTON) {

			if(SAMPLE_BEEP2 >= 0)
				sound_play_sample(SAMPLE_BEEP2, 0, savedata.effectvol, savedata.effectvol, 100);

			switch (selector) {
				case 0:
					movie_closefile();	//close first
					movie_openfile(1);	// save movie
					quit = 1;
					break;
				case 1:
					selector = 0;
					movie_closefile();	//close first
					movie_openfile(0);	// play movie
					quit = 1;
					break;
				default:
					quit = (bothnewkeys & FLAG_ANYBUTTON);
			}
		}
	}
	savesettings();
	bothnewkeys = 0;
}

void input_options(void) {
	int quit = 0;
	int selector = 1;	// 0

	bothnewkeys = 0;

	while(!quit) {
		_menutextm(2, -5, 0, "Control Options");

		if(savedata.usejoy) {
			_menutext((selector == 0), -4, -2, "GamePads Enabled");
			if(!control_getjoyenabled()) {
				_menutext((selector == 0), 7, -2, " - Device Not Ready");
			}
		} else
			_menutext((selector == 0), -4, -2, "GamePads Disabled");

		_menutext((selector == 1), -4, -1, "Setup Player 1...");
		_menutext((selector == 2), -4, 0, "Setup Player 2...");
		_menutext((selector == 3), -4, 1, "Setup Player 3...");
		_menutext((selector == 4), -4, 2, "Setup Player 4...");
		_menutextm((selector == 5), 7, 0, "Back");
		update(0, 0);

		if(bothnewkeys & FLAG_ESC)
			quit = 1;
		if(bothnewkeys & FLAG_MOVEUP) {
			--selector;
			if(SAMPLE_BEEP >= 0)
				sound_play_sample(SAMPLE_BEEP, 0, savedata.effectvol, savedata.effectvol, 100);
		}
		if(bothnewkeys & FLAG_MOVEDOWN) {
			++selector;
			if(SAMPLE_BEEP >= 0)
				sound_play_sample(SAMPLE_BEEP, 0, savedata.effectvol, savedata.effectvol, 100);
		}
		if(selector < 0)
			selector = 5;
		if(selector > 5)
			selector = 0;
		if(bothnewkeys & (FLAG_MOVELEFT | FLAG_MOVERIGHT | FLAG_ANYBUTTON)) {

			if(SAMPLE_BEEP2 >= 0)
				sound_play_sample(SAMPLE_BEEP2, 0, savedata.effectvol, savedata.effectvol, 100);

			switch (selector) {
				case 0:
					control_usejoy((savedata.usejoy ^= 1));
					break;
				case 1:
					keyboard_setup(0);
					break;
				case 2:
					keyboard_setup(1);
					break;
				case 3:
					keyboard_setup(2);
					break;
				case 4:
					keyboard_setup(3);
					break;
				default:
					quit = (bothnewkeys & FLAG_ANYBUTTON);
			}
		}
	}
	savesettings();
	bothnewkeys = 0;
}

void sound_options(void) {

	int quit = 0;
	int selector = 0;
	int dir;
	int col1 = -8;
	int col2 = 6;

	bothnewkeys = 0;

	while(!quit) {
		_menutextm(2, -5, 0, "Sound Options");
		_menutext((selector == 0), col1, -2, "Sound Volume:");
		_menutext((selector == 0), col2, -2, "%i", savedata.soundvol);
		_menutext((selector == 1), col1, -1, "SFX Volume:");
		_menutext((selector == 1), col2, -1, "%i", savedata.effectvol);
		_menutext((selector == 2), col1, 0, "Music Volume:");
		_menutext((selector == 2), col2, 0, "%i", savedata.musicvol);
		_menutext((selector == 3), col1, 1, "BGM:");
		_menutext((selector == 3), col2, 1, "%s", (savedata.usemusic ? "Enabled" : "Disabled"));
		_menutext((selector == 4), col1, 2, "Show Titles:");
		_menutext((selector == 4), col2, 2, "%s", (savedata.showtitles ? "Yes" : "No"));
		_menutext((selector == 5), col1, 3, "Advanced Options...");
		_menutextm((selector == 6), 7, 0, "Back");

		update(0, 0);

		if(bothnewkeys & FLAG_ESC)
			quit = 1;
		if(bothnewkeys & FLAG_MOVEUP) {
			--selector;

			if(SAMPLE_BEEP >= 0)
				sound_play_sample(SAMPLE_BEEP, 0, savedata.effectvol, savedata.effectvol, 100);
		}
		if(bothnewkeys & FLAG_MOVEDOWN) {
			++selector;

			if(SAMPLE_BEEP >= 0)
				sound_play_sample(SAMPLE_BEEP, 0, savedata.effectvol, savedata.effectvol, 100);
		}
		if(selector < 0)
			selector = 6;
		if(selector > 6)
			selector = 0;

		if(bothnewkeys & (FLAG_MOVELEFT | FLAG_MOVERIGHT | FLAG_ANYBUTTON)) {
			dir = 0;

			if(bothnewkeys & FLAG_MOVELEFT)
				dir = -1;
			else if(bothnewkeys & FLAG_MOVERIGHT)
				dir = 1;

			if(SAMPLE_BEEP2 >= 0)
				sound_play_sample(SAMPLE_BEEP2, 0, savedata.effectvol, savedata.effectvol, 100);

			switch (selector) {
				case 0:
					savedata.soundvol += dir;
					if(savedata.soundvol < 0)
						savedata.soundvol = 0;
					if(savedata.soundvol > 15)
						savedata.soundvol = 15;
					SB_setvolume(SB_VOICEVOL, savedata.soundvol);
					break;
				case 1:
					savedata.effectvol += 4 * dir;
					if(savedata.effectvol < 0)
						savedata.effectvol = 0;
					if(savedata.effectvol > 512)
						savedata.effectvol = 512;
					break;
				case 3:
					if(!dir)
						break;
					if(!savedata.usemusic) {
						savedata.usemusic = 1;
						music("data/music/remix", 1, 0);
					} else {
						savedata.usemusic = 0;
						sound_close_music();
					}
					break;
				case 2:
					savedata.musicvol += 4 * dir;
					if(savedata.musicvol < 0)
						savedata.musicvol = 0;
					if(savedata.musicvol > 512)
						savedata.musicvol = 512;
					sound_volume_music(savedata.musicvol, savedata.musicvol);
					break;
				case 4:
					savedata.showtitles = !savedata.showtitles;
					break;
				case 5:
					soundcard_options();
					break;
				default:
					quit = 1;
			}
		}
	}
	savesettings();
	bothnewkeys = 0;
}

void config_settings(void) {	//  OX. Load from / save to default.cfg. Restore OpenBoR "factory" settings.
	int quit = 0;
	int selector = 0;
	int dir = 0;
	int saved = 0;
	int loaded = 0;
	int restored = 0;

	bothnewkeys = 0;

	while(!quit) {
		_menutextm(2, -5, 0, "Configuration Settings");

		if(saved == 1)
			_menutextm((selector == 0), -2, 0, "Save Settings To Default.cfg%s", "  Done!");
		else
			_menutextm((selector == 0), -2, 0, "Save Settings To Default.cfg%s", "");

		if(loaded == 1)
			_menutextm((selector == 1), -1, 0, "Load Settings From Default.cfg%s", "  Done!");
		else
			_menutextm((selector == 1), -1, 0, "Load Settings From Default.cfg%s", "");

		if(restored == 1)
			_menutextm((selector == 2), 0, 0, "Restore OpenBoR Defaults%s", "  Done!");
		else
			_menutextm((selector == 2), 0, 0, "Restore OpenBoR Defaults%s", "");

		_menutextm((selector == 3), 1, 0, "Back");

		update(0, 0);

		if(bothnewkeys & FLAG_ESC)
			quit = 1;
		if(bothnewkeys & FLAG_MOVEUP) {
			--selector;

			if(SAMPLE_BEEP >= 0)
				sound_play_sample(SAMPLE_BEEP, 0, savedata.effectvol, savedata.effectvol, 100);
		}
		if(bothnewkeys & FLAG_MOVEDOWN) {
			++selector;

			if(SAMPLE_BEEP >= 0)
				sound_play_sample(SAMPLE_BEEP, 0, savedata.effectvol, savedata.effectvol, 100);
		}

		if(selector < 0)
			selector = 3;
		if(selector > 3)
			selector = 0;

		if(bothnewkeys & (FLAG_MOVELEFT | FLAG_MOVERIGHT | FLAG_ANYBUTTON)) {
			dir = 0;

			if(bothnewkeys & FLAG_MOVELEFT)
				dir = -1;
			else if(bothnewkeys & FLAG_MOVERIGHT)
				dir = 1;

			if(SAMPLE_BEEP2 >= 0)
				sound_play_sample(SAMPLE_BEEP2, 0, savedata.effectvol, savedata.effectvol, 100);

			switch (selector) {
				case 0:
					saveasdefault();
					saved = 1;
					break;

				case 1:
					loadfromdefault();
					//shutdown(2, "\nSettings Loaded From Default.cfg. Restart Required.\n\n");
					init_videomodes();
					SB_setvolume(SB_VOICEVOL, savedata.soundvol);
					sound_volume_music(savedata.musicvol, savedata.musicvol);
					loaded = 1;
					break;
				case 2:
					clearsettings();
					//shutdown(2, "\nSettings Loaded From Default.cfg. Restart Required.\n\n");
					init_videomodes();
					SB_setvolume(SB_VOICEVOL, savedata.soundvol);
					sound_volume_music(savedata.musicvol, savedata.musicvol);
					restored = 1;
					break;
				default:
					quit = 1;
			}
		}
	}
	savesettings();
	bothnewkeys = 0;
}

static inline void toggle(int *dest) {
	*dest = !(*dest);
}

void cheatoptions(void) {	//  LTB 1-13-05 took out sameplayer option
	int quit = 0;
	int selector = 0;
	int dir;
	int col1 = -8;
	int col2 = 6;

	bothnewkeys = 0;

	while(!quit) {
		_menutextm(2, -5, 0, "Cheat Options");
		_menutext((selector == 0), col1, -3, "Brightness:");
		_menutext((selector == 0), col2, -3, "%i", savedata.brightness);
		_menutext((selector == 1), col1, -2, "Gamma:");
		_menutext((selector == 1), col2, -2, "%i", savedata.gamma);
		_menutext((selector == 2), col1, -1, "Control Options...");
		_menutext((selector == 3), col1, 0, "Sound Options...");
		_menutext((selector == 4), col1, 1, "System Options...");

		if(livescheat)
			_menutext((selector == 5), col1, 2, "Infinite Lives On");
		else
			_menutext((selector == 5), col1, 2, "Infinite Lives Off");

		if(creditscheat)
			_menutext((selector == 6), col1, 3, "Infinite Credits On");	// Enemies fall down when you respawn
		else
			_menutext((selector == 6), col1, 3, "Infinite Credits Off");	//Enemies don't fall down when you respawn
		if(healthcheat)
			_menutext((selector == 7), col1, 4, "Infinite Health On");	// Enemies fall down when you respawn
		else
			_menutext((selector == 7), col1, 4, "Infinite Health Off");	//Enemies don't fall down when you respawn

		_menutextm((selector == 8), 7, 0, "Back");

		update(0, 0);

		if(bothnewkeys & FLAG_ESC)
			quit = 1;
		if(bothnewkeys & FLAG_MOVEUP) {
			--selector;

			if(SAMPLE_BEEP >= 0)
				sound_play_sample(SAMPLE_BEEP, 0, savedata.effectvol, savedata.effectvol, 100);
		}
		if(bothnewkeys & FLAG_MOVEDOWN) {
			++selector;

			if(SAMPLE_BEEP >= 0)
				sound_play_sample(SAMPLE_BEEP, 0, savedata.effectvol, savedata.effectvol, 100);
		}

		if(selector < 0)
			selector = 8;	// 7-1-2005 //13-1-2005 changed to 11
		if(selector > 8)
			selector = 0;	//    7-1-2005 6 changed to 10 //13-1-2005 changed to 11

		if(bothnewkeys & (FLAG_MOVELEFT | FLAG_MOVERIGHT | FLAG_ANYBUTTON)) {
			dir = 0;

			if(bothnewkeys & FLAG_MOVELEFT)
				dir = -1;
			else if(bothnewkeys & FLAG_MOVERIGHT)
				dir = 1;

			if(SAMPLE_BEEP2 >= 0)
				sound_play_sample(SAMPLE_BEEP2, 0, savedata.effectvol, savedata.effectvol, 100);

			switch (selector) {
				case 0:
					savedata.brightness += 8 * dir;
					if(savedata.brightness < -256)
						savedata.brightness = -256;
					if(savedata.brightness > 256)
						savedata.brightness = 256;
					vga_vwait();
					palette_set_corrected(pal, savedata.gamma, savedata.gamma, savedata.gamma,
							      savedata.brightness, savedata.brightness,
							      savedata.brightness);
					break;
				case 1:
					savedata.gamma += 8 * dir;
					if(savedata.gamma < -256)
						savedata.gamma = -256;
					if(savedata.gamma > 256)
						savedata.gamma = 256;
					vga_vwait();
					palette_set_corrected(pal, savedata.gamma, savedata.gamma, savedata.gamma,
							      savedata.brightness, savedata.brightness,
							      savedata.brightness);
					break;
				case 2:
					input_options();
					break;
				case 3:
					sound_options();
					break;
				case 4:
					system_options();
					break;
				case 5:
					toggle(&livescheat);
					break;
				case 6:
					toggle(&creditscheat);
					break;
				case 7:
					toggle(&healthcheat);
					break;

				default:
					quit = 1;
			}
		}
	}
	savesettings();
	bothnewkeys = 0;
}


void system_options(void) {

	int quit = 0;
	int selector = 0;
	int dir = 0;
	int ret = 6;
	int col1 = -8;
	int col2 = 5;

	bothnewkeys = 0;

	while(!quit) {
		_menutextm(2, -5, 0, "System Options");

		_menutext((selector == 0), col1, 0, "Debug Info:");
		_menutext((selector == 0), col2, 0, (savedata.debuginfo ? "Enabled" : "Disabled"));

		_menutext((selector == 1), col1, 1, "File Logging:");
		_menutext((selector == 1), col2, 1, (savedata.uselog ? "Enabled" : "Disabled"));

		_menutext((selector == 2), col1, 2, "Players: ");
		if(!ctrlmaxplayers[current_set])
			_menutext((selector == 2), col2, 2, "%i", maxplayers[current_set]);
		else
			_menutext((selector == 2), col2, 2, "%i by Mod", maxplayers[current_set]);

		_menutext((selector == 3), col1, 3, "Versus Damage:", 0);
		if(versusdamage == 0)
			_menutext((selector == 3), col2, 3, "Disabled by Mod");
		else if(versusdamage == 1)
			_menutext((selector == 3), col2, 3, "Enabled by Mod");
		else {
			if(savedata.mode)
				_menutext((selector == 3), col2, 3, "Disabled");	//Mode 1 - Players CAN'T attack each other
			else
				_menutext((selector == 3), col2, 3, "Enabled");	//Mode 2 - Players CAN attack each other
		}

		_menutext((selector == 4), col1, 4, "Cheats:");
		_menutext((selector == 4), col2, 4, forcecheatsoff ? "Disabled by Mod" : (cheats ? "On" : "Off"));

		_menutext((selector == 5), col1, 5, "Config Settings");

		_menutextm((selector == ret), 8, 0, "Back");

		update(0, 0);

		if(bothnewkeys & FLAG_ESC)
			quit = 1;
		if(bothnewkeys & FLAG_MOVEUP) {
			--selector;
			sound_play_sample(SAMPLE_BEEP, 0, savedata.effectvol, savedata.effectvol, 100);
		}
		if(bothnewkeys & FLAG_MOVEDOWN) {
			++selector;
			sound_play_sample(SAMPLE_BEEP, 0, savedata.effectvol, savedata.effectvol, 100);
		}

		if(selector < 0)
			selector = ret;
		if(selector > ret)
			selector = 0;

		if(bothnewkeys & (FLAG_MOVELEFT | FLAG_MOVERIGHT | FLAG_ANYBUTTON)) {
			dir = 0;
			if(bothnewkeys & FLAG_MOVELEFT)
				dir = -1;
			else if(bothnewkeys & FLAG_MOVERIGHT)
				dir = 1;
			sound_play_sample(SAMPLE_BEEP2, 0, savedata.effectvol, savedata.effectvol, 100);

			switch (selector) {
				case 0:
					savedata.debuginfo = !savedata.debuginfo;
					break;

				case 1:
					savedata.uselog = !savedata.uselog;
					break;

				case 2:
					if(!ctrlmaxplayers[current_set]) {
						if(maxplayers[current_set] == 2)
							maxplayers[current_set] = 4;
						else
							maxplayers[current_set] = 2;
					}
					break;

				case 3:
					if(versusdamage > 1) {
						if(savedata.mode)
							savedata.mode = 0;
						else
							savedata.mode = 1;
					}
					break;

				case 4:
					toggle(&cheats);
					break;
				case 5:
					config_settings();
					break;
				default:
					quit = 1;
					break;
			}
		}
	}
	savesettings();
	bothnewkeys = 0;
}


void video_options(void) {
	int quit = 0;
	int selector = 0;
	int dir;
	int col1 = -8, col2 = 6;

	bothnewkeys = 0;

	while(!quit) {
		_menutextm(2, -5, 0, "Video Options");
		_menutext((selector == 0), col1, -3, "Brightness:");
		_menutext((selector == 0), col2, -3, "%i", savedata.brightness);
		_menutext((selector == 1), col1, -2, "Gamma:");
		_menutext((selector == 1), col2, -2, "%i", savedata.gamma);
		_menutext((selector == 2), col1, -1, "Window Offset:");
		_menutext((selector == 2), col2, -1, "%i", savedata.windowpos);
		_menutext((selector == 3), col1, 0, "Display Mode:");
		_menutext((selector == 3), col2, 0, "%s", savedata.fullscreen ? "Full" : "Window");

		_menutext((selector == 4), col1, 1, "Video Backend:");
		_menutext((selector == 4), col2, 1, savedata.fullscreen ? "Automatic (%s)" : "%s",
			  opengl ? "OpenGL" : "SDL");

		if(opengl) {
			_menutext((selector == 5), col1, 2, "Screen:");
			if(savedata.fullscreen)
				_menutext((selector == 5), col2, 2, "Automatic");
			else
				_menutext((selector == 5), col2, 2, "%4.2fx - %ix%i", savedata.glscale,
					  (int) (videomodes.hRes * savedata.glscale),
					  (int) (videomodes.vRes * savedata.glscale));

			_menutext((selector == 6), col1, 3, "Filters:");
			if(savedata.fullscreen)
				_menutext((selector == 6), col2, 3, "Automatic (Bilinear)");
			else
				_menutext((selector == 6), col2, 3, "%s",
					  savedata.glscale !=
					  1.0 ? (savedata.glfilter ? "Simple" : "Bilinear") : "Disabled");
		} else {
			_menutext((selector == 5), col1, 2, "Screen:");
			if(savedata.screen[videoMode][0])
				_menutext((selector == 3), col2, 2, "%ix - %ix%i", savedata.screen[videoMode][0],
					  videomodes.hRes * savedata.screen[videoMode][0],
					  videomodes.vRes * savedata.screen[videoMode][0]);
			else
				_menutext((selector == 5), col2, 2, "Disabled");

			_menutext((selector == 6), col1, 3, "Filters:");
			_menutext((selector == 6), col2, 3, "%s",
				  savedata.screen[videoMode][0] ==
				  2 ? GfxBlitterNames[(int) savedata.screen[videoMode][1]] : "Disabled");
		}

		if(savedata.fullscreen) {
			_menutext((selector == 7), col1, 4, "Fullscreen Type:");
			_menutext((selector == 7), col2, 4, "%s",
				  savedata.stretch ? "Stretch to Screen" : "Preserve Aspect Ratio");
		} else if(selector == 7)
			selector = (bothnewkeys & FLAG_MOVEUP) ? 6 : 8;

		_menutextm((selector == 8), 7, 0, "Back");
		if(selector < 0)
			selector = 8;
		if(selector > 8)
			selector = 0;

		update(0, 0);

		if(bothnewkeys & FLAG_ESC)
			quit = 1;
		if(bothnewkeys & FLAG_MOVEUP) {
			--selector;
			if(SAMPLE_BEEP >= 0)
				sound_play_sample(SAMPLE_BEEP, 0, savedata.effectvol, savedata.effectvol, 100);
		}
		if(bothnewkeys & FLAG_MOVEDOWN) {
			++selector;
			if(SAMPLE_BEEP >= 0)
				sound_play_sample(SAMPLE_BEEP, 0, savedata.effectvol, savedata.effectvol, 100);
		}
		if(bothnewkeys & (FLAG_MOVELEFT | FLAG_MOVERIGHT | FLAG_ANYBUTTON)) {
			dir = 0;

			if(bothnewkeys & FLAG_MOVELEFT)
				dir = -1;
			else if(bothnewkeys & FLAG_MOVERIGHT)
				dir = 1;

			if(SAMPLE_BEEP2 >= 0)
				sound_play_sample(SAMPLE_BEEP2, 0, savedata.effectvol, savedata.effectvol, 100);

			switch (selector) {
				case 0:
					savedata.brightness += 8 * dir;
					if(savedata.brightness < -256)
						savedata.brightness = -256;
					if(savedata.brightness > 256)
						savedata.brightness = 256;
					vga_vwait();
					set_color_correction(savedata.gamma, savedata.brightness);
					break;
				case 1:
					savedata.gamma += 8 * dir;
					if(savedata.gamma < -256)
						savedata.gamma = -256;
					if(savedata.gamma > 256)
						savedata.gamma = 256;
					vga_vwait();
					set_color_correction(savedata.gamma, savedata.brightness);
					break;
				case 2:
					savedata.windowpos += dir;
					if(savedata.windowpos < -2)
						savedata.windowpos = -2;
					if(savedata.windowpos > 20)
						savedata.windowpos = 20;
					break;
				case 3:
					video_fullscreen_flip();
					break;
				case 4:
					if(savedata.fullscreen)
						break;
					savedata.usegl ^= 1;
					video_set_mode(videomodes);
					set_color_correction(savedata.gamma, savedata.brightness);
					break;
				case 5:
					if(opengl) {
						if(savedata.fullscreen)
							break;
						savedata.glscale += dir * 0.25;
						if(savedata.glscale < 0.25)
							savedata.glscale = 0.25;
						if(savedata.glscale > 4.00)
							savedata.glscale = 4.00;
						video_set_mode(videomodes);
					} else {
						videomodes.mode += dir * 2;
						if(videomodes.mode > 4)
							videomodes.mode = 0;
						if(videomodes.mode < 0)
							videomodes.mode = 4;
						savedata.screen[videoMode][0] = videomodes.mode;
						video_set_mode(videomodes);
						change_system_palette(current_palette);
					}
					break;
				case 6:
					if(opengl) {
						if(savedata.fullscreen || (savedata.glscale == 1.0))
							break;
						savedata.glfilter += dir;
						if(savedata.glfilter < 0)
							savedata.glfilter = 1;
						if(savedata.glfilter > 1)
							savedata.glfilter = 0;
					} else {
						if(videomodes.mode != 2)
							break;
						videomodes.filter += dir;
						if(videomodes.filter > BLITTER_MAX - 1)
							videomodes.filter = 0;
						if(videomodes.filter < 0)
							videomodes.filter = BLITTER_MAX - 1;
						savedata.screen[videoMode][1] = videomodes.filter;
					}
					break;
				case 7:
					video_stretch((savedata.stretch ^= 1));
					break;
				default:
					quit = 1;
			}
		}
	}
	savesettings();
	bothnewkeys = 0;
}

void options(void) {
	int quit = 0;
	int selector = 0;
	int dir;

	bothnewkeys = 0;

	while(!quit) {
		_menutextm(2, 0, 0, "Options");
		_menutextm((selector == 0), 2, 0, "Video Options...");
		_menutextm((selector == 1), 3, 0, "Sound Options...");
		_menutextm((selector == 2), 4, 0, "Control Options...");
		_menutextm((selector == 3), 5, 0, "System Options...");
		_menutextm((selector == 4), 7, 0, "Back");

		if(selector < 0)
			selector = 4;
		if(selector > 4)
			selector = 0;

		update(0, 0);

		if(bothnewkeys & FLAG_ESC)
			quit = 1;
		if(bothnewkeys & FLAG_MOVEUP) {
			--selector;

			if(SAMPLE_BEEP >= 0)
				sound_play_sample(SAMPLE_BEEP, 0, savedata.effectvol, savedata.effectvol, 100);
		}
		if(bothnewkeys & FLAG_MOVEDOWN) {
			++selector;

			if(SAMPLE_BEEP >= 0)
				sound_play_sample(SAMPLE_BEEP, 0, savedata.effectvol, savedata.effectvol, 100);
		}
		if(bothnewkeys & (FLAG_MOVELEFT | FLAG_MOVERIGHT | FLAG_ANYBUTTON)) {
			dir = 0;

			if(bothnewkeys & FLAG_MOVELEFT)
				dir = -1;
			else if(bothnewkeys & FLAG_MOVERIGHT)
				dir = 1;

			if(SAMPLE_BEEP2 >= 0)
				sound_play_sample(SAMPLE_BEEP2, 0, savedata.effectvol, savedata.effectvol, 100);

			switch (selector) {
				case 0:
					video_options();
					break;
				case 1:
					sound_options();
					break;
				case 2:
					input_options();
					break;
				case 3:
					system_options();
					break;
				default:
					quit = 1;
			}
		}
	}
	savesettings();
	if(pause == 1)
		pause = 2;
	bothnewkeys = 0;
}

void soundcard_options(void) {
	int quit = 0;
	int selector = 0;
	int col1 = -8, col2 = 6;

	savesettings();

	bothnewkeys = 0;

	while(!quit) {
		_menutextm(2, -5, 0, "Advanced Sound Options");
		_menutext((selector == 0), col1, -2, "Frequency:");
		_menutext((selector == 0), col2, -2, "%i", savedata.soundrate);
		_menutext((selector == 1), col1, -1, "Bits:");
		_menutext((selector == 1), col2, -1, "%i", savedata.soundbits);
		_menutextm((selector == 2), 1, 0, "Apply");
		_menutextm((selector == 3), 2, 0, "Discard");
		_menutextm((selector == 4), 7, 0, "Back");
		update(0, 0);

		if(bothnewkeys & FLAG_ESC)
			quit = 1;
		if(bothnewkeys & FLAG_MOVEUP) {
			--selector;
			sound_play_sample(SAMPLE_BEEP, 0, savedata.effectvol, savedata.effectvol, 100);
		}
		if(bothnewkeys & FLAG_MOVEDOWN) {
			++selector;
			sound_play_sample(SAMPLE_BEEP, 0, savedata.effectvol, savedata.effectvol, 100);
		}
		if(selector < 0)
			selector = 4;
		selector %= 5;
		if(bothnewkeys & (FLAG_MOVELEFT | FLAG_MOVERIGHT | FLAG_ANYBUTTON)) {
			sound_play_sample(SAMPLE_BEEP2, 0, savedata.effectvol, savedata.effectvol, 100);
			switch (selector) {
				case 0:
					if(bothnewkeys & FLAG_MOVELEFT)
						savedata.soundrate >>= 1;
					if(bothnewkeys & FLAG_MOVERIGHT)
						savedata.soundrate <<= 1;
					if(savedata.soundrate < 11025)
						savedata.soundrate = 44100;
					if(savedata.soundrate > 44100)
						savedata.soundrate = 11025;
					break;
				case 1:
					savedata.soundbits = (savedata.soundbits ^ (8 + 16));
					if(savedata.soundbits != 8 && savedata.soundbits != 16)
						savedata.soundbits = 8;
					break;
				case 2:
					if(!(bothnewkeys & FLAG_ANYBUTTON))
						break;
					// Apply new hardware settings
					sound_stop_playback();
					if(!sound_start_playback(savedata.soundbits, savedata.soundrate)) {
						savedata.soundbits = 8;
						savedata.soundrate = 11025;
						sound_start_playback(savedata.soundbits, savedata.soundrate);
					}
					music("data/music/remix", 1, 0);
					savesettings();
					break;
				case 3:
					if(bothnewkeys & FLAG_ANYBUTTON)
						loadsettings();
					break;
				default:
					quit = (bothnewkeys & FLAG_ANYBUTTON);
			}
		}
	}
	loadsettings();
	bothnewkeys = 0;
}
