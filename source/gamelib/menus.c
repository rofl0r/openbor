#include "openbor.h"
#include "movie.h"
#include "video.h"
#include "gfx.h"

typedef int (*menu_toggle_callback)(int direction, int* quit, int* selector, void* data);

static int menu_handle_input(int max_entry, int button_only, menu_toggle_callback cb, int* quit, int* selector, void* data) {
	int dir;
	if(bothnewkeys & FLAG_ESC)
		*quit = 1;
	if(bothnewkeys & FLAG_MOVEUP) {
		--(*selector);
		sound_play_sample(samples.beep, 0, savedata.effectvol, savedata.effectvol, 100);
	}
	if(bothnewkeys & FLAG_MOVEDOWN) {
		++(*selector);
		sound_play_sample(samples.beep, 0, savedata.effectvol, savedata.effectvol, 100);
	}
	if(*selector < 0)
		*selector = max_entry;
	if(*selector > max_entry)
		*selector = 0;
	if(
		(!button_only && bothnewkeys & (FLAG_MOVELEFT | FLAG_MOVERIGHT | FLAG_ANYBUTTON)) ||
		(bothnewkeys & FLAG_ANYBUTTON)
	) {
		sound_play_sample(samples.beep2, 0, savedata.effectvol, savedata.effectvol, 100);
		
		dir = 0;
		if(!button_only) {
			if(bothnewkeys & FLAG_MOVELEFT)
				dir = -1;
			else if(bothnewkeys & FLAG_MOVERIGHT)
				dir = 1;
		}
		return cb(dir, quit, selector, data);
	}
	return -1;
}

static int movie_options_toggle_cb(int direction, int* quit, int* selector, void* data) {
	(void) direction;
	switch (*selector) {
		case 0:
			movie_closefile();	//close first
			movie_openfile(1);	// save movie
			*quit = 1;
			break;
		case 1:
			*selector = 0;
			movie_closefile();	//close first
			movie_openfile(0);	// play movie
			*quit = 1;
			break;
		default:
			*quit = (bothnewkeys & FLAG_ANYBUTTON);
	}
	return 0;
}

void movie_options(void) {
	int quit = 0;
	int selector = 1;

	bothnewkeys = 0;

	while(!quit) {
		_menutextm(2, 1, 0, "Movie Mode");
		_menutextm((selector == 0), 2, 0, "Save Movie");
		_menutextm((selector == 1), 3, 0, "Load Movie");
		_menutextm((selector == 2), 5, 0, "Back");
		update(0, 0);
		menu_handle_input(2, 1, movie_options_toggle_cb, &quit, &selector, NULL);
	}
	savesettings();
	bothnewkeys = 0;
}

static int input_options_toggle_cb(int direction, int* quit, int* selector, void* data) {
	(void) direction;
	(void) data;
	switch (*selector) {
		case 0:
			control_usejoy((savedata.usejoy ^= 1));
			break;
		case 1: case 2: case 3: case 4:
			keyboard_setup(*selector -1);
			break;
		default:
			*quit = (bothnewkeys & FLAG_ANYBUTTON);
	}
	return 0;
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
		menu_handle_input(5, 1, input_options_toggle_cb, &quit, &selector, NULL);

	}
	savesettings();
	bothnewkeys = 0;
}

static int sound_options_toggle_cb(int direction, int* quit, int* selector, void* data) {
	(void) data;
	switch (*selector) {
		case 0:
			savedata.soundvol += direction;
			if(savedata.soundvol < 0)
				savedata.soundvol = 0;
			if(savedata.soundvol > 15)
				savedata.soundvol = 15;
			SB_setvolume(SB_VOICEVOL, savedata.soundvol);
			break;
		case 1:
			savedata.effectvol += 4 * direction;
			if(savedata.effectvol < 0)
				savedata.effectvol = 0;
			if(savedata.effectvol > 512)
				savedata.effectvol = 512;
			break;
		case 3:
			if(!direction)
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
			savedata.musicvol += 4 * direction;
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
			*quit = 1;
	}
	return 0;
}

void sound_options(void) {

	int quit = 0;
	int selector = 0;
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
		menu_handle_input(6, 0, sound_options_toggle_cb, &quit, &selector, NULL);
	}
	savesettings();
	bothnewkeys = 0;
}

typedef struct {
	int saved;
	int loaded;
	int restored;
} config_settings_cb_data;

static int config_settings_toggle_cb(int direction, int* quit, int* selector, void* data) {
	(void) direction;
	config_settings_cb_data *cb_data = (config_settings_cb_data*) data;
	switch (*selector) {
		case 0:
			saveasdefault();
			cb_data->saved = 1;
			break;

		case 1:
			loadfromdefault();
			//shutdown(2, "\nSettings Loaded From Default.cfg. Restart Required.\n\n");
			init_videomodes();
			SB_setvolume(SB_VOICEVOL, savedata.soundvol);
			sound_volume_music(savedata.musicvol, savedata.musicvol);
			cb_data->loaded = 1;
			break;
		case 2:
			clearsettings();
			//shutdown(2, "\nSettings Loaded From Default.cfg. Restart Required.\n\n");
			init_videomodes();
			SB_setvolume(SB_VOICEVOL, savedata.soundvol);
			sound_volume_music(savedata.musicvol, savedata.musicvol);
			cb_data->restored = 1;
			break;
		default:
			*quit = 1;
	}
	return 0;
}

void config_settings(void) {	//  OX. Load from / save to default.cfg. Restore OpenBoR "factory" settings.
	int quit = 0;
	int selector = 0;
	config_settings_cb_data cb_data = {0};

	bothnewkeys = 0;

	while(!quit) {
		_menutextm(2, -5, 0, "Configuration Settings");

		if(cb_data.saved == 1)
			_menutextm((selector == 0), -2, 0, "Save Settings To Default.cfg%s", "  Done!");
		else
			_menutextm((selector == 0), -2, 0, "Save Settings To Default.cfg%s", "");

		if(cb_data.loaded == 1)
			_menutextm((selector == 1), -1, 0, "Load Settings From Default.cfg%s", "  Done!");
		else
			_menutextm((selector == 1), -1, 0, "Load Settings From Default.cfg%s", "");

		if(cb_data.restored == 1)
			_menutextm((selector == 2), 0, 0, "Restore OpenBoR Defaults%s", "  Done!");
		else
			_menutextm((selector == 2), 0, 0, "Restore OpenBoR Defaults%s", "");

		_menutextm((selector == 3), 1, 0, "Back");

		update(0, 0);
		menu_handle_input(3, 1, config_settings_toggle_cb, &quit, &selector, &cb_data);
	}
	savesettings();
	bothnewkeys = 0;
}

static inline void toggle(int *dest) {
	*dest = !(*dest);
}

static int cheat_options_toggle_cb(int direction, int* quit, int* selector, void* data) {
	(void) data;
	switch (*selector) {
		case 0:
			toggle(&livescheat);
			break;
		case 1:
			toggle(&creditscheat);
			break;
		case 2:
			toggle(&healthcheat);
			break;

		default:
			*quit = 1;
	}
	return 0;
}

void cheat_options(void) {	//  LTB 1-13-05 took out sameplayer option
	int quit = 0;
	int selector = 0;
	int col1 = -8;

	bothnewkeys = 0;

	while(!quit) {
		_menutextm(2, -5, 0, "Cheat Options");
		_menutext((selector == 0), col1, 0, livescheat ? "Infinite Lives ""On" : "Infinite Lives ""Off");
		_menutext((selector == 1), col1, 1, creditscheat ? "Infinite Credits ""On" : "Infinite Credits ""Off");
		_menutext((selector == 2), col1, 2, healthcheat ? "Infinite Health ""On" : "Infinite Health ""Off");
		_menutextm((selector == 3), 4, 0, "Back");

		update(0, 0);
		menu_handle_input(8, 0, cheat_options_toggle_cb, &quit, &selector, NULL);
	}
	savesettings();
	bothnewkeys = 0;
}

static int system_options_toggle_cb(int direction, int* quit, int* selector, void* data) {
	(void) direction;
	(void) data;
	switch (*selector) {
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
			*quit = 1;
			break;
	}
	return 0;
}

void system_options(void) {

	int quit = 0;
	int selector = 0;
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
		menu_handle_input(ret, 0, system_options_toggle_cb, &quit, &selector, NULL);

	}
	savesettings();
	bothnewkeys = 0;
}

static int video_options_toggle_cb(int direction, int* quit, int* selector, void* data) {
	(void) data;
	switch (*selector) {
		case 0:
			savedata.brightness += 8 * direction;
			if(savedata.brightness < -256)
				savedata.brightness = -256;
			if(savedata.brightness > 256)
				savedata.brightness = 256;
			vga_vwait();
			set_color_correction(savedata.gamma, savedata.brightness);
			break;
		case 1:
			savedata.gamma += 8 * direction;
			if(savedata.gamma < -256)
				savedata.gamma = -256;
			if(savedata.gamma > 256)
				savedata.gamma = 256;
			vga_vwait();
			set_color_correction(savedata.gamma, savedata.brightness);
			break;
		case 2:
			savedata.windowpos += direction;
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
				savedata.glscale += direction * 0.25;
				if(savedata.glscale < 0.25)
					savedata.glscale = 0.25;
				if(savedata.glscale > 4.00)
					savedata.glscale = 4.00;
				video_set_mode(videomodes);
			} else {
				videomodes.mode += direction * 2;
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
				savedata.glfilter += direction;
				if(savedata.glfilter < 0)
					savedata.glfilter = 1;
				if(savedata.glfilter > 1)
					savedata.glfilter = 0;
			} else {
				if(videomodes.mode != 2)
					break;
				videomodes.filter += direction;
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
			*quit = 1;
	}
	return 0;
}

void video_options(void) {
	int quit = 0;
	int selector = 0;
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

		update(0, 0);
		menu_handle_input(8, 0, video_options_toggle_cb, &quit, &selector, NULL);

	}
	savesettings();
	bothnewkeys = 0;
}

static int options_toggle_cb(int direction, int* quit, int* selector, void* data) {
	(void) data;
	(void) direction;
	switch (*selector) {
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
			if(cheats && *selector == 4)
				cheat_options();
			else 
				*quit = 1;
	}
	return 0;
}

void options(void) {
	int quit = 0;
	int selector = 0;

	bothnewkeys = 0;

	while(!quit) {
		_menutextm(2, 0, 0, "Options");
		_menutextm((selector == 0), 2, 0, "Video Options...");
		_menutextm((selector == 1), 3, 0, "Sound Options...");
		_menutextm((selector == 2), 4, 0, "Control Options...");
		_menutextm((selector == 3), 5, 0, "System Options...");
		if(cheats) _menutextm((selector == 4), 6, 0, "Cheat Options...");
		_menutextm((selector == 4 + cheats), 7 + cheats, 0, "Back");

		update(0, 0);
		menu_handle_input(4 + cheats, 1, options_toggle_cb, &quit, &selector, NULL);
	}
	savesettings();
	if(pause == 1)
		pause = 2;
	bothnewkeys = 0;
}

static int soundcard_options_toggle_cb(int direction, int* quit, int* selector, void* data) {
	(void) data;
	int temprate;
	switch (*selector) {
		case 0:
			if(direction == -1)
				temprate = savedata.soundrate / 2;
			else
				temprate = (int) savedata.soundrate * 2;
			savedata.soundrate = temprate > 44100 ? 11025 : temprate < 11025 ? 44100 : temprate;
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
			*quit = (bothnewkeys & FLAG_ANYBUTTON);
	}
	return 0;
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
		menu_handle_input(4, 0, soundcard_options_toggle_cb, &quit, &selector, NULL);
	}
	loadsettings();
	bothnewkeys = 0;
}

static int choose_difficulty_toggle_cb(int direction, int* quit, int* selector, void* data) {
	(void) data;
	(void) direction;
	if(*selector == num_difficulties) {
		*quit = 1;
	} else if(bonus >= ifcomplete[*selector]) {
		saveslot = *selector;
		strncpy(savelevel[saveslot].dName, set_names[saveslot], MAX_NAME_LEN + 1);
		return saveslot;
	}
	return -1;
}

int choose_difficulty(void) {
	int quit = 0;
	int selector = 0;
	int maxdisplay = 5;
	int i, j;
	//float slider = 0;
	int ret;
	int barx, bary, barw, barh;

	barx = videomodes.hRes / 5;
	bary = _liney(0, 2) - 2;
	barw = videomodes.hRes * 3 / 5;
	barh = 5 * (font_heights[0] + 1) + 4;
	bothnewkeys = 0;

	if(loadGameFile()) {
		bonus = 0;
		for(i = 0; i < MAX_DIFFICULTIES; i++)
			if(savelevel[i].times_completed > 0)
				bonus += savelevel[i].times_completed;
	}

	while(!quit) {
		if(num_difficulties > 1) {
			_menutextm(2, 0, 0, "Game Mode");
			for(j = 0, i = num_difficulties <= maxdisplay ? 0 : (selector >= maxdisplay ? maxdisplay : 0);
			    i < num_difficulties; j++, i++) {
				if(j < maxdisplay) {
					if(bonus >= ifcomplete[i])
						_menutextm((selector == i), 2 + j, 0, "%s", set_names[i]);
					else {
						if(ifcomplete[i] > 1)
							_menutextm((selector == i), 2 + j, 0,
								   "%s - Finish Game %i Times To UnLock", set_names[i],
								   ifcomplete[i]);
						else
							_menutextm((selector == i), 2 + j, 0,
								   "%s - Finish Game To UnLock", set_names[i]);
					}
				} else
					break;
			}
			_menutextm((selector == i), 8, 0, "Back");

			//draw the scroll bar
			if(num_difficulties > maxdisplay) {
				spriteq_add_box(barx, bary, barw, barh, 0, colors.black, 0);	//outerbox
				spriteq_add_line(barx, bary, barx + 8, bary, 1, colors.white, 0);
				spriteq_add_line(barx, bary, barx, bary + barh, 1, colors.white, 0);
				spriteq_add_line(barx + 8, bary, barx + 8, bary + barh, 1, colors.white, 0);
				spriteq_add_line(barx, bary + barh, barx + 8, bary + barh, 1, colors.white, 0);
				spriteq_add_box(barx + 1, bary + selector * (barh - 3) / num_difficulties, 7, 3, 2, colors.white, 0);	//slider
			}
		}

		update(0, 0);

		if(num_difficulties == 1) {	// OX. Mods with only one set will auto load that difficulty.
			if(selector == num_difficulties)
				quit = 1;
			else if(bonus >= ifcomplete[selector]) {
				saveslot = selector;
				strncpy(savelevel[saveslot].dName, set_names[saveslot], MAX_NAME_LEN + 1);
				return saveslot;
			}
		}
		if((ret = menu_handle_input(num_difficulties, 1, choose_difficulty_toggle_cb, &quit, &selector, NULL)) != -1)
			return ret;
	}
	bothnewkeys = 0;
	return -1;
}

typedef struct {
	int relback;
	int* players;
} choose_mode_data;

static int choose_mode_toggle_cb(int direction, int* quit, int* selector, void* data) {
	(void) direction;
	int status;
	choose_mode_data* cb_data = (choose_mode_data*) data;
	cb_data->relback = 0;
	switch (*selector) {
		case 0:
			status = choose_difficulty();
			if(status != -1) {
				playgame(cb_data->players, status, 0);
				cb_data->relback = 1;
				*quit = 1;
			}
			break;
		case 1:
			status = load_saved_game();
			if(status != -1) {
				playgame(cb_data->players, status, 1);
				cb_data->relback = 1;
				*quit = 1;
			}
			break;
		default:
			*quit = 1;
			break;
	}
	return 0;
}

int choose_mode(int *players) {
	int quit = 0;
	choose_mode_data data = {0, players};
	int selector = 0;

	bothnewkeys = 0;

	while(!quit) {
		_menutextm(2, 0, 0, "Choose Mode");
		_menutextm((selector == 0), 2, 0, "New Game");
		_menutextm((selector == 1), 3, 0, "Load Game");
		_menutextm((selector == 2), 5, 0, "Back");

		update(0, 0);
		menu_handle_input(2, 1, choose_mode_toggle_cb, &quit, &selector, &data);
	}
	bothnewkeys = 0;
	return data.relback;
}

int load_saved_game(void) {
	int quit = 0;
	int selector = 0;
	int savedStatus = 0;
	char name[256] = { "" };
	int col1 = -8, col2 = 6;

	bothnewkeys = 0;

	if((savedStatus = loadGameFile()))
		getSaveFileName(name, ST_SAVE);
	for(saveslot = 0; saveslot < MAX_DIFFICULTIES; saveslot++)
		if(savelevel[saveslot].flag && savelevel[saveslot].level)
			break;

	while(!quit) {
		if(saveslot >= MAX_DIFFICULTIES)	// not found
		{
			_menutextm(2, 0, 0, "Load Game");
			_menutext(0, col1, 2, "Saved File:");
			_menutext(0, col2, 2, "Not Found!");
			_menutextm(1, 4, 0, "Back");

			selector = 2;
		} else {
			_menutextm(2, -5, 0, "Load Game");
			_menutext(0, col1, -3, "Saved File:");
			if(savedStatus)
				_menutext(0, col2, -3, "%s", name);
			else
				_menutext(0, col2, -3, "Not Found!");

			if(savedStatus) {
				_menutext((selector == 0), col1, -2, "Mode:");
				_menutext((selector == 0), col2, -2, "%s", savelevel[saveslot].dName);
				_menutext(0, col1, -1, "Stage:");
				_menutext(0, col2, -1, "%d", savelevel[saveslot].stage);
				_menutext(0, col1, 0, "Level:");
				_menutext(0, col2, 0, "%d", savelevel[saveslot].level);
				_menutext(0, col1, 1, "Credits:");
				_menutext(0, col2, 1, "%d", savelevel[saveslot].credits);
				_menutext(0, col1, 2, "Player 1 Lives:");
				_menutext(0, col2, 2, "%d", savelevel[saveslot].pLives[0]);
				_menutext(0, col1, 3, "Player 2 Lives:");
				_menutext(0, col2, 3, "%d", savelevel[saveslot].pLives[1]);
				_menutext(0, col1, 4, "Player 3 Lives:");
				_menutext(0, col2, 4, "%d", savelevel[saveslot].pLives[2]);
				_menutext(0, col1, 5, "Player 4 Lives:");
				_menutext(0, col2, 5, "%d", savelevel[saveslot].pLives[3]);
				_menutextm((selector == 1), 7, 0, "Start Game");
			}
			_menutextm((selector == 2), 8, 0, "Back");
		}
		update(0, 0);

		if(bothnewkeys & FLAG_ESC)
			quit = 1;
		if(selector == 0 && (bothnewkeys & FLAG_MOVELEFT)) {
			while(1) {
				--saveslot;
				if(saveslot < 0)
					saveslot = MAX_DIFFICULTIES - 1;
				if(savelevel[saveslot].flag && savelevel[saveslot].level)
					break;
			}
			sound_play_sample(samples.beep, 0, savedata.effectvol, savedata.effectvol, 100);
		}
		if(selector == 0 && (bothnewkeys & FLAG_MOVERIGHT)) {
			while(1) {
				++saveslot;
				if(saveslot > MAX_DIFFICULTIES - 1)
					saveslot = 0;
				if(savelevel[saveslot].flag && savelevel[saveslot].level)
					break;
			}
			sound_play_sample(samples.beep, 0, savedata.effectvol, savedata.effectvol, 100);
		}
		if(bothnewkeys & FLAG_MOVEUP) {
			--selector;
			sound_play_sample(samples.beep, 0, savedata.effectvol, savedata.effectvol, 100);
		}
		if(bothnewkeys & FLAG_MOVEDOWN) {
			++selector;
			sound_play_sample(samples.beep, 0, savedata.effectvol, savedata.effectvol, 100);
		}
		if(savedStatus) {
			if(selector < 0)
				selector = 2;
			if(selector > 2)
				selector = 0;
		} else
			selector = 2;

		if(selector > 0 && (bothnewkeys & FLAG_ANYBUTTON)) {
			sound_play_sample(samples.beep2, 0, savedata.effectvol, savedata.effectvol, 100);
			switch (selector) {
				case 1:
					return saveslot;
					break;
				case 2:
					quit = 1;
					break;
			}
		}
	}
	bothnewkeys = 0;
	return -1;
}

typedef struct {
	int* started;
	u32* introtime;
	int* players;
} main_menu_cb_data;

static int main_menu_mode_toggle_cb(int direction, int* quit, int* selector, void* data) {
	(void) direction;
	main_menu_cb_data* cb_data = (main_menu_cb_data*) data;
	int ret = 0;
	unsigned i;
	switch (*selector) {
		case 0:
			for(i = 0; i < MAX_PLAYERS; i++)
				cb_data->players[i] = player[i].newkeys & (FLAG_ANYBUTTON);
			ret = choose_mode(cb_data->players);
			if(ret)
				*cb_data->started = 0;
			break;
		case 1:
			movie_options();
			break;
		case 2:
			options();
			break;
		case 3:
			playscene("data/scenes/howto.txt");
			ret = 1;
			break;
		case 4:
			hallfame(0);
			ret = 1;
			break;
		default:
			*quit = 1;
			break;
	}
	*cb_data->introtime = borTime + GAME_SPEED * 20;
	return ret;
}

int main_menu(int* started, u32* introtime, int *players) {
	int selector;
	int ret = 0;
	main_menu_cb_data cb_data = {started, introtime, players};
	_menutextm((selector == 0), 2, 0, "Start Game");
	_menutextm((selector == 1), 3, 0, "Movie Mode");
	_menutextm((selector == 2), 4, 0, "Options");
	_menutextm((selector == 3), 5, 0, "How To Play");
	_menutextm((selector == 4), 6, 0, "Hall Of Fame");
	_menutextm((selector == 5), 7, 0, "Quit");
	
	update(0, 0);
	
	if(bothnewkeys)
		*introtime = borTime + GAME_SPEED * 20;
	
	ret = menu_handle_input(5, 1, main_menu_mode_toggle_cb, &quit_game, &selector, &cb_data);
	
	return ret == -1 ? 0 : ret;
}

typedef struct {
	int* ok;
	int* setting;
	int player_nr;
	int max_button;
	char* disabled_keys;
} controller_options_cb_data;

static int controller_options_toggle_cb(int direction, int* quit, int* selector, void* data) {
	(void) direction;
	controller_options_cb_data *cb_data = (controller_options_cb_data*) data;
	unsigned i, hit;
	
	if(*selector == cb_data->max_button)
		*quit = 2;
	else if(*selector == cb_data->max_button + 1)
		*quit = 1;
	else {
		hit = 0;
		i = 0;
		while(1) {
			if(!cb_data->disabled_keys[i])
				hit++;
			if(hit > *selector) break;
			i++;
		}
		*cb_data->setting = i;
		*cb_data->ok = savedata.keys[cb_data->player_nr][i];
		savedata.keys[cb_data->player_nr][i] = 0;
		keyboard_getlastkey();
	}
	return 0;
}

void controller_options(int player_nr, int *quit, char** buttonnames, char* disabledkey) {
	int selector = 0;
	int setting = -1;
	int i, k, ok = 0;
	int col1 = -8, col2 = 6;
	controller_options_cb_data data = {&ok, &setting, player_nr, 0, disabledkey};

	while(!(*quit)) {
		data.max_button = 0;
		_menutextm(2, -8, 0, "Player %i", player_nr + 1);
		for(i = 0; i < CB_MAX; i++) {
			if(!disabledkey[i]) {
				_menutext((selector == data.max_button), col1, data.max_button -6, "%s", buttonnames[i]);
				_menutext((selector == data.max_button), col2, data.max_button -6, "%s", control_getkeyname(savedata.keys[player_nr][i]));
				data.max_button++;
			}
		}
		_menutextm((selector == data.max_button), data.max_button -6, 0, "OK");
		_menutextm((selector == data.max_button + 1), data.max_button -5, 0, "Cancel");
		
		update(0, 0);

		if(setting > -1) {
			if(bothnewkeys & FLAG_ESC) {
				savedata.keys[player_nr][setting] = ok;
				sound_play_sample(samples.beep2, 0, savedata.effectvol, savedata.effectvol, 50);
				setting = -1;
			}
			if(setting > -1) {
				k = control_scankey();
				if(k) {
					safe_set(savedata.keys[player_nr], setting, k, ok);
					sound_play_sample(samples.beep2, 0, savedata.effectvol, savedata.effectvol, 100);
					setting = -1;
					// Prevent accidental screenshot
					bothnewkeys = 0;
				}
			}
		} else {
			menu_handle_input(data.max_button + 1, 1, controller_options_toggle_cb, quit, &selector, &data);
		}
	}
	bothnewkeys = 0;
}

